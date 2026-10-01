/**
******************************************************************************
*
* @file       waypointmissionengine.h
* @brief      航点任务飞行状态机：喂点驱动的到达判定/悬停计时/动作触发/航点推进。
*             纯状态机，无内部定时器——真机（飞控沿航点飞，地面站喂遥测）与
*             模拟器（插值生成位置流）同构接入
* @see        The GNU Public License (GPL) Version 3
* @{
*
*****************************************************************************/
/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*/
#ifndef WAYPOINTMISSIONENGINE_H
#define WAYPOINTMISSIONENGINE_H

#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QElapsedTimer>
#include "pointlatlng.h"

namespace opmap {

/**
* @brief 航点任务飞行状态机（engine 层，喂点驱动）
*
* @class WaypointMissionEngine waypointmissionengine.h "waypointmissionengine.h"
*
* 用法：SetMission 装载航点序列 → StartMission 启动 → 位置源持续调
* UpdatePosition 喂点 → 引擎自动判定到达、悬停计时、触发动作信号、推进
* 下一航点，全部完成后发 missionFinished。上层只响应信号（下发拍照/悬停
* 指令、更新目标显示），不参与状态推进。
*/
class WaypointMissionEngine : public QObject
{
    Q_OBJECT

public:
    /// 航点动作：与 WayPointItem::WayPointAction 数值对齐（ui 层 typedef 引用本枚举）
    enum WaypointAction
    {
        ActionNone = 0,    ///< 无动作（普通途经点）
        ActionPhoto = 1,   ///< 到达即触发拍照
        ActionHover = 2    ///< 到达后悬停
    };

    /// 任务航点：坐标 + 悬停时长（秒，仅 ActionHover 生效）+ 动作
    struct MissionWaypoint
    {
        opmap::PointLatLng position;
        int hoverSeconds;
        int action;
        MissionWaypoint() : hoverSeconds(0), action(ActionNone) {}
        MissionWaypoint(opmap::PointLatLng const& pos, int hoverSecs, int act)
            : position(pos), hoverSeconds(hoverSecs), action(act) {}
    };

    explicit WaypointMissionEngine(QObject *parent = 0);

    /// 装载任务航点序列与到达判定半径（米）；<1 点视为清空任务
    void SetMission(QList<MissionWaypoint> const& waypoints, double arrivalRadiusMeters = 15.0);
    void StartMission();
    void StopMission();
    void ClearMission();

    bool IsMissionActive() const { return m_active; }
    int CurrentWaypointIndex() const { return m_current; }
    int WaypointCount() const { return m_waypoints.size(); }

    /// 位置源喂点：驱动到达判定/悬停计时/航点推进（任务未激活时静默忽略）
    void UpdatePosition(opmap::PointLatLng const& position);

signals:
    void missionStarted();
    /// 当前目标航点切换（0 起）；模拟器/显示层据此更新目标
    void currentWaypointChanged(int index);
    /// 抵达某航点（动作与悬停随后由 actionTriggered/hoverStateChanged 表达）
    void waypointReached(int index, int action);
    /// 悬停状态切换：enter=true 进入悬停（seconds 为时长），false 结束
    void hoverStateChanged(bool hovering, int seconds);
    /// 到达动作触发：Photo 立即发；Hover 在进入悬停时发
    void actionTriggered(int index, int action);
    void missionFinished();

private:
    void advanceToNext();

    QList<MissionWaypoint> m_waypoints;
    double m_arrivalRadiusM;      ///< 到达判定半径（米）
    bool m_active;                ///< 任务进行中
    int m_current;                ///< 当前目标航点下标（0 起）
    bool m_hovering;              ///< 悬停中（原地等待计时）
    QElapsedTimer m_hoverTimer;   ///< 悬停计时（喂点时检查，无内部定时器）
    int m_hoverSeconds;           ///< 当前航点悬停总时长
};

} // namespace opmap

#endif // WAYPOINTMISSIONENGINE_H
