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

private slots:
    void onRequestFinished();

private:
    /// maneuver.type + modifier → 中文指令（如 turn+right → "右转"）
    static QString instructionFromManeuver(const QString &type, const QString &modifier);

    QNetworkAccessManager *m_nam;
    QString m_serverUrl;
    bool m_busy;
};

} // namespace opmap

#endif // OSRMROUTEPROVIDER_H
