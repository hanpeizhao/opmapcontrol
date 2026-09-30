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
    // 拖动/缩放跟随由 MapGraphicItem::ChildPosRefresh 统一驱动（OnMapDrag 信号在 core 上，
    // 不能直接 connect map），与航点/UAV/Home 子项机制一致
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

    painter->setRenderHint(QPainter::Antialiasing, true);

    // 取点预览：不足 3 点时画顶点圆点与连线，便于取点过程可见
    if (screenPolygon.size() < 3) {
        painter->setPen(QPen(QColor(220, 40, 40), 2));
        if (screenPolygon.size() == 2)
            painter->drawLine(screenPolygon.at(0), screenPolygon.at(1));
        painter->setBrush(QBrush(QColor(220, 40, 40)));
        for (int i = 0; i < screenPolygon.size(); ++i)
            painter->drawEllipse(screenPolygon.at(i), 4, 4);
        return;
    }

    QColor fill(Qt::red);
    fill.setAlpha(28);
    painter->setPen(QPen(QColor(220, 40, 40), 2, Qt::DashLine));
    painter->setBrush(QBrush(fill));
    painter->drawPolygon(screenPolygon);

    // 顶点圆点标记（不填充，与预览态区分闭合区域）
    painter->setBrush(QBrush(QColor(220, 40, 40)));
    for (int i = 0; i < screenPolygon.size(); ++i)
        painter->drawEllipse(screenPolygon.at(i), 4, 4);
}

void GeofenceItem::RefreshPos()
{
    RefreshPolygon();
    update();
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
