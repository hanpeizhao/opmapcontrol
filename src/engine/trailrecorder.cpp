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
    : QObject(parent)
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
    StopReplayOf(uavId);                // 按道互斥：该道回放中则先停（其他道不受影响）
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

bool TrailRecorder::ParseTrailFile(const QString &path, QString *error,
                                   QList<TrailPoint> &out, int &uavIdOut)
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
    uavIdOut = root.value(QLatin1String("uavId")).toInt();   // 无标注默认 0 道
    const QJsonArray arr = root.value(QLatin1String("points")).toArray();
    out.clear();
    for (int i = 0; i < arr.size(); ++i) {
        const QJsonObject o = arr.at(i).toObject();
        TrailPoint p;
        p.tMs = (qint64)o.value(QLatin1String("t")).toDouble();
        p.lat = o.value(QLatin1String("lat")).toDouble();
        p.lng = o.value(QLatin1String("lng")).toDouble();
        p.altM = o.value(QLatin1String("alt")).toInt();
        out.append(p);
    }
    return true;
}

bool TrailRecorder::LoadFromFile(const QString &path, QString *error, int uavId)
{
    QList<TrailPoint> loaded;
    int fileUavId = 0;
    if (!ParseTrailFile(path, error, loaded, fileUavId))
        return false;
    // 接管指定道：停该道回放、该道记录停止并装入数据，其他道不受影响
    StopReplayOf(uavId);
    Channel &ch = channelRef(uavId);
    ch.recording = false;
    ch.points = loaded;
    return true;
}

int TrailRecorder::LoadFromFileAuto(const QString &path, QString *error)
{
    QList<TrailPoint> loaded;
    int fileUavId = 0;
    if (!ParseTrailFile(path, error, loaded, fileUavId))
        return -1;
    // 按文件内机号标注自动归道（保存时的来源机即回放时的目标机）
    StopReplayOf(fileUavId);
    Channel &ch = channelRef(fileUavId);
    ch.recording = false;
    ch.points = loaded;
    return fileUavId;
}

// ———————— 回放（按道并行）———————

bool TrailRecorder::StartReplay(double speed, int uavId)
{
    if (channelOf(uavId).points.size() < 2)
        return false;
    if (speed <= 0.0)
        speed = 1.0;
    StopReplayOf(uavId);                  // 同道重启=从头再来（其他道回放继续）
    channels[uavId].recording = false;    // 该道记录停止：回放喂点经 SetUAVPos 回到本道，停记录避免混流
    ReplayRun run;
    run.buf = channelOf(uavId).points;    // 快照：回放期间外部改动该道数据不影响本次回放
    run.speed = speed;
    run.idx = 0;
    run.clock.start();
    replayRuns.insert(uavId, run);
    replayTimer->start();
    return true;
}

void TrailRecorder::StopReplay()
{
    if (replayRuns.isEmpty())
        return;
    replayRuns.clear();
    replayTimer->stop();
}

void TrailRecorder::StopReplayOf(int uavId)
{
    if (replayRuns.remove(uavId) > 0 && replayRuns.isEmpty())
        replayTimer->stop();
}

void TrailRecorder::onReplayTick()
{
    QList<int> finished;
    for (QMap<int, ReplayRun>::iterator it = replayRuns.begin(); it != replayRuns.end(); ++it) {
        ReplayRun &run = it.value();
        if (run.buf.isEmpty())
            continue;
        // 该道回放时钟 × 倍速 = 轨迹时间轴位置（毫秒）
        const qint64 t = (qint64)(run.clock.elapsed() * run.speed);

        // 越过末点：补发末点位置，该道收尾（全部道播完才发 replayFinished）
        if (t >= run.buf.last().tMs) {
            emit replayPosition(PointLatLng(run.buf.last().lat, run.buf.last().lng),
                                run.buf.last().altM, it.key());
            finished.append(it.key());
            continue;
        }

        // 区间左端点单调推进（时间轴只前进，无需从头查找）
        while (run.idx + 2 < run.buf.size() && run.buf.at(run.idx + 1).tMs <= t)
            ++run.idx;
        // t 早于首点（理论上首帧 elapsed≈0 即命中）：直接发首点
        if (t <= run.buf.first().tMs) {
            emit replayPosition(PointLatLng(run.buf.first().lat, run.buf.first().lng),
                                run.buf.first().altM, it.key());
            continue;
        }

        // 相邻点线性插值（lat/lng/alt 三分量独立线性，短间隔下与球面插值差异可忽略）
        const TrailPoint &a = run.buf.at(run.idx);
        const TrailPoint &b = run.buf.at(run.idx + 1);
        const double span = (double)(b.tMs - a.tMs);
        const double f = span > 0.0 ? (double)(t - a.tMs) / span : 0.0;
        emit replayPosition(PointLatLng(a.lat + (b.lat - a.lat) * f,
                                        a.lng + (b.lng - a.lng) * f),
                            (int)(a.altM + (b.altM - a.altM) * f + 0.5), it.key());
    }
    for (int i = 0; i < finished.size(); ++i)
        replayRuns.remove(finished.at(i));
    if (!finished.isEmpty() && replayRuns.isEmpty()) {
        replayTimer->stop();
        emit replayFinished();   // 最后一道播完才发（单道回放语义与旧版一致）
    }
}

} // end of namespace opmap
