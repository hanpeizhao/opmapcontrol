/**
******************************************************************************
*
* @file       trailrecorder.h
* @brief      运动轨迹记录器：按喂点时序记录 位置+高度+相对毫秒，
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
#include <QElapsedTimer>
#include <QTimer>
#include <QString>
#include "pointlatlng.h"

namespace opmap {

/**
* @brief 轨迹记录/回放引擎（纯喂点驱动，不含任何图元/UI 依赖）。
*
*        记录：StartRecording 后每次 AddPoint 记 (lat, lng, alt, t)，
*        t 为相对记录起点的毫秒数——保留原始时序，回放即按 t 轴推进。
*
*        文件：JSON（opmap-trail 格式），人类可读、向后兼容易扩展：
*        { "format":"opmap-trail", "version":1, "created":"...",
*          "points":[ {"t":0,"lat":34.26,"lng":108.94,"alt":100}, ... ] }
*
*        回放：QTimer 50ms tick 按真实时间轴 × 倍速推进，相邻点间线性
*        插值出位置经 replayPosition 信号发出（facade 接 SetUAVPos 驱动
*        图标/轨迹/围栏/任务机），播完发 replayFinished。
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

    // —— 记录 ——
    void StartRecording();                 ///< 开始记录（自动停止回放，互斥）
    void StopRecording();                  ///< 停止记录（缓冲保留，可存盘/回放）
    bool IsRecording() const { return recording; }
    /// 记录一个位置点（未在记录中时静默忽略；t 自动取相对起点毫秒）
    void AddPoint(opmap::PointLatLng const& pos, int altM);
    int PointCount() const { return points.size(); }
    void Clear();                          ///< 清空缓冲（自动停止记录/回放）

    // —— 文件 ——
    bool SaveToFile(const QString &path, QString *error = 0) const;
    bool LoadFromFile(const QString &path, QString *error = 0);

    // —— 回放 ——
    bool StartReplay(double speed = 1.0);  ///< 开始回放（缓冲空/点数<2 返回 false）
    void StopReplay();
    bool IsReplaying() const { return replaying; }

signals:
    /// 回放推进出的插值位置（facade 接 SetUAVPos 驱动地图表现）
    void replayPosition(opmap::PointLatLng pos, int altM);
    void replayFinished();                 ///< 回放播完（StopReplay 主动停不发）

private slots:
    void onReplayTick();

private:
    QList<TrailPoint> points;      ///< 轨迹采样序列（按 t 递增）
    bool recording;
    QElapsedTimer recordClock;     ///< 记录时钟（AddPoint 取相对毫秒）
    bool replaying;
    QElapsedTimer replayClock;     ///< 回放时钟（tick 换算 × 倍速）
    QTimer *replayTimer;           ///< 50ms 推进定时器
    double replaySpeed;            ///< 回放倍速（1.0=原速）
    int replayIdx;                 ///< 当前插值区间左端点（单调推进）
};

} // end of namespace opmap

#endif // TRAILRECORDER_H
