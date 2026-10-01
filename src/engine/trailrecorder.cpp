/**
******************************************************************************
*
* @file       trailrecorder.cpp
* @brief      运动轨迹记录器实现（记录/JSON 存取/时间轴变速回放）
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
      recording(false),
      replaying(false),
      replaySpeed(1.0),
      replayIdx(0)
{
    replayTimer = new QTimer(this);
    replayTimer->setInterval(kReplayIntervalMs);
    connect(replayTimer, SIGNAL(timeout()), this, SLOT(onReplayTick()));
}

// ———————— 记录 ————————

void TrailRecorder::StartRecording()
{
    if (recording)
        return;
    StopReplay();                       // 记录与回放互斥
    points.clear();
    recording = true;
    recordClock.start();
}

void TrailRecorder::StopRecording()
{
    recording = false;
}

void TrailRecorder::AddPoint(opmap::PointLatLng const& pos, int altM)
{
    if (!recording)
        return;
    TrailPoint p;
    p.lat = pos.Lat();
    p.lng = pos.Lng();
    p.altM = altM;
    p.tMs = recordClock.elapsed();
    points.append(p);
}

void TrailRecorder::Clear()
{
    StopReplay();
    recording = false;
    points.clear();
}

// ———————— JSON 文件 ————————

bool TrailRecorder::SaveToFile(const QString &path, QString *error) const
{
    if (points.isEmpty()) {
        if (error)
            *error = QString::fromUtf8("轨迹为空：未记录任何点（需先 StartTrailRecording 再喂点）");
        return false;
    }
    QJsonArray arr;
    for (int i = 0; i < points.size(); ++i) {
        const TrailPoint &p = points.at(i);
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
    root.insert(QLatin1String("created"),
                QDateTime::currentDateTime().toString(Qt::ISODate));
    root.insert(QLatin1String("count"), points.size());
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

bool TrailRecorder::LoadFromFile(const QString &path, QString *error)
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
    // 加载即接管缓冲：停掉进行中的记录/回放，避免状态错乱
    StopReplay();
    recording = false;
    points = loaded;
    return true;
}

// ———————— 回放 ————————

bool TrailRecorder::StartReplay(double speed)
{
    if (replaying)
        StopReplay();
    if (points.size() < 2)
        return false;
    if (speed <= 0.0)
        speed = 1.0;
    replaySpeed = speed;
    replayIdx = 0;
    recording = false;                  // 回放期间不再收记录点
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
    if (!replaying || points.isEmpty())
        return;
    // 回放时钟 × 倍速 = 轨迹时间轴位置（毫秒）
    const qint64 t = (qint64)(replayClock.elapsed() * replaySpeed);

    // 越过末点：补发末点位置并自然收尾
    if (t >= points.last().tMs) {
        replaying = false;
        replayTimer->stop();
        emit replayPosition(PointLatLng(points.last().lat, points.last().lng),
                            points.last().altM);
        emit replayFinished();
        return;
    }

    // 区间左端点单调推进（时间轴只前进，无需从头查找）
    while (replayIdx + 2 < points.size() && points.at(replayIdx + 1).tMs <= t)
        ++replayIdx;
    // t 早于首点（理论上首帧 elapsed≈0 即命中）：直接发首点
    if (t <= points.first().tMs) {
        emit replayPosition(PointLatLng(points.first().lat, points.first().lng),
                            points.first().altM);
        return;
    }

    // 相邻点线性插值（lat/lng/alt 三分量独立线性，短间隔下与球面插值差异可忽略）
    const TrailPoint &a = points.at(replayIdx);
    const TrailPoint &b = points.at(replayIdx + 1);
    const double span = (double)(b.tMs - a.tMs);
    const double f = span > 0.0 ? (double)(t - a.tMs) / span : 0.0;
    const double lat = a.lat + (b.lat - a.lat) * f;
    const double lng = a.lng + (b.lng - a.lng) * f;
    const int alt = (int)(a.altM + (b.altM - a.altM) * f + 0.5);
    emit replayPosition(PointLatLng(lat, lng), alt);
}

} // end of namespace opmap
