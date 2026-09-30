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
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QScrollArea>
#include <QtCore/QTime>
#include <QtCore/QTimer>
#include <QtCore/QDebug>
#include <QtGui/QResizeEvent>
#include <QtPositioning/QGeoPositionInfoSource>
#include <QtPositioning/QGeoPositionInfo>

#include "waypoint_store.h"
#include "navigation_simulator.h"
#include "waypoint_flight_simulator.h"
#include "uavitem.h"
#include "homeitem.h"
#include "waypointitem.h"

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
      m_flightBtn(new QPushButton(QString::fromUtf8("航点飞行"), this)),
      m_flightSim(0),
      m_wpActionCombo(new QComboBox(this)),
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
      m_mavProvider(0),
      m_ipTimer(new QTimer(this)),
      m_locatePending(false),
      m_followCheck(new QCheckBox(QString::fromUtf8("地图跟随车辆"), this)),
      m_trailCheck(new QCheckBox(QString::fromUtf8("显示行车轨迹"), this)),
      m_simInfo(new QLabel(QString::fromUtf8("空闲"), this)),
      m_banner(new QLabel(m_map)),
      m_posLabel(new QLabel(tr("lng: --, lat: --"), this)),
      m_tileLabel(new QLabel(tr("tiles: --"), this)),
      m_eventLog(0),
      m_navStateLabel(0),
      m_wingmanTimer(0),
      m_wingmanAngle(0),
      m_wingmanId(2),
      m_lastDlPct(-1),
      m_fenceBtn(new QPushButton(QString::fromUtf8("绘制围栏"), this)),
      m_pickMode(PickNone),
      m_origin(0, 0),
      m_dest(0, 0),
      m_hasOrigin(false),
      m_hasDest(false),
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
    qDebug("[app] MainWindow constructed, build=%s", __TIMESTAMP__);
}

// 菜单栏与右键菜单共用的地图源列表
namespace {
struct MapSourceEntry { opmap::MapType::Types type; const char *name; };
const MapSourceEntry kMapSources[] = {
    { opmap::MapType::AutoNaviRoad,      "高德路网" },
    { opmap::MapType::AutoNaviSatellite, "高德卫星" },
    { opmap::MapType::OpenStreetMap,     "OpenStreetMap" },
    { opmap::MapType::ArcGIS_Map,        "ArcGIS 地图" },
    { opmap::MapType::GoogleMap,         "Google 地图" },
};
const int kMapSourceCount = (int)(sizeof(kMapSources) / sizeof(kMapSources[0]));
}

