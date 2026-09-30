/**
******************************************************************************
*
* @file       navigation_simulator.cpp
* @brief      导航模拟器：驱动 UAV 位置沿航点序列或路线折线推进
* @see        The GNU Public License (GPL) Version 3
* @{
*
*****************************************************************************/

#include "navigation_simulator.h"

#include "uavitem.h"
#include "waypointitem.h"
#include "geoutils.h"

#include <QtMath>

namespace {

const double kArriveThresholdM = 30.0;    ///< 到达判定距离（米）
const int kTickMs = 100;                  ///< 推进周期（毫秒）

} // anonymous namespace

NavigationSimulator::NavigationSimulator(QObject *parent)
    : QObject(parent),
      m_uav(0),
      m_currentPos(0, 0),
      m_index(0),
      m_speed(15.0),
      m_running(false),
      m_pathMode(false),
      m_paused(false)
{
    m_timer.setInterval(kTickMs);
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(onTick()));
}

void NavigationSimulator::setWaypointRoute(const QList<opmap::WayPointItem*> &wps)
{
    m_waypoints = wps;
    m_pathPoints.clear();
    m_pathAltitudes.clear();
    m_pathMode = false;
    m_index = 0;
    if (!wps.isEmpty())
        m_currentPos = wps.first()->Coord();
}

void NavigationSimulator::setPathRoute(const QList<opmap::PointLatLng> &pts, int altitudeMeters)
{
    m_pathPoints = pts;
    m_pathAltitudes.clear();
    for (int i = 0; i < pts.size(); ++i)
        m_pathAltitudes.append(altitudeMeters);
    m_waypoints.clear();
    m_pathMode = true;
    m_index = 0;
    if (!pts.isEmpty())
        m_currentPos = pts.first();
}

void NavigationSimulator::start()
{
    if (m_pathMode ? m_pathPoints.size() < 2 : m_waypoints.size() < 1)
        return;
    if (!m_paused) {
        m_index = 0;
        m_currentPos = m_pathMode ? m_pathPoints.first() : m_waypoints.first()->Coord();
    }
    m_paused = false;
    m_running = true;
    m_timer.start();
    emit statusUpdated(0, m_pathMode ? m_pathPoints.size() : m_waypoints.size(),
                       QString::fromUtf8("飞行中"));
}

void NavigationSimulator::pause()
{
    if (!m_running)
        return;
    m_paused = true;
    m_timer.stop();
    emit statusUpdated(m_index, m_pathMode ? m_pathPoints.size() : m_waypoints.size(),
                       QString::fromUtf8("已暂停"));
}

void NavigationSimulator::stop()
{
    m_timer.stop();
    m_running = false;
    m_paused = false;
    m_index = 0;
    emit statusUpdated(0, 0, QString::fromUtf8("已停止"));
}

void NavigationSimulator::advanceTarget()
{
    ++m_index;
    const int total = m_pathMode ? m_pathPoints.size() : m_waypoints.size();
    if (m_index >= total) {
        m_timer.stop();
        m_running = false;
        emit finished();
        emit statusUpdated(total, total, QString::fromUtf8("航线飞行完成"));
    }
}

void NavigationSimulator::onTick()
{
    if (!m_uav)
        return;

    const int total = m_pathMode ? m_pathPoints.size() : m_waypoints.size();
    if (m_index >= total) {
        m_timer.stop();
        m_running = false;
        return;
    }

    // 当前目标
    opmap::PointLatLng target;
    int altitude = 0;
    if (m_pathMode) {
        target = m_pathPoints.at(m_index);
        altitude = m_pathAltitudes.at(m_index);
    } else {
        opmap::WayPointItem *wp = m_waypoints.at(m_index);
        target = wp->Coord();
        altitude = (int)wp->Altitude();
    }

    const double step = m_speed * kTickMs / 1000.0;
    double dist = opmap::geoutils::haversineDistanceM(m_currentPos, target);

    // 到达判定：距离不足一步，或已跨过目标点
    while (dist <= kArriveThresholdM || dist <= step) {
        m_currentPos = target;

        if (m_pathMode) {
            advanceTarget();
            if (m_index >= total)
                return;
            target = m_pathPoints.at(m_index);
        } else {
            opmap::WayPointItem *wp = m_waypoints.at(m_index);
            wp->SetReached(true);
            emit waypointReached(m_index);
            advanceTarget();
            if (m_index >= total)
                return;
            target = m_waypoints.at(m_index)->Coord();
            altitude = (int)m_waypoints.at(m_index)->Altitude();
        }

        dist = opmap::geoutils::haversineDistanceM(m_currentPos, target);
    }

    const double heading = opmap::geoutils::bearingDeg(m_currentPos, target);
    m_currentPos = opmap::geoutils::destPoint(m_currentPos, heading, step);

    m_uav->SetUAVPos(m_currentPos, altitude);
    m_uav->SetUAVHeading(heading);
    emit statusUpdated(m_index, total, QString::fromUtf8("飞往目标 %1/%2").arg(m_index + 1).arg(total));
}
