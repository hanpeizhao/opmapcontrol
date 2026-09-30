/**
******************************************************************************
*
* @file       configuration.h
* @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
* @brief      A class that centralizes most of the mapcontrol configurations
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

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <QBrush>
#include <QPen>
#include <QString>
#include <QFont>
#include "mapservice.h"
#include "accessmode.h"
namespace opmap
{
    
/**
* @brief  A class that centralizes most of the mapcontrol configurations
*
* @class Configuration configuration.h "configuration.h"
*/
class Configuration
{
public:
    Configuration();
    /**
    * @brief Used to draw empty map tiles
    *
    * @var EmptytileBrush
    */
    QBrush EmptytileBrush;
    /**
    * @brief Used for empty tiles text
    *
    * @var EmptyTileText
    */
    QString EmptyTileText;
    /**
    * @brief Used to draw empty tile borders
    *
    * @var EmptyTileBorders
    */
    QPen EmptyTileBorders;
    /**
    * @brief Used to Draw the maps scale
    *
    * @var ScalePen
    */
    QPen ScalePen;
    /**
    * @brief Used to draw selection box
    *
    * @var SelectionPen
    */
    QPen SelectionPen;
    /**
    * @brief Font used to draw missing data text on empty tiles
    *
    * @var MissingDataFont
    */
    QFont MissingDataFont;

    /**
    * @brief Button used for dragging
    *
    * @var DragButton
    */
    Qt::MouseButton DragButton;

    opmap::MapService *mapService;   ///< 地图数据服务（OPMapWidget 注入，不接管所有权）

    /**
    * @brief Sets the access mode for the map (cache only, server and cache...)
    *
    * @param type access mode
    */
    void SetAccessMode(opmap::AccessMode::Types const& type);
    /**
    * @brief Returns the access mode for the map (cache only, server and cache...)
    *
    * @return opmap::AccessMode::Types access mode for the map
    */
    opmap::AccessMode::Types AccessMode();

    /**
    * @brief Sets the language used for geocaching
    *
    * @param type The language to be used
    */
    void SetLanguage(opmap::LanguageType::Types const& type);
    /**
    * @brief Returns the language used for geocaching
    *
    * @return opmap::LanguageType::Types
    */
    opmap::LanguageType::Types Language();

    /**
    * @brief Used to allow disallow use of memory caching
    *
    * @param value
    * @return
    */
    void SetUseMemoryCache(bool const& value);
    /**
    * @brief  Return if memory caching is in use
    *
    * @return
    */
    bool UseMemoryCache(){return mapService->UseMemoryCache();}

    /**
    * @brief  Returns the currently used memory for tiles
    *
    * @return
    */
    double TileMemoryUsed()const{return mapService->TilesInMemory.MemoryCacheSize();}

    /**
    * @brief  Sets the size of the memory for tiles
    *
    * @param  value size in Mb to use for tiles
    * @return
    */
    void SetTileMemorySize(int const& value){mapService->TilesInMemory.setMemoryCacheCapacity(value);}

    /**
    * @brief Sets the location for the SQLite Database used for caching and the geocoding cache files
    *
    * @param dir The path location for the cache file-IMPORTANT Must end with closing slash "/"
    */
    void SetCacheLocation(QString const& dir)
    {
        mapService->setCacheLocation(dir);

    }

    /**
    * @brief  Deletes tiles in DataBase older than "days" days
    *
    * @param days
    * @return
    */
    void DeleteTilesOlderThan(int const& days){mapService->ImageCache.deleteOlderTiles(days);}

    /**
    * @brief  Exports tiles from one DB to another. Only new tiles are added.
    *
    * @param sourceDB the source DB
    * @param destDB the destination DB. If it doesnt exhist it will be created.
    * @return
    */
    void ExportMapDataToDB(QString const& sourceDB, QString const& destDB)const{opmap::TileDiskCache::ExportMapDataToDB(sourceDB,destDB);}
    /**
    * @brief Returns the location for the SQLite Database used for caching and the geocoding cache files
    *
    * @return
    */
    QString CacheLocation(){return mapService->CacheLocation();}

    /**
    * @brief 注入地图数据服务（由 OPMapWidget 构造时调用）
    *
    * @param service 地图数据服务指针，不接管所有权
    */
    void SetMapService(opmap::MapService *service){mapService=service;}


};
}
#endif // CONFIGURATION_H
