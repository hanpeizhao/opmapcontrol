/**
******************************************************************************
* @file       abstractrouteprovider.cpp
* @brief      路径规划 provider 抽象基类实现（见 abstractrouteprovider.h）
******************************************************************************
*/

#include "abstractrouteprovider.h"

namespace opmap {

AbstractRouteProvider::AbstractRouteProvider(QObject *parent)
    : QObject(parent)
{
}

} // namespace opmap
