/**
******************************************************************************
*
* @file       mainwindow.cpp
* @brief      示例主窗口：地图浏览、航点管理、路径规划与导航模拟的面板组装
* @see        The GNU Public License (GPL) Version 3
* @{
*
*****************************************************************************/

#include "mainwindow.h"

#include <QtWidgets/QDockWidget>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QGraphicsPathItem>
#include <QtWidgets/QGraphicsEllipseItem>
#include <QtGui/QPen>
#include <QtGui/QBrush>

#include "waypoint_store.h"
#include "route_service.h"
#include "navigation_simulator.h"
#include "uavitem.h"

namespace {

const opmap::PointLatLng kHomePos(34.2609, 108.9424);   ///< 初始位置：西安
const double kHomeZoom = 12.0;

} // anonymous namespace

MainWindow::MainWindow()
    : m_map(new opmap::OPMapWidget(this)),
      m_store(new WaypointStore(m_map)),
      m_routeService(new RouteService(this)),
      m_simulator(new NavigationSimulator(this)),
      m_wpList(new QListWidget(this)),
      m_addWpBtn(new QPushButton(QString::fromUtf8("地图点选添加"), this)),
      m_delWpBtn(new QPushButton(QString::fromUtf8("删除选中"), this)),
      m_originLabel(new QLabel(QString::fromUtf8("起点：未设置"), this)),
      m_destLabel(new QLabel(QString::fromUtf8("终点：未设置"), this)),
      m_providerCombo(new QComboBox(this)),
      m_amapKeyEdit(new QLineEdit(this)),
      m_planBtn(new QPushButton(QString::fromUtf8("规划路线"), this)),
      m_followRouteBtn(new QPushButton(QString::fromUtf8("沿路线导航"), this)),
      m_routeInfo(new QLabel(QString::fromUtf8("尚未规划"), this)),
      m_simStartBtn(new QPushButton(QString::fromUtf8("开始"), this)),
      m_simPauseBtn(new QPushButton(QString::fromUtf8("暂停"), this)),
      m_simStopBtn(new QPushButton(QString::fromUtf8("停止"), this)),
      m_speedCombo(new QComboBox(this)),
      m_followCheck(new QCheckBox(QString::fromUtf8("地图跟随 UAV"), this)),
      m_trailCheck(new QCheckBox(QString::fromUtf8("显示飞行轨迹"), this)),
      m_simInfo(new QLabel(QString::fromUtf8("空闲"), this)),
      m_posLabel(new QLabel(tr("lng: --, lat: --"), this)),
      m_tileLabel(new QLabel(tr("tiles: --"), this)),
      m_pickMode(PickNone),
      m_hasOrigin(false),
      m_hasDest(false),
      m_routeMeters(0),
      m_routeSeconds(0),
      m_routeItem(0),
      m_originMarker(0),
      m_destMarker(0)
{
    setWindowTitle(QString::fromUtf8("opmapcontrol 示例 — 地图/航点/路径规划导航"));
    resize(1200, 800);

    setCentralWidget(m_map);
    m_map->SetMapType(opmap::MapType::AutoNaviRoad);
    m_map->SetCurrentPosition(kHomePos);
    m_map->SetZoom(kHomeZoom);

    // 地图信号 → 本窗口
    connect(m_map, SIGNAL(mousePress(QMouseEvent*)), this, SLOT(onMapMousePress(QMouseEvent*)));
    connect(m_map, SIGNAL(mouseMove(QMouseEvent*)), this, SLOT(onMapMouseMove(QMouseEvent*)));
    connect(m_map, SIGNAL(zoomChanged(double,double,double)), this, SLOT(onZoomChanged(double,double,double)));
    connect(m_map, SIGNAL(OnTilesStillToLoad(int)), this, SLOT(onTilesStill(int)));
    // 地图拖动/缩放/移动后重算路线绘制 item 坐标
    connect(m_map, SIGNAL(OnMapDrag()), this, SLOT(rebuildRouteItems()));
    connect(m_map, SIGNAL(OnMapZoomChanged()), this, SLOT(rebuildRouteItems()));
    connect(m_map, SIGNAL(OnCurrentPositionChanged(opmap::PointLatLng)), this, SLOT(rebuildRouteItems()));

    // 路由服务
    connect(m_routeService, SIGNAL(routeReady(QList<opmap::PointLatLng>,double,int)),
            this, SLOT(onRouteReady(QList<opmap::PointLatLng>,double,int)));
    connect(m_routeService, SIGNAL(routeFailed(QString)), this, SLOT(onRouteFailed(QString)));

    // 模拟器
    connect(m_simulator, SIGNAL(statusUpdated(int,int,QString)),
            this, SLOT(onSimStatus(int,int,QString)));
    connect(m_simulator, SIGNAL(waypointReached(int)), this, SLOT(onWaypointReached(int)));
    connect(m_simulator, SIGNAL(finished()), this, SLOT(onSimFinished()));

    setupMenus();
    setupDocks();
    setupStatusBar();

    m_followCheck->setChecked(true);
    m_trailCheck->setChecked(true);
    setPickMode(PickNone);
}

