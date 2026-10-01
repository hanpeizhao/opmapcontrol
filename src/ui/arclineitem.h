/**
******************************************************************************
* @file       arclineitem.h
* @brief      贝塞尔弧线航线图元：迁徙图/航线可视化（拱弧 + 沿弧箭头 + 流动光效）
*
*             地理空间二次贝塞尔：from/to 为端点，拱顶控制点 apex 由
*             destPoint 按距离比例（0.18 倍）与拱向 side 自动生成。
*             绘制时三点投影到屏幕构建 QPainterPath 二次贝塞尔：底层半透明
*             宽衬 + 主线 + 沿弧均布方向箭头 + 循环流动亮段（内建 QTimer）。
*             地图拖动/缩放时经 MapAnchoredItem::RefreshPos 重投影跟随；
*             ArcPointAt(t) 返回弧上参数 t 处的地理坐标，供上层驱动标记
*             （如候鸟）沿弧飞行——鸟位置与弧线严格重合。
*
*             注意：插值在经纬度空间做贝塞尔组合，不跨 180° 经线的航线
*             视觉无失真；跨经线的长航线建议分段。
*
* @class      ArcLineItem arclineitem.h "arclineitem.h"
******************************************************************************
*/
#ifndef ARCLINEITEM_H
#define ARCLINEITEM_H

#include <QObject>
#include <QGraphicsItem>
#include <QPainterPath>
#include <QColor>

#include "pointlatlng.h"
#include "mapanchoreditem.h"

class QTimer;

namespace opmap {

class MapGraphicItem;

class ArcLineItem : public QObject, public QGraphicsItem, public MapAnchoredItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    enum { Type = UserType + 14 };   ///< 图元类型（qgraphicsitem_cast 分派依据）

    /**
     * @param side 拱向：+1 航向右侧拱，-1 左侧拱（相邻段交替呈 S 形更自然）
     */
    ArcLineItem(MapGraphicItem *map, const opmap::PointLatLng &from, const opmap::PointLatLng &to,
                const QColor &color, int side, QGraphicsItem *parent = 0);
    ~ArcLineItem();

    int type() const { return Type; }

    /// 弧上参数 t∈[0,1] 处的地理坐标（上层驱动标记沿弧飞行用）
    opmap::PointLatLng ArcPointAt(double t) const;

    void SetColor(const QColor &color) { m_color = color; update(); }
    void SetArrowCount(int n) { m_arrows = qMax(0, n); update(); }
    void SetFlowEnabled(bool on);

    /// MapAnchoredItem：地图拖动/缩放后重投影并重建路径
    virtual void RefreshPos();

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

private slots:
    void onFlowTick();

private:
    void BuildPath();   ///< 投影三控制点 → 屏幕贝塞尔路径（缓存，RefreshPos 触发）

    MapGraphicItem *m_map;
    opmap::PointLatLng m_from;
    opmap::PointLatLng m_to;
    opmap::PointLatLng m_apex;      ///< 拱顶控制点（地理坐标，构造时按距离/拱向生成）
    QColor m_color;
    int m_arrows;                   ///< 沿弧方向箭头数量
    bool m_flow;                    ///< 流动光效开关
    double m_flowPhase;             ///< 光效相位 0..1（随时间推进取模循环）
    QPainterPath m_path;            ///< 屏幕坐标贝塞尔路径（map 局部坐标系）
    QRectF m_bounding;              ///< 缓存包围盒（含箭头/光点余量）
    QTimer *m_flowTimer;
};

} // end of namespace opmap

#endif // ARCLINEITEM_H
