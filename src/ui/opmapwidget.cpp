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
#include <QFile>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "waypointitem.h"
#include "geoutils.h"
#include "osrmrouteprovider.h"
#include "iplocationprovider.h"
#include "elevationprovider.h"
#include "positionsource.h"
#include "geofenceitem.h"
#include "navigationengine.h"
#include "waypointmissionengine.h"
#include "routeitem.h"
#include "mapmarkeritem.h"
#include "arclineitem.h"
#include "scalebaritem.h"
#include "measureitem.h"
#include "trailrecorder.h"
#include "uas_types.h"

namespace opmap {

OPMapWidget::OPMapWidget(QWidget *parent, Configuration *config) : QGraphicsView(parent),
    configuration(config),
    UAV(0),
    GPS(0),
    Home(0),
    followmouse(true),
    compass(0),
    scaleBar(0),
    measureItem(0),
    trailRecorder(0),
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
    elevProvider(0),
    geofenceItem(0),
    geofenceBreached(false),
    vehiclePosValid(false),
    lastRealPosValid(false),
    followVehicle(false),
    locatePending(false),
    pickMode(PickNone),
    positionSource(SourceNone),
    routeFromMarker(0),
    routeToMarker(0)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    lastRealPosAge.start();   // 未喂点状态从构造时刻起算（防无效计时器）

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

    // —— 比例尺 / 测距 / 轨迹记录：比例尺默认开，测距折线空态不可见 ——
    measureItem = new MeasureItem(map);
    trailRecorder = new TrailRecorder(this);
    connect(trailRecorder, SIGNAL(replayPosition(opmap::PointLatLng,int,int)),
            this, SLOT(onTrailReplayPosition(opmap::PointLatLng,int,int)));
    connect(trailRecorder, SIGNAL(replayFinished()),
            this, SIGNAL(trailReplayFinished()));
    SetShowScale(true);

    // —— 车载导航：provider → 引擎 → 路线绘制项 ——
    routeProvider = new OsrmRouteProvider(this);
    connect(routeProvider, SIGNAL(alternativesReady(QList<opmap::Route>)),
            this, SLOT(onRouteAlternatives(QList<opmap::Route>)));
    navEngine = new NavigationEngine(this);
    navEngine->SetRouteProvider(routeProvider);
    routeItem = new RouteItem(map);
    routeItem->hide();

    ipLocator = new IpLocationProvider(this);
    // 结果先经 onIpLocated（处理一键定位兜底），再转发 ipLocationReady
    connect(ipLocator, SIGNAL(locationReady(opmap::PointLatLng,QString)),
            this, SLOT(onIpLocated(opmap::PointLatLng,QString)));
    connect(ipLocator, SIGNAL(locationFailed(QString)),
            this, SLOT(onIpLocationFailed(QString)));

    // —— 地面海拔查询：回调式取用后转发 elevationReady/elevationFailed ——
    elevProvider = new ElevationProvider(this);
    connect(elevProvider, SIGNAL(elevationReady(opmap::PointLatLng,int)),
            this, SLOT(onElevationReady(opmap::PointLatLng,int)));
    connect(elevProvider, SIGNAL(elevationFailed(QString)),
            this, SLOT(onElevationFailed(QString)));

    // —— 位置源管理：统一互斥切换；位置点自动喂车 + 分发 positionUpdated ——
    posSourceManager = new PositionSourceManager(this);
    connect(posSourceManager, SIGNAL(positionUpdated(opmap::PointLatLng,double,double,int)),
            this, SLOT(UpdateVehiclePosition(opmap::PointLatLng)));
    connect(posSourceManager, SIGNAL(positionUpdated(opmap::PointLatLng,double,double,int)),
            this, SLOT(onPositionUpdate(opmap::PointLatLng,double,double,int)));
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
            // 文本是背景卡片子项：删卡片（连带文本），不留孤儿矩形
            QGraphicsItem *diagOwner = diagGraphItem->parentItem() ? diagGraphItem->parentItem()
                                                                   : static_cast<QGraphicsItem*>(diagGraphItem);
            delete diagOwner;
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
    ConnectUAV(newUAV);

