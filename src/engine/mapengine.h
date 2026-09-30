/**
******************************************************************************
*
* @file       core.h
* @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
* @brief      地图引擎核心：瓦片矩阵调度、加载线程与几何变换      
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

#ifndef CORE_H
#define CORE_H

#include "debugheader.h"

#include "pointlatlng.h"
#include "mousewheelzoomtype.h"
#include "size.h"
#include "point.h"

#include "maptype.h"
#include "rectangle.h"
#include "QThreadPool"
#include "tilematrix.h"
#include <QQueue>
#include "loadtask.h"
#include "rectlatlng.h"
#include "mercatorprojection.h"
#include "platecarreeprojection.h"
#include "mapservice.h"
#include "diagnostics.h"

#include <QSemaphore>
#include <QThread>
#include <QDateTime>

#include <QObject>

namespace opmap {
class OPMapControl;
class MapGraphicItem;
}

namespace opmap {

class MapEngine:public QObject, public QRunnable
{
    Q_OBJECT

    friend class opmap::OPMapControl;
    friend class opmap::MapGraphicItem;

public:
    explicit MapEngine(opmap::MapService *mapService);
    ~MapEngine();

    /// 所属的地图数据服务（MapRipper 等经此访问缓存与下载）
    opmap::MapService *Service()const{return service;}

    void run();

    PointLatLng CurrentPosition()const{return currentPosition;}

    void SetCurrentPosition(const PointLatLng &value);

    opmap::Point GetcurrentPositionGPixel(){return currentPositionPixel;}
    void SetcurrentPositionGPixel(const opmap::Point &value){currentPositionPixel=value;}

    opmap::Point GetrenderOffset(){return renderOffset;}
    void SetrenderOffset(const opmap::Point &value){renderOffset=value;}

    opmap::Point GetcenterTileXYLocation(){return centerTileXYLocation;}
    void SetcenterTileXYLocation(const opmap::Point &value){centerTileXYLocation=value;}

    opmap::Point GetcenterTileXYLocationLast(){return centerTileXYLocationLast;}
    void SetcenterTileXYLocationLast(const opmap::Point &value){centerTileXYLocationLast=value;}

    opmap::Point GetdragPoint(){return dragPoint;}
    void SetdragPoint(const opmap::Point &value){dragPoint=value;}

    opmap::Point GetmouseDown(){return mouseDown;}
    void SetmouseDown(const opmap::Point &value){mouseDown=value;}

    opmap::Point GetmouseCurrent(){return mouseCurrent;}
    void SetmouseCurrent(const opmap::Point &value){mouseCurrent=value;}

    opmap::Point GetmouseLastZoom(){return mouseLastZoom;}
    void SetmouseLastZoom(const opmap::Point &value){mouseLastZoom=value;}

    MouseWheelZoomType::Types GetMouseWheelZoomType(){return mousewheelzoomtype;}
    void SetMouseWheelZoomType(const MouseWheelZoomType::Types &value){mousewheelzoomtype=value;}

    PointLatLng GetLastLocationInBounds(){return LastLocationInBounds;}
    void SetLastLocationInBounds(const PointLatLng &value){LastLocationInBounds=value;}

    Size GetsizeOfMapArea(){return sizeOfMapArea;}
    void SetsizeOfMapArea(const Size &value){sizeOfMapArea=value;}

    Size GetminOfTiles(){return minOfTiles;}
    void SetminOfTiles(const Size &value){minOfTiles=value;}

    Size GetmaxOfTiles(){return maxOfTiles;}
    void SetmaxOfTiles(const Size &value){maxOfTiles=value;}

    Rectangle GettileRect(){return tileRect;}
    void SettileRect(const Rectangle &value){tileRect=value;}

    opmap::Point GettilePoint(){return tilePoint;}
    void SettilePoint(const opmap::Point &value){tilePoint=value;}

    Rectangle GetCurrentRegion(){return CurrentRegion;}
    void SetCurrentRegion(const Rectangle &value){CurrentRegion=value;}

    QList<opmap::Point> tileDrawingList;

    PureProjection* Projection()
    {
        return projection;
    }

    void SetProjection(PureProjection* value)
    {
        projection=value;
        tileRect=Rectangle(opmap::Point(0,0),value->TileSize());
    }


    bool IsDragging()const{return isDragging;}

    int Zoom()const{return zoom;}
    void SetZoom(int const& value);

    int MaxZoom()const{return maxzoom;}
    void SetMaxZoom(int mz) {maxzoom = mz;}

    int MinZoom()const{return minzoom;}
    void SetMinZoom(int mz) {minzoom = mz;}

    void UpdateBounds();

    MapType::Types GetMapType(){return mapType;}
    void SetMapType(MapType::Types const& value);

    void StartSystem();

    void UpdateCenterTileXYLocation();

    void OnMapSizeChanged(int const& width, int const& height);//TODO had as slot

    void OnMapClose();//TODO had as slot

    RectLatLng CurrentViewArea();

    PointLatLng FromLocalToLatLng(int const& x, int const& y);

    Point FromLatLngToLocal(PointLatLng const& latlng);

    /// <summary>
    /// 用户坐标(WGS-84) -> 当前地图源的瓦片坐标系（如高德/谷歌中国为 GCJ-02）
    /// 在所有 latlng -> 像素/瓦片 的投影计算前调用
    /// </summary>
    PointLatLng ToTileDatum(PointLatLng const& pt) const;

    /// <summary>
    /// 当前地图源的瓦片坐标系 -> 用户坐标(WGS-84)
    /// 在所有 像素/瓦片 -> latlng 的反投影后调用
    /// </summary>
    PointLatLng FromTileDatum(PointLatLng const& pt) const;

    int GetMaxZoomToFitRect(RectLatLng const& rect);

    void BeginDrag(opmap::Point const& pt);

    void EndDrag();

    void ReloadMap();

    void GoToCurrentPosition();

    bool MouseWheelZooming;

    void DragOffset(opmap::Point const& offset);

    void Drag(opmap::Point const& pt);

    void CancelAsyncTasks();

    void FindTilesAround(QList<opmap::Point> &list);

    void UpdateGroundResolution();

    TileMatrix Matrix;

    bool isStarted(){return started;}

    diagnostics GetDiagnostics();

signals:
    void OnCurrentPositionChanged(opmap::PointLatLng point);
    void OnTileLoadComplete();
    void OnTilesStillToLoad(int number);
    void OnTileLoadStart();
    void OnMapDrag();
    void OnMapZoomChanged();
    void OnMapTypeChanged(MapType::Types type);
    void OnEmptyTileError(int zoom, opmap::Point pos);
    void OnNeedInvalidation();

private:
    PointLatLng currentPosition;
    opmap::Point currentPositionPixel;
    opmap::Point renderOffset;
    opmap::Point centerTileXYLocation;
    opmap::Point centerTileXYLocationLast;
    opmap::Point dragPoint;
    Rectangle tileRect;
    opmap::Point mouseDown;
    bool CanDragMap;
    opmap::Point mouseCurrent;
    PointLatLng LastLocationInBounds;
    opmap::Point mouseLastZoom;

    MouseWheelZoomType::Types mousewheelzoomtype;


    Size sizeOfMapArea;
    Size minOfTiles;
    Size maxOfTiles;

    opmap::Point tilePoint;

    Rectangle CurrentRegion;

    QQueue<LoadTask> tileLoadQueue;

    int zoom;

    PureProjection* projection;

    bool isDragging;

    QMutex MtileLoadQueue;

    QMutex Moverlays;

    QMutex MtileDrawingList;
#ifdef DEBUG_CORE
    QMutex Mdebug;
    static qlonglong debugcounter;
#endif
    Size TooltipTextPadding;

    MapType::Types mapType;

    QSemaphore loaderLimit;

    QThreadPool ProcessLoadTaskCallback;
    QMutex MtileToload;
    int tilesToload;

    int maxzoom;
    int minzoom;   ///< 地图源可用最低缩放（低于该级瓦片服务器返回空白占位图）
    QMutex MrunningThreads;
    int runningThreads;
    diagnostics diag;

    opmap::MapService *service;   ///< 地图数据服务（OPMapWidget 所有，注入）

protected:
    bool started;

    int Width;
    int Height;
    int pxRes100m;  // 100 meters
    int pxRes1000m;  // 1km
    int pxRes10km; // 10km
    int pxRes100km; // 100km
    int pxRes1000km; // 1000km
    int pxRes5000km; // 5000km
    void SetCurrentPositionGPixel(opmap::Point const& value){currentPositionPixel = value;}
    void GoToCurrentPositionOnZoom();

};

} // end of namespace opmap

#endif // CORE_H
