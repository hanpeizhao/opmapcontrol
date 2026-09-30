/**
******************************************************************************
* @file       iplocationprovider.cpp
* @brief      IP 定位 provider 实现（见 iplocationprovider.h）
******************************************************************************
*/

#include "iplocationprovider.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QTimer>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QUrl>

namespace opmap {

namespace {

const char *kPrimaryUrl = "http://ip-api.com/json/?fields=status,lat,lon,city";
const char *kFallbackUrl = "https://ipwho.is/";
const int kTimeoutMs = 8000;

} // anonymous namespace

IpLocationProvider::IpLocationProvider(QObject *parent)
    : QObject(parent),
      m_nam(new QNetworkAccessManager(this)),
      m_reply(0),
      m_timeout(new QTimer(this)),
      m_busy(false),
      m_useFallback(false)
{
    m_timeout->setSingleShot(true);
    connect(m_timeout, SIGNAL(timeout()), this, SLOT(onRequestTimeout()));
}

void IpLocationProvider::requestLocation()
{
    if (m_busy)
        return;
    m_busy = true;
    m_useFallback = false;      // 每轮都先试主源（其城市库对本机更准）
    startRequest();
}

void IpLocationProvider::startRequest()
{
    // 主源 ip-api.com：纯 IPv4 出口、其库对国内城市识别准；备源 ipwho.is：
    // HTTPS 直连，但对部分 IPv6 出口的库偏差较大（曾把长沙误判为岳阳）
    const QUrl url = QUrl(QLatin1String(m_useFallback ? kFallbackUrl : kPrimaryUrl));
    m_reply = m_nam->get(QNetworkRequest(url));
    // 注意：必须连 reply 自己的 finished()。若连 QNAM 的 finished(QNetworkReply*)，
    // 槽内 sender() 是 QNAM 而非 reply，qobject_cast 恒为 null，结果永远无人处理
    connect(m_reply, SIGNAL(finished()), this, SLOT(onReplyFinished()));
    m_timeout->start(kTimeoutMs);
}

void IpLocationProvider::onRequestTimeout()
{
    if (!m_reply)
        return;
    m_timeout->stop();
    m_reply->abort();           // abort 触发 finished，由 onReplyFinished 统一走回退或报错
}

void IpLocationProvider::onReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply || reply != m_reply)
        return;
    m_timeout->stop();
    m_reply = 0;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        if (!m_useFallback) {
            m_useFallback = true;   // 主源失败，静默切备源重试一次
            startRequest();
            return;
        }
        m_busy = false;
        emit locationFailed(reply->errorString());
        return;
    }

    const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
    // ip-api 主源字段：{"status":"success","lat":..,"lon":..,"city":..}
    // ipwho.is 备源字段：{"success":true,"latitude":..,"longitude":..,"city":..}
    bool ok;
    double lat;
    double lon;
    QString city;
    if (m_useFallback) {
        ok = obj.value(QLatin1String("success")).toBool();
        lat = obj.value(QLatin1String("latitude")).toDouble();
        lon = obj.value(QLatin1String("longitude")).toDouble();
        city = obj.value(QLatin1String("city")).toString();
    } else {
        ok = (obj.value(QLatin1String("status")).toString() == QLatin1String("success"));
        lat = obj.value(QLatin1String("lat")).toDouble();
        lon = obj.value(QLatin1String("lon")).toDouble();
        city = obj.value(QLatin1String("city")).toString();
    }

    if (!ok || (lat == 0.0 && lon == 0.0)) {
        if (!m_useFallback) {
            m_useFallback = true;   // 主源返回异常，切备源重试一次
            startRequest();
            return;
        }
        m_busy = false;
        emit locationFailed(QString::fromUtf8("IP 定位服务返回异常"));
        return;
    }

    m_busy = false;
    emit locationReady(PointLatLng(lat, lon), city);
}

} // namespace opmap
