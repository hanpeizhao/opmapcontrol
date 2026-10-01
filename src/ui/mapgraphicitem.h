/**
******************************************************************************
*
* @file       mapgraphicitem.h
* @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
* @brief      The main graphicsItem used on the widget, contains the map and map logic
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

#ifndef MAPGRAPHICITEM_H
#define MAPGRAPHICITEM_H

#include <QGraphicsItem>
#include "mapengine.h"
//#include "point.h"
#include "diagnostics.h"
#include "configuration.h"
#include <QtGui>
#include <QTransform>
#include <QWidget>
#include <QBrush>
#include <QFont>
#include <QObject>
#include "waypointitem.h"
//#include "uavitem.h"

namespace opmap
{
class OPMapWidget;

/**
    * @brief The main graphicsItem used on the widget, contains the map and map logic
    *
    * @class MapGraphicItem mapgraphicitem.h "mapgraphicitem.h"
    */
class MapGraphicItem:public QObject,public QGraphicsItem
{
    friend class opmap::OPMapWidget;
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    /**
        * @brief Contructer
        *
        * @param core
        * @param configuration the configuration to be used
        * @return
        */
    MapGraphicItem(opmap::MapEngine *core,Configuration *configuration);

    QRectF boundingRect() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget);

    QSize sizeHint()const;

    /**
        * @brief Convertes LatLong coordinates to local item coordinates
        *
        * @param point LatLong point to be converted
        * @return opmap::Point Local item point
        */
    opmap::Point FromLatLngToLocal(opmap::PointLatLng const& point);

    /**
        * @brief Converts from local item coordinates to LatLong point
        *
        * @param x x local coordinate
        * @param y y local coordinate
        * @return opmap::PointLatLng LatLng coordinate
        */
    opmap::PointLatLng FromLocalToLatLng(int x, int y);

    /**
        * @brief Converts from meters at one location to pixels
        *
        * @param meters Distance to convert
        * @param coord Coordinate close to the distance measure
        * @return float Distance in pixels
        */
    float metersToPixels(double meters, opmap::PointLatLng coord);

    /**
        * @brief Returns true if map is being dragged
        *
        * @return
        */
    bool IsDragging()const{return core->IsDragging();}

    QImage lastimage;
    //        QPainter* imagePainter;
    opmap::Point lastimagepoint;

    void paintImage(QPainter* painter);
    void ConstructLastImage(int const& zoomdiff);
    opmap::PureProjection* Projection()const{return core->Projection();}
    double Zoom();
    double ZoomDigi();
    double ZoomTotal();

    /**
        * @brief The area currently selected by the user
        *
        * @return The rectangle in lat/lon coordinates currently selected
        */
    opmap::RectLatLng SelectedArea()const{return selectedArea;}

public slots:
    void SetSelectedArea(opmap::RectLatLng const& value){selectedArea = value;this->update();}

protected:
    void mouseMoveEvent ( QGraphicsSceneMouseEvent * event );
    void mousePressEvent ( QGraphicsSceneMouseEvent * event );
    void wheelEvent ( QGraphicsSceneWheelEvent * event );
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    bool IsMouseOverMarker()const{return isMouseOverMarker;}

    /**
        * @brief Returns current map zoom
        *
        * @return int Current map zoom
        */
    int ZoomStep()const;

    /**
        * @brief Sets map zoom
        *
        * @param value zoom value
        */
    void SetZoomStep(int const& value);

    /**
        * @brief Ask Stacey
        *
        * @param value
        */
    void SetShowDragons(bool const& value);

private:
    bool showDragons;
    bool SetZoomToFitRect(opmap::RectLatLng const& rect);
    opmap::MapEngine *core;
    Configuration *config;
    bool showTileGridLines;
    qreal MapRenderTransform;

    void DrawMap2D(QPainter *painter);

    opmap::RectLatLng selectedArea;
    opmap::PointLatLng selectionStart;
    opmap::PointLatLng selectionEnd;

    double zoomReal;
    qreal rotation;
    double zoomDigi;
    QRectF maprect;
    bool isSelected;
    bool isMouseOverMarker;
    QPixmap dragons;

    void SetIsMouseOverMarker(bool const& value){isMouseOverMarker = value;}


    /**
        * @brief Creates a rectangle that represents the "view" of the cuurent map, to compensate
        *       rotation
        *
        * @param rect original rectangle
        * @param angle angle of rotation
        * @return QRectF
        */
    QRectF boundingBox(QRectF const& rect, qreal const& angle);

    /**
        * @brief Returns the maximum allowed zoom
        *
        * @return int
        */
    int MaxZoom()const{return core->MaxZoom();}
    void SetMaxZoom(int mz) {core->SetMaxZoom(mz); }
    /**
        * @brief Returns the minimum allowed zoom
        *
        * @return int
        */
    int MinZoom()const{return core->MinZoom();}
    void SetMinZoom(int mz) {core->SetMinZoom(mz); }

    opmap::MouseWheelZoomType::Types GetMouseWheelZoomType(){return core->GetMouseWheelZoomType();}
    opmap::RectLatLng BoundsOfMap;

    void Offset(int const& x, int const& y);
    bool CanDragMap()const{return core->CanDragMap;}
    void SetCanDragMap(bool const& value){core->CanDragMap = value;}

    void SetZoom(double const& value);
    void mapRotate ( qreal angle );
    void start();
    void  ReloadMap(){core->ReloadMap();}
    MapType::Types GetMapType(){return core->GetMapType();}
    void SetMapType(MapType::Types const& value){core->SetMapType(value);}

public slots:
    /// 全场景子项屏幕位置重算（地理锚定项按各自 coord 重新落点）。
    /// 拖拽/缩放经 OnMapDrag/OnMapZoomChanged 自动触发；
    /// 编程式跳中心（SetCurrentPosition）不经过该信号链，需由调用方手动触发
    void ChildPosRefresh();

private slots:
    void Core_OnNeedInvalidation();

public slots:
    /**
        * @brief To be called when the scene size changes
        *
        * @param rect
        */
    void resize ( QRectF const &rect=QRectF() );

signals:
    /**
        * @brief Fired when the current zoom is changed
        *
        * @param zoom
        */
    void zoomChanged(double zoomtotal,double zoomreal,double zoomdigi);

    /**
        * @brief Fired when map changes in any visible way
        */
    void mapChanged();
};

}

#endif // MAPGRAPHICITEM_H
