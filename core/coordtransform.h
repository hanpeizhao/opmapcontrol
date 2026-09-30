/**
******************************************************************************
* @file       coordtransform.h
* @brief      地图坐标系转换：WGS-84 / GCJ-02（火星坐标）
*
*              各地图源使用的坐标系不同：
*              - WGS-84：国际标准（GPS 原始坐标），OSM、ArcGIS、Google 国际版
*              - GCJ-02：国测局加密坐标（俗称火星坐标），高德、谷歌中国、腾讯
*
*              本模块提供两个坐标系之间的相互转换，中国范围外为恒等变换。
******************************************************************************
*/
#ifndef COORDTRANSFORM_H
#define COORDTRANSFORM_H

#include "../internals/pointlatlng.h"

namespace core {
namespace coordtransform {

/// <summary>
/// WGS-84 坐标转换为 GCJ-02 坐标（中国境外原样返回）
/// </summary>
internals::PointLatLng WGS84ToGCJ02(const internals::PointLatLng &pt);

/// <summary>
/// GCJ-02 坐标转换为 WGS-84 坐标（近似逆变换，误差亚米级）
/// </summary>
internals::PointLatLng GCJ02ToWGS84(const internals::PointLatLng &pt);

/// <summary>
/// 判断坐标是否位于中国范围（GCJ-02 加密只在中国境内生效）
/// </summary>
bool outOfChina(double lat, double lng);

} // namespace coordtransform
} // namespace core

#endif // COORDTRANSFORM_H
