/**
******************************************************************************
*
* @file       opmapwidget.h
* @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
* @brief      The Map Widget, this is the part exposed to the user
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

#ifndef OPMAPWIDGET_H
#define OPMAPWIDGET_H


#include <QObject>
#include <QtOpenGL/QGLWidget>

// Qt4 or Qt5
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include <QtGui/QGraphicsView>
#else
#include <QtWidgets/QGraphicsView>
#endif

#include "mapgraphicitem.h"
#include "maptype.h"
#include "languagetype.h"
#include "diagnostics.h"
#include "configuration.h"
#include "waypointitem.h"
#include "QtSvg/QGraphicsSvgItem"
#include "uavitem.h"
#include "gpsitem.h"
#include "homeitem.h"
#include "waypointlineitem.h"
#include "mapripper.h"
#include "uavtrailtype.h"
#include "route.h"

namespace opmap {

class UAVItem;
class GPSItem;
class HomeItem;
class AbstractRouteProvider;
class IpLocationProvider;
class NavigationEngine;
class RouteItem;

/**
    * @brief Collection of static functions to help dealing with various enums used
    *       Contains functions for enumToString conversio, StringToEnum, QStringList of enum values...
    *
    * @class Helper opmapwidget.h "opmapwidget.h"
    */
class Helper
{
public:
    /**
         * @brief Converts from String to Type
         *
         * @param value String to convert
         * @return
         */
    static MapType::Types MapTypeFromString(QString const& value){return MapType::TypeByStr(value);}

    /**
         * @brief Converts from Type to String
         */
    static QString StrFromMapType(MapType::Types const& value){return MapType::StrByType(value);}

    /**
         * @brief Returns QStringList with string representing all the enum values
         */
    static QStringList MapTypes(){return MapType::TypesList();}

    /**
        * @brief Converts from String to Type
        */
    static opmap::MouseWheelZoomType::Types MouseWheelZoomTypeFromString(QString const& value) {
        return opmap::MouseWheelZoomType::TypeByStr(value);
    }

    /**
        * @brief Converts from Type to String
        */
    static QString StrFromMouseWheelZoomType(opmap::MouseWheelZoomType::Types const& value) {
        return opmap::MouseWheelZoomType::StrByType(value);
    }

    /**
        * @brief Returns QStringList with string representing all the enum values
        */
    static QStringList MouseWheelZoomTypes() {
        return opmap::MouseWheelZoomType::TypesList();
    }

    /**
        * @brief Converts from String to Type
        */
    static opmap::LanguageType::Types LanguageTypeFromString(QString const& value) {
        return opmap::LanguageType::TypeByStr(value);
    }

    /**
        * @brief Converts from Type to String
        */
    static QString StrFromLanguageType(opmap::LanguageType::Types const& value) {
        return opmap::LanguageType::StrByType(value);
    }

    /**
        * @brief Returns QStringList with string representing all the enum values
        */
    static QStringList LanguageTypes() {
        return opmap::LanguageType::TypesList();
    }

    /**
        * @brief Converts from String to Type
        */
    static opmap::AccessMode::Types AccessModeFromString(QString const& value) {
        return opmap::AccessMode::TypeByStr(value);
    }

    /**
        * @brief Converts from Type to String
        */
    static QString StrFromAccessMode(opmap::AccessMode::Types const& value) {
        return opmap::AccessMode::StrByType(value);
    }

    /**
        * @brief Returns QStringList with string representing all the enum values
        */
    static QStringList AccessModeTypes() {
        return opmap::AccessMode::TypesList();
    }

    /**
        * @brief Converts from String to Type
        */
    static UAVMapFollowType::Types UAVMapFollowFromString(QString const& value) {
        return UAVMapFollowType::TypeByStr(value);
    }

    /**
        * @brief Converts from Type to String
        */
    static QString StrFromUAVMapFollow(UAVMapFollowType::Types const& value){return UAVMapFollowType::StrByType(value);}

    /**
        * @brief Returns QStringList with string representing all the enum values
        */
    static QStringList UAVMapFollowTypes(){return UAVMapFollowType::TypesList();}

    /**
         * @brief Converts from String to Type
         */
    static UAVTrailType::Types UAVTrailTypeFromString(QString const& value){return UAVTrailType::TypeByStr(value);}

    /**
         * @brief Converts from Type to String
         */
    static QString StrFromUAVTrailType(UAVTrailType::Types const& value){return UAVTrailType::StrByType(value);}