void MainWindow::setupMenus()
{
    // 地图源菜单
    QMenu *mapMenu = menuBar()->addMenu(QString::fromUtf8("地图(&M)"));
    QActionGroup *group = new QActionGroup(this);
    struct { opmap::MapType::Types type; const char *name; } sources[] = {
        { opmap::MapType::AutoNaviRoad,      "高德路网" },
        { opmap::MapType::AutoNaviSatellite, "高德卫星" },
        { opmap::MapType::OpenStreetMap,     "OpenStreetMap" },
        { opmap::MapType::ArcGIS_Map,        "ArcGIS 地图" },
        { opmap::MapType::GoogleMap,         "Google 地图" },
    };
    for (int i = 0; i < (int)(sizeof(sources) / sizeof(sources[0])); ++i) {
        QAction *act = mapMenu->addAction(QString::fromUtf8(sources[i].name));
        act->setCheckable(true);
        act->setData((int)sources[i].type);
        if (sources[i].type == opmap::MapType::AutoNaviRoad)
            act->setChecked(true);
        group->addAction(act);
        connect(act, SIGNAL(triggered()), this, SLOT(onMapSourceTriggered()));
    }
    mapMenu->addSeparator();
    QAction *ripAct = mapMenu->addAction(QString::fromUtf8("下载框选区域离线瓦片…"));
    connect(ripAct, SIGNAL(triggered()), this, SLOT(onRipMapClicked()));

    // 工具栏（缩放与定位）
    QToolBar *toolBar = addToolBar(QString::fromUtf8("视图"));
    QAction *zoomIn = toolBar->addAction(QString::fromUtf8("放大 +"));
    connect(zoomIn, &QAction::triggered, [this]() { m_map->SetZoom(m_map->ZoomTotal() + 1); });
    QAction *zoomOut = toolBar->addAction(QString::fromUtf8("缩小 −"));
    connect(zoomOut, &QAction::triggered, [this]() { m_map->SetZoom(m_map->ZoomTotal() - 1); });
    QAction *homeAct = toolBar->addAction(QString::fromUtf8("回到初始位置"));
    connect(homeAct, &QAction::triggered, [this]() {
        m_map->SetCurrentPosition(kHomePos);
        m_map->SetZoom(kHomeZoom);
    });
}

