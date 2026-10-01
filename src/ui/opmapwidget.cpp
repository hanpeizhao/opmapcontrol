/**
******************************************************************************
*
* @file       opmapwidget.cpp
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

#include "opmapwidget.h"
#include <QtGui>
#include <QMetaObject>
#include "waypointitem.h"
#include "geoutils.h"
#include "osrmrouteprovider.h"
#include "iplocationprovider.h"
#include "positionsource.h"
#include "geofenceitem.h"
#include "navigationengine.h"
#include "waypointmissionengine.h"
#include "routeitem.h"

namespace opmap {

OPMapWidget::OPMapWidget(QWidget *parent, Configuration *config) : QGraphicsView(parent),
    configuration(config),
    UAV(0),
    GPS(0),
    Home(0),
    followmouse(true),
    compass(0),
    showuav(false),
    showhome(false),
    diagTimer(0),
    showDiag(false),
    diagGraphItem(0),
    routeProvider(0),
    navEngine(0),
    missionEngine(0),
    routeItem(0),
    ipLocator(0),
    geofenceItem(0),
    geofenceBreached(false),
    vehiclePosValid(false),
    followVehicle(false),
    locatePending(false),
    pickMode(PickNone),
    positionSource(SourceNone)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    service=new opmap::MapService;
    configuration->SetMapService(service);
    core=new opmap::MapEngine(service);
    map=new MapGraphicItem(core, config);
    mscene.addItem(map);
    this->setScene(&mscene);
    this->adjustSize();

    connect(map,SIGNAL(zoomChanged(double,double,double)),this,SIGNAL(zoomChanged(double,double,double)));
    connect(map->core,SIGNAL(OnCurrentPositionChanged(opmap::PointLatLng)),this,SIGNAL(OnCurrentPositionChanged(opmap::PointLatLng)));
    connect(map->core,SIGNAL(OnEmptyTileError(int,opmap::Point)),this,SIGNAL(OnEmptyTileError(int,opmap::Point)));
    connect(map->core,SIGNAL(OnMapDrag()),this,SIGNAL(OnMapDrag()));
    connect(map->core,SIGNAL(OnMapTypeChanged(MapType::Types)),this,SIGNAL(OnMapTypeChanged(MapType::Types)));
    connect(map->core,SIGNAL(OnMapZoomChanged()),this,SIGNAL(OnMapZoomChanged()));
    connect(map->core,SIGNAL(OnMapZoomChanged()),this,SLOT(emitMapZoomChanged()));
    connect(map->core,SIGNAL(OnTileLoadComplete()),this,SIGNAL(OnTileLoadComplete()));
    connect(map->core,SIGNAL(OnTileLoadStart()),this,SIGNAL(OnTileLoadStart()));
    connect(map->core,SIGNAL(OnTilesStillToLoad(int)),this,SIGNAL(OnTilesStillToLoad(int)));

    SetShowDiagnostics(showDiag);
    this->setMouseTracking(followmouse);
    SetShowCompass(true);

    // —— 车载导航：provider → 引擎 → 路线绘制项 ——
    routeProvider = new OsrmRouteProvider(this);
    navEngine = new NavigationEngine(this);
    navEngine->SetRouteProvider(routeProvider);
    routeItem = new RouteItem(map);
    routeItem->hide();

    ipLocator = new IpLocationProvider(this);
    // 结果先经 onIpLocated（处理一键定位兜底），再转发 ipLocationReady
    connect(ipLocator, SIGNAL(locationReady(opmap::PointLatLng,QString)),
            this, SLOT(onIpLocated(opmap::PointLatLng,QString)));
    connect(ipLocator, SIGNAL(locationFailed(QString)),
            this, SIGNAL(ipLocationFailed(QString)));

    // —— 位置源管理：统一互斥切换；位置点自动喂车 + 分发 positionUpdated ——
    posSourceManager = new PositionSourceManager(this);
    connect(posSourceManager, SIGNAL(positionUpdated(opmap::PointLatLng,double,double,int)),
            this, SLOT(UpdateVehiclePosition(opmap::PointLatLng)));
    connect(posSourceManager, SIGNAL(positionUpdated(opmap::PointLatLng,double,double,int)),
            this, SIGNAL(positionUpdated(opmap::PointLatLng,double,double,int)));
    connect(posSourceManager, SIGNAL(sourceError(QString,bool)),
            this, SIGNAL(positionSourceError(QString,bool)));
    connect(posSourceManager, SIGNAL(linkAlive()), this, SIGNAL(positionLinkAlive()));
    connect(posSourceManager, SIGNAL(linkTimeout()), this, SIGNAL(positionLinkTimeout()));

    // —— 航点任务：引擎信号 → facade 信号转发（喂点驱动，见 UpdatePosition）——
    missionEngine = new WaypointMissionEngine(this);
    connect(missionEngine, SIGNAL(missionStarted()), this, SLOT(onMissionStarted()));
    connect(missionEngine, SIGNAL(currentWaypointChanged(int)), this, SLOT(onMissionCurrentWaypointChanged(int)));
    connect(missionEngine, SIGNAL(waypointReached(int,int)), this, SLOT(onMissionWaypointReached(int,int)));
    connect(missionEngine, SIGNAL(hoverStateChanged(bool,int)), this, SLOT(onMissionHoverStateChanged(bool,int)));
    connect(missionEngine, SIGNAL(actionTriggered(int,int)), this, SLOT(onMissionActionTriggered(int,int)));
    connect(missionEngine, SIGNAL(missionFinished()), this, SLOT(onMissionFinished()));

    connect(navEngine, SIGNAL(routePlanned(opmap::Route)), routeItem, SLOT(SetRoute(opmap::Route)));
    connect(navEngine, SIGNAL(routePlanned(opmap::Route)), this, SIGNAL(navigationRouteReady(opmap::Route)));
    connect(navEngine, SIGNAL(rerouteReady(opmap::Route)), routeItem, SLOT(SetRoute(opmap::Route)));
    connect(navEngine, SIGNAL(rerouteReady(opmap::Route)), this, SIGNAL(rerouteReady(opmap::Route)));
    connect(navEngine, SIGNAL(progressUpdated(double,double,int,QString)),
            routeItem, SLOT(SetTraveledDistance(double)));
    connect(navEngine, SIGNAL(progressUpdated(double,double,int,QString)),
            this, SLOT(onNavProgress(double,double,int,QString)));
    connect(navEngine, SIGNAL(offRouteDetected(opmap::PointLatLng,double)),
            this, SIGNAL(offRouteDetected(opmap::PointLatLng,double)));
    connect(navEngine, SIGNAL(arrived()), this, SIGNAL(navigationArrived()));
    connect(navEngine, SIGNAL(navigationFailed(QString)), this, SIGNAL(navigationFailed(QString)));
}

void OPMapWidget::SetShowDiagnostics(bool const& value)
{
    showDiag=value;
    if(!showDiag)
    {
        if(diagGraphItem!=0)
        {
            delete diagGraphItem;
            diagGraphItem=0;
        }
        if(diagTimer!=0)
        {
            delete diagTimer;
            diagTimer=0;
        }
    }
    else
    {
        diagTimer=new QTimer();
        connect(diagTimer,SIGNAL(timeout()),this,SLOT(diagRefresh()));
        diagTimer->start(500);
    }
}

void OPMapWidget::SetUavPic(QString UAVPic)
{
    if(UAV!=0)
        UAV->SetUavPic(UAVPic);
    if(GPS!=0)
        GPS->SetUavPic(UAVPic);
}

UAVItem* OPMapWidget::AddUAV(int id)
{
    UAVItem* newUAV = new UAVItem(map,this);
    newUAV->setParentItem(map);
    UAVS.insert(id, newUAV);
    QGraphicsItemGroup* waypointLine = new QGraphicsItemGroup(map);
    waypointLines.insert(id, waypointLine);

    return newUAV;
}

void OPMapWidget::AddUAV(int id, UAVItem* uav)
{
    uav->setParentItem(map);
    QGraphicsItemGroup* waypointLine = new QGraphicsItemGroup(map);
    waypointLines.insert(id, waypointLine);
    UAVS.insert(id, uav);
}

void OPMapWidget::DeleteUAV(int id)
{
    UAVItem* uav = UAVS.value(id, NULL);
    UAVS.remove(id);
    if (uav) {
        // remove all trail lines
        uav->DeleteTrail();

        // delete this UAV item
        delete uav;
        uav = NULL;
    }

    // remove all associated WP lines
    QGraphicsItemGroup* wpLine = waypointLines.value(id, NULL);
    waypointLines.remove(id);
    if (wpLine) {
        delete wpLine;
        wpLine = NULL;
    }
}

/**
     * @return The reference to the UAVItem or NULL if no item exists yet
     * @see AddUAV() for adding a not yet existing UAV to the map
     */
