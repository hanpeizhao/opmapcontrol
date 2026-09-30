/**
******************************************************************************
*
* @file       core.cpp
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

#include "mapengine.h"
#include "coordtransform.h"
#include <QThread>

#ifdef DEBUG_CORE
qlonglong opmap::MapEngine::debugcounter=0;
#endif



namespace opmap {

MapEngine::MapEngine(opmap::MapService *mapService) :
    MouseWheelZooming(false),
    currentPosition(0,0),
    currentPositionPixel(0,0),
    LastLocationInBounds(-1,-1),
    sizeOfMapArea(0,0),
    minOfTiles(0,0),
    maxOfTiles(0,0),
    zoom(0),
    isDragging(false),
    TooltipTextPadding(10,10),
    loaderLimit(5),
    maxzoom(21),
    minzoom(0),
    runningThreads(0),
    service(mapService),
    started(false)
{
    mousewheelzoomtype=MouseWheelZoomType::MousePositionAndCenter;
    SetProjection(new MercatorProjection());
    this->setAutoDelete(false);
    ProcessLoadTaskCallback.setMaxThreadCount(10);
    renderOffset=Point(0,0);
    dragPoint=Point(0,0);
    CanDragMap=true;
    tilesToload=0;
}

MapEngine::~MapEngine()
{
    ProcessLoadTaskCallback.waitForDone();
}

void MapEngine::run()
{
    MrunningThreads.lock();
    ++runningThreads;
    MrunningThreads.unlock();

#ifdef DEBUG_CORE
    qlonglong debug;
    Mdebug.lock();
    debug=++debugcounter;
    Mdebug.unlock();
    qDebug()<<"core:run"<<" ID="<<debug;
#endif //DEBUG_CORE

    bool last = false;

    LoadTask task;

    MtileLoadQueue.lock();
    {
        if(tileLoadQueue.count() > 0)
        {
            task = tileLoadQueue.dequeue();
            {

                last = (tileLoadQueue.count() == 0);
#ifdef DEBUG_CORE
                qDebug()<<"TileLoadQueue: " << tileLoadQueue.count()<<" Point:"<<task.Pos.ToString()<<" ID="<<debug;;
#endif //DEBUG_CORE
            }
        }
    }
    MtileLoadQueue.unlock();

    if(task.HasValue())
        if(loaderLimit.tryAcquire(1,service->Timeout()))
        {
            MtileToload.lock();
            --tilesToload;
            MtileToload.unlock();
#ifdef DEBUG_CORE
            qDebug()<<"loadLimit semaphore aquired "<<loaderLimit.available()<<" ID="<<debug<<" TASK="<<task.Pos.ToString()<<" "<<task.Zoom;
#endif //DEBUG_CORE

            {

#ifdef DEBUG_CORE
                qDebug()<<"task as value, begining get"<<" ID="<<debug;;
#endif //DEBUG_CORE
                {
                    Tile* m = Matrix.TileAt(task.Pos);

                    if(m==0 || m->Overlays.count() == 0)
                    {
#ifdef DEBUG_CORE
                        qDebug()<<"Fill empty TileMatrix: " + task.ToString()<<" ID="<<debug;;
#endif //DEBUG_CORE

                        Tile* t = new Tile(task.Zoom, task.Pos);
                        QVector<MapType::Types> layers= service->GetAllLayersOfType(GetMapType());

                        foreach(MapType::Types tl,layers)
                        {
                            int retry = 0;
                            do
                            {
                                QByteArray img;

#ifdef DEBUG_CORE
                                qDebug()<<"start getting image"<<" ID="<<debug;
#endif //DEBUG_CORE
                                img = service->GetImageFrom(tl, task.Pos, task.Zoom);
#ifdef DEBUG_CORE
                                qDebug()<<"MapEngine::run:gotimage size:"<<img.count()<<" ID="<<debug<<" time="<<t.elapsed();
#endif //DEBUG_CORE

                                if(img.length()!=0)
                                {
                                    Moverlays.lock();
                                    {
                                        t->Overlays.append(img);
#ifdef DEBUG_CORE
                                        qDebug()<<"MapEngine::run append img:"<<img.length()<<" to tile:"<<t->GetPos().ToString()<<" now has "<<t->Overlays.count()<<" overlays"<<" ID="<<debug;
#endif //DEBUG_CORE

                                    }
                                    Moverlays.unlock();

                                    break;
                                }
                                else if(service->RetryLoadTile > 0)
                                {
#ifdef DEBUG_CORE
                                    qDebug()<<"ProcessLoadTask: " << task.ToString()<< " -> empty tile, retry " << retry<<" ID="<<debug;;
#endif //DEBUG_CORE
                                    // 原实现为局部 QMutex 加锁后 QWaitCondition 定时等待，
                                    // 作用域结束时带锁销毁，每次重试都刷
                                    // "QMutex: destroying locked mutex"，改为普通休眠
                                    QThread::msleep(500);
                                }
                            }
                            while(++retry < service->RetryLoadTile);
                        }

                        if(t->Overlays.count() > 0)
                        {
                            Matrix.SetTileAt(task.Pos,t);
                            emit OnNeedInvalidation();

#ifdef DEBUG_CORE
                            qDebug()<<"MapEngine::run add tile "<<t->GetPos().ToString()<<" to matrix index "<<task.Pos.ToString()<<" ID="<<debug;
                            qDebug()<<"MapEngine::run matrix index "<<task.Pos.ToString()<<" as tile with "<<Matrix.TileAt(task.Pos)->Overlays.count()<<" ID="<<debug;
#endif //DEBUG_CORE
                        }
                        else
                        {
                            // emit OnTilesStillToLoad(tilesToload);

                            delete t;
                            t = 0;
                            emit OnNeedInvalidation();
                        }

                        // layers = null;
                    }
                }


                {
                    // last buddy cleans stuff ;}
                    if(last)
                    {
                        service->kiberCacheLock.lockForWrite();
                        service->TilesInMemory.RemoveMemoryOverload();
                        service->kiberCacheLock.unlock();

                        MtileDrawingList.lock();
                        {
                            Matrix.ClearPointsNotIn(tileDrawingList);
                        }
                        MtileDrawingList.unlock();

                        emit OnTileLoadComplete();

                        emit OnNeedInvalidation();
                    }
                }
            }
#ifdef DEBUG_CORE
            qDebug()<<"loaderLimit release:"+loaderLimit.available()<<" ID="<<debug;
#endif
            emit OnTilesStillToLoad(tilesToload<0? 0:tilesToload);
            loaderLimit.release();
        }

    MrunningThreads.lock();
    --runningThreads;
    MrunningThreads.unlock();
}

diagnostics MapEngine::GetDiagnostics()
{
    MrunningThreads.lock();
    diag=service->GetDiagnostics();
    diag.runningThreads=runningThreads;
    MrunningThreads.unlock();
    return diag;
}

void MapEngine::SetZoom(const int &value)
{
    if (!isDragging)
    {
        zoom=value;
        minOfTiles=Projection()->GetTileMatrixMinXY(value);
        maxOfTiles=Projection()->GetTileMatrixMaxXY(value);
        currentPositionPixel=Projection()->FromLatLngToPixel(ToTileDatum(currentPosition),value);
        if(started)
        {
            MtileLoadQueue.lock();
            tileLoadQueue.clear();
            MtileLoadQueue.unlock();
            MtileToload.lock();
            tilesToload=0;
            MtileToload.unlock();
            Matrix.Clear();
            GoToCurrentPositionOnZoom();
            UpdateBounds();
            emit OnMapDrag();
            emit OnMapZoomChanged();
            emit OnNeedInvalidation();
        }
    }
}

void MapEngine::SetCurrentPosition(const PointLatLng &value)
{
    if(!IsDragging())
    {
        currentPosition = value;
        SetCurrentPositionGPixel(Projection()->FromLatLngToPixel(ToTileDatum(value), Zoom()));

        if(started)
        {
            GoToCurrentPosition();
            emit OnCurrentPositionChanged(currentPosition);
        }
    }
    else
    {
        currentPosition = value;
        SetCurrentPositionGPixel(Projection()->FromLatLngToPixel(ToTileDatum(value), Zoom()));

        if(started)
        {
            emit OnCurrentPositionChanged(currentPosition);
        }
    }
}

void MapEngine::SetMapType(const MapType::Types &value)
{

    if(value != GetMapType())
    {
        mapType = value;

        switch(value)
        {

        case MapType::ArcGIS_Map:
        case MapType::ArcGIS_Satellite:
        {
            if(Projection()->Type()!="PlateCarreeProjection")
            {
                SetProjection(new PlateCarreeProjection());
            }
            maxzoom=13;
            minzoom=0;
        }
            break;

        case MapType::AutoNaviRoad:
        case MapType::AutoNaviSatellite:
        case MapType::AutoNaviLabels:
        {
            if(Projection()->Type()!="MercatorProjection")
            {
                SetProjection(new MercatorProjection());
            }
            // 高德瓦片服务器最高支持 z=18，再往上返回空白占位图
            maxzoom=18;
            // 高德在 z<=2 时同样只返回空白占位图（实测 z2 瓦片仅 179 字节）
            minzoom=3;
        }
            break;

        case MapType::OpenStreetMap:
        case MapType::ArcGIS_WorldTopo:
        {
            if(Projection()->Type()!="MercatorProjection")
            {
                SetProjection(new MercatorProjection());
            }
            maxzoom=19;
            minzoom=0;
        }
            break;

        case MapType::GoogleMap:
        case MapType::GoogleSatellite:
        case MapType::GoogleLabels:
        case MapType::GoogleTerrain:
        {
            if(Projection()->Type()!="MercatorProjection")
            {
                SetProjection(new MercatorProjection());
            }
            maxzoom=20;
            minzoom=0;
        }
            break;

        default:
        {
            if(Projection()->Type()!="MercatorProjection")
            {
                SetProjection(new MercatorProjection());
            }
            maxzoom=21;
            minzoom=0;
        }
            break;
        }

        minOfTiles = Projection()->GetTileMatrixMinXY(Zoom());
        maxOfTiles = Projection()->GetTileMatrixMaxXY(Zoom());
        SetCurrentPositionGPixel(Projection()->FromLatLngToPixel(ToTileDatum(CurrentPosition()), Zoom()));

        if(started)
        {
            // 新地图源的最小/最大缩放可能比当前缩放更紧，先钳制再刷新瓦片
            if(Zoom() < minzoom)
                SetZoom(minzoom);
            else if(Zoom() > maxzoom)
                SetZoom(maxzoom);

            CancelAsyncTasks();
            OnMapSizeChanged(Width, Height);
            GoToCurrentPosition();
            ReloadMap();
            GoToCurrentPosition();
            emit OnMapTypeChanged(value);

        }
    }
}

void MapEngine::StartSystem()
{
    if(!started)
    {
        started = true;

        ReloadMap();
        GoToCurrentPosition();
    }
}

namespace {
/// floor 除法：C++ 整除向零截断，负的地图平面坐标换算瓦片号需向下取整
int FloorDiv(int a, int b)
{
    int q = a / b;
    if(a % b != 0 && ((a < 0) != (b < 0)))
        --q;
    return q;
}
}

void MapEngine::UpdateCenterTileXYLocation()
{
    // 中心点直接由 renderOffset 换算成地图平面坐标（screen = map + renderOffset），
    // 不再走"经纬度 -> 像素"的钳制往返：旧算法在拖出 ±180° 后中心瓦片被钉死在边界，
    // 而 renderOffset 随鼠标继续漂移，视口滑出覆盖窗口后只剩白屏。平面坐标算法
    // 配合 WrapTileX 使中心瓦片持续回绕，实现无限水平循环
    Size ts = Projection()->TileSize();
    Point centerMapPx(Width/2 - renderOffset.X(), Height/2 - renderOffset.Y());
    Point raw(FloorDiv(centerMapPx.X(), ts.Width()), FloorDiv(centerMapPx.Y(), ts.Height()));

    // 中心瓦片折回 [0, maxOfTiles.Width]，renderOffset 同步平移整世界宽度：
    // 瓦片在屏幕上的位置不变（视觉无跳变），覆盖窗口始终跟随视口
    int period = Projection()->GetTileMatrixMaxXY(Zoom()).Width() + 1;
    int wraps = FloorDiv(raw.X(), period);
    if(wraps != 0)
    {
        raw.SetX(raw.X() - wraps * period);
        renderOffset.SetX(renderOffset.X() + wraps * period * ts.Width());
    }

    // 垂直钳制：纬度方向两极有界、不循环（极地之外本无数据），极线不得拖入屏幕——
    // 拖到北极/南极后视口停住，杜绝上下大空白（此前 renderOffset.Y 会无限漂移）
    int mapSizeY = (Projection()->GetTileMatrixMaxXY(Zoom()).Height() + 1) * ts.Height();
    int lowerY = Height - mapSizeY;   // 南极线贴屏幕底
    int upperY = 0;                   // 北极线贴屏幕顶
    if(lowerY <= upperY)
        renderOffset.SetY(qBound(lowerY, renderOffset.Y(), upperY));
    else
        renderOffset.SetY((lowerY + upperY) / 2);  // 世界矮于屏幕（极低缩放）：垂直居中
    raw.SetY(FloorDiv(Height/2 - renderOffset.Y(), ts.Height()));

    centerTileXYLocation = raw;
}

void MapEngine::OnMapSizeChanged(int const& width, int const& height)
{
    Width = width;
    Height = height;

    sizeOfMapArea.SetWidth(1 + (Width/Projection()->TileSize().Width())/2);
    sizeOfMapArea.SetHeight(1 + (Height/Projection()->TileSize().Height())/2);

    UpdateCenterTileXYLocation();

    if(started)
    {
        UpdateBounds();

        emit OnCurrentPositionChanged(currentPosition);
    }
}

void MapEngine::OnMapClose()
{
    //        if(waitOnEmptyTasks != null)
    //        {
    //           try
    //           {
    //              waitOnEmptyTasks.Set();
    //              waitOnEmptyTasks.Close();
    //           }
    //           catch
    //           {
    //           }
    //        }

    CancelAsyncTasks();
}

RectLatLng MapEngine::CurrentViewArea()
{
    // 视野边界用用户坐标(WGS-84)表示
    PointLatLng p = FromTileDatum(Projection()->FromPixelToLatLng(-renderOffset.X(), -renderOffset.Y(), Zoom()));
    double rlng = FromTileDatum(Projection()->FromPixelToLatLng(-renderOffset.X() + Width, -renderOffset.Y(), Zoom())).Lng();
    double blat = FromTileDatum(Projection()->FromPixelToLatLng(-renderOffset.X(), -renderOffset.Y() + Height, Zoom())).Lat();
    return RectLatLng::FromLTRB(p.Lng(), p.Lat(), rlng, blat);

}

PointLatLng MapEngine::ToTileDatum(PointLatLng const& pt) const
{
    if(MapType::DatumByType(mapType) == MapType::DatumGCJ02)
        return coordtransform::WGS84ToGCJ02(pt);
    return pt;
}

PointLatLng MapEngine::FromTileDatum(PointLatLng const& pt) const
{
    if(MapType::DatumByType(mapType) == MapType::DatumGCJ02)
        return coordtransform::GCJ02ToWGS84(pt);
    return pt;
}

PointLatLng MapEngine::FromLocalToLatLng(int const& x, int const& y)
{
    // 屏幕像素 -> 瓦片坐标系经纬度 -> 用户坐标(WGS-84)
    return FromTileDatum(Projection()->FromPixelToLatLng(Point(x - renderOffset.X(), y - renderOffset.Y()), Zoom()));
}


Point MapEngine::FromLatLngToLocal(PointLatLng const& latlng)
{
    // 用户坐标(WGS-84) -> 瓦片坐标系经纬度 -> 屏幕像素
    Point pLocal = Projection()->FromLatLngToPixel(ToTileDatum(latlng), Zoom());
    pLocal.Offset(renderOffset);
    return pLocal;
}

int MapEngine::GetMaxZoomToFitRect(RectLatLng const& rect)
{
    int zoom = 0;

    for(int i = 1; i <= MaxZoom(); i++)
    {
        Point p1 = Projection()->FromLatLngToPixel(ToTileDatum(rect.LocationTopLeft()), i);
        PointLatLng br = ToTileDatum(PointLatLng(rect.Bottom(), rect.Right()));
        Point p2 = Projection()->FromLatLngToPixel(br.Lat(), br.Lng(), i);

        if(((p2.X() - p1.X()) <= Width+10) && (p2.Y() - p1.Y()) <= Height+10)
        {
            zoom = i;
        }
        else
        {
            break;
        }
    }

    return zoom;
}

void MapEngine::BeginDrag(Point const& pt)
{
    dragPoint.SetX(pt.X() - renderOffset.X());
    dragPoint.SetY(pt.Y() - renderOffset.Y());
    isDragging = true;
}

void MapEngine::EndDrag()
{
    isDragging = false;
    emit OnNeedInvalidation();

}

void MapEngine::ReloadMap()
{
    if(started)
    {
#ifdef DEBUG_CORE
        qDebug()<<"------------------";
#endif //DEBUG_CORE

        MtileLoadQueue.lock();
        {
            tileLoadQueue.clear();
        }
        MtileLoadQueue.unlock();
        MtileToload.lock();
        tilesToload=0;
        MtileToload.unlock();
        Matrix.Clear();

        emit OnNeedInvalidation();

    }
}

void MapEngine::GoToCurrentPosition()
{
    // reset stuff
    renderOffset = Point::Empty;
    centerTileXYLocationLast = Point::Empty;
    dragPoint = Point::Empty;

    // goto location
    Drag(Point(-(GetcurrentPositionGPixel().X() - Width/2), -(GetcurrentPositionGPixel().Y() - Height/2)));
}

void MapEngine::GoToCurrentPositionOnZoom()
{
    // reset stuff
    renderOffset = Point::Empty;
    centerTileXYLocationLast = Point::Empty;
    dragPoint = Point::Empty;

    // goto location and centering
    if(MouseWheelZooming)
    {
        if(mousewheelzoomtype != MouseWheelZoomType::MousePositionWithoutCenter)
        {
            Point pt = Point(-(GetcurrentPositionGPixel().X() - Width/2), -(GetcurrentPositionGPixel().Y() - Height/2));
            renderOffset.SetX(pt.X() - dragPoint.X());
            renderOffset.SetY(pt.Y() - dragPoint.Y());
        }
        else // without centering
        {
            renderOffset.SetX(-GetcurrentPositionGPixel().X() - dragPoint.X());
            renderOffset.SetY(-GetcurrentPositionGPixel().Y() - dragPoint.Y());
            renderOffset.Offset(mouseLastZoom);
        }
    }
    else // use current map center
    {
        mouseLastZoom = Point::Empty;

        Point pt = Point(-(GetcurrentPositionGPixel().X() - Width/2), -(GetcurrentPositionGPixel().Y() - Height/2));
        renderOffset.SetX(pt.X() - dragPoint.X());
        renderOffset.SetY(pt.Y() - dragPoint.Y());
    }

    UpdateCenterTileXYLocation();
}

void MapEngine::DragOffset(Point const& offset)
{
    renderOffset.Offset(offset);

    UpdateCenterTileXYLocation();

    if(centerTileXYLocation != centerTileXYLocationLast)
    {
        centerTileXYLocationLast = centerTileXYLocation;
        UpdateBounds();
    }

    {
        LastLocationInBounds = CurrentPosition();
        SetCurrentPosition (FromLocalToLatLng((int) Width/2, (int) Height/2));
    }

    emit OnNeedInvalidation();
    emit OnMapDrag();
}

void MapEngine::Drag(Point const& pt)
{
    renderOffset.SetX(pt.X() - dragPoint.X());
    renderOffset.SetY(pt.Y() - dragPoint.Y());

    UpdateCenterTileXYLocation();

    if(centerTileXYLocation != centerTileXYLocationLast)
    {
        centerTileXYLocationLast = centerTileXYLocation;
        UpdateBounds();
    }

    if(IsDragging())
    {
        LastLocationInBounds = CurrentPosition();
        SetCurrentPosition(FromLocalToLatLng((int) Width/2, (int) Height/2));
    }

    emit OnNeedInvalidation();


    emit OnMapDrag();

}

void MapEngine::CancelAsyncTasks()
{
    if(started)
    {
        ProcessLoadTaskCallback.waitForDone();
        MtileLoadQueue.lock();
        {
            tileLoadQueue.clear();
            //tilesToload=0;
        }
        MtileLoadQueue.unlock();
        MtileToload.lock();
        tilesToload=0;
        MtileToload.unlock();
        //  ProcessLoadTaskCallback.waitForDone();
    }
}

void MapEngine::UpdateBounds()
{
    MtileDrawingList.lock();
    {
        FindTilesAround(tileDrawingList);

#ifdef DEBUG_CORE
        qDebug()<<"OnTileLoadStart: " << tileDrawingList.count() << " tiles to load at zoom " << Zoom() << ", time: " << QDateTime::currentDateTime().date();
#endif //DEBUG_CORE

        emit OnTileLoadStart();


        foreach(Point p, tileDrawingList)
        {
            LoadTask task = LoadTask(p, Zoom());
            {
                MtileLoadQueue.lock();
                {
                    if(!tileLoadQueue.contains(task))
                    {
                        MtileToload.lock();
                        ++tilesToload;
                        MtileToload.unlock();
                        tileLoadQueue.enqueue(task);
#ifdef DEBUG_CORE
                        qDebug()<<"MapEngine::UpdateBounds new Task"<<task.Pos.ToString();
#endif //DEBUG_CORE
                        ProcessLoadTaskCallback.start(this);
                    }
                }
                MtileLoadQueue.unlock();
            }

        }
    }
    MtileDrawingList.unlock();
    UpdateGroundResolution();
}

Point MapEngine::WrapTileX(const Point &p)
{
    // 水平回绕：经度是 360° 环绕的，x 越界时折回另一端（周期 = maxOfTiles.Width + 1，
    // Mercator 为 2^z，PlateCarree 为 2^(z+1)）；纬度方向两极有界，不回绕
    int period = Projection()->GetTileMatrixMaxXY(Zoom()).Width() + 1;
    Point ret = p;
    ret.SetX(((p.X() % period) + period) % period);
    return ret;
}

void MapEngine::FindTilesAround(QList<Point> &list)
{
    list.clear();;
    for(int i = -sizeOfMapArea.Width(); i <= sizeOfMapArea.Width(); i++)
    {
        for(int j = -sizeOfMapArea.Height(); j <= sizeOfMapArea.Height(); j++)
        {
            Point p = centerTileXYLocation;
            p.SetX(p.X() + i);
            p.SetY(p.Y() + j);

            // 恢复水平循环：x 越界折回另一端，跨界瓦片照常请求与绘制
            p = WrapTileX(p);

            if(p.X() >= minOfTiles.Width() && p.Y() >= minOfTiles.Height() && p.X() <= maxOfTiles.Width() && p.Y() <= maxOfTiles.Height())
            {
                if(!list.contains(p))
                {
                    list.append(p);
                }
            }
        }
    }
}

void MapEngine::UpdateGroundResolution()
{
    double rez = Projection()->GetGroundResolution(Zoom(), CurrentPosition().Lat());
    pxRes100m =   (int) (100.0 / rez); // 100 meters
    pxRes1000m =  (int) (1000.0 / rez); // 1km
    pxRes10km =   (int) (10000.0 / rez); // 10km
    pxRes100km =  (int) (100000.0 / rez); // 100km
    pxRes1000km = (int) (1000000.0 / rez); // 1000km
    pxRes5000km = (int) (5000000.0 / rez); // 5000km
}
}
