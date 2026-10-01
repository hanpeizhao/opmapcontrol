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
#include <QElapsedTimer>
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
#include "positionsource.h"

namespace opmap {

class UAVItem;
class GPSItem;
class HomeItem;
class AbstractRouteProvider;
class IpLocationProvider;
class GeofenceItem;
class NavigationEngine;
class WaypointMissionEngine;
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
    /**
     * @brief 地图点选模式：库内统一处理 6px 防抖、多点累积与右键结束，
     *        上层只接 positionPicked/pickFinished 两个信号即可完成取点交互
     */
    enum PickMode
    {
        PickNone,       ///< 正常浏览（无取点）
        PickWaypoint,   ///< 点选放航点（单发：取一次自动结束）
        PickOrigin,     ///< 点选导航起点（单发）
        PickDest,       ///< 点选导航目的地（单发）
        PickFence,      ///< 围栏取点：多点累积 + 橡皮筋预览，右键/再设 PickNone 闭合
        PickPosition    ///< 点选喂位置：点哪喂哪（连续取点，右键/再设 PickNone 结束）
    };

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
    /** @brief 内部地图画布访问器：供上层直接挂载自定义地理锚定图元（WayPointItem 等） */
    MapGraphicItem* GetMap() const { return map; }
    QGraphicsItemGroup* waypointLine(int id);
    void SetShowUAV(bool const& value);
    void SetShowGPS(bool const& value);   ///< 独立 GPS 位置标记（"我的位置"图标，与导航车互不相干）
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

    /**
     * @brief 选择备选路线（routeAlternativesReady 给出的索引，0=推荐路线）：
     *        立即切换画布显示，之后的导航/偏航重规划也沿选中的那条走
     */
    void SelectRoute(int index);

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
    UAVItem *EnsureUAV(int id);   ///< 惰性取用 UAV：不存在则创建并套用默认样式
    void ConnectUAV(UAVItem *uav);   ///< UAV 事件 → facade 信号转发（到达/飞出安全圈/回圈）
    void ShowRealLocation(opmap::PointLatLng const& pos);   ///< "我的位置"落到地图：GPS 图标+居中+街区缩放
    void HandlePickClick(opmap::PointLatLng const& pos);   ///< 一次有效选点：累积+发信号+单发自动收尾
    void EndPick();               ///< 结束当前取点（围栏收尾 + pickFinished）
    WayPointItem *EnsureRouteMarker(WayPointItem *&marker, opmap::PointLatLng const& pos, QString const& text);   ///< 惰性取用路线端点图钉（auxiliary 装饰航点）
    void ClearRouteMarkers();     ///< 清除路线端点图钉（停止导航时）
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
    QList<opmap::Route> routeAlternatives;         ///< 最近一次规划的备选路线全集（SelectRoute 取用）
    opmap::WaypointMissionEngine *missionEngine;   ///< 航点任务状态机（喂点驱动）
    opmap::RouteItem *routeItem;                   ///< 路线绘制项（随 map 析构）
    opmap::IpLocationProvider *ipLocator;          ///< IP 定位服务（城市级兜底）
    opmap::GeofenceItem *geofenceItem;             ///< 多边形地理围栏（多边形内为允许区）
    bool geofenceBreached;                         ///< 当前是否处于越界状态（沿沿只发一次信号）
    opmap::PointLatLng vehiclePos;                 ///< 最近喂入的车位置
    bool vehiclePosValid;
    // —— "我的位置"（一键定位目标：只认真实源，模拟车位置不参与）——
    opmap::PointLatLng lastRealPos;                ///< 最近真实源（GPS/MAVLink）位置
    bool lastRealPosValid;
    QElapsedTimer lastRealPosAge;                  ///< 距上次真实源喂点的时长（活性判断用）
    bool followVehicle;                            ///< 地图跟随车辆开关（喂点时自动居中）
    bool locatePending;                            ///< 定位按钮触发的 IP 兜底在途（结果只居中）

    // —— 地图点选状态 ——
    PickMode pickMode;                             ///< 当前取点模式
    QPoint pickPressPos;                           ///< 按下位置（抬起位移 <6px 才算选点，拖图不算）
    QList<opmap::PointLatLng> pickPoints;          ///< 本次取点累积（围栏多点）

    // —— 位置源状态 ——
    PositionSource positionSource;                 ///< 当前位置源
    PositionSourceManager *posSourceManager;       ///< 统一位置源管理器（providers 层）

