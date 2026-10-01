# opmapcontrol_ex

基于 OpenPilot GCS `opmapcontrol` 的 Qt 地图控件库，已重构迁移至 **Qt 5.12**，并重组为分层架构。支持多地图源切换、WGS-84 坐标自动纠偏、瓦片三级缓存与离线地图下载，内置航点任务（动作/悬停）、多边形地理围栏、车载导航（多备选路线/转向指引/偏航重规划）、多位置源接入（GPS/IP 定位/MAVLink）、地图量测与轨迹回放能力，附完整 example。

## 特性

- **多地图源**：高德（路网/卫星/标注/混合）、OpenStreetMap、ArcGIS（地图/卫星/地形）、Google（地图/卫星/地形/混合）
- **坐标系自动纠偏**：用户坐标统一使用 WGS-84，控件根据地图源声明的瓦片坐标系（GCJ-02/WGS-84）在投影边界自动转换，鼠标读出的经纬度始终为 WGS-84
- **瓦片三级缓存**：内存 LRU 缓存 → SQLite 磁盘缓存 → 网络下载，离线数据与在线浏览共用同一缓存
- **离线下载**：框选区域后一键抓取指定缩放级别的全部瓦片
- **地图元素**：UAV 位置/航迹、航点（增删改/JSON 导入导出）、Home 点、GPS 轨迹、通用标记（图片/文字钉点 + 移动轨迹线）、比例尺（随缩放实时换算）、贝塞尔弧线航线（拱弧 + 方向箭头 + 流动光效，迁徙图/航线可视化）
- **航点任务**：任务状态机纯喂点驱动（真机遥测与模拟同构接入），到达判定/悬停计时/动作触发（拍照/悬停）全自动
- **多边形地理围栏**：取点绘制，多边形内部为允许飞行区，越界自动判定发信号
- **车载导航**：内置路径规划（OSRM/高德，多备选路线可切换）、中文转向指引、偏航检测与自动重规划、到达判定；车辆位置由外部喂入（行车模拟 / 系统 GPS / MAVLink 遥测），路线自动绘制
- **位置源体系**：行车模拟 / 系统 GPS / IP 定位（双源兜底、城市级）/ MAVLink UDP 遥测，互斥切换统一喂点
- **地图量测**：多点测距（逐点画折线，实时标注每段与总距离）；运动轨迹记录 → JSON 保存 → 按倍速回放
- **地面海拔查询**：在线高程服务免 key 查询任意点真实海拔
- **API 全量示范**：example 附"库能力示范"面板（视图控制/缓存管理/量测与轨迹/多机演示）与"事件日志"面板（库信号实时流），features.md 按 API 逐条标注演示状态
- **界面可配置**：网格线显示、缩放范围、旋转、空瓦片样式、内存缓存容量等

## 目录结构

```
src/
├── ui/          Qt 界面控件（OPMapWidget 及地图元素 item）
├── engine/      地图引擎（MapService 组合服务 / MapEngine 加载调度 / 瓦片矩阵 / 投影）
├── providers/   地图源与在线服务（MapType 枚举 / UrlFactory / 路线规划 provider / IP 定位 provider）
├── cache/       瓦片缓存（内存 LRU / SQLite 磁盘缓存 / 下载队列）
└── platform/    基础类型（几何点/矩形、坐标系转换、诊断）
```

依赖方向自上而下单向传递：`ui → engine → providers + cache → platform`。外部程序只需包含 `src/opmapcontrol.h`。

## 文档

- [API 参考](doc/api-reference.md) — 库公开 API 权威清单：签名/参数/功能/用法，按模块分类
- [功能清单](doc/features.md) — 库全部公开能力按模块整理，并标注 example 是否演示
- [实现原理](doc/architecture.md) — 分层架构、瓦片加载流程、三级缓存、坐标系纠偏、投影与离线下载原理、扩展指南
- [任务功能](doc/mission-features.md) — 航点任务/地理围栏/MAVLink 接入的语义与用法

## 构建

依赖：Qt 5.12（库需 core gui widgets network sql svg opengl 模块，example 另需 positioning 模块）、MinGW（Windows）或 GCC（Linux）。地图瓦片下载使用 HTTPS，Windows 下需要 OpenSSL 运行库（example/demo 目录已附带 `libssl-1_1-x64.dll` / `libcrypto-1_1-x64.dll`）。

