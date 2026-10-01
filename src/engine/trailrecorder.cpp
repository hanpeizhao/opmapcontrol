/**
******************************************************************************
*
* @file       trailrecorder.cpp
* @brief      运动轨迹记录器实现（按机分道记录/JSON 存取/时间轴变速回放）
* @see        The GNU Public License (GPL) Version 3
* @{
*
*****************************************************************************/
/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/
#include "trailrecorder.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QFile>

namespace opmap {

namespace {
const char *kFormatId = "opmap-trail";   ///< 文件格式标识
const int kVersion = 1;                  ///< 文件格式版本
const int kReplayIntervalMs = 50;        ///< 回放 tick 周期
}

TrailRecorder::TrailRecorder(QObject *parent)
    : QObject(parent),
      replayUavId(0),
      replaying(false),
      replaySpeed(1.0),
      replayIdx(0)
{
    replayTimer = new QTimer(this);
    replayTimer->setInterval(kReplayIntervalMs);
    connect(replayTimer, SIGNAL(timeout()), this, SLOT(onReplayTick()));
}

// ———————— 通道访问 ————————

const TrailRecorder::Channel &TrailRecorder::channelOf(int uavId) const
{
    static const Channel emptyChannel;
    // 注意：必须用 constFind 取 map 内对象的左值引用；若写 channels.value(uavId)
    // 会返回值拷贝临时，跨 return 的临时不延长生命周期 → 调用方拿悬空引用
    QMap<int, Channel>::const_iterator it = channels.constFind(uavId);
    return it != channels.constEnd() ? it.value() : emptyChannel;
}

TrailRecorder::Channel &TrailRecorder::channelRef(int uavId)
{
    return channels[uavId];
}

// ———————— 记录（按机分道）———————

void TrailRecorder::StartRecording(int uavId)
{
    StopReplay();                       // 记录与回放互斥（不影响其他道记录）
    Channel &ch = channelRef(uavId);
    ch.points.clear();                  // 该道清空重启；其他道记录继续
    ch.recording = true;
    ch.recordClock.start();
}

void TrailRecorder::StopRecording(int uavId)
{
    if (channels.contains(uavId))
        channels[uavId].recording = false;
}

void TrailRecorder::AddPoint(int uavId, opmap::PointLatLng const& pos, int altM)
{
    QMap<int, Channel>::iterator it = channels.find(uavId);
    if (it == channels.end() || !it.value().recording)
        return;
    TrailPoint p;
    p.lat = pos.Lat();
    p.lng = pos.Lng();
    p.altM = altM;
    p.tMs = it.value().recordClock.elapsed();
    it.value().points.append(p);
}

void TrailRecorder::Clear()
{
    StopReplay();
    channels.clear();
}

// ———————— JSON 文件 ————————

bool TrailRecorder::SaveToFile(const QString &path, QString *error, int uavId) const
{
    const QList<TrailPoint> &pts = channelOf(uavId).points;
    if (pts.isEmpty()) {
        if (error)
            *error = QString::fromUtf8("轨迹为空：%1 号机未记录任何点（需先 StartTrailRecording(%1) 再喂点）").arg(uavId);
        return false;
    }
    QJsonArray arr;
    for (int i = 0; i < pts.size(); ++i) {
        const TrailPoint &p = pts.at(i);
        QJsonObject o;
        o.insert(QLatin1String("t"), (double)p.tMs);
        o.insert(QLatin1String("lat"), p.lat);
        o.insert(QLatin1String("lng"), p.lng);
        o.insert(QLatin1String("alt"), p.altM);
        arr.append(o);
    }
    QJsonObject root;
    root.insert(QLatin1String("format"), QLatin1String(kFormatId));
    root.insert(QLatin1String("version"), kVersion);
    root.insert(QLatin1String("uavId"), uavId);   // 来源机标注（加载目标由参数决定）
    root.insert(QLatin1String("created"),
                QDateTime::currentDateTime().toString(Qt::ISODate));
    root.insert(QLatin1String("count"), pts.size());
    root.insert(QLatin1String("points"), arr);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = QString::fromUtf8("无法写入文件：%1").arg(path);
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool TrailRecorder::LoadFromFile(const QString &path, QString *error, int uavId)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QString::fromUtf8("无法读取文件：%1").arg(path);
        return false;
    }
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseErr);
    file.close();
    if (doc.isNull() || !doc.isObject()) {
        if (error)
            *error = QString::fromUtf8("JSON 解析失败（%1）").arg(parseErr.errorString());
        return false;
    }
    const QJsonObject root = doc.object();
    if (root.value(QLatin1String("format")).toString() != QLatin1String(kFormatId)) {
        if (error)
            *error = QString::fromUtf8("不是 opmap-trail 格式的轨迹文件");
        return false;
    }
    const QJsonArray arr = root.value(QLatin1String("points")).toArray();
    QList<TrailPoint> loaded;
    for (int i = 0; i < arr.size(); ++i) {
        const QJsonObject o = arr.at(i).toObject();
        TrailPoint p;
        p.tMs = (qint64)o.value(QLatin1String("t")).toDouble();
        p.lat = o.value(QLatin1String("lat")).toDouble();
        p.lng = o.value(QLatin1String("lng")).toDouble();
        p.altM = o.value(QLatin1String("alt")).toInt();
        loaded.append(p);
    }
    // 加载即接管指定道：停回放（全局互斥），该道记录停止并装入数据，
    // 其他道的记录不受影响
    StopReplay();
    Channel &ch = channelRef(uavId);
    ch.recording = false;
    ch.points = loaded;
    return true;
}

