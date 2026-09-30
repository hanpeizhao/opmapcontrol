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
#include <QtGui/QMouseEvent>

#include "opmapcontrol.h"

class WaypointStore;
class NavigationSimulator;
class QGeoPositionInfoSource;
class QGeoPositionInfo;
class QTimer;
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
    void onMapMousePress(QMouseEvent *event);
    void onMapMouseMove(QMouseEvent *event);
    void onZoomChanged(double zoomt, double zoom, double zoomd);
    void onTilesStill(int number);

    // 航点面板
    void onAddWaypointClicked();
    void onDeleteWaypointClicked();
    void onClearWaypointsClicked();
    void onImportWaypointsClicked();
    void onExportWaypointsClicked();
    void onWaypointListItemClicked(QListWidgetItem *item);

    // 导航面板
    void onPickOriginClicked();
    void onPickDestClicked();
    void onPlanClicked();
    void onNavigateClicked();
    void onStopNavClicked();
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
    void onSpeedChanged(int index);
    void onFollowToggled(bool on);
    void onTrailToggled(bool on);
    void onSimStatus(int current, int total, const QString &message);
    void onSimFinished();

    // 位置源（模拟 / 系统 GPS）
    void onPosSourceChanged(int index);
    void onGpsPositionUpdated(const QGeoPositionInfo &info);
    // IP 定位源（城市级兜底，桌面无 GPS 时仍能拿到大概位置）
    void onIpPollTimeout();         ///< 60s 轮询触发库请求 IP 定位
    // 库 IP 定位结果回调（OPMapWidget 的 ipLocationReady/ipLocationFailed 信号）
    void onIpLocationReady(opmap::PointLatLng pos, QString city);
    void onIpLocationFailed(QString reason);
    // 工具栏定位：优先用已喂入的车辆位置，否则自动走一次 IP 兜底
    void onLocateClicked();
    void CenterOnVehicle();

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
    void onWingmanTick();                 ///< 僚机绕飞动画（destPoint 几何演示）

protected:
    void resizeEvent(QResizeEvent *event);

private:
    enum PickMode
    {
        PickNone,
        PickWaypoint,
        PickOrigin,
        PickDest
    };

    void setupMenus();
    void setupDocks();
    void setupStatusBar();
    void applyPickPoint(const opmap::PointLatLng &p);
    void setPickMode(PickMode mode);
    void refreshWaypointList();
    opmap::UAVItem* ensureUAV();
    void applyProviderFromUI();   ///< 按面板选择创建/更新库内路由 provider
    void stopGps();               ///< 停止系统 GPS 位置源（若有）
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

    // 导航面板
    QLabel *m_originLabel;
    QLabel *m_destLabel;
    QComboBox *m_providerCombo;
    QLineEdit *m_amapKeyEdit;
    QPushButton *m_planBtn;
    QPushButton *m_navBtn;
    QPushButton *m_stopNavBtn;
    QLabel *m_navInfo;

    // 行车模拟面板
    QPushButton *m_simStartBtn;
    QPushButton *m_simPauseBtn;
    QPushButton *m_simStopBtn;
    QPushButton *m_yawBtn;
    QComboBox *m_speedCombo;
    QComboBox *m_posSourceCombo;     ///< 位置源：模拟 / 系统 GPS / IP 定位
    QGeoPositionInfoSource *m_gpsSource;   ///< 系统 GPS 源（惰性创建，可能为空）
    QTimer *m_ipTimer;                     ///< IP 定位轮询定时器（触发库的 RequestIpLocation）
    bool m_locatePending;                  ///< 定位按钮触发的 IP 兜底进行中
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
    QTimer *m_wingmanTimer;          ///< 僚机绕飞动画（多机 UAV 演示）
    double m_wingmanAngle;           ///< 僚机当前方位角（度）
    int m_wingmanId;                 ///< 僚机 UAV id
    int m_lastDlPct;                 ///< 下载日志节流（上个 10% 档位）

    PickMode m_pickMode;
    opmap::PointLatLng m_origin;     ///< 点选的起点（缺省用当前位置）
    opmap::PointLatLng m_dest;
    bool m_hasOrigin;
    bool m_hasDest;
    bool m_providerIsAmap;       ///< 当前库内 provider 是否为高德
    opmap::Route m_navRoute;     ///< 最近一次导航路线（喂跟车模拟器）
};

#endif // MAINWINDOW_H
