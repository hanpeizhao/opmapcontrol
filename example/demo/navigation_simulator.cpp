/**
******************************************************************************
*
* @file       navigation_simulator.cpp
* @brief      跟车模拟器：沿导航路线折线匀速推进车辆位置，驱动库内导航引擎
* @see        The GNU Public License (GPL) Version 3
* @{
*
*****************************************************************************/

#include "navigation_simulator.h"

#include "geoutils.h"

namespace {

const int kTickMs = 100;   ///< 推进周期（毫秒）

} // anonymous namespace

NavigationSimulator::NavigationSimulator(QObject *parent)
    : QObject(parent),
      m_currentPos(0, 0),
      m_heading(0),
      m_index(0),
      m_speed(15.0),
      m_running(false),
      m_paused(false)
{
    m_timer.setInterval(kTickMs);
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(onTick()));
}

void NavigationSimulator::setPath(const QList<opmap::PointLatLng> &pts)
{
    m_path = pts;
    m_index = 0;
    if (!pts.isEmpty())
        m_currentPos = pts.first();
}

void NavigationSimulator::reroute(const QList<opmap::PointLatLng> &pts)
{
    if (pts.size() < 2)
        return;
    m_path = pts;
    // 找距当前位置最近的折线点，从其下一点继续跟车
    int best = 0;
    double bestDist = -1.0;
    for (int i = 0; i < pts.size(); ++i) {
        const double d = opmap::geoutils::haversineDistanceM(m_currentPos, pts.at(i));
        if (bestDist < 0 || d < bestDist) {
            bestDist = d;
            best = i;
        }
    }
    m_index = qMin(best + 1, pts.size() - 1);
}

void NavigationSimulator::start()
{
    if (m_path.size() < 2)
        return;
    if (!m_paused) {
        m_index = 0;
        m_currentPos = m_path.first();
    }
    m_paused = false;
    m_running = true;
    m_timer.start();
    emit statusUpdated(0, m_path.size(), QString::fromUtf8("行车中"));
}

void NavigationSimulator::pause()
{
    if (!m_running)
        return;
    m_paused = true;
    m_timer.stop();
    emit statusUpdated(m_index, m_path.size(), QString::fromUtf8("已暂停"));
}

void NavigationSimulator::stop()
{
    m_timer.stop();
    m_running = false;
    m_paused = false;
    m_index = 0;
    emit statusUpdated(0, 0, QString::fromUtf8("已停止"));
}

void NavigationSimulator::simulateYaw()
{
    if (!m_running || m_path.isEmpty())
        return;
    // 沿当前航向的垂直方向甩出 80~120 米，模拟驶离路线
    const double side = (qrand() % 2 == 0) ? 90.0 : 270.0;
    const double dist = 80.0 + (qrand() % 41);
    m_currentPos = opmap::geoutils::destPoint(m_currentPos, m_heading + side, dist);
    emit positionChanged(m_currentPos, m_heading);
}

void NavigationSimulator::onTick()
{
    if (m_index >= m_path.size()) {
        m_timer.stop();
        m_running = false;
        return;
    }

    const double step = m_speed * kTickMs / 1000.0;
    double dist = opmap::geoutils::haversineDistanceM(m_currentPos, m_path.at(m_index));

    // 到达判定：距离不足一步直接落点并切下一目标
    while (dist <= step) {
        m_currentPos = m_path.at(m_index);
        emit positionChanged(m_currentPos, m_heading);
        ++m_index;
        if (m_index >= m_path.size()) {
            m_timer.stop();
            m_running = false;
            emit finished();
            emit statusUpdated(m_path.size(), m_path.size(), QString::fromUtf8("跟车完成"));
            return;
        }
        dist = opmap::geoutils::haversineDistanceM(m_currentPos, m_path.at(m_index));
    }

    m_heading = opmap::geoutils::bearingDeg(m_currentPos, m_path.at(m_index));
    m_currentPos = opmap::geoutils::destPoint(m_currentPos, m_heading, step);

    emit positionChanged(m_currentPos, m_heading);
    emit statusUpdated(m_index, m_path.size(), QString::fromUtf8("行车中"));
}
