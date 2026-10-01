/**
******************************************************************************
* @file       arclineitem.cpp
* @brief      贝塞尔弧线航线图元实现（见 arclineitem.h）
******************************************************************************
*/

#include "arclineitem.h"

#include <QPainter>
#include <QTimer>
#include <QtCore/qmath.h>

#include "mapgraphicitem.h"
#include "geoutils.h"

namespace opmap {

namespace {

/// 拱顶偏移系数：拱高 = 端点球面距离 × 该系数（0.18 在全国视野下弧度醒目且不夸张）
const double kApexRatio = 0.18;

/// 弧长按参数近似均分：贝塞尔中段参数-弧长非线性轻微，箭头均布误差可忽略
const double kFlowSegLen = 0.07;   ///< 单个流动亮段的参数长度

/// 在 path 上取 [u0,u1] 参数区间的采样折线（u1 截断到 1.0）
QPolygonF PathSlice(const QPainterPath &path, double u0, double u1, int samples)
{
    QPolygonF poly;
    for (int i = 0; i <= samples; ++i) {
        const double u = u0 + (u1 - u0) * i / samples;
        if (u > 1.0)
            break;
        poly.append(path.pointAtPercent(u));
    }
    return poly;
}

} // anonymous namespace

ArcLineItem::ArcLineItem(MapGraphicItem *map, const opmap::PointLatLng &from,
                         const opmap::PointLatLng &to, const QColor &color, int side,
                         QGraphicsItem *parent)
    : QObject(),
      QGraphicsItem(parent),
      m_map(map),
      m_from(from),
      m_to(to),
      m_color(color),
      m_arrows(3),
      m_flow(true),
      m_flowPhase(0.0),
      m_flowTimer(0)
{
    setPos(0, 0);                       // 路径直接用 map 局部坐标，item 锚在原点
    setZValue(2);                       // 轨迹/路线之上、标记之下
    setAcceptedMouseButtons(Qt::NoButton);

    // 拱顶控制点：球面中点沿航向垂直方向外推 0.18 倍距离
    const double distM = geoutils::haversineDistanceM(from, to);
    const double brgDeg = geoutils::bearingDeg(from, to);
    const opmap::PointLatLng mid = geoutils::destPoint(from, brgDeg, distM / 2.0);
    m_apex = geoutils::destPoint(mid, brgDeg + 90.0 * side, distM * kApexRatio);

    BuildPath();

    // 流动光效：120ms 推进相位，隐藏时暂停避免无效重绘
    m_flowTimer = new QTimer(this);
    m_flowTimer->setInterval(120);
    connect(m_flowTimer, SIGNAL(timeout()), this, SLOT(onFlowTick()));
    m_flowTimer->start();
}

ArcLineItem::~ArcLineItem()
{
}

void ArcLineItem::SetFlowEnabled(bool on)
{
    m_flow = on;
    if (on)
        m_flowTimer->start();
    else
        m_flowTimer->stop();
    update();
}

void ArcLineItem::onFlowTick()
{
    if (!isVisible())
        return;
    m_flowPhase += 0.04;
    if (m_flowPhase >= 1.0)
        m_flowPhase -= 1.0;
    update();
}

opmap::PointLatLng ArcLineItem::ArcPointAt(double t) const
{
    // 地理空间二次贝塞尔：P = (1-t)²·P0 + 2(1-t)t·Pa + t²·P1
    t = qBound(0.0, t, 1.0);
    const double w0 = (1.0 - t) * (1.0 - t);
    const double w1 = 2.0 * (1.0 - t) * t;
    const double w2 = t * t;
    return opmap::PointLatLng(
                w0 * m_from.Lat() + w1 * m_apex.Lat() + w2 * m_to.Lat(),
                w0 * m_from.Lng() + w1 * m_apex.Lng() + w2 * m_to.Lng());
}

void ArcLineItem::RefreshPos()
{
    BuildPath();
    update();
}

void ArcLineItem::BuildPath()
{
    const opmap::Point p1 = m_map->FromLatLngToLocal(m_from);
    const opmap::Point pa = m_map->FromLatLngToLocal(m_apex);
    const opmap::Point p2 = m_map->FromLatLngToLocal(m_to);

    m_path = QPainterPath();
    m_path.moveTo(p1.X(), p1.Y());
    m_path.quadTo(pa.X(), pa.Y(), p2.X(), p2.Y());

    prepareGeometryChange();
    m_bounding = m_path.boundingRect().adjusted(-14, -14, 14, 14);   // 余量容纳箭头/光点
}

QRectF ArcLineItem::boundingRect() const
{
    return m_bounding;
}

void ArcLineItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    if (m_path.isEmpty())
        return;
    painter->setRenderHint(QPainter::Antialiasing, true);

    // 底层半透明宽衬（暗底图上增强对比）+ 主弧线
    QPen base(m_color);
    base.setWidthF(5.0);
    base.setCapStyle(Qt::RoundCap);
    base.setColor(QColor(m_color.red(), m_color.green(), m_color.blue(), 70));
    painter->setPen(base);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(m_path);

    QPen main(m_color);
    main.setWidthF(2.2);
    main.setCapStyle(Qt::RoundCap);
    painter->setPen(main);
    painter->drawPath(m_path);

    // 沿弧均布方向箭头：按参数取点与切线方向，画实心三角
    if (m_arrows > 0) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_color);
        for (int i = 0; i < m_arrows; ++i) {
            const double u = (i + 1.0) / (m_arrows + 1.0);
            const QPointF p = m_path.pointAtPercent(u);
            const QPointF q = m_path.pointAtPercent(qMin(1.0, u + 0.01));
            const double ang = qAtan2(q.y() - p.y(), q.x() - p.x());
            QPolygonF tri;
            tri.append(QPointF(9.0, 0.0));
            tri.append(QPointF(-5.0, 4.5));
            tri.append(QPointF(-5.0, -4.5));
            painter->save();
            painter->translate(p);
            painter->rotate(qRadiansToDegrees(ang));
            painter->drawPolygon(tri);
            painter->restore();
        }
    }

    // 流动光效：3 段亮色短弧沿航线循环推进（ECharts 迁徙图 trails 风格）
    if (m_flow) {
        QPen flow(Qt::white);
        flow.setWidthF(2.8);
        flow.setCapStyle(Qt::RoundCap);
        flow.setColor(QColor(255, 255, 255, 210));
        painter->setPen(flow);
        painter->setBrush(Qt::NoBrush);
        for (int i = 0; i < 3; ++i) {
            const double u0 = m_flowPhase + i / 3.0;
            const QPolygonF seg = PathSlice(m_path, u0, u0 + kFlowSegLen, 8);
            if (seg.size() >= 2)
                painter->drawPolyline(seg);
        }
    }
}

} // end of namespace opmap
