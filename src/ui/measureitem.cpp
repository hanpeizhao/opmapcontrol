/**
******************************************************************************
*
* @file       measureitem.cpp
* @brief      地图测距图元实现
* @see        The GNU Public License (GPL) Version 3
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
#include "measureitem.h"
#include "geoutils.h"

#include <QPainter>
#include <QtMath>

namespace opmap {

namespace {
/// 距离文本：米（<1 km）或 km，短距离取整、长距离保留合适位数
QString FormatDistance(double meters)
{
    if (meters < 1000.0)
        return QString::number(meters, 'f', 0) + QString::fromUtf8(" m");
    const double km = meters / 1000.0;
    return QString::number(km, 'f', km >= 100.0 ? 1 : 2) + QString::fromUtf8(" km");
}
}

MeasureItem::MeasureItem(MapGraphicItem *map)
    : QGraphicsItem(map),
      map(map),
      currentActive(false),
      hasPreview(false)
{
    setZValue(2);                       // 轨迹/路线之上、标记之下
    setAcceptedMouseButtons(Qt::NoButton);   // 纯展示：点击穿透到地图
    labelFont = QFont();
    labelFont.setPointSize(9);
    labelFont.setBold(true);
}

void MeasureItem::AddPoint(opmap::PointLatLng const& coord)
{
    if (!currentActive) {               // 上一段已结束：自动开新段
        currentPts.clear();
        currentActive = true;
        hasPreview = false;
    }
    currentPts.append(coord);
    RebuildGeometry();
}

void MeasureItem::SetPreviewPoint(opmap::PointLatLng const& coord)
{
    if (!currentActive || currentPts.isEmpty())
        return;
    previewPos = coord;
    hasPreview = true;
    RebuildGeometry();
}

double MeasureItem::CommitMeasure()
{
    currentActive = false;
    hasPreview = false;
    if (currentPts.size() < 2) {        // 不足两点不成测距：丢弃
        currentPts.clear();
        RebuildGeometry();
        return 0.0;
    }
    double total = 0.0;
    for (int i = 1; i < currentPts.size(); ++i)
        total += geoutils::haversineDistanceM(currentPts.at(i - 1), currentPts.at(i));
    done.append(currentPts);
    currentPts.clear();
    RebuildGeometry();
    return total;
}

void MeasureItem::ClearAll()
{
    done.clear();
    currentPts.clear();
    currentActive = false;
    hasPreview = false;
    RebuildGeometry();
}

void MeasureItem::RefreshPos()
{
    RebuildGeometry();
}

void MeasureItem::RebuildGeometry()
{
    prepareGeometryChange();
    doneScreen.clear();
    for (int i = 0; i < done.size(); ++i) {
        QPolygonF poly;
        const QList<opmap::PointLatLng> &seg = done.at(i);
        for (int j = 0; j < seg.size(); ++j) {
            const opmap::Point p = map->FromLatLngToLocal(seg.at(j));
            poly.append(QPointF(p.X(), p.Y()));
        }
        doneScreen.append(poly);
    }
    currentScreen = QPolygonF();
    for (int j = 0; j < currentPts.size(); ++j) {
        const opmap::Point p = map->FromLatLngToLocal(currentPts.at(j));
        currentScreen.append(QPointF(p.X(), p.Y()));
    }
    if (hasPreview) {
        const opmap::Point p = map->FromLatLngToLocal(previewPos);
        previewScreen = QPointF(p.X(), p.Y());
    }
    update();
}

QRectF MeasureItem::boundingRect() const
{
    QRectF r;
    for (int i = 0; i < doneScreen.size(); ++i) {
        const QPolygonF &poly = doneScreen.at(i);
        if (!poly.isEmpty())
            r = r.isEmpty() ? poly.boundingRect() : r.united(poly.boundingRect());
    }
    if (!currentScreen.isEmpty())
        r = r.isEmpty() ? currentScreen.boundingRect() : r.united(currentScreen.boundingRect());
    if (hasPreview && !currentScreen.isEmpty())
        r = r.united(QRectF(currentScreen.last(), previewScreen).normalized());
    if (r.isEmpty())
        return QRectF();
    // 距离标签会向四周外扩（字号 9pt 总计标签更宽），留足余量保证标签不被裁掉
    return r.adjusted(-80, -40, 80, 40);
}

void MeasureItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    if (doneScreen.isEmpty() && currentScreen.isEmpty())
        return;

    painter->setRenderHint(QPainter::Antialiasing, true);

    // 折线：黑底（半透明宽线）+ 黄芯双 pass，深浅瓦片上都清晰
    QPen casingPen(QColor(0, 0, 0, 140), 5);
    casingPen.setCapStyle(Qt::RoundCap);
    casingPen.setJoinStyle(Qt::RoundJoin);
    QPen corePen(QColor(255, 205, 0), 2);
    corePen.setCapStyle(Qt::RoundCap);
    corePen.setJoinStyle(Qt::RoundJoin);

    for (int pass = 0; pass < 2; ++pass) {
        painter->setPen(pass == 0 ? casingPen : corePen);
        for (int i = 0; i < doneScreen.size(); ++i) {
            const QPolygonF &poly = doneScreen.at(i);
            if (poly.size() >= 2)
                painter->drawPolyline(poly);
        }
        if (currentScreen.size() >= 2)
            painter->drawPolyline(currentScreen);
        // 橡皮筋预览：末顶点 → 鼠标，虚线
        if (pass == 1 && hasPreview && !currentScreen.isEmpty()) {
            QPen dashPen(QColor(255, 205, 0, 180), 2, Qt::DashLine);
            painter->setPen(dashPen);
            painter->drawLine(currentScreen.last(), previewScreen);
        }
    }

    // 顶点圆点（白芯黑边）
    QPen dotPen(QColor(20, 20, 20), 1.5);
    painter->setPen(dotPen);
    painter->setBrush(Qt::white);
    for (int i = 0; i < doneScreen.size(); ++i) {
        const QPolygonF &poly = doneScreen.at(i);
        for (int j = 0; j < poly.size(); ++j)
            painter->drawEllipse(poly.at(j), 4, 4);
    }
    for (int j = 0; j < currentScreen.size(); ++j)
        painter->drawEllipse(currentScreen.at(j), 4, 4);

    painter->setFont(labelFont);
    const QFontMetrics fm(labelFont);

    // 每段中点距离标签：橙底白字（终点侧抬高避让折线本体）
    for (int i = 0; i < doneScreen.size(); ++i) {
        const QPolygonF &poly = doneScreen.at(i);
        for (int j = 1; j < poly.size(); ++j) {
            const double segM = geoutils::haversineDistanceM(done.at(i).at(j - 1), done.at(i).at(j));
            const QPointF mid = (poly.at(j - 1) + poly.at(j)) / 2.0;
            const QString text = FormatDistance(segM);
            const int w = fm.width(text) + 8;
            const int h = fm.height() + 2;
            const QRectF bg(mid.x() - w / 2.0, mid.y() - h / 2.0 - 12, w, h);
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(180, 110, 0, 200));
            painter->drawRoundedRect(bg, 3, 3);
            painter->setPen(Qt::white);
            painter->drawText(bg, Qt::AlignCenter, text);
        }
    }
    for (int j = 1; j < currentScreen.size(); ++j) {
        const double segM = geoutils::haversineDistanceM(currentPts.at(j - 1), currentPts.at(j));
        const QPointF mid = (currentScreen.at(j - 1) + currentScreen.at(j)) / 2.0;
        const QString text = FormatDistance(segM);
        const int w = fm.width(text) + 8;
        const int h = fm.height() + 2;
        const QRectF bg(mid.x() - w / 2.0, mid.y() - h / 2.0 - 12, w, h);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(180, 110, 0, 200));
        painter->drawRoundedRect(bg, 3, 3);
        painter->setPen(Qt::white);
        painter->drawText(bg, Qt::AlignCenter, text);
    }

    // 预览段的瞬时距离（灰色提示，不落最终标签）
    if (hasPreview && !currentScreen.isEmpty()) {
        const double segM = geoutils::haversineDistanceM(currentPts.last(), previewPos);
        const QPointF mid = (currentScreen.last() + previewScreen) / 2.0;
        const QString text = FormatDistance(segM);
        const int w = fm.width(text) + 8;
        const int h = fm.height() + 2;
        const QRectF bg(mid.x() - w / 2.0, mid.y() - h / 2.0 - 12, w, h);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(90, 90, 90, 170));
        painter->drawRoundedRect(bg, 3, 3);
        painter->setPen(Qt::white);
        painter->drawText(bg, Qt::AlignCenter, text);
    }

    // 总距离标签：画在每段最后一个顶点处（红橙底白字加粗）
    for (int i = 0; i < doneScreen.size(); ++i) {
        const QPolygonF &poly = doneScreen.at(i);
        if (poly.isEmpty())
            continue;
        double total = 0.0;
        for (int j = 1; j < done.at(i).size(); ++j)
            total += geoutils::haversineDistanceM(done.at(i).at(j - 1), done.at(i).at(j));
        const QString text = QString::fromUtf8("总计 ") + FormatDistance(total);
        const QPointF tail = poly.last();
        const int w = fm.width(text) + 10;
        const int h = fm.height() + 4;
        const QRectF bg(tail.x() - w / 2.0, tail.y() - h - 16, w, h);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(200, 55, 0, 215));
        painter->drawRoundedRect(bg, 4, 4);
        painter->setPen(Qt::white);
        painter->drawText(bg, Qt::AlignCenter, text);
    }
    // 进行中段：当前累计距离实时跟随末顶点
    if (!currentScreen.isEmpty() && currentPts.size() >= 2) {
        double total = 0.0;
        for (int j = 1; j < currentPts.size(); ++j)
            total += geoutils::haversineDistanceM(currentPts.at(j - 1), currentPts.at(j));
        const QString text = QString::fromUtf8("已测 ") + FormatDistance(total);
        const QPointF tail = hasPreview ? previewScreen : currentScreen.last();
        const int w = fm.width(text) + 10;
        const int h = fm.height() + 4;
        const QRectF bg(tail.x() - w / 2.0, tail.y() - h - 16, w, h);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(200, 55, 0, 175));
        painter->drawRoundedRect(bg, 4, 4);
        painter->setPen(Qt::white);
        painter->drawText(bg, Qt::AlignCenter, text);
    }
}

int MeasureItem::type() const
{
    return Type;
}

} // end of namespace opmap