UAVItem* OPMapWidget::GetUAV(int id)
{
    return UAVS.value(id, 0);
}

const QList<UAVItem*> OPMapWidget::GetUAVS()
{
    return UAVS.values();
}

QGraphicsItemGroup* OPMapWidget::waypointLine(int id)
{
    return waypointLines.value(id, NULL);
}

void OPMapWidget::SetShowUAV(const bool &value)
{
    if( value && UAV==0 ) {
        UAV = new UAVItem(map,this);
        UAV->setParentItem(map);

        // FIXME XXX The map widget is here actually handling
        // safety and mission logic - might be worth some refactoring
        connect(this,SIGNAL(UAVLeftSafetyBouble(opmap::PointLatLng)),UAV,SIGNAL(UAVLeftSafetyBouble(opmap::PointLatLng)));
        connect(this,SIGNAL(UAVReachedWayPoint(int,WayPointItem*)),UAV,SIGNAL(UAVReachedWayPoint(int,WayPointItem*)));
    } else if(!value) {
        if(UAV!=0) {
            UAV->DeleteTrail();

            delete UAV;
            UAV=0;
        }
    }

    if( value && GPS==0 ) {
        GPS=new GPSItem(map,this);
        GPS->setParentItem(map);
    } else if(!value) {
        if(GPS!=0) {
            GPS->DeleteTrail();

            delete GPS;
            GPS=0;
        }
    }
}

