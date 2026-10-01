# 功能清单与 API 参考

> 全量 API 参考以 [api-reference.md](api-reference.md) 为准（含任务飞行/围栏/MAVLink 等新增能力）；本文保留为按 example 视角整理的功能清单。

库的全部公开能力按模块整理。**example 一列**标注 `opmapcontrol_example` 是否演示了该能力——example 以"库能力示范"面板 + 事件日志面板 + 各功能面板覆盖了绝大多数 API，其余多为样式定制类低频配置。

## 1. 地图控制（OPMapWidget）

| 功能 | API | example |
|------|:---:|:----:|
| 切换地图源 | `SetMapType()` / `GetMapType()` | ✅ |
| 缩放（double/float 级别） | `SetZoom()` / `ZoomReal()` / `ZoomDigi()` / `ZoomTotal()` | ✅ |
| 最大/最小缩放限制 | `SetMaxZoom()` / `SetMinZoom()` | ✅（库能力示范面板） |
| 定位地图中心（WGS-84） | `SetCurrentPosition()` / `CurrentPosition()` | ✅ |
| 地图旋转 | `SetRotate()` / `Rotate()` | ✅（旋转滑条） |
| 拖动开关 | `SetCanDragMap()` | ✅ |
| 瓦片网格线显示 | `SetShowTileGridLines()` | ✅ |
| 指北针显示 | `SetShowCompass()` | ✅（默认显示） |
| 比例尺显示（左下角，随缩放/中心纬度实时换算整距离） | `SetShowScale()` / `ShowScale()` | ✅（默认显示） |
| OpenGL 渲染开关 | `SetUseOpenGL()` | ✅ |
| 强制重载地图 | `ReloadMap()` | ✅ |
| 鼠标跟随模式 | `SetFollowMouse()` | ✅ |

## 2. 坐标与几何工具

| 功能 | API | example |
|------|:---:|:----:|
| 当前鼠标位置（WGS-84） | `currentMousePosition()` | ✅ |
| 屏幕像素 → 经纬度 | `GetFromLocalToLatLng(QPointF)` | — |
| 米 → 像素换算 | `metersToPixels(double)` | ✅（几何演示） |
| 两点方位角（度） | `bearing(from, to)` | ✅（几何演示） |
| 源点+方位+距离 → 目标点 | `destPoint(source, bearing, dist)` | ✅（几何演示，dist 单位千米） |

## 3. 航点管理（WayPointItem）

| 功能 | API | example |
|------|:---:|:----:|
| 创建航点（坐标/高度/描述，5 种重载） | `WPCreate()` | ✅ |
| 指定位置插入航点 | `WPInsert()` | ✅（中点插入按钮） |
| 删除单个/全部航点 | `WPDelete()` / `WPDeleteAll()` | ✅ |
| 查询全部/选中的航点 | `WPAll()` / `WPSelected()` | ✅ |
| 重新编号（自动连锁） | `WPRenumber()` | ✅（选中移至末尾按钮） |
| 航点任务文件 JSON 导出（全属性：编号/经纬度/海拔/描述/动作/悬停时长） | `WPExportToFile()` | ✅（导出按钮） |
| 航点任务文件导入（自动嗅探 JSON 或旧 AP 列式 .wp） | `WPImportFromFile()` | ✅（导入按钮） |
| 航点可拖拽编辑、显示高度 | `WayPointItem`（QGraphicsItem） | 部分 |
| 航点间连线 | `waypointLines` / `WayPointLineItem` | — |

航点变化通过信号实时通知（见第 7 节），可与飞行控制逻辑解耦对接。

## 4. UAV / Home / GPS 元素