    /**
         * @brief Returns QStringList with string representing all the enum values
         */
    static QStringList UAVTrailTypes(){return UAVTrailType::TypesList();}
};


class OPMapWidget:public QGraphicsView
{
    Q_OBJECT

    // Q_PROPERTY(int MaxZoom READ MaxZoom WRITE SetMaxZoom)
    Q_PROPERTY(int MinZoom READ MinZoom WRITE SetMinZoom)
    Q_PROPERTY(bool ShowTileGridLines READ ShowTileGridLines WRITE SetShowTileGridLines)
    Q_PROPERTY(double Zoom READ ZoomTotal WRITE SetZoom)
    Q_PROPERTY(qreal Rotate READ Rotate WRITE SetRotate)
    Q_ENUMS(opmap::MouseWheelZoomType::Types)

public:
    QSize sizeHint() const;

    /**
     * @brief Constructor
     *
     * @param parent parent widget
     * @param config pointer to configuration classed to be used
     * @return
     */
    OPMapWidget(QWidget *parent=0,Configuration *config=new Configuration);
    virtual ~OPMapWidget();

    /**
     * @brief Returns true if map is showing gridlines
     *
     * @return bool
     */
    bool ShowTileGridLines() const {
        return map->showTileGridLines;
    }

    /**
     * @brief Defines if map is to show gridlines
     *
     * @param value
     * @return
     */
    void SetShowTileGridLines(bool const& value) {
        map->showTileGridLines=value;
        map->update();
    }

    /**
     * @brief Returns the maximum zoom for the map
     *
     */
    int MaxZoom() const {
        return map->MaxZoom();
    }
    void SetMaxZoom(int mz) {
        map->SetMaxZoom(mz);
    }

    //  void SetMaxZoom(int const& value){map->maxZoom = value;}

    /**
     * @brief Returns the minimum zoom for the map
     *
     */
    int MinZoom()const{return map->MinZoom();}
    /**
     * @brief Sets the minimum zoom for the map
     *
     * @param value
     */
    void SetMinZoom(int const& value){map->SetMinZoom(value);}

    opmap::MouseWheelZoomType::Types GetMouseWheelZoomType() {
        return  map->core->GetMouseWheelZoomType();
    }
    void SetMouseWheelZoomType(opmap::MouseWheelZoomType::Types const& value) {
        map->core->SetMouseWheelZoomType(value);
    }
    //  void SetMouseWheelZoomTypeByStr(const QString &value){map->core->SetMouseWheelZoomType(opmap::MouseWheelZoomType::TypeByStr(value));}
    //  QString GetMouseWheelZoomTypeStr(){return map->GetMouseWheelZoomTypeStr();}

    opmap::RectLatLng SelectedArea() const {
        return  map->selectedArea;
    }
    void SetSelectedArea(opmap::RectLatLng const& value) {
        map->selectedArea = value;
        this->update();
    }

    bool CanDragMap() const {
        return map->CanDragMap();
    }
    void SetCanDragMap(bool const& value) {
        map->SetCanDragMap(value);
    }

    opmap::PointLatLng CurrentPosition() const {
        return map->core->CurrentPosition();
    }
    void SetCurrentPosition(opmap::PointLatLng const& value) {
        map->core->SetCurrentPosition(value);
    }

    double ZoomReal()  {return map->Zoom();}
    double ZoomDigi()  {return map->ZoomDigi();}
    double ZoomTotal() {return map->ZoomTotal();}

    qreal Rotate(){return map->rotation;}
    void SetRotate(qreal const& value);

    void ReloadMap() {
        map->ReloadMap();
        map->resize();
    }

    bool UseOpenGL(){return useOpenGL;}
    void SetUseOpenGL(bool const& value);

    MapType::Types GetMapType() {
        return map->core->GetMapType();
    }
    void SetMapType(MapType::Types const& value) {
        map->lastimage=QImage();
        map->core->SetMapType(value);
    }

    bool isStarted() {
        return map->core->isStarted();
    }

    Configuration* configuration;

    opmap::PointLatLng currentMousePosition();

    void SetFollowMouse(bool const& value){followmouse=value;this->setMouseTracking(followmouse);}
    bool FollowMouse(){return followmouse;}

    opmap::PointLatLng GetFromLocalToLatLng(QPointF p) {return map->FromLocalToLatLng(p.x(),p.y());}

