/**
******************************************************************************
* @file       iplocationprovider.h
* @brief      IP 定位 provider（城市级兜底定位，双源自动回退）
*
*             通过公网出口 IP 估算设备所在城市，作为无 GPS 环境下的定位兜底。
*             主源 ip-api.com（对国内城市识别准），失败或超时（8 秒）自动静默
*             切备源 ipwho.is 重试一次。结果经 locationReady/locationFailed
*             信号返回（QNetworkAccessManager 回调在主线程，全程无跨线程）。
*             注意：IP 定位为城市级精度（约数公里），坐标为 WGS-84。
******************************************************************************
*/
#ifndef IPLOCATIONPROVIDER_H
#define IPLOCATIONPROVIDER_H

#include <QObject>
#include <QString>

#include "pointlatlng.h"

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace opmap {

class IpLocationProvider : public QObject
{
    Q_OBJECT

public:
    explicit IpLocationProvider(QObject *parent = 0);

    /**
     * @brief 异步发起一次 IP 定位，结果经 locationReady/locationFailed 信号返回
     *        在途时重复调用被忽略
     */
    void requestLocation();

    /// 是否有请求在途
    bool isBusy() const { return m_busy; }

signals:
    /// IP 定位成功（pos 为 WGS-84 城市级坐标，city 为城市名）
    void locationReady(opmap::PointLatLng pos, QString city);
    /// IP 定位失败（双源均不可用或返回异常）
    void locationFailed(QString reason);

private slots:
    void onReplyFinished();
    void onRequestTimeout();

private:
    /// 按当前主/备源发起请求（m_useFallback 决定）
    void startRequest();

    QNetworkAccessManager *m_nam;
    QNetworkReply *m_reply;
    QTimer *m_timeout;
    bool m_busy;
    bool m_useFallback;   ///< true=ipwho.is 备源，false=ip-api.com 主源
};

} // namespace opmap

#endif // IPLOCATIONPROVIDER_H