| 功能 | API | example |
|------|:---:|:----:|
| 添加/删除多 UAV（多机同时显示） | `AddUAV(id)` / `DeleteUAV(id)` / `GetUAV()` / `GetUAVS()` | ✅（僚机绕飞演示） |
| UAV 位置/航向/轨迹更新 | `UAVItem::SetUAVPos()` / `SetUAVHeading()` / 轨迹类型 `UAVTrailType` | ✅（位置+航向+轨迹） |
| UAV 图标自定义 | `SetUAVPic(路径)` | — |
| UAV 显示开关 | `SetShowUAV()` | ✅ |
| Home 点显示开关 | `SetShowHome()` | ✅ |
| 安全圈报警（飞出安全范围信号） | 信号 `UAVLeftSafetyBouble()` | ✅（僚机 + Home 安全圈 400m） |
| 到达航点事件 | 信号 `UAVReachedWayPoint()` | ✅（事件日志订阅） |
| GPS 轨迹元素 | `GPSItem` | — |
| 诊断信息叠显（线程/缓存状态） | `SetShowDiagnostics()` | ✅ |
| 通用标记：图片/文字钉在任意坐标（可选中、可移动轨迹） | `AddMarker` / `RemoveMarker` / `ClearMarkers`；句柄 `SetImage` / `SetText` / `SetCoord` / `SetShowTrail` | ✅（右键贴图/多人共享位置轨迹） |

## 5. 离线地图下载

| 功能 | API | example |
|------|:---:|:----:|
| 框选区域 | `SelectedArea()` / `SetSelectedArea()`（WGS-84 矩形） | ✅ |
| 抓取框选区域瓦片入库 | 槽 `RipMap()` | ✅ |
| 下载进度/完成事件 | `mapDownloadProgress/Tiles/Finished` 信号（库转发自 MapRipper） | ✅（事件日志） |

## 6. 缓存与访问控制（Configuration）

| 功能 | API | example |
|------|:---:|:----:|
| 访问模式（仅缓存 / 仅网络 / 网络+缓存） | `SetAccessMode(AccessMode::Types)` | ✅（访问模式下拉） |
| 内存缓存开关与容量 | `SetUseMemoryCache()` / `SetTileMemorySize(MB)` | ✅（容量调节） |
| 当前内存占用查询 | `TileMemoryUsed()` | ✅ |
| 缓存目录自定义 | `SetCacheLocation(路径)` | —（目录展示已有） |
| 删除 N 天前的旧瓦片 | `DeleteTilesOlderThan(天数)` | ✅（清理按钮） |
| 两个缓存库间增量导出 | `ExportMapDataToDB(源库, 目标库)` | ✅（导出按钮） |
| 空瓦片样式（画刷/边框/文字/字体） | `EmptytileBrush` / `EmptyTileBorders` / `EmptyTileText` / `MissingDataFont` | — |
| 选择框/比例尺画笔 | `SelectionPen` / `ScalePen` | — |
| 拖动按键定义 | `DragButton` | — |

## 7. 信号（回调）清单

界面无关的飞行/业务逻辑通过 Qt 信号对接，无需侵入控件：

| 信号 | 触发时机 | example |
|------|----------|:----:|
| `mouseMove` / `mousePress` / `mouseRelease` | 鼠标事件转发 | ✅ |
| `zoomChanged` | 缩放变化 | ✅ |
| `OnCurrentPositionChanged` | 地图中心移动 | —（高频，日志未订阅） |
| `OnMapDrag` / `OnMapZoomChanged` / `OnMapTypeChanged` | 拖动/缩放/源切换 | ✅（缩放/切源入日志） |
| `OnTileLoadStart` / `OnTileLoadComplete` / `OnTilesStillToLoad(n)` | 瓦片加载生命周期 | ✅（事件日志/状态栏） |
| `OnEmptyTileError` | 瓦片加载失败 | ✅（事件日志） |
| `WPInserted` / `WPDeleted` / `WPNumberChanged` / `WPValuesChanged` / `WPReached` | 航点增删/改号/改值/到达 | ✅（事件日志） |
| `UAVReachedWayPoint` / `UAVLeftSafetyBouble` | UAV 到点/出安全圈 | ✅（事件日志） |

其余业务信号（导航、任务、围栏、IP 定位、位置源、取点、测距/轨迹、下载）见第 8/9/10 节及 [api-reference.md](api-reference.md) 第 12 节总表。

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
| 是否导航中 | `IsNavigating()` | ✅（导航状态行） |
| 当前导航路线 | `CurrentNavigationRoute()` | ✅（导航状态行） |
| 路线显示开关（默认导航时自动显示） | `SetShowRoute(bool)` / `ShowRoute()` | ✅ |
| 切换备选路线（规划返回多条时） | `SelectRoute(index)`，索引来自 `routeAlternativesReady`，0=推荐 | ✅（循环切换按钮） |
| 导航引擎参数调整 | `GetNavigationEngine()` → `SetOffRouteThresholdM` 等 | ✅（偏航/到达阈值调节） |