void MainWindow::setupMenus()
{
    // 地图源菜单
    QMenu *mapMenu = menuBar()->addMenu(QString::fromUtf8("地图(&M)"));
    QActionGroup *group = new QActionGroup(this);
    for (int i = 0; i < kMapSourceCount; ++i) {
        QAction *act = mapMenu->addAction(QString::fromUtf8(kMapSources[i].name));
        act->setCheckable(true);
        act->setData((int)kMapSources[i].type);
        if (kMapSources[i].type == opmap::MapType::AutoNaviRoad)
            act->setChecked(true);
        group->addAction(act);
        m_mapTypeActions.append(act);
        connect(act, SIGNAL(triggered()), this, SLOT(onMapSourceTriggered()));
    }
    mapMenu->addSeparator();
    QAction *ripAct = mapMenu->addAction(QString::fromUtf8("下载框选区域离线瓦片…"));
    connect(ripAct, SIGNAL(triggered()), this, SLOT(onRipMapClicked()));

    // 右键菜单：切换地图源 / 航点增删（与旧版示例一致）
    m_map->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_map, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(onMapContextMenu(QPoint)));

    // 工具栏（缩放与定位）
    QToolBar *toolBar = addToolBar(QString::fromUtf8("视图"));
    QAction *zoomIn = toolBar->addAction(QString::fromUtf8("放大 +"));
    zoomIn->setShortcut(QKeySequence(QString::fromUtf8("Ctrl++")));
    connect(zoomIn, &QAction::triggered, [this]() {
        m_map->SetZoom(m_map->ZoomTotal() + 1);
        qDebug("[zoom] total=%d min=%d max=%d", (int)m_map->ZoomTotal(), m_map->MinZoom(), m_map->MaxZoom());
    });
    QAction *zoomOut = toolBar->addAction(QString::fromUtf8("缩小 −"));
    zoomOut->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+-")));
    connect(zoomOut, &QAction::triggered, [this]() {
        m_map->SetZoom(m_map->ZoomTotal() - 1);
        qDebug("[zoom] total=%d min=%d max=%d", (int)m_map->ZoomTotal(), m_map->MinZoom(), m_map->MaxZoom());
    });
    QAction *homeAct = toolBar->addAction(QString::fromUtf8("回到初始位置"));
    connect(homeAct, &QAction::triggered, [this]() {
        m_map->SetCurrentPosition(kHomePos);
        m_map->SetZoom(kHomeZoom);
    });
    QAction *locateAct = toolBar->addAction(QString::fromUtf8("定位当前位置"));
    locateAct->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+L")));
    connect(locateAct, SIGNAL(triggered()), this, SLOT(onLocateClicked()));
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
    // 库 WPInsert/WPRenumber 演示：中点插入与选中移至末尾
    QPushButton *insertWpBtn = new QPushButton(QString::fromUtf8("中点插入航点"), wpPanel);
    QPushButton *renumWpBtn = new QPushButton(QString::fromUtf8("选中移至末尾"), wpPanel);
    QHBoxLayout *wpBtnRow3 = new QHBoxLayout();
    wpBtnRow3->addWidget(insertWpBtn);
    wpBtnRow3->addWidget(renumWpBtn);
    wpBtnRow3->addWidget(m_flightBtn);
    wpLayout->addLayout(wpBtnRow3);
    connect(m_flightBtn, &QPushButton::clicked, this, &MainWindow::onFlightClicked);
    // 到达动作选择：点选添加的航点携带该动作，航点飞行到达时触发
    QHBoxLayout *wpActionRow = new QHBoxLayout();
    wpActionRow->addWidget(new QLabel(QString::fromUtf8("到达动作"), wpPanel));
    m_wpActionCombo->addItems(QStringList() << QString::fromUtf8("无")
                              << QString::fromUtf8("拍照") << QString::fromUtf8("悬停 30 秒"));
    wpActionRow->addWidget(m_wpActionCombo);
    wpActionRow->addStretch(1);
    wpLayout->addLayout(wpActionRow);
    wpLayout->addWidget(m_wpList);
    connect(m_addWpBtn, SIGNAL(clicked()), this, SLOT(onAddWaypointClicked()));
    connect(m_delWpBtn, SIGNAL(clicked()), this, SLOT(onDeleteWaypointClicked()));
    connect(clearBtn, SIGNAL(clicked()), this, SLOT(onClearWaypointsClicked()));
    connect(importBtn, SIGNAL(clicked()), this, SLOT(onImportWaypointsClicked()));
    connect(exportBtn, SIGNAL(clicked()), this, SLOT(onExportWaypointsClicked()));
    connect(m_wpList, SIGNAL(itemClicked(QListWidgetItem*)),
            this, SLOT(onWaypointListItemClicked(QListWidgetItem*)));
    connect(insertWpBtn, &QPushButton::clicked, this, &MainWindow::onInsertWaypointClicked);
    connect(renumWpBtn, &QPushButton::clicked, this, &MainWindow::onRenumberClicked);

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
    // —— 导航状态查询 / 路线显示开关 / 引擎参数（库能力示范）——
    m_navStateLabel = new QLabel(navPanel);
    m_navStateLabel->setWordWrap(true);
    navLayout->addWidget(m_navStateLabel);
    QCheckBox *showRouteCheck = new QCheckBox(QString::fromUtf8("显示路线绘制"), navPanel);
    showRouteCheck->setChecked(true);
    navLayout->addWidget(showRouteCheck);
    QHBoxLayout *engineRow = new QHBoxLayout();
    engineRow->addWidget(new QLabel(QString::fromUtf8("偏航m"), navPanel));
    QSpinBox *offRouteSpin = new QSpinBox(navPanel);
    offRouteSpin->setRange(10, 500);
    offRouteSpin->setValue(50);
    engineRow->addWidget(offRouteSpin);
    engineRow->addWidget(new QLabel(QString::fromUtf8("到达m"), navPanel));
    QSpinBox *arriveSpin = new QSpinBox(navPanel);
    arriveSpin->setRange(10, 200);
    arriveSpin->setValue(30);
    engineRow->addWidget(arriveSpin);
    navLayout->addLayout(engineRow);
    navLayout->addStretch(1);
    connect(showRouteCheck, &QCheckBox::toggled, m_map, &opmap::OPMapWidget::SetShowRoute);
    connect(offRouteSpin, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            [this](int v) { if (m_map->GetNavigationEngine()) m_map->GetNavigationEngine()->SetOffRouteThresholdM(v); });
    connect(arriveSpin, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            [this](int v) { if (m_map->GetNavigationEngine()) m_map->GetNavigationEngine()->SetArrivalThresholdM(v); });
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
    m_posSourceCombo->addItem(QString::fromUtf8("MAVLink (UDP)"));
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
    connect(m_ipTimer, SIGNAL(timeout()), this, SLOT(onIpPollTimeout()));
    // IP 定位的请求/双源回退/超时都在库内，demo 只处理结果
    connect(m_map, SIGNAL(ipLocationReady(opmap::PointLatLng,QString)),
            this, SLOT(onIpLocationReady(opmap::PointLatLng,QString)));
    connect(m_map, SIGNAL(ipLocationFailed(QString)),
            this, SLOT(onIpLocationFailed(QString)));
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

    // —— 库能力示范面板（左侧）与事件日志（底部）——
    setupCapabilityDock();
    setupEventLogDock();
    connectEventLog();
    refreshNavState();
}

/** 库能力示范面板：把 features.md 中未演示的地图控制/缓存管理/多机/几何 API
 *  以可交互控件的形式全部摆出来，作为各 API 的活文档。 */