    // —— 路线端点图钉 ——
    WayPointItem *routeFromMarker;                 ///< 路线起点图钉（auxiliary，装饰性不进任务序列）
    WayPointItem *routeToMarker;                   ///< 路线终点图钉（auxiliary，装饰性不进任务序列）

private slots:
    void diagRefresh();
    void onNavProgress(double traveledM, double remainingM, int remainingS, const QString &instruction);
    /// 缓存本次规划的备选路线并转发 routeAlternativesReady
    void onRouteAlternatives(QList<opmap::Route> routes);
    //   WayPointItem* item;//apagar
    // 航点任务引擎信号 → facade 信号转发
    void onMissionStarted();
    void onMissionCurrentWaypointChanged(int index);
    void onMissionWaypointReached(int index, int action);
    void onMissionHoverStateChanged(bool hovering, int seconds);
    void onMissionActionTriggered(int index, int action);
    void onMissionFinished();
    /// IP 定位结果：先处理一键定位兜底（只居中），再转发 ipLocationReady
    void onIpLocated(opmap::PointLatLng pos, QString city);
    /// IP 定位失败：清一键定位兜底标记再转发（防残留 locatePending 误居中后续结果）
    void onIpLocationFailed(QString reason);
    /// 位置源喂点：记录真实源（GPS/MAVLink）位置供一键定位取用，并同步 GPS 图标
    void onPositionUpdate(opmap::PointLatLng pos, double altM, double headingDeg, int source);

protected:
    MapGraphicItem *map;
    void resizeEvent(QResizeEvent *event);
    void showEvent ( QShowEvent * event );
    void closeEvent(QCloseEvent *event);

    void mouseMoveEvent(QMouseEvent *event );
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void contextMenuEvent(QContextMenuEvent *event);   ///< 取点中右键=结束取点，不弹上层菜单

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
    void UAVEnteredSafetyBouble(opmap::PointLatLng const& position);

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
    /** @brief 本次规划的备选路线全集（第 0 条=推荐路线，与路线绘制同步） */
    void routeAlternativesReady(QList<opmap::Route> routes);

    // ———————— IP 定位信号 ————————
    /** @brief IP 定位成功（pos 为 WGS-84 城市级坐标，city 为城市名） */
    void ipLocationReady(opmap::PointLatLng pos, QString city);
    /** @brief IP 定位失败（双源均不可用或返回异常） */
    void ipLocationFailed(QString reason);
    void geofenceBreach(opmap::PointLatLng position);   ///< UAV 飞出多边形围栏

    // —— 航点任务飞行（WaypointMissionEngine 转发）——
    void missionStarted();                              ///< 任务启动
    void missionCurrentWaypointChanged(int index);      ///< 当前目标航点切换（0 起）
    void missionWaypointReached(int index, int action); ///< 抵达某航点
    void missionHoverStateChanged(bool hovering, int seconds); ///< 悬停开始/结束
    void missionActionTriggered(int index, int action); ///< 到达动作触发（拍照/悬停）
    void missionFinished();                             ///< 全部航点完成
    void geofenceEntered(opmap::PointLatLng position);  ///< UAV 回到多边形围栏内
    void mapFollowChanged(bool following);              ///< 地图跟随车辆开关变化（供 UI 复选框同步）

    // —— 地图点选信号 ——
    /** @brief 一次有效选点（库已完成防抖；mode 为 PickMode 枚举值）。
     *         单发模式（航点/起点/目的地）取一次后自动结束并再发 pickFinished */
    void positionPicked(int mode, opmap::PointLatLng pos);
    /** @brief 取点结束：右键取消/切换模式/再设 PickNone 均触发；
     *         points 为本次取到的全部点（围栏闭合时即顶点序列） */
    void pickFinished(int mode, QList<opmap::PointLatLng> points);
    /** @brief 右键菜单请求（非取点模式）：取点中的右键由库拦截为"结束取点"，
     *         正常右键转发此信号供上层弹自定义菜单（等同 Qt::CustomContextMenu） */
    void mapContextMenuRequested(QPoint pos);

    // —— 位置源信号 ——
    /** @brief 统一位置流：SetPositionSource 启用的源产生的每个位置点
     *         （库已同步喂入 UpdateVehiclePosition，上层通常只做记录/日志）；
     *         source 为 PositionSource 枚举值 */
    void positionUpdated(opmap::PointLatLng pos, double altM, double headingDeg, int source);
    /** @brief 位置源错误（fatal=true：源无法启用已被库停用，需上层回退 UI；
     *         false：单次失败，源继续运行自动重试） */
    void positionSourceError(QString reason, bool fatal);
    void positionLinkAlive();     ///< MAVLink 链路建立/恢复
    void positionLinkTimeout();   ///< MAVLink 链路超时（5 秒无包）

