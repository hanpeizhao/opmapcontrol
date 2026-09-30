/**
******************************************************************************
*
* @file       urlfactory.h
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
#ifndef URLFACTORY_H
#define URLFACTORY_H

#include <QtNetwork/QNetworkProxy>
#include <QtNetwork/QNetworkAccessManager>
#include <QUrl>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QTimer>
#include <QCoreApplication>
#include "providerstrings.h"
#include "maptype.h"
#include "point.h"
#include "../internals/pointlatlng.h"
#include <QTime>
#include <QMutex>

/// 生成各地图源（Google/OSM/ArcGIS/高德）的瓦片 URL，
/// 并负责探测 Google 瓦片版本号
namespace core {
    class UrlFactory: public QObject,public ProviderStrings
    {
        Q_OBJECT
    public:
        /// <summary>
        /// Gets or sets the value of the User-agent HTTP header.
        /// </summary>
        QByteArray UserAgent;
        QNetworkProxy Proxy;
        UrlFactory();
        ~UrlFactory();
        QString MakeImageUrl(const MapType::Types &type,const core::Point &pos,const int &zoom,const QString &language);
        int Timeout;
    private:
        int Random(int low, int high);
        void GetSecGoogleWords(const core::Point &pos,  QString &sec1, QString &sec2);
        int GetServerNum(const core::Point &pos,const int &max) const;
        void TryCorrectGoogleVersions();
        bool isCorrectedGoogleVersions;
        bool CorrectGoogleVersions;
        static const double EarthRadiusKm;
        QMutex mutex;

    protected:
        static short timelapse;
        QString LanguageStr;
        bool IsCorrectGoogleVersions();
        void setIsCorrectGoogleVersions(bool value);
    };

}
#endif // URLFACTORY_H
