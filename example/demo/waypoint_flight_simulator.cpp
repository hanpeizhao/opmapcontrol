/**
******************************************************************************
*
* @file       waypoint_flight_simulator.cpp
* @brief      航点飞行模拟遥测源实现：定时朝库下发的目标点匀速插值推进
* @see        The GNU Public License (GPL) Version 3
* @{
*
*****************************************************************************/
#include "waypoint_flight_simulator.h"

#include <QtCore/QTimer>
#include <QtMath>

namespace {

const int kTickMs = 100;  ///< 推进周期

} // anonymous namespace

WaypointFlightSimulator::WaypointFlightSimulator(QObject *parent)
    : QObject(parent),
      m_timer(new QTimer(this)),
      m_pos(0, 0),
      m_target(0, 0),
      m_hasTarget(false),
      m_speedMps(25.0),
      m_lastHeading(0.0),
      m_hovering(false),
      m_active(false)
{
    connect(m_timer, SIGNAL(timeout()), this, SLOT(onTick()));
}

void WaypointFlightSimulator::start(const opmap::PointLatLng &startPos, double speedMps)
{
    m_pos = startPos;
    m_target = startPos;
    m_hasTarget = false;
    m_speedMps = speedMps > 0 ? speedMps : 25.0;
    m_lastHeading = 0.0;
    m_hovering = false;
    m_active = true;
    emit positionChanged(m_pos, 0.0);
    m_timer->start(kTickMs);
}

void WaypointFlightSimulator::stop()
{
    m_timer->stop();
    m_active = false;
    m_hovering = false;
    m_hasTarget = false;
}

void WaypointFlightSimulator::setTarget(const opmap::PointLatLng &target)
{
    m_target = target;
    m_hasTarget = true;
}

void WaypointFlightSimulator::setHovering(bool hovering)
{
    m_hovering = hovering;
}

void WaypointFlightSimulator::onTick()
{
    if (!m_active)
        return;

    // 悬停中：原地保持遥测心跳（悬停计时/解除由库任务引擎判定）
    if (m_hovering || !m_hasTarget)
    {
        emit positionChanged(m_pos, m_lastHeading);
        return;
    }

    // 朝目标匀速直线推进；到达判定完全由库喂点完成（本类不做任何任务判定）
    const double rad = M_PI / 180.0;
    const double distM = opmap::PureProjection::DistanceBetweenLatLng(m_pos, m_target) * 1000.0;
    const double dLat = (m_target.Lat() - m_pos.Lat()) * rad;
    const double dLng = (m_target.Lng() - m_pos.Lng()) * rad;
    double heading = qAtan2(dLng * qCos(m_pos.Lat() * rad), dLat) / rad;
    if (heading < 0)
        heading += 360.0;

    const double stepM = m_speedMps * (kTickMs / 1000.0);
    if (distM <= stepM)
    {
        m_pos = m_target;   // 直接压到目标点，库喂点判定到达并推进
    }
    else
    {
        const double ratio = stepM / distM;
        m_pos = opmap::PointLatLng(m_pos.Lat() + (m_target.Lat() - m_pos.Lat()) * ratio,
                                   m_pos.Lng() + (m_target.Lng() - m_pos.Lng()) * ratio);
    }

    m_lastHeading = heading;
    emit positionChanged(m_pos, heading);
}