void OPMapWidget::SetShowHome(const bool &value)
{
    if(value && Home==0)  {
        Home=new HomeItem(map, this);
        Home->setParentItem(map);
    } else if(!value) {
        if(Home!=0) {
            delete Home;
            Home=0;
        }
    }
}

// ————————————————— 车载导航 —————————————————

void OPMapWidget::SetRouteProvider(opmap::AbstractRouteProvider *provider)
{
    if (!provider || provider == routeProvider)
        return;
    delete routeProvider;          // 接管所有权；在途请求随 QNAM 释放中止
    routeProvider = provider;
    routeProvider->setParent(this);
    navEngine->SetRouteProvider(routeProvider);
}

void OPMapWidget::NavigateTo(opmap::PointLatLng const& dest)
{
    const opmap::PointLatLng from = vehiclePosValid ? vehiclePos
                                                    : map->core->CurrentPosition();
    navEngine->NavigateTo(from, dest);
}

void OPMapWidget::PlanRoute(opmap::PointLatLng const& from, opmap::PointLatLng const& to)
{
    navEngine->PlanRoute(from, to);
    routeItem->setVisible(true);   // 预览结果经 routePlanned 回来时绘制
}

/// 惰性取用 UAV：不存在则创建并套用默认跟踪样式——
/// 图标=位置标记大头针（有位置数据才出现，语义为"这是实时位置"），
/// 轨迹=每秒一个点 + 实线连线，跟随=SetFollowVehicle 设定的开关
UAVItem *OPMapWidget::EnsureUAV(int id)
{
    UAVItem *uav = GetUAV(id);
    if (uav)
        return uav;
    uav = AddUAV(id);
    uav->SetIcon(QString::fromUtf8(":/markers/images/bigMarkerGreen.png"));
    uav->SetTrailType(UAVTrailType::ByTimeElapsed);
    uav->SetTrailTime(1);
    uav->SetShowTrailLine(true);
    uav->SetMapFollowType(followVehicle ? UAVMapFollowType::CenterMap
                                        : UAVMapFollowType::None);
    return uav;
}

void OPMapWidget::UpdateVehiclePosition(opmap::PointLatLng const& pos)
{
    // UAV 图标同步（EnsureUAV 惰性创建：首次喂点图标即出现，避免静默无效）
    UAVItem *uav = EnsureUAV(0);
    if (vehiclePosValid && geoutils::haversineDistanceM(vehiclePos, pos) > 1.0)
        uav->SetUAVHeading(geoutils::bearingDeg(vehiclePos, pos));
    uav->SetUAVPos(pos, 0);

    vehiclePos = pos;
    vehiclePosValid = true;
    navEngine->UpdatePosition(pos);
    CheckGeofence(pos);   // 围栏判定对所有位置源（模拟/GPS/MAVLink）统一生效
    if (missionEngine && missionEngine->IsMissionActive())
        missionEngine->UpdatePosition(pos);   // 航点任务状态推进（喂点驱动）
}