void MainWindow::setupDocks()
{
    // —— 航点面板 ——
    QWidget *wpPanel = new QWidget(this);
    QVBoxLayout *wpLayout = new QVBoxLayout(wpPanel);
    QHBoxLayout *wpBtnRow = new QHBoxLayout();
    wpBtnRow->addWidget(m_addWpBtn);
    wpBtnRow->addWidget(m_delWpBtn);
    wpLayout->addLayout(wpBtnRow);
    QPushButton *clearBtn = new QPushButton(QString::fromUtf8("清空全部"), wpPanel);
    QPushButton *importBtn = new QPushButton(QString::fromUtf8("导入 .wp…"), wpPanel);
    QPushButton *exportBtn = new QPushButton(QString::fromUtf8("导出 .wp…"), wpPanel);
    QHBoxLayout *wpBtnRow2 = new QHBoxLayout();
    wpBtnRow2->addWidget(clearBtn);
    wpBtnRow2->addWidget(importBtn);
    wpBtnRow2->addWidget(exportBtn);
    wpLayout->addLayout(wpBtnRow2);
    wpLayout->addWidget(m_wpList);
    connect(m_addWpBtn, SIGNAL(clicked()), this, SLOT(onAddWaypointClicked()));
    connect(m_delWpBtn, SIGNAL(clicked()), this, SLOT(onDeleteWaypointClicked()));
    connect(clearBtn, SIGNAL(clicked()), this, SLOT(onClearWaypointsClicked()));
    connect(importBtn, SIGNAL(clicked()), this, SLOT(onImportWaypointsClicked()));
    connect(exportBtn, SIGNAL(clicked()), this, SLOT(onExportWaypointsClicked()));
    connect(m_wpList, SIGNAL(itemClicked(QListWidgetItem*)),
            this, SLOT(onWaypointListItemClicked(QListWidgetItem*)));

    // —— 路径规划面板 ——
    QWidget *routePanel = new QWidget(this);
    QVBoxLayout *routeLayout = new QVBoxLayout(routePanel);
    QPushButton *setOriginBtn = new QPushButton(QString::fromUtf8("① 在地图上点选起点"), routePanel);
    QPushButton *setDestBtn = new QPushButton(QString::fromUtf8("② 在地图上点选终点"), routePanel);
    connect(setOriginBtn, SIGNAL(clicked()), this, SLOT(onSetOriginClicked()));
    connect(setDestBtn, SIGNAL(clicked()), this, SLOT(onSetDestClicked()));
    routeLayout->addWidget(setOriginBtn);
    routeLayout->addWidget(setDestBtn);
    routeLayout->addWidget(m_originLabel);
    routeLayout->addWidget(m_destLabel);
    QHBoxLayout *providerRow = new QHBoxLayout();
    providerRow->addWidget(new QLabel(QString::fromUtf8("服务："), routePanel));
    m_providerCombo->addItem(QString::fromUtf8("OSRM（免 key）"));
    m_providerCombo->addItem(QString::fromUtf8("高德（需 key）"));
    providerRow->addWidget(m_providerCombo);
    routeLayout->addLayout(providerRow);
    m_amapKeyEdit->setPlaceholderText(QString::fromUtf8("高德 Web 服务 key（选高德时填写）"));
    routeLayout->addWidget(m_amapKeyEdit);
    routeLayout->addWidget(m_planBtn);
    routeLayout->addWidget(m_routeInfo);
    m_followRouteBtn->setEnabled(false);
    routeLayout->addWidget(m_followRouteBtn);
    connect(m_planBtn, SIGNAL(clicked()), this, SLOT(onPlanClicked()));
    connect(m_followRouteBtn, SIGNAL(clicked()), this, SLOT(onFollowRouteClicked()));

    // —— 导航模拟面板 ——
    QWidget *simPanel = new QWidget(this);
    QVBoxLayout *simLayout = new QVBoxLayout(simPanel);
    QHBoxLayout *simBtnRow = new QHBoxLayout();
    simBtnRow->addWidget(m_simStartBtn);
    simBtnRow->addWidget(m_simPauseBtn);
    simBtnRow->addWidget(m_simStopBtn);
    simLayout->addLayout(simBtnRow);
    QHBoxLayout *speedRow = new QHBoxLayout();
    speedRow->addWidget(new QLabel(QString::fromUtf8("速度："), simPanel));
    m_speedCombo->addItem(QString::fromUtf8("慢 5 m/s"));
    m_speedCombo->addItem(QString::fromUtf8("中 20 m/s"));
    m_speedCombo->addItem(QString::fromUtf8("快 60 m/s"));
    m_speedCombo->setCurrentIndex(1);
    speedRow->addWidget(m_speedCombo);
    simLayout->addLayout(speedRow);
    simLayout->addWidget(m_followCheck);
    simLayout->addWidget(m_trailCheck);
    simLayout->addWidget(m_simInfo);
    simLayout->addStretch(1);
    connect(m_simStartBtn, SIGNAL(clicked()), this, SLOT(onSimStartClicked()));
    connect(m_simPauseBtn, SIGNAL(clicked()), this, SLOT(onSimPauseClicked()));
    connect(m_simStopBtn, SIGNAL(clicked()), this, SLOT(onSimStopClicked()));
    connect(m_speedCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onSpeedChanged(int)));
    connect(m_followCheck, SIGNAL(toggled(bool)), this, SLOT(onFollowToggled(bool)));
    connect(m_trailCheck, SIGNAL(toggled(bool)), this, SLOT(onTrailToggled(bool)));

    QDockWidget *wpDock = new QDockWidget(QString::fromUtf8("航点"), this);
    wpDock->setWidget(wpPanel);
    QDockWidget *routeDock = new QDockWidget(QString::fromUtf8("路径规划"), this);
    routeDock->setWidget(routePanel);
    QDockWidget *simDock = new QDockWidget(QString::fromUtf8("导航模拟"), this);
    simDock->setWidget(simPanel);
    addDockWidget(Qt::RightDockWidgetArea, wpDock);
    addDockWidget(Qt::RightDockWidgetArea, routeDock);
    addDockWidget(Qt::RightDockWidgetArea, simDock);
    tabifyDockWidget(wpDock, routeDock);
    tabifyDockWidget(routeDock, simDock);
    wpDock->raise();
}

