/**
******************************************************************************
*
* @file       traillineitem.h
* @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
* @brief      A graphicsItem representing a WayPoint
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
#ifndef TAILLINEITEM_H
#define TAILLINEITEM_H

#include <QGraphicsItem>
#include <QPainter>
#include <QLabel>
#include "pointlatlng.h"
#include <QObject>

namespace opmap
{
class MapGraphicItem;

    class TrailLineItem:public QObject,public QGraphicsLineItem
    {
        Q_OBJECT
        Q_INTERFACES(QGraphicsItem)
    public:
                enum { Type = UserType + 10 };   ///< 7 已被 WayPointLineItem 占用，同值会导致 cast 互串
        TrailLineItem(opmap::PointLatLng const& coord1,opmap::PointLatLng const& coord2, QBrush color, QGraphicsItem* parent);
        int type() const;
      //  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
       //             QWidget *widget);
        /// 地图拖动/缩放时由 ChildPosRefresh 驱动，重算屏幕线段
        void RefreshPos();
        opmap::PointLatLng coord1;
        opmap::PointLatLng coord2;
    private:
        QBrush m_brush;
        MapGraphicItem *map;


    public slots:

    signals:

    };
}

#endif // TAILLINEITEM_H
