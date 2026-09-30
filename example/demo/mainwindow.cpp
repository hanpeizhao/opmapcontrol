/**
******************************************************************************
*
* @file       mainwindow.cpp
* @brief      示例主窗口：地图浏览、航点管理与车载导航（库能力）的面板组装
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
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#include <QtCore/QTimer>
#include <QtGui/QResizeEvent>
#include <QtPositioning/QGeoPositionInfoSource>
#include <QtPositioning/QGeoPositionInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

#include "waypoint_store.h"
#include "navigation_simulator.h"
#include "uavitem.h"

namespace {

const opmap::PointLatLng kHomePos(34.2609, 108.9424);   ///< 初始位置：西安
const double kHomeZoom = 12.0;

const char *kBannerNormal   = "rgba(20,20,20,220)";    ///< 指令横幅常态底色
const char *kBannerOffRoute = "rgba(170,120,0,220)";   ///< 偏航警示底色（黄）
const char *kBannerArrived  = "rgba(0,110,40,220)";    ///< 到达提示底色（绿）

} // anonymous namespace

MainWindow::MainWindow()
    : m_map(new opmap::OPMapWidget(this)),
      m_store(new WaypointStore(m_map)),
      m_simulator(new NavigationSimulator(this)),
      m_wpList(new QListWidget(this)),
      m_addWpBtn(new QPushButton(QString::fromUtf8("地图点选添加"), this)),
      m_delWpBtn(new QPushButton(QString::fromUtf8("删除选中"), this)),
      m_originLabel(new QLabel(QString::fromUtf8("起点：未设置（导航缺省用当前位置）"), this)),
      m_destLabel(new QLabel(QString::fromUtf8("目的地：未设置"), this)),
      m_providerCombo(new QComboBox(this)),
      m_amapKeyEdit(new QLineEdit(this)),
      m_planBtn(new QPushButton(QString::fromUtf8("规划路线"), this)),
      m_navBtn(new QPushButton(QString::fromUtf8("开始导航"), this)),
      m_stopNavBtn(new QPushButton(QString::fromUtf8("停止导航"), this)),
      m_navInfo(new QLabel(QString::fromUtf8("空闲"), this)),
      m_simStartBtn(new QPushButton(QString::fromUtf8("开始跟车"), this)),
      m_simPauseBtn(new QPushButton(QString::fromUtf8("暂停"), this)),
      m_simStopBtn(new QPushButton(QString::fromUtf8("停止"), this)),
      m_yawBtn(new QPushButton(QString::fromUtf8("模拟偏航"), this)),
      m_speedCombo(new QComboBox(this)),
      m_posSourceCombo(new QComboBox(this)),
      m_gpsSource(0),
      m_followCheck(new QCheckBox(QString::fromUtf8("地图跟随车辆"), this)),
      m_trailCheck(new QCheckBox(QString::fromUtf8("显示行车轨迹"), this)),
      m_simInfo(new QLabel(QString::fromUtf8("空闲"), this)),
      m_banner(new QLabel(m_map)),
      m_posLabel(new QLabel(tr("lng: --, lat: --"), this)),
      m_tileLabel(new QLabel(tr("tiles: --"), this)),
      m_pickMode(PickNone),
      m_origin(0, 0),
      m_dest(0, 0),
      m_hasOrigin(false),
      m_hasDest(false),
      m_ipTimer(new QTimer(this)),
      m_ipNam(new QNetworkAccessManager(this)),
      m_providerIsAmap(false)
{
    setWindowTitle(QString::fromUtf8("opmapcontrol 示例 — 地图/航点/车载导航"));
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

    // 库导航信号 → 面板/横幅
    connect(m_map, SIGNAL(navigationRouteReady(opmap::Route)), this, SLOT(onNavigationRouteReady(opmap::Route)));
    connect(m_map, SIGNAL(navigationProgress(double,int,QString)), this, SLOT(onNavProgress(double,int,QString)));
    connect(m_map, SIGNAL(offRouteDetected(opmap::PointLatLng,double)),
            this, SLOT(onOffRouteDetected(opmap::PointLatLng,double)));
    connect(m_map, SIGNAL(rerouteReady(opmap::Route)), this, SLOT(onRerouteReady(opmap::Route)));
    connect(m_map, SIGNAL(navigationArrived()), this, SLOT(onNavigationArrived()));
    connect(m_map, SIGNAL(navigationFailed(QString)), this, SLOT(onNavigationFailed(QString)));

    // 跟车模拟 → 喂库（库内同步 UAV 图标并驱动导航引擎）
    connect(m_simulator, SIGNAL(positionChanged(opmap::PointLatLng,double)),
            m_map, SLOT(UpdateVehiclePosition(opmap::PointLatLng)));
    connect(m_simulator, SIGNAL(statusUpdated(int,int,QString)),
            this, SLOT(onSimStatus(int,int,QString)));
    connect(m_simulator, SIGNAL(finished()), this, SLOT(onSimFinished()));

    setupMenus();
    setupDocks();
    setupStatusBar();

    m_followCheck->setChecked(true);
    m_trailCheck->setChecked(true);
    setPickMode(PickNone);

    // 指令横幅：地图底部叠加，默认隐藏
    m_banner->setTextFormat(Qt::RichText);
    m_banner->setAlignment(Qt::AlignCenter);
    m_banner->hide();
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
    QAction *locateAct = toolBar->addAction(QString::fromUtf8("定位当前位置"));
    connect(locateAct, &QAction::triggered, [this]() {
        if (!m_map->HasVehiclePosition()) {
            QMessageBox::information(this, QString::fromUtf8("尚无车辆位置"),
                    QString::fromUtf8("还没有任何位置源喂入车辆位置（桌面 PC 默认无 GPS），可：\n\n"
                                      "· 点开始导航后，用行车模拟喂点\n"
                                      "· 行车模拟面板把位置源切到系统 GPS"));
            return;
        }
        m_map->SetCurrentPosition(m_map->VehiclePosition());
        if (m_map->ZoomTotal() < 15.0)
            m_map->SetZoom(15.0);       // 定位时切到街区级缩放
        statusBar()->showMessage(QString::fromUtf8("已定位到车辆当前位置"), 3000);
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

    // —— 车载导航面板 ——
    QWidget *navPanel = new QWidget(this);
    QVBoxLayout *navLayout = new QVBoxLayout(navPanel);
    QHBoxLayout *pickBtnRow = new QHBoxLayout();
    QPushButton *pickOriginBtn = new QPushButton(QString::fromUtf8("①点选起点"), navPanel);
    QPushButton *pickDestBtn = new QPushButton(QString::fromUtf8("②点选目的地"), navPanel);
    connect(pickOriginBtn, SIGNAL(clicked()), this, SLOT(onPickOriginClicked()));
    connect(pickDestBtn, SIGNAL(clicked()), this, SLOT(onPickDestClicked()));
    pickBtnRow->addWidget(pickOriginBtn);
    pickBtnRow->addWidget(pickDestBtn);
    navLayout->addLayout(pickBtnRow);
    navLayout->addWidget(m_originLabel);
    navLayout->addWidget(m_destLabel);
    QHBoxLayout *providerRow = new QHBoxLayout();
    providerRow->addWidget(new QLabel(QString::fromUtf8("服务："), navPanel));
    m_providerCombo->addItem(QString::fromUtf8("OSRM（免 key）"));
    m_providerCombo->addItem(QString::fromUtf8("高德（需 key）"));
    providerRow->addWidget(m_providerCombo);
    navLayout->addLayout(providerRow);
    m_amapKeyEdit->setPlaceholderText(QString::fromUtf8("高德 Web 服务 key（选高德时填写）"));
    navLayout->addWidget(m_amapKeyEdit);
    QHBoxLayout *navBtnRow = new QHBoxLayout();
    navBtnRow->addWidget(m_planBtn);
    navBtnRow->addWidget(m_navBtn);
    navBtnRow->addWidget(m_stopNavBtn);
    navLayout->addLayout(navBtnRow);
    navLayout->addWidget(m_navInfo);
    navLayout->addStretch(1);
    connect(m_planBtn, SIGNAL(clicked()), this, SLOT(onPlanClicked()));
    connect(m_navBtn, SIGNAL(clicked()), this, SLOT(onNavigateClicked()));
    connect(m_stopNavBtn, SIGNAL(clicked()), this, SLOT(onStopNavClicked()));
    connect(m_providerCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onProviderChanged(int)));

    // —— 行车模拟面板 ——
    QWidget *simPanel = new QWidget(this);
    QVBoxLayout *simLayout = new QVBoxLayout(simPanel);
    QHBoxLayout *simBtnRow = new QHBoxLayout();
    simBtnRow->addWidget(m_simStartBtn);
    simBtnRow->addWidget(m_simPauseBtn);
    simBtnRow->addWidget(m_simStopBtn);
    simLayout->addLayout(simBtnRow);
    QHBoxLayout *posSourceRow = new QHBoxLayout();
    posSourceRow->addWidget(new QLabel(QString::fromUtf8("位置源："), simPanel));
    m_posSourceCombo->addItem(QString::fromUtf8("行车模拟"));
    m_posSourceCombo->addItem(QString::fromUtf8("系统 GPS"));
    m_posSourceCombo->addItem(QString::fromUtf8("IP 定位（城市级）"));
    posSourceRow->addWidget(m_posSourceCombo);
    simLayout->addLayout(posSourceRow);
    QHBoxLayout *speedRow = new QHBoxLayout();
    speedRow->addWidget(new QLabel(QString::fromUtf8("速度："), simPanel));
    m_speedCombo->addItem(QString::fromUtf8("慢 5 m/s"));
    m_speedCombo->addItem(QString::fromUtf8("中 20 m/s"));
    m_speedCombo->addItem(QString::fromUtf8("快 60 m/s"));
    m_speedCombo->setCurrentIndex(1);
    speedRow->addWidget(m_speedCombo);
    simLayout->addLayout(speedRow);
    simLayout->addWidget(m_yawBtn);
    simLayout->addWidget(m_followCheck);
    simLayout->addWidget(m_trailCheck);
    simLayout->addWidget(m_simInfo);
    simLayout->addStretch(1);
    connect(m_simStartBtn, SIGNAL(clicked()), this, SLOT(onSimStartClicked()));
    connect(m_simPauseBtn, SIGNAL(clicked()), this, SLOT(onSimPauseClicked()));
    connect(m_simStopBtn, SIGNAL(clicked()), this, SLOT(onSimStopClicked()));
    connect(m_yawBtn, SIGNAL(clicked()), this, SLOT(onYawClicked()));
    connect(m_speedCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onSpeedChanged(int)));
    connect(m_posSourceCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onPosSourceChanged(int)));
    connect(m_ipTimer, SIGNAL(timeout()), this, SLOT(onIpFetchTimeout()));
    connect(m_ipNam, SIGNAL(finished(QNetworkReply*)), this, SLOT(onIpReplyFinished()));
    connect(m_followCheck, SIGNAL(toggled(bool)), this, SLOT(onFollowToggled(bool)));
    connect(m_trailCheck, SIGNAL(toggled(bool)), this, SLOT(onTrailToggled(bool)));

    QDockWidget *wpDock = new QDockWidget(QString::fromUtf8("航点"), this);
    wpDock->setWidget(wpPanel);
    QDockWidget *navDock = new QDockWidget(QString::fromUtf8("车载导航"), this);
    navDock->setWidget(navPanel);
    QDockWidget *simDock = new QDockWidget(QString::fromUtf8("行车模拟"), this);
    simDock->setWidget(simPanel);
    addDockWidget(Qt::RightDockWidgetArea, wpDock);
    addDockWidget(Qt::RightDockWidgetArea, navDock);
    addDockWidget(Qt::RightDockWidgetArea, simDock);
    tabifyDockWidget(wpDock, navDock);
    tabifyDockWidget(navDock, simDock);
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

// ————————————————— 车载导航面板 —————————————————

void MainWindow::onPickOriginClicked()
{
    setPickMode(PickOrigin);
    statusBar()->showMessage(QString::fromUtf8("在地图上点击设置起点"), 5000);
}

void MainWindow::onPickDestClicked()
{
    setPickMode(PickDest);
    statusBar()->showMessage(QString::fromUtf8("在地图上点击设置目的地"), 5000);
}

void MainWindow::onPlanClicked()
{
    if (!m_hasOrigin || !m_hasDest) {
        QMessageBox::information(this, QString::fromUtf8("提示"),
                                 QString::fromUtf8("请先点选起点和目的地，再规划路线"));
        return;
    }
    if (m_map->IsNavigating()) {
        QMessageBox::information(this, QString::fromUtf8("提示"),
                                 QString::fromUtf8("正在导航中，请先停止导航再规划预览"));
        return;
    }
    if (m_pickMode != PickNone)
        setPickMode(PickNone);
    applyProviderFromUI();
    m_navInfo->setText(QString::fromUtf8("规划中…"));
    m_map->PlanRoute(m_origin, m_dest);
}

void MainWindow::onNavigateClicked()
{
    if (!m_hasDest) {
        QMessageBox::information(this, QString::fromUtf8("提示"),
                                 QString::fromUtf8("请先在地图上点选目的地"));
        return;
    }
    if (m_pickMode != PickNone)
        setPickMode(PickNone);
    applyProviderFromUI();
    if (m_hasOrigin) {
        // 指定点选起点出发：先把车辆位置喂到起点（图标同步 + 作为规划起点）
        m_map->UpdateVehiclePosition(m_origin);
    }
    m_navInfo->setText(QString::fromUtf8("规划中…"));
    m_banner->show();
    setBanner(QString::fromUtf8("正在规划路线…"), QString(), kBannerNormal);
    m_map->NavigateTo(m_dest);
}

void MainWindow::onStopNavClicked()
{
    m_simulator->stop();
    m_map->StopNavigation();
    m_banner->hide();
    m_navInfo->setText(QString::fromUtf8("已停止"));
    statusBar()->showMessage(QString::fromUtf8("导航已停止"), 5000);
}

void MainWindow::onProviderChanged(int index)
{
    Q_UNUSED(index);
    applyProviderFromUI();
}

void MainWindow::applyProviderFromUI()
{
    const bool wantAmap = (m_providerCombo->currentIndex() == 1);
    // OSRM→OSRM 无需重建；其余情况重建（高德需每次刷新 key）
    if (!wantAmap && !m_providerIsAmap)
        return;
    if (wantAmap) {
        opmap::AmapRouteProvider *p = new opmap::AmapRouteProvider();
        p->SetKey(m_amapKeyEdit->text().trimmed());
        m_map->SetRouteProvider(p);
    } else {
        m_map->SetRouteProvider(new opmap::OsrmRouteProvider());
    }
    m_providerIsAmap = wantAmap;
}

void MainWindow::onNavigationRouteReady(const opmap::Route &route)
{
    m_navRoute = route;
    m_navInfo->setText(QString::fromUtf8("距离 %1 km，预计 %2 分钟（%3 点）")
                       .arg(route.totalDistanceMeters / 1000.0, 0, 'f', 1)
                       .arg(route.totalDurationSeconds / 60).arg(route.polyline.size()));
    statusBar()->showMessage(QString::fromUtf8("导航开始"), 5000);
}

void MainWindow::onNavProgress(double remainingMeters, int remainingSeconds, const QString &instruction)
{
    const QString timeText = remainingSeconds >= 60
            ? QString::fromUtf8("约 %1 分钟").arg((remainingSeconds + 59) / 60)
            : QString::fromUtf8("不足 1 分钟");
    setBanner(instruction,
              QString::fromUtf8("剩余 %1 km · %2")
              .arg(remainingMeters / 1000.0, 0, 'f', 1).arg(timeText),
              kBannerNormal);
    if (!m_banner->isVisible())
        m_banner->show();
}

void MainWindow::onOffRouteDetected(const opmap::PointLatLng &pos, double deviationMeters)
{
    Q_UNUSED(pos);
    setBanner(QString::fromUtf8("偏航，正在重新规划…"),
              QString::fromUtf8("偏离路线 %1 米").arg(deviationMeters, 0, 'f', 0),
              kBannerOffRoute);
}

void MainWindow::onRerouteReady(const opmap::Route &route)
{
    m_navRoute = route;
    if (m_simulator->isRunning())
        m_simulator->reroute(route.polyline);   // 模拟车从当前位置切入新路线
    statusBar()->showMessage(QString::fromUtf8("已重新规划路线"), 5000);
    // 横幅底色由下一次 progressUpdated 恢复为常态
}

void MainWindow::onNavigationArrived()
{
    m_simulator->stop();
    setBanner(QString::fromUtf8("已到达目的地"), QString(), kBannerArrived);
    m_navInfo->setText(QString::fromUtf8("已到达"));
    QTimer::singleShot(4000, m_banner, SLOT(hide()));
}

void MainWindow::onNavigationFailed(const QString &reason)
{
    m_banner->hide();
    m_navInfo->setText(QString::fromUtf8("导航失败：%1").arg(reason));
    statusBar()->showMessage(QString::fromUtf8("导航失败：%1").arg(reason), 8000);
}

// ————————————————— 行车模拟面板 —————————————————

void MainWindow::onSimStartClicked()
{
    if (m_navRoute.polyline.size() < 2) {
        QMessageBox::information(this, QString::fromUtf8("提示"),
                                 QString::fromUtf8("请先开始导航，路线规划成功后再跟车模拟"));
        return;
    }
    if (m_pickMode != PickNone)
        setPickMode(PickNone);

    ensureUAV();
    m_simulator->setSpeed(m_speedCombo->currentIndex() == 0 ? 5.0
                          : m_speedCombo->currentIndex() == 1 ? 20.0 : 60.0);
    m_simulator->setPath(m_navRoute.polyline);
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

void MainWindow::onYawClicked()
{
    if (!m_simulator->isRunning()) {
        statusBar()->showMessage(QString::fromUtf8("跟车模拟未在运行，无法模拟偏航"), 5000);
        return;
    }
    m_simulator->simulateYaw();
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
    m_simInfo->setText(QString::fromUtf8("%1（进度 %2/%3）").arg(message).arg(current).arg(total));
}

void MainWindow::onSimFinished()
{
    statusBar()->showMessage(QString::fromUtf8("跟车模拟到达路线终点"), 5000);
}

// ————————————————— 位置源（模拟 / 系统 GPS） —————————————————

void MainWindow::onPosSourceChanged(int index)
{
    if (index == 0) {           // 行车模拟：停 GPS / IP，喂点交还模拟器
        stopGps();
        m_ipTimer->stop();
        return;
    }

    // 系统 GPS / IP 定位：先停模拟器，避免双源同时喂点
    m_simulator->stop();

    if (index == 2) {           // IP 定位（城市级兜底）
        stopGps();
        onIpFetchTimeout();     // 立即取一次，之后每 60s 轮询
        m_ipTimer->start(60000);
        return;
    }

    m_ipTimer->stop();

    if (!m_gpsSource) {
        // Windows 桌面默认 serialnmea 后端，无可用 GPS 时返回空指针
        m_gpsSource = QGeoPositionInfoSource::createDefaultSource(this);
        if (!m_gpsSource) {
            QMessageBox::warning(this, QString::fromUtf8("系统 GPS 不可用"),
                                 QString::fromUtf8("未找到可用的系统定位源，已回退到行车模拟"));
            m_posSourceCombo->blockSignals(true);
            m_posSourceCombo->setCurrentIndex(0);
            m_posSourceCombo->blockSignals(false);
            return;
        }
        connect(m_gpsSource, SIGNAL(positionUpdated(QGeoPositionInfo)),
                this, SLOT(onGpsPositionUpdated(QGeoPositionInfo)));
        ensureUAV();            // GPS 模式下按面板开关应用跟随/轨迹设置
    }
    m_gpsSource->startUpdates();
    statusBar()->showMessage(QString::fromUtf8("已切换到系统 GPS 位置源"), 5000);
}

void MainWindow::onGpsPositionUpdated(const QGeoPositionInfo &info)
{
    if (!info.isValid())
        return;
    const QGeoCoordinate c = info.coordinate();
    // QGeoCoordinate 为 (纬度, 经度)，与 PointLatLng(Lat, Lng) 顺序一致
    m_map->UpdateVehiclePosition(opmap::PointLatLng(c.latitude(), c.longitude()));
}

void MainWindow::stopGps()
{
    if (m_gpsSource)
        m_gpsSource->stopUpdates();
}

// ————————————————— IP 定位源（城市级兜底） —————————————————

void MainWindow::onIpFetchTimeout()
{
    // ip-api.com 免费无 key，返回 WGS-84 城市级坐标
    m_ipNam->get(QNetworkRequest(QUrl(QLatin1String("http://ip-api.com/json/?fields=status,lat,lon,city"))));
}

void MainWindow::onIpReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        statusBar()->showMessage(QString::fromUtf8("IP 定位失败：%1").arg(reply->errorString()), 8000);
        return;
    }
    QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
    if (obj.value(QLatin1String("status")).toString() != QLatin1String("success")) {
        statusBar()->showMessage(QString::fromUtf8("IP 定位失败：服务返回异常"), 8000);
        return;
    }
    const double lat = obj.value(QLatin1String("lat")).toDouble();
    const double lon = obj.value(QLatin1String("lon")).toDouble();
    const QString city = obj.value(QLatin1String("city")).toString();
    ensureUAV();
    m_map->UpdateVehiclePosition(opmap::PointLatLng(lat, lon));
    statusBar()->showMessage(QString::fromUtf8("IP 定位（城市级，精度约数公里）：%1 (%2, %3)")
                             .arg(city).arg(lat, 0, 'f', 4).arg(lon, 0, 'f', 4), 10000);
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
        m_destLabel->setText(QString::fromUtf8("目的地：lat %1, lng %2")
                             .arg(p.Lat(), 0, 'f', 5).arg(p.Lng(), 0, 'f', 5));
        break;
    default:
        break;
    }
    setPickMode(PickNone);
}

opmap::UAVItem* MainWindow::ensureUAV()
{
    // 只用 UAVS 表管理（AddUAV），避免 SetShowUAV 连带创建 GPSItem 与重复图标
    opmap::UAVItem *uav = m_map->GetUAV(0);
    if (!uav)
        uav = m_map->AddUAV(0);
    uav->SetTrailType(opmap::UAVTrailType::ByTimeElapsed);
    uav->SetTrailTime(3);
    uav->SetShowTrail(m_trailCheck->isChecked());
    uav->SetMapFollowType(m_followCheck->isChecked()
                          ? opmap::UAVMapFollowType::CenterMap
                          : opmap::UAVMapFollowType::None);
    return uav;
}

void MainWindow::setBanner(const QString &headline, const QString &subText, const QString &bgColor)
{
    QString html = QString::fromUtf8(
                "<div align='center' style='font-size:17px; font-weight:bold;'>%1</div>")
            .arg(headline.toHtmlEscaped());
    if (!subText.isEmpty())
        html += QString::fromUtf8(
                    "<div align='center' style='font-size:12px;'>%1</div>")
                .arg(subText.toHtmlEscaped());
    m_banner->setText(html);
    m_banner->setStyleSheet(QString::fromUtf8(
                "QLabel { background-color: %1; color: white; padding: 10px 24px;"
                " border-radius: 8px; }").arg(bgColor));
    m_banner->adjustSize();
    repositionBanner();
}

void MainWindow::repositionBanner()
{
    const int x = qMax(0, (m_map->width() - m_banner->width()) / 2);
    const int y = qMax(0, m_map->height() - m_banner->height() - 24);
    m_banner->move(x, y);
    m_banner->raise();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    repositionBanner();
}
