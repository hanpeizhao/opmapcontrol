/**
******************************************************************************
*
* @file       mapmarkeritem.h
* @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
* @brief      A graphicsItem pinning an image and/or a text label to a map location
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
#ifndef MAPMARKERITEM_H
#define MAPMARKERITEM_H

#include <QGraphicsItem>
#include <QPainter>
#include <QFont>
#include "pointlatlng.h"
#include "mapgraphicitem.h"
#include "mapanchoreditem.h"

namespace opmap {

class MarkerTrailItem;   ///< 移动轨迹线（定义见文件尾，MapMarkerItem 持其指针）

/**
* @brief 通用地图标记：把任意图片和/或文字标签钉在指定经纬度上。
*
*        组合形态（可叠加，至少配一项内容）：
*        - 仅图片：图片底边中点对准坐标点（图钉语义）
*        - 仅文字：文字标签显示在坐标点正下方（地名标签样式，白字黑描边）
*        - 图片+文字：图片在上，文字标签在其下方居中
*        - 两者都未设置：回退绘制红点占位，保证标记可见
*
*        纯装饰图元——不可拖选、不进任务序列、不参与导航/围栏逻辑。
*        图片支持文件路径与 qrc 资源路径；缩放地图时标记保持屏幕尺寸
*        不随瓦片缩放（ItemIgnoresTransformations）。
*
* @class MapMarkerItem mapmarkeritem.h "mapmarkeritem.h"
*/
class MapMarkerItem : public QGraphicsItem, public MapAnchoredItem
{
public:
    enum { Type = UserType + 11 };   // 与既有 Type 分配错开（WP=1/UAV=2/Trail=3/Home=4/GPS=6/WPLine=7/Route=8/Geofence=9/TrailLine=10）

    /**
    * @brief Constructer
    *
    * @param coord 标记锚点的经纬度（WGS-84）
    * @param map 地图画布指针
    */
    MapMarkerItem(opmap::PointLatLng const& coord, MapGraphicItem* map);

    opmap::PointLatLng Coord() const { return coord; }
    void SetCoord(opmap::PointLatLng const& value);   ///< 移动标记到新坐标（地理锚定）

    void SetImage(QString const& imagePath);          ///< 设置图片（文件或 qrc 路径），空串=清除图片
    void SetImageSize(int width, int height);         ///< 图片显示尺寸（像素）；任一维度为 0 按另一维等比缩放，均 ≤0 恢复原始尺寸
    void SetText(QString const& text);                ///< 设置文字标签，空串=不显示
    void SetFontSize(int pointSize);                  ///< 文字字号（磅），默认 10 加粗
    void SetShowTrail(bool on);                       ///< 开启移动轨迹显示：每次 SetCoord 记录足迹连为折线（关闭即清除）

    virtual void RefreshPos();                        ///< coord → 屏幕位置重算（拖动/缩放地图时由 ChildPosRefresh 驱动）

    ~MapMarkerItem();                                 ///< 析构并移除附属轨迹线

    int type() const;
    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

private:
    void updateDisplay();                             ///< picture + imgW/imgH → display 缩放结果

    MapGraphicItem* map;
    opmap::PointLatLng coord;   ///< 锚点经纬度
    MarkerTrailItem* trailItem; ///< 附属移动轨迹线（SetShowTrail 创建；独立于本图元——ItemIgnoresTransformations 会让轨迹在地图旋转时方向失真）
    QPixmap picture;            ///< 原始加载图片（未设置/加载失败为空）
    QPixmap display;            ///< 按 SetImageSize 缩放后的绘制用图
    int imgW;                   ///< 请求的显示宽（0=不限）
    int imgH;                   ///< 请求的显示高（0=不限）
    QString text;               ///< 文字标签（空=不显示）
    QFont font;                 ///< 文字字体
};

/**
* @brief 标记移动轨迹线：MapMarkerItem::SetShowTrail 的附属图元。
*
*        独立于 MapMarkerItem（其 ItemIgnoresTransformations 会让轨迹在
*        地图旋转时方向失真），直接挂在地图画布下按地理位置重算折线，
*        随地图拖动/缩放/旋转自动跟随（实现 MapAnchoredItem 纳入统一刷新分派）。
*        不可交互（无鼠标按键），点击穿透到地图拖动。
*
* @class MarkerTrailItem mapmarkeritem.h "mapmarkeritem.h"
*/
class MarkerTrailItem : public QGraphicsItem, public MapAnchoredItem
{
public:
    enum { Type = UserType + 12 };   // 与既有 Type 分配错开（…/TrailLine=10/MapMarker=11）

    explicit MarkerTrailItem(MapGraphicItem* map);
    ~MarkerTrailItem();

    void AppendPoint(opmap::PointLatLng const& coord);   ///< 追加轨迹点（超过上限丢最老）
    void ClearTrail();                                   ///< 清空已记录轨迹
    virtual void RefreshPos();                           ///< 地图拖动/缩放后重算屏幕折线

    int type() const;
    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

private:
    MapGraphicItem* map;
    QList<opmap::PointLatLng> coords;   ///< 轨迹地理点序列（时序）
    QPolygonF screenPts;                ///< RefreshPos 换算的屏幕折线缓存
};

} // end of namespace opmap

#endif // MAPMARKERITEM_H
