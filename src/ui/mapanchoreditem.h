/**
******************************************************************************
*
* @file       mapanchoreditem.h
* @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
* @brief      地理锚定图元接口：实现此接口的子图元由 MapGraphicItem 统一驱动
*             在地图拖动/缩放/失效时刷新 coord→屏幕位置，无需修改分派链
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
#ifndef MAPANCHOREDITEM_H
#define MAPANCHOREDITEM_H

namespace opmap {

/**
* @brief 地理锚定图元纯接口（mixin）
*
* 新增地图子图元时，令其多继承本接口并实现 RefreshPos()，
* 即自动被 MapGraphicItem 的刷新循环（ChildPosRefresh / Core_OnNeedInvalidation）
* 通过 dynamic_cast 识别并驱动，地图拖动/缩放时钉死在正确地理位置。
* 本接口非 QObject，可与 QObject+QGraphicsItem 多继承共存。
*
* @class MapAnchoredItem mapanchoreditem.h "mapanchoreditem.h"
*/
class MapAnchoredItem
{
public:
    virtual ~MapAnchoredItem() {}
    /// 地图拖动/缩放后由 MapGraphicItem 调用，子类根据 coord 重算屏幕位置
    virtual void RefreshPos() = 0;
};

}
#endif // MAPANCHOREDITEM_H
