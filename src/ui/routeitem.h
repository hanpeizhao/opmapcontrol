/**
******************************************************************************
* @file       routeitem.h
* @brief      车载导航路线绘制项：灰色已走段 + 蓝色剩余段 + 起/终点标记
*
*             挂在 MapGraphicItem 之下，连接 mapChanged 信号自行重投影刷新
*             （与 WaypointLineItem 同一套自刷新机制）。无路线时整体隐藏，
*             不参与鼠标交互。
******************************************************************************
*/
#ifndef ROUTEITEM_H
#define ROUTEITEM_H

#include <QObject>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QGraphicsEllipseItem>

#include "mapgraphicitem.h"
#include "route.h"

namespace opmap {

class RouteItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    enum { Type = UserType + 8 };

    explicit RouteItem(MapGraphicItem *map);

    int type() const { return Type; }
    QRectF boundingRect() const { return childrenBoundingRect(); }
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
    { Q_UNUSED(painter); Q_UNUSED(option); Q_UNUSED(widget); }

public slots:
    /// 设置/替换导航路线（重置已走进度为 0 并整体显示）
    void SetRoute(const opmap::Route &route);

    /// 已行驶距离（米）：按累计距离插值出分割点，拆分灰/蓝两段
    void SetTraveledDistance(double meters);

    /// 清空路线并整体隐藏
    void ClearRoute();

    /// 地图拖动/缩放后重投影全部绘制点
    void RefreshPos();

private:
    void hideChildren();

    MapGraphicItem *m_map;
    opmap::Route m_route;
    QList<double> m_cumDist;      ///< 累计距离（米），size == polyline.size()
    double m_traveledM;           ///< 已行驶距离（米）

    QGraphicsPathItem *m_traveledPath;    ///< 已走段（灰）
    QGraphicsPathItem *m_remainingPath;   ///< 剩余段（蓝）
    QGraphicsEllipseItem *m_originMark;   ///< 起点（绿）
    QGraphicsEllipseItem *m_destMark;     ///< 终点（红）
};

} // namespace opmap

#endif // ROUTEITEM_H
