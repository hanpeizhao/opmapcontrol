/**
******************************************************************************
*
* @file       route_service.h
* @brief      路径规划客户端：OSRM（默认）/ 高德驾车路径规划，统一输出 WGS-84 折线
* @see        The GNU Public License (GPL) Version 3
* @{
*
*****************************************************************************/

#ifndef ROUTE_SERVICE_H
#define ROUTE_SERVICE_H

#include <QObject>
#include <QString>
#include "pointlatlng.h"

/**
* @brief 在线路径规划：请求路由服务，把 A→B 的道路折线解析为 WGS-84 点序列
*
* 支持的服务：
* - OSRM 公共服务器（免费、无需 key，坐标即 WGS-84）
* - 高德 Web 服务驾车路径规划（需自备 key；请求/返回均为 GCJ-02，
*   发送前 WGS84→GCJ02，返回后 GCJ02→WGS84 纠偏）
*/
class RouteService : public QObject
{
    Q_OBJECT

public:
    /// 路由服务提供方
    enum Provider
    {
        Osrm,   ///< OSRM 公共服务器（免 key）
        Amap    ///< 高德驾车路径规划（需 key）
    };
    Q_ENUMS(Provider)

    explicit RouteService(QObject *parent = 0);

    /**
     * @brief 异步规划路线，结果经 routeReady/routeFailed 信号返回
     * @param origin  起点（WGS-84）
     * @param dest    终点（WGS-84）
     * @param provider 路由服务
     * @param amapKey 高德 key（仅 Amap 需要）
     */
    void planRoute(const opmap::PointLatLng &origin,
                   const opmap::PointLatLng &dest,
                   Provider provider,
                   const QString &amapKey = QString());

    bool busy() const { return m_busy; }

signals:
    /// 路线规划成功：pts 为 WGS-84 折线点，meters 总距离，seconds 预计耗时
    void routeReady(const QList<opmap::PointLatLng> &pts, double meters, int seconds);
    /// 路线规划失败（网络/解析/服务错误）
    void routeFailed(const QString &reason);

private slots:
    void onRequestFinished();

private:
    void requestOsrm(const opmap::PointLatLng &origin, const opmap::PointLatLng &dest);
    void requestAmap(const opmap::PointLatLng &origin, const opmap::PointLatLng &dest, const QString &key);

    class QNetworkAccessManager *m_nam;
    Provider m_provider;
    bool m_busy;
};

#endif // ROUTE_SERVICE_H
