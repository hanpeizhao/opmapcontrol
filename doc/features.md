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

## 8. example 未覆盖的能力汇总

example 是原作者的测试窗口，仅演示了基础链路（切源、缩放、框选下载、单 UAV 位置、航点增删）。以下能力为库完整提供但 example 未使用：

- 地图旋转、OpenGL 渲染、缩放级别限制、访问模式控制
- 多机 UAV 同时显示、轨迹样式、安全圈报警、到点事件
- 航点插入/重编号、连线、值变化信号
- 缓存目录/容量管理、旧瓦片清理、缓存库间导出
- 全部瓦片加载生命周期信号与几何换算工具

如需这些能力，直接包含 `src/opmapcontrol.h` 调用对应 API 即可，无需改动库代码。