导航信号：

| 信号 | 触发时机 | example |
|------|----------|:----:|
| `navigationRouteReady(opmap::Route)` | 规划成功，开始导航 | ✅ |
| `navigationProgress(剩余米, 剩余秒, 转向指令)` | 每次喂点后更新 | ✅ |
| `offRouteDetected(位置, 偏离米)` | 连续 3 次偏离超过 50m | ✅ |
| `rerouteReady(opmap::Route)` | 偏航自动重规划成功 | ✅ |
| `navigationArrived()` | 距目的地 ≤30m，自动停止 | ✅ |
| `navigationFailed(原因)` | 路线规划失败 | ✅ |
| `routeAlternativesReady(备选路线全集)` | 规划成功给出全部备选（第 0 条=推荐，与画布同步） | ✅ |
| `routeSelected(索引, 路线)` | 备选路线切换（`SelectRoute`）：上层据此同步面板距离/时间 | ✅ |

说明：

- 路径规划源：`OsrmRouteProvider`（OSRM 演示服务器，免 key 默认）、`AmapRouteProvider`（高德 Web 服务，需 key）；继承 `AbstractRouteProvider` 并实现 `requestRoute`/`isBusy` 即可接入自定义规划服务
- `opmap::Route` 携带 WGS-84 折线、分步中文转向指令、总距离/总时长
- 引擎默认参数：偏航阈值 50m × 连续 3 次、到达阈值 30m、重规划最小间隔 5s、自动重规划开启（调整入口为 `NavigationEngine` 的 `SetOffRouteThresholdM` 等方法）
- `geoutils`（platform 层）提供 haversine 距离、方位角等几何工具，模拟器与引擎共用

## 9. IP 定位兜底与地面海拔查询

库内置两项在线位置/高程增强：城市级 IP 定位与真实地面海拔查询。

库内置城市级 IP 定位：通过公网出口 IP 估算所在城市，作为无 GPS 环境（桌面端等）的一键定位兜底。双源自动回退——主源 ip-api.com（国内城市识别准）、备源 ipwho.is（HTTPS），主源网络失败/返回异常/超时（8 秒）时静默切备源重试一次。坐标为 WGS-84 城市级精度（约数公里）。

| 功能 | API | example |
|------|:---:|:----:|
| 发起一次 IP 定位（在途时重复调用被忽略） | 槽 `RequestIpLocation()` | ✅ |
| 回调式一步到位（完成/失败自动回调 `cb(ok, pos, city)`，信号仍并行发射） | `RequestIpLocation(IpLocationCallback)` | — |
| 查询是否有请求在途 | `IsIpLocationBusy()` | ✅（轮询日志） |

IP 定位信号：

| 信号 | 触发时机 | example |
|------|----------|:----:|
| `ipLocationReady(opmap::PointLatLng pos, QString city)` | 定位成功（WGS-84 城市级坐标 + 城市名） | ✅ |
| `ipLocationFailed(QString reason)` | 双源均不可用或返回异常 | ✅ |

说明：

- 实现位于 `src/providers/iplocationprovider.h/.cpp`，与路线规划 provider 同层，由 `OPMapWidget` 持有并转发信号
- 调用方拿到 `ipLocationReady` 后自行决定喂给 `UpdateVehiclePosition`（example 的做法）或仅显示
- 若需更换定位服务，可在 `OPMapWidget` 外自行实现并替换（provider 只依赖 Qt 网络模块，无库内耦合）

**地面海拔查询**（`ElevationProvider`，open-meteo 免 key，8 秒超时）：地图瓦片是 2D 影像不含高程，查询某点真实地面海拔走在线高程服务，与 IP 定位同构接入。

