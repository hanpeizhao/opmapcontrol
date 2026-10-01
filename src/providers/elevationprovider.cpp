/**
******************************************************************************
* @file       elevationprovider.cpp
* @brief      地面高程查询 provider 实现（见 elevationprovider.h）
******************************************************************************
*/

#include "elevationprovider.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QTimer>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QUrl>

namespace opmap {

namespace {

// open-meteo elevation API：免 key，响应形如 {"elevation":[523.0]}
//（米，约 EGM96 平均海平面基准）；支持一次最多 100 个坐标，此处单点查询
const char *kElevationUrl = "https://api.open-meteo.com/v1/elevation";
const int kTimeoutMs = 8000;

} // anonymous namespace

ElevationProvider::ElevationProvider(QObject *parent)
    : QObject(parent),
      m_nam(new QNetworkAccessManager(this)),
      m_reply(0),
      m_timeout(new QTimer(this)),
      m_busy(false),
      m_pos(0, 0)
{
    m_timeout->setSingleShot(true);
    connect(m_timeout, SIGNAL(timeout()), this, SLOT(onRequestTimeout()));
}

void ElevationProvider::requestElevation(const opmap::PointLatLng &pos)
{
    if (m_busy)
        return;
    m_busy = true;
    m_pos = pos;

    // 高纬度纬度符号直接进查询串，QUrl 会做百分号编码
    const QString qs = QString::fromLatin1("latitude=%1&longitude=%2")
                               .arg(pos.Lat(), 0, 'f', 6)
                               .arg(pos.Lng(), 0, 'f', 6);
    const QUrl url = QUrl(QLatin1String(kElevationUrl) + QLatin1Char('?') + qs);
    m_reply = m_nam->get(QNetworkRequest(url));
    // 必须连 reply 自己的 finished()：连 QNAM 的重载会因 sender() 类型不符丢结果
    connect(m_reply, SIGNAL(finished()), this, SLOT(onReplyFinished()));
    m_timeout->start(kTimeoutMs);
}

void ElevationProvider::onRequestTimeout()
{
    if (!m_reply)
        return;
    m_timeout->stop();
    m_reply->abort();           // abort 触发 finished，由 onReplyFinished 统一报失败
}

void ElevationProvider::onReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply || reply != m_reply)
        return;
    m_timeout->stop();
    m_reply = 0;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        m_busy = false;
        emit elevationFailed(reply->errorString());
        return;
    }

    const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
    const QJsonArray arr = obj.value(QLatin1String("elevation")).toArray();
    if (arr.isEmpty()) {
        m_busy = false;
        emit elevationFailed(QString::fromUtf8("高程服务返回异常"));
        return;
    }

    m_busy = false;
    emit elevationReady(m_pos, static_cast<int>(qRound(arr.at(0).toDouble())));
}

} // namespace opmap
