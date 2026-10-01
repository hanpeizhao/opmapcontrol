/**
******************************************************************************
*
* @file       imagemarkeritem.cpp
* @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
* @brief      A graphicsItem pinning an arbitrary picture to a map location
* @see        The GNU Public License (GPL) Version 3
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
#include "imagemarkeritem.h"

namespace opmap {

ImageMarkerItem::ImageMarkerItem(const opmap::PointLatLng &coord, const QString &imagePath, MapGraphicItem *map) :
    map(map),
    coord(coord)
{
    picture.load(imagePath);
    setFlag(QGraphicsItem::ItemIgnoresTransformations, true);   // 屏幕尺寸恒定，不随瓦片缩放
    RefreshPos();
}

void ImageMarkerItem::SetCoord(opmap::PointLatLng const& value)
{
    coord = value;
    RefreshPos();
    update();
}

void ImageMarkerItem::RefreshPos()
{
    opmap::Point point = map->FromLatLngToLocal(coord);
    setPos(point.X(), point.Y());
}

int ImageMarkerItem::type() const
{
    return Type;
}

QRectF ImageMarkerItem::boundingRect() const
{
    if (picture.isNull())
        return QRectF(-5, -5, 10, 10);   // 回退红点
    // 底部中心锚定：图钉语义，图片底边中点对准坐标点
    return QRectF(-picture.width() / 2, -picture.height(), picture.width(), picture.height());
}

void ImageMarkerItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    if (picture.isNull()) {
        // 图片加载失败：红点占位，保证标记在图上可见可发现
        painter->setBrush(Qt::red);
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(QPointF(0, 0), 5, 5);
        return;
    }
    painter->drawPixmap(-picture.width() / 2, -picture.height(), picture);
}

} // end of namespace opmap