void MainWindow::setupCapabilityDock()
{
    QWidget *panel = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setSpacing(6);

    // —— 视图控制 ——
    QGroupBox *viewBox = new QGroupBox(QString::fromUtf8("视图控制"), panel);
    QVBoxLayout *viewLayout = new QVBoxLayout(viewBox);
    QCheckBox *gridCheck = new QCheckBox(QString::fromUtf8("瓦片网格线"), viewBox);
    QCheckBox *dragCheck = new QCheckBox(QString::fromUtf8("允许拖动地图"), viewBox);
    dragCheck->setChecked(true);
    QCheckBox *glCheck = new QCheckBox(QString::fromUtf8("OpenGL 渲染"), viewBox);
    QCheckBox *followMouseCheck = new QCheckBox(QString::fromUtf8("鼠标跟随"), viewBox);
    QCheckBox *diagCheck = new QCheckBox(QString::fromUtf8("诊断信息叠显"), viewBox);
    viewLayout->addWidget(gridCheck);
    viewLayout->addWidget(dragCheck);
    viewLayout->addWidget(glCheck);
    viewLayout->addWidget(followMouseCheck);
    viewLayout->addWidget(diagCheck);
    QHBoxLayout *rotateRow = new QHBoxLayout();
    rotateRow->addWidget(new QLabel(QString::fromUtf8("旋转"), viewBox));
    QSlider *rotateSlider = new QSlider(Qt::Horizontal, viewBox);
    rotateSlider->setRange(-180, 180);
    rotateRow->addWidget(rotateSlider);
    QPushButton *rotResetBtn = new QPushButton(QString::fromUtf8("复位"), viewBox);
    rotateRow->addWidget(rotResetBtn);
    viewLayout->addLayout(rotateRow);
    QHBoxLayout *zoomRow = new QHBoxLayout();
    zoomRow->addWidget(new QLabel(QString::fromUtf8("缩放下限"), viewBox));
    QSpinBox *minZoomSpin = new QSpinBox(viewBox);
    minZoomSpin->setRange(0, 20);
    minZoomSpin->setValue(m_map->MinZoom());
    zoomRow->addWidget(minZoomSpin);
    zoomRow->addWidget(new QLabel(QString::fromUtf8("上限"), viewBox));
    QSpinBox *maxZoomSpin = new QSpinBox(viewBox);
    maxZoomSpin->setRange(1, 21);
    maxZoomSpin->setValue(m_map->MaxZoom());
    zoomRow->addWidget(maxZoomSpin);
    viewLayout->addLayout(zoomRow);
    QPushButton *reloadBtn = new QPushButton(QString::fromUtf8("强制重载地图"), viewBox);
    viewLayout->addWidget(reloadBtn);
    layout->addWidget(viewBox);

    connect(gridCheck, &QCheckBox::toggled, m_map, &opmap::OPMapWidget::SetShowTileGridLines);
    connect(dragCheck, &QCheckBox::toggled, m_map, &opmap::OPMapWidget::SetCanDragMap);
    connect(glCheck, &QCheckBox::toggled, m_map, &opmap::OPMapWidget::SetUseOpenGL);
    connect(followMouseCheck, &QCheckBox::toggled, m_map, &opmap::OPMapWidget::SetFollowMouse);
    connect(diagCheck, &QCheckBox::toggled, m_map, &opmap::OPMapWidget::SetShowDiagnostics);
    connect(rotateSlider, &QSlider::valueChanged, m_map, &opmap::OPMapWidget::SetRotate);
    connect(rotResetBtn, &QPushButton::clicked, [rotateSlider]() { rotateSlider->setValue(0); });
    connect(minZoomSpin, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            [this](int v) { m_map->SetMinZoom(v); });
    connect(maxZoomSpin, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            [this](int v) { m_map->SetMaxZoom(v); });
    connect(reloadBtn, &QPushButton::clicked, [this]() {
        m_map->ReloadMap();
        logEvent(QString::fromUtf8("已强制重载地图（ReloadMap）"));
    });

    // —— 缓存与访问 ——
    QGroupBox *cacheBox = new QGroupBox(QString::fromUtf8("缓存与访问"), panel);
    QVBoxLayout *cacheLayout = new QVBoxLayout(cacheBox);
    QHBoxLayout *modeRow = new QHBoxLayout();
    modeRow->addWidget(new QLabel(QString::fromUtf8("访问模式"), cacheBox));
    QComboBox *modeCombo = new QComboBox(cacheBox);
    modeCombo->addItem(QString::fromUtf8("仅网络"));
    modeCombo->addItem(QString::fromUtf8("网络+缓存"));
    modeCombo->addItem(QString::fromUtf8("仅缓存"));
    modeCombo->setCurrentIndex(1);
    modeRow->addWidget(modeCombo);
    cacheLayout->addLayout(modeRow);
    QHBoxLayout *memRow = new QHBoxLayout();
    memRow->addWidget(new QLabel(QString::fromUtf8("内存缓存MB"), cacheBox));
    QSpinBox *memSpin = new QSpinBox(cacheBox);
    memSpin->setRange(8, 1024);
    memSpin->setValue(64);
    memRow->addWidget(memSpin);
    QPushButton *memUseBtn = new QPushButton(QString::fromUtf8("占用?"), cacheBox);
    memRow->addWidget(memUseBtn);
    cacheLayout->addLayout(memRow);
    QPushButton *cleanBtn = new QPushButton(QString::fromUtf8("清理 7 天前旧瓦片"), cacheBox);
    cacheLayout->addWidget(cleanBtn);
    QPushButton *exportBtn = new QPushButton(QString::fromUtf8("导出缓存库到…"), cacheBox);
    cacheLayout->addWidget(exportBtn);
    QLabel *cacheDirLabel = new QLabel(m_map->configuration->CacheLocation(), cacheBox);
    cacheDirLabel->setWordWrap(true);
    cacheLayout->addWidget(cacheDirLabel);
    layout->addWidget(cacheBox);

    connect(modeCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            [this](int index) {
                const opmap::AccessMode::Types mode = (opmap::AccessMode::Types)index;
                m_map->configuration->SetAccessMode(mode);
                logEvent(QString::fromUtf8("访问模式 → %1").arg(opmap::AccessMode::StrByType(mode)));
            });
    connect(memSpin, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            [this](int mb) { m_map->configuration->SetTileMemorySize(mb); });
    connect(memUseBtn, &QPushButton::clicked, [this]() {
        logEvent(QString::fromUtf8("内存缓存占用 %1 MB").arg(m_map->configuration->TileMemoryUsed(), 0, 'f', 1));
    });
    connect(cleanBtn, &QPushButton::clicked, [this]() {
        m_map->configuration->DeleteTilesOlderThan(7);
        logEvent(QString::fromUtf8("已清理 7 天前的旧瓦片（DeleteTilesOlderThan）"));
    });
    connect(exportBtn, &QPushButton::clicked, [this]() {
        const QString dest = QFileDialog::getSaveFileName(
                    this, QString::fromUtf8("导出缓存到新库"), QString::fromUtf8("exported.qmdb"),
                    QString::fromUtf8("瓦片缓存库 (*.qmdb);;所有文件 (*.*)"));
        if (dest.isEmpty())
            return;
        m_map->configuration->ExportMapDataToDB(m_map->configuration->CacheLocation(), dest);
        logEvent(QString::fromUtf8("缓存库增量导出 → %1").arg(dest));
    });

    // —— 多机与几何 ——
    QGroupBox *demoBox = new QGroupBox(QString::fromUtf8("多机与几何"), panel);
    QVBoxLayout *demoLayout = new QVBoxLayout(demoBox);
    QPushButton *wingBtn = new QPushButton(QString::fromUtf8("添加僚机（多机演示）"), demoBox);
    demoLayout->addWidget(wingBtn);
    QPushButton *geoBtn = new QPushButton(QString::fromUtf8("几何换算演示"), demoBox);
    demoLayout->addWidget(geoBtn);
    demoLayout->addWidget(m_fenceBtn);   // 多边形地理围栏（取点 → 闭合 → 清除）
    connect(m_fenceBtn, &QPushButton::clicked, this, &MainWindow::onFenceClicked);
    layout->addWidget(demoBox);
    layout->addStretch(1);

    connect(wingBtn, &QPushButton::clicked, [this, wingBtn]() {
        if (!m_wingmanTimer) {   // 添加僚机：第二架 UAV 绕车辆/中心盘旋（演示 AddUAV/destPoint）
            opmap::UAVItem *wing = m_map->AddUAV(m_wingmanId);
            if (m_map->Home) {   // 打开 Home 安全圈（400m），僚机飞出即触发 UAVLeftSafetyBouble
                m_map->Home->SetShowSafeArea(true);
                m_map->Home->SetSafeArea(400);
                m_map->Home->update();
            }
            wing->SetUAVPos(m_map->HasVehiclePosition() ? m_map->VehiclePosition()
                                                        : m_map->CurrentPosition(), 100);
            m_wingmanTimer = new QTimer(this);
            connect(m_wingmanTimer, &QTimer::timeout, this, &MainWindow::onWingmanTick);
            m_wingmanTimer->start(400);
            wingBtn->setText(QString::fromUtf8("删除僚机"));
            logEvent(QString::fromUtf8("已添加僚机 #%1（AddUAV），400ms 绕飞，安全圈 400m").arg(m_wingmanId));
        } else {                 // 删除僚机
            m_wingmanTimer->stop();
            delete m_wingmanTimer;
            m_wingmanTimer = 0;
            m_map->DeleteUAV(m_wingmanId);
            wingBtn->setText(QString::fromUtf8("添加僚机（多机演示）"));
            logEvent(QString::fromUtf8("已删除僚机 #%1（DeleteUAV）").arg(m_wingmanId));
        }
    });
    connect(geoBtn, &QPushButton::clicked, [this]() {
        // 几何工具演示：bearing / haversineDistanceM / metersToPixels / destPoint
        const opmap::PointLatLng center = m_map->CurrentPosition();
        const opmap::PointLatLng to = m_map->HasVehiclePosition()
                ? m_map->VehiclePosition()
                : opmap::PointLatLng(center.Lat() + 0.01, center.Lng() + 0.01);
        const double brg = m_map->bearing(center, to);
        const double distM = opmap::geoutils::haversineDistanceM(center, to);
        const double px = m_map->metersToPixels(100.0);
        const opmap::PointLatLng probe = m_map->destPoint(center, 45.0, 1.0);   // 1 km（destPoint 距离单位为千米）
        logEvent(QString::fromUtf8("几何：中心→车辆 方位%1° 距离%2m | 100m=%3px | 中心向45°1km → (%4,%5)")
                 .arg(brg, 0, 'f', 1).arg(distM, 0, 'f', 0).arg(px, 0, 'f', 1)
                 .arg(probe.Lat(), 0, 'f', 5).arg(probe.Lng(), 0, 'f', 5));
    });

    // 左侧停靠 + 滚动区（面板内容较多，小窗口不挤爆）
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(panel);
    QDockWidget *capDock = new QDockWidget(QString::fromUtf8("库能力示范"), this);
    capDock->setWidget(scroll);
    addDockWidget(Qt::LeftDockWidgetArea, capDock);
}

