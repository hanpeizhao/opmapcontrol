/**
******************************************************************************
*
* @file       mapservice.cpp
* @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
* @brief      地图数据服务实现：三级缓存取瓦片（内存→SQLite→网络）与统计
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
*/
#include "mapservice.h"

#include <QEventLoop>
#include <QTimer>
#include <QDir>

namespace opmap {

MapService::MapService() :
    RetryLoadTile(2),
    tileDBcacheQueue(&ImageCache),
    useMemoryCache(true),
    language(LanguageType::PortuguesePortugal)
{
    accessmode = AccessMode::ServerAndCache;
    languageStr = LanguageType().toShortString(language);
    cacheLocation = QDir::homePath() + "/mapscache/";
    ImageCache.setGtileCache(cacheLocation);
}

MapService::~MapService()
{
    tileDBcacheQueue.wait();
}

void MapService::setCacheLocation(const QString &value)
{
    cacheLocation = value;
    ImageCache.setGtileCache(value);
}

void MapService::setLanguage(const LanguageType::Types &value)
{
    language = value;
    languageStr = LanguageType().toShortString(value);
}

QByteArray MapService::GetTileFromMemoryCache(const TileKey &tile)
{
    kiberCacheLock.lockForRead();
    QByteArray pic;
    pic = TilesInMemory.cachequeue.value(tile);
    kiberCacheLock.unlock();
    return pic;
}

void MapService::AddTileToMemoryCache(const TileKey &tile, const QByteArray &pic)
{
    kiberCacheLock.lockForWrite();
    TilesInMemory.memoryCacheSize += pic.size();
#ifdef DEBUG_MEMORY_CACHE
    qDebug()<<"Current memory="<<TilesInMemory.memoryCacheSize<<" in "<<TilesInMemory.cachequeue.count()<<" tiles";
#endif
    TilesInMemory.cachequeue.insert(tile, pic);
    TilesInMemory.list.enqueue(tile);
    kiberCacheLock.unlock();
}

QByteArray MapService::GetImageFrom(const MapType::Types &type, const Point &pos, const int &zoom)
{
    QByteArray ret;

    if(useMemoryCache)
    {
        ret = GetTileFromMemoryCache(TileKey(type, pos, zoom));
        if(!ret.isEmpty())
        {
            errorvars.lock();
            ++diag.tilesFromMem;
            errorvars.unlock();
        }
    }

    if(ret.isEmpty())
    {
        if(accessmode != (AccessMode::ServerOnly))
        {
            ret = ImageCache.GetImageFromCache(type, pos, zoom);
            if(!ret.isEmpty())
            {
                errorvars.lock();
                ++diag.tilesFromDB;
                errorvars.unlock();
                if(useMemoryCache)
                {
                    AddTileToMemoryCache(TileKey(type, pos, zoom), ret);
                }
                return ret;
            }
        }

        if(accessmode != AccessMode::CacheOnly)
        {
            QEventLoop q;
            QNetworkReply *reply;
            QNetworkRequest qheader;
            // QNetworkAccessManager 内部持有线程与锁，栈上临时对象销毁时会刷
            // "QMutex: destroying locked mutex"；改为每工作线程复用一个实例
            static thread_local QNetworkAccessManager *tlsNam = 0;
            if (!tlsNam)
                tlsNam = new QNetworkAccessManager;
            QNetworkAccessManager &network = *tlsNam;
            QTimer tT;

            tT.setSingleShot(true);
            QObject::connect(&network, SIGNAL(finished(QNetworkReply*)),
                    &q, SLOT(quit()));
            QObject::connect(&tT, SIGNAL(timeout()), &q, SLOT(quit()));
            network.setProxy(urlFactory.Proxy);

            QString url = urlFactory.MakeImageUrl(type, pos, zoom, languageStr);
            qheader.setUrl(QUrl(url));
            qheader.setRawHeader("User-Agent", urlFactory.UserAgent);
            qheader.setRawHeader("Accept","*/*");
            // Qt5 默认不跟随重定向，http->https 301 会导致瓦片下载失败
            qheader.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
            switch(type)
            {
            case MapType::GoogleMap:
            case MapType::GoogleSatellite:
            case MapType::GoogleLabels:
            case MapType::GoogleTerrain:
            case MapType::GoogleHybrid:
            {
                qheader.setRawHeader("Referrer", "https://maps.google.com/");
            }
                break;

            case MapType::OpenStreetMap:
            {
                qheader.setRawHeader("Referrer", "http://www.openstreetmap.org/");
            }
                break;

            default:
                break;
            }
            reply = network.get(qheader);
            tT.start(urlFactory.Timeout);
            q.exec();

            if(!tT.isActive()){
                errorvars.lock();
                ++diag.timeouts;
                errorvars.unlock();
                return ret;
            }
            tT.stop();
            if( (reply->error()!=QNetworkReply::NoError))
            {
                errorvars.lock();
                ++diag.networkerrors;
                errorvars.unlock();
                reply->deleteLater();
                return ret;
            }
            ret = reply->readAll();
            reply->deleteLater();
            if(ret.isEmpty())
            {
                errorvars.lock();
                ++diag.emptytiles;
                errorvars.unlock();
                return ret;
            }
            errorvars.lock();
            ++diag.tilesFromNet;
            errorvars.unlock();
            if (useMemoryCache)
            {
                AddTileToMemoryCache(TileKey(type, pos, zoom), ret);
            }

            if(accessmode != AccessMode::ServerOnly)
            {
                CacheItemQueue * item = new CacheItemQueue(type, pos, ret, zoom);
                tileDBcacheQueue.EnqueueCacheTask(item);
            }
        }
    }

    return ret;
}

QVector<MapType::Types> MapService::GetAllLayersOfType(const MapType::Types &type)
{
    return allLayers.GetAllLayersOfType(type);
}

bool MapService::ExportToGMDB(const QString &file)
{
    return ImageCache.ExportMapDataToDB(
                ImageCache.GtileCache() + QDir::separator() + "OPMaps.qmdb",
                file);
}

bool MapService::ImportFromGMDB(const QString &file)
{
    return ImageCache.ExportMapDataToDB(
                file,
                ImageCache.GtileCache() + QDir::separator() + "OPMaps.qmdb");
}

diagnostics MapService::GetDiagnostics()
{
    diagnostics i;

    errorvars.lock();
    i = diag;
    errorvars.unlock();
    return i;
}

} // end of namespace opmap
