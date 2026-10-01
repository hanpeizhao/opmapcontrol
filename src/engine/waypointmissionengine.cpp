/**
******************************************************************************
*
* @file       waypointmissionengine.cpp
* @brief      航点任务飞行状态机实现：纯喂点驱动，真机/模拟器同构接入
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
#include "waypointmissionengine.h"

#include "pureprojection.h"

namespace opmap {

WaypointMissionEngine::WaypointMissionEngine(QObject *parent)
    : QObject(parent),
      m_arrivalRadiusM(15.0),
      m_active(false),
      m_current(0),
      m_hovering(false),
      m_hoverSeconds(0)
{
}

void WaypointMissionEngine::SetMission(QList<MissionWaypoint> const& waypoints,
                                       double arrivalRadiusMeters)
{
    StopMission();
    m_waypoints = waypoints;
    m_arrivalRadiusM = arrivalRadiusMeters > 0 ? arrivalRadiusMeters : 15.0;
}

void WaypointMissionEngine::StartMission()
{
    if (m_waypoints.isEmpty())
        return;

    m_active = true;
    m_current = 0;
    m_hovering = false;
    m_hoverSeconds = 0;

    const MissionWaypoint &wp = m_waypoints.first();
    m_hoverSeconds = wp.action == ActionHover ? wp.hoverSeconds : 0;

    emit missionStarted();
    emit currentWaypointChanged(0);
}

void WaypointMissionEngine::StopMission()
{
    const bool wasActive = m_active;
    m_active = false;
    m_hovering = false;
    m_current = 0;
    if (wasActive)
        emit hoverStateChanged(false, 0);
}

void WaypointMissionEngine::ClearMission()
{
    StopMission();
    m_waypoints.clear();
}

void WaypointMissionEngine::UpdatePosition(opmap::PointLatLng const& position)
{
    if (!m_active || m_current >= m_waypoints.size())
        return;

    // 悬停中：只推进计时（喂点即心跳），时间到则解除悬停并推进下一航点
    if (m_hovering) {
        if (m_hoverTimer.elapsed() >= m_hoverSeconds * 1000) {
            m_hovering = false;
            emit hoverStateChanged(false, 0);
            advanceToNext();
        }
        return;
    }

    const MissionWaypoint &wp = m_waypoints.at(m_current);

    // 到达判定：与目标航点的大圆距离（DistanceBetweenLatLng 返回千米）
    const double distM = PureProjection::DistanceBetweenLatLng(position, wp.position) * 1000.0;
    if (distM > m_arrivalRadiusM)
        return;

    emit waypointReached(m_current, wp.action);

    if (wp.action == ActionHover && wp.hoverSeconds > 0) {
        m_hovering = true;
        m_hoverTimer.restart();
        emit actionTriggered(m_current, wp.action);
        emit hoverStateChanged(true, wp.hoverSeconds);
        return;   // 悬停计时由后续喂点推进
    }

    if (wp.action == ActionPhoto)
        emit actionTriggered(m_current, wp.action);

    advanceToNext();
}

void WaypointMissionEngine::advanceToNext()
{
    ++m_current;
    if (m_current >= m_waypoints.size()) {
        m_active = false;
        emit missionFinished();
        return;
    }

    const MissionWaypoint &wp = m_waypoints.at(m_current);
    m_hoverSeconds = wp.action == ActionHover ? wp.hoverSeconds : 0;
    emit currentWaypointChanged(m_current);
}

} // namespace opmap