void OPMapWidget::SetUAVPos(int const& id, opmap::PointLatLng const& pos, int const& alt)
{
    UAVItem *uav = EnsureUAV(id);
    uav->SetUAVPos(pos, alt);
    CheckGeofence(pos);   // 任务飞行喂点同样接入围栏越界判定
    if (missionEngine && missionEngine->IsMissionActive())
        missionEngine->UpdatePosition(pos);   // 任务状态推进（真机遥测喂点同构）
}

void OPMapWidget::SetUAVHeading(int const& id, qreal const& deg)
{
    if (UAVItem *uav = GetUAV(id))
        uav->SetUAVHeading(deg);   // 无位置即无图标：航向没有意义，忽略
}

void OPMapWidget::SetFollowVehicle(bool const& on)
{
    if (followVehicle == on)
        return;
    followVehicle = on;
    if (UAVItem *uav = GetUAV(0))
        uav->SetMapFollowType(on ? UAVMapFollowType::CenterMap : UAVMapFollowType::None);
    emit mapFollowChanged(on);   // 供上层 UI 开关同步
}

void OPMapWidget::StopNavigation()
{
    navEngine->Stop();
    routeItem->ClearRoute();
}

void OPMapWidget::RequestIpLocation()
{
    if (ipLocator)
        ipLocator->requestLocation();
}

bool OPMapWidget::IsIpLocationBusy() const
{
    return ipLocator ? ipLocator->isBusy() : false;
}

// ———————— 一键定位（车载导航式） ————————
// 定位语义分级：运动位置流（模拟/GPS/MAVLink）喂出的 vehiclePos 是权威位置，
// 直接居中；从未有过位置时才用城市级 IP 定位兜底——只居中绝不喂 vehiclePos
//（城市级进位置流会把导航引擎的进度推到错误位置，见路线着色缺失教训）

void OPMapWidget::LocateCurrentPosition()
{
    if (vehiclePosValid) {
        SetCurrentPosition(vehiclePos);
        if (ZoomTotal() < 15.0)
            SetZoom(15.0);          // 定位时切到街区级缩放
        return;
    }
    locatePending = true;           // 结果在 onIpLocated 兜底居中
    RequestIpLocation();
}

void OPMapWidget::onIpLocated(opmap::PointLatLng pos, QString city)
{
    if (locatePending) {
        locatePending = false;
        SetCurrentPosition(pos);    // 只居中：城市级位置不喂导航车
        if (ZoomTotal() < 15.0)
            SetZoom(15.0);
    }
    emit ipLocationReady(pos, city);
}

// ———————— 位置源管理（互斥切换） ————————

void OPMapWidget::SetPositionSource(PositionSource src)
{
    if (positionSource == src)
        return;
    positionSource = src;
    posSourceManager->SetSource(src);   // 互斥停启/错误回退在管理器内完成
}

opmap::NavigationEngine *OPMapWidget::GetNavigationEngine() const
{
    return navEngine;
}

// ———————— 航点任务飞行 ————————

void OPMapWidget::StartWaypointMission(QList<WayPointItem*> const& waypoints,
                                       double arrivalRadiusMeters)
{
    if (waypoints.isEmpty())
        return;
    // —— 任务启动默认编排（上层无需再手工铺垫）——
    // 惰性建 UAV 并打开自动到达判定（进入 arrivalRadiusMeters 即到达信号）
    UAVItem *uav = EnsureUAV(0);
    uav->SetAutoSetReached(true);
    uav->SetAutoSetDistance(arrivalRadiusMeters);
    if (Home)
        Home->SetShowSafeArea(true);   // 起飞点安全圈可见
    // 起飞点取位：Home 返航点优先，其次车辆位置；有则摆好机位并跳转视图
    opmap::PointLatLng start;
    bool haveStart = false;
    if (Home) {
        start = Home->Coord();
        haveStart = true;
    } else if (vehiclePosValid) {
        start = vehiclePos;
        haveStart = true;
    }
    if (haveStart) {
        uav->SetUAVPos(start, 120);
        uav->SetUAVHeading(0);
        SetCurrentPosition(start);   // 地图跳到起飞点，起飞位置一目了然
    }
    // 跟随会让 UAV 钉在屏幕中央、看起来"原地不动"，任务观察期间自动暂停
    SetFollowVehicle(false);

    QList<WaypointMissionEngine::MissionWaypoint> mission;
    for (int i = 0; i < waypoints.size(); ++i) {
        WayPointItem *wp = waypoints.at(i);
        mission.append(WaypointMissionEngine::MissionWaypoint(
                           wp->Coord(), wp->HoverTime(), (int)wp->Action()));
    }
    missionEngine->SetMission(mission, arrivalRadiusMeters);
    missionEngine->StartMission();
}

