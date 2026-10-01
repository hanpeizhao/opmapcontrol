/**
******************************************************************************
* @file       abstractrouteprovider.h
* @brief      在线路径规划 provider 抽象基类
*
*             异步请求 A→B 驾车路线，结果经 routeReady/routeFailed 信号返回
*             （QNetworkAccessManager 回调在主线程，全程无跨线程）。具体实现：
*             OsrmRouteProvider（免 key 默认）、AmapRouteProvider（高德，需 key）。
******************************************************************************
*/
#ifndef ABSTRACTROUTEPROVIDER_H
#define ABSTRACTROUTEPROVIDER_H

#include <QObject>
#include <QString>

#include "route.h"
#include "pointlatlng.h"

namespace opmap {

class AbstractRouteProvider : public QObject
{
    Q_OBJECT

public:
    explicit AbstractRouteProvider(QObject *parent = 0);

    /**
     * @brief 异步规划路线，结果经 routeReady/routeFailed 信号返回
     * @param from 起点（WGS-84）
     * @param to   终点（WGS-84）
     */
    virtual void requestRoute(const opmap::PointLatLng &from, const opmap::PointLatLng &to) = 0;

    /// 是否有请求在途（在途时再次 requestRoute 被忽略）
    virtual bool isBusy() const = 0;

    /// 用户备选路线偏好：下次 routeReady 发出第 index 条（0=推荐路线）。
    /// 默认空实现——只支持单路线的 provider 忽略之
    virtual void setPreferredAlternative(int index) { Q_UNUSED(index); }

signals:
    /// 路线规划成功（WGS-84 折线 + 分步指令）
    void routeReady(const opmap::Route &route);
    /// 路线规划失败（网络/解析/服务错误）
    void routeFailed(const QString &reason);
    /// 本次规划的备选路线全集（第一条与 routeReady 同步；单路线 provider 只发一条）
    void alternativesReady(QList<opmap::Route> routes);
};

} // namespace opmap

#endif // ABSTRACTROUTEPROVIDER_H
