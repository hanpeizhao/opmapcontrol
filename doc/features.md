# 功能清单与 API 参考

库的全部公开能力按模块整理。**example 一列**标注 `opmapcontrol_example` 是否演示了该能力——example 只覆盖了一小部分，多数能力需在自己的代码中直接调用 API 使用。

## 1. 地图控制（OPMapWidget）

| 功能 | API | example |
|------|:---:|:----:|
| 切换地图源 | `SetMapType()` / `GetMapType()` | ✅ |
| 缩放（double/float 级别） | `SetZoom()` / `ZoomReal()` / `ZoomDigi()` / `ZoomTotal()` | ✅ |
| 最大/最小缩放限制 | `SetMaxZoom()` / `SetMinZoom()` | — |
| 定位地图中心（WGS-84） | `SetCurrentPosition()` / `CurrentPosition()` | — |
| 地图旋转 | `SetRotate()` / `Rotate()` | — |
| 拖动开关 | `SetCanDragMap()` | — |
| 瓦片网格线显示 | `SetShowTileGridLines()` | — |
| 指北针显示 | `SetShowCompass()` | ✅（默认显示） |
| OpenGL 渲染开关 | `SetUseOpenGL()` | — |
| 强制重载地图 | `ReloadMap()` | — |
| 鼠标跟随模式 | `SetFollowMouse()` | — |

## 2. 坐标与几何工具

| 功能 | API | example |
|------|:---:|:----:|
| 当前鼠标位置（WGS-84） | `currentMousePosition()` | ✅ |
| 屏幕像素 → 经纬度 | `GetFromLocalToLatLng(QPointF)` | — |
| 米 → 像素换算 | `metersToPixels(double)` | — |
| 两点方位角（度） | `bearing(from, to)` | — |
| 源点+方位+距离 → 目标点 | `destPoint(source, bearing, dist)` | — |

## 3. 航点管理（WayPointItem）

| 功能 | API | example |
|------|:---:|:----:|
| 创建航点（坐标/高度/描述，5 种重载） | `WPCreate()` | ✅ |
| 指定位置插入航点 | `WPInsert()` | — |
| 删除单个/全部航点 | `WPDelete()` / `WPDeleteAll()` | ✅ |
| 查询全部/选中的航点 | `WPAll()` / `WPSelected()` | ✅ |
| 重新编号（自动连锁） | `WPRenumber()` | — |
| 航点可拖拽编辑、显示高度 | `WayPointItem`（QGraphicsItem） | 部分 |
| 航点间连线 | `waypointLines` / `WayPointLineItem` | — |

航点变化通过信号实时通知（见第 7 节），可与飞行控制逻辑解耦对接。

## 4. UAV / Home / GPS 元素

| 功能 | API | example |
|------|:---:|:----:|
| 添加/删除多 UAV（多机同时显示） | `AddUAV(id)` / `DeleteUAV(id)` / `GetUAV()` / `GetUAVS()` | ✅（单机） |
| UAV 位置/航向/轨迹更新 | `UAVItem::SetUAVPos()` / `SetUAVHeading()` / 轨迹类型 `UAVTrailType` | ✅（位置） |
| UAV 图标自定义 | `SetUAVPic(路径)` | — |
| UAV 显示开关 | `SetShowUAV()` | ✅ |
| Home 点显示开关 | `SetShowHome()` | ✅ |
| 安全圈报警（飞出安全范围信号） | 信号 `UAVLeftSafetyBouble()` | — |
| 到达航点事件 | 信号 `UAVReachedWayPoint()` | — |
| GPS 轨迹元素 | `GPSItem` | — |
| 诊断信息叠显（线程/缓存状态） | `SetShowDiagnostics()` | — |

## 5. 离线地图下载

| 功能 | API | example |
|------|:---:|:----:|
| 框选区域 | `SelectedArea()` / `SetSelectedArea()`（WGS-84 矩形） | ✅ |
| 抓取框选区域瓦片入库 | 槽 `RipMap()` | ✅ |
| 下载进度/完成事件 | `MapRipper` 信号 | — |

## 6. 缓存与访问控制（Configuration）

| 功能 | API | example |
|------|:---:|:----:|
| 访问模式（仅缓存 / 仅网络 / 网络+缓存） | `SetAccessMode(AccessMode::Types)` | — |
| 内存缓存开关与容量 | `SetUseMemoryCache()` / `SetTileMemorySize(MB)` | — |
| 当前内存占用查询 | `TileMemoryUsed()` | — |
| 缓存目录自定义 | `SetCacheLocation(路径)` | — |
| 删除 N 天前的旧瓦片 | `DeleteTilesOlderThan(天数)` | — |
| 两个缓存库间增量导出 | `ExportMapDataToDB(源库, 目标库)` | — |
| 空瓦片样式（画刷/边框/文字/字体） | `EmptytileBrush` / `EmptyTileBorders` / `EmptyTileText` / `MissingDataFont` | — |
| 选择框/比例尺画笔 | `SelectionPen` / `ScalePen` | — |
| 拖动按键定义 | `DragButton` | — |

## 7. 信号（回调）清单

界面无关的飞行/业务逻辑通过 Qt 信号对接，无需侵入控件：

