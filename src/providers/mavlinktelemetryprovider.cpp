/**
******************************************************************************
*
* @file       mavlinktelemetryprovider.cpp
* @brief      MAVLink UDP 遥测解析实现：v1(0xFE)/v2(0xFD) 帧解析 + CRC 校验，
*             仅提取 GLOBAL_POSITION_INT（msgid 33）
* @see        The GNU Public License (GPL) Version 3
* @{
*
*****************************************************************************/
#include "mavlinktelemetryprovider.h"

#include <QtCore/QDateTime>

namespace opmap {

namespace {

const int kGlobalPositionInt = 33;    ///< GLOBAL_POSITION_INT 消息 ID
const quint8 kGlobalPositionIntCrcExtra = 104;  ///< 该消息的 CRC_EXTRA

/// MAVLink CRC16-CCITT (X.25)：poly 0x1021，初值 0xFFFF，结果异或 0xFFFF
quint16 crcAccumulate(quint16 crc, quint8 data)
{
    quint8 tmp = data ^ static_cast<quint8>(crc & 0xff);
    tmp ^= (tmp << 4);
    return static_cast<quint16>(((crc >> 8) ^ (static_cast<quint16>(tmp) << 8) ^
                                 (static_cast<quint16>(tmp) << 3) ^ (tmp >> 4)) & 0xffff);
}

quint16 crcFinalize(quint16 crc)
{
    return static_cast<quint16>(crc ^ 0xffff);
}

template <typename T>
T littleEndian(const QByteArray &data, int offset)
{
    T value = 0;
    for (uint i = 0; i < sizeof(T); ++i)
        value |= static_cast<T>(static_cast<quint8>(data.at(offset + i))) << (8 * i);
    return value;
}

} // anonymous namespace

MavlinkTelemetryProvider::MavlinkTelemetryProvider(QObject *parent)
    : QObject(parent),
      socket(new QUdpSocket(this)),
      hadPacket(false),
      lastPacketMs(0)
{
    connect(socket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    connect(socket, SIGNAL(readyRead()), this, SLOT(onLinkCheck()));
}

bool MavlinkTelemetryProvider::start(quint16 port)
{
    stop();
    hadPacket = false;
    lastPacketMs = 0;
    if (!socket->bind(QHostAddress::AnyIPv4, port, QUdpSocket::ShareAddress))
        return false;
    startTimer(1000);
    return true;
}

void MavlinkTelemetryProvider::stop()
{
    if (socket->state() == QAbstractSocket::BoundState)
        socket->close();
}

bool MavlinkTelemetryProvider::isListening() const
{
    return socket->state() == QAbstractSocket::BoundState;
}

void MavlinkTelemetryProvider::onReadyRead()
{
    while (socket->hasPendingDatagrams()) {
        QByteArray data;
        data.resize(int(socket->pendingDatagramSize()));
        socket->readDatagram(data.data(), data.size());
        parseDatagram(data);
    }
}

void MavlinkTelemetryProvider::onLinkCheck()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (lastPacketMs != 0 && now - lastPacketMs > 5000) {
        emit linkTimeout();
        lastPacketMs = 0;   // 只发一次，直到新包恢复
    }
}

void MavlinkTelemetryProvider::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event);
    onLinkCheck();
}

void MavlinkTelemetryProvider::parseDatagram(const QByteArray &data)
{
    for (int i = 0; i < data.size(); ++i) {
        const quint8 marker = static_cast<quint8>(data.at(i));
        if (marker != 0xFD && marker != 0xFE)   // 仅接受 MAVLink v1/v2 帧起始
            continue;

        const bool isV2 = (marker == 0xFD);
        const int headerLen = isV2 ? 10 : 6;    // STX 后的头部字段数（不含 STX）
        if (i + headerLen + 2 > data.size())    // 头 + payload(len) + CRC(2)
            continue;

        const quint8 payloadLen = static_cast<quint8>(data.at(i + 1));
        int msgId;
        int payloadOffset;
        if (isV2) {
            // v2: len, incompat, compat, seq, sysid, compid, msgid(3B LE)
            msgId = static_cast<quint8>(data.at(i + 7)) |
                    (static_cast<quint8>(data.at(i + 8)) << 8) |
                    (static_cast<quint8>(data.at(i + 9)) << 16);
            payloadOffset = i + 10;
        } else {
            // v1: len, seq, sysid, compid, msgid(1B)
            msgId = static_cast<quint8>(data.at(i + 5));
            payloadOffset = i + 6;
        }

        if (payloadOffset + payloadLen + 2 > data.size())
            continue;

        // CRC：对头部字段（v2 含 incompat/compat）+ payload + CRC_EXTRA 校验
        quint16 crc = 0xFFFF;
        for (int k = i + 1; k < i + headerLen; ++k)
            crc = crcAccumulate(crc, static_cast<quint8>(data.at(k)));
        for (int k = payloadOffset; k < payloadOffset + payloadLen; ++k)
            crc = crcAccumulate(crc, static_cast<quint8>(data.at(k)));
        crc = crcAccumulate(crc, kGlobalPositionIntCrcExtra);
        const quint16 wireCrc = littleEndian<quint16>(data, payloadOffset + payloadLen);
        if (crcFinalize(crc) != wireCrc)
            continue;   // 校验失败按噪声丢弃

        if (msgId != kGlobalPositionInt || payloadLen < 30) {
            i = payloadOffset + payloadLen + 1;   // 跳过本帧
            continue;
        }

        const qint32 latE7 = littleEndian<qint32>(data, payloadOffset);
        const qint32 lonE7 = littleEndian<qint32>(data, payloadOffset + 4);
        const qint32 altMm = littleEndian<qint32>(data, payloadOffset + 8);
        const quint16 hdgCdeg = littleEndian<quint16>(data, payloadOffset + 28);

        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (!hadPacket || lastPacketMs == 0) {
            hadPacket = true;
            emit linkAlive();
        }
        lastPacketMs = now;

        emit positionUpdated(latE7 / 1e7, lonE7 / 1e7, altMm / 1000.0,
                             hdgCdeg == 65535 ? -1.0 : hdgCdeg / 100.0);
        i = payloadOffset + payloadLen + 1;
    }
}

} // namespace opmap
