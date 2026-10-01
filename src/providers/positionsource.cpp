/**
******************************************************************************
*
* @file       positionsource.cpp
* @brief      统一位置源管理器实现（见 positionsource.h）
* @see        The GNU Public License (GPL) Version 3
* @{
*
*****************************************************************************/
#include "positionsource.h"

#include <QtPositioning/QGeoPositionInfoSource>
#include <QtPositioning/QGeoPositionInfo>
#include <QtPositioning/QGeoCoordinate>

namespace opmap {

PositionSourceManager::PositionSourceManager(QObject *parent) :
    QObject(parent),
    m_source(SourceNone),
    m_gps(0),
    m_ip(new IpLocationProvider(this)),
    m_ipTimer(new QTimer(this)),
    m_mav(new MavlinkTelemetryProvider(this))
{
    m_ipTimer->setInterval(60000);
    connect(m_ipTimer, SIGNAL(timeout()), this, SLOT(onIpPoll()));
    connect(m_ip, SIGNAL(locationReady(opmap::PointLatLng,QString)),
            this, SLOT(onIpReady(opmap::PointLatLng,QString)));
    connect(m_mav, SIGNAL(positionUpdated(double,double,double,double)),
            this, SLOT(onMavUpdated(double,double,double,double)));
    connect(m_mav, SIGNAL(linkAlive()), this, SIGNAL(linkAlive()));
    connect(m_mav, SIGNAL(linkTimeout()), this, SIGNAL(linkTimeout()));
}

PositionSourceManager::~PositionSourceManager()
{
    StopAll();
}

void PositionSourceManager::SetSource(PositionSource src)
{
    StopAll();          // 互斥：先停掉全部旧源
    m_source = src;

    switch (src) {
    case SourceNone:
    case SourceExternal:
        break;   // 外部喂点由上层经 UpdateVehiclePosition 自行驱动

    case SourceSystemGps: {
        if (!m_gps) {
            // Windows 桌面默认 serialnmea 后端，无可用定位源时返回空指针
            m_gps = QGeoPositionInfoSource::createDefaultSource(this);
            if (!m_gps) {
                m_source = SourceNone;
                emit sourceError(QString::fromUtf8("未找到可用的系统定位源"), true);
                return;
            }
            connect(m_gps, SIGNAL(positionUpdated(QGeoPositionInfo)),
                    this, SLOT(onGpsUpdated(QGeoPositionInfo)));
        }
        m_gps->startUpdates();
        break;
    }

    case SourceIpLocation:
        m_ip->requestLocation();   // 启用即取一次，之后 60s 轮询
        m_ipTimer->start();
        break;

    case SourceMavlink:
        if (!m_mav->start(14550)) {
            m_source = SourceNone;
            emit sourceError(QString::fromUtf8("MAVLink UDP 14550 端口监听失败（可能被占用）"), true);
            return;
        }
        break;
    }
}

void PositionSourceManager::StopAll()
{
    m_ipTimer->stop();
    if (m_gps)
        m_gps->stopUpdates();
    if (m_mav->isListening())
        m_mav->stop();
}

void PositionSourceManager::onGpsUpdated(const QGeoPositionInfo &info)
{
    if (!info.isValid())
        return;
    const QGeoCoordinate c = info.coordinate();
    // QGeoCoordinate 为 (纬度, 经度)，与 PointLatLng(Lat, Lng) 顺序一致
    const double alt = (c.type() == QGeoCoordinate::Coordinate3D) ? c.altitude() : 0.0;
    emit positionUpdated(PointLatLng(c.latitude(), c.longitude()), alt, -1.0, SourceSystemGps);
}

void PositionSourceManager::onIpReady(opmap::PointLatLng pos, QString city)
{
    Q_UNUSED(city);
    // 只分发本源活动期间的结果；一键定位等外部请求的兜底不由位置源分发
    if (m_source != SourceIpLocation)
        return;
    emit positionUpdated(pos, 0.0, -1.0, SourceIpLocation);
}

void PositionSourceManager::onIpPoll()
{
    m_ip->requestLocation();   // 单次失败不断源（下一轮重试），错误经 facade ipLocationFailed 提示
}

void PositionSourceManager::onMavUpdated(double lat, double lon, double altM, double headingDeg)
{
    if (m_source != SourceMavlink)
        return;
    emit positionUpdated(PointLatLng(lat, lon), altM, headingDeg, SourceMavlink);
}

} // namespace opmap
