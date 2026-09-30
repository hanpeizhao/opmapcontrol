/**
******************************************************************************
*
* @file       waypoint_flight_simulator.h
* @brief      航点飞行模拟数据源：从起飞点沿航点序列匀速推进，模拟无人机遥测。
*             未来接真实无人机时，仅需以真机遥测数据替换本类的
*             positionChanged 信号（SetUAVPos 喂点链路完全不变）
* @see        The GNU Public License (GPL) Version 3
* @{
*
*****************************************************************************/
#ifndef WAYPOINT_FLIGHT_SIMULATOR_H
#define WAYPOINT_FLIGHT_SIMULATOR_H

#include <QtCore/QObject>
#include <QtCore/QList>

#include "opmapcontrol.h"

class QTimer;

class WaypointFlightSimulator : public QObject
{
    Q_OBJECT

public:
    explicit WaypointFlightSimulator(QObject *parent = 0);
    bool isActive() const { return m_active; }

public slots:
    /// 从 startPos（通常是 Home 返航点）出发，依次飞往 waypoints（米/秒）
    void start(const opmap::PointLatLng &startPos,
               const QList<opmap::PointLatLng> &waypoints,
               double speedMps);
    void stop();

signals:
    /// 每个推进周期的遥测：位置 / 航向（度，正北 0 顺时针） / 目标航点下标（0 起） / 总数
    void positionChanged(opmap::PointLatLng position, double headingDeg,
                         int targetIndex, int total);
    /// 抵达某航点（序号从 1 起，与库的 UAVReachedWayPoint 判定对拍）
    void waypointPassed(int index, int total);
    void finished();

private slots:
    void onTick();

private:
    QTimer *m_timer;
    QList<opmap::PointLatLng> m_waypoints;
    opmap::PointLatLng m_pos;      ///< 当前遥测位置
    double m_speedMps;             ///< 巡航速度（米/秒）
    int m_target;                  ///< 当前目标航点下标（0 起）
    bool m_active;
};

#endif // WAYPOINT_FLIGHT_SIMULATOR_H
