/**
******************************************************************************
* @file       geoutils.h
* @brief      球面几何工具：距离 / 方位角 / 目标点 / 点到线段距离 / 折线抽稀
*
*             作用于 WGS-84 经纬度（PointLatLng），供导航进度计算、
*             偏航检测、航向推算、模拟行车等模块共用，平台层无 Qt 依赖。
******************************************************************************
*/
#ifndef GEOUTILS_H
#define GEOUTILS_H

#include <QList>
#include "pointlatlng.h"

namespace opmap {
namespace geoutils {

/// 两点球面距离（米，Haversine 公式）
double haversineDistanceM(const opmap::PointLatLng &a, const opmap::PointLatLng &b);

/// 从 from 看向 to 的方位角（度，正北为 0，顺时针，归一化到 [0,360)）
double bearingDeg(const opmap::PointLatLng &from, const opmap::PointLatLng &to);

/// 由源点 + 方位角（度）+ 距离（米）求目标点（球面正解）
opmap::PointLatLng destPoint(const opmap::PointLatLng &src, double bearingDeg, double distMeters);

/// 点到线段的垂直距离结果：距离（米）、段索引、段上投影参数 t∈[0,1]
struct SegmentHit
{
    double distanceM;   ///< 垂直距离（米）
    int segIndex;       ///< 段索引（段起点在折线中的下标）
    double t;           ///< 投影点在段上的参数位置 0..1
};

/// 点到线段的垂直距离（米）：以段中点为中心做等距圆柱局部平面近似，
/// 在偏航检测（几十米量级）的距离尺度下误差可忽略
SegmentHit pointToSegmentM(const opmap::PointLatLng &p,
                           const opmap::PointLatLng &segA,
                           const opmap::PointLatLng &segB,
                           int segIndex);

/// Douglas–Peucker 折线抽稀（容差米），用于长路线降点保流畅
QList<opmap::PointLatLng> simplifyPolyline(const QList<opmap::PointLatLng> &pts, double toleranceM);

} // namespace geoutils
} // namespace opmap

#endif // GEOUTILS_H