/** 事件日志面板：底部停靠，实时展示库的事件流（信号驱动，零侵入）。 */
void MainWindow::setupEventLogDock()
{
    m_eventLog = new QListWidget(this);
    m_eventLog->setSelectionMode(QAbstractItemView::NoSelection);
    QDockWidget *logDock = new QDockWidget(QString::fromUtf8("事件日志（库信号）"), this);
    logDock->setWidget(m_eventLog);
    addDockWidget(Qt::BottomDockWidgetArea, logDock);
}

/** 库信号 → 日志。OnMapDrag / OnCurrentPositionChanged / OnTilesStillToLoad 等
 *  高频信号不进日志（状态栏 posLabel/tileLabel 已实时显示），避免刷屏。 */
void MainWindow::connectEventLog()
{
    // —— 瓦片加载生命周期 ——
    connect(m_map, &opmap::OPMapWidget::OnTileLoadStart,
            [this]() { logEvent(QString::fromUtf8("瓦片加载开始")); });
    connect(m_map, &opmap::OPMapWidget::OnTileLoadComplete,
            [this]() { logEvent(QString::fromUtf8("瓦片加载完成")); });
    connect(m_map, &opmap::OPMapWidget::OnEmptyTileError,
            [this](int zoom, opmap::Point pos) {
                logEvent(QString::fromUtf8("空瓦片错误 z%1 @(%2,%3)").arg(zoom).arg(pos.X()).arg(pos.Y()));
            });

    // —— 地图状态 ——
    connect(m_map, &opmap::OPMapWidget::OnMapZoomChanged,
            [this]() { logEvent(QString::fromUtf8("缩放变化 → %1").arg(m_map->ZoomTotal(), 0, 'f', 1)); });
    connect(m_map, &opmap::OPMapWidget::OnMapTypeChanged,
            [this](opmap::MapType::Types type) {
                logEvent(QString::fromUtf8("地图源 → %1").arg(opmap::MapType::StrByType(type)));
            });

    // —— 航点生命周期 ——
    connect(m_map, &opmap::OPMapWidget::WPInserted,
            [this](int number, opmap::WayPointItem *wp) {
                logEvent(QString::fromUtf8("航点 #%1 添加 %2").arg(number).arg(wp ? wp->Description() : QString()));
            });
    connect(m_map, &opmap::OPMapWidget::WPDeleted,
            [this](int number) { logEvent(QString::fromUtf8("航点 #%1 删除").arg(number)); });
    connect(m_map, &opmap::OPMapWidget::WPNumberChanged,
            [this](int oldn, int newn, opmap::WayPointItem *wp) {
                Q_UNUSED(wp);
                logEvent(QString::fromUtf8("航点 #%1 → #%2（重新编号）").arg(oldn).arg(newn));
            });
    connect(m_map, &opmap::OPMapWidget::WPValuesChanged,
            [this](opmap::WayPointItem *wp) {
                if (wp)
                    logEvent(QString::fromUtf8("航点 #%1 值变化（%2）").arg(wp->Number()).arg(wp->Description()));
            });
    connect(m_map, &opmap::OPMapWidget::WPReached,
            [this](opmap::WayPointItem *wp) {
                if (wp)
                    logEvent(QString::fromUtf8("到达航点 #%1").arg(wp->Number()));
            });

    // —— UAV 事件 ——
    connect(m_map, &opmap::OPMapWidget::UAVReachedWayPoint,
            [this](int number, opmap::WayPointItem *wp) {
                Q_UNUSED(wp);
                logEvent(QString::fromUtf8("UAV 到达航点 #%1").arg(number));
            });
    connect(m_map, &opmap::OPMapWidget::UAVLeftSafetyBouble,
            [this](const opmap::PointLatLng &pos) {
                logEvent(QString::fromUtf8("警告：飞出安全圈 @ (%1, %2)")
                         .arg(pos.Lat(), 0, 'f', 5).arg(pos.Lng(), 0, 'f', 5));
            });

    // —— 多边形围栏越界：日志 + 状态栏警示 + 横幅 5 秒 ——
    connect(m_map, &opmap::OPMapWidget::geofenceBreach,
            [this](const opmap::PointLatLng &pos) {
                logEvent(QString::fromUtf8("警告：飞出地理围栏 @ (%1, %2)")
                         .arg(pos.Lat(), 0, 'f', 5).arg(pos.Lng(), 0, 'f', 5));
                statusBar()->showMessage(QString::fromUtf8("越界警告：飞机已飞出地理围栏红线！"), 10000);
                setBanner(QString::fromUtf8("警告：飞出地理围栏"),
                          QString::fromUtf8("位置 lat %1, lng %2")
                          .arg(pos.Lat(), 0, 'f', 5).arg(pos.Lng(), 0, 'f', 5), kBannerOffRoute);
                QTimer::singleShot(5000, m_banner, &QLabel::hide);
            });

    // —— 离线下载进度（percent 按 10% 档节流）——
    connect(m_map, &opmap::OPMapWidget::mapDownloadProgress,
            [this](int percent) {
                if (percent / 10 != m_lastDlPct) {
                    m_lastDlPct = percent / 10;
                    logEvent(QString::fromUtf8("离线下载 %1%").arg(percent));
                }
            });
    connect(m_map, &opmap::OPMapWidget::mapDownloadFinished,
            [this]() { m_lastDlPct = -1; logEvent(QString::fromUtf8("离线下载结束")); });

    // —— 导航状态行刷新（停止导航无库信号，onStopNavClicked 内手动刷新）——
    connect(m_map, &opmap::OPMapWidget::navigationRouteReady, this, &MainWindow::refreshNavState);
    connect(m_map, &opmap::OPMapWidget::rerouteReady, this, &MainWindow::refreshNavState);
    connect(m_map, &opmap::OPMapWidget::navigationProgress, this, &MainWindow::refreshNavState);
    connect(m_map, &opmap::OPMapWidget::navigationArrived, this, &MainWindow::refreshNavState);
    connect(m_map, &opmap::OPMapWidget::navigationFailed, this, &MainWindow::refreshNavState);
}

