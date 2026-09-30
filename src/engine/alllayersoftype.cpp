/**
******************************************************************************
*
* @file       alllayersoftype.cpp
* @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
* @brief      地图类型→图层列表拆分（如 Hybrid = 影像 + 路网）      
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

#include "alllayersoftype.h"

namespace opmap {

AllLayersOfType::AllLayersOfType()
{

}

QVector<MapType::Types> AllLayersOfType::GetAllLayersOfType(const MapType::Types &type)
{
    QVector<MapType::Types> types;

    {
        switch(type)
        {
        case MapType::GoogleHybrid:
            types.append(MapType::GoogleSatellite);
            types.append(MapType::GoogleLabels);
            break;

        case MapType::AutoNaviHybrid:
            types.append(MapType::AutoNaviSatellite);
            types.append(MapType::AutoNaviLabels);
            break;

        default:
            types.append(type);
            break;
        }
    }

    return types;
}

} // end of namespace opmap