    // ———————— 离线下载进度信号（转发自 MapRipper）————————
    /** @brief 抓取进度百分比（0-100） */
    void mapDownloadProgress(int percent);
    /** @brief 抓取进度（总瓦片数 / 已完成数） */
    void mapDownloadTiles(int total, int actual);
    /** @brief 本轮抓取结束（完成或取消） */
    void mapDownloadFinished();

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

    /**
     * @brief 喂入 UAV 实时位置（WGS-84）：驱动轨迹/到达判定/围栏越界判定
     *        （与 UpdateVehiclePosition 的区别：此路径带围栏判定，供任务飞行喂点）
     *        首次喂点自动创建 UAV 图标（默认位置标记大头针 + 每秒轨迹点）
     */
    void SetUAVPos(int const& id, opmap::PointLatLng const& pos, int const& alt);

    /**
     * @brief 设置 UAV 航向（度，正北 0 顺时针）；UAV 未创建时忽略
     */
    void SetUAVHeading(int const& id, qreal const& deg);

    /**
     * @brief 地图跟随车辆开关：开启后每次喂点地图自动居中到 UAV 位置；
     *        每次喂点自动创建 UAV 时同样生效。状态变化经 mapFollowChanged 通知
     */
    void SetFollowVehicle(bool const& on);

    /// 停止导航并清除路线绘制
    void StopNavigation();

    /**
     * @brief 一键定位（车载导航式）：有车辆位置直接居中并切街区级缩放（zoom 15）；
     *        从未喂过位置则自动 IP 定位兜底——结果只居中不喂导航车
     *        （城市级精度进入位置流会污染导航引擎进度），完成/失败仍发
     *        ipLocationReady/ipLocationFailed 供上层提示
     */
    void LocateCurrentPosition();

    // —— 位置源管理（库内互斥切换：先停用全部旧源再启用目标源）——
    /// 切换位置源；位置点自动喂入 UpdateVehiclePosition 并经 positionUpdated 分发
    void SetPositionSource(PositionSource src);
    PositionSource GetPositionSource() const { return positionSource; }   ///< 当前位置源

    // —— 地图点选（库内防抖/多点累积/右键结束）——
    /// 进入/切换/退出取点模式；设为 PickNone 或右键 = 结束（围栏按顶点数闭合或清理）
    void SetPickMode(PickMode mode);
    PickMode GetPickMode() const { return pickMode; }   ///< 当前取点模式

    /**
     * @brief 发起一次 IP 定位（城市级兜底，双源自动回退，8 秒超时），
     *        结果经 ipLocationReady/ipLocationFailed 信号返回；在途时重复调用被忽略
     */
    void RequestIpLocation();

    /// 是否有 IP 定位请求在途
    bool IsIpLocationBusy() const;

    // —— 多边形地理围栏（多边形内部为允许飞行区）——
    /// 设置/更新围栏顶点；空列表清除，1~2 点为取点预览（不参与越界判定），≥3 点生效
    void SetGeofence(QList<opmap::PointLatLng> const& vertices);
    void ClearGeofence();                                  ///< 移除围栏
    bool HasGeofence() const;                              ///< 是否已设置有效围栏
    /// 喂点后调用：判定位置是否越界，越界沿沿第一次发出 geofenceBreach
    void CheckGeofence(opmap::PointLatLng const& position);

    /// 导航引擎访问器（调整偏航阈值/到达阈值/重规划参数等引擎默认行为）
    opmap::NavigationEngine *GetNavigationEngine() const;

    // —— 航点任务飞行（库内状态机，喂点驱动）——
    /// 启动航点任务：从航点图元提取坐标/悬停时长/动作组装任务并启动。
    /// 启动时库自动完成默认编排：惰性创建 UAV、打开到达判定（半径=arrivalRadiusMeters）、
    /// 显示 Home 安全圈、UAV 摆位到起飞点并跳转视图（Home 优先，其次车辆位置）、
    /// 自动暂停地图跟随。之后每次喂点（UpdateVehiclePosition/SetUAVPos）由库推进状态：
    /// 到达判定→悬停计时→动作信号→下一航点→missionFinished
    void StartWaypointMission(QList<WayPointItem*> const& waypoints,
                              double arrivalRadiusMeters = 15.0);
    void StopWaypointMission();                            ///< 中止当前任务
    bool IsWaypointMissionActive() const;                  ///< 任务是否进行中

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
