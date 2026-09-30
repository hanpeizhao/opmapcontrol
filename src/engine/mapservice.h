/**
******************************************************************************
*
* @file       mapservice.h
* @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
* @brief      地图数据服务：组合内存缓存/磁盘缓存/写库队列/URL 工厂/图层拆分，
*             由 OPMapWidget 创建并注入到引擎与 UI 配置，取代原全局单例。
* @see        The GNU Public License (GPL) Version 3
* @defgroup   OPMapWidget
* @{
*
*****************************************************************************/
#ifndef MAPSERVICE_H
#define MAPSERVICE_H

#include <QByteArray>
#include <QMutex>
#include <QReadWriteLock>
#include <QVector>

#include "rawtile.h"
#include "point.h"
#include "kibertilecache.h"
#include "pureimagecache.h"
#include "tilecachequeue.h"
#include "cacheitemqueue.h"
#include "accessmode.h"
#include "languagetype.h"
#include "alllayersoftype.h"
#include "urlfactory.h"
#include "diagnostics.h"

namespace core {

class MapService
{
public:
    MapService();
    ~MapService();

    /// 瓦片获取：内存缓存 → 磁盘缓存(SQLite) → 网络，并按访问模式回写缓存
    QByteArray GetImageFrom(const MapType::Types &type, const Point &pos, const int &zoom);

    /// 将地图类型拆分为需叠加的图层列表（如 Hybrid = 影像 + 路网）
    QVector<MapType::Types> GetAllLayersOfType(const MapType::Types &type);

    /// 网络与缓存命中统计
    diagnostics GetDiagnostics();

    /// 离线数据导入导出（OPMaps.qmdb 兼容格式）
    bool ImportFromGMDB(const QString &file);
    bool ExportToGMDB(const QString &file);

    /// 磁盘缓存目录
    void setCacheLocation(const QString &value);
    QString CacheLocation() const { return cacheLocation; }

    // ---- 访问配置 ----
    AccessMode::Types GetAccessMode() const { return accessmode; }
    void setAccessMode(const AccessMode::Types &mode) { accessmode = mode; }

    void setLanguage(const LanguageType::Types &value);
    LanguageType::Types GetLanguage() const { return language; }

    bool UseMemoryCache() const { return useMemoryCache; }
    void setUseMemoryCache(const bool &value) { useMemoryCache = value; }

    /// 网络超时（毫秒）
    int Timeout() const { return urlFactory.Timeout; }

    /// 空瓦片重试次数
    int RetryLoadTile;

    // ---- 缓存设施（Core 与配置项直接访问） ----
    KiberTileCache TilesInMemory;       ///< 内存瓦片 LRU 缓存
    QReadWriteLock kiberCacheLock;      ///< 保护 TilesInMemory
    PureImageCache ImageCache;          ///< 磁盘瓦片库（SQLite）

private:
    QByteArray GetTileFromMemoryCache(const RawTile &tile);
    void AddTileToMemoryCache(const RawTile &tile, const QByteArray &pic);

    UrlFactory urlFactory;              ///< 瓦片 URL 工厂（含代理/UA/超时）
    AllLayersOfType allLayers;          ///< 地图类型 → 图层列表
    TileCacheQueue tileDBcacheQueue;    ///< 后台写库队列线程

    bool useMemoryCache;
    LanguageType::Types language;
    QString languageStr;
    AccessMode::Types accessmode;
    QString cacheLocation;
    diagnostics diag;
    QMutex errorvars;
};

} // end of namespace core

#endif // MAPSERVICE_H