void MainWindow::setupStatusBar()
{
    statusBar()->addWidget(m_posLabel);
    statusBar()->addPermanentWidget(m_tileLabel);
}

// ————————————————— 地图交互 —————————————————

void MainWindow::onMapSourceTriggered()
{
    QAction *act = qobject_cast<QAction*>(sender());
    if (!act)
        return;
    m_map->SetMapType((opmap::MapType::Types)act->data().toInt());
}

void MainWindow::onMapMousePress(QMouseEvent *)
{
    if (m_pickMode == PickNone)
        return;
    applyPickPoint(m_map->currentMousePosition());
}

void MainWindow::onMapMouseMove(QMouseEvent *)
{
    const opmap::PointLatLng p = m_map->currentMousePosition();
    m_posLabel->setText(QString::fromUtf8("lng: %1, lat: %2 (WGS-84)")
                        .arg(p.Lng(), 0, 'f', 6).arg(p.Lat(), 0, 'f', 6));
}

void MainWindow::onZoomChanged(double, double, double)
{
    m_tileLabel->setText(QString::fromUtf8("zoom: %1").arg(m_map->ZoomTotal(), 0, 'f', 1));
}

void MainWindow::onTilesStill(int number)
{
    m_tileLabel->setText(QString::fromUtf8("tiles: %1").arg(number));
}

// ————————————————— 航点面板 —————————————————

void MainWindow::onAddWaypointClicked()
{
    setPickMode(PickWaypoint);
    statusBar()->showMessage(QString::fromUtf8("在地图上点击以放置航点"), 5000);
}

void MainWindow::onDeleteWaypointClicked()
{
    QListWidgetItem *cur = m_wpList->currentItem();
    if (!cur)
        return;
    opmap::WayPointItem *wp = reinterpret_cast<opmap::WayPointItem*>(
                static_cast<quintptr>(cur->data(Qt::UserRole).toULongLong()));
    if (wp)
        m_map->WPDelete(wp);
    refreshWaypointList();
}

void MainWindow::onClearWaypointsClicked()
{
    m_map->WPDeleteAll();
    refreshWaypointList();
}

void MainWindow::onImportWaypointsClicked()
{
    const QString path = QFileDialog::getOpenFileName(this, QString::fromUtf8("导入航点"),
                                                      QString(), QString::fromUtf8("航点文件 (*.wp)"));
    if (path.isEmpty())
        return;
    if (!m_store->load(path)) {
        QMessageBox::warning(this, QString::fromUtf8("导入失败"), m_store->error());
        return;
    }
    refreshWaypointList();
}

void MainWindow::onExportWaypointsClicked()
{
    const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("导出航点"),
                                                      QString(), QString::fromUtf8("航点文件 (*.wp)"));
    if (path.isEmpty())
        return;
    if (!m_store->save(path)) {
        QMessageBox::warning(this, QString::fromUtf8("导出失败"), m_store->error());
        return;
    }
    statusBar()->showMessage(QString::fromUtf8("已导出 %1").arg(path), 5000);
}

void MainWindow::onWaypointListItemClicked(QListWidgetItem *item)
{
    const double lat = item->data(Qt::UserRole + 1).toDouble();
    const double lng = item->data(Qt::UserRole + 2).toDouble();
    m_map->SetCurrentPosition(opmap::PointLatLng(lat, lng));
}

void MainWindow::refreshWaypointList()
{
    m_wpList->clear();
    const QMap<int, opmap::WayPointItem*> wpMap = m_map->WPAll();
    QMap<int, opmap::WayPointItem*>::const_iterator it = wpMap.constBegin();
    for (; it != wpMap.constEnd(); ++it) {
        const opmap::PointLatLng c = it.value()->Coord();
        QListWidgetItem *item = new QListWidgetItem(
                QString::fromUtf8("WP%1  lat:%2  lng:%3  alt:%4m")
                .arg(it.key()).arg(c.Lat(), 0, 'f', 5)
                .arg(c.Lng(), 0, 'f', 5).arg(it.value()->Altitude(), 0, 'f', 0),
                m_wpList);
        item->setData(Qt::UserRole, (quintptr)it.value());
        item->setData(Qt::UserRole + 1, c.Lat());
        item->setData(Qt::UserRole + 2, c.Lng());
    }
}

