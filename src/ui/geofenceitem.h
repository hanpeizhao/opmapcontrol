/**
******************************************************************************
*
* @file       geofenceitem.h
* @brief      多边形地理围栏：绘制围栏多边形并提供点在多边形内判定（射线法）。
*             语义：多边形内部为允许飞行区，UAV 飞出即触发 geofenceBreach
* @see        The GNU Public License (GPL) Version 3
* @{
*
*****************************************************************************/
#ifndef GEOFENCEITEM_H
#define GEOFENCEITEM_H

#include <QtCore/QList>
#include <QtGui/QColor>

#include "pointlatlng.h"
#include "mapgraphicitem.h"
#include "mapanchoreditem.h"

namespace opmap {

class GeofenceItem : public QObject, public QGraphicsItem, public MapAnchoredItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    explicit GeofenceItem(MapGraphicItem *map, QGraphicsItem *parent = 0);

    enum { Type = UserType + 9 };   ///< qgraphicsitem_cast 依据；缺失时会与默认 type()=1 的子项（轨迹组等）误匹配

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
    int type() const;   ///< 返回 Type；缺失重写时 qgraphicsitem_cast 运行期取基类 type()=1，分派永不命中

    const QList<opmap::PointLatLng>& Vertices() const { return vertices; }
    void SetVertices(const QList<opmap::PointLatLng> &value);

    /// 地图拖动/缩放时由 MapGraphicItem::ChildPosRefresh 统一驱动重算屏幕多边形
    virtual void RefreshPos();

    /// 射线法：point 是否在多边形内部（围栏允许区内）
    static bool Contains(const QList<opmap::PointLatLng> &polygon, const opmap::PointLatLng &point);

private slots:
    void RefreshPolygon();

private:
    MapGraphicItem *map;
    QList<opmap::PointLatLng> vertices;   ///< 围栏顶点（WGS-84）
    QPolygonF screenPolygon;              ///< 屏幕坐标多边形（随地图刷新）
};

} // namespace opmap

#endif // GEOFENCEITEM_H
