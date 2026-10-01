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
#include <QtWidgets/QInputDialog>
#include <QtCore/QTime>
#include <QtCore/QTimer>
#include <QtCore/QDebug>
#include <QtGui/QResizeEvent>

#include "navigation_simulator.h"
#include "waypoint_flight_simulator.h"
#include "uavitem.h"
#include "homeitem.h"
#include "waypointitem.h"
#include "mapmarkeritem.h"
#include <QtMath>

namespace {

const opmap::PointLatLng kHomePos(34.2609, 108.9424);   ///< 初始位置：西安
const double kHomeZoom = 12.0;

const char *kBannerNormal   = "rgba(20,20,20,220)";    ///< 指令横幅常态底色
const char *kBannerOffRoute = "rgba(170,120,0,220)";   ///< 偏航警示底色（黄）
const char *kBannerArrived  = "rgba(0,110,40,220)";    ///< 到达提示底色（绿）

} // anonymous namespace

MainWindow::MainWindow()
    : m_map(new opmap::OPMapWidget(this)),
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
      m_routeSwitchBtn(new QPushButton(QString::fromUtf8("备选路线"), this)),
      m_altIndex(0),
      m_navInfo(new QLabel(QString::fromUtf8("空闲"), this)),
      m_simStartBtn(new QPushButton(QString::fromUtf8("开始跟车"), this)),
      m_simPauseBtn(new QPushButton(QString::fromUtf8("暂停"), this)),
      m_simStopBtn(new QPushButton(QString::fromUtf8("停止"), this)),
      m_yawBtn(new QPushButton(QString::fromUtf8("模拟偏航"), this)),
      m_mockPosBtn(new QPushButton(QString::fromUtf8("点选喂位置"), this)),
      m_speedCombo(new QComboBox(this)),
      m_posSourceCombo(new QComboBox(this)),
      m_followCheck(new QCheckBox(QString::fromUtf8("地图跟随车辆"), this)),
      m_trailCheck(new QCheckBox(QString::fromUtf8("显示行车轨迹"), this)),
      m_simInfo(new QLabel(QString::fromUtf8("空闲"), this)),
      m_banner(new QLabel(m_map)),
      m_posLabel(new QLabel(tr("lng: --, lat: --"), this)),
      m_tileLabel(new QLabel(tr("tiles: --"), this)),
      m_eventLog(0),
      m_navStateLabel(0),
      m_lastDlPct(-1),
      m_fenceBtn(new QPushButton(QString::fromUtf8("绘制围栏"), this)),
      m_flightSpeedMps(80),
      m_origin(0, 0),
      m_dest(0, 0),
      m_hasOrigin(false),
      m_hasDest(false),
      m_providerIsAmap(false),
      m_peerTimer(new QTimer(this)),
      m_peerAngle(0),
      m_migrantTimer(new QTimer(this)),
      m_migrantElapsed(0),
      m_measureBtn(new QPushButton(QString::fromUtf8("开始测距"), this)),
      m_recTrailBtn(new QPushButton(QString::fromUtf8("记录轨迹"), this)),
      m_replayBtn(new QPushButton(QString::fromUtf8("回放轨迹…"), this)),
      m_stopReplayBtn(new QPushButton(QString::fromUtf8("停止回放"), this)),
      m_replaySpeedSpin(new QDoubleSpinBox(this))
{
    setWindowTitle(QString::fromUtf8("opmapcontrol 示例 — 地图/航点/车载导航"));
    resize(1200, 800);

    setCentralWidget(m_map);
    m_map->SetMapType(opmap::MapType::AutoNaviRoad);
    m_map->SetCurrentPosition(kHomePos);
    m_map->SetZoom(kHomeZoom);
    m_map->SetShowHome(true);   // 打开 Home 返航点图标（惰性创建，默认不显示）

    // 地图信号 → 本窗口
    connect(m_map, SIGNAL(mouseMove(QMouseEvent*)), this, SLOT(onMapMouseMove(QMouseEvent*)));
    connect(m_map, SIGNAL(zoomChanged(double,double,double)), this, SLOT(onZoomChanged(double,double,double)));
    connect(m_map, SIGNAL(OnTilesStillToLoad(int)), this, SLOT(onTilesStill(int)));
    // 库点选交互：取点/结束由库防抖与多点累积统一分发（航点/起终点/围栏/喂位置）
    connect(m_map, SIGNAL(positionPicked(int,opmap::PointLatLng)),
            this, SLOT(onPositionPicked(int,opmap::PointLatLng)));
    connect(m_map, SIGNAL(pickFinished(int,QList<opmap::PointLatLng>)),
            this, SLOT(onPickFinished(int,QList<opmap::PointLatLng>)));

    // 库导航信号 → 面板/横幅
    connect(m_map, SIGNAL(navigationRouteReady(opmap::Route)), this, SLOT(onNavigationRouteReady(opmap::Route)));
    connect(m_map, &opmap::OPMapWidget::routeAlternativesReady,
            this, &MainWindow::onRouteAlternativesReady);
    connect(m_map, SIGNAL(navigationProgress(double,int,QString)), this, SLOT(onNavProgress(double,int,QString)));
    connect(m_map, SIGNAL(routeSelected(int,opmap::Route)), this, SLOT(onRouteSelected(int,opmap::Route)));
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

    // 多人共享位置演示：定时器驱动模拟位置报文
    connect(m_peerTimer, SIGNAL(timeout()), this, SLOT(onPeerTick()));

    // 候鸟迁徙演示：定时器沿大圆弧路线推进各个体
    connect(m_migrantTimer, SIGNAL(timeout()), this, SLOT(onMigrantTick()));

    // 量测与轨迹：库信号 → 按钮状态/日志
    connect(m_map, SIGNAL(measureFinished(double,QList<opmap::PointLatLng>)),
            this, SLOT(onMeasureFinished(double,QList<opmap::PointLatLng>)));
    connect(m_map, SIGNAL(trailReplayFinished()), this, SLOT(onTrailReplayFinished()));

    setupMenus();
    setupDocks();
    setupStatusBar();

    m_followCheck->setChecked(true);
    m_trailCheck->setChecked(true);
    m_map->SetFollowVehicle(true);   // 跟随开关下沉到库：喂点自动居中，UAV 创建时套用

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

    // 右键菜单：切换地图源 / 航点增删（库转发右键事件，取点中的右键已被库拦截为"结束取点"）
    connect(m_map, SIGNAL(mapContextMenuRequested(QPoint)),
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

    // 多人共享位置演示：点击开始/停止（成员标记实时移动）
    QAction *peersAct = toolBar->addAction(QString::fromUtf8("多人位置演示"));
    peersAct->setCheckable(true);
    connect(peersAct, SIGNAL(toggled(bool)), this, SLOT(onPeersDemoToggled(bool)));
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
    // 备选路线循环切换：规划出 ≥2 条时可用（库 SelectRoute 切换显示与导航偏好）
    m_routeSwitchBtn->setEnabled(false);
    navBtnRow->addWidget(m_routeSwitchBtn);
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
    connect(m_routeSwitchBtn, SIGNAL(clicked()), this, SLOT(onSwitchRouteClicked()));
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
    simLayout->addWidget(m_mockPosBtn);
    simLayout->addWidget(m_followCheck);
    simLayout->addWidget(m_trailCheck);
    simLayout->addWidget(m_simInfo);
    simLayout->addStretch(1);
    connect(m_simStartBtn, SIGNAL(clicked()), this, SLOT(onSimStartClicked()));
    connect(m_simPauseBtn, SIGNAL(clicked()), this, SLOT(onSimPauseClicked()));
    connect(m_simStopBtn, SIGNAL(clicked()), this, SLOT(onSimStopClicked()));
    connect(m_mockPosBtn, SIGNAL(clicked()), this, SLOT(onMockPosClicked()));
    connect(m_yawBtn, SIGNAL(clicked()), this, SLOT(onYawClicked()));
    connect(m_speedCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onSpeedChanged(int)));
    connect(m_posSourceCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onPosSourceChanged(int)));
    // IP 定位的请求/双源回退/超时都在库内，demo 只处理结果提示
    connect(m_map, SIGNAL(ipLocationReady(opmap::PointLatLng,QString)),
            this, SLOT(onIpLocationReady(opmap::PointLatLng,QString)));
    connect(m_map, SIGNAL(ipLocationFailed(QString)),
            this, SLOT(onIpLocationFailed(QString)));
    // 库位置源管理：错误提示回退 + MAVLink 链路状态日志
    connect(m_map, SIGNAL(positionSourceError(QString,bool)),
            this, SLOT(onPosSourceError(QString,bool)));
    connect(m_map, SIGNAL(positionLinkAlive()), this, SLOT(onPositionLinkAlive()));
    connect(m_map, SIGNAL(positionLinkTimeout()), this, SLOT(onPositionLinkTimeout()));
    connect(m_followCheck, SIGNAL(toggled(bool)), this, SLOT(onFollowToggled(bool)));
    // 库内跟随开关变化 → 复选框同步（任务启动时库自动暂停跟随）
    connect(m_map, SIGNAL(mapFollowChanged(bool)), this, SLOT(onMapFollowChanged(bool)));
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
    QCheckBox *scaleCheck = new QCheckBox(QString::fromUtf8("显示比例尺"), viewBox);
    scaleCheck->setChecked(m_map->ShowScale());   // 库默认开启，勾选态与库状态对齐
    viewLayout->addWidget(gridCheck);
    viewLayout->addWidget(dragCheck);
    viewLayout->addWidget(glCheck);
    viewLayout->addWidget(followMouseCheck);
    viewLayout->addWidget(diagCheck);
    viewLayout->addWidget(scaleCheck);
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
    connect(scaleCheck, &QCheckBox::toggled, m_map, &opmap::OPMapWidget::SetShowScale);
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

    // —— 量测与轨迹 ——
    QGroupBox *measureBox = new QGroupBox(QString::fromUtf8("量测与轨迹"), panel);
    QVBoxLayout *measureLayout = new QVBoxLayout(measureBox);
    measureLayout->addWidget(m_measureBtn);   // 多点测距：开始 → 地图点取顶点 → 右键结束一段
    QPushButton *clearMeasureBtn = new QPushButton(QString::fromUtf8("清除测距"), measureBox);
    measureLayout->addWidget(clearMeasureBtn);
    measureLayout->addWidget(m_recTrailBtn);  // 记录位置流（所有位置源统一截获）
    QPushButton *saveTrailBtn = new QPushButton(QString::fromUtf8("保存轨迹…"), measureBox);
    measureLayout->addWidget(saveTrailBtn);
    QHBoxLayout *replayRow = new QHBoxLayout();
    m_replaySpeedSpin->setRange(0.5, 16.0);
    m_replaySpeedSpin->setSingleStep(0.5);
    m_replaySpeedSpin->setValue(1.0);
    m_replaySpeedSpin->setToolTip(QString::fromUtf8("回放倍速"));
    replayRow->addWidget(new QLabel(QString::fromUtf8("倍速"), measureBox));
    replayRow->addWidget(m_replaySpeedSpin);
    replayRow->addWidget(m_replayBtn);
    measureLayout->addLayout(replayRow);
    m_stopReplayBtn->setEnabled(false);
    measureLayout->addWidget(m_stopReplayBtn);
    QPushButton *migrationBtn = new QPushButton(QString::fromUtf8("候鸟迁徙演示"), measureBox);
    migrationBtn->setCheckable(true);
    measureLayout->addWidget(migrationBtn);
    connect(m_measureBtn, &QPushButton::clicked, this, &MainWindow::onMeasureClicked);
    connect(clearMeasureBtn, &QPushButton::clicked, [this]() {
        m_map->ClearMeasurements();
        logEvent(QString::fromUtf8("已清除全部测距折线"));
    });
    connect(m_recTrailBtn, &QPushButton::clicked, this, &MainWindow::onRecTrailClicked);
    connect(saveTrailBtn, &QPushButton::clicked, [this]() {
        const QString path = QFileDialog::getSaveFileName(
                    this, QString::fromUtf8("保存运动轨迹"), QString::fromUtf8("flight.trail.json"),
                    QString::fromUtf8("轨迹文件 (*.trail.json *.json);;所有文件 (*.*)"));
        if (path.isEmpty())
            return;
        QString err;
        if (m_map->SaveTrailToFile(path, &err)) {
            m_recTrailBtn->setText(QString::fromUtf8("记录轨迹"));   // 存盘即收笔（缓冲保留可回放）
            m_map->StopTrailRecording();
            logEvent(QString::fromUtf8("轨迹已保存 → %1").arg(path));
        } else {
            logEvent(QString::fromUtf8("轨迹保存失败：%1").arg(err));
        }
    });
    connect(m_replayBtn, &QPushButton::clicked, this, &MainWindow::onReplayClicked);
    connect(m_stopReplayBtn, &QPushButton::clicked, this, &MainWindow::onStopReplayClicked);
    connect(migrationBtn, &QPushButton::toggled, this, &MainWindow::onMigrationToggled);
    layout->addWidget(measureBox);

    // —— 地理围栏 ——
    QGroupBox *demoBox = new QGroupBox(QString::fromUtf8("地理围栏"), panel);
    QVBoxLayout *demoLayout = new QVBoxLayout(demoBox);
    demoLayout->addWidget(m_fenceBtn);   // 多边形地理围栏（绘制 → 闭合 → 清除）
    connect(m_fenceBtn, &QPushButton::clicked, this, &MainWindow::onFenceClicked);
    layout->addWidget(demoBox);
    layout->addStretch(1);

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
    connect(m_map, &opmap::OPMapWidget::UAVEnteredSafetyBouble,
            [this](const opmap::PointLatLng &pos) {
                logEvent(QString::fromUtf8("提示：已回到安全圈 @ (%1, %2)")
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

    // —— 回到围栏内：日志 + 状态栏提示（与越界成对）——
    connect(m_map, &opmap::OPMapWidget::geofenceEntered,
            [this](const opmap::PointLatLng &pos) {
                logEvent(QString::fromUtf8("提示：已进入地理围栏 @ (%1, %2)")
                         .arg(pos.Lat(), 0, 'f', 5).arg(pos.Lng(), 0, 'f', 5));
                statusBar()->showMessage(QString::fromUtf8("已进入地理围栏区域"), 8000);
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
    // 二次点击 = 停止飞行（库任务状态机 + 模拟遥测源一起停）
    if (m_flightSim && m_flightSim->isActive()) {
        m_map->StopWaypointMission();
        m_flightSim->stop();
        m_flightBtn->setText(QString::fromUtf8("航点飞行"));
        // 恢复位置源图标语义（大头针=位置标记，四旋翼=飞行目标）
        if (opmap::UAVItem *u = m_map->GetUAV(0))
            u->SetIcon(QString::fromUtf8(":/markers/images/bigMarkerGreen.png"));
        logEvent(QString::fromUtf8("航点飞行已手动停止"));
        return;
    }

    // 收集航点（WPAll 按编号有序）；坐标/悬停/动作由库 StartWaypointMission 提取
    QMap<int, opmap::WayPointItem*> wps = m_map->WPAll();
    if (wps.isEmpty()) {
        statusBar()->showMessage(QString::fromUtf8("请先在地图点选添加航点"), 5000);
        return;
    }
    int photoCount = 0, hoverCount = 0;
    for (QMap<int, opmap::WayPointItem*>::const_iterator it = wps.constBegin(); it != wps.constEnd(); ++it) {
        if (it.value()->Action() == opmap::WayPointItem::WayPointActionPhoto)
            ++photoCount;
        else if (it.value()->Action() == opmap::WayPointItem::WayPointActionHover)
            ++hoverCount;
    }

    // 起飞点仅用于模拟遥测源（真机接入时遥测从实际位置来）；
    // 库 StartWaypointMission 自动完成：惰性建 UAV/到达参数/安全圈显示/
    // UAV 摆位起飞点并跳转视图/暂停跟随
    opmap::PointLatLng start;
    if (m_map->Home)
        start = m_map->Home->Coord();
    else
        start = m_map->HasVehiclePosition() ? m_map->VehiclePosition() : kHomePos;

    if (!m_flightSim) {
        m_flightSim = new WaypointFlightSimulator(this);
        connect(m_flightSim, &WaypointFlightSimulator::positionChanged, this,
                [this](opmap::PointLatLng p, double heading) {
            // 真机接入点：整体替换本模拟器，遥测直接喂 SetUAVPos——
            // 围栏判定、到达判定、动作触发全在库内，链路不变
            m_map->SetUAVPos(0, p, 120);
            m_map->SetUAVHeading(0, heading);
        });
    }
    // 模拟器跟随库任务状态机：目标切换/悬停开关/动作日志/完成收尾全部来自库信号
    connect(m_map, &opmap::OPMapWidget::missionCurrentWaypointChanged, m_flightSim,
            [this](int idx) {
        QMap<int, opmap::WayPointItem*> all = m_map->WPAll();
        opmap::WayPointItem *wp = all.values().value(idx);
        if (wp)
            m_flightSim->setTarget(wp->Coord());
        m_flightBtn->setText(QString::fromUtf8("停止飞行（目标 %1/%2）")
                             .arg(idx + 1).arg(all.size()));
    });
    connect(m_map, &opmap::OPMapWidget::missionHoverStateChanged, m_flightSim,
            &WaypointFlightSimulator::setHovering);
    connect(m_map, &opmap::OPMapWidget::missionActionTriggered, this,
            [this](int idx, int action) {
        // 实际拍照/悬停由上层在动作信号里响应（真机=下发任务指令）
        QMap<int, opmap::WayPointItem*> all = m_map->WPAll();
        opmap::WayPointItem *wp = all.values().value(idx);
        if (action == (int)opmap::WayPointItem::WayPointActionPhoto) {
            logEvent(QString::fromUtf8("【动作】触发拍照 [航点 %1]").arg(idx));
            statusBar()->showMessage(QString::fromUtf8("已触发拍照 [航点 %1]（真机上此处下发相机指令）").arg(idx), 8000);
        } else if (action == (int)opmap::WayPointItem::WayPointActionHover && wp) {
            logEvent(QString::fromUtf8("【动作】原地悬停 %1 秒 [航点 %2]").arg(wp->HoverTime()).arg(idx));
            statusBar()->showMessage(QString::fromUtf8("悬停中：%1 秒后飞向下一航点").arg(wp->HoverTime()), 8000);
        }
    });
    connect(m_map, &opmap::OPMapWidget::missionWaypointReached, this,
            [this](int idx, int total_) {
        Q_UNUSED(total_);
        logEvent(QString::fromUtf8("航点飞行：已到达 [航点 %1]（库侧 UAVReachedWayPoint 同步打勾）").arg(idx));
    });
    connect(m_map, &opmap::OPMapWidget::missionFinished, this, [this]() {
        m_flightSim->stop();
        m_flightBtn->setText(QString::fromUtf8("航点飞行"));
        logEvent(QString::fromUtf8("航点任务完成：全部航点已到达（库任务状态机 missionFinished）"));
    });

    // 启动：库状态机 + 假遥测源（真机接入时删除模拟器，遥测直接喂 SetUAVPos）
    m_flightSim->start(start, double(m_flightSpeedMps));
    m_map->StartWaypointMission(wps.values(), 15.0);
    if (opmap::UAVItem *u = m_map->GetUAV(0))
        u->SetIcon(QString::fromUtf8(":/uavs/images/mapquad.png"));   // 飞行=四旋翼图标（展示语义，库默认大头针）
    logEvent(QString::fromUtf8("航点飞行开始：%1 个航点（拍照 %2、悬停 %3），从 Home 图标处起飞，巡航 %4 m/s，安全围栏 %5 m，跟随已自动暂停")
             .arg(wps.size()).arg(photoCount).arg(hoverCount).arg(m_flightSpeedMps)
             .arg(m_map->Home ? m_map->Home->SafeArea() : 0));
}

/** 围栏按钮三态：绘制围栏 →（库内连续取点+橡皮筋预览）→ 结束围栏 → 清除围栏 → 绘制围栏。 */
void MainWindow::onFenceClicked()
{
    // 取点中：再点按钮 = 结束（库按顶点数固化/清理，pickFinished 回调恢复按钮文案）
    if (m_map->GetPickMode() == opmap::OPMapWidget::PickFence) {
        m_map->SetPickMode(opmap::OPMapWidget::PickNone);
        return;
    }

    if (m_map->HasGeofence()) {             // 已有围栏：清除
        m_map->ClearGeofence();
        m_fenceBtn->setText(QString::fromUtf8("绘制围栏"));
        logEvent(QString::fromUtf8("地理围栏已清除"));
        return;
    }

    // 开始取点：地图上依次点击放置顶点（防抖/预览/累积全在库内），完成后再次点击本按钮闭合
    m_map->SetPickMode(opmap::OPMapWidget::PickFence);
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
    // 取点模式中的右键已由库拦截为"结束取点"，此槽只会收到正常浏览的右键
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

    // 测距：进入多点测距模式（逐点画线，右键结束一段），或清除已有结果
    QAction *startMeasure = menu.addAction(QString::fromUtf8("开始测距（多点）"));
    QAction *clearMeasure = menu.addAction(QString::fromUtf8("清除测距结果"));
    clearMeasure->setEnabled(m_map->HasMeasurements());

    // Home 返航点与飞行参数
    menu.addSeparator();
    QAction *setHome = menu.addAction(QString::fromUtf8("设置 Home 返航点到此位置"));
    setHome->setEnabled(m_map->Home != 0);
    QAction *setSafeArea = menu.addAction(QString::fromUtf8("设置安全围栏半径…"));
    setSafeArea->setEnabled(m_map->Home != 0);
    QAction *setSpeed = menu.addAction(QString::fromUtf8("设置航点飞行速度…"));

    // 通用标记演示：图片/文字/图文组合/清除
    menu.addSeparator();
    QAction *addImgMarker = menu.addAction(QString::fromUtf8("在此处添加图片标记"));
    QAction *addTextMarker = menu.addAction(QString::fromUtf8("在此处添加文字标记…"));
    QAction *addBothMarker = menu.addAction(QString::fromUtf8("在此处添加图文标记…"));
    QAction *clearMarkers = menu.addAction(QString::fromUtf8("清除所有标记"));
    QAction *delMarker = menu.addAction(QString::fromUtf8("删除选中标记"));
    delMarker->setEnabled(false);
    const QList<QGraphicsItem*> selNow = m_map->scene()->selectedItems();
    for (int i = 0; i < selNow.count(); ++i)
        if (qgraphicsitem_cast<opmap::MapMarkerItem*>(selNow.at(i))) {
            delMarker->setEnabled(true);
            break;
        }

    QAction *chosen = menu.exec(m_map->mapToGlobal(pos));
    if (chosen == addWp) {
        const opmap::PointLatLng p = m_map->currentMousePosition();
        m_map->WPCreate(p, 0, QString::fromUtf8("航点 %1").arg(m_map->WPAll().count() + 1));
        refreshWaypointList();
    } else if (chosen == delWp) {
        QList<opmap::WayPointItem*> sel = m_map->WPSelected();
        for (int i = 0; i < sel.count(); ++i)
            m_map->WPDelete(sel.at(i));
        if (!sel.isEmpty())
            refreshWaypointList();
    } else if (chosen == startMeasure) {
        m_map->SetPickMode(opmap::OPMapWidget::PickMeasure);
        m_measureBtn->setText(QString::fromUtf8("结束测距"));
        statusBar()->showMessage(QString::fromUtf8("多点测距：在地图上逐点点击画折线（每段/总距离实时标注），右键结束一段"), 10000);
        logEvent(QString::fromUtf8("多点测距开始（右键菜单进入）"));
    } else if (chosen == clearMeasure) {
        m_map->ClearMeasurements();
        logEvent(QString::fromUtf8("已清除全部测距折线"));
    } else if (chosen == setHome) {
        m_map->Home->SetCoord(m_map->currentMousePosition());
        m_map->Home->update();
        logEvent(QString::fromUtf8("Home 返航点已移至 (%1, %2)，起飞将从此处开始")
                 .arg(m_map->Home->Coord().Lat(), 0, 'f', 5)
                 .arg(m_map->Home->Coord().Lng(), 0, 'f', 5));
    } else if (chosen == setSafeArea) {
        bool ok = false;
        int meters = QInputDialog::getInt(this, QString::fromUtf8("安全围栏半径"),
                                          QString::fromUtf8("半径（米，飞出即告警）："),
                                          m_map->Home->SafeArea(), 100, 50000, 100, &ok);
        if (ok) {
            m_map->Home->SetSafeArea(meters);
            m_map->Home->update();
            logEvent(QString::fromUtf8("Home 安全围栏半径已设为 %1 m").arg(meters));
        }
    } else if (chosen == setSpeed) {
        bool ok = false;
        int mps = QInputDialog::getInt(this, QString::fromUtf8("航点飞行速度"),
                                       QString::fromUtf8("巡航速度（m/s）："),
                                       m_flightSpeedMps, 1, 300, 5, &ok);
        if (ok) {
            m_flightSpeedMps = mps;
            logEvent(QString::fromUtf8("航点飞行巡航速度已设为 %1 m/s").arg(mps));
        }
    } else if (chosen == addImgMarker) {
        // 演示库 API：创建即带图 + SetImageSize 控制显示尺寸（32×32），
        // 返回句柄可再 SetCoord 移动 / SetText 追加标签 / RemoveMarker 删除
        opmap::MapMarkerItem *m = m_map->AddMarker(m_map->currentMousePosition(),
                                                   QString::fromUtf8(":/markers/images/marker.png"));
        m->SetImageSize(32, 32);
        logEvent(QString::fromUtf8("已添加图片标记（库图钉缩至 32×32）"));
    } else if (chosen == addTextMarker) {
        bool ok = false;
        QString label = QInputDialog::getText(this, QString::fromUtf8("文字标记"),
                                              QString::fromUtf8("标签内容："), QLineEdit::Normal,
                                              QString(), &ok);
        if (ok && !label.isEmpty()) {
            opmap::MapMarkerItem *m = m_map->AddMarker(m_map->currentMousePosition());
            m->SetText(label);
            m->SetFontSize(11);
            logEvent(QString::fromUtf8("已添加文字标记：%1").arg(label));
        }
    } else if (chosen == addBothMarker) {
        bool ok = false;
        QString label = QInputDialog::getText(this, QString::fromUtf8("图文标记"),
                                              QString::fromUtf8("标签内容："), QLineEdit::Normal,
                                              QString(), &ok);
        if (ok) {
            // 图文组合：SetImage + SetText 叠加生效——图片底边钉在坐标上，标签在其正下方
            opmap::MapMarkerItem *m = m_map->AddMarker(m_map->currentMousePosition(),
                                                       QString::fromUtf8(":/markers/images/marker.png"));
            m->SetImageSize(32, 32);
            if (!label.isEmpty())
                m->SetText(label);
            logEvent(QString::fromUtf8("已添加图文标记%1").arg(label.isEmpty() ? QString() : QString::fromUtf8("：%1").arg(label)));
        }
    } else if (chosen == clearMarkers) {
        m_map->ClearMarkers();
        logEvent(QString::fromUtf8("已清除所有标记"));
    } else if (chosen == delMarker) {
        QList<opmap::MapMarkerItem*> sel;
        const QList<QGraphicsItem*> items = m_map->scene()->selectedItems();
        for (int i = 0; i < items.count(); ++i)
            if (opmap::MapMarkerItem *mk = qgraphicsitem_cast<opmap::MapMarkerItem*>(items.at(i)))
                sel.append(mk);
        for (int i = 0; i < sel.count(); ++i)
            m_map->RemoveMarker(sel.at(i));
        if (!sel.isEmpty())
            logEvent(QString::fromUtf8("已删除 %1 个选中标记").arg(sel.count()));
    }
}

// ————————————————— 多人共享位置演示 —————————————————

void MainWindow::onPeersDemoToggled(bool on)
{
    if (on) {
        if (!m_migrantMarkers.isEmpty()) {   // 迁徙与多人位置演示共用标记层，防互删
            logEvent(QString::fromUtf8("多人位置演示与迁徙演示共用标记层，请先关闭候鸟迁徙演示"));
            if (QAction *act = qobject_cast<QAction*>(sender()))
                act->setChecked(false);
            return;
        }
        // 3 名模拟成员：头像图钉 + 名字标签，绕西安附近各自圆心匀速转圈。
        // 真实接入时只需：收到共享报文 → m_peerMarkers[id]->SetCoord(newPos)
        struct Seed { const char *name; double dLat, dLng, rLat, rLng, phase; };
        const Seed seeds[] = {
            { "张三",  0.002,  0.003, 0.004, 0.005, 0.0   },
            { "李四", -0.003, -0.002, 0.003, 0.004, 2.094 },
            { "王五",  0.001, -0.004, 0.005, 0.003, 4.189 },
        };
        m_peerSims.clear();
        m_peerMarkers.clear();
        for (int i = 0; i < 3; ++i) {
            PeerSim s;
            s.name = QString::fromUtf8(seeds[i].name);
            s.cLat = kHomePos.Lat() + seeds[i].dLat;
            s.cLng = kHomePos.Lng() + seeds[i].dLng;
            s.rLat = seeds[i].rLat;
            s.rLng = seeds[i].rLng;
            s.phase = seeds[i].phase;
            opmap::MapMarkerItem *m = m_map->AddMarker(
                opmap::PointLatLng(s.cLat, s.cLng),
                QString::fromUtf8(":/markers/images/marker.png"));
            m->SetImageSize(24, 24);
            m->SetText(s.name);
            m->SetShowTrail(true);   // 演示标记移动轨迹（5Hz 上报点连成折线）
            m_peerSims.append(s);
            m_peerMarkers.insert(s.name, m);
        }
        m_peerAngle = 0.0;
        m_map->SetCurrentPosition(kHomePos);   // 视野带回成员聚集区
        m_peerTimer->start(200);               // 5Hz 模拟位置报文
        logEvent(QString::fromUtf8("多人共享位置演示开始：3 名成员每 200ms 上报一次位置"));
    } else {
        m_peerTimer->stop();
        m_map->ClearMarkers();
        m_peerMarkers.clear();
        m_peerSims.clear();
        logEvent(QString::fromUtf8("多人共享位置演示停止，标记已清除"));
    }
}

void MainWindow::onPeerTick()
{
    // 模拟位置报文到达：各成员绕圆心匀速转动，一行 SetCoord 即实时移动
    m_peerAngle += 0.15;
    for (int i = 0; i < m_peerSims.size(); ++i) {
        const PeerSim &s = m_peerSims.at(i);
        const double a = m_peerAngle + s.phase;
        m_peerMarkers.value(s.name)->SetCoord(
            opmap::PointLatLng(s.cLat + s.rLat * qSin(a),
                               s.cLng + s.rLng * qCos(a)));
    }
}

// ————————————————— 候鸟迁徙演示 —————————————————

namespace {
/// 大圆弧（球面）插值：把两经纬点投到单位球做 slerp 后转回经纬度，
/// 远距离迁徙路线呈地球表面最短弧线而非墨卡托直线
opmap::PointLatLng SlerpLatLng(const opmap::PointLatLng &a, const opmap::PointLatLng &b, double f)
{
    if (f <= 0.0)
        return a;
    if (f >= 1.0)
        return b;
    const double d2r = M_PI / 180.0;
    const double latA = a.Lat() * d2r, lngA = a.Lng() * d2r;
    const double latB = b.Lat() * d2r, lngB = b.Lng() * d2r;
    // 经纬度 → 单位球笛卡尔
    const double ax = qCos(latA) * qCos(lngA), ay = qCos(latA) * qSin(lngA), az = qSin(latA);
    const double bx = qCos(latB) * qCos(lngB), by = qCos(latB) * qSin(lngB), bz = qSin(latB);
    const double dot = qBound(-1.0, ax * bx + ay * by + az * bz, 1.0);
    const double omega = qAcos(dot);
    if (omega < 1e-9)   // 两点重合/极近：线性退化
        return a;
    const double s = qSin(omega);
    const double wA = qSin((1.0 - f) * omega) / s;
    const double wB = qSin(f * omega) / s;
    const double x = wA * ax + wB * bx;
    const double y = wA * ay + wB * by;
    const double z = wA * az + wB * bz;
    const double r2r = 180.0 / M_PI;
    return opmap::PointLatLng(qAsin(qBound(-1.0, z, 1.0)) * r2r, qAtan2(y, x) * r2r);
}
}

void MainWindow::onMigrationToggled(bool on)
{
    if (on) {
        if (!m_peerMarkers.isEmpty()) {   // 迁徙与多人位置演示共用标记层，防互删
            logEvent(QString::fromUtf8("迁徙演示与多人位置演示共用标记层，请先关闭多人位置演示"));
            if (QPushButton *b = qobject_cast<QPushButton*>(sender()))
                b->setChecked(false);
            return;
        }
        // 4 只候鸟各自真实迁徙路线（繁殖地 → 中停地 → 越冬地），时长不同
        // 长途演示压缩到 1 分钟级：tick 200ms，全程 45~75s
        m_migrants.clear();
        m_migrantMarkers.clear();
        struct Seed { const char *name; int durationS;
                      double pts[3][2]; };   // 每个体 3 个途经点（纬度,经度）
        const Seed seeds[] = {
            { "红嘴鸥", 60, { {52.5, 107.2}, {39.2, 119.6}, {29.1, 116.3} } },  // 贝加尔湖→渤海湾→鄱阳湖
            { "大天鹅", 50, { {47.2, 103.8}, {37.8, 119.1}, {37.2, 122.6} } },  // 蒙古高原→黄河三角洲→荣成
            { "白鹤",   75, { {62.0, 129.7}, {45.3, 132.5}, {29.1, 116.3} } },  // 雅库特→兴凯湖→鄱阳湖
            { "绿头鸭", 55, { {52.0, 112.0}, {35.0, 122.5}, {31.2, 121.9} } },  // 贝加尔湖东→黄海→长江口
        };
        for (int i = 0; i < 4; ++i) {
            MigrantSim m;
            m.name = QString::fromUtf8(seeds[i].name);
            for (int j = 0; j < 3; ++j)
                m.route.append(opmap::PointLatLng(seeds[i].pts[j][0], seeds[i].pts[j][1]));
            m.durationMs = seeds[i].durationS * 1000;
            m.arrived = false;
            opmap::MapMarkerItem *mk = m_map->AddMarker(m.route.first());
            mk->SetText(m.name);
            mk->SetFontSize(10);
            mk->SetShowTrail(true);   // 移动轨迹连为橙色折线（库内 MarkerTrailItem）
            m_migrants.append(m);
            m_migrantMarkers.insert(m.name, mk);
        }
        m_migrantElapsed = 0;
        m_map->SetCurrentPosition(opmap::PointLatLng(44.0, 119.0));   // 视野罩住东亚迁飞区
        m_map->SetZoom(4);
        m_migrantTimer->start(200);
        logEvent(QString::fromUtf8("候鸟迁徙演示开始：4 只候鸟沿大圆弧迁飞区路线南迁（每 200ms 推进，橙色线为飞行轨迹）"));
    } else {
        m_migrantTimer->stop();
        m_map->ClearMarkers();
        m_migrantMarkers.clear();
        m_migrants.clear();
        logEvent(QString::fromUtf8("候鸟迁徙演示停止，标记已清除"));
    }
}

void MainWindow::onMigrantTick()
{
    m_migrantElapsed += 200;
    int arrivedCount = 0;
    for (int i = 0; i < m_migrants.size(); ++i) {
        MigrantSim &m = m_migrants[i];
        const double f = qMin(1.0, (double)m_migrantElapsed / m.durationMs);
        // 全程按段均分时间：段内大圆弧插值推进
        const int legs = m.route.size() - 1;
        const double scaled = f * legs;
        const int leg = qMin(legs - 1, (int)scaled);
        const double legF = scaled - leg;
        const opmap::PointLatLng pos = SlerpLatLng(m.route.at(leg), m.route.at(leg + 1), legF);
        m_migrantMarkers.value(m.name)->SetCoord(pos);
        if (f >= 1.0) {
            ++arrivedCount;
            if (!m.arrived) {
                m.arrived = true;
                logEvent(QString::fromUtf8("%1 已抵达越冬地").arg(m.name));
            }
        }
    }
    if (arrivedCount == m_migrants.size()) {
        m_migrantTimer->stop();
        logEvent(QString::fromUtf8("迁徙演示完成：全部候鸟抵达越冬地（标记与轨迹保留，关闭按钮可清除）"));
    }
}

// ————————————————— 量测与轨迹 —————————————————

void MainWindow::onMeasureClicked()
{
    // 测距中：再点按钮 = 结束当前段（库 EndPick 固化并发 measureFinished）
    if (m_map->GetPickMode() == opmap::OPMapWidget::PickMeasure) {
        m_map->SetPickMode(opmap::OPMapWidget::PickNone);
        return;
    }
    // 开始/继续：进入多点测距模式，逐点点击画折线（防抖/预览/标签全在库内）
    m_map->SetPickMode(opmap::OPMapWidget::PickMeasure);
    m_measureBtn->setText(QString::fromUtf8("结束测距"));
    statusBar()->showMessage(QString::fromUtf8("在地图上逐点点击画测距折线（每段/总距离实时标注），右键结束一段"), 10000);
}

void MainWindow::onMeasureFinished(double totalMeters, const QList<opmap::PointLatLng> &points)
{
    m_measureBtn->setText(QString::fromUtf8("开始测距"));
    const QString dist = totalMeters < 1000.0
            ? QString::fromUtf8("%1 m").arg(totalMeters, 0, 'f', 1)
            : QString::fromUtf8("%1 km").arg(totalMeters / 1000.0, 0, 'f', 3);
    logEvent(QString::fromUtf8("测距完成：%1 段折线，总距离 %2").arg(points.size() - 1).arg(dist));
    statusBar()->showMessage(QString::fromUtf8("测距总距离 %1，可再次点击“开始测距”追加新测量").arg(dist), 8000);
}

void MainWindow::onRecTrailClicked()
{
    if (m_map->IsTrailReplaying())
        m_map->StopTrailReplay();   // 记录与回放互斥（库内也有保护，UI 同步停）
    if (m_map->IsTrailRecording()) {
        m_map->StopTrailRecording();
        m_recTrailBtn->setText(QString::fromUtf8("记录轨迹"));
        logEvent(QString::fromUtf8("轨迹记录停止，共 %1 个采样点，可保存或回放")
                 .arg(m_map->TrailPointCount()));
    } else {
        m_map->StartTrailRecording();
        m_recTrailBtn->setText(QString::fromUtf8("停止记录"));
        logEvent(QString::fromUtf8("轨迹记录开始：所有位置源喂点（跟车/航点飞行/GPS/MAVLink）自动入库"));
    }
}

void MainWindow::onReplayClicked()
{
    const QString path = QFileDialog::getOpenFileName(
                this, QString::fromUtf8("选择轨迹文件"), QString(),
                QString::fromUtf8("轨迹文件 (*.trail.json *.json);;所有文件 (*.*)"));
    if (path.isEmpty())
        return;
    QString err;
    if (!m_map->LoadTrailFromFile(path, &err)) {
        logEvent(QString::fromUtf8("轨迹加载失败：%1").arg(err));
        return;
    }
    const double speed = m_replaySpeedSpin->value();
    if (!m_map->StartTrailReplay(speed)) {
        logEvent(QString::fromUtf8("轨迹点数不足（至少 2 个），无法回放"));
        return;
    }
    m_recTrailBtn->setText(QString::fromUtf8("记录轨迹"));
    m_replayBtn->setEnabled(false);
    m_stopReplayBtn->setEnabled(true);
    logEvent(QString::fromUtf8("轨迹回放开始（%1 倍速）：UAV 图标按原始时序沿轨迹重演").arg(speed));
}

void MainWindow::onStopReplayClicked()
{
    m_map->StopTrailReplay();
    m_replayBtn->setEnabled(true);
    m_stopReplayBtn->setEnabled(false);
    logEvent(QString::fromUtf8("轨迹回放已中止"));
}

void MainWindow::onTrailReplayFinished()
{
    m_replayBtn->setEnabled(true);
    m_stopReplayBtn->setEnabled(false);
    logEvent(QString::fromUtf8("轨迹回放播完"));
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
    m_map->SetPickMode(opmap::OPMapWidget::PickWaypoint);   // 单发：取一次库自动结束
    m_addWpBtn->setEnabled(false);
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
    QString err;
    if (!m_map->WPImportFromFile(path, &err)) {
        QMessageBox::warning(this, QString::fromUtf8("导入失败"), err);
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
    QString err;
    if (!m_map->WPExportToFile(path, &err)) {
        QMessageBox::warning(this, QString::fromUtf8("导出失败"), err);
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
    m_map->SetPickMode(opmap::OPMapWidget::PickOrigin);   // 单发：取一次库自动结束
    statusBar()->showMessage(QString::fromUtf8("在地图上点击设置起点"), 5000);
}

void MainWindow::onPickDestClicked()
{
    m_map->SetPickMode(opmap::OPMapWidget::PickDest);
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
    m_map->SetPickMode(opmap::OPMapWidget::PickNone);   // 中止未完成的取点（无取点时为空操作）
    applyProviderFromUI();
    m_altRoutes.clear();                // 新规划重置换选状态（结果在 onRouteAlternativesReady 刷新）
    m_altIndex = 0;
    m_routeSwitchBtn->setEnabled(false);
    m_routeSwitchBtn->setText(QString::fromUtf8("备选路线"));
    m_navInfo->setText(QString::fromUtf8("规划中…"));
    m_map->PlanRoute(m_origin, m_dest);
}

/// 备选路线就绪：列出各条概要，≥2 条时启用切换按钮
void MainWindow::onRouteAlternativesReady(QList<opmap::Route> routes)
{
    m_altRoutes = routes;
    m_altIndex = 0;
    for (int i = 0; i < routes.size(); ++i) {
        logEvent(QString::fromUtf8("%1路线 %2：%3 km，约 %4 分钟")
                 .arg(i == 0 ? QString::fromUtf8("推荐") : QString::fromUtf8("备选"))
                 .arg(i + 1)
                 .arg(routes.at(i).totalDistanceMeters / 1000.0, 0, 'f', 1)
                 .arg(qRound(routes.at(i).totalDurationSeconds / 60.0)));
    }
    if (routes.size() >= 2) {
        m_routeSwitchBtn->setEnabled(true);
        m_routeSwitchBtn->setText(QString::fromUtf8("备选路线 1/%1").arg(routes.size()));
    } else {
        m_routeSwitchBtn->setEnabled(false);
        m_routeSwitchBtn->setText(QString::fromUtf8("备选路线（无）"));
    }
}

/// 循环切换备选路线：库内立即换画线，之后的导航/偏航重规划沿选中的走
void MainWindow::onSwitchRouteClicked()
{
    if (m_altRoutes.size() < 2)
        return;
    m_altIndex = (m_altIndex + 1) % m_altRoutes.size();
    m_map->SelectRoute(m_altIndex);
    m_routeSwitchBtn->setText(QString::fromUtf8("备选路线 %1/%2")
                              .arg(m_altIndex + 1).arg(m_altRoutes.size()));
    logEvent(QString::fromUtf8("已切换到%1路线：%2 km，约 %3 分钟")
             .arg(m_altIndex == 0 ? QString::fromUtf8("推荐") : QString::fromUtf8("备选"))
             .arg(m_altRoutes.at(m_altIndex).totalDistanceMeters / 1000.0, 0, 'f', 1)
             .arg(qRound(m_altRoutes.at(m_altIndex).totalDurationSeconds / 60.0)));
}

/// 备选路线切换（库信号）：面板距离/预计时间同步到当前选中路线
void MainWindow::onRouteSelected(int index, const opmap::Route &route)
{
    Q_UNUSED(index);
    m_navInfo->setText(QString::fromUtf8("距离 %1 km，预计 %2 分钟（%3 点）")
                       .arg(route.totalDistanceMeters / 1000.0, 0, 'f', 1)
                       .arg(route.totalDurationSeconds / 60).arg(route.polyline.size()));
}

void MainWindow::onNavigateClicked()
{
    if (!m_hasDest) {
        QMessageBox::information(this, QString::fromUtf8("提示"),
                                 QString::fromUtf8("请先在地图上点选目的地"));
        return;
    }
    m_map->SetPickMode(opmap::OPMapWidget::PickNone);   // 中止未完成的取点
    applyProviderFromUI();
    // 新导航任务：清空旧轨迹，从头记录（否则起点瞬移会与旧轨迹终点拉出连线）
    if (opmap::UAVItem *u = m_map->GetUAV(0))
        u->DeleteTrail();
    if (m_hasOrigin) {
        // 指定点选起点出发：先把车辆位置喂到起点（图标同步 + 作为规划起点）
        m_map->UpdateVehiclePosition(m_origin);
        // 瞬移无航向意义：复位图标竖直（原位置→起点的随机方位角会让大头针歪斜）
        m_map->SetUAVHeading(0, 0);
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
    m_map->SetPickMode(opmap::OPMapWidget::PickNone);   // 中止未完成的取点

    if (opmap::UAVItem *u = m_map->GetUAV(0))
        u->DeleteTrail();   // 跟车从头记录轨迹（UAV 尚未出现则无可清理）
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

void MainWindow::onMockPosClicked()
{
    // 切换"点选喂位置"模式：开启后点哪喂哪（所见即所得）；
    // 库内连续取点（防抖/多点累积），右键或再点本按钮结束
    if (m_map->GetPickMode() == opmap::OPMapWidget::PickPosition) {
        m_map->SetPickMode(opmap::OPMapWidget::PickNone);
        return;
    }
    m_map->SetPickMode(opmap::OPMapWidget::PickPosition);
    m_mockPosBtn->setText(QString::fromUtf8("结束喂点"));
    statusBar()->showMessage(QString::fromUtf8("点选喂位置模式：在地图上点击任意位置即喂入该点"), 8000);
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
    m_map->SetFollowVehicle(on);   // 库内管理：喂点自动居中，UAV 未创建时先记忆
}

void MainWindow::onMapFollowChanged(bool following)
{
    m_followCheck->blockSignals(true);   // 回写复选框，避免再触发 onFollowToggled 回环
    m_followCheck->setChecked(following);
    m_followCheck->blockSignals(false);
}

void MainWindow::onTrailToggled(bool on)
{
    opmap::UAVItem *uav = m_map->GetUAV(0);
    if (!uav)
        return;   // UAV 由首次喂点惰性创建（库默认轨迹开启），出现后再调此开关
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

// ————————————————— 位置源（模拟 / 系统 GPS / IP / MAVLink） —————————————————

void MainWindow::onPosSourceChanged(int index)
{
    // 图标语义区分：位置源=绿色大头针（位置标记），航点飞行=四旋翼无人机图标
    if (opmap::UAVItem *u = m_map->GetUAV(0))
        u->SetIcon(QString::fromUtf8(":/markers/images/bigMarkerGreen.png"));

    // 库内互斥切换：启用一个源前自动停用其它库源（GPS/IP/MAVLink 源的
    // 启停、轮询、错误回退全部库内完成），位置点自动喂入并经 positionUpdated 分发
    switch (index) {
    case 1:
        m_map->SetPositionSource(opmap::SourceSystemGps);
        break;
    case 2:
        m_map->SetPositionSource(opmap::SourceIpLocation);
        break;
    case 3:
        m_map->SetPositionSource(opmap::SourceMavlink);
        break;
    default:
        m_map->SetPositionSource(opmap::SourceExternal);   // 行车模拟：demo 侧喂点
        break;
    }

    // 模拟器是 demo 侧喂点器（库管不到）：切走时停掉，避免双源同时喂点
    if (index != 0)
        m_simulator->stop();
}

void MainWindow::onPosSourceError(const QString &reason, bool fatal)
{
    if (!fatal) {
        logEvent(QString::fromUtf8("位置源错误：%1").arg(reason));
        return;
    }
    // fatal（GPS 缺失 / UDP 端口占用）：库已停用该源，demo 回退 UI 到行车模拟
    QMessageBox::warning(this, QString::fromUtf8("位置源不可用"),
                         QString::fromUtf8("%1，已回退到行车模拟").arg(reason));
    m_posSourceCombo->blockSignals(true);
    m_posSourceCombo->setCurrentIndex(0);
    m_posSourceCombo->blockSignals(false);
    m_map->SetPositionSource(opmap::SourceExternal);
    logEvent(QString::fromUtf8("位置源启用失败：%1，已回退到外部喂点").arg(reason));
}

void MainWindow::onPositionLinkAlive()
{
    logEvent(QString::fromUtf8("MAVLink 链路建立：收到 GLOBAL_POSITION_INT 遥测"));
}

void MainWindow::onPositionLinkTimeout()
{
    logEvent(QString::fromUtf8("MAVLink 链路超时：5 秒未收到遥测包，请检查飞控/模拟器"));
}

// ————————————————— IP 定位源（城市级兜底） —————————————————

void MainWindow::onLocateClicked()
{
    // 一键定位已下沉库：活位置流（10 秒内有喂点）直接居中，否则库内自动 IP 定位兜底（只居中不喂车）
    statusBar()->showMessage(QString::fromUtf8("定位中…（无实时位置时走 IP 兜底，约需数秒）"), 6000);
    m_map->LocateCurrentPosition();
}

void MainWindow::onIpLocationReady(opmap::PointLatLng pos, QString city)
{
    // 仅结果提示：喂入由库位置源管理完成（IP 作为活动位置源时），一键定位兜底只居中
    statusBar()->showMessage(QString::fromUtf8("IP 定位（城市级，精度约数公里）：%1 (%2, %3)")
                             .arg(city).arg(pos.Lat(), 0, 'f', 4).arg(pos.Lng(), 0, 'f', 4), 10000);
}

void MainWindow::onIpLocationFailed(QString reason)
{
    statusBar()->showMessage(QString::fromUtf8("IP 定位失败：%1").arg(reason), 8000);
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

// ————————————————— 库点选分发（防抖/多点累积/右键结束全在库内） —————————————————

/// 库 positionPicked：一次有效选点（mode 为 PickMode 枚举值）
void MainWindow::onPositionPicked(int mode, const opmap::PointLatLng &p)
{
    switch (mode) {
    case opmap::OPMapWidget::PickWaypoint:
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
    case opmap::OPMapWidget::PickOrigin: {
        m_origin = p;
        m_hasOrigin = true;
        // 起点/目的地图钉由库在 PlanRoute/NavigateTo 时自动挂载（auxiliary 装饰航点）
        m_originLabel->setText(QString::fromUtf8("起点：lat %1, lng %2")
                               .arg(p.Lat(), 0, 'f', 5).arg(p.Lng(), 0, 'f', 5));
        break;
    }
    case opmap::OPMapWidget::PickDest: {
        m_dest = p;
        m_hasDest = true;
        m_destLabel->setText(QString::fromUtf8("目的地：lat %1, lng %2")
                             .arg(p.Lat(), 0, 'f', 5).arg(p.Lng(), 0, 'f', 5));
        break;
    }
    case opmap::OPMapWidget::PickFence:
        // 围栏顶点逐点记录（预览与累积在库内），模式保持直到右键/按钮结束
        logEvent(QString::fromUtf8("围栏顶点: lat %1, lng %2")
                 .arg(p.Lat(), 0, 'f', 5).arg(p.Lng(), 0, 'f', 5));
        break;
    case opmap::OPMapWidget::PickPosition:
    {
        // 点选喂位置：点哪喂哪（所见即所得），模式保持可连续喂点
        m_map->UpdateVehiclePosition(p);   // 惰性创建 UAV（库默认大头针/每秒轨迹点）
        // 手动喂点是瞬移定位不是运动：清旧轨迹避免乱线；瞬移无航向意义复位竖直
        //（航向仅对连续运动流有意义，瞬移两点间随机方位角会让大头针歪斜）
        if (opmap::UAVItem *u = m_map->GetUAV(0)) {
            u->DeleteTrail();
            u->SetUAVHeading(0);
        }
        // 偏航计数可见化：连续 3 次偏 50m 外才触发重规划（防 GPS 抖动），中间点到路线 50m 内即清零
        QString tip = QString::fromUtf8("已喂入模拟位置 (%1, %2)")
                .arg(p.Lat(), 0, 'f', 4).arg(p.Lng(), 0, 'f', 4);
        if (m_map->IsNavigating())
            tip += QString::fromUtf8("，偏航计数 %1/3").arg(m_map->GetNavigationEngine()->OffRouteCount());
        logEvent(tip);
        statusBar()->showMessage(tip + QString::fromUtf8("，继续点击可连续喂点，右键/按钮结束"), 8000);
        break;
    }
    default:
        break;
    }
}

/// 库 pickFinished：取点结束（单发自动结束/右键/按钮），恢复交互状态
void MainWindow::onPickFinished(int mode, const QList<opmap::PointLatLng> &points)
{
    switch (mode) {
    case opmap::OPMapWidget::PickWaypoint:
        m_addWpBtn->setEnabled(true);
        break;
    case opmap::OPMapWidget::PickFence:
        // 库内已按顶点数固化（≥3）/清理预览（<3）
        if (m_map->HasGeofence()) {
            m_fenceBtn->setText(QString::fromUtf8("清除围栏"));
            logEvent(QString::fromUtf8("多边形地理围栏生效：%1 个顶点。配合航点飞行或行车模拟，飞机飞出红区即在日志报越界")
                     .arg(points.size()));
        } else {
            m_fenceBtn->setText(QString::fromUtf8("绘制围栏"));
            logEvent(QString::fromUtf8("围栏顶点不足 3 个，已取消"));
        }
        break;
    case opmap::OPMapWidget::PickMeasure:
        // 段固化后 measureFinished 已发；不足 2 点的丢弃段没有结果信号，此处兜底复位按钮
        m_measureBtn->setText(QString::fromUtf8("开始测距"));
        break;
    case opmap::OPMapWidget::PickPosition:
        m_mockPosBtn->setText(QString::fromUtf8("点选喂位置"));
        statusBar()->showMessage(QString::fromUtf8("已结束喂点"), 5000);
        break;
    default:
        break;
    }
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