void MainWindow::logEvent(const QString &text)
{
    if (!m_eventLog)
        return;
    m_eventLog->addItem(QString::fromUtf8("[%1] %2")
                        .arg(QTime::currentTime().toString(QString::fromUtf8("HH:mm:ss")), text));
    while (m_eventLog->count() > 200)          // 限长，避免长跑涨内存
        delete m_eventLog->takeItem(0);
    m_eventLog->scrollToBottom();
}

void MainWindow::refreshNavState()
{
    if (!m_navStateLabel)
        return;
    if (m_map->IsNavigating()) {
        const opmap::Route r = m_map->CurrentNavigationRoute();
        m_navStateLabel->setText(QString::fromUtf8("状态：导航中 · %1 段 / %2 km（CurrentNavigationRoute）")
                                 .arg(r.polyline.size())
                                 .arg(r.totalDistanceMeters / 1000.0, 0, 'f', 1));
    } else {
        m_navStateLabel->setText(QString::fromUtf8("状态：未导航（IsNavigating=false）"));
    }
}

/** 僚机绕飞：每拍方位角推进 30°，绕车辆位置（无则地图中心）300m 半径盘旋，
 *  同时演示 destPoint 几何换算与多机 UAV 同屏。 */
void MainWindow::onWingmanTick()
{
    opmap::UAVItem *wing = m_map->GetUAV(m_wingmanId);
    if (!wing)
        return;
    const opmap::PointLatLng base = m_map->HasVehiclePosition() ? m_map->VehiclePosition()
                                                                : m_map->CurrentPosition();
    m_wingmanAngle = fmod(m_wingmanAngle + 30.0, 360.0);
    // destPoint 的距离单位为千米（历史语义），300m = 0.3km
    wing->SetUAVPos(m_map->destPoint(base, m_wingmanAngle, 0.3), 100);
    wing->SetUAVHeading(m_wingmanAngle);
}

