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
 *        加载目标由参数 uavId 决定（文件内 uavId 仅为来源标注），
 *        也可用 LoadFromFileAuto 按文件内标注自动归道。
 *
 *        回放：按道并行——每道独立时间轴/倍速快照，共享 50ms tick 推进，
 *        相邻点间线性插值出位置经 replayPosition 信号发出（携带 uavId，
 *        facade 接 SetUAVPos 驱动对应机图标/轨迹/围栏/任务机）；
 *        某道播完自动补发末点，全部道播完才发 replayFinished。
 *
 *        互斥：按道互斥——开始回放某道自动停该道记录（回放喂点会经
 *        SetUAVPos 回到本道，停记录避免混流），其他道的记录/回放不受影响。
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
    void StartRecording(int uavId = 0);    ///< 开始记录指定机（该道清空重启；该道回放中则先停）
    void StopRecording(int uavId = 0);     ///< 停止指定机记录（缓冲保留，可存盘/回放）
    bool IsRecording(int uavId = 0) const { return channelOf(uavId).recording; }
    /// 记录指定机的位置点（该道未在记录中时静默忽略；t 自动取相对起点毫秒）
    void AddPoint(int uavId, opmap::PointLatLng const& pos, int altM);
    int PointCount(int uavId = 0) const { return channelOf(uavId).points.size(); }
    void Clear();                          ///< 清空全部道（自动停止回放）

    // —— 文件（读写指定道 / 按文件内机号标注自动归道）——
    bool SaveToFile(const QString &path, QString *error = 0, int uavId = 0) const;
    bool LoadFromFile(const QString &path, QString *error = 0, int uavId = 0);
    /// 读取文件内 uavId 标注装入对应道（无标注默认 0 道）；返回机号，失败 -1
    int LoadFromFileAuto(const QString &path, QString *error = 0);

    // —— 回放（按道并行）——
    /// 开始回放指定机（该道点数<2 返回 false；自动停该道记录，其他道不受影响）；
    /// 同道重复开始=从头重启该道回放
    bool StartReplay(double speed = 1.0, int uavId = 0);
    void StopReplay();                     ///< 停止全部道的回放（主动停不发 replayFinished）
    bool IsReplaying() const { return !replayRuns.isEmpty(); }
    int ReplayUavId() const { return replayRuns.size() == 1 ? replayRuns.constBegin().key() : -1; }   ///< 单道回放时为该机 id，多道/无回放为 -1

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

    /// 单道回放状态：数据快照 + 独立时钟（多道并行互不干扰）
    struct ReplayRun
    {
        QList<TrailPoint> buf;   ///< 该道数据快照（回放期间该道继续记录不影响）
        double speed;            ///< 回放倍速（1.0=原速）
        int idx;                 ///< 当前插值区间左端点（单调推进）
        QElapsedTimer clock;     ///< 该道回放时钟（tick 换算 × 倍速）
        ReplayRun() : speed(1.0), idx(0) {}
    };

    const Channel &channelOf(int uavId) const;   ///< 取道（不存在时返回空道）
    Channel &channelRef(int uavId);              ///< 取道引用（不存在时创建）
    void StopReplayOf(int uavId);                ///< 停止指定道的回放（内部复用）
    /// 读轨迹文件 → 采样序列 + 文件内机号标注（format/version 校验）
    bool ParseTrailFile(const QString &path, QString *error,
                        QList<TrailPoint> &out, int &uavIdOut);

    QMap<int, Channel> channels;     ///< 按 uavId 分道（多机同时记录互不混流）
    QMap<int, ReplayRun> replayRuns; ///< 进行中的回放（按 uavId，多道并行）
    QTimer *replayTimer;             ///< 共享 50ms 推进定时器（驱动所有道的回放）
};

} // end of namespace opmap

#endif // TRAILRECORDER_H
