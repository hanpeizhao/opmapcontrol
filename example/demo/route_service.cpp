/**
******************************************************************************
*
* @file       route_service.cpp
* @brief      路径规划客户端：OSRM（默认）/ 高德驾车路径规划，统一输出 WGS-84 折线
* @see        The GNU Public License (GPL) Version 3
* @{
*
*****************************************************************************/

#include "route_service.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QUrlQuery>
#include <QtCore/QUrl>

#include "coordtransform.h"

namespace {

const char *kOsrmBase = "https://router.project-osrm.org";
const char *kAmapDrivingUrl = "https://restapi.amap.com/v3/direction/driving";

/// 简易经纬度点串分割："lng,lat;lng,lat;..." → 点列表（GCJ-02，原样）
QList<opmap::PointLatLng> parsePolyline(const QString &s)
{
    QList<opmap::PointLatLng> pts;
    const QStringList pairs = s.split(';', QString::SkipEmptyParts);
    for (int i = 0; i < pairs.size(); ++i) {
        const QStringList xy = pairs.at(i).split(',');
        if (xy.size() < 2)
            continue;
        const double lng = xy.at(0).toDouble();
        const double lat = xy.at(1).toDouble();
        pts.append(opmap::PointLatLng(lat, lng));
    }
    return pts;
}

} // anonymous namespace

RouteService::RouteService(QObject *parent)
    : QObject(parent),
      m_nam(new QNetworkAccessManager(this)),
      m_provider(Osrm),
      m_busy(false)
{
}

void RouteService::planRoute(const opmap::PointLatLng &origin,
                             const opmap::PointLatLng &dest,
                             Provider provider,
                             const QString &amapKey)
{
    if (m_busy)
        return;

    m_provider = provider;
    m_busy = true;

    if (provider == Osrm)
        requestOsrm(origin, dest);
    else
        requestAmap(origin, dest, amapKey);
}

void RouteService::requestOsrm(const opmap::PointLatLng &origin, const opmap::PointLatLng &dest)
{
    // OSRM 使用 "经度,纬度" 顺序，坐标即 WGS-84
    const QString path = QString::fromLatin1("/route/v1/driving/%1,%2;%3,%4")
            .arg(origin.Lng(), 0, 'f', 6)
            .arg(origin.Lat(), 0, 'f', 6)
            .arg(dest.Lng(), 0, 'f', 6)
            .arg(dest.Lat(), 0, 'f', 6);
    QUrl url(QString::fromLatin1(kOsrmBase) + path);
    QUrlQuery query;
    query.addQueryItem(QLatin1String("overview"), QLatin1String("full"));
    query.addQueryItem(QLatin1String("geometries"), QLatin1String("geojson"));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "opmapcontrol_example/1.0");
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, SIGNAL(finished()), this, SLOT(onRequestFinished()));
}

void RouteService::requestAmap(const opmap::PointLatLng &origin, const opmap::PointLatLng &dest, const QString &key)
{
    // 高德 API 使用 GCJ-02 坐标、"经度,纬度" 顺序，发送前先纠偏
    const opmap::PointLatLng o = opmap::coordtransform::WGS84ToGCJ02(origin);
    const opmap::PointLatLng d = opmap::coordtransform::WGS84ToGCJ02(dest);

    QUrl url(QString::fromLatin1(kAmapDrivingUrl));
    QUrlQuery query;
    query.addQueryItem(QLatin1String("origin"),
                       QString::number(o.Lng(), 'f', 6) + QLatin1Char(',') + QString::number(o.Lat(), 'f', 6));
    query.addQueryItem(QLatin1String("destination"),
                       QString::number(d.Lng(), 'f', 6) + QLatin1Char(',') + QString::number(d.Lat(), 'f', 6));
    query.addQueryItem(QLatin1String("extensions"), QLatin1String("base"));
    query.addQueryItem(QLatin1String("key"), key);
    url.setQuery(query);

    QNetworkRequest request(url);
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, SIGNAL(finished()), this, SLOT(onRequestFinished()));
}

void RouteService::onRequestFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;
    reply->deleteLater();
    m_busy = false;

    if (reply->error() != QNetworkReply::NoError) {
        emit routeFailed(reply->errorString());
        return;
    }

    const QByteArray data = reply->readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        emit routeFailed(QString::fromUtf8("响应不是有效 JSON"));
        return;
    }
    const QJsonObject root = doc.object();

    QList<opmap::PointLatLng> pts;
    double meters = 0;
    int seconds = 0;

    if (m_provider == Osrm) {
        const QJsonArray routes = root.value(QLatin1String("routes")).toArray();
        if (root.value(QLatin1String("code")).toString() != QLatin1String("Ok") || routes.isEmpty()) {
            emit routeFailed(QString::fromUtf8("OSRM 无可用路线"));
            return;
        }
        const QJsonObject route = routes.at(0).toObject();
        meters = route.value(QLatin1String("distance")).toDouble();
        seconds = (int)route.value(QLatin1String("duration")).toDouble();
        const QJsonArray coords = route.value(QLatin1String("geometry"))
                                      .toObject().value(QLatin1String("coordinates")).toArray();
        for (int i = 0; i < coords.size(); ++i) {
            const QJsonArray c = coords.at(i).toArray();
            if (c.size() < 2)
                continue;
            // GeoJSON: [lng, lat]，OSRM 即 WGS-84，无需纠偏
            pts.append(opmap::PointLatLng(c.at(1).toDouble(), c.at(0).toDouble()));
        }
    } else {
        if (root.value(QLatin1String("status")).toString() != QLatin1String("1")) {
            emit routeFailed(QString::fromUtf8("高德错误: ") + root.value(QLatin1String("info")).toString());
            return;
        }
        const QJsonObject routeObj = root.value(QLatin1String("route")).toObject();
        const QJsonArray paths = routeObj.value(QLatin1String("paths")).toArray();
        if (paths.isEmpty()) {
            emit routeFailed(QString::fromUtf8("高德无可用路线"));
            return;
        }
        const QJsonObject path = paths.at(0).toObject();
        meters = path.value(QLatin1String("distance")).toString().toDouble();
        seconds = path.value(QLatin1String("duration")).toString().toInt();
        const QJsonArray steps = path.value(QLatin1String("steps")).toArray();
        for (int i = 0; i < steps.size(); ++i) {
            const QString polyline = steps.at(i).toObject().value(QLatin1String("polyline")).toString();
            const QList<opmap::PointLatLng> seg = parsePolyline(polyline);
            // 高德返回 GCJ-02，统一转回 WGS-84
            for (int j = 0; j < seg.size(); ++j)
                pts.append(opmap::coordtransform::GCJ02ToWGS84(seg.at(j)));
        }
    }

    if (pts.size() < 2) {
        emit routeFailed(QString::fromUtf8("路线点过少"));
        return;
    }

    emit routeReady(pts, meters, seconds);
}