    return newUAV;
}

void OPMapWidget::AddUAV(int id, UAVItem* uav)
{
    uav->setParentItem(map);
    QGraphicsItemGroup* waypointLine = new QGraphicsItemGroup(map);
    waypointLines.insert(id, waypointLine);
    UAVS.insert(id, uav);
    ConnectUAV(uav);
}

/// UAV 事件 → facade 信号转发（到达航点/飞出安全圈/回到安全圈）。
/// 三条创建路径（AddUAV 两个重载、SetShowUAV）统一走这里，
/// EnsureUAV 惰性建出的 UAV 同样能向客户代码发事件
void OPMapWidget::ConnectUAV(UAVItem *uav)
{
    connect(uav, SIGNAL(UAVReachedWayPoint(int,WayPointItem*)),
            this,  SIGNAL(UAVReachedWayPoint(int,WayPointItem*)));
    connect(uav, SIGNAL(UAVLeftSafetyBouble(opmap::PointLatLng)),
            this,  SIGNAL(UAVLeftSafetyBouble(opmap::PointLatLng)));
    connect(uav, SIGNAL(UAVEnteredSafetyBouble(opmap::PointLatLng)),
            this,  SIGNAL(UAVEnteredSafetyBouble(opmap::PointLatLng)));
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
        ConnectUAV(UAV);
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
    }
}

