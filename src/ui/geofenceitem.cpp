/**
******************************************************************************
*
* @file       geofenceitem.cpp
* @brief      多边形地理围栏实现：经纬度顶点 ↔ 屏幕坐标转换与绘制
* @see        The GNU Public License (GPL) Version 3
* @{
*
*****************************************************************************/
#include "geofenceitem.h"

#include <QtGui/QPainter>

namespace opmap {

GeofenceItem::GeofenceItem(MapGraphicItem *map, QGraphicsItem *parent)
    : QObject(),
      QGraphicsItem(parent),
      map(map),
      vertices()
{
    setZValue(4);
    RefreshPolygon();
    connect(map, SIGNAL(OnMapDrag()), this, SLOT(RefreshPolygon()));
    connect(map, SIGNAL(OnMapZoomChanged()), this, SLOT(RefreshPolygon()));
}

void GeofenceItem::SetVertices(const QList<opmap::PointLatLng> &value)
{
    vertices = value;
    RefreshPolygon();
    update();
}

QRectF GeofenceItem::boundingRect() const
{
    return screenPolygon.boundingRect().adjusted(-8, -8, 8, 8);
}

void GeofenceItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    if (screenPolygon.size() < 3)
        return;

    painter->setRenderHint(QPainter::Antialiasing, true);
    QColor fill(Qt::red);
    fill.setAlpha(28);
    painter->setPen(QPen(QColor(220, 40, 40), 2, Qt::DashLine));
    painter->setBrush(QBrush(fill));
    painter->drawPolygon(screenPolygon);
}

void GeofenceItem::RefreshPolygon()
{
    if (map)
        prepareGeometryChange();
    screenPolygon.clear();
    for (int i = 0; i < vertices.size(); ++i) {
        opmap::Point point = map->FromLatLngToLocal(vertices.at(i));
        screenPolygon.append(QPointF(point.X(), point.Y()));
    }
}

bool GeofenceItem::Contains(const QList<opmap::PointLatLng> &polygon, const opmap::PointLatLng &point)
{
    // 射线法：从 point 向右引射线，按与各边的交点数奇偶判定
    const int n = polygon.size();
    if (n < 3)
        return false;

    bool inside = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        const double xi = polygon.at(i).Lng();
        const double yi = polygon.at(i).Lat();
        const double xj = polygon.at(j).Lng();
        const double yj = polygon.at(j).Lat();
        const bool intersect = ((yi > point.Lat()) != (yj > point.Lat())) &&
                (point.Lng() < (xj - xi) * (point.Lat() - yi) / (yj - yi) + xi);
        if (intersect)
            inside = !inside;
    }
    return inside;
}

} // namespace opmap
