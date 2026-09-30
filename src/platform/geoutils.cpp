/**
******************************************************************************
* @file       geoutils.cpp
* @brief      球面几何工具实现（见 geoutils.h）
******************************************************************************
*/

#include "geoutils.h"

#include <QtMath>
#include <cmath>

namespace opmap {
namespace geoutils {

namespace {

const double kEarthRadiusM = 6371000.0;   ///< 地球平均半径（米）
const double kPi = 3.14159265358979323846;

double degToRad(double d) { return d * kPi / 180.0; }
double radToDeg(double r) { return r * 180.0 / kPi; }

} // anonymous namespace

double haversineDistanceM(const opmap::PointLatLng &a, const opmap::PointLatLng &b)
{
    const double lat1 = degToRad(a.Lat());
    const double lat2 = degToRad(b.Lat());
    const double dLat = degToRad(b.Lat() - a.Lat());
    const double dLng = degToRad(b.Lng() - a.Lng());
    const double h = qSin(dLat / 2) * qSin(dLat / 2)
            + qCos(lat1) * qCos(lat2) * qSin(dLng / 2) * qSin(dLng / 2);
    return 2 * kEarthRadiusM * qAsin(qSqrt(h));
}

double bearingDeg(const opmap::PointLatLng &from, const opmap::PointLatLng &to)
{
    const double lat1 = degToRad(from.Lat());
    const double lat2 = degToRad(to.Lat());
    const double dLng = degToRad(to.Lng() - from.Lng());
    const double y = qSin(dLng) * qCos(lat2);
    const double x = qCos(lat1) * qSin(lat2) - qSin(lat1) * qCos(lat2) * qCos(dLng);
    double bear = radToDeg(qAtan2(y, x));
    // 归一化到 [0, 360)
    bear = std::fmod(bear, 360.0);
    if (bear < 0)
        bear += 360.0;
    return bear;
}

opmap::PointLatLng destPoint(const opmap::PointLatLng &src, double bearingDeg, double distMeters)
{
    const double lat1 = degToRad(src.Lat());
    const double lng1 = degToRad(src.Lng());
    const double theta = degToRad(bearingDeg);
    const double delta = distMeters / kEarthRadiusM;

    const double lat2 = qAsin(qSin(lat1) * qCos(delta)
                              + qCos(lat1) * qSin(delta) * qCos(theta));
    const double lng2 = lng1 + qAtan2(qSin(theta) * qSin(delta) * qCos(lat1),
                                      qCos(delta) - qSin(lat1) * qSin(lat2));
    return opmap::PointLatLng(radToDeg(lat2), radToDeg(lng2));
}

geoutils::SegmentHit pointToSegmentM(const opmap::PointLatLng &p,
                                     const opmap::PointLatLng &segA,
                                     const opmap::PointLatLng &segB,
                                     int segIndex)
{
    geoutils::SegmentHit hit;
    hit.distanceM = 0;
    hit.segIndex = segIndex;
    hit.t = 0;

    // 以段中点为中心的等距圆柱局部平面近似
    const double midLat = degToRad((segA.Lat() + segB.Lat()) / 2);
    const double kx = kEarthRadiusM * qCos(midLat) * kPi / 180.0;   // 经度弧长系数（米/度）
    const double ky = kEarthRadiusM * kPi / 180.0;                  // 纬度弧长系数（米/度）

    const double px = p.Lng() * kx;
    const double py = p.Lat() * ky;
    const double ax = segA.Lng() * kx;
    const double ay = segA.Lat() * ky;
    const double bx = segB.Lng() * kx;
    const double by = segB.Lat() * ky;

    const double abx = bx - ax;
    const double aby = by - ay;
    const double ab2 = abx * abx + aby * aby;
    if (ab2 <= 0) {
        // 退化段（两点重合）
        const double dx = px - ax;
        const double dy = py - ay;
        hit.distanceM = qSqrt(dx * dx + dy * dy);
        hit.t = 0;
        return hit;
    }

    double t = ((px - ax) * abx + (py - ay) * aby) / ab2;
    t = qBound(0.0, t, 1.0);
    const double cx = ax + t * abx;
    const double cy = ay + t * aby;
    const double dx = px - cx;
    const double dy = py - cy;

    hit.distanceM = qSqrt(dx * dx + dy * dy);
    hit.t = t;
    return hit;
}

QList<opmap::PointLatLng> simplifyPolyline(const QList<opmap::PointLatLng> &pts, double toleranceM)
{
    // Douglas–Peucker 递归抽稀
    if (pts.size() <= 2)
        return pts;

    // 找离首末点连线最远的点
    double maxDist = 0;
    int maxIdx = 0;
    const opmap::PointLatLng &first = pts.first();
    const opmap::PointLatLng &last = pts.last();
    for (int i = 1; i < pts.size() - 1; ++i) {
        const SegmentHit hit = pointToSegmentM(pts.at(i), first, last, i - 1);
        if (hit.distanceM > maxDist) {
            maxDist = hit.distanceM;
            maxIdx = i;
        }
    }

    if (maxDist > toleranceM) {
        // 分两段递归
        QList<opmap::PointLatLng> leftPts = pts.mid(0, maxIdx + 1);
        QList<opmap::PointLatLng> rightPts = pts.mid(maxIdx);
        const QList<opmap::PointLatLng> left = simplifyPolyline(leftPts, toleranceM);
        const QList<opmap::PointLatLng> right = simplifyPolyline(rightPts, toleranceM);
        QList<opmap::PointLatLng> result = left;
        result.removeLast();
        result.append(right);
        return result;
    }

    QList<opmap::PointLatLng> result;
    result.append(first);
    result.append(last);
    return result;
}

} // namespace geoutils
} // namespace opmap