| 信号 | 触发时机 | example |
|------|----------|:----:|
| `mouseMove` / `mousePress` / `mouseRelease` | 鼠标事件转发 | ✅ |
| `zoomChanged` | 缩放变化 | ✅ |
| `OnCurrentPositionChanged` | 地图中心移动 | — |
| `OnMapDrag` / `OnMapZoomChanged` / `OnMapTypeChanged` | 拖动/缩放/源切换 | — |
| `OnTileLoadStart` / `OnTileLoadComplete` / `OnTilesStillToLoad(n)` | 瓦片加载生命周期 | — |
| `OnEmptyTileError` | 瓦片加载失败 | — |
| `WPInserted` / `WPDeleted` / `WPNumberChanged` / `WPValuesChanged` / `WPReached` | 航点增删/改号/改值/到达 | — |
| `UAVReachedWayPoint` / `UAVLeftSafetyBouble` | UAV 到点/出安全圈 | — |

## 8. 车载导航（路径规划 / NavigationEngine / RouteItem）

内置完整车载导航链路：异步路径规划 → 沿线转向指引 → 偏航检测与自动重规划 → 到达判定。路线绘制（已走灰/未走蓝 + 起终点标记）由控件内部完成，车辆位置由外部程序喂入（行车模拟器、系统 GPS 等任意来源均可，见第 9 节的 IP 定位兜底）。

| 功能 | API | example |
|------|:---:|:----:|
| 更换路径规划源（OSRM/高德，可自定义扩展） | `SetRouteProvider(AbstractRouteProvider*)` | ✅ |
| 选点预览路线（起→终点画线，不导航） | `PlanRoute(起点, 终点)` | ✅ |
| 规划并开始导航（起点缺省为当前车辆位置） | 槽 `NavigateTo(目的地)` | ✅ |
| 喂入车辆实时位置（同步 UAV 图标 + 驱动引擎） | 槽 `UpdateVehiclePosition(PointLatLng)` | ✅ |
| 停止导航 | 槽 `StopNavigation()` | ✅ |
| 查询车辆当前位置（最近一次喂入，非地图中心） | `HasVehiclePosition()` / `VehiclePosition()` | ✅ |
| 是否导航中 | `IsNavigating()` | — |
| 当前导航路线 | `CurrentNavigationRoute()` | — |
| 路线显示开关（默认导航时自动显示） | `SetShowRoute(bool)` / `ShowRoute()` | — |

导航信号：

| 信号 | 触发时机 | example |
|------|----------|:----:|
| `navigationRouteReady(opmap::Route)` | 规划成功，开始导航 | ✅ |
| `navigationProgress(剩余米, 剩余秒, 转向指令)` | 每次喂点后更新 | ✅ |
| `offRouteDetected(位置, 偏离米)` | 连续 3 次偏离超过 50m | ✅ |
| `rerouteReady(opmap::Route)` | 偏航自动重规划成功 | ✅ |
| `navigationArrived()` | 距目的地 ≤30m，自动停止 | ✅ |
| `navigationFailed(原因)` | 路线规划失败 | ✅ |

说明：

- 路径规划源：`OsrmRouteProvider`（OSRM 演示服务器，免 key 默认）、`AmapRouteProvider`（高德 Web 服务，需 key）；继承 `AbstractRouteProvider` 并实现 `requestRoute`/`isBusy` 即可接入自定义规划服务
- `opmap::Route` 携带 WGS-84 折线、分步中文转向指令、总距离/总时长
- 引擎默认参数：偏航阈值 50m × 连续 3 次、到达阈值 30m、重规划最小间隔 5s、自动重规划开启（调整入口为 `NavigationEngine` 的 `SetOffRouteThresholdM` 等方法）
- `geoutils`（platform 层）提供 haversine 距离、方位角等几何工具，模拟器与引擎共用

## 9. IP 定位兜底（IpLocationProvider）

库内置城市级 IP 定位：通过公网出口 IP 估算所在城市，作为无 GPS 环境（桌面端等）的一键定位兜底。双源自动回退——主源 ip-api.com（国内城市识别准）、备源 ipwho.is（HTTPS），主源网络失败/返回异常/超时（8 秒）时静默切备源重试一次。坐标为 WGS-84 城市级精度（约数公里）。

| 功能 | API | example |
|------|:---:|:----:|
| 发起一次 IP 定位（在途时重复调用被忽略） | 槽 `RequestIpLocation()` | ✅ |
| 查询是否有请求在途 | `IsIpLocationBusy()` | — |

IP 定位信号：

| 信号 | 触发时机 | example |
|------|----------|:----:|
| `ipLocationReady(opmap::PointLatLng pos, QString city)` | 定位成功（WGS-84 城市级坐标 + 城市名） | ✅ |
| `ipLocationFailed(QString reason)` | 双源均不可用或返回异常 | ✅ |

说明：

- 实现位于 `src/providers/iplocationprovider.h/.cpp`，与路线规划 provider 同层，由 `OPMapWidget` 持有并转发信号
- 调用方拿到 `ipLocationReady` 后自行决定喂给 `UpdateVehiclePosition`（example 的做法）或仅显示
- 若需更换定位服务，可在 `OPMapWidget` 外自行实现并替换（provider 只依赖 Qt 网络模块，无库内耦合）

## 10. example 未覆盖的能力汇总

example 已覆盖主要链路（切源、缩放、框选下载、航点增删/导入导出、车载导航全流程、行车模拟与模拟偏航、系统 GPS 与 IP 定位位置源）。以下能力为库完整提供但 example 未使用：

- 地图旋转、OpenGL 渲染、缩放级别限制、访问模式控制
- 多机 UAV 同时显示、安全圈报警、到点事件
- 航点插入/重编号、连线、值变化信号
- 缓存目录/容量管理、旧瓦片清理、缓存库间导出
- 全部瓦片加载生命周期信号与几何换算工具
- 导航状态查询（`IsNavigating`/`CurrentNavigationRoute`）、路线显示开关、引擎参数调整

如需这些能力，直接包含 `src/opmapcontrol.h` 调用对应 API 即可，无需改动库代码。