```bash
# 编译静态库
qmake opmapcontrol.pro
mingw32-make        # Linux 下为 make

# 编译 example
cd example/demo
qmake demo.pro
mingw32-make
```

构建中间产物（moc/uic/rcc/目标文件）统一输出到 `build/` 目录，静态库输出为根目录的 `libopmapwidget.a`。

## 运行 example

```bash
# 需保证 Qt 的 bin 目录在 PATH 中（DLL 依赖），例如：
# set PATH=C:\Qt\Qt5.12.12\5.12.12\mingw73_64\bin;%PATH%
opmapcontrol_example.exe
```

- 拖动/滚轮缩放浏览地图，鼠标读数为 WGS-84 经纬度，左下角比例尺随缩放实时换算
- 右键菜单：切换地图类型 / 添加与删除航点 / 开始测距 / 查询此处地面海拔
- 面板能力：航点编辑与航点飞行、车载导航（取点规划/开始/备选路线切换）、行车模拟、多边形围栏、多人位置与候鸟迁徙演示、多点测距、轨迹记录/保存/回放
- 框选区域后点击 `Cache map` 下载离线瓦片；重启程序后命中缓存，不再联网
- 地图类型等初始配置位于 `example/demo/data/demo.ini`

## 集成到自己的项目

1. `.pro` 中添加 `INCLUDEPATH`（指向 `src/` 及各子目录）并链接 `libopmapwidget.a`
2. 包含 `opmapcontrol.h`，创建 widget：

```cpp
#include "opmapcontrol.h"

OPMapWidget *map = new OPMapWidget(this);

map->SetMapType(opmap::MapType::AutoNaviRoad);      // 地图源
map->SetCurrentPosition(opmap::PointLatLng(30.66, 104.06)); // WGS-84 经纬度
map->SetZoom(10);

// 离线下载：框选区域（SelectedArea）后抓取当前级别瓦片
map->RipMap();
```

注意：`MapType::Types` 的枚举数值与磁盘缓存索引绑定，请勿修改数值或调整顺序，否则已缓存瓦片无法命中。

## 地图源与坐标系

每个地图源在 `UrlFactory` 中声明对应的瓦片坐标系（`MapType::TileDatum`）：

| 坐标系 | 地图源 | 说明 |
|--------|--------|------|
| WGS-84 | OpenStreetMap、ArcGIS、Google | 无需纠偏 |
| GCJ-02 | 高德（AutoNavi*） | 国内火星坐标，控件在投影边界自动与 WGS-84 互转 |

新增地图源时：在 `MapType::Types` 添加枚举值，在 `UrlFactory` 的 URL 工厂添加对应 case，并在 `DatumByType()` 中声明其坐标系，控件即可自动完成纠偏。

## 缓存机制

| 层级 | 位置 | 说明 |
|------|------|------|
| 内存缓存 | LRU 链表 | 容量可配（`Configuration::SetTileMemorySize`），超出自动淘汰 |
| 磁盘缓存 | SQLite 数据库 | 按（地图类型, 缩放级别, x, y）索引，Tile 字段存原始图片字节；example 默认 `example/demo/data/OPMaps.qmdb` |
| 网络下载 | UrlFactory | 支持 HTTPS 与重定向跟随，仅前两层未命中时触发 |

## 截图

主界面：视图控制 / 缓存管理 / 量测与轨迹面板，左下角比例尺随缩放实时换算，底部事件日志实时滚动库信号。

![screenshot 1](images/screenshot_01.png)

候鸟迁徙演示：4 条贝塞尔弧线航线（沿弧方向箭头 + 流动光效），候鸟标记沿弧飞行，与航线严格重合。

![screenshot 2](images/screenshot_02.png)

车载导航：取点规划后沿路指引，备选路线可切换，路线按进度着色，面板实时显示距离/预计时间/偏航参数。

![screenshot 3](images/screenshot_03.png)

## 许可证

本项目基于 GNU GPL v3 协议发布，原始代码版权归 [OpenPilot Project](http://www.openpilot.org) 所有。
