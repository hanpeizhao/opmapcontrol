/**
******************************************************************************
* @file       amaprouteprovider.h
* @brief      高德驾车路径规划 provider（Web 服务，需自备 key）
*
*             高德 API 使用 GCJ-02 坐标：发送前 WGS84→GCJ02，返回 steps[].polyline
*             逐点 GCJ02→WGS84；steps[].instruction 自带中文指令。
******************************************************************************
*/
#ifndef AMAPROUTEPROVIDER_H
#define AMAPROUTEPROVIDER_H

#include <QString>
#include "abstractrouteprovider.h"

class QNetworkAccessManager;

namespace opmap {

class AmapRouteProvider : public AbstractRouteProvider
{
    Q_OBJECT

public:
    explicit AmapRouteProvider(QObject *parent = 0);

    /// 设置高德 Web 服务 key（驾车路径规划权限）
    void SetKey(const QString &key) { m_key = key; }

    virtual void requestRoute(const opmap::PointLatLng &from, const opmap::PointLatLng &to);
    virtual bool isBusy() const { return m_busy; }

private slots:
    void onRequestFinished();

private:
    /// "lng,lat;lng,lat;..." 点串 → GCJ-02 点列表（原样，不纠偏）
    static QList<PointLatLng> parsePolyline(const QString &s);

    QNetworkAccessManager *m_nam;
    QString m_key;
    bool m_busy;
};

} // namespace opmap

#endif // AMAPROUTEPROVIDER_H
