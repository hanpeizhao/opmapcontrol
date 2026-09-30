/**
******************************************************************************
* @file       amaprouteprovider.cpp
* @brief      高德驾车路径规划 provider 实现（见 amaprouteprovider.h）
******************************************************************************
*/

#include "amaprouteprovider.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QRegExp>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

#include "coordtransform.h"

namespace opmap {

namespace {

const char *kAmapDrivingUrl = "https://restapi.amap.com/v3/direction/driving";

} // anonymous namespace

AmapRouteProvider::AmapRouteProvider(QObject *parent)
    : AbstractRouteProvider(parent),
      m_nam(new QNetworkAccessManager(this)),
      m_busy(false)
{
}

void AmapRouteProvider::requestRoute(const opmap::PointLatLng &from, const opmap::PointLatLng &to)
{
    if (m_busy)
        return;
    m_busy = true;

    // 高德 API 使用 GCJ-02 坐标、"经度,纬度" 顺序，发送前先纠偏
    const opmap::PointLatLng o = coordtransform::WGS84ToGCJ02(from);
    const opmap::PointLatLng d = coordtransform::WGS84ToGCJ02(to);

    QUrl url(QString::fromLatin1(kAmapDrivingUrl));
    QUrlQuery query;
    query.addQueryItem(QLatin1String("origin"),
                       QString::number(o.Lng(), 'f', 6) + QLatin1Char(',') + QString::number(o.Lat(), 'f', 6));
    query.addQueryItem(QLatin1String("destination"),
                       QString::number(d.Lng(), 'f', 6) + QLatin1Char(',') + QString::number(d.Lat(), 'f', 6));
    query.addQueryItem(QLatin1String("extensions"), QLatin1String("base"));
    query.addQueryItem(QLatin1String("key"), m_key);
    url.setQuery(query);

    QNetworkRequest request(url);
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, SIGNAL(finished()), this, SLOT(onRequestFinished()));
}

QList<PointLatLng> AmapRouteProvider::parsePolyline(const QString &s)
{
    QList<PointLatLng> pts;
    const QStringList pairs = s.split(';', QString::SkipEmptyParts);
    for (int i = 0; i < pairs.size(); ++i) {
        const QStringList xy = pairs.at(i).split(',');
        if (xy.size() < 2)
            continue;
        pts.append(PointLatLng(xy.at(1).toDouble(), xy.at(0).toDouble()));
    }
    return pts;
}

void AmapRouteProvider::onRequestFinished()
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

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject()) {
        emit routeFailed(QString::fromUtf8("高德响应不是有效 JSON"));
        return;
    }
    const QJsonObject root = doc.object();
    if (root.value(QLatin1String("status")).toString() != QLatin1String("1")) {
        emit routeFailed(QString::fromUtf8("高德错误: ") + root.value(QLatin1String("info")).toString());
        return;
    }

    const QJsonArray paths = root.value(QLatin1String("route")).toObject()
                                     .value(QLatin1String("paths")).toArray();
    if (paths.isEmpty()) {
        emit routeFailed(QString::fromUtf8("高德无可用路线"));
        return;
    }

    const QJsonObject path = paths.at(0).toObject();
    Route route;
    route.totalDistanceMeters = path.value(QLatin1String("distance")).toString().toDouble();
    route.totalDurationSeconds = path.value(QLatin1String("duration")).toString().toInt();

    // steps[].polyline 顺序拼接总折线；instruction 自带中文，去除 HTML 标签
    const QRegExp tagRe(QLatin1String("<[^>]*>"));
    const QJsonArray steps = path.value(QLatin1String("steps")).toArray();
    for (int i = 0; i < steps.size(); ++i) {
        const QJsonObject stepObj = steps.at(i).toObject();

        RouteStep step;
        QString instr = stepObj.value(QLatin1String("instruction")).toString();
        step.instruction = instr.remove(tagRe);
        step.startIndex = route.polyline.isEmpty() ? 0 : route.polyline.size() - 1;
        step.distanceMeters = stepObj.value(QLatin1String("distance")).toString().toDouble();
        step.durationSeconds = stepObj.value(QLatin1String("duration")).toString().toInt();

        const QList<PointLatLng> seg = parsePolyline(stepObj.value(QLatin1String("polyline")).toString());
        // 高德返回 GCJ-02，统一转回 WGS-84
        for (int j = 0; j < seg.size(); ++j)
            route.polyline.append(coordtransform::GCJ02ToWGS84(seg.at(j)));

        route.steps.append(step);
    }

    if (!route.isValid()) {
        emit routeFailed(QString::fromUtf8("高德路线点过少"));
        return;
    }

    emit routeReady(route);
}

} // namespace opmap
