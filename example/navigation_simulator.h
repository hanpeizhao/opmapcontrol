/**
******************************************************************************
*
* @file       navigation_simulator.h
* @brief      导航模拟器：驱动 UAV 位置沿航点序列或路线折线推进
* @see        The GNU Public License (GPL) Version 3
* @{
*
*****************************************************************************/

#ifndef NAVIGATION_SIMULATOR_H
#define NAVIGATION_SIMULATOR_H

#include <QObject>
#include <QTimer>
#include <QList>
#include "pointlatlng.h"

namespace opmap {
class UAVItem;
class WayPointItem;
}

/**
* @brief 航点/路线飞行模拟：按固定速度沿目标序列推进 UAV 位置
*
* 两种模式：
* - 航点模式：目标为 WayPointItem 序列，到达后标记 SetReached 并发信号
* - 路线模式：目标为规划路线折线点序列（PathMode），逐段推进
*
* 纯逻辑类：仅持有 UAVItem 指针更新位置，不依赖任何界面组件。
* 位置推进使用标准球面几何（haversine / 方位角 / 目标点公式）。
*/
class NavigationSimulator : public QObject
{
    Q_OBJECT

public:
    explicit NavigationSimulator(QObject *parent = 0);

    void setUAV(opmap::UAVItem *uav) { m_uav = uav; }

    /// 设置航点模式目标序列
    void setWaypointRoute(const QList<opmap::WayPointItem*> &wps);

    /// 设置路线模式目标折线（WGS-84 点序列）
    void setPathRoute(const QList<opmap::PointLatLng> &pts, int altitudeMeters);

    /// 飞行速度（米/秒）
    void setSpeed(double metersPerSecond) { m_speed = metersPerSecond; }

    bool isRunning() const { return m_running; }
    int currentIndex() const { return m_index; }

    void start();
    void pause();
    void stop();

signals:
    /// 到达航点（航点模式）：index 为序列中的序号
    void waypointReached(int index);
    /// 沿路线推进完成
    void finished();
    /// 位置/状态更新（用于面板显示进度）
    void statusUpdated(int current, int total, const QString &message);

private slots:
    void onTick();

private:
    void advanceTarget();

    QTimer m_timer;
    opmap::UAVItem *m_uav;
    opmap::PointLatLng m_currentPos;

    QList<opmap::WayPointItem*> m_waypoints;      ///< 航点模式目标
    QList<opmap::PointLatLng> m_pathPoints;       ///< 路线模式目标折线
    QList<int> m_pathAltitudes;                   ///< 路线模式各点高度（统一值展开）

    int m_index;
    double m_speed;          ///< 米/秒
    bool m_running;
    bool m_pathMode;
    bool m_paused;
};

#endif // NAVIGATION_SIMULATOR_H
