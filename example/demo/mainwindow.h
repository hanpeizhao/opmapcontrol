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
    void onPickDestClicked();
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

    // 离线下载
    void onRipMapClicked();

protected:
    void resizeEvent(QResizeEvent *event);

private:
    enum PickMode
    {
        PickNone,
        PickWaypoint,
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
    void setBanner(const QString &headline, const QString &subText, const QString &bgColor);
    void repositionBanner();

    opmap::OPMapWidget *m_map;
    WaypointStore *m_store;
    NavigationSimulator *m_simulator;

    // 航点面板
    QListWidget *m_wpList;
    QPushButton *m_addWpBtn;
    QPushButton *m_delWpBtn;

    // 导航面板
    QLabel *m_destLabel;
    QComboBox *m_providerCombo;
    QLineEdit *m_amapKeyEdit;
    QPushButton *m_navBtn;
    QPushButton *m_stopNavBtn;
    QLabel *m_navInfo;

    // 行车模拟面板
    QPushButton *m_simStartBtn;
    QPushButton *m_simPauseBtn;
    QPushButton *m_simStopBtn;
    QPushButton *m_yawBtn;
    QComboBox *m_speedCombo;
    QCheckBox *m_followCheck;
    QCheckBox *m_trailCheck;
    QLabel *m_simInfo;

    // 指令横幅（地图底部叠加）
    QLabel *m_banner;

    // 状态栏
    QLabel *m_posLabel;
    QLabel *m_tileLabel;

    PickMode m_pickMode;
    opmap::PointLatLng m_dest;
    bool m_hasDest;
    bool m_providerIsAmap;       ///< 当前库内 provider 是否为高德
    opmap::Route m_navRoute;     ///< 最近一次导航路线（喂跟车模拟器）
};

#endif // MAINWINDOW_H
