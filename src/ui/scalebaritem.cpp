/**
******************************************************************************
*
* @file       scalebaritem.cpp
* @brief      地图比例尺叠加图元实现
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
#include "scalebaritem.h"
#include "mapgraphicitem.h"

#include <QPainter>
#include <QFont>
#include <QGraphicsScene>
#include <cmath>

namespace opmap {

namespace {
const int kBarTargetPx = 110;   ///< 标尺目标像素宽（实际宽按最整距离略有出入）
const int kBarThickness = 2;    ///< 标尺主线宽（像素）
}

ScaleBarItem::ScaleBarItem(MapGraphicItem *map, QGraphicsItem *parent)
    : QObject(),
      QGraphicsItem(parent),
      map(map)
{
    setZValue(3);          // 与罗盘同层：瓦片之上、鼠标事件不遮挡地图交互
    setAcceptedMouseButtons(Qt::NoButton);
}

void ScaleBarItem::Reposition()
{
    if (!scene())
        return;
    const QRectF sr = scene()->sceneRect();
    if (sr.width() <= 0 || sr.height() <= 0)
        return;
    // 左下角：横留 10px、底留 14px（避开 statusBar 一侧不留也没关系，场景内即视野内）
    setPos(10, sr.height() - 44);
}

QRectF ScaleBarItem::boundingRect() const
{
    // 目标 110px 标尺 + 两端刻度 + 文本余量
    return QRectF(-4, -2, kBarTargetPx + 60, 40);
}

void ScaleBarItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    if (!map)
        return;

    // 地面分辨率：当前缩放级别、视野中心纬度处的 米/像素
    const double mpp = map->Projection()->GetGroundResolution(map->ZoomTotal(),
                                                              map->CurrentPosition().Lat());
    if (mpp <= 0.0)
        return;

    // 在 1/2/5×10ⁿ 系列里取最接近目标像素宽的距离（人读整数标尺惯例）
    const double target = kBarTargetPx * mpp;
    const double pow10 = std::pow(10.0, std::floor(std::log10(target)));
    const double cands[4] = { pow10, 2.0 * pow10, 5.0 * pow10, 10.0 * pow10 };
    double best = cands[0];
    for (int i = 1; i < 4; ++i)
        if (std::fabs(cands[i] - target) < std::fabs(best - target))
            best = cands[i];
    const double barPx = best / mpp;
    if (barPx <= 0.0)
        return;

    // 距离文本：<1000 m 用米，否则用 km（10 km 以上不留小数）
    QString label;
    if (best >= 1000.0) {
        const double km = best / 1000.0;
        label = QString::number(km, 'f', km >= 10.0 ? 0 : 1) + QString::fromLatin1(" km");
    } else {
        label = QString::number((int)best) + QString::fromLatin1(" m");
    }

    painter->setRenderHint(QPainter::Antialiasing, true);
    QFont f = painter->font();
    f.setPointSize(9);
    f.setBold(true);
    painter->setFont(f);

    const int barY = 26;   // 标尺基线（文本画在其上方）

    // 白色描边打底 + 黑色主线：浅色/深色瓦片上都清晰
    QPen casingPen(Qt::white, kBarThickness + 3);
    casingPen.setCapStyle(Qt::FlatCap);
    QPen corePen(QColor(20, 20, 20), kBarThickness);
    corePen.setCapStyle(Qt::FlatCap);
    for (int pass = 0; pass < 2; ++pass) {
        painter->setPen(pass == 0 ? casingPen : corePen);
        painter->drawLine(QPointF(0, barY), QPointF(barPx, barY));
        // 两端刻度（竖向短杠）
        painter->drawLine(QPointF(0, barY - 5), QPointF(0, barY + 5));
        painter->drawLine(QPointF(barPx, barY - 5), QPointF(barPx, barY + 5));
    }

    // 文本：半透明圆角底 + 白字，画在标尺上方居中
    const QFontMetrics fm(f);
    const QRectF textRect(0, 0, barPx, barY - 4);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0, 0, 0, 130));
    painter->drawRoundedRect(textRect, 3, 3);
    painter->setPen(Qt::white);
    painter->drawText(textRect, Qt::AlignCenter, label);
}

} // end of namespace opmap
