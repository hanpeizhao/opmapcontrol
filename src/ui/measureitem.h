/**
******************************************************************************
*
* @file       measureitem.h
* @brief      地图测距图元：多点折线测距，逐点固化顶点、实时标注每段与
*             总距离（大圆距离），支持多段测量共存与一键清除
* @see        The GNU Public License (GPL) Version 3
* @defgroup   OPMapWidget
* @{
*
*****************************************************************************/
/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/
#ifndef MEASUREITEM_H
#define MEASUREITEM_H

#include <QGraphicsItem>
#include <QFont>
#include <QPen>
#include <QPolygonF>
#include "pointlatlng.h"
#include "mapgraphicitem.h"
#include "mapanchoreditem.h"

namespace opmap {

/**
* @brief 测距折线图元：挂在地图画布下，随地图拖动/缩放自动跟随
*        （实现 MapAnchoredItem 纳入统一刷新分派）。
*
*        数据分两层：
*        - done：已结束的测量段（右键结束一段后入库，画面保留）
*        - current：进行中的一段（末顶点到鼠标的橡皮筋虚线预览）
*
*        绘制：折线（黑底黄芯双 pass，深浅瓦片皆清晰）+ 顶点圆点 +
*        每段中点距离标签 + 末端总距离标签。距离用 Haversine 大圆距离。
*
* @class MeasureItem measureitem.h "measureitem.h"
*/
class MeasureItem : public QGraphicsItem, public MapAnchoredItem
{
public:
    enum { Type = UserType + 13 };   // 与既有 Type 分配错开（…/TrailLine=10/MapMarker=11/MarkerTrail=12）

    explicit MeasureItem(MapGraphicItem *map);

    void AddPoint(opmap::PointLatLng const& coord);         ///< 固化一个顶点（首点自动开新段）
    void SetPreviewPoint(opmap::PointLatLng const& coord);  ///< 橡皮筋预览点（鼠标位置，段中才显示）
    double CommitMeasure();                                 ///< 结束当前段（<2 点丢弃；返回总距离米，无段返回 0）
    void ClearAll();                                        ///< 清除全部测量（含进行中）
    bool HasContent() const { return !done.isEmpty() || !currentPts.isEmpty(); }

    virtual void RefreshPos();                              ///< coord → 屏幕位置重算（拖动/缩放时由分派链驱动）

    int type() const;
    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

private:
    void RebuildGeometry();                                 ///< 内容变化后统一：prepareGeometryChange + 重算屏幕点 + update

    MapGraphicItem *map;
    QList< QList<opmap::PointLatLng> > done;   ///< 已结束测量段的顶点序列
    QList<opmap::PointLatLng> currentPts;      ///< 进行中测量段的顶点序列
    bool currentActive;                        ///< 是否有进行中的段（Commit 后 false，AddPoint 重新激活）
    opmap::PointLatLng previewPos;             ///< 橡皮筋跟随点（鼠标）
    bool hasPreview;
    QFont labelFont;                           ///< 距离标签字体

    // RefreshPos 缓存的屏幕几何（item 本地坐标 = 地图平面坐标）
    QVector<QPolygonF> doneScreen;             ///< done 各段的屏幕折线
    QPolygonF currentScreen;                   ///< 进行中段的屏幕折线
    QPointF previewScreen;                     ///< 预览点屏幕位置
};

} // end of namespace opmap

#endif // MEASUREITEM_H
