/**
******************************************************************************
*
* @file       positionsource.h
* @brief      统一位置源管理器：互斥管理 外部喂点 / 系统GPS / IP定位 / MAVLink遥测
*             四类位置源，位置点统一经 positionUpdated 信号对外分发。
*             切换源时先停用全部旧源再启用目标源（避免双源同时喂点），
*             源不可用（GPS 缺失 / UDP 端口占用）发 sourceError(…, true) 并停用
* @see        The GNU Public License (GPL) Version 3
* @{
*
*****************************************************************************/
#ifndef POSITIONSOURCE_H
#define POSITIONSOURCE_H

#include <QtCore/QObject>
#include <QtCore/QTimer>

#include "pointlatlng.h"
#include "iplocationprovider.h"
#include "mavlinktelemetryprovider.h"

class QGeoPositionInfoSource;
class QGeoPositionInfo;

namespace opmap {

/**
* @brief 位置源类型：库内互斥管理（切换前自动停用其它源），
*        位置点统一经 positionUpdated 信号分发并自动喂入 UpdateVehiclePosition 链路
*/
enum PositionSource
{
    SourceNone,        ///< 关闭位置源（不喂点）
    SourceExternal,    ///< 外部喂点：上层自行调 UpdateVehiclePosition/SetUAVPos 驱动
    SourceSystemGps,   ///< 系统 GPS（Qt Positioning 默认定位源）
    SourceIpLocation,  ///< IP 定位（城市级兜底，启用即取一次，之后 60s 轮询刷新）
    SourceMavlink      ///< MAVLink UDP 遥测（默认 14550，真机/SITL）
};

class PositionSourceManager : public QObject
{
    Q_OBJECT

public:
    explicit PositionSourceManager(QObject *parent = 0);
    ~PositionSourceManager();

    PositionSource source() const { return m_source; }   ///< 当前活动源

public slots:
    /// 切换位置源：先停用当前全部源再启用目标源（互斥）；启用失败回退 None 并报错
    void SetSource(PositionSource src);

signals:
    /// 统一位置流（altM 海拔米；headingDeg 正北 0 顺时针，无效为 -1）
    void positionUpdated(opmap::PointLatLng pos, double altM, double headingDeg, int source);
    /// 位置源错误（fatal=true：源无法启用已停用；false：单次失败源继续运行）
    void sourceError(QString reason, bool fatal);
    void linkAlive();      ///< MAVLink 链路建立/恢复
    void linkTimeout();    ///< MAVLink 链路超时（5 秒无包）

private slots:
    void onGpsUpdated(const QGeoPositionInfo &info);
    void onIpReady(opmap::PointLatLng pos, QString city);
    void onIpPoll();
    void onMavUpdated(double lat, double lon, double altM, double headingDeg);

private:
    void StopAll();   ///< 停用全部源（切换互斥的基础）

    PositionSource m_source;          ///< 当前活动源
    QGeoPositionInfoSource *m_gps;    ///< 系统 GPS（惰性创建，桌面无源时为空）
    IpLocationProvider *m_ip;         ///< IP 定位（城市级，本管理器私有实例）
    QTimer *m_ipTimer;                ///< IP 定位 60s 轮询定时器
    MavlinkTelemetryProvider *m_mav;  ///< MAVLink UDP 遥测
};

} // namespace opmap

#endif // POSITIONSOURCE_H
