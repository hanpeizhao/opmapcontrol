/**
******************************************************************************
* @file       navigationengine.cpp
* @brief      车载导航状态机实现（见 navigationengine.h）
******************************************************************************
*/

#include "navigationengine.h"

#include <QDateTime>

#include "geoutils.h"
#include "abstractrouteprovider.h"

namespace opmap {

namespace {

const int kInstructionNearM = 30;   ///< 指令切换阈值：临近步骤 30 米内提示"即将"

} // anonymous namespace

NavigationEngine::NavigationEngine(QObject *parent)
    : QObject(parent),
      m_provider(0),
      m_state(Idle),
      m_planOnly(false),
      m_lastPos(0, 0),
      m_hasLastPos(false),
      m_lastSegIdx(0),
      m_offRouteCount(0),
      m_lastRerouteMs(0),
      m_autoReroute(true),
      m_offRouteThresholdM(50.0),
      m_offRouteConsecutiveFixes(3),
      m_arrivalThresholdM(30.0),
      m_minRerouteIntervalMs(5000)
{
}

void NavigationEngine::SetRouteProvider(AbstractRouteProvider *provider)
{
    if (m_provider == provider)
        return;
    if (m_provider)
        disconnect(m_provider, 0, this, 0);
    m_provider = provider;
    if (m_provider) {
        connect(m_provider, SIGNAL(routeReady(opmap::Route)),
                this, SLOT(onRouteReady(opmap::Route)));
        connect(m_provider, SIGNAL(routeFailed(QString)),
                this, SLOT(onRouteFailed(QString)));
    }
}

void NavigationEngine::NavigateTo(const opmap::PointLatLng &from, const opmap::PointLatLng &dest)
{
    if (!m_provider) {
        emit navigationFailed(QString::fromUtf8("未设置路由服务"));
        return;
    }
    if (m_provider->isBusy()) {
        emit navigationFailed(QString::fromUtf8("路由服务正忙，请稍后重试"));
        return;
    }
    m_planOnly = false;
    m_destination = dest;
    m_route = opmap::Route();
    m_cumDist.clear();
    m_lastSegIdx = 0;
    m_offRouteCount = 0;
    m_state = Planning;
    m_provider->requestRoute(from, dest);
}

void NavigationEngine::PlanRoute(const opmap::PointLatLng &from, const opmap::PointLatLng &dest)
{
    if (!m_provider) {
        emit navigationFailed(QString::fromUtf8("未设置路由服务"));
        return;
    }
    if (m_provider->isBusy()) {
        emit navigationFailed(QString::fromUtf8("路由服务正忙，请稍后重试"));
        return;
    }
    m_planOnly = true;
    m_destination = dest;
    m_route = opmap::Route();
    m_cumDist.clear();
    m_lastSegIdx = 0;
    m_offRouteCount = 0;
    m_state = Planning;
    m_provider->requestRoute(from, dest);
}

void NavigationEngine::StartRoute(const opmap::Route &route)
{
    if (!route.isValid()) {
        emit navigationFailed(QString::fromUtf8("路线无效"));
        return;
    }
    installRoute(route);
    m_state = Navigating;
    emit routePlanned(route);
}

void NavigationEngine::UpdatePosition(const opmap::PointLatLng &pos)
{
    m_lastPos = pos;
    m_hasLastPos = true;

    // 规划/重规划期间只缓存位置，不做进度与偏航判定
    if (m_state != Navigating || !m_route.isValid())
        return;

    const geoutils::SegmentHit hit = locateNearestSegment(pos);
    const double segLen = m_cumDist.at(hit.segIndex + 1) - m_cumDist.at(hit.segIndex);
    const double traveled = m_cumDist.at(hit.segIndex) + hit.t * segLen;
    const double total = m_cumDist.last();
    const double remaining = qMax(0.0, total - traveled);
    const int remainingS = (total > 0 && m_route.totalDurationSeconds > 0)
            ? (int)(m_route.totalDurationSeconds * remaining / total) : 0;

    emit progressUpdated(traveled, remaining, remainingS, instructionFor(hit.segIndex, traveled));

    if (remaining <= m_arrivalThresholdM) {
        emit arrived();
        Stop();
        return;
    }
    checkOffRoute(pos, hit.distanceM);
}

void NavigationEngine::Stop()
{
    m_state = Idle;
    m_planOnly = false;
    m_route = opmap::Route();
    m_cumDist.clear();
    m_lastSegIdx = 0;
    m_offRouteCount = 0;
}

void NavigationEngine::onRouteReady(const opmap::Route &route)
{
    if (!route.isValid()) {
        onRouteFailed(QString::fromUtf8("路由服务返回的路线无效"));
        return;
    }
    if (m_state == Planning) {
        installRoute(route);
        if (m_planOnly) {
            // 预览模式：只绘制路线，不进入导航状态机
            m_planOnly = false;
            m_state = Idle;
        } else {
            m_state = Navigating;
        }
        emit routePlanned(route);
    } else if (m_state == Rerouting) {
        installRoute(route);
        m_state = Navigating;
        emit rerouteReady(route);
    }
    // 其余状态（已 Stop 等）收到迟到结果直接丢弃
}

void NavigationEngine::onRouteFailed(const QString &reason)
{
    if (m_state == Planning) {
        m_state = Idle;
        emit navigationFailed(reason);
    } else if (m_state == Rerouting) {
        // 重规划失败非致命：继续沿旧路线行驶
        m_state = Navigating;
        emit rerouteFailed(reason);
    }
}

void NavigationEngine::installRoute(const opmap::Route &route)
{
    m_route = route;
    m_cumDist.clear();
    m_cumDist.reserve(route.polyline.size());
    m_cumDist.append(0.0);
    for (int i = 1; i < route.polyline.size(); ++i) {
        m_cumDist.append(m_cumDist.last()
                         + geoutils::haversineDistanceM(route.polyline.at(i - 1), route.polyline.at(i)));
    }
    m_lastSegIdx = 0;
    m_offRouteCount = 0;
}

geoutils::SegmentHit NavigationEngine::locateNearestSegment(const opmap::PointLatLng &pos)
{
    const int segCount = m_route.polyline.size() - 1;

    // 窗口查找：车辆沿路推进时最近段索引单调前移，先在缓存索引附近找
    const int from = qBound(0, m_lastSegIdx - 3, segCount - 1);
    const int to = qMin(segCount - 1, m_lastSegIdx + 40);

    geoutils::SegmentHit best;
    best.distanceM = 0;
    best.segIndex = from;
    best.t = 0;
    double bestDist = -1.0;
    for (int i = from; i <= to; ++i) {
        const geoutils::SegmentHit h = geoutils::pointToSegmentM(
                    pos, m_route.polyline.at(i), m_route.polyline.at(i + 1), i);
        if (bestDist < 0 || h.distanceM < bestDist) {
            bestDist = h.distanceM;
            best = h;
        }
    }

    // 窗口最小距离过大（如索引回跳/路线刚切换）时全量扫描兜底
    if (bestDist > 2.0 * m_offRouteThresholdM) {
        for (int i = 0; i < segCount; ++i) {
            const geoutils::SegmentHit h = geoutils::pointToSegmentM(
                        pos, m_route.polyline.at(i), m_route.polyline.at(i + 1), i);
            if (h.distanceM < bestDist) {
                bestDist = h.distanceM;
                best = h;
            }
        }
    }

    m_lastSegIdx = best.segIndex;
    return best;
}

QString NavigationEngine::instructionFor(int segIdx, double traveledM) const
{
    for (int i = 0; i < m_route.steps.size(); ++i) {
        const RouteStep &step = m_route.steps.at(i);
        if (step.startIndex > segIdx && step.startIndex < m_cumDist.size()) {
            const double d = m_cumDist.at(step.startIndex) - traveledM;
            if (d < kInstructionNearM)
                return QString::fromUtf8("即将 %1").arg(step.instruction);
            return QString::fromUtf8("前方 %1 米 %2")
                    .arg((int)(d + 0.5)).arg(step.instruction);
        }
    }
    return QString::fromUtf8("沿当前道路行驶");
}

void NavigationEngine::checkOffRoute(const opmap::PointLatLng &pos, double devM)
{
    if (devM <= m_offRouteThresholdM) {
        m_offRouteCount = 0;    // 回到路线上，重置连续偏航计数
        return;
    }

    ++m_offRouteCount;
    if (m_offRouteCount == m_offRouteConsecutiveFixes)
        emit offRouteDetected(pos, devM);

    // 自动重规划：受 provider 忙碌与最小重规划间隔双重抑制，防止风暴
    if (m_offRouteCount >= m_offRouteConsecutiveFixes
            && m_autoReroute && m_provider && !m_provider->isBusy()) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - m_lastRerouteMs >= m_minRerouteIntervalMs) {
            m_lastRerouteMs = now;
            m_state = Rerouting;
            const PointLatLng from = m_hasLastPos ? m_lastPos : pos;
            m_provider->requestRoute(from, m_destination);
        }
    }
}

} // namespace opmap
