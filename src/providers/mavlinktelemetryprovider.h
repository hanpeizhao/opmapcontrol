/**
******************************************************************************
*
* @file       mavlinktelemetryprovider.h
* @brief      MAVLink UDP 遥测数据源：监听标准地面站端口（默认 14550），
*             解析 GLOBAL_POSITION_INT 消息得到经纬度/高度/航向。
*             真机接入：飞控数传或 SITL 仿真器向本端口发 MAVLink v1/v2 包即可，
*             本类只做"链路 → 信号"的转换，喂地图走 SetUAVPos 链路不变
* @see        The GNU Public License (GPL) Version 3
* @{
*
*****************************************************************************/
#ifndef MAVLINKTELEMETRYPROVIDER_H
#define MAVLINKTELEMETRYPROVIDER_H

#include <QtCore/QObject>
#include <QtNetwork/QUdpSocket>

namespace opmap {

class MavlinkTelemetryProvider : public QObject
{
    Q_OBJECT

public:
    explicit MavlinkTelemetryProvider(QObject *parent = 0);

    /// 开始监听（默认 14550）；重复调用会先关闭旧 socket
    bool start(quint16 port = 14550);
    void stop();
    bool isListening() const;

signals:
    /// GLOBAL_POSITION_INT 解析结果：纬度/经度（度）、海拔（米）、航向（度，正北 0 顺时针；无效为 -1）
    void positionUpdated(double lat, double lon, double altM, double headingDeg);
    void linkAlive();      ///< 收到首包或断流恢复
    void linkTimeout();    ///< 超过 5 秒无任何包

private slots:
    void onReadyRead();
    void onLinkCheck();

protected:
    void timerEvent(QTimerEvent *event);

private:
    /// 从缓冲中按 MAVLink v1/v2 帧格式解析，命中 GLOBAL_POSITION_INT 则发信号
    void parseDatagram(const QByteArray &data);

    QUdpSocket *socket;
    bool hadPacket;          ///< 已收到过包（用于 linkAlive 只发一次）
    qint64 lastPacketMs;     ///< 上一包时间戳（linkTimeout 判定）
};

} // namespace opmap

#endif // MAVLINKTELEMETRYPROVIDER_H