void OPMapWidget::SetShowGPS(const bool &value)
{
    if(value && GPS==0) {
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

// ————————————————— 通用标记 —————————————————

MapMarkerItem *OPMapWidget::AddMarker(opmap::PointLatLng const& pos, QString const& imagePath)
{
    MapMarkerItem *marker = new MapMarkerItem(pos, map);
    if (!imagePath.isEmpty())
        marker->SetImage(imagePath);
    marker->setParentItem(map);
    markers.append(marker);
    return marker;
}

void OPMapWidget::RemoveMarker(MapMarkerItem *marker)
{
    if (!marker)
        return;
    markers.removeOne(marker);
    delete marker;
}

void OPMapWidget::ClearMarkers()
{
    foreach(MapMarkerItem *m, markers)
        delete m;
    markers.clear();
}

// ————————————————— 贝塞尔弧线航线 —————————————————

ArcLineItem *OPMapWidget::AddArcLine(opmap::PointLatLng const& from, opmap::PointLatLng const& to,
                                     QColor const& color, int side)
{
    ArcLineItem *line = new ArcLineItem(map, from, to, color, side, map);
    arcLines.append(line);
    return line;
}

void OPMapWidget::RemoveArcLine(ArcLineItem *line)
{
    if (!line)
        return;
    arcLines.removeOne(line);
    delete line;
}

void OPMapWidget::ClearArcLines()
{
    foreach(ArcLineItem *l, arcLines)
        delete l;
    arcLines.clear();
}

// ————————————————— 航点文件（JSON，兼容旧 .wp） —————————————————

bool OPMapWidget::WPExportToFile(const QString &path, QString *error)
{
    QMap<int, WayPointItem*> wpMap = WPAll();
    if (wpMap.isEmpty()) {
        if (error)
            *error = QString::fromUtf8("地图上没有航点");
        return false;
    }
    // JSON 任务文件：携带航点全部属性（编号/经纬度/海拔/描述/到达动作/悬停时长），
    // 缩进可读可手改；旧 .wp 列式文本只导出不再支持，导入仍兼容（见 WPImportFromFile 嗅探）
    QJsonArray wps;
    QMap<int, WayPointItem*>::const_iterator it = wpMap.constBegin();
    for (; it != wpMap.constEnd(); ++it) {
        WayPointItem *wp = it.value();
        QJsonObject o;
        o.insert(QLatin1String("number"), it.key());
        o.insert(QLatin1String("lat"), wp->Coord().Lat());
        o.insert(QLatin1String("lng"), wp->Coord().Lng());
        o.insert(QLatin1String("alt"), wp->Altitude());
        o.insert(QLatin1String("description"), wp->Description());
        if (wp->Action() == WayPointItem::WayPointActionPhoto)
            o.insert(QLatin1String("action"), QLatin1String("photo"));
        else if (wp->Action() == WayPointItem::WayPointActionHover)
            o.insert(QLatin1String("action"), QLatin1String("hover"));
        else
            o.insert(QLatin1String("action"), QLatin1String("none"));
        if (wp->HoverTime() > 0)
            o.insert(QLatin1String("hoverTime"), wp->HoverTime());
        wps.append(o);
    }
    QJsonObject root;
    root.insert(QLatin1String("format"), QLatin1String("opmap-waypoints"));
    root.insert(QLatin1String("version"), 1);
    root.insert(QLatin1String("created"), QDateTime::currentDateTime().toString(Qt::ISODate));
    root.insert(QLatin1String("count"), wps.size());
    root.insert(QLatin1String("waypoints"), wps);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = QString::fromUtf8("写入文件失败: %1").arg(path);
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

bool OPMapWidget::WPImportFromFile(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QString::fromUtf8("读取文件失败: %1").arg(path);
        return false;
    }
    QByteArray raw = f.readAll();
    if (raw.startsWith("\xEF\xBB\xBF"))   // 容忍手改时编辑器加的 UTF-8 BOM
        raw.remove(0, 3);

    WPDeleteAll();

    // 格式嗅探：JSON 走新链路（全属性还原）；否则按旧 AP .wp 列式文本走 AP_WPArray
    if (QString::fromUtf8(raw.trimmed().left(1)) == QLatin1String("{")) {
        const QJsonObject root = QJsonDocument::fromJson(raw).object();
        if (root.value(QLatin1String("format")).toString() != QLatin1String("opmap-waypoints")) {
            if (error)
                *error = QString::fromUtf8("不是有效的 opmap-waypoints JSON 文件");
            return false;
        }
        const QJsonArray wps = root.value(QLatin1String("waypoints")).toArray();
        for (int i = 0; i < wps.size(); ++i) {
            const QJsonObject o = wps.at(i).toObject();
            const PointLatLng coord(o.value(QLatin1String("lat")).toDouble(),
                                    o.value(QLatin1String("lng")).toDouble());
            WayPointItem *item = WPCreate(coord,
                                          o.value(QLatin1String("alt")).toDouble());
            item->SetDescription(o.value(QLatin1String("description")).toString());
            const QString action = o.value(QLatin1String("action")).toString();
            if (action == QLatin1String("photo"))
                item->SetAction(WayPointItem::WayPointActionPhoto);
            else if (action == QLatin1String("hover"))
                item->SetAction(WayPointItem::WayPointActionHover);
            const int hoverSec = o.value(QLatin1String("hoverTime")).toInt();
            if (hoverSec > 0)
                item->SetHoverTime(hoverSec);
            item->SetReached(false);
        }
        return true;
    }

    // 旧 .wp：AP_WPArray 列式文本（idx/lat/lng/alt/heading），保留导入兼容
    AP_WPArray arrWP;
    if (arrWP.load(std::string(raw.constData(), raw.size())) != 0) {
        if (error)
            *error = QString::fromUtf8("不是有效的航点文件（JSON 或 .wp）");
        return false;
    }
    AP_WayPointMap *wpMap = arrWP.getAll();
    for (AP_WayPointMap::const_iterator it = wpMap->begin(); it != wpMap->end(); ++it) {
        WayPointItem *item = WPCreate(PointLatLng(it->second->lat, it->second->lng),
                                      (int)it->second->alt);
        item->SetReached(false);
    }
    return true;
}

// ————————————————— 车载导航 —————————————————

void OPMapWidget::SetRouteProvider(opmap::AbstractRouteProvider *provider)
{
    if (!provider || provider == routeProvider)
        return;
    delete routeProvider;          // 接管所有权；在途请求随 QNAM 释放中止
    routeProvider = provider;
    routeProvider->setParent(this);
    connect(routeProvider, SIGNAL(alternativesReady(QList<opmap::Route>)),
            this, SLOT(onRouteAlternatives(QList<opmap::Route>)));
    navEngine->SetRouteProvider(routeProvider);
}

void OPMapWidget::NavigateTo(opmap::PointLatLng const& dest)
{
    const opmap::PointLatLng from = vehiclePosValid ? vehiclePos
                                                    : map->core->CurrentPosition();
    navEngine->NavigateTo(from, dest);
    EnsureRouteMarker(routeToMarker, dest, QString::fromUtf8("目的地"));
}

void OPMapWidget::PlanRoute(opmap::PointLatLng const& from, opmap::PointLatLng const& to)
{
    navEngine->PlanRoute(from, to);
    routeItem->setVisible(true);   // 预览结果经 routePlanned 回来时绘制
    EnsureRouteMarker(routeFromMarker, from, QString::fromUtf8("起点"));
    EnsureRouteMarker(routeToMarker, to, QString::fromUtf8("目的地"));
}

void OPMapWidget::onRouteAlternatives(QList<opmap::Route> routes)
{
    routeAlternatives = routes;    // SelectRoute 取用；第 0 条已随 routePlanned 画出
    emit routeAlternativesReady(routes);
}

void OPMapWidget::SelectRoute(int index)
{
    if (index < 0 || index >= routeAlternatives.size())
        return;
    routeProvider->setPreferredAlternative(index);   // 之后的导航/重规划沿选中的走
    routeItem->SetRoute(routeAlternatives.at(index)); // 预览画布立即切换
    routeItem->setVisible(true);
    emit routeSelected(index, routeAlternatives.at(index));
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
    // 地图中心跟随只归主机：多机场景若僚机也跟随，每次喂点互相拉扯
    // 地图中心（每秒十几次中心跳变），地图乱跳且持续触发瓦片重排风暴
    uav->SetMapFollowType((followVehicle && id == 0) ? UAVMapFollowType::CenterMap
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
    if (trailRecorder->IsRecording(0))
        trailRecorder->AddPoint(0, pos, 0);   // 轨迹记录进主机道（所有位置源统一在此截获）
    navEngine->UpdatePosition(pos);
    CheckGeofence(pos);   // 围栏判定对所有位置源（模拟/GPS/MAVLink）统一生效
    if (missionEngine && missionEngine->IsMissionActive())
        missionEngine->UpdatePosition(pos);   // 航点任务状态推进（喂点驱动）
}

void OPMapWidget::SetUAVPos(int const& id, opmap::PointLatLng const& pos, int const& alt)
{
    UAVItem *uav = EnsureUAV(id);
    uav->SetUAVPos(pos, alt);
    if (trailRecorder->IsRecording(id))
        trailRecorder->AddPoint(id, pos, alt);   // 按机分道记录，多机互不混流
    CheckGeofence(pos);   // 任务飞行喂点同样接入围栏越界判定
    // 任务状态机只认主机（僚机喂点不得误推主机的任务进度）
    if (id == 0 && missionEngine && missionEngine->IsMissionActive())
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
    ClearRouteMarkers();
}

// ———————— 路线端点自动图钉 ————————

/// 惰性取用端点图钉：不存在则创建（auxiliary 装饰航点——不进任务序列、
/// 不可拖选、不显示序号，WPAll/WPDeleteAll 自动跳过），存在则仅移动到新坐标
WayPointItem *OPMapWidget::EnsureRouteMarker(WayPointItem *&marker, opmap::PointLatLng const& pos, QString const& text)
{
    if (!marker) {
        marker = new WayPointItem(pos, 0, text, map);
        --WayPointItem::snumber;   // 图钉不占任务航点号段（构造器消耗的号立即归还）
        marker->SetAuxiliary(true);
        marker->SetShowNumber(false);
        marker->setFlag(QGraphicsItem::ItemIsMovable, false);
        marker->setFlag(QGraphicsItem::ItemIsSelectable, false);
        marker->setParentItem(map);
    } else {
        marker->SetCoord(pos);
    }
    return marker;
}

void OPMapWidget::ClearRouteMarkers()
{
    delete routeFromMarker;   // QGraphicsItem 析构自动从场景摘除
    routeFromMarker = 0;
    delete routeToMarker;
    routeToMarker = 0;
}

void OPMapWidget::RequestIpLocation()
{
    if (ipLocator)
        ipLocator->requestLocation();
}

void OPMapWidget::RequestIpLocation(IpLocationCallback callback)
{
    m_ipCallback = callback;        // 一次性：完成/失败时取出调用并清空
    RequestIpLocation();
}

bool OPMapWidget::IsIpLocationBusy() const
{
    return ipLocator ? ipLocator->isBusy() : false;
}

// ———————— 一键定位（"我的位置"） ————————
// 定位语义：找"我"在哪，而不是找车在哪——只认真实源（系统 GPS/MAVLink）的
// 活位置（10 秒内有喂点）；没有就走城市级 IP 定位兜底。模拟车位置、导航
// 起点摆位一律不参与（车辆跟随时用跟随开关，不占用定位按钮）
//（城市级 IP 结果只落 GPS 图标+居中，绝不喂 vehiclePos——见路线着色缺失教训）

void OPMapWidget::LocateCurrentPosition()
{
    // 真实源活位置：GPS 图标落到该处并居中
    if (lastRealPosValid && lastRealPosAge.elapsed() < 10000) {
        ShowRealLocation(lastRealPos);
        return;
    }
    locatePending = true;           // 结果在 onIpLocated 兜底落图标+居中
    RequestIpLocation();
}

void OPMapWidget::ShowRealLocation(opmap::PointLatLng const& pos)
{
    SetShowGPS(true);
    GPS->SetUAVPos(pos, 0);
    SetCurrentPosition(pos);
    if (ZoomTotal() < 15.0)
        SetZoom(15.0);              // 定位时切到街区级缩放
}

void OPMapWidget::onPositionUpdate(opmap::PointLatLng pos, double altM, double headingDeg, int source)
{
    Q_UNUSED(altM);
    Q_UNUSED(headingDeg);   // 海拔/航向暂不消费（GPS 图标只落平面位置）
    if (source == SourceSystemGps || source == SourceMavlink) {
        lastRealPos = pos;
        lastRealPosValid = true;
        lastRealPosAge.restart();
    }
    if (GPS && source != SourceExternal)   // 图标只跟随真实估计（GPS/MAVLink/IP），模拟喂点不动它
        GPS->SetUAVPos(pos, 0);
}

void OPMapWidget::onIpLocated(opmap::PointLatLng pos, QString city)
{
    if (locatePending) {
        locatePending = false;
        ShowRealLocation(pos);      // 只落图标+居中：城市级位置不喂导航车
    }
    if (m_ipCallback) {             // 回调式通知（与信号并行，不影响 emit）
        IpLocationCallback cb;
        cb.swap(m_ipCallback);
        cb(true, pos, city);
    }
    emit ipLocationReady(pos, city);
}

void OPMapWidget::onIpLocationFailed(QString reason)
{
    locatePending = false;   // 兜底请求已终结，防残留标记误居中后续无关结果
    if (m_ipCallback) {      // 回调式通知（与信号并行，不影响 emit）
        IpLocationCallback cb;
        cb.swap(m_ipCallback);
        cb(false, opmap::PointLatLng(), QString());
    }
    emit ipLocationFailed(reason);
}

// ———————— 地面海拔查询 ————————

void OPMapWidget::onElevationReady(opmap::PointLatLng pos, int altitudeMeters)
{
    if (m_elevCallback) {   // 回调式通知（与信号并行，不影响 emit）
        ElevationCallback cb;
        cb.swap(m_elevCallback);
        cb(true, pos, altitudeMeters);
    }
    emit elevationReady(pos, altitudeMeters);
}

void OPMapWidget::onElevationFailed(QString reason)
{
    if (m_elevCallback) {   // 回调式通知（与信号并行，不影响 emit）
        ElevationCallback cb;
        cb.swap(m_elevCallback);
        cb(false, opmap::PointLatLng(), 0);
    }
    emit elevationFailed(reason);
}

void OPMapWidget::RequestElevation(opmap::PointLatLng const& pos)
{
    if (elevProvider)
        elevProvider->requestElevation(pos);
}

void OPMapWidget::RequestElevation(opmap::PointLatLng const& pos, ElevationCallback callback)
{
    m_elevCallback = callback;      // 一次性：完成/失败时取出调用并清空
    RequestElevation(pos);
}

bool OPMapWidget::IsElevationBusy() const
{
    return elevProvider ? elevProvider->isBusy() : false;
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
    uav->DeleteTrail();   // 任务（重）启动=轨迹从头记录，否则上次飞行终点会与新起飞点拉出跨场连线
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
    RepositionScaleBar();   // 比例尺随窗口缩放保持贴左下角

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

    // 测距橡皮筋预览：末顶点到鼠标的虚线 + 瞬时距离（同围栏模式手感）
    if (pickMode == PickMeasure && measureItem)
        measureItem->SetPreviewPoint(currentmouseposition);

    emit mouseMove(event);
}

void OPMapWidget::mousePressEvent(QMouseEvent *event)
{
    QGraphicsView::mousePressEvent(event);

    // 点选模式（仅左键）：记录按下位置做防抖基线；单发模式（航点/起点/目的地）按下即取点
    if (pickMode != PickNone && event->button() == Qt::LeftButton) {
        pickPressPos = event->pos();
        if (pickMode != PickFence && pickMode != PickPosition && pickMode != PickMeasure)
            HandlePickClick(currentMousePosition());
    }

    emit mousePress(event);
}

void OPMapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    QGraphicsView::mouseReleaseEvent(event);

    // 连续取点模式（围栏/喂位置/测距）左键：抬起位移 <6px 才固化一次选点，拖动地图不算
    if (event->button() == Qt::LeftButton
            && (pickMode == PickFence || pickMode == PickPosition || pickMode == PickMeasure)
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

/// 结束当前取点：围栏按顶点数固化/清理预览，测距段固化，随后发 pickFinished
void OPMapWidget::EndPick()
{
    const PickMode finished = pickMode;
    pickMode = PickNone;
    if (finished == PickFence) {
        if (pickPoints.size() >= 3)
            SetGeofence(pickPoints);                     // 顶点足够：固化为有效围栏
        else
            SetGeofence(QList<opmap::PointLatLng>());    // 不足 3 点：清取点预览残留
    } else if (finished == PickMeasure) {
        // 固化刚结束的测距段（<2 点自动丢弃），发结果信号后再清取点累积
        const double totalM = measureItem ? measureItem->CommitMeasure() : 0.0;
        if (totalM > 0.0)
            emit measureFinished(totalM, pickPoints);
    }
    emit pickFinished((int)finished, pickPoints);
    pickPoints.clear();
}

/// 一次有效选点：入库累积并发信号；单发模式取一次即自动结束
void OPMapWidget::HandlePickClick(opmap::PointLatLng const& pos)
{
    pickPoints.append(pos);
    emit positionPicked((int)pickMode, pos);
    if (pickMode == PickMeasure && measureItem)
        measureItem->AddPoint(pos);
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
        // auxiliary 图钉（路线起终点）由 PlanRoute/NavigateTo/StopNavigation 自管理，
        // 这里删除会造成 facade 悬空指针，必须跳过
        if(w && !w->IsAuxiliary())
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
        if ( w && !w->IsAuxiliary() ) wpMap.insert(w->Number(), w);   // auxiliary 图钉不属任务序列
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
            // 半透明黑底卡片 + 白字：浅色/深色底图上都清晰可读
            QGraphicsRectItem *diagBg = new QGraphicsRectItem();
            mscene.addItem(diagBg);
            diagBg->setPos(10,100);
            diagBg->setZValue(3);
            diagBg->setFlag(QGraphicsItem::ItemIsMovable,true);
            diagBg->setBrush(QColor(0,0,0,150));
            diagBg->setPen(Qt::NoPen);
            diagGraphItem=new QGraphicsTextItem(diagBg);
            diagGraphItem->setDefaultTextColor(Qt::white);
            diagGraphItem->setPos(6,4);
        }

        diagGraphItem->setPlainText(core->GetDiagnostics().toString());
        // 背景卡片随文本尺寸自适应（文本子项位于 (6,4)，四周各留 6/4 边距）
        QGraphicsRectItem *diagBg = qgraphicsitem_cast<QGraphicsRectItem*>(diagGraphItem->parentItem());
        if (diagBg)
            diagBg->setRect(0, 0, diagGraphItem->boundingRect().width() + 12,
                            diagGraphItem->boundingRect().height() + 8);
    } else {
        if(diagGraphItem!=0) {
            // 同 SetShowDiagnostics：删背景卡片（文本父项）连带文本
            QGraphicsItem *diagOwner = diagGraphItem->parentItem() ? diagGraphItem->parentItem()
                                                                   : static_cast<QGraphicsItem*>(diagGraphItem);
            delete diagOwner;
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

// ———————— 比例尺 ————————

void OPMapWidget::SetShowScale(const bool &value)
{
    if (value && !scaleBar) {
        scaleBar = new ScaleBarItem(map);
        mscene.addItem(scaleBar);
        scaleBar->setOpacity(0.85);
        scaleBar->Reposition();
        // 地图失效/缩放/拖动 → 重算标尺（实际换算在 paint 时按当前状态进行）
        connect(map, SIGNAL(mapChanged()), scaleBar, SLOT(RefreshScale()));
        connect(map, SIGNAL(zoomChanged(double,double,double)), scaleBar, SLOT(RefreshScale()));
    } else if (!value && scaleBar) {
        delete scaleBar;
        scaleBar = 0;
    }
}

void OPMapWidget::RepositionScaleBar()
{
    if (scaleBar)
        scaleBar->Reposition();
}

// ———————— 地图测距 ————————

void OPMapWidget::ClearMeasurements()
{
    if (measureItem)
        measureItem->ClearAll();
}

bool OPMapWidget::HasMeasurements() const
{
    return measureItem && measureItem->HasContent();
}

// ———————— 运动轨迹记录与回放 ————————

void OPMapWidget::StartTrailRecording(int uavId)
{
    trailRecorder->StartRecording(uavId);
}

void OPMapWidget::StopTrailRecording(int uavId)
{
    trailRecorder->StopRecording(uavId);
}

bool OPMapWidget::IsTrailRecording(int uavId) const
{
    return trailRecorder->IsRecording(uavId);
}

void OPMapWidget::ClearTrailRecording()
{
    trailRecorder->Clear();
}

int OPMapWidget::TrailPointCount(int uavId) const
{
    return trailRecorder->PointCount(uavId);
}

bool OPMapWidget::SaveTrailToFile(const QString &path, QString *error, int uavId)
{
    return trailRecorder->SaveToFile(path, error, uavId);
}

bool OPMapWidget::LoadTrailFromFile(const QString &path, QString *error, int uavId)
{
    return trailRecorder->LoadFromFile(path, error, uavId);
}

bool OPMapWidget::StartTrailReplay(double speed, int uavId)
{
    const bool ok = trailRecorder->StartReplay(speed, uavId);
    if (ok) {
        // 回放重演=轨迹从头展示：清该机旧轨迹，否则旧轨迹终点会与回放起点
        // （多为起飞点）连出跨场直线（与 StartWaypointMission 启动清轨迹同理）；
        // UAV 尚不存在（仅导入文件未飞过）则无需清理，首帧喂点时才惰性创建
        if (UAVItem *uav = GetUAV(uavId))
            uav->DeleteTrail();
    }
    return ok;
}

void OPMapWidget::StopTrailReplay()
{
    trailRecorder->StopReplay();
}

bool OPMapWidget::IsTrailReplaying() const
{
    return trailRecorder->IsReplaying();
}

void OPMapWidget::onTrailReplayPosition(opmap::PointLatLng pos, int altM, int uavId)
{
    SetUAVPos(uavId, pos, altM);   // 回放=重演对应机：图标/轨迹/围栏全联动
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
