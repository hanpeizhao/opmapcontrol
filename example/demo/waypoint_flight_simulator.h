/**
******************************************************************************
*
* @file       waypoint_flight_simulator.h
* @brief      航点飞行模拟遥测源（纯数据源，不含任务状态机）：朝库任务引擎
*             指定的当前目标匀速推进，悬停/到达/动作/任务完成判定全部在库
*             （WaypointMissionEngine）。真机接入时整体替换本类——把真机
*             遥测直接喂 SetUAVPos/UpdateVehiclePosition 即可，链路不变
* @see        The GNU Public License (GPL) Version 3
* @{
*
*****************************************************************************/
#ifndef WAYPOINT_FLIGHT_SIMULATOR_H
#define WAYPOINT_FLIGHT_SIMULATOR_H

#include <QtCore/QObject>

#include "opmapcontrol.h"

class QTimer;

class WaypointFlightSimulator : public QObject
{
    Q_OBJECT

public:
    explicit WaypointFlightSimulator(QObject *parent = 0);
    bool isActive() const { return m_active; }

public slots:
    /// 从 startPos 起飞，以 speedMps（米/秒）巡航；目标点由库任务引擎下发
    /// （起飞前已下发的目标保留不清，支持"先建任务后起飞"的多机编排）
    void start(const opmap::PointLatLng &startPos, double speedMps);
    void stop();
    /// 库 missionCurrentWaypointChanged → 切换推进目标
    void setTarget(const opmap::PointLatLng &target);
    /// 库 missionHoverStateChanged → 悬停时原地保持遥测心跳
    void setHovering(bool hovering);

signals:
    /// 每个推进周期的模拟遥测：位置 / 航向（度，正北 0 顺时针）
    void positionChanged(opmap::PointLatLng position, double headingDeg);

private slots:
    void onTick();

private:
    QTimer *m_timer;
    opmap::PointLatLng m_pos;      ///< 当前模拟遥测位置
    opmap::PointLatLng m_target;   ///< 当前推进目标（库下发）
    bool m_hasTarget;
    double m_speedMps;             ///< 巡航速度（米/秒）
    double m_lastHeading;          ///< 上一周期航向（悬停期间沿用）
    bool m_hovering;               ///< 悬停中（原地心跳，不推进）
    bool m_active;
};

#endif // WAYPOINT_FLIGHT_SIMULATOR_H