// ————————————————— 路径规划面板 —————————————————

void MainWindow::onSetOriginClicked()
{
    setPickMode(PickOrigin);
    statusBar()->showMessage(QString::fromUtf8("在地图上点击设置起点"), 5000);
}

void MainWindow::onSetDestClicked()
{
    setPickMode(PickDest);
    statusBar()->showMessage(QString::fromUtf8("在地图上点击设置终点"), 5000);
}

void MainWindow::onPlanClicked()
{
    if (!m_hasOrigin || !m_hasDest) {
        QMessageBox::information(this, QString::fromUtf8("提示"), QString::fromUtf8("请先在地图上点选起点与终点"));
        return;
    }
    const RouteService::Provider provider = (m_providerCombo->currentIndex() == 0)
            ? RouteService::Osrm : RouteService::Amap;
    m_planBtn->setEnabled(false);
    m_routeInfo->setText(QString::fromUtf8("规划中…"));
    statusBar()->showMessage(QString::fromUtf8("正在请求路径规划（需联网）…"));
    m_routeService->planRoute(m_origin, m_dest, provider, m_amapKeyEdit->text().trimmed());
}

void MainWindow::onFollowRouteClicked()
{
    if (m_routePts.size() < 2)
        return;
    m_simulator->stop();
    m_simulator->setUAV(ensureUAV());
    m_simulator->setPathRoute(m_routePts, 100);
    m_simulator->setSpeed(m_speedCombo->currentIndex() == 0 ? 5.0
                          : m_speedCombo->currentIndex() == 1 ? 20.0 : 60.0);
    m_simulator->start();
}

void MainWindow::onRouteReady(const QList<opmap::PointLatLng> &pts, double meters, int seconds)
{
    m_routePts = pts;
    m_routeMeters = meters;
    m_routeSeconds = seconds;
    m_planBtn->setEnabled(true);
    m_followRouteBtn->setEnabled(true);
    m_routeInfo->setText(QString::fromUtf8("距离 %1 km，预计 %2 分钟（%3 点）")
                         .arg(meters / 1000.0, 0, 'f', 1)
                         .arg(seconds / 60).arg(pts.size()));
    statusBar()->showMessage(QString::fromUtf8("路径规划完成"), 5000);
    rebuildRouteItems();
}

void MainWindow::onRouteFailed(const QString &reason)
{
    m_planBtn->setEnabled(true);
    m_routeInfo->setText(QString::fromUtf8("规划失败"));
    statusBar()->showMessage(QString::fromUtf8("路径规划失败: %1").arg(reason), 8000);
}

// ————————————————— 导航模拟面板 —————————————————

void MainWindow::onSimStartClicked()
{
    const QMap<int, opmap::WayPointItem*> wpMap = m_map->WPAll();
    if (wpMap.isEmpty()) {
        QMessageBox::information(this, QString::fromUtf8("提示"),
                                 QString::fromUtf8("请先在地图上添加航点（航点面板→地图点选添加），或使用路径规划后沿路线导航"));
        return;
    }
    if (m_pickMode != PickNone)
        setPickMode(PickNone);

    m_simulator->stop();
    m_simulator->setUAV(ensureUAV());
    m_simulator->setWaypointRoute(wpMap.values());
    m_simulator->setSpeed(m_speedCombo->currentIndex() == 0 ? 5.0
                          : m_speedCombo->currentIndex() == 1 ? 20.0 : 60.0);
    m_simulator->start();
}

void MainWindow::onSimPauseClicked()
{
    m_simulator->pause();
}

void MainWindow::onSimStopClicked()
{
    m_simulator->stop();
}

void MainWindow::onSpeedChanged(int index)
{
    m_simulator->setSpeed(index == 0 ? 5.0 : index == 1 ? 20.0 : 60.0);
}

void MainWindow::onFollowToggled(bool on)
{
    opmap::UAVItem *uav = ensureUAV();
    uav->SetMapFollowType(on ? opmap::UAVMapFollowType::CenterMap
                             : opmap::UAVMapFollowType::None);
}

void MainWindow::onTrailToggled(bool on)
{
    opmap::UAVItem *uav = ensureUAV();
    uav->SetShowTrail(on);
    uav->SetTrailType(on ? opmap::UAVTrailType::ByTimeElapsed
                         : opmap::UAVTrailType::NoTrail);
    if (on)
        uav->SetTrailTime(3);
}