/** WPInsert 演示：在前两个航点的中点插入新航点（后续编号自动连锁）。 */
void MainWindow::onInsertWaypointClicked()
{
    const QMap<int, opmap::WayPointItem*> all = m_map->WPAll();
    if (all.size() < 2) {
        statusBar()->showMessage(QString::fromUtf8("中点插入需先有两个航点"), 5000);
        return;
    }
    const opmap::PointLatLng a = all.value(1)->Coord();
    const opmap::PointLatLng b = all.value(2)->Coord();
    const opmap::PointLatLng mid((a.Lat() + b.Lat()) / 2.0, (a.Lng() + b.Lng()) / 2.0);
    m_map->WPInsert(mid, 100, 2);
}

/** WPRenumber 演示：把选中航点移到末尾编号（其余航点自动连锁重排）。 */
void MainWindow::onRenumberClicked()
{
    const QList<opmap::WayPointItem*> sel = m_map->WPSelected();
    if (sel.isEmpty()) {
        statusBar()->showMessage(QString::fromUtf8("请先在地图或列表中选中一个航点"), 5000);
        return;
    }
    m_map->WPRenumber(sel.first(), m_map->WPAll().size() + 1);
}

// ———————— 航点飞行（模拟遥测沿航点序列飞）————————

void MainWindow::onFlightClicked()
{
    // 二次点击 = 停止飞行
    if (m_flightSim && m_flightSim->isActive()) {
        m_flightSim->stop();
        m_flightBtn->setText(QString::fromUtf8("航点飞行"));
        logEvent(QString::fromUtf8("航点飞行已手动停止"));
        return;
    }

    // 收集航点坐标（WPAll 按编号有序）
    QMap<int, opmap::WayPointItem*> wps = m_map->WPAll();
    if (wps.isEmpty()) {
        statusBar()->showMessage(QString::fromUtf8("请先在地图点选添加航点"), 5000);
        return;
    }
    QList<opmap::PointLatLng> coords;
    QList<int> hoverSecs;
    int photoCount = 0, hoverCount = 0;
    for (QMap<int, opmap::WayPointItem*>::const_iterator it = wps.constBegin(); it != wps.constEnd(); ++it) {
        coords.append(it.value()->Coord());
        hoverSecs.append(it.value()->HoverTime());
        if (it.value()->Action() == opmap::WayPointItem::WayPointActionPhoto)
            ++photoCount;
        else if (it.value()->Action() == opmap::WayPointItem::WayPointActionHover)
            ++hoverCount;
    }

    // Home 返航点设在起飞位置并打开安全围栏圈（现实场景：飞机飞出返航点半径即告警）
    const opmap::PointLatLng start = m_map->HasVehiclePosition() ? m_map->VehiclePosition() : kHomePos;
    if (m_map->Home) {
        m_map->Home->SetCoord(start);
        m_map->Home->SetShowSafeArea(true);
        m_map->Home->SetSafeArea(3000);
        m_map->Home->update();
    }

    // 主机 UAV：打开库内自动到达判定（进入 15 m 即 SetReached + UAVReachedWayPoint 信号）
    opmap::UAVItem *uav = ensureUAV();
    uav->SetAutoSetReached(true);
    uav->SetAutoSetDistance(15);
    uav->SetUAVPos(start, 120);
    uav->SetUAVHeading(0);

    if (!m_flightSim) {
        m_flightSim = new WaypointFlightSimulator(this);
        connect(m_flightSim, &WaypointFlightSimulator::positionChanged, this,
                [this](opmap::PointLatLng p, double heading, int idx, int total) {
            opmap::UAVItem *u = ensureUAV();   // 真机接入点：替换为遥测回调喂 SetUAVPos 即可
            u->SetUAVPos(p, 120);
            u->SetUAVHeading(heading);
            m_flightBtn->setText(QString::fromUtf8("停止飞行（目标 %1/%2）")
                                 .arg(qMin(idx + 1, total)).arg(total));
        });
        connect(m_flightSim, &WaypointFlightSimulator::waypointPassed, this,
                [this](int idx, int total) {
            // 库只携带动作数据，实际拍照/悬停由上层在到达信号里响应（真机=下发任务指令）
            opmap::WayPointItem *wp = m_map->WPAll().values().value(idx);
            if (wp && wp->Action() == opmap::WayPointItem::WayPointActionPhoto) {
                logEvent(QString::fromUtf8("【动作】触发拍照 [航点 %1]").arg(idx));
                statusBar()->showMessage(QString::fromUtf8("已触发拍照 [航点 %1]（真机上此处下发相机指令）").arg(idx), 8000);
            } else if (wp && wp->Action() == opmap::WayPointItem::WayPointActionHover) {
                logEvent(QString::fromUtf8("【动作】原地悬停 %1 秒 [航点 %2]").arg(wp->HoverTime()).arg(idx));
                statusBar()->showMessage(QString::fromUtf8("悬停中：%1 秒后飞向下一航点").arg(wp->HoverTime()), 8000);
            }
            logEvent(QString::fromUtf8("航点飞行：已到达 %1/%2（库侧 UAVReachedWayPoint 同步打勾）").arg(idx).arg(total));
        });
        connect(m_flightSim, &WaypointFlightSimulator::finished, this, [this]() {
            m_flightBtn->setText(QString::fromUtf8("航点飞行"));
            logEvent(QString::fromUtf8("航点任务完成：全部航点已到达"));
        });
    }

    m_flightSim->start(start, coords, hoverSecs, 25.0);
    logEvent(QString::fromUtf8("航点飞行开始：%1 个航点（拍照 %2、悬停 %3），从 Home 位置起飞，巡航 25 m/s，安全围栏 3000 m")
             .arg(coords.size()).arg(photoCount).arg(hoverCount));
}

