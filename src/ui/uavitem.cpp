/**
******************************************************************************
*
* @file       uavitem.cpp
* @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
* @brief      A graphicsItem representing a UAV
* @see        The GNU Public License (GPL) Version 3
* @defgroup   OPMapWidget
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

#include "pureprojection.h"
#include "uavitem.h"
#include "geoutils.h"

namespace opmap {

UAVItem::UAVItem(MapGraphicItem* map,OPMapWidget* parent,QString uavPic) :
    map(map), mapwidget(parent),
    showtrail(true), showtrailline(true),
    trailtime(5), traildistance(5),
    autosetreached(true), autosetdistance(3)
{
    //QDir dir(":/uavs/images/");
    //QStringList list=dir.entryList();
    pic.load(uavPic);
    // Don't scale but trust the image we are given
    // pic=pic.scaled(50,33,Qt::IgnoreAspectRatio);
    localposition=map->FromLatLngToLocal(mapwidget->CurrentPosition());
    this->setPos(localposition.X(),localposition.Y());
    this->setZValue(4);
    trail=new QGraphicsItemGroup();
    trail->setParentItem(map);
    trailLine=new QGraphicsItemGroup();
    trailLine->setParentItem(map);
    this->setFlag(QGraphicsItem::ItemIgnoresTransformations,true);
    mapfollowtype=UAVMapFollowType::None;
    trailtype=UAVTrailType::ByDistance;
    timer.start();
}

UAVItem::~UAVItem()
{
    delete trail;
    delete trailLine;

    trail = NULL;
    trailLine = NULL;
}

void UAVItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    // painter->rotate(-90);
    QPainter::RenderHints oldhints = painter->renderHints();
    painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    painter->drawPixmap(-pic.width()/2,-pic.height()/2,pic);
    painter->setRenderHints(oldhints);
    //   painter->drawRect(QRectF(-pic.width()/2,-pic.height()/2,pic.width()-1,pic.height()-1));
}

QRectF UAVItem::boundingRect()const
{
    return QRectF(-pic.width()/2,-pic.height()/2,pic.width(),pic.height());
}

/**
 * @brief 轨迹采样统一入口：先做瞬移检测，再加轨迹点、连线
 *
 * 根治"跨场直线"问题：位置瞬移（任务重启回摆、轨迹回放换场、位置源切换等）
 * 不代表真实运动，若仍从上一采样点连线就会拉出一条假直线。判据为单帧球面
 * 位移超过物理上限 kTeleportThresholdM——本机巡航 80 m/s、喂点帧距百米级，
 * 1000 m 只可能是瞬移。触发即清空旧轨迹（点+线），从新位置重新记录。
 * 各瞬移入口处的手动 DeleteTrail 因此从"必须记得调"降级为"提前清更及时"
 * 的可选项，新增瞬移场景无需再逐处打补丁。
 */
void UAVItem::AppendTrailSample(const opmap::PointLatLng &position, const int &altitude,
                                const QColor &color)
{
    const double kTeleportThresholdM = 1000.0;
    if(!lasttrailline.IsEmpty()
       && opmap::geoutils::haversineDistanceM(lasttrailline, position) > kTeleportThresholdM)
        DeleteTrail();   // 瞬移：旧轨迹作废，从新位置重记

    trail->addToGroup(new TrailItem(position,altitude,color,map));
    if(!lasttrailline.IsEmpty())
        trailLine->addToGroup(new TrailLineItem(lasttrailline,position,color,map));
    lasttrailline=position;
}

