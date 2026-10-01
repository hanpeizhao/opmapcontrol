/**
******************************************************************************
* @file       elevationprovider.h
* @brief      地面高程查询 provider（open-meteo 免费高程 API，免 key）
*
*             查询指定 WGS-84 坐标的真实地面海拔（米，约等于平均海平面基准）。
*             地图瓦片是 2D 影像不含高程，此 provider 补齐"查某点海拔"能力。
*             结果经 elevationReady/elevationFailed 信号返回（QNetworkAccessManager
*             回调在主线程，全程无跨线程）；8 秒超时自动中止并报失败。
******************************************************************************
*/
#ifndef ELEVATIONPROVIDER_H
#define ELEVATIONPROVIDER_H

#include <QObject>
#include <QString>

#include "pointlatlng.h"

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace opmap {

class ElevationProvider : public QObject
{
    Q_OBJECT

public:
    explicit ElevationProvider(QObject *parent = 0);

    /**
     * @brief 异步查询 pos 处的地面海拔，结果经 elevationReady/elevationFailed 信号返回
     *        在途时重复调用被忽略
     */
    void requestElevation(const opmap::PointLatLng &pos);

    /// 是否有查询在途
    bool isBusy() const { return m_busy; }

signals:
    /// 查询成功（pos 回显请求坐标，altitudeMeters 为地面海拔米数）
    void elevationReady(opmap::PointLatLng pos, int altitudeMeters);
    /// 查询失败（网络错误、超时或服务返回异常）
    void elevationFailed(QString reason);

private slots:
    void onReplyFinished();
    void onRequestTimeout();

private:
    QNetworkAccessManager *m_nam;
    QNetworkReply *m_reply;
    QTimer *m_timeout;
    bool m_busy;
    opmap::PointLatLng m_pos;   ///< 请求坐标（信号回显用）
};

} // namespace opmap

#endif // ELEVATIONPROVIDER_H
