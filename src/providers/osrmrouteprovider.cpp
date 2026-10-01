/**
******************************************************************************
* @file       osrmrouteprovider.cpp
* @brief      OSRM 在线路径规划 provider 实现（见 osrmrouteprovider.h）
******************************************************************************
*/

#include "osrmrouteprovider.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

namespace opmap {

namespace {

const char *kDefaultOsrmBase = "https://router.project-osrm.org";

} // anonymous namespace

OsrmRouteProvider::OsrmRouteProvider(QObject *parent)
    : AbstractRouteProvider(parent),
      m_nam(new QNetworkAccessManager(this)),
      m_serverUrl(QString::fromLatin1(kDefaultOsrmBase)),
      m_busy(false),
      m_preferredAlternative(0)
{
}

void OsrmRouteProvider::requestRoute(const opmap::PointLatLng &from, const opmap::PointLatLng &to)
{
    if (m_busy)
        return;
    m_busy = true;

    // OSRM 使用 "经度,纬度" 顺序，坐标即 WGS-84
    const QString path = QString::fromLatin1("/route/v1/driving/%1,%2;%3,%4")
            .arg(from.Lng(), 0, 'f', 6)
            .arg(from.Lat(), 0, 'f', 6)
            .arg(to.Lng(), 0, 'f', 6)
            .arg(to.Lat(), 0, 'f', 6);
    QUrl url(m_serverUrl + path);
    QUrlQuery query;
    query.addQueryItem(QLatin1String("overview"), QLatin1String("full"));
    query.addQueryItem(QLatin1String("geometries"), QLatin1String("geojson"));
    query.addQueryItem(QLatin1String("steps"), QLatin1String("true"));
    query.addQueryItem(QLatin1String("alternatives"), QLatin1String("2"));   // 最多 2 条备选 + 1 条推荐
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "opmapcontrol/1.0");
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, SIGNAL(finished()), this, SLOT(onRequestFinished()));
}

QString OsrmRouteProvider::instructionFromManeuver(const QString &type, const QString &modifier)
{
    // 常见 maneuver 类型/修饰语的中文映射
    if (type == QLatin1String("depart"))
        return QString::fromUtf8("出发");
    if (type == QLatin1String("arrive"))
        return QString::fromUtf8("到达目的地");
    if (type == QLatin1String("merge"))
        return QString::fromUtf8("汇入主路");
    if (type == QLatin1String("on ramp"))
        return QString::fromUtf8("上匝道");
    if (type == QLatin1String("off ramp"))
        return QString::fromUtf8("下匝道");
    if (type == QLatin1String("roundabout") || type == QLatin1String("rotary"))
        return QString::fromUtf8("进入环岛");
    if (type == QLatin1String("exit roundabout") || type == QLatin1String("exit rotary"))
        return QString::fromUtf8("驶出环岛");

    if (type == QLatin1String("turn") || type == QLatin1String("new name")
            || type == QLatin1String("continue") || type == QLatin1String("end of road")
            || type == QLatin1String("fork")) {
        if (modifier == QLatin1String("straight"))
            return QString::fromUtf8("直行");
        if (modifier == QLatin1String("left") || modifier == QLatin1String("slight left"))
            return QString::fromUtf8("左转");
        if (modifier == QLatin1String("right") || modifier == QLatin1String("slight right"))
            return QString::fromUtf8("右转");
        if (modifier == QLatin1String("sharp left"))
            return QString::fromUtf8("向左急转");
        if (modifier == QLatin1String("sharp right"))
            return QString::fromUtf8("向右急转");
        if (modifier == QLatin1String("uturn"))
            return QString::fromUtf8("掉头");
    }
    return QString::fromUtf8("直行");
}

/// 解析 OSRM 单条 route JSON：由 steps[].geometry.coordinates 顺序拼接折线，
/// 保证 step 与折线索引严格对齐
bool OsrmRouteProvider::parseRoute(const QJsonObject &routeObj, opmap::Route &route)
{
    route = opmap::Route();
    route.totalDistanceMeters = routeObj.value(QLatin1String("distance")).toDouble();
    route.totalDurationSeconds = (int)routeObj.value(QLatin1String("duration")).toDouble();

    const QJsonArray legs = routeObj.value(QLatin1String("legs")).toArray();
    for (int i = 0; i < legs.size(); ++i) {
        const QJsonArray steps = legs.at(i).toObject().value(QLatin1String("steps")).toArray();
        for (int j = 0; j < steps.size(); ++j) {
            const QJsonObject stepObj = steps.at(j).toObject();

            RouteStep step;
            const QJsonObject maneuver = stepObj.value(QLatin1String("maneuver")).toObject();
            step.instruction = instructionFromManeuver(maneuver.value(QLatin1String("type")).toString(),
                                                       maneuver.value(QLatin1String("modifier")).toString());
            step.startIndex = route.polyline.isEmpty() ? 0 : route.polyline.size() - 1;
            step.distanceMeters = stepObj.value(QLatin1String("distance")).toDouble();
            step.durationSeconds = (int)stepObj.value(QLatin1String("duration")).toDouble();

            const QJsonArray coords = stepObj.value(QLatin1String("geometry"))
                                          .toObject().value(QLatin1String("coordinates")).toArray();
            for (int k = 0; k < coords.size(); ++k) {
                const QJsonArray c = coords.at(k).toArray();
                if (c.size() < 2)
                    continue;
                // GeoJSON: [lng, lat]，OSRM 即 WGS-84，无需纠偏
                route.polyline.append(PointLatLng(c.at(1).toDouble(), c.at(0).toDouble()));
            }
            route.steps.append(step);
        }
    }

    return route.isValid();
}

void OsrmRouteProvider::onRequestFinished()
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
        emit routeFailed(QString::fromUtf8("OSRM 响应不是有效 JSON"));
        return;
    }
    const QJsonObject root = doc.object();
    const QJsonArray routes = root.value(QLatin1String("routes")).toArray();
    if (root.value(QLatin1String("code")).toString() != QLatin1String("Ok") || routes.isEmpty()) {
        emit routeFailed(QString::fromUtf8("OSRM 无可用路线"));
        return;
    }

    // 解析全部备选（推荐路线在最前）；无备选时只有一条
    QList<opmap::Route> alternatives;
    for (int i = 0; i < routes.size(); ++i) {
        opmap::Route route;
        if (parseRoute(routes.at(i).toObject(), route))
            alternatives.append(route);
    }
    if (alternatives.isEmpty()) {
        emit routeFailed(QString::fromUtf8("OSRM 路线点过少"));
        return;
    }

    // routeReady 按用户偏好发出（默认 0=推荐路线），导航/重规划随之走选中的那条
    const int preferred = (m_preferredAlternative >= 0 && m_preferredAlternative < alternatives.size())
            ? m_preferredAlternative : 0;
    emit alternativesReady(alternatives);
    emit routeReady(alternatives.at(preferred));
}

} // namespace opmap
