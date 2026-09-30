/**
******************************************************************************
*
* @file       waypoint_store.cpp
* @brief      航点文件(.wp)读写封装：OPMapWidget 航点 ↔ AP_WPArray
* @see        The GNU Public License (GPL) Version 3
* @{
*
*****************************************************************************/

#include "waypoint_store.h"

#include "opmapcontrol.h"
#include "uas_types.h"

WaypointStore::WaypointStore(opmap::OPMapWidget *mapWidget)
    : m_map(mapWidget)
{
}

bool WaypointStore::save(const QString &path)
{
    m_error.clear();

    QMap<int, opmap::WayPointItem*> wpMap = m_map->WPAll();
    if (wpMap.isEmpty()) {
        m_error = QString::fromUtf8("地图上没有航点");
        return false;
    }

    AP_WPArray arrWP;
    QMap<int, opmap::WayPointItem*>::const_iterator it = wpMap.constBegin();
    for (; it != wpMap.constEnd(); ++it) {
        AP_WayPoint *wp = new AP_WayPoint;
        wp->idx = it.key();
        wp->set(it.value()->Coord().Lat(),
                it.value()->Coord().Lng(),
                it.value()->Altitude());
        arrWP.set(*wp);
        delete wp;
    }

    if (arrWP.save(path.toStdString()) != 0) {
        m_error = QString::fromUtf8("写入文件失败: %1").arg(path);
        return false;
    }
    return true;
}

bool WaypointStore::load(const QString &path)
{
    m_error.clear();

    AP_WPArray arrWP;
    if (arrWP.load(path.toStdString()) != 0) {
        m_error = QString::fromUtf8("读取文件失败: %1").arg(path);
        return false;
    }

    m_map->WPDeleteAll();
    AP_WayPointMap *wpMap = arrWP.getAll();
    for (AP_WayPointMap::const_iterator it = wpMap->begin(); it != wpMap->end(); ++it) {
        opmap::PointLatLng coord(it->second->lat, it->second->lng);
        opmap::WayPointItem *item = m_map->WPCreate(coord, (int)it->second->alt);
        item->SetReached(false);
    }
    return true;
}
