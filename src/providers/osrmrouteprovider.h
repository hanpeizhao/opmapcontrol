/**
******************************************************************************
* @file       osrmrouteprovider.h
* @brief      OSRM 在线路径规划 provider（公共服务器，免 key，坐标即 WGS-84）
*
*             请求 OSRM route 服务（steps=true），由 steps[].geometry 拼接完整
*             折线并按 maneuver.type+modifier 合成中文转向指令。
******************************************************************************
*/
#ifndef OSRMROUTEPROVIDER_H
#define OSRMROUTEPROVIDER_H

#include <QString>
#include "abstractrouteprovider.h"

class QNetworkAccessManager;

namespace opmap {

class OsrmRouteProvider : public AbstractRouteProvider
{
    Q_OBJECT

public:
    explicit OsrmRouteProvider(QObject *parent = 0);

    /// 设置 OSRM 服务器地址（默认公共服务器 https://router.project-osrm.org）
    void SetServerUrl(const QString &url) { m_serverUrl = url; }

    virtual void requestRoute(const opmap::PointLatLng &from, const opmap::PointLatLng &to);
    virtual bool isBusy() const { return m_busy; }
    /// 用户备选路线偏好：routeReady 发出第 index 条备选（0=推荐路线，越界自动回 0）
    virtual void setPreferredAlternative(int index) { m_preferredAlternative = index; }

private slots:
    void onRequestFinished();

private:
    /// maneuver.type + modifier → 中文指令（如 turn+right → "右转"）
    static QString instructionFromManeuver(const QString &type, const QString &modifier);
    /// 解析 OSRM 单条 route JSON → Route（折线+分步指令），点过少返回 false
    static bool parseRoute(const QJsonObject &routeObj, opmap::Route &route);

    QNetworkAccessManager *m_nam;
    QString m_serverUrl;
    bool m_busy;
    int m_preferredAlternative;
};

} // namespace opmap

#endif // OSRMROUTEPROVIDER_H