    /** @brief Convert lat/lon to local scene point (inverse of GetFromLocalToLatLng) */
    QPointF GetFromLatLngToLocal(opmap::PointLatLng const& latlng) {
        opmap::Point p=map->FromLatLngToLocal(latlng);
        return QPointF(p.X(),p.Y());
    }

    /** @brief Convert meters to pixels */
    float metersToPixels(double meters);

    /** @brief Return the bearing from one point to another .. in degrees */
    double bearing(opmap::PointLatLng from, opmap::PointLatLng to);

    /** @brief Return a destination lat/lon point given a source lat/lon point and the bearing and distance(KILOMETERS) from the source point */
    opmap::PointLatLng destPoint(opmap::PointLatLng source, double bear, double dist);

    /**
     * @brief Creates a new WayPoint on the center of the map
     *
     * @return WayPointItem a pointer to the WayPoint created
     */
    WayPointItem* WPCreate();

    /**
     * @brief Creates a new WayPoint
     *
     * @param item the WayPoint to create
     */
    void WPCreate(WayPointItem* item);

    /**
     * @brief Creates a new WayPoint
     *
     * @param id the system (MAV) id this waypoint belongs to
     * @param item the WayPoint to create
     */
    void WPCreate(int id, WayPointItem* item);

    /**
     * @brief Creates a new WayPoint
     *
     * @param coord the coordinates in LatLng of the WayPoint
     * @param altitude the Altitude of the WayPoint
     * @return WayPointItem a pointer to the WayPoint created
     */
    WayPointItem* WPCreate(opmap::PointLatLng const& coord, int const& altitude);

    /**
     * @brief Creates a new WayPoint
     *
     * @param coord the coordinates in LatLng of the WayPoint
     * @param altitude the Altitude of the WayPoint
     * @param description the description of the WayPoint
     * @return WayPointItem a pointer to the WayPoint created
     */
    WayPointItem* WPCreate(opmap::PointLatLng const& coord,int const& altitude, QString const& description);

    /**
     * @brief Inserts a new WayPoint on the specified position
     *
     * @param position index of the WayPoint
     * @return WayPointItem a pointer to the WayPoint created
     */
    WayPointItem* WPInsert(int const& position);

    /**
     * @brief Inserts a new WayPoint on the specified position
     *
     * @param item the WayPoint to Insert
     * @param position index of the WayPoint
     */
    void WPInsert(WayPointItem* item,int const& position);

    /**
     * @brief Inserts a new WayPoint on the specified position
     *
     * @param coord the coordinates in LatLng of the WayPoint
     * @param altitude the Altitude of the WayPoint
     * @param position index of the WayPoint
     * @return WayPointItem a pointer to the WayPoint Inserted
     */
    WayPointItem* WPInsert(opmap::PointLatLng const& coord,int const& altitude,int const& position);

    /**
     * @brief Inserts a new WayPoint on the specified position
     *
     * @param coord the coordinates in LatLng of the WayPoint
     * @param altitude the Altitude of the WayPoint
     * @param description the description of the WayPoint
     * @param position index of the WayPoint
     * @return WayPointItem a pointer to the WayPoint Inserted
     */
    WayPointItem* WPInsert(opmap::PointLatLng const& coord,int const& altitude, QString const& description,int const& position);

    /**
     * @brief Deletes the WayPoint
     *
     * @param item the WayPoint to delete
     */
    void WPDelete(WayPointItem* item);

    /**
     * @brief deletes all WayPoints
     *
     */
    void WPDeleteAll();

    /**
     * @brief Returns the currently selected WayPoints
     *
     * @return @return QList<WayPointItem *>
     */
    QList<WayPointItem*> WPSelected();

    /**
     * @brief Get all waypoints
     * @return
     */
    QMap<int, WayPointItem*> WPAll();

    /**
     * @brief Renumbers the WayPoint and all others as needed
     *
     * @param item the WayPoint to renumber
     * @param newnumber the WayPoint's new number
     */
    void WPRenumber(WayPointItem* item,int const& newnumber);

    void SetShowCompass(bool const& value);

    // FIXME XXX Move to protected namespace
    UAVItem* UAV;
    QMap<int, QGraphicsItemGroup*> waypointLines;
    GPSItem* GPS;
    HomeItem* Home;
    // END OF FIXME XXX