void OPMapWidget::StopWaypointMission()
{
    missionEngine->StopMission();
}

bool OPMapWidget::IsWaypointMissionActive() const
{
    return missionEngine->IsMissionActive();
}

// 任务引擎信号 → facade 信号原样转发

void OPMapWidget::onMissionStarted() { emit missionStarted(); }

void OPMapWidget::onMissionCurrentWaypointChanged(int index)
{
    emit missionCurrentWaypointChanged(index);
}

void OPMapWidget::onMissionWaypointReached(int index, int action)
{
    emit missionWaypointReached(index, action);
}

void OPMapWidget::onMissionHoverStateChanged(bool hovering, int seconds)
{
    emit missionHoverStateChanged(hovering, seconds);
}

void OPMapWidget::onMissionActionTriggered(int index, int action)
{
    emit missionActionTriggered(index, action);
}

void OPMapWidget::onMissionFinished() { emit missionFinished(); }

// ———————— 多边形地理围栏 ————————

void OPMapWidget::SetGeofence(QList<opmap::PointLatLng> const& vertices)
{
    if (vertices.isEmpty()) {
        ClearGeofence();
        return;
    }
    if (!geofenceItem) {
        geofenceItem = new GeofenceItem(map, map);   // 挂为 map 子项，随地图变换
        geofenceBreached = false;
    }
    geofenceItem->SetVertices(vertices);   // 1~2 点为取点预览（画顶点/连线），不参与判定
}

void OPMapWidget::ClearGeofence()
{
    delete geofenceItem;   // 父子挂载，自动移出场景
    geofenceItem = 0;
    geofenceBreached = false;
}

bool OPMapWidget::HasGeofence() const
{
    return geofenceItem != 0;
}

void OPMapWidget::CheckGeofence(opmap::PointLatLng const& position)
{
    if (!geofenceItem || geofenceItem->Vertices().size() < 3)
        return;   // 取点预览态（<3 点）不参与越界判定
    const bool inside = GeofenceItem::Contains(geofenceItem->Vertices(), position);
    if (!inside && !geofenceBreached) {
        geofenceBreached = true;    // 越界沿沿只发一次，回界内复位
        emit geofenceBreach(position);
    }
    else if (inside && geofenceBreached)
    {
        geofenceBreached = false;
        emit geofenceEntered(position);   // 回到围栏内也通知上层（进入/退出成对事件）
    }
}

void OPMapWidget::SetShowRoute(bool const& value)
{
    routeItem->setVisible(value);
}

bool OPMapWidget::ShowRoute() const
{
    return routeItem->isVisible();
}

opmap::Route OPMapWidget::CurrentNavigationRoute() const
{
    return navEngine->CurrentRoute();
}

bool OPMapWidget::IsNavigating() const
{
    return navEngine->IsNavigating();
}

void OPMapWidget::onNavProgress(double traveledM, double remainingM, int remainingS,
                                const QString &instruction)
{
    Q_UNUSED(traveledM);
    emit navigationProgress(remainingM, remainingS, instruction);
}

void OPMapWidget::resizeEvent(QResizeEvent *event)
{
    if (scene())
        scene()->setSceneRect(
                    QRect(QPoint(0, 0), event->size()));
    QGraphicsView::resizeEvent(event);
    if(compass)
        compass->setScale(0.1+0.05*(qreal)(event->size().width())/1000*(qreal)(event->size().height())/600);

}

QSize OPMapWidget::sizeHint() const
{
    return map->sizeHint();
}

void OPMapWidget::showEvent(QShowEvent *event)
{
    connect(&mscene,SIGNAL(sceneRectChanged(QRectF)),map,SLOT(resize(QRectF)));
    map->start();
    QGraphicsView::showEvent(event);
}

