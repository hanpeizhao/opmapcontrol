/**
******************************************************************************
*
* @file       navigation_simulator.h
* @brief      跟车模拟器：沿导航路线折线匀速推进车辆位置，驱动库内导航引擎
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

/**
* @brief 跟车模拟：按固定速度沿导航路线折线推进位置
*
* 纯逻辑类：只发 positionChanged 信号（由 MainWindow 转喂
* OPMapWidget::UpdateVehiclePosition），不直接触碰地图控件。
* 位置推进使用库内 opmap::geoutils 球面几何（haversine / 方位角 / 目标点公式）。
*/
class NavigationSimulator : public QObject
{
    Q_OBJECT

public:
    explicit NavigationSimulator(QObject *parent = 0);

    /// 设置跟车路线（WGS-84 折线，通常取导航路线的 polyline）
    void setPath(const QList<opmap::PointLatLng> &pts);

    /// 重规划后切换路线：从距当前位置最近的折线点继续跟车
    void reroute(const QList<opmap::PointLatLng> &pts);

    /// 行车速度（米/秒）
    void setSpeed(double metersPerSecond) { m_speed = metersPerSecond; }

    bool isRunning() const { return m_running; }

    void start();
    void pause();
    void stop();

    /// 模拟偏离路线：沿当前航向垂直方向甩出 80~120 米
    void simulateYaw();

signals:
    /// 车辆新位置与航向（度，由推进方向推算）
    void positionChanged(const opmap::PointLatLng &pos, double headingDeg);
    /// 到达路线终点
    void finished();
    /// 状态更新（用于面板显示进度）
    void statusUpdated(int current, int total, const QString &message);

private slots:
    void onTick();

private:
    QTimer m_timer;
    opmap::PointLatLng m_currentPos;
    double m_heading;                  ///< 当前航向（度）
    QList<opmap::PointLatLng> m_path;  ///< 跟车路线折线
    int m_index;                       ///< 当前目标点下标
    double m_speed;                    ///< 米/秒
    bool m_running;
    bool m_paused;
};

#endif // NAVIGATION_SIMULATOR_H
