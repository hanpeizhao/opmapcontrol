/**
******************************************************************************
* @file       coordtransform.cpp
* @brief      WGS-84 与 GCJ-02 坐标系转换的标准算法实现
******************************************************************************
*/
#include "coordtransform.h"
#include <qmath.h>

namespace core {
namespace coordtransform {

static const double PI          = 3.14159265358979323846;
static const double A           = 6378245.0;          // 长半轴
static const double EE          = 0.00669342162296594323; // 偏心率平方

bool outOfChina(double lat, double lng)
{
    // 中国领土的大致外包范围
    return (lng < 72.004 || lng > 137.8347 ||
            lat < 0.8293 || lat > 55.8271);
}

// 纬度方向的偏移量
static double transformLat(double x, double y)
{
    double ret = -100.0 + 2.0 * x + 3.0 * y + 0.2 * y * y + 0.1 * x * y
                 + 0.2 * qSqrt(qFabs(x));
    ret += (20.0 * qSin(6.0 * x * PI) + 20.0 * qSin(2.0 * x * PI)) * 2.0 / 3.0;
    ret += (20.0 * qSin(y * PI) + 40.0 * qSin(y / 3.0 * PI)) * 2.0 / 3.0;
    ret += (160.0 * qSin(y / 12.0 * PI) + 320.0 * qSin(y * PI / 30.0)) * 2.0 / 3.0;
    return ret;
}

// 经度方向的偏移量
static double transformLng(double x, double y)
{
    double ret = 300.0 + x + 2.0 * y + 0.1 * x * x + 0.1 * x * y
                 + 0.1 * qSqrt(qFabs(x));
    ret += (20.0 * qSin(6.0 * x * PI) + 20.0 * qSin(2.0 * x * PI)) * 2.0 / 3.0;
    ret += (20.0 * qSin(x * PI) + 40.0 * qSin(x / 3.0 * PI)) * 2.0 / 3.0;
    ret += (150.0 * qSin(x / 12.0 * PI) + 300.0 * qSin(x / 30.0 * PI)) * 2.0 / 3.0;
    return ret;
}

internals::PointLatLng WGS84ToGCJ02(const internals::PointLatLng &pt)
{
    double lat = pt.Lat();
    double lng = pt.Lng();

    if (outOfChina(lat, lng))
    {
        return pt;
    }

    double dLat = transformLat(lng - 105.0, lat - 35.0);
    double dLng = transformLng(lng - 105.0, lat - 35.0);
    double radLat = lat / 180.0 * PI;
    double magic = qSin(radLat);
    magic = 1 - EE * magic * magic;
    double sqrtMagic = qSqrt(magic);
    dLat = (dLat * 180.0) / ((A * (1 - EE)) / (magic * sqrtMagic) * PI);
    dLng = (dLng * 180.0) / (A / sqrtMagic * qCos(radLat) * PI);

    return internals::PointLatLng(lat + dLat, lng + dLng);
}

internals::PointLatLng GCJ02ToWGS84(const internals::PointLatLng &pt)
{
    // GCJ-02 无解析逆变换，用一步近似逆：在目标点处重新求偏移并扣除
    internals::PointLatLng gcj = WGS84ToGCJ02(pt);
    double dLat = gcj.Lat() - pt.Lat();
    double dLng = gcj.Lng() - pt.Lng();

    return internals::PointLatLng(pt.Lat() - dLat, pt.Lng() - dLng);
}

} // namespace coordtransform
} // namespace core