OPMapWidget::~OPMapWidget()
{
    // 先停导航引擎并释放路由 provider（中止在途网络请求），再走原有析构链
    if (navEngine)
        navEngine->Stop();
    delete navEngine;
    delete routeProvider;

    delete UAV;

    foreach(UAVItem* uav, this->UAVS) {
        uav->DeleteTrail();
        delete uav;
    }

    delete Home;
    // 先停引擎（等待瓦片加载线程池结束），再释放服务（等待写库线程结束）
    delete map;
    delete core;
    delete configuration;
    delete service;

    foreach(QGraphicsItem* i,this->items()) {
        delete i;
    }
}

void OPMapWidget::closeEvent(QCloseEvent *event)
{
    core->OnMapClose();
    event->accept();
}

void OPMapWidget::SetUseOpenGL(const bool &value)
{
    useOpenGL=value;
    if (useOpenGL)
        setViewport(new QGLWidget(QGLFormat(QGL::SampleBuffers)));
    else
        setupViewport(new QWidget());
    update();
}

opmap::PointLatLng OPMapWidget::currentMousePosition()
{
    return currentmouseposition;
}

void OPMapWidget::mouseMoveEvent(QMouseEvent *event)
{
    QGraphicsView::mouseMoveEvent(event);
    QPointF p=event->pos();
    p=map->mapFromParent(p);
    currentmouseposition=map->FromLocalToLatLng(p.x(),p.y());

    // 围栏橡皮筋预览：末段线实时跟随鼠标，点击地图即固化一个顶点
    if (pickMode == PickFence && !pickPoints.isEmpty()) {
        QList<opmap::PointLatLng> preview = pickPoints;
        preview.append(currentmouseposition);
        SetGeofence(preview);
    }

    emit mouseMove(event);
}

void OPMapWidget::mousePressEvent(QMouseEvent *event)
{
    QGraphicsView::mousePressEvent(event);

    // 点选模式（仅左键）：记录按下位置做防抖基线；单发模式（航点/起点/目的地）按下即取点
    if (pickMode != PickNone && event->button() == Qt::LeftButton) {
        pickPressPos = event->pos();
        if (pickMode != PickFence && pickMode != PickPosition)
            HandlePickClick(currentMousePosition());
    }

    emit mousePress(event);
}

void OPMapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    QGraphicsView::mouseReleaseEvent(event);

    // 连续取点模式（围栏/喂位置）左键：抬起位移 <6px 才固化一次选点，拖动地图不算
    if (event->button() == Qt::LeftButton
            && (pickMode == PickFence || pickMode == PickPosition)
            && (event->pos() - pickPressPos).manhattanLength() <= 6)
        HandlePickClick(currentMousePosition());

    emit mouseRelease(event);
}

void OPMapWidget::contextMenuEvent(QContextMenuEvent *event)
{
    // 取点模式中右键 = 结束取点（围栏闭合/喂点停止），并拦下上层右键菜单
    if (pickMode != PickNone) {
        EndPick();
        event->accept();
        return;
    }
    // 非取点：转发场景处理，同时发右键菜单信号供上层弹自定义菜单
    QGraphicsView::contextMenuEvent(event);
    emit mapContextMenuRequested(event->pos());
}

// ———————— 地图点选（SetPickMode）————————

void OPMapWidget::SetPickMode(PickMode mode)
{
    if (pickMode == mode)
        return;
    if (pickMode != PickNone)
        EndPick();   // 模式切换前收尾当前取点（发 pickFinished + 围栏收尾）
    pickMode = mode;
    pickPoints.clear();
}

/// 结束当前取点：围栏按顶点数固化/清理预览，随后发 pickFinished
void OPMapWidget::EndPick()
{
    const PickMode finished = pickMode;
    pickMode = PickNone;
    if (finished == PickFence) {
        if (pickPoints.size() >= 3)
            SetGeofence(pickPoints);                     // 顶点足够：固化为有效围栏
        else
            SetGeofence(QList<opmap::PointLatLng>());    // 不足 3 点：清取点预览残留
    }
    emit pickFinished((int)finished, pickPoints);
    pickPoints.clear();
}

