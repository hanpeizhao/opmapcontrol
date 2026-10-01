/**
******************************************************************************
*
* @file       imagemarkeritem.h
* @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
* @brief      A graphicsItem pinning an arbitrary picture to a map location
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
#ifndef IMAGEMARKERITEM_H
#define IMAGEMARKERITEM_H

#include <QGraphicsItem>
#include <QPainter>
#include "pointlatlng.h"
#include "mapgraphicitem.h"

namespace opmap {

/**
* @brief 地图图片标记：把任意图片钉在指定经纬度上（底部中心为锚点）。
*
*        纯装饰图元——不可拖选、不进任务序列、不参与导航/围栏逻辑。
*        图片支持文件路径与 qrc 资源路径（QPixmap::load 均可）；
*        缩放地图时标记保持屏幕尺寸不随瓦片缩放（ItemIgnoresTransformations）。
*        加载失败时回退绘制一个红点占位，保证标记可见。
*
* @class ImageMarkerItem imagemarkeritem.h "imagemarkeritem.h"
*/
class ImageMarkerItem : public QGraphicsItem
{
public:
    enum { Type = UserType + 11 };   // 与既有 Type 分配错开（WP=1/UAV=2/Trail=3/Home=4/GPS=6/WPLine=7/Route=8/Geofence=9/TrailLine=10）

    /**
    * @brief Constructer
    *
    * @param coord 标记锚点的经纬度（WGS-84，图片底部中心对准该点）
    * @param imagePath 图片路径（文件或 qrc 资源）
    * @param map 地图画布指针
    */
    ImageMarkerItem(opmap::PointLatLng const& coord, QString const& imagePath, MapGraphicItem* map);

    opmap::PointLatLng Coord() const { return coord; }
    void SetCoord(opmap::PointLatLng const& value);   ///< 移动标记到新坐标（地理锚定）
    void RefreshPos();                                ///< coord → 屏幕位置重算（拖动/缩放地图时由 ChildPosRefresh 驱动）

    int type() const;
    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

private:
    MapGraphicItem* map;
    opmap::PointLatLng coord;   ///< 锚点经纬度
    QPixmap picture;            ///< 标记图片（加载失败为空，paint 回退红点）
};

} // end of namespace opmap

#endif // IMAGEMARKERITEM_H