void UAVItem::SetUAVPos(const opmap::PointLatLng &position, const int &altitude,
                        const QColor &color)
{
    if(coord.IsEmpty())
        lastcoord=coord;

    if(coord!=position) {
        if(trailtype==UAVTrailType::ByTimeElapsed) {
            if(timer.elapsed() > trailtime*1000) {
                AppendTrailSample(position,altitude,color);
                timer.restart();
            }
        } else if(trailtype==UAVTrailType::ByDistance) {
            if(qAbs(opmap::PureProjection::DistanceBetweenLatLng(lastcoord,position)*1000) > traildistance) {
                AppendTrailSample(position,altitude,color);
                lastcoord=position;
            }
        }

        coord=position;
        this->altitude=altitude;
        RefreshPos();

        if(mapfollowtype==UAVMapFollowType::CenterAndRotateMap||mapfollowtype==UAVMapFollowType::CenterMap) {
            mapwidget->SetCurrentPosition(coord);
        }
        this->update();

        if(autosetreached) {
            foreach(QGraphicsItem* i,map->childItems()) {
                WayPointItem* wp=qgraphicsitem_cast<WayPointItem*>(i);
                if(wp) {
                    if(Distance3D(wp->Coord(),wp->Altitude())<autosetdistance) {
                        wp->SetReached(true);
                        emit UAVReachedWayPoint(wp->Number(),wp);
                    }
                }
            }
        }

        if(mapwidget->Home != 0) {
            //verify if the UAV is inside the safety bouble
            if(Distance3D(mapwidget->Home->Coord(),mapwidget->Home->Altitude())>mapwidget->Home->SafeArea()) {
                if(mapwidget->Home->safe!=false) {
                    mapwidget->Home->safe=false;
                    mapwidget->Home->update();
                    emit UAVLeftSafetyBouble(this->coord);
                }
            } else {
                if(mapwidget->Home->safe!=true) {
                    mapwidget->Home->safe=true;
                    mapwidget->Home->update();
                    emit UAVEnteredSafetyBouble(this->coord);   // 与飞出成对：回圈也通知
                }
            }
        }
    }
}

/**
      * Rotate the UAV Icon on the map, or rotate the map
      * depending on the display mode
      */
void UAVItem::SetUAVHeading(const qreal &value)
{
    if(mapfollowtype==UAVMapFollowType::CenterAndRotateMap) {
        mapwidget->SetRotate(-value);
    } else {
        if (this->rotation() != value)
            this->setRotation(value);
    }
}


int UAVItem::type()const
{
    return Type;
}


void UAVItem::RefreshPos()
{
    localposition=map->FromLatLngToLocal(coord);
    this->setPos(localposition.X(), localposition.Y());

    foreach(QGraphicsItem* i,trail->childItems()) {
        TrailItem* w=qgraphicsitem_cast<TrailItem*>(i);
        if(w)
            w->setPos(map->FromLatLngToLocal(w->coord).X(),map->FromLatLngToLocal(w->coord).Y());
    }

    foreach(QGraphicsItem* i,trailLine->childItems()) {
        TrailLineItem* ww=qgraphicsitem_cast<TrailLineItem*>(i);
        if(ww)
            ww->setLine(map->FromLatLngToLocal(ww->coord1).X(),map->FromLatLngToLocal(ww->coord1).Y(),map->FromLatLngToLocal(ww->coord2).X(),map->FromLatLngToLocal(ww->coord2).Y());
    }
}

void UAVItem::SetTrailType(const UAVTrailType::Types &value)
{
    trailtype=value;
    if(trailtype==UAVTrailType::ByTimeElapsed)
        timer.restart();
}

void UAVItem::SetShowTrail(const bool &value)
{
    showtrail=value;
    trail->setVisible(value);
}

void UAVItem::SetShowTrailLine(const bool &value)
{
    showtrailline=value;
    trailLine->setVisible(value);
}

void UAVItem::DeleteTrail()
{
    foreach(QGraphicsItem* i,trail->childItems())
        delete i;

    foreach(QGraphicsItem* i,trailLine->childItems())
        delete i;

    // 轨迹线起点记忆置空：清空后首个采样点只加点不画线，第二个采样点
    // 才从新起点连线。（此前复位为 coord——清空时 UAV 所在位置——会让
    // 首喂点与该位置之间拉出幽灵连线，如开始导航时原位置→起点）
    lasttrailline=PointLatLng();
}

double UAVItem::Distance3D(const opmap::PointLatLng &coord, const int &altitude)
{
    return sqrt(pow(opmap::PureProjection::DistanceBetweenLatLng(this->coord,coord)*1000,2)+
                pow(static_cast<float>(this->altitude-altitude),2));
}

void UAVItem::SetUavPic(QString UAVPic)
{
    pic.load(":/uavs/images/"+UAVPic);
    prepareGeometryChange();
    update();
}

void UAVItem::SetIcon(QString const& iconPath)
{
    pic.load(iconPath);
    prepareGeometryChange();
    update();
}

} // end of namespace opmap