/// 一次有效选点：入库累积并发信号；单发模式取一次即自动结束
void OPMapWidget::HandlePickClick(opmap::PointLatLng const& pos)
{
    pickPoints.append(pos);
    emit positionPicked((int)pickMode, pos);
    if (pickMode == PickWaypoint || pickMode == PickOrigin || pickMode == PickDest)
        SetPickMode(PickNone);   // 单发模式：内部再发 pickFinished
}


////////////////WAYPOINT////////////////////////
WayPointItem* OPMapWidget::WPCreate()
{
    WayPointItem* item=new WayPointItem(this->CurrentPosition(),0,map);
    ConnectWP(item);
    item->setParentItem(map);
    return item;
}

void OPMapWidget::WPCreate(WayPointItem* item)
{
    ConnectWP(item);
    item->setParentItem(map);
}

void OPMapWidget::WPCreate(int id, WayPointItem* item)
{
    Q_UNUSED(id);
    static opmap::PointLatLng lastPos;

    ConnectWP(item);
    item->setParentItem(map);

    //        QGraphicsItemGroup* wpLine = waypointLines.value(id, NULL);
    //        if (!wpLine)
    //        {
    //            wpLine = new QGraphicsItemGroup(map);
    //            waypointLines.insert(id, wpLine);
    //        }

    //        if (!lastPos.IsEmpty())
    //        {
    //            wpLine->addToGroup(new TrailLineItem(lastPos, item->Coord(), Qt::red, map));
    //            lastPos = item->Coord();
    //        }



    // Add waypoint line
    //        trail->addToGroup(new TrailItem(position,altitude,color,this));
    //        if(!lasttrailline.IsEmpty())
    //            trailLine->addToGroup((new TrailLineItem(lasttrailline,position,color,map)));
    //        lasttrailline=position;
}

WayPointItem* OPMapWidget::WPCreate(opmap::PointLatLng const& coord,int const& altitude)
{
    WayPointItem* item=new WayPointItem(coord,altitude,map);
    ConnectWP(item);
    item->setParentItem(map);
    return item;
}

WayPointItem* OPMapWidget::WPCreate(opmap::PointLatLng const& coord,int const& altitude, QString const& description)
{
    WayPointItem* item=new WayPointItem(coord,altitude,description,map);
    ConnectWP(item);
    item->setParentItem(map);
    return item;
}

WayPointItem* OPMapWidget::WPInsert(const int &position)
{
    WayPointItem* item=new WayPointItem(this->CurrentPosition(),0,map);
    item->SetNumber(position);
    ConnectWP(item);
    item->setParentItem(map);
    emit WPInserted(position,item);
    return item;
}

void OPMapWidget::WPInsert(WayPointItem* item,const int &position)
{
    item->SetNumber(position);
    ConnectWP(item);
    item->setParentItem(map);
    emit WPInserted(position,item);

}

WayPointItem* OPMapWidget::WPInsert(opmap::PointLatLng const& coord,int const& altitude,const int &position)
{
    WayPointItem* item=new WayPointItem(coord,altitude,map);
    item->SetNumber(position);
    ConnectWP(item);
    item->setParentItem(map);
    emit WPInserted(position,item);
    return item;
}

WayPointItem* OPMapWidget::WPInsert(opmap::PointLatLng const& coord,int const& altitude, QString const& description,const int &position)
{
    WayPointItem* item=new WayPointItem(coord,altitude,description,map);
    item->SetNumber(position);
    ConnectWP(item);
    item->setParentItem(map);
    emit WPInserted(position,item);
    return item;
}

void OPMapWidget::WPDelete(WayPointItem *item)
{
    emit WPDeleted(item->Number());
    delete item;
}

void OPMapWidget::WPDeleteAll()
{
    foreach(QGraphicsItem* i,map->childItems())
    {
        WayPointItem* w=qgraphicsitem_cast<WayPointItem*>(i);
        if(w)
            delete w;
    }
}

QList<WayPointItem*> OPMapWidget::WPSelected()
{
    QList<WayPointItem*> list;

    foreach(QGraphicsItem* i, mscene.selectedItems()) {
        WayPointItem* w=qgraphicsitem_cast<WayPointItem*>(i);
        if(w)
            list.append(w);
    }

    return list;
}

QMap<int, WayPointItem*> OPMapWidget::WPAll()
{
    QMap<int, WayPointItem*> wpMap;

    foreach(QGraphicsItem* i, mscene.items()) {
        WayPointItem* w=qgraphicsitem_cast<WayPointItem*>(i);
        if ( w ) wpMap.insert(w->Number(), w);
    }

    return wpMap;
}