| 功能 | API | example |
|------|:---:|:----:|
| 查询 WGS-84 坐标处地面海拔（米，在途重复调用被忽略） | 槽 `RequestElevation(pos)` | ✅（右键菜单"查询此处地面海拔"） |
| 回调式一步到位（完成/失败自动回调 `cb(ok, pos, altM)`，信号仍并行发射） | `RequestElevation(pos, ElevationCallback)` | — |
| 查询是否有请求在途 | `IsElevationBusy()` | ✅（在途时菜单置灰） |

海拔查询信号：`elevationReady(pos, altitudeMeters)` / `elevationFailed(reason)`。

## 10. 地图测距 / 运动轨迹（保存回放）/ 迁徙示例

**多点测距**（复用取点模式机制：6px 防抖、橡皮筋预览、每段与总距离标注全在库内）：

| 功能 | API | example |
|------|:---:|:----:|
| 进入/退出测距模式（逐点点击画折线，右键结束一段，画面保留可续测） | `SetPickMode(PickMeasure / PickNone)` | ✅（测距按钮三态 + 右键菜单） |
| 清除全部测距结果 | `ClearMeasurements()` | ✅ |
| 是否有测距内容 | `HasMeasurements()` | — |

| 信号 | 触发时机 | example |
|------|----------|:----:|
| `measureFinished(总米数, 点列表)` | 一段测距完成（≥2 点），画面保留可继续追加 | ✅（日志+状态栏） |

**运动轨迹记录 / 保存 / 回放**（记录挂钩在 facade 喂点链路，模拟/GPS/IP 定位/MAVLink/回放等所有位置源统一入库，engine 层 `TrailRecorder` 纯数据无图元依赖）：

| 功能 | API | example |
|------|:---:|:----:|
| 开始/停止记录（喂点自动采样：经纬度+高度+相对毫秒时间轴） | `StartTrailRecording()` / `StopTrailRecording()` | ✅ |
| 记录状态/点数查询 | `IsTrailRecording()` / `TrailPointCount()` | ✅ |
| 清空轨迹缓冲 | `ClearTrailRecording()` | ✅ |
| 保存为 JSON（`opmap-trail` 格式：t/lat/lng/alt） | `SaveTrailToFile(路径, *错误)` | ✅（QFileDialog） |
| 加载轨迹文件 | `LoadTrailFromFile(路径, *错误)` | ✅ |
| 按时间轴回放（QTimer 50ms × 倍速，相邻点线性插值喂 SetUAVPos，图标/轨迹/围栏/任务机全联动） | `StartTrailReplay(倍速=1.0)` / `StopTrailReplay()` / `IsTrailReplaying()` | ✅（倍速 0.5~16） |

| 信号 | 触发时机 | example |
|------|----------|:----:|
| `trailReplayFinished()` | 回放推进到末点 | ✅ |

**迁徙图示例**（demo 层）：4 只候鸟沿球面 slerp 大圆弧插值路线从繁殖地飞往越冬地（贝加尔湖→鄱阳湖、蒙古高原→荣成等），途经点分段推进，通用标记（`AddMarker` + `SetText` + `SetShowTrail`）实时携带移动轨迹线；与多人位置演示互斥（共用标记层，双向守卫）。

## 11. example 未覆盖的能力汇总

example 以三块组合覆盖库的绝大多数能力：**各功能面板**（地图/航点/导航/行车模拟）、**库能力示范面板**（左侧：视图控制 / 缓存与访问 / 多机与几何）、**事件日志面板**（底部：库信号实时流）。以下为仍未演示的少数项，多为样式定制或预留能力：

- `GetFromLocalToLatLng`（像素→经纬度；内部逻辑已由鼠标读数覆盖）
- 航点间连线 `waypointLines` / `WayPointLineItem`（库预留半成品，仅建组不填线）
- `GPSItem` GPS 轨迹元素、`SetUAVPic` UAV 图标自定义
- `SetCacheLocation` 缓存目录设置（运行中切换目录需重载）
- 空瓦片样式、选择框/比例尺画笔、拖动按键等外观定制（`EmptytileBrush` / `SelectionPen` / `ScalePen` / `DragButton` 等）

如需这些能力，直接包含 `src/opmapcontrol.h` 调用对应 API 即可，无需改动库代码。
