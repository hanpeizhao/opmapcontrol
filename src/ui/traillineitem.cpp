/**
******************************************************************************
*
* @file       trailitem.cpp
* @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
* @brief      A graphicsItem representing a trail point
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
#include "traillineitem.h"
#include "mapgraphicitem.h"

namespace opmap {

TrailLineItem::TrailLineItem(opmap::PointLatLng const& coord1,
                             opmap::PointLatLng const& coord2,
                             QBrush color, QGraphicsItem* parent) :
    QGraphicsLineItem(parent), coord1(coord1), coord2(coord2)
{
    m_brush=color;
    QPen pen;
    pen.setBrush(m_brush);
    pen.setWidth(2);
    this->setPen(pen);
    map = static_cast<MapGraphicItem*>(parent);
    RefreshPos();
}

/// 历史 bug：原始 OpenPilot 实现只存经纬度从未 setLine（零长度线不可见），
/// 这里补上经纬度→屏幕坐标换算，拖动/缩放由 ChildPosRefresh 驱动重算
void TrailLineItem::RefreshPos()
{
    if (!map)
        return;
    const opmap::Point p1 = map->FromLatLngToLocal(coord1);
    const opmap::Point p2 = map->FromLatLngToLocal(coord2);
    setLine(QLineF(QPointF(p1.X(), p1.Y()), QPointF(p2.X(), p2.Y())));
}

/*
    void TrailLineItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
    {
      //  painter->drawRect(QRectF(-3,-3,6,6));
        painter->setBrush(m_brush);
        QPen pen;
        pen.setBrush(m_brush);
        pen.setWidth(2);
        painter->drawLine(this->line().x1(),this->line().y1(),this->line().x2(),this->line().y2());
    }
*/

int TrailLineItem::type()const
{
    return Type;
}

}