    UAVItem* AddUAV(int id);
    void AddUAV(int id, UAVItem* uav);
    /** @brief Deletes UAV and its waypoints from map */
    void DeleteUAV(int id);
    UAVItem* GetUAV(int id);
    const QList<UAVItem*> GetUAVS();
    QGraphicsItemGroup* waypointLine(int id);
    void SetShowUAV(bool const& value);
    bool ShowUAV()const{return showuav;}
    void SetUavPic(QString UAVPic);

    void SetShowHome(bool const& value);
    bool ShowHome()const{return showhome;}
    void SetShowDiagnostics(bool const& value);

    // ———————— 车载导航 ————————
    /**
     * @brief 替换路由规划 provider（接管所有权），默认内置 OsrmRouteProvider
     */
    void SetRouteProvider(opmap::AbstractRouteProvider *provider);

    /**
     * @brief 仅规划并显示 from→to 路线，不进入导航（选点预览用）
     */
    void PlanRoute(opmap::PointLatLng const& from, opmap::PointLatLng const& to);

    void SetShowRoute(bool const& value);
    bool ShowRoute() const;
    opmap::Route CurrentNavigationRoute() const;
    bool IsNavigating() const;

    /**
     * @brief 是否有车辆位置（曾通过 UpdateVehiclePosition 喂入）
     */
    bool HasVehiclePosition() const { return vehiclePosValid; }

    /**
     * @brief 最近一次喂入的车辆位置（WGS-84，非地图中心；
     *        未喂过时无意义，先用 HasVehiclePosition 判断）
     */
    opmap::PointLatLng VehiclePosition() const { return vehiclePos; }

    QMap<int, UAVItem*> UAVS;

private:
    opmap::MapService *service;   ///< 地图数据服务（构造创建、析构释放）
    opmap::MapEngine *core;
    QGraphicsScene mscene;
    bool useOpenGL;
    MapType y;
    opmap::AccessMode xx;
    opmap::PointLatLng currentmouseposition;
    bool followmouse;
    QGraphicsSvgItem *compass;
    bool showuav;
    bool showhome;
    QTimer *diagTimer;
    bool showDiag;
    QGraphicsTextItem *diagGraphItem;

    opmap::AbstractRouteProvider *routeProvider;   ///< 路由规划服务（接管外部传入者）
    opmap::NavigationEngine *navEngine;            ///< 导航状态机
    opmap::RouteItem *routeItem;                   ///< 路线绘制项（随 map 析构）
    opmap::IpLocationProvider *ipLocator;          ///< IP 定位服务（城市级兜底）
    opmap::PointLatLng vehiclePos;                 ///< 最近喂入的车位置
    bool vehiclePosValid;

private slots:
    void diagRefresh();
    void onNavProgress(double traveledM, double remainingM, int remainingS, const QString &instruction);
    //   WayPointItem* item;//apagar

protected:
    MapGraphicItem *map;
    void resizeEvent(QResizeEvent *event);
    void showEvent ( QShowEvent * event );
    void closeEvent(QCloseEvent *event);

    void mouseMoveEvent(QMouseEvent *event );
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

    void ConnectWP(WayPointItem* item);


signals:
    /**
     * @brief mouseMove, mousePress, and mouseRelease signals
     * @param event
     */
    void mouseMove(QMouseEvent *event);
    void mousePress(QMouseEvent *event);
    void mouseRelease(QMouseEvent *event);

    /**
     * @brief Notify connected widgets about new map zoom
     */
    void zoomChanged(double zoomt,double zoom, double zoomd);
    void zoomChanged(int newZoom);

    /**
     * @brief fires when one of the WayPoints numbers changes (not fired if due to a auto-renumbering)
     *
     * @param oldnumber WayPoint old number
     * @param newnumber WayPoint new number
     * @param waypoint a pointer to the WayPoint that was renumbered
     */
    void WPNumberChanged(int const& oldnumber,int const& newnumber,WayPointItem* waypoint);

    /**
     * @brief Fired when the description, altitude or coordinates of a WayPoint changed
     *
     * @param waypoint a pointer to the WayPoint
     */
    void WPValuesChanged(WayPointItem* waypoint);

    /**
     * @brief Fires when a new WayPoint is inserted
     *
     * @param number new WayPoint number
     * @param waypoint WayPoint inserted
     */
    void WPReached(WayPointItem* waypoint);

