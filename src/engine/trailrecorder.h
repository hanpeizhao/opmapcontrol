/**
******************************************************************************
*
* @file       trailrecorder.h
* @brief      运动轨迹记录器：按机分道记录 位置+高度+相对毫秒，
*             JSON 文件存取，内置按时间轴变速回放（QTimer 插值喂点）
* @see        The GNU Public License (GPL) Version 3
* @defgroup   OPMapWidget
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
#ifndef TRAILRECORDER_H
#define TRAILRECORDER_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QElapsedTimer>
#include <QTimer>
#include <QString>
#include "pointlatlng.h"

namespace opmap {

/**
* @brief 轨迹记录/回放引擎（纯喂点驱动，不含任何图元/UI 依赖）。
*
*        多机分道：每架 UAV（uavId）独立一道记录缓冲与时间轴，
*        多机同时记录互不混流；单机使用传默认 id=0 即可。
*
*        记录：StartRecording(uavId) 后每次 AddPoint(uavId, ...) 记
*        (lat, lng, alt, t)，t 为相对该道记录起点的毫秒数——保留原始
*        时序，回放即按 t 轴推进。
*
*        文件：JSON（opmap-trail 格式），人类可读、向后兼容易扩展：
*        { "format":"opmap-trail", "version":1, "uavId":0, "created":"...",
*          "points":[ {"t":0,"lat":34.26,"lng":108.94,"alt":100}, ... ] }
*        加载目标由参数 uavId 决定（文件内 uavId 仅为来源标注）。
*
*        回放：QTimer 50ms tick 按真实时间轴 × 倍速推进，相邻点间线性
*        插值出位置经 replayPosition 信号发出（携带 uavId，facade 接
*        SetUAVPos 驱动对应机图标/轨迹/围栏/任务机），播完发 replayFinished。
*
*        互斥：记录与回放全局互斥（开始回放停掉所有道的记录）；
*        多道记录之间互不影响（多机同时记录正是需求）。
*
* @class TrailRecorder trailrecorder.h "trailrecorder.h"
*/
class TrailRecorder : public QObject
{
    Q_OBJECT

public:
    /// 单个轨迹采样点：WGS-84 位置 + 海拔高度（米）+ 相对记录起点毫秒
    struct TrailPoint
    {
        double lat;
        double lng;
        int altM;
        qint64 tMs;
    };

    explicit TrailRecorder(QObject *parent = 0);

    // —— 记录（按机分道）——
    void StartRecording(int uavId = 0);    ///< 开始记录指定机（该道清空重启；自动停止回放）
    void StopRecording(int uavId = 0);     ///< 停止指定机记录（缓冲保留，可存盘/回放）
    bool IsRecording(int uavId = 0) const { return channelOf(uavId).recording; }
    /// 记录指定机的位置点（该道未在记录中时静默忽略；t 自动取相对起点毫秒）
    void AddPoint(int uavId, opmap::PointLatLng const& pos, int altM);
    int PointCount(int uavId = 0) const { return channelOf(uavId).points.size(); }
    void Clear();                          ///< 清空全部道（自动停止回放）

    // —— 文件（读写指定道）——
    bool SaveToFile(const QString &path, QString *error = 0, int uavId = 0) const;
    bool LoadFromFile(const QString &path, QString *error = 0, int uavId = 0);

    // —— 回放（回放指定道）——
    /// 开始回放指定机（该道点数<2 返回 false；停止全部道的记录，互斥）
    bool StartReplay(double speed = 1.0, int uavId = 0);
    void StopReplay();
    bool IsReplaying() const { return replaying; }
    int ReplayUavId() const { return replayUavId; }   ///< 当前回放的机 id

signals:
    /// 回放推进出的插值位置（facade 接 SetUAVPos(uavId) 驱动对应机）
    void replayPosition(opmap::PointLatLng pos, int altM, int uavId);
    void replayFinished();                 ///< 回放播完（StopReplay 主动停不发）

private slots:
    void onReplayTick();

private:
    /// 单道记录状态：采样序列 + 记录开关 + 独立时间轴
    struct Channel
    {
        QList<TrailPoint> points;  ///< 轨迹采样序列（按 t 递增）
        bool recording;
        QElapsedTimer recordClock; ///< 记录时钟（AddPoint 取相对毫秒）
        Channel() : recording(false) {}
    };

    const Channel &channelOf(int uavId) const;   ///< 取道（不存在时返回空道）
    Channel &channelRef(int uavId);              ///< 取道引用（不存在时创建）

    QMap<int, Channel> channels;   ///< 按 uavId 分道（多机同时记录互不混流）
    QList<TrailPoint> replayBuf;   ///< 回放快照（开始回放时从道拷贝，记录继续不影响）
    int replayUavId;               ///< 当前回放的机 id
    bool replaying;
    QElapsedTimer replayClock;     ///< 回放时钟（tick 换算 × 倍速）
    QTimer *replayTimer;           ///< 50ms 推进定时器
    double replaySpeed;            ///< 回放倍速（1.0=原速）
    int replayIdx;                 ///< 当前插值区间左端点（单调推进）
};

} // end of namespace opmap

#endif // TRAILRECORDER_H
