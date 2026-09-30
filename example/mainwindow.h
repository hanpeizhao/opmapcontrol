/**
******************************************************************************
*
* @file       mainwindow.h
* @brief      示例主窗口：地图浏览、航点管理、路径规划与导航模拟的面板组装
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
class RouteService;
class NavigationSimulator;
class QGraphicsPathItem;
class QGraphicsEllipseItem;

/**
* @brief 示例程序主窗口
*
* 只负责组装与信号转发：中央为 OPMapWidget 地图，右侧三个停靠面板
* （航点管理 / 路径规划 / 导航模拟），业务逻辑分别在
* WaypointStore、RouteService、NavigationSimulator 中。
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
    void rebuildRouteItems();   ///< 地图拖动/缩放/平移后重算路线绘制项坐标

    // 航点面板
    void onAddWaypointClicked();
    void onDeleteWaypointClicked();
    void onClearWaypointsClicked();
    void onImportWaypointsClicked();
    void onExportWaypointsClicked();
    void onWaypointListItemClicked(QListWidgetItem *item);

    // 路径规划面板
    void onSetOriginClicked();
    void onSetDestClicked();
    void onPlanClicked();
    void onFollowRouteClicked();
    void onRouteReady(const QList<opmap::PointLatLng> &pts, double meters, int seconds);
    void onRouteFailed(const QString &reason);

    // 导航模拟面板
    void onSimStartClicked();
    void onSimPauseClicked();
    void onSimStopClicked();
    void onSpeedChanged(int index);
    void onFollowToggled(bool on);
    void onTrailToggled(bool on);
    void onSimStatus(int current, int total, const QString &message);
    void onWaypointReached(int index);
    void onSimFinished();

    // 离线下载
    void onRipMapClicked();

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
    void clearRouteItems();
    opmap::UAVItem* ensureUAV();

    opmap::OPMapWidget *m_map;
    WaypointStore *m_store;
    RouteService *m_routeService;
    NavigationSimulator *m_simulator;

    // 航点面板
    QListWidget *m_wpList;
    QPushButton *m_addWpBtn;
    QPushButton *m_delWpBtn;

    // 路径规划面板
    QLabel *m_originLabel;
    QLabel *m_destLabel;
    QComboBox *m_providerCombo;
    QLineEdit *m_amapKeyEdit;
    QPushButton *m_planBtn;
    QPushButton *m_followRouteBtn;
    QLabel *m_routeInfo;

    // 导航模拟面板
    QPushButton *m_simStartBtn;
    QPushButton *m_simPauseBtn;
    QPushButton *m_simStopBtn;
    QComboBox *m_speedCombo;
    QCheckBox *m_followCheck;
    QCheckBox *m_trailCheck;
    QLabel *m_simInfo;

    // 状态栏
    QLabel *m_posLabel;
    QLabel *m_tileLabel;

    PickMode m_pickMode;
    opmap::PointLatLng m_origin;
    opmap::PointLatLng m_dest;
    bool m_hasOrigin;
    bool m_hasDest;
    QList<opmap::PointLatLng> m_routePts;
    double m_routeMeters;
    int m_routeSeconds;

    // 路线绘制 item（挂在地图书签场景，随地图变化重算）
    QGraphicsPathItem *m_routeItem;
    QGraphicsEllipseItem *m_originMarker;
    QGraphicsEllipseItem *m_destMarker;
};

#endif // MAINWINDOW_H
