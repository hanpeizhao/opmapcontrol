/**
******************************************************************************
*
* @file       maptype.h
* @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
* @brief      地图源类型枚举与名称映射（数值是历史遗留的稳定 ID）      
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

#ifndef MAPTYPE_H
#define MAPTYPE_H

#include <QMetaObject>
#include <QMetaEnum>
#include <QStringList>

namespace opmap {

class MapType:public QObject
{
    Q_OBJECT
    Q_ENUMS(Types)

public:
    /// 可用的地图源。
    /// 数值是历史遗留的稳定 ID：SQLite 瓦片缓存按该数值索引，
    /// 修改数值会导致已有缓存(OPMaps.qmdb)失效。
    enum Types
    {
        GoogleMap=1,
        GoogleSatellite=4,
        GoogleLabels=8,
        GoogleTerrain=16,
        GoogleHybrid=20,

        OpenStreetMap=32,

        ArcGIS_Map=777,
        ArcGIS_Satellite=788,
        ArcGIS_WorldTopo=812,

        // 高德地图（国内可达性好；瓦片为 GCJ-02 坐标，与 WGS-84 有数百米偏移）
        AutoNaviRoad = 6000,
        AutoNaviSatellite = 6001,
        AutoNaviLabels = 6002,
        AutoNaviHybrid = 6003
    };

    /// <summary>
    /// 瓦片数据基准坐标系：地图源声明自己的瓦片对齐到哪个坐标系，
    /// 控件据此在用户坐标(WGS-84)与瓦片坐标之间自动纠偏
    /// </summary>
    enum TileDatum
    {
        DatumWGS84,     // WGS-84：OSM、ArcGIS、Google 国际版等
        DatumGCJ02      // GCJ-02：高德等国内加密坐标
    };

    static TileDatum DatumByType(Types const& value)
    {
        switch(value)
        {
        case AutoNaviRoad:
        case AutoNaviSatellite:
        case AutoNaviLabels:
        case AutoNaviHybrid:
            return DatumGCJ02;
        default:
            return DatumWGS84;
        }
    }

    static QString StrByType(Types const& value)
    {
        QMetaObject metaObject = MapType().staticMetaObject;
        QMetaEnum metaEnum= metaObject.enumerator( metaObject.indexOfEnumerator("Types"));
        QString s=metaEnum.valueToKey(value);
        return s;
    }

    static Types TypeByStr(QString const& value)
    {
        QMetaObject metaObject = MapType().staticMetaObject;
        QMetaEnum metaEnum= metaObject.enumerator( metaObject.indexOfEnumerator("Types"));
        Types s=(Types)metaEnum.keyToValue(value.toLatin1());
        return s;
    }

    static QStringList TypesList()
    {
        QStringList ret;
        QMetaObject metaObject = MapType().staticMetaObject;
        QMetaEnum metaEnum= metaObject.enumerator( metaObject.indexOfEnumerator("Types"));
        for(int x=0;x<metaEnum.keyCount();++x)
        {
            ret.append(metaEnum.key(x));
        }
        return ret;
    }
};

}

#endif // MAPTYPE_H
