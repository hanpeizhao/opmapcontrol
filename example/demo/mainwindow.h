/**
******************************************************************************
*
* @file       mainwindow.h
* @brief      示例主窗口：地图浏览、航点管理与车载导航（库能力）的面板组装
* @see        The GNU Public License (GPL) Version 3
* @{
*
*****************************************************************************/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QPushButton>
#include <QtCore/QTimer>
#include <QtCore/QHash>
#include <QtCore/QVector>
#include <QtGui/QMouseEvent>

#include "opmapcontrol.h"

class WaypointStore;
class NavigationSimulator;
class WaypointFlightSimulator;
class QAction;

/**
* @brief 示例程序主窗口（车载导航式调用库）
*
* 中央为 OPMapWidget 地图；右侧停靠面板：航点管理 / 车载导航 / 行车模拟。
* 路径规划、沿路指引、偏航重规划、路线绘制全部来自库（NavigateTo /
* UpdateVehiclePosition + navigation* 信号）；本窗口只做面板组装与横幅展示，
* 另以 NavigationSimulator 模拟行车喂点。
*/
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();

private slots:
    // 地图交互
    void onMapSourceTriggered();
    void onMapMouseMove(QMouseEvent *event);
    // 库点选信号：取点/结束由库防抖分发（航点/起终点/围栏/喂位置共用）
    void onPositionPicked(int mode, const opmap::PointLatLng &p);
    void onPickFinished(int mode, const QList<opmap::PointLatLng> &points);
    void onZoomChanged(double zoomt, double zoom, double zoomd);
    void onTilesStill(int number);

    // 航点面板
    void onAddWaypointClicked();
    void onDeleteWaypointClicked();
    void onClearWaypointsClicked();
    void onImportWaypointsClicked();
    void onExportWaypointsClicked();
    void onFlightClicked();   ///< 航点飞行：模拟遥测沿航点序列飞（开始/停止二态）
    void onFenceClicked();    ///< 多边形围栏：取点 → 结束闭合 → 清除 三态
    void onWaypointListItemClicked(QListWidgetItem *item);

    // 导航面板
    void onPickOriginClicked();
    void onPickDestClicked();
    void onPlanClicked();
    void onNavigateClicked();
    void onStopNavClicked();
    void onRouteAlternativesReady(QList<opmap::Route> routes);   ///< 备选路线就绪：刷新切换按钮
    void onSwitchRouteClicked();                                 ///< 循环切换备选路线
    void onProviderChanged(int index);
    void onNavigationRouteReady(const opmap::Route &route);
    void onNavProgress(double remainingMeters, int remainingSeconds, const QString &instruction);
    void onOffRouteDetected(const opmap::PointLatLng &pos, double deviationMeters);
    void onRerouteReady(const opmap::Route &route);
    void onNavigationArrived();
    void onNavigationFailed(const QString &reason);

    // 行车模拟面板
    void onSimStartClicked();
    void onSimPauseClicked();
    void onSimStopClicked();
    void onYawClicked();
    void onMockPosClicked();
    void onSpeedChanged(int index);
    void onFollowToggled(bool on);
    void onMapFollowChanged(bool following);   ///< 库跟随开关变化 → 复选框同步
    void onTrailToggled(bool on);
    void onSimStatus(int current, int total, const QString &message);
    void onSimFinished();

    // 位置源（模拟 / 系统 GPS / IP / MAVLink）：启停互斥与轮询全在库内，
    // demo 只做下拉切换、错误回退与链路状态日志
    void onPosSourceChanged(int index);
    void onPosSourceError(const QString &reason, bool fatal);
    void onPositionLinkAlive();
    void onPositionLinkTimeout();
    // 库 IP 定位结果回调（OPMapWidget 的 ipLocationReady/ipLocationFailed 信号，仅提示）
    void onIpLocationReady(opmap::PointLatLng pos, QString city);
    void onIpLocationFailed(QString reason);
    // 工具栏定位：库内一键定位（有车辆位置直接居中，无则 IP 兜底只居中）
    void onLocateClicked();

    // 多人共享位置演示：成员上线创建标记，模拟报文驱动 SetCoord 实时移动
    void onPeersDemoToggled(bool on);
    void onPeerTick();

    // 地图右键菜单：切换地图源 / 航点增删
    void onMapContextMenu(const QPoint &pos);
    void SyncMapTypeActions();   ///< 菜单栏与右键菜单的地图源勾选状态同步

    // 离线下载
    void onRipMapClicked();

    // —— 库能力示范（控件回调统一 lambda 连接，故不占 slots 区）——
    void setupCapabilityDock();     ///< 库能力演示面板：视图控制 / 缓存与访问 / 多机与几何
    void setupEventLogDock();       ///< 事件日志面板：订阅库信号
    void connectEventLog();         ///< 库信号 → 日志面板（高频信号除外）
    void logEvent(const QString &text);   ///< 日志统一入口（时间戳 + 限长）
    void refreshNavState();               ///< 导航状态行（IsNavigating / 路线摘要）
    void onInsertWaypointClicked();       ///< 前两航点中点插入（WPInsert 演示）
    void onRenumberClicked();             ///< 选中航点移至末尾（WPRenumber 演示）

