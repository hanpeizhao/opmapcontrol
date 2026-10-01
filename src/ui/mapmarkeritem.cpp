/**
******************************************************************************
*
* @file       mapmarkeritem.cpp
* @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
* @brief      A graphicsItem pinning an image and/or a text label to a map location
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
#include "mapmarkeritem.h"

namespace opmap {

MapMarkerItem::MapMarkerItem(const opmap::PointLatLng &coord, MapGraphicItem *map) :
    map(map),
    coord(coord),
    imgW(0),
    imgH(0)
{
    font.setPointSize(10);
    font.setBold(true);
    setFlag(QGraphicsItem::ItemIgnoresTransformations, true);   // 屏幕尺寸恒定，不随瓦片缩放
    setFlag(QGraphicsItem::ItemIsSelectable, true);   // 可选中（供上层"删除选中标记"等操作）；不可拖动，移动用 SetCoord
    setZValue(5);   // 纯装饰最上层（UAV=4），不被飞行器图标遮挡
    RefreshPos();
}

void MapMarkerItem::SetCoord(opmap::PointLatLng const& value)
{
    coord = value;
    RefreshPos();
    update();
}

void MapMarkerItem::SetImage(QString const& imagePath)
{
    picture = QPixmap(imagePath);   // 加载失败为 null，paint 回退红点
    updateDisplay();
    prepareGeometryChange();
    update();
}

void MapMarkerItem::SetImageSize(int width, int height)
{
    imgW = width;
    imgH = height;
    updateDisplay();
    prepareGeometryChange();
    update();
}

void MapMarkerItem::SetText(QString const& value)
{
    text = value;
    prepareGeometryChange();
    update();
}

void MapMarkerItem::SetFontSize(int pointSize)
{
    font.setPointSize(pointSize);
    prepareGeometryChange();
    update();
}

void MapMarkerItem::RefreshPos()
{
    opmap::Point point = map->FromLatLngToLocal(coord);
    setPos(point.X(), point.Y());
}

int MapMarkerItem::type() const
{
    return Type;
}

void MapMarkerItem::updateDisplay()
{
    if (picture.isNull() || (imgW <= 0 && imgH <= 0)) {
        display = picture;   // 未定尺寸直接用原图
        return;
    }
    QSize s = picture.size();
    if (imgW > 0 && imgH > 0)
        s = QSize(imgW, imgH);   // 两维都给定：精确拉伸
    else if (imgW > 0)
        s = QSize(imgW, qMax(1, picture.height() * imgW / picture.width()));   // 只给宽：等比
    else
        s = QSize(qMax(1, picture.width() * imgH / picture.height()), imgH);   // 只给高：等比
    display = picture.scaled(s, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QRectF MapMarkerItem::boundingRect() const
{
    // 默认红点占位范围
    int halfW = 5;
    double top = -5.0;
    double bottom = 5.0;

    if (!display.isNull()) {
        halfW = qMax(halfW, display.width() / 2);
        top = -display.height() - 1.0;   // 图片底边中点对准锚点
        bottom = qMax(bottom, 1.0);
    }
    if (!text.isEmpty()) {
        const QFontMetrics fm(font);
        halfW = qMax(halfW, fm.horizontalAdvance(text) / 2 + 1);
        bottom = qMax(bottom, 2 + double(fm.height()));   // 文字在锚点下方 2px 起
    }
    return QRectF(-halfW - 1.0, top, 2 * halfW + 2.0, bottom - top + 1.0);
}

void MapMarkerItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    if (!display.isNull()) {
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter->drawPixmap(-display.width() / 2, -display.height(), display);
    } else if (text.isEmpty()) {
        // 无图无字：红点占位，保证标记在图上可见可发现
        painter->setBrush(Qt::red);
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(QPointF(0, 0), 5, 5);
    }

    if (!text.isEmpty()) {
        painter->setFont(font);
        const QFontMetrics fm(font);
        const double tx = -fm.horizontalAdvance(text) / 2.0;
        const double ty = 2 + fm.ascent();   // 文字顶部距锚点 2px，水平居中
        QPainterPath outline;
        outline.addText(QPointF(tx, ty), font, text);
        // 黑描边 + 白填充：亮暗底图上都清晰可读
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(Qt::black, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->drawPath(outline);
        painter->setPen(Qt::NoPen);
        painter->setBrush(Qt::white);
        painter->drawPath(outline);
    }

    if (isSelected()) {   // 选中反馈：青色虚线框围住全部内容（与航点选中框同语义）
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(Qt::cyan, 1, Qt::DashLine));
        painter->drawRect(boundingRect());
    }
}

} // end of namespace opmap
