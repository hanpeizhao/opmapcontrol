/**
******************************************************************************
* @file       navigationengine.h
* @brief      车载导航状态机：规划 → 沿路进度 → 偏航检测 → 自动重规划 → 到达
*
*             纯逻辑 QObject，不依赖任何界面组件。路线来源由注入的
*             AbstractRouteProvider 异步请求；车辆位置由调用方经
*             UpdatePosition 喂入（库不绑定 GPS 硬件）。
******************************************************************************
*/
#ifndef NAVIGATIONENGINE_H
#define NAVIGATIONENGINE_H

#include <QObject>
#include <QList>

#include "pointlatlng.h"
#include "route.h"
#include "geoutils.h"

namespace opmap {

class AbstractRouteProvider;

class NavigationEngine : public QObject
{
    Q_OBJECT

public:
    explicit NavigationEngine(QObject *parent = 0);

    /// 注入路由 provider（不接管所有权；替换时自动改接信号）
    void SetRouteProvider(AbstractRouteProvider *provider);

    /**
     * @brief 发起导航：请求 from→dest 路线，成功后自动进入 Navigating
     */
    void NavigateTo(const opmap::PointLatLng &from, const opmap::PointLatLng &dest);

    /**
     * @brief 仅规划并显示 from→dest 路线，不进入导航（选点预览用）
     *
     * 结果同样经 routePlanned 发出并绘制；不喂位置、不判偏航/到达
     */
    void PlanRoute(const opmap::PointLatLng &from, const opmap::PointLatLng &dest);

    /// 直接以已有路线开始导航（重规划结果复用同一入口）
    void StartRoute(const opmap::Route &route);

    /// 喂入车辆位置（WGS-84）：规划期间仅缓存，导航期更新进度/偏航/到达
    void UpdatePosition(const opmap::PointLatLng &pos);

    /// 停止导航并清空路线
    void Stop();

    bool IsNavigating() const { return m_state == Navigating; }
    opmap::Route CurrentRoute() const { return m_route; }
    opmap::PointLatLng Destination() const { return m_destination; }

    /// 是否已有车辆位置（曾通过 UpdatePosition 喂入）
    bool HasPosition() const { return m_hasLastPos; }
    /// 最近一次喂入的车辆位置（WGS-84；先用 HasPosition 判断有效性）
    opmap::PointLatLng LastPosition() const { return m_lastPos; }

    // —— 可调参数 ——
    void SetAutoReroute(bool on) { m_autoReroute = on; }
    void SetOffRouteThresholdM(double meters) { m_offRouteThresholdM = meters; }
    void SetOffRouteConsecutiveFixes(int count) { m_offRouteConsecutiveFixes = count; }
    void SetArrivalThresholdM(double meters) { m_arrivalThresholdM = meters; }
    void SetMinRerouteIntervalMs(int ms) { m_minRerouteIntervalMs = ms; }

    /** @brief 当前连续偏航计数（相对 ConsecutiveFixes 阈值；0=在路线上） */
    int OffRouteCount() const { return m_offRouteCount; }

signals:
    /// 规划成功，导航开始
    void routePlanned(const opmap::Route &route);
    /// 沿路进度（traveledMeters 为已行驶距离；remainingS 按剩余距离占总距离
    /// 比例从总时长近似折算，非逐段 ETA）
    void progressUpdated(double traveledMeters, double remainingMeters,
                         int remainingSeconds, const QString &instruction);
    /// 偏航确认（连续 count 次超过阈值时发出一次）
    void offRouteDetected(const opmap::PointLatLng &pos, double deviationMeters);
    /// 重规划成功，已切换新路线
    void rerouteReady(const opmap::Route &route);
    /// 重规划失败（非致命：继续沿旧路线行驶，受最小间隔约束会自动重试）
    void rerouteFailed(const QString &reason);
    /// 到达目的地（剩余距离小于到达阈值）
    void arrived();
    /// 导航失败（无 provider、provider 忙或初次规划失败）
    void navigationFailed(const QString &reason);

private slots:
    void onRouteReady(const opmap::Route &route);
    void onRouteFailed(const QString &reason);

private:
    enum NavState
    {
        Idle,        ///< 未导航
        Planning,    ///< 初次规划中
        Navigating,  ///< 沿路导航中
        Rerouting    ///< 偏航重规划中（期间继续按旧路线显示）
    };

    void installRoute(const opmap::Route &route);   ///< 存路线 + 预计算累计距离表
    geoutils::SegmentHit locateNearestSegment(const opmap::PointLatLng &pos);
    QString instructionFor(int segIdx, double traveledM) const;
    void checkOffRoute(const opmap::PointLatLng &pos, double devM);

    AbstractRouteProvider *m_provider;
    NavState m_state;
    bool m_planOnly;               ///< 规划预览标记（PlanRoute 置位，成功后回 Idle）

    opmap::Route m_route;
    QList<double> m_cumDist;       ///< 折线累计距离（米），size == polyline.size()
    opmap::PointLatLng m_destination;
    opmap::PointLatLng m_lastPos;
    bool m_hasLastPos;

    int m_lastSegIdx;              ///< 最近命中段索引（窗口查找的起点）
    int m_offRouteCount;           ///< 连续偏航计数
    qint64 m_lastRerouteMs;        ///< 上次发起重规划的时刻（防重规划风暴）

    // 可调参数
    bool m_autoReroute;
    double m_offRouteThresholdM;
    int m_offRouteConsecutiveFixes;
    double m_arrivalThresholdM;
    int m_minRerouteIntervalMs;
};

} // namespace opmap

#endif // NAVIGATIONENGINE_H