/** 围栏按钮三态：绘制围栏 →（地图连续取点）→ 结束围栏 → 清除围栏 → 绘制围栏。 */
void MainWindow::onFenceClicked()
{
    if (m_pickMode == PickFence) {          // 第二次点击：结束取点并闭合
        setPickMode(PickNone);
        if (m_fencePts.size() >= 3) {
            m_map->SetGeofence(m_fencePts);
            m_fenceBtn->setText(QString::fromUtf8("清除围栏"));
            logEvent(QString::fromUtf8("多边形地理围栏生效：%1 个顶点。配合航点飞行或行车模拟，飞机飞出红区即在日志报越界").arg(m_fencePts.size()));
        } else {
            m_fencePts.clear();
            m_map->SetGeofence(QList<opmap::PointLatLng>());   // 清掉取点预览残留
            m_fenceBtn->setText(QString::fromUtf8("绘制围栏"));
            logEvent(QString::fromUtf8("围栏顶点不足 3 个，已取消"));
        }
        return;
    }

    if (m_map->HasGeofence()) {             // 已有围栏：清除
        m_map->ClearGeofence();
        m_fencePts.clear();
        m_fenceBtn->setText(QString::fromUtf8("绘制围栏"));
        logEvent(QString::fromUtf8("地理围栏已清除"));
        return;
    }

    // 开始取点：地图上依次点击放置顶点，完成后再次点击本按钮闭合
    m_fencePts.clear();
    setPickMode(PickFence);
    m_fenceBtn->setText(QString::fromUtf8("结束围栏"));
    statusBar()->showMessage(QString::fromUtf8("在地图上点击放置围栏顶点（至少 3 个），完成后点击“结束围栏”"), 10000);
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
    SyncMapTypeActions();
}

void MainWindow::SyncMapTypeActions()
{
    // 菜单栏与右键菜单可能各自触发，统一按当前地图源同步勾选态
    const opmap::MapType::Types cur = m_map->GetMapType();
    for (int i = 0; i < m_mapTypeActions.count(); ++i)
        m_mapTypeActions.at(i)->setChecked(m_mapTypeActions.at(i)->data().toInt() == (int)cur);
}

void MainWindow::onMapContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    QMenu *typeMenu = menu.addMenu(QString::fromUtf8("切换地图类型"));
    for (int i = 0; i < kMapSourceCount; ++i) {
        QAction *act = typeMenu->addAction(QString::fromUtf8(kMapSources[i].name));
        act->setData((int)kMapSources[i].type);
        act->setCheckable(true);
        act->setChecked(kMapSources[i].type == m_map->GetMapType());
        connect(act, SIGNAL(triggered()), this, SLOT(onMapSourceTriggered()));
    }
    menu.addSeparator();
    QAction *addWp = menu.addAction(QString::fromUtf8("在此处添加航点"));
    QAction *delWp = menu.addAction(QString::fromUtf8("删除选中航点"));
    delWp->setEnabled(!m_map->WPSelected().isEmpty());

    QAction *chosen = menu.exec(m_map->mapToGlobal(pos));
    if (chosen == addWp) {
        const opmap::PointLatLng p = m_map->currentMousePosition();
        m_map->WPCreate(p, 0, QString::fromUtf8("航点 %1").arg(m_map->WPAll().count() + 1));
    } else if (chosen == delWp) {
        QList<opmap::WayPointItem*> sel = m_map->WPSelected();
        for (int i = 0; i < sel.count(); ++i)
            m_map->WPDelete(sel.at(i));
    }
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
    refreshNavState();
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
        m_map->RequestIpLocation();     // 立即取一次，之后每 60s 轮询
        m_ipTimer->start(60000);
        return;
    }

    if (index == 3) {           // MAVLink (UDP 14550)：真机/SITL 遥测接入点
        stopGps();
        m_ipTimer->stop();
        if (!m_mavProvider) {
            m_mavProvider = new opmap::MavlinkTelemetryProvider(this);
            connect(m_mavProvider, SIGNAL(positionUpdated(double,double,double,double)),
                    this, SLOT(onMavPositionUpdated(double,double,double,double)));
            connect(m_mavProvider, SIGNAL(linkAlive()), this, SLOT(onMavLinkAlive()));
            connect(m_mavProvider, SIGNAL(linkTimeout()), this, SLOT(onMavLinkTimeout()));
            ensureUAV();        // 遥测喂车辆/UAV 图标，跟随/轨迹按面板开关生效
        }
        if (m_mavProvider->start(14550))
            statusBar()->showMessage(QString::fromUtf8("MAVLink 遥测监听中（UDP 14550），等待飞控数据…"), 8000);
        else
            QMessageBox::warning(this, QString::fromUtf8("MAVLink 不可用"),
                                 QString::fromUtf8("UDP 14550 端口监听失败（可能被占用），已保持当前模式"));
        return;
    }

    m_ipTimer->stop();

    // 切回系统 GPS：停 MAVLink 遥测，避免双源同时喂点
    if (m_mavProvider && m_mavProvider->isListening())
        m_mavProvider->stop();

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

