/**
******************************************************************************
* @file       routeitem.cpp
* @brief      车载导航路线绘制项实现（见 routeitem.h）
******************************************************************************
*/

#include "routeitem.h"

#include <QPainter>

#include "geoutils.h"

namespace opmap {

namespace {

const int kMarkerRadiusPx = 6;

} // anonymous namespace

RouteItem::RouteItem(MapGraphicItem *map)
    : QObject(),
      QGraphicsItem(),
      m_map(map),
      m_traveledM(0),
      m_traveledPath(new QGraphicsPathItem(this)),
      m_remainingPath(new QGraphicsPathItem(this)),
      m_originMark(new QGraphicsEllipseItem(this)),
      m_destMark(new QGraphicsEllipseItem(this))
{
    setParentItem(map);
    setFlag(QGraphicsItem::ItemHasNoContents);   // 自身无绘制内容，仅承载子项

    // 路线仅作展示，不吞鼠标事件（保证地图拖动/点选不受影响）
    setAcceptedMouseButtons(Qt::NoButton);
    m_traveledPath->setAcceptedMouseButtons(Qt::NoButton);
    m_remainingPath->setAcceptedMouseButtons(Qt::NoButton);
    m_originMark->setAcceptedMouseButtons(Qt::NoButton);
    m_destMark->setAcceptedMouseButtons(Qt::NoButton);

    QPen traveledPen(QColor(150, 150, 150), 2);
    traveledPen.setCapStyle(Qt::RoundCap);
    traveledPen.setJoinStyle(Qt::RoundJoin);
    m_traveledPath->setPen(traveledPen);

    QPen remainPen(QColor(0, 120, 255), 3);
    remainPen.setCapStyle(Qt::RoundCap);
    remainPen.setJoinStyle(Qt::RoundJoin);
    m_remainingPath->setPen(remainPen);

    m_originMark->setPen(QPen(Qt::green, 2));
    m_originMark->setBrush(QBrush(QColor(0, 180, 0, 120)));
    m_destMark->setPen(QPen(Qt::red, 2));
    m_destMark->setBrush(QBrush(QColor(220, 0, 0, 120)));

    m_traveledPath->setZValue(1);
    m_remainingPath->setZValue(1);
    m_originMark->setZValue(2);
    m_destMark->setZValue(2);

    hideChildren();
    setVisible(false);

    // 地图拖动/缩放/瓦片刷新后重投影（与 WaypointLineItem 同机制）
    connect(map, SIGNAL(mapChanged()), this, SLOT(RefreshPos()));
}

void RouteItem::SetRoute(const opmap::Route &route)
{
    m_route = route;
    m_traveledM = 0;
    m_cumDist.clear();
    if (route.isValid()) {
        m_cumDist.reserve(route.polyline.size());
        m_cumDist.append(0.0);
        for (int i = 1; i < route.polyline.size(); ++i) {
            m_cumDist.append(m_cumDist.last()
                             + geoutils::haversineDistanceM(route.polyline.at(i - 1),
                                                            route.polyline.at(i)));
        }
    }
    setVisible(true);
    RefreshPos();
}

void RouteItem::SetTraveledDistance(double meters)
{
    if (m_cumDist.isEmpty())
        return;
    m_traveledM = qBound(0.0, meters, m_cumDist.last());
    RefreshPos();
}

void RouteItem::ClearRoute()
{
    m_route = opmap::Route();
    m_cumDist.clear();
    m_traveledM = 0;
    hideChildren();
    setVisible(false);
}

void RouteItem::RefreshPos()
{
    const int n = m_route.polyline.size();
    if (n < 2 || m_cumDist.size() != n) {
        hideChildren();
        return;
    }

    // 一次性重投影全部折线点（投影未就绪时整体隐藏，等下次刷新）
    QVector<QPointF> locals(n);
    for (int i = 0; i < n; ++i) {
        opmap::Point lp = m_map->FromLatLngToLocal(m_route.polyline.at(i));
        if (lp.IsEmpty()) {
            hideChildren();
            return;
        }
        locals[i] = QPointF(lp.X(), lp.Y());
    }

    QPainterPath trav;
    QPainterPath rem;

    const double total = m_cumDist.last();
    if (m_traveledM <= 0.0) {
        rem.moveTo(locals.at(0));
        for (int i = 1; i < n; ++i)
            rem.lineTo(locals.at(i));
    } else if (m_traveledM >= total) {
        trav.moveTo(locals.at(0));
        for (int i = 1; i < n; ++i)
            trav.lineTo(locals.at(i));
    } else {
        // 分割点所在段：最后一个累计距离 <= 已行驶距离的段
        int k = 0;
        while (k + 2 < n && m_cumDist.at(k + 1) <= m_traveledM)
            ++k;
        const double segLen = m_cumDist.at(k + 1) - m_cumDist.at(k);
        const double t = segLen > 0 ? (m_traveledM - m_cumDist.at(k)) / segLen : 0.0;
        const QPointF split(locals.at(k).x() + t * (locals.at(k + 1).x() - locals.at(k).x()),
                            locals.at(k).y() + t * (locals.at(k + 1).y() - locals.at(k).y()));

        trav.moveTo(locals.at(0));
        for (int i = 1; i <= k; ++i)
            trav.lineTo(locals.at(i));
        trav.lineTo(split);

        rem.moveTo(split);
        for (int i = k + 1; i < n; ++i)
            rem.lineTo(locals.at(i));
    }

    m_traveledPath->setPath(trav);
    m_remainingPath->setPath(rem);
    m_originMark->setRect(locals.at(0).x() - kMarkerRadiusPx, locals.at(0).y() - kMarkerRadiusPx,
                          kMarkerRadiusPx * 2, kMarkerRadiusPx * 2);
    m_destMark->setRect(locals.last().x() - kMarkerRadiusPx, locals.last().y() - kMarkerRadiusPx,
                        kMarkerRadiusPx * 2, kMarkerRadiusPx * 2);

    m_traveledPath->show();
    m_remainingPath->show();
    m_originMark->show();
    m_destMark->show();
}

void RouteItem::hideChildren()
{
    m_traveledPath->hide();
    m_remainingPath->hide();
    m_originMark->hide();
    m_destMark->hide();
}

} // namespace opmap