    /**
     * @brief Fires when a new WayPoint is inserted
     *
     * @param number new WayPoint number
     * @param waypoint WayPoint inserted
     */
    void WPInserted(int const& number,WayPointItem* waypoint);

    /**
     * @brief Fires When a WayPoint is deleted
     *
     * @param number number of the deleted WayPoint
     */
    void WPDeleted(int const& number);

    /**
     * @brief Fires When a WayPoint is Reached
     *
     * @param number number of the Reached WayPoint
     */
    void UAVReachedWayPoint(int const& waypointnumber, WayPointItem* waypoint);

    /**
     * @brief Fires When the UAV lives the safety bouble
     *
     * @param position the position of the UAV
     */
    void UAVLeftSafetyBouble(opmap::PointLatLng const& position);

    /**
     * @brief Fires when map position changes
     *
     * @param point the point in LatLng of the new center of the map
     */
    void OnCurrentPositionChanged(opmap::PointLatLng point);

    /**
     * @brief Fires when there are no more tiles to load
     *
     */
    void OnTileLoadComplete();

    /**
     * @brief Fires when tiles loading begins
     *
     */
    void OnTileLoadStart();

    /**
     * @brief Fires when the map is dragged
     *
     */
    void OnMapDrag();

    /**
     * @brief Fires when map zoom changes
     *
     */
    void OnMapZoomChanged();

    /**
     * @brief Fires when map type changes
     *
     * @param type The maps new type
     */
    void OnMapTypeChanged(MapType::Types type);

    /**
     * @brief Fires when an error ocurred while loading a tile
     *
     * @param zoom tile zoom
     * @param pos tile position
     */
    void OnEmptyTileError(int zoom, opmap::Point pos);

    /**
     * @brief Fires when the number of tiles in the load queue changes
     *
     * @param number the number of tiles still in the queue
     */
    void OnTilesStillToLoad(int number);

    // ———————— 车载导航信号 ————————
    /** @brief 导航路线规划完成（含重规划），route 为 WGS-84 折线+分步指令 */
    void navigationRouteReady(opmap::Route route);
    /** @brief 沿路进度：剩余距离（米）、剩余时间（秒）、当前中文转向指令 */
    void navigationProgress(double remainingMeters, int remainingSeconds, QString instruction);
    /** @brief 车辆偏离路线确认（pos 偏离 deviationMeters 米） */
    void offRouteDetected(opmap::PointLatLng pos, double deviationMeters);
    /** @brief 偏航后重规划成功，已切换新路线 */
    void rerouteReady(opmap::Route route);
    /** @brief 到达目的地 */
    void navigationArrived();
    /** @brief 导航失败（无路由服务、服务忙或初次规划失败） */
    void navigationFailed(QString reason);

    // ———————— IP 定位信号 ————————
    /** @brief IP 定位成功（pos 为 WGS-84 城市级坐标，city 为城市名） */
    void ipLocationReady(opmap::PointLatLng pos, QString city);
    /** @brief IP 定位失败（双源均不可用或返回异常） */
    void ipLocationFailed(QString reason);

public slots:
    /**
     * @brief Ripps the current selection to the DB
     */
    void RipMap();

    /**
     * @brief 发起导航：以最近喂入的车位置（否则地图中心）为起点规划到 dest，
     *        成功后自动开始沿路指引
     */
    void NavigateTo(opmap::PointLatLng const& dest);

    /**
     * @brief 喂入车辆实时位置（WGS-84）：同步 UAV 图标并驱动导航进度/偏航/到达
     */
    void UpdateVehiclePosition(opmap::PointLatLng const& pos);

    /// 停止导航并清除路线绘制
    void StopNavigation();

    /**
     * @brief 发起一次 IP 定位（城市级兜底，双源自动回退，8 秒超时），
     *        结果经 ipLocationReady/ipLocationFailed 信号返回；在途时重复调用被忽略
     */
    void RequestIpLocation();

    /// 是否有 IP 定位请求在途
    bool IsIpLocationBusy() const;

    /**
     * @brief Sets the map zoom level
     */
    void SetZoom(double const& value){map->SetZoom(value);}

    /**
     * @brief Sets the map zoom level
     */
    void SetZoom(int const& value){map->SetZoom(value);}

    /**
     * @brief Notify external widgets about map zoom change
     */
    void emitMapZoomChanged()
    {
        emit zoomChanged(ZoomReal());
    }
};

} // end of namespace opmap

#endif // OPMAPWIDGET_H