// ———————— 回放 ————————

bool TrailRecorder::StartReplay(double speed, int uavId)
{
    if (replaying)
        StopReplay();
    // 快照该道数据：回放期间该道继续记录不影响回放
    replayBuf = channelOf(uavId).points;
    if (replayBuf.size() < 2)
        return false;
    if (speed <= 0.0)
        speed = 1.0;
    replaySpeed = speed;
    replayIdx = 0;
    replayUavId = uavId;
    // 回放期间停掉所有道的记录（全局互斥，避免回放喂点与实时喂点混流）
    for (QMap<int, Channel>::iterator it = channels.begin(); it != channels.end(); ++it)
        it.value().recording = false;
    replaying = true;
    replayClock.start();
    replayTimer->start();
    return true;
}

void TrailRecorder::StopReplay()
{
    if (!replaying)
        return;
    replaying = false;
    replayTimer->stop();
}

void TrailRecorder::onReplayTick()
{
    if (!replaying || replayBuf.isEmpty())
        return;
    // 回放时钟 × 倍速 = 轨迹时间轴位置（毫秒）
    const qint64 t = (qint64)(replayClock.elapsed() * replaySpeed);

    // 越过末点：补发末点位置并自然收尾
    if (t >= replayBuf.last().tMs) {
        replaying = false;
        replayTimer->stop();
        emit replayPosition(PointLatLng(replayBuf.last().lat, replayBuf.last().lng),
                            replayBuf.last().altM, replayUavId);
        emit replayFinished();
        return;
    }

    // 区间左端点单调推进（时间轴只前进，无需从头查找）
    while (replayIdx + 2 < replayBuf.size() && replayBuf.at(replayIdx + 1).tMs <= t)
        ++replayIdx;
    // t 早于首点（理论上首帧 elapsed≈0 即命中）：直接发首点
    if (t <= replayBuf.first().tMs) {
        emit replayPosition(PointLatLng(replayBuf.first().lat, replayBuf.first().lng),
                            replayBuf.first().altM, replayUavId);
        return;
    }

    // 相邻点线性插值（lat/lng/alt 三分量独立线性，短间隔下与球面插值差异可忽略）
    const TrailPoint &a = replayBuf.at(replayIdx);
    const TrailPoint &b = replayBuf.at(replayIdx + 1);
    const double span = (double)(b.tMs - a.tMs);
    const double f = span > 0.0 ? (double)(t - a.tMs) / span : 0.0;
    const double lat = a.lat + (b.lat - a.lat) * f;
    const double lng = a.lng + (b.lng - a.lng) * f;
    const int alt = (int)(a.altM + (b.altM - a.altM) * f + 0.5);
    emit replayPosition(PointLatLng(lat, lng), alt, replayUavId);
}

} // end of namespace opmap