void MainWindow::onSimStatus(int current, int total, const QString &message)
{
    m_simInfo->setText(QString::fromUtf8("%1（目标 %2/%3）").arg(message).arg(current + 1).arg(total));
}

void MainWindow::onWaypointReached(int index)
{
    statusBar()->showMessage(QString::fromUtf8("已到达航点 %1").arg(index + 1), 5000);
    refreshWaypointList();
}

void MainWindow::onSimFinished()
{
    QMessageBox::information(this, QString::fromUtf8("导航模拟"),
                             QString::fromUtf8("全部目标飞行完成"));
    refreshWaypointList();
}

// ————————————————— 离线下载 —————————————————

void MainWindow::onRipMapClicked()
{
    if (m_map->SelectedArea().IsEmpty()) {
        QMessageBox::information(this, QString::fromUtf8("提示"),
                                 QString::fromUtf8("请先按住 Ctrl 拖动鼠标在地图上框选下载区域"));
        return;
    }
    m_map->RipMap();
    statusBar()->showMessage(QString::fromUtf8("离线瓦片下载已启动"), 5000);
}

// ————————————————— 内部工具 —————————————————

void MainWindow::setPickMode(PickMode mode)
{
    m_pickMode = mode;
    m_addWpBtn->setEnabled(mode != PickWaypoint);
}

void MainWindow::applyPickPoint(const opmap::PointLatLng &p)
{
    switch (m_pickMode) {
    case PickWaypoint:
    {
        opmap::WayPointItem *wp = m_map->WPCreate(p, 100);
        wp->SetDescription(QString::fromUtf8("WP%1").arg(m_map->WPAll().size()));
        refreshWaypointList();
        break;
    }
    case PickOrigin:
        m_origin = p;
        m_hasOrigin = true;
        m_originLabel->setText(QString::fromUtf8("起点：lat %1, lng %2")
                               .arg(p.Lat(), 0, 'f', 5).arg(p.Lng(), 0, 'f', 5));
        break;
    case PickDest:
        m_dest = p;
        m_hasDest = true;
        m_destLabel->setText(QString::fromUtf8("终点：lat %1, lng %2")
                             .arg(p.Lat(), 0, 'f', 5).arg(p.Lng(), 0, 'f', 5));
        break;
    default:
        break;
    }
    rebuildRouteItems();
    setPickMode(PickNone);
}

void MainWindow::clearRouteItems()
{
    delete m_routeItem;
    delete m_originMarker;
    delete m_destMarker;
    m_routeItem = 0;
    m_originMarker = 0;
    m_destMarker = 0;
}

void MainWindow::rebuildRouteItems()
{
    clearRouteItems();
    if (!m_routePts.isEmpty()) {
        QPainterPath path;
        path.moveTo(m_map->GetFromLatLngToLocal(m_routePts.first()));
        for (int i = 1; i < m_routePts.size(); ++i)
            path.lineTo(m_map->GetFromLatLngToLocal(m_routePts.at(i)));
        m_routeItem = m_map->scene()->addPath(path, QPen(QColor(0, 120, 255), 3));
        m_routeItem->setZValue(1);
    }
    if (m_hasOrigin) {
        const QPointF local = m_map->GetFromLatLngToLocal(m_origin);
        m_originMarker = m_map->scene()->addEllipse(local.x() - 6, local.y() - 6, 12, 12,
                                                    QPen(Qt::green, 2), QBrush(QColor(0, 180, 0, 120)));
        m_originMarker->setZValue(2);
    }
    if (m_hasDest) {
        const QPointF local = m_map->GetFromLatLngToLocal(m_dest);
        m_destMarker = m_map->scene()->addEllipse(local.x() - 6, local.y() - 6, 12, 12,
                                                  QPen(Qt::red, 2), QBrush(QColor(220, 0, 0, 120)));
        m_destMarker->setZValue(2);
    }
}

opmap::UAVItem* MainWindow::ensureUAV()
{
    opmap::UAVItem *uav = m_map->GetUAV(0);
    if (!uav) {
        uav = m_map->AddUAV(0);
        m_map->SetShowUAV(true);
        uav->SetTrailType(opmap::UAVTrailType::ByTimeElapsed);
        uav->SetTrailTime(3);
        uav->SetShowTrail(m_trailCheck->isChecked());
        uav->SetMapFollowType(m_followCheck->isChecked()
                              ? opmap::UAVMapFollowType::CenterMap
                              : opmap::UAVMapFollowType::None);
    }
    return uav;
}
