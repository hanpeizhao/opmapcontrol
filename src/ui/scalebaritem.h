/**
******************************************************************************
*
* @file       scalebaritem.h
* @brief      地图比例尺叠加图元：场景级图钉（不随地图旋转），实时按当前
*             缩放级别与视野中心纬度换算 1/2/5×10ⁿ 的整距离标尺
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
#ifndef SCALEBARITEM_H
#define SCALEBARITEM_H

#include <QObject>
#include <QGraphicsItem>

namespace opmap {

class MapGraphicItem;

/**
* @brief 比例尺图元：挂在场景上（非地图子项），地图旋转时不跟随旋转。
*
*        每次绘制时按地面分辨率（米/像素，取决于缩放级别与视野中心纬度）
*        现算标尺长度，取约 110 像素宽的最整距离（1/2/5×10ⁿ 系列），
*        缩放/拖动后由 OPMapWidget 转发 mapChanged/zoomChanged 触发重绘。
*        位置固定在视野左下角（Reposition），窗口缩放时由 facade 重排。
*
* @class ScaleBarItem scalebaritem.h "scalebaritem.h"
*/
class ScaleBarItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    explicit ScaleBarItem(MapGraphicItem *map, QGraphicsItem *parent = 0);

    /// 依据场景尺寸移到左下角（窗口 resize 时由 facade 调用）
    void Reposition();

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

public slots:
    /// 地图缩放/拖动/失效后重算标尺（仅置脏，实际换算在下次 paint 时进行）
    void RefreshScale() { update(); }

private:
    MapGraphicItem *map;   ///< 地图画布（取缩放级别/中心纬度/投影换算）
};

} // end of namespace opmap

#endif // SCALEBARITEM_H