protected:
    void resizeEvent(QResizeEvent *event);

private:
    void setupMenus();
    void setupDocks();
    void setupStatusBar();
    void refreshWaypointList();
    void applyProviderFromUI();   ///< 按面板选择创建/更新库内路由 provider
    void setBanner(const QString &headline, const QString &subText, const QString &bgColor);
    void repositionBanner();

    opmap::OPMapWidget *m_map;
    QList<QAction*> m_mapTypeActions;   ///< 地图源菜单动作（右键菜单与菜单栏共用）
    WaypointStore *m_store;
    NavigationSimulator *m_simulator;

    // 航点面板
    QListWidget *m_wpList;
    QPushButton *m_addWpBtn;
    QPushButton *m_delWpBtn;
    QPushButton *m_flightBtn;               ///< 航点飞行开始/停止按钮
    WaypointFlightSimulator *m_flightSim;   ///< 航点飞行模拟数据源（真机接入时替换为遥测）
    QComboBox *m_wpActionCombo;             ///< 下一个航点的到达动作（无/拍照/悬停30s）

    // 导航面板
    QLabel *m_originLabel;
    QLabel *m_destLabel;
    QComboBox *m_providerCombo;
    QLineEdit *m_amapKeyEdit;
    QPushButton *m_planBtn;
    QPushButton *m_navBtn;
    QPushButton *m_stopNavBtn;
    QPushButton *m_routeSwitchBtn;         ///< 备选路线循环切换（规划出 ≥2 条时可用）
    QList<opmap::Route> m_altRoutes;       ///< 最近规划的备选路线（切换显示用）
    int m_altIndex;                        ///< 当前选中备选索引
    QLabel *m_navInfo;

    // 行车模拟面板
    QPushButton *m_simStartBtn;
    QPushButton *m_simPauseBtn;
    QPushButton *m_simStopBtn;
    QPushButton *m_yawBtn;
    QPushButton *m_mockPosBtn;   ///< 喂模拟位置：在当前视野内随机取点手动喂 vehiclePos
    QComboBox *m_speedCombo;
    QComboBox *m_posSourceCombo;     ///< 位置源：模拟 / 系统 GPS / IP 定位 / MAVLink
    QCheckBox *m_followCheck;
    QCheckBox *m_trailCheck;
    QLabel *m_simInfo;

    // 指令横幅（地图底部叠加）
    QLabel *m_banner;

    // 状态栏
    QLabel *m_posLabel;
    QLabel *m_tileLabel;

    // 库能力示范
    QListWidget *m_eventLog;         ///< 库事件日志面板
    QLabel *m_navStateLabel;         ///< 导航状态行（导航中/路线摘要/请求状态）
    int m_lastDlPct;                 ///< 下载日志节流（上个 10% 档位）
    QPushButton *m_fenceBtn;         ///< 多边形围栏三态按钮
    int m_flightSpeedMps;            ///< 航点飞行巡航速度（m/s，右键菜单可调）

    opmap::PointLatLng m_origin;     ///< 点选的起点（缺省用当前位置）
    opmap::PointLatLng m_dest;
    bool m_hasOrigin;
    bool m_hasDest;
    bool m_providerIsAmap;       ///< 当前库内 provider 是否为高德
    opmap::Route m_navRoute;     ///< 最近一次导航路线（喂跟车模拟器）

    // 多人共享位置演示
    struct PeerSim {             ///< 模拟成员运动参数（绕圆心匀速转圈）
        QString name;
        double cLat, cLng;       // 圆心
        double rLat, rLng;       // 转圈半径（度）
        double phase;            // 初始相位
    };
    QVector<PeerSim> m_peerSims;                            ///< 成员运动参数表
    QHash<QString, opmap::MapMarkerItem *> m_peerMarkers;   ///< 成员名 → 地图标记
    QTimer *m_peerTimer;         ///< 模拟位置报文定时器
    double m_peerAngle;          ///< 演示公转角（rad，随 tick 递增）
};

#endif // MAINWINDOW_H
