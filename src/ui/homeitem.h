/**
******************************************************************************
*
* @file       homeitem.h
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

#ifndef HOMEITEM_H
#define HOMEITEM_H

#include <QGraphicsItem>
#include <QPainter>
#include <QLabel>
#include "pointlatlng.h"
#include <QObject>
#include "opmapwidget.h"

namespace opmap
{

class HomeItem:public QObject,public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    enum { Type = UserType + 4 };
    HomeItem(MapGraphicItem* map,OPMapWidget* parent);
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget);
    QRectF boundingRect() const;
    int type() const;
    void RefreshPos();
    bool ShowSafeArea()const{return showsafearea;}
    int SafeArea()const{return safearea;}
    bool safe;
    /// 设置返航点坐标（立即重算屏幕位置，无需等地图拖动）
    void SetCoord(opmap::PointLatLng const& value);
    opmap::PointLatLng Coord()const{return coord;}
    void SetAltitude(int const& value){altitude=value;}
    int Altitude()const{return altitude;}
    /// 设置安全围栏半径（米，立即重算圈大小）
    void SetSafeArea(int const& value);
    void SetShowSafeArea(bool const& value);

private:
    MapGraphicItem* map;
    OPMapWidget* mapwidget;
    QPixmap pic;
    opmap::Point localposition;
    opmap::PointLatLng coord;
    bool showsafearea;
    int safearea;
    int localsafearea;
    int altitude;

public slots:

signals:

};

} // end of namespace opmap

#endif // HOMEITEM_H
