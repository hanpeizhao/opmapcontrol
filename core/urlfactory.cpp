/**
******************************************************************************
*
* @file       urlfactory.cpp
* @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
* @brief      
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

#include "urlfactory.h"
#include <QRegExp>
#include <qmath.h>

namespace core {

const double UrlFactory::EarthRadiusKm = 6378.137; // WGS-84

UrlFactory::UrlFactory()
{
    /// <summary>
    /// timeout for map connections
    /// </summary>

    Proxy.setType(QNetworkProxy::NoProxy);

    /// <summary>
    /// Gets or sets the value of the User-agent HTTP header.
    /// </summary>
    UserAgent = QString("Mozilla/5.0 (Windows NT 6.1; WOW64; rv:%1.0) Gecko/%2%3%4 Firefox/%5.0.%6").arg(QString::number(Random(3,14)), QString::number(Random(QDate().currentDate().year() - 4, QDate().currentDate().year())), QString::number(Random(11,12)), QString::number(Random(10,30)), QString::number(Random(3,14)), QString::number(Random(1,10))).toLatin1();

    Timeout = 5 * 1000;
    CorrectGoogleVersions=true;
    isCorrectedGoogleVersions = false;
}

UrlFactory::~UrlFactory()
{
}

int UrlFactory::Random(int low, int high)
{
    return low + qrand() % (high - low);
}

int UrlFactory::GetServerNum(const Point &pos,const int &max) const
{
    return (pos.X() + 2 * pos.Y()) % max;
}

void UrlFactory::setIsCorrectGoogleVersions(bool value)
{
    isCorrectedGoogleVersions=value;
}

bool UrlFactory::IsCorrectGoogleVersions()
{
    return isCorrectedGoogleVersions;
}

void UrlFactory::TryCorrectGoogleVersions()
{
    QMutexLocker locker(&mutex);
    if(CorrectGoogleVersions && !IsCorrectGoogleVersions())
    {
        QNetworkReply *reply;
        QNetworkRequest qheader;
        QNetworkAccessManager network;
        QEventLoop q;
        QTimer tT;

        tT.setSingleShot(true);
        connect(&network, SIGNAL(finished(QNetworkReply*)),
                &q, SLOT(quit()));
        connect(&tT, SIGNAL(timeout()), &q, SLOT(quit()));
        network.setProxy(Proxy);
#ifdef DEBUG_URLFACTORY
        qDebug()<<"Correct GoogleVersion";
#endif //DEBUG_URLFACTORY
        setIsCorrectGoogleVersions(true);
        QString url = "https://maps.google.com";

        qheader.setUrl(QUrl(url));
        qheader.setRawHeader("User-Agent",UserAgent);
        reply=network.get(qheader);
        tT.start(Timeout);
        q.exec();
        if(!tT.isActive())
            return;
        tT.stop();
        if( (reply->error()!=QNetworkReply::NoError))
        {
#ifdef DEBUG_URLFACTORY
            qDebug()<<"Try corrected version withou abort or error:"<<reply->errorString();
#endif //DEBUG_URLFACTORY
            return;
        }

        QString html=QString(reply->readAll());
        QRegExp reg("\"*https://mt0.google.com/vt/lyrs=m@(\\d*)",Qt::CaseInsensitive);
        if(reg.indexIn(html)!=-1)
        {
            QStringList gc=reg.capturedTexts();
            VersionGoogleMap = QString("m@%1").arg(gc[1]);
#ifdef DEBUG_URLFACTORY
            qDebug()<<"TryCorrectGoogleVersions, VersionGoogleMap: "<<VersionGoogleMap;
#endif //DEBUG_URLFACTORY
        }

        reg=QRegExp("\"*https://mt0.google.com/vt/lyrs=h@(\\d*)",Qt::CaseInsensitive);
        if(reg.indexIn(html)!=-1)
        {
            QStringList gc=reg.capturedTexts();
            VersionGoogleLabels = QString("h@%1").arg(gc[1]);
#ifdef DEBUG_URLFACTORY
            qDebug()<<"TryCorrectGoogleVersions, VersionGoogleLabels: "<<VersionGoogleLabels;
#endif //DEBUG_URLFACTORY
        }

        reg=QRegExp("\"*https://khm\\D?\\d.google.com/kh/v=(\\d*)",Qt::CaseInsensitive);
        if(reg.indexIn(html)!=-1)
        {
            QStringList gc=reg.capturedTexts();
            VersionGoogleSatellite = gc[1];

            qDebug()<<"TryCorrectGoogleVersions, VersionGoogleSatellite: "<<VersionGoogleSatellite;

        }

        reg=QRegExp("\"*https://mt0.google.com/vt/lyrs=t@(\\d*),r@(\\d*)",Qt::CaseInsensitive);
        if(reg.indexIn(html)!=-1)
        {
            QStringList gc=reg.capturedTexts();
            VersionGoogleTerrain = QString("t@%1,r@%2").arg(gc[1]).arg(gc[2]);
#ifdef DEBUG_URLFACTORY
            qDebug()<<"TryCorrectGoogleVersions, VersionGoogleTerrain: "<<VersionGoogleTerrain;
#endif //DEBUG_URLFACTORY
        }

        reply->deleteLater();
    }

}

QString UrlFactory::MakeImageUrl(const MapType::Types &type,const Point &pos,const int &zoom,const QString &language)
{
#ifdef DEBUG_URLFACTORY
    qDebug()<<"Entered MakeImageUrl";
#endif //DEBUG_URLFACTORY
    switch(type)
    {
    case MapType::GoogleMap:
    {
        QString server = "mt";
        QString request = "vt";
        QString sec1 = ""; // after &x=...
        QString sec2 = ""; // after &zoom=...
        GetSecGoogleWords(pos,  sec1,  sec2);

        // 现代接口不再需要 @version 后缀
        return QString("https://%1%2.google.com/%3/lyrs=m&hl=%4&x=%5%6&y=%7&z=%8&s=%9").arg(server).arg(GetServerNum(pos, 4)).arg(request).arg(language).arg(pos.X()).arg(sec1).arg(pos.Y()).arg(zoom).arg(sec2);
    }
        break;
    case MapType::GoogleSatellite:
    {
        // 卫星图同样走 mt/vt 的 lyrs=s 通道，避免依赖过期的 khm 版本号
        QString server = "mt";
        QString request = "vt";
        QString sec1 = ""; // after &x=...
        QString sec2 = ""; // after &zoom=...
        GetSecGoogleWords(pos,  sec1,  sec2);
        return QString("https://%1%2.google.com/%3/lyrs=s&hl=%4&x=%5%6&y=%7&z=%8&s=%9").arg(server).arg(GetServerNum(pos, 4)).arg(request).arg(language).arg(pos.X()).arg(sec1).arg(pos.Y()).arg(zoom).arg(sec2);
    }
        break;
    case MapType::GoogleLabels:
    {
        QString server = "mt";
        QString request = "vt";
        QString sec1 = ""; // after &x=...
        QString sec2 = ""; // after &zoom=...
        GetSecGoogleWords(pos,  sec1,  sec2);

        return QString("https://%1%2.google.com/%3/lyrs=h&hl=%4&x=%5%6&y=%7&z=%8&s=%9").arg(server).arg(GetServerNum(pos, 4)).arg(request).arg(language).arg(pos.X()).arg(sec1).arg(pos.Y()).arg(zoom).arg(sec2);
    }
        break;
    case MapType::GoogleTerrain:
    {
        QString server = "mt";
        QString request = "vt";
        QString sec1 = ""; // after &x=...
        QString sec2 = ""; // after &zoom=...
        GetSecGoogleWords(pos,  sec1,  sec2);
        return QString("https://%1%2.google.com/%3/lyrs=p&hl=%4&x=%5%6&y=%7&z=%8&s=%9").arg(server).arg(GetServerNum(pos, 4)).arg(request).arg(language).arg(pos.X()).arg(sec1).arg(pos.Y()).arg(zoom).arg(sec2);
    }
        break;
    case MapType::OpenStreetMap:
    {
        // OSM 官方主域名，强制 https（http 会被 301 跳转）
        return QString("https://tile.openstreetmap.org/%1/%2/%3.png").arg(zoom).arg(pos.X()).arg(pos.Y());
    }
        break;
    case MapType::ArcGIS_Map:
    {
        // http://server.arcgisonline.com/ArcGIS/rest/services/ESRI_StreetMap_World_2D/MapServer/tile/0/0/0.jpg

        // ESRI_StreetMap_World_2D 服务已下线，改用 World_Street_Map
        return QString("https://server.arcgisonline.com/ArcGIS/rest/services/World_Street_Map/MapServer/tile/%1/%2/%3").arg(zoom).arg(pos.Y()).arg(pos.X());
    }
        break;
    case MapType::ArcGIS_Satellite:
    {
        // http://server.arcgisonline.com/ArcGIS/rest/services/ESRI_Imagery_World_2D/MapServer/tile/1/0/1.jpg

        // ESRI_Imagery_World_2D 服务已下线，改用 World_Imagery
        return QString("https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/%1/%2/%3").arg(zoom).arg(pos.Y()).arg(pos.X());
    }
        break;
    case MapType::ArcGIS_WorldTopo:
    {
        // http://server.arcgisonline.com/ArcGIS/rest/services/World_Topo_Map/MapServer/tile/4/3/15

        return QString("https://server.arcgisonline.com/ArcGIS/rest/services/World_Topo_Map/MapServer/tile/%1/%2/%3").arg(zoom).arg(pos.Y()).arg(pos.X());
    }
        break;
    case MapType::AutoNaviRoad:
    {
        // 高德路网图，webrd01-04 四台负载
        return QString("https://webrd0%1.is.autonavi.com/appmaptile?lang=zh_cn&size=1&scale=1&style=8&x=%2&y=%3&z=%4").arg(GetServerNum(pos, 4) + 1).arg(pos.X()).arg(pos.Y()).arg(zoom);
    }
        break;
    case MapType::AutoNaviSatellite:
    {
        // 高德卫星影像，webst01-04
        return QString("https://webst0%1.is.autonavi.com/appmaptile?style=6&x=%2&y=%3&z=%4").arg(GetServerNum(pos, 4) + 1).arg(pos.X()).arg(pos.Y()).arg(zoom);
    }
        break;
    case MapType::AutoNaviLabels:
    {
        // 高德路网标注图层，叠加在卫星图上组成混合图
        return QString("https://webst0%1.is.autonavi.com/appmaptile?style=8&x=%2&y=%3&z=%4").arg(GetServerNum(pos, 4) + 1).arg(pos.X()).arg(pos.Y()).arg(zoom);
    }
        break;
    default:
        break;
    }

    return QString::null;
}

void UrlFactory::GetSecGoogleWords(const Point &pos,  QString &sec1, QString &sec2)
{
    sec1 = ""; // after &x=...
    sec2 = ""; // after &zoom=...
    int seclen = ((pos.X() * 3) + pos.Y()) % 8;
    sec2 = SecGoogleWord.left(seclen);
    if(pos.Y() >= 10000 && pos.Y() < 100000)
    {
        sec1 = "&s=";
    }
}

} // end of namespace core
