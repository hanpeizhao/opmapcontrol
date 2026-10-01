/**
******************************************************************************
* @file       route.h
* @brief      路径规划结果值类型：WGS-84 折线 + 分步转向指令 + 总距离/总时长
*
*             由路径规划 provider（OsrmRouteProvider/AmapRouteProvider）产出，
*             NavigationEngine 与 RouteItem 消费。
******************************************************************************
*/
#ifndef ROUTE_H
#define ROUTE_H

#include <QMetaType>
#include <QList>
#include <QString>

#include "pointlatlng.h"

namespace opmap {

/// 一条导航转向步骤；startIndex 指向 Route::polyline 中该步骤的起点索引
struct RouteStep
{
    QString instruction;     ///< 中文指令文本（如 "右转"、"进入环岛"）
    int startIndex;          ///< 该步骤在 Route::polyline 中的起始点索引
    double distanceMeters;   ///< 该步骤距离（米）
    int durationSeconds;     ///< 该步骤预计耗时（秒）

    RouteStep() : startIndex(0), distanceMeters(0), durationSeconds(0) {}
};

/// 规划结果：WGS-84 完整折线 + 分步指令 + 总距离/总时长
struct Route
{
    QList<opmap::PointLatLng> polyline;   ///< WGS-84 完整折线
    QList<opmap::RouteStep> steps;        ///< 按 startIndex 升序
    double totalDistanceMeters;           ///< 总距离（米）
    int totalDurationSeconds;             ///< 总预计耗时（秒）

    Route() : totalDistanceMeters(0), totalDurationSeconds(0) {}

    bool isValid() const { return polyline.size() >= 2; }
};

} // namespace opmap

Q_DECLARE_METATYPE(opmap::Route)
Q_DECLARE_METATYPE(QList<opmap::Route>)

#endif // ROUTE_H