void OPMapWidget::WPRenumber(WayPointItem *item, const int &newnumber)
{
    item->SetNumber(newnumber);
}

void OPMapWidget::ConnectWP(WayPointItem *item)
{
    connect(item,SIGNAL(WPNumberChanged(int,int,WayPointItem*)),this,SIGNAL(WPNumberChanged(int,int,WayPointItem*)));
    connect(item,SIGNAL(WPValuesChanged(WayPointItem*)),this,SIGNAL(WPValuesChanged(WayPointItem*)));
    connect(this,SIGNAL(WPInserted(int,WayPointItem*)),item,SLOT(WPInserted(int,WayPointItem*)));
    connect(this,SIGNAL(WPNumberChanged(int,int,WayPointItem*)),item,SLOT(WPRenumbered(int,int,WayPointItem*)));
    connect(this,SIGNAL(WPDeleted(int)),item,SLOT(WPDeleted(int)));
}

void OPMapWidget::diagRefresh()
{
    if(showDiag) {
        if(diagGraphItem==0) {
            diagGraphItem=new QGraphicsTextItem();
            mscene.addItem(diagGraphItem);
            diagGraphItem->setPos(10,100);
            diagGraphItem->setZValue(3);
            diagGraphItem->setFlag(QGraphicsItem::ItemIsMovable,true);
            diagGraphItem->setDefaultTextColor(Qt::yellow);
        }

        diagGraphItem->setPlainText(core->GetDiagnostics().toString());
    } else {
        if(diagGraphItem!=0) {
            delete diagGraphItem;
            diagGraphItem=0;
        }
    }
}

//////////////////////////////////////////////
void OPMapWidget::SetShowCompass(const bool &value)
{
    if(value && !compass) {
        compass=new QGraphicsSvgItem(QString::fromUtf8(":/markers/images/compas.svg"));
        compass->setScale(0.1+0.05*(qreal)(this->size().width())/1000*(qreal)(this->size().height())/600);
        //    compass->setTransformOriginPoint(compass->boundingRect().width(),compass->boundingRect().height());
        compass->setFlag(QGraphicsItem::ItemIsMovable,true);
        mscene.addItem(compass);
        compass->setTransformOriginPoint(compass->boundingRect().width()/2,compass->boundingRect().height()/2);
        compass->setPos(55-compass->boundingRect().width()/2,55-compass->boundingRect().height()/2);
        compass->setZValue(3);
        compass->setOpacity(0.7);
    }

    if(!value && compass) {
        delete compass;
        compass=0;
    }
}

void OPMapWidget::SetRotate(qreal const& value)
{
    map->mapRotate(value);
    if(compass && (compass->rotation() != value)) {
        compass->setRotation(value);
    }
}

void OPMapWidget::RipMap()
{
    // MapRipper 线程结束自删（deleteLater），连接随对象销毁自动断开，无需持有指针
    MapRipper *ripper = new MapRipper(core, map->SelectedArea());
    connect(ripper, SIGNAL(percentageChanged(int)), this, SIGNAL(mapDownloadProgress(int)));
    connect(ripper, SIGNAL(numberOfTilesChanged(int,int)), this, SIGNAL(mapDownloadTiles(int,int)));
    connect(ripper, SIGNAL(finish()), this, SIGNAL(mapDownloadFinished()));
}


// *************************************************************************************
// 球面几何委托 platform 层 geoutils（保持既有公有 API 签名不变）

// return the bearing from one point to another .. in degrees
double OPMapWidget::bearing(opmap::PointLatLng from, opmap::PointLatLng to)
{
    return geoutils::bearingDeg(from, to);
}

/// 米 → 当前缩放级别下的屏幕像素数（在指定纬度处的地面分辨率换算）
float OPMapWidget::metersToPixels(double meters)
{
    return map->metersToPixels(meters, CurrentPosition());
}

// return a destination lat/lon point given a source lat/lon point and the bearing and distance from the source point
// 注意：本 API 沿用历史语义，dist 单位为千米（geoutils::destPoint 为米，此处换算）
opmap::PointLatLng OPMapWidget::destPoint(opmap::PointLatLng source, double bear, double dist)
{
    return geoutils::destPoint(source, bear, dist * 1000.0);
}

} // end namespace opmap
