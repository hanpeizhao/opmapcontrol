/**
******************************************************************************
*
* @file       waypoint_store.h
* @brief      航点文件(.wp)读写封装：OPMapWidget 航点 ↔ AP_WPArray
* @see        The GNU Public License (GPL) Version 3
* @{
*
*****************************************************************************/

#ifndef WAYPOINT_STORE_H
#define WAYPOINT_STORE_H

#include <QString>

namespace opmap {
class OPMapWidget;
}

/**
* @brief 航点持久化：把地图上的航点保存为 .wp 文件，或从 .wp 文件加载
*
* 本类是唯一接触 AP_WPArray 数据结构的模块，UI 层不感知文件格式。
*/
class WaypointStore
{
public:
    explicit WaypointStore(opmap::OPMapWidget *mapWidget);

    /**
     * @brief 把地图当前航点保存到 .wp 文件
     * @param path 文件路径
     * @return 成功返回 true，失败时 error() 给出原因
     */
    bool save(const QString &path);

    /**
     * @brief 从 .wp 文件加载航点（会先清空地图现有航点）
     * @param path 文件路径
     * @return 成功返回 true，失败时 error() 给出原因
     */
    bool load(const QString &path);

    QString error() const { return m_error; }

private:
    opmap::OPMapWidget *m_map;
    QString m_error;
};

#endif // WAYPOINT_STORE_H
