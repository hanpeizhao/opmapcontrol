/**
******************************************************************************
*
* @file       waypoint_flight_simulator.cpp
* @brief      航点飞行模拟数据源实现：定时推进位置并向目标航点插值
* @see        The GNU Public License (GPL) Version 3
* @{
*
*****************************************************************************/
#include "waypoint_flight_simulator.h"

#include <QtCore/QTimer>
#include <QtMath>

namespace {

const int kTickMs = 100;          ///< 推进周期
const double kArriveMeters = 15;  ///< 到达判定半径（与 demo 侧 SetAutoSetDistance 保持一致）

} // anonymous namespace

WaypointFlightSimulator::WaypointFlightSimulator(QObject *parent)
    : QObject(parent),
      m_timer(new QTimer(this)),
      m_pos(0, 0),
      m_speedMps(25.0),
      m_target(0),
      m_active(false)
{
    connect(m_timer, SIGNAL(timeout()), this, SLOT(onTick()));
}

void WaypointFlightSimulator::start(const opmap::PointLatLng &startPos,
                                    const QList<opmap::PointLatLng> &waypoints,
                                    double speedMps)
{
    if (waypoints.isEmpty())
        return;

    m_waypoints = waypoints;
    m_pos = startPos;
    m_speedMps = speedMps > 0 ? speedMps : 25.0;
    m_target = 0;
    m_active = true;
    emit positionChanged(m_pos, 0.0, 0, m_waypoints.size());
    m_timer->start(kTickMs);
}

void WaypointFlightSimulator::stop()
{
    m_timer->stop();
    m_active = false;
}

void WaypointFlightSimulator::onTick()
{
    if (m_target >= m_waypoints.size())
    {
        stop();
        emit finished();
        return;
    }

    const opmap::PointLatLng target = m_waypoints.at(m_target);

    // 距离（DistanceBetweenLatLng 返回千米）与方位角（度，正北 0 顺时针）
    const double rad = M_PI / 180.0;
    const double distM = opmap::PureProjection::DistanceBetweenLatLng(m_pos, target) * 1000.0;
    const double dLat = (target.Lat() - m_pos.Lat()) * rad;
    const double dLng = (target.Lng() - m_pos.Lng()) * rad;
    double heading = qAtan2(dLng * qCos(m_pos.Lat() * rad), dLat) / rad;
    if (heading < 0)
        heading += 360.0;

    const double stepM = m_speedMps * (kTickMs / 1000.0);
    if (distM <= qMax(stepM, kArriveMeters))
    {
        m_pos = target;
        ++m_target;
        emit waypointPassed(m_target, m_waypoints.size());
        if (m_target >= m_waypoints.size())
        {
            stop();
            emit positionChanged(m_pos, heading, m_target, m_waypoints.size());
            emit finished();
            return;
        }
    }
    else
    {
        const double ratio = stepM / distM;
        m_pos = opmap::PointLatLng(m_pos.Lat() + (target.Lat() - m_pos.Lat()) * ratio,
                                   m_pos.Lng() + (target.Lng() - m_pos.Lng()) * ratio);
    }

    emit positionChanged(m_pos, heading, m_target, m_waypoints.size());
}