// ————————————————— MAVLink 遥测源（真机/SITL 接入点） —————————————————

void MainWindow::onMavPositionUpdated(double lat, double lon, double altM, double headingDeg)
{
    Q_UNUSED(altM);
    // 链路对齐 QGeoCoordinate：lat/lon 直接对应 PointLatLng(Lat, Lng)
    m_map->UpdateVehiclePosition(opmap::PointLatLng(lat, lon));
    if (headingDeg >= 0)    // 库内 UpdateVehiclePosition 也按位移推算航向，飞控自带 hdg 优先
        qDebug("[mavlink] pos update lat=%.6f lon=%.6f alt=%.1fm hdg=%.0f", lat, lon, altM, headingDeg);
}

void MainWindow::onMavLinkAlive()
{
    logEvent(QString::fromUtf8("MAVLink 链路建立：收到 GLOBAL_POSITION_INT 遥测"));
}

void MainWindow::onMavLinkTimeout()
{
    logEvent(QString::fromUtf8("MAVLink 链路超时：5 秒未收到遥测包，请检查飞控/模拟器"));
}

// ————————————————— IP 定位源（城市级兜底） —————————————————

void MainWindow::onLocateClicked()
{
    qDebug("[locate] onLocateClicked fired, hasVehicle=%d", (int)m_map->HasVehiclePosition());
    if (m_map->HasVehiclePosition()) {
        CenterOnVehicle();
        return;
    }
    // 无车辆位置：自动走一次 IP 定位兜底（车载导航式的一键定位）
    statusBar()->showMessage(QString::fromUtf8("正在通过 IP 定位当前位置…"), 10000);
    m_locatePending = true;
    m_map->RequestIpLocation();
}

void MainWindow::CenterOnVehicle()
{
    qDebug("[locate] CenterOnVehicle %.4f,%.4f curZoom=%.1f",
           m_map->VehiclePosition().Lat(), m_map->VehiclePosition().Lng(), m_map->ZoomTotal());
    m_map->SetCurrentPosition(m_map->VehiclePosition());
    if (m_map->ZoomTotal() < 15.0)
        m_map->SetZoom(15.0);       // 定位时切到街区级缩放
    statusBar()->showMessage(QString::fromUtf8("已定位到车辆当前位置"), 3000);
}

void MainWindow::onIpPollTimeout()
{
    // 位置源选"IP 定位"时每 60s 触发一次；请求/双源回退/超时全在库内
    m_map->RequestIpLocation();
    logEvent(QString::fromUtf8("IP 定位轮询触发（IsIpLocationBusy=%1）")
             .arg(m_map->IsIpLocationBusy() ? "true" : "false"));
}

void MainWindow::onIpLocationReady(opmap::PointLatLng pos, QString city)
{
    ensureUAV();
    m_map->UpdateVehiclePosition(pos);
    statusBar()->showMessage(QString::fromUtf8("IP 定位（城市级，精度约数公里）：%1 (%2, %3)")
                             .arg(city).arg(pos.Lat(), 0, 'f', 4).arg(pos.Lng(), 0, 'f', 4), 10000);
    if (m_locatePending) {
        m_locatePending = false;
        CenterOnVehicle();
    }
}

void MainWindow::onIpLocationFailed(QString reason)
{
    statusBar()->showMessage(QString::fromUtf8("IP 定位失败：%1").arg(reason), 8000);
    if (m_locatePending) {
        m_locatePending = false;
        QMessageBox::warning(this, QString::fromUtf8("定位失败"),
                             QString::fromUtf8("IP 定位失败（%1），请检查网络后重试").arg(reason));
    }
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
        // 携带面板选择的到达动作（库只存数据，飞行时上层响应）
        const int act = m_wpActionCombo->currentIndex();
        if (act == 1) {
            wp->SetAction(opmap::WayPointItem::WayPointActionPhoto);
            wp->SetDescription(wp->Description() + QString::fromUtf8("[拍照]"));
        } else if (act == 2) {
            wp->SetAction(opmap::WayPointItem::WayPointActionHover);
            wp->SetHoverTime(30);
            wp->SetDescription(wp->Description() + QString::fromUtf8("[悬停30s]"));
        }
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
    case PickFence:
        // 围栏取点：逐点加入并实时预览（1 点=顶点圆点，2 点=连线，≥3 点闭合生效），模式保持不退出
        m_fencePts.append(p);
        m_map->SetGeofence(m_fencePts);
        logEvent(QString::fromUtf8("围栏顶点 %1: lat %2, lng %3")
                 .arg(m_fencePts.size()).arg(p.Lat(), 0, 'f', 5).arg(p.Lng(), 0, 'f', 5));
        return;   // 保持 PickFence，直到点"结束围栏"
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
