# opmapcontrol 全量 API 参考（分类）

> 本文是库公开 API 的权威清单：每个方法列出**签名 / 参数 / 功能 / 用法**。
> 概念背景（航点任务 vs 车载导航、围栏语义等）见 [mission-features.md](mission-features.md)；
> 内部实现见 [architecture.md](architecture.md)。
>
> **总入口只有一个类**：`opmap::OPMapWidget`（QWidget，直接放进布局即显示地图）。
> 全部用户坐标为 **WGS-84**（`opmap::PointLatLng`，注意顺序是 **(纬度 Lat, 经度 Lng)**），
> 高德等 GCJ-02 源由库自动纠偏，调用方无感知。

## 0. 快速上手

```cpp
#include "opmapcontrol.h"

// ① 显示地图 —— 一个 new 即可（默认 GoogleHybrid 源、缩放 2）
opmap::OPMapWidget *map = new opmap::OPMapWidget(this);
layout->addWidget(map);

// ② 两点路径规划 —— 一个方法（仅画线预览，不进导航状态机）
map->PlanRoute(opmap::PointLatLng(34.34, 108.94),   // 起点
               opmap::PointLatLng(34.24, 108.98));  // 终点

// ③ 一键导航 —— 一个方法（以最近喂入的车位置为起点，自动沿路指引）
map->NavigateTo(dest);

// ④ 喂位置 = 驱动一切（导航进度/偏航重规划/轨迹/围栏/任务状态机）
map->UpdateVehiclePosition(pos);   // 运动位置流（行车模拟/GPS/MAVLink 遥测）
map->SetUAVPos(0, pos, 120);       // 任务飞行喂点（带围栏判定）
```

---

## 1. 地图显示与视图控制（OPMapWidget）

| 方法 | 参数 | 功能 |
|------|------|------|
| `OPMapWidget(QWidget *parent=0, Configuration *config=new Configuration)` | 父控件；配置对象（可选，自定义缓存时传入） | 构造即显示地图：默认 `GoogleHybrid` 源、缩放 2 |
| `SetMapType(MapType::Types)` / `GetMapType()` | 见下表 | 切换地图源；缩放上下限自动按源钳制 |
| `SetZoom(double)` | 2.0 ~ 当前源 MaxZoom | 缩放（`ZoomReal()` 实际级 / `ZoomDigi()` 数字放大 / `ZoomTotal()` 合计） |
| `SetMaxZoom(int)` / `SetMinZoom(int)` | 缩放级 | 手动收紧上下限（默认随源：高德 3~18，OSM/ArcGIS 19，Google 20） |
| `SetRotate(qreal)` / `Rotate()` | 角度（度） | 地图旋转（自动开平滑插值） |
| `SetCurrentPosition(PointLatLng)` / `CurrentPosition()` | WGS-84 中心点 | 定位地图中心 |
| `SetCanDragMap(bool)` | — | 拖动开关（默认可拖） |
| `SetShowTileGridLines(bool)` | — | 瓦片网格线（调试用） |
| `SetShowCompass(bool)` | — | 指北针（默认显示） |
| `SetShowScale(bool)` / `ShowScale()` | — | 比例尺（左下角，按缩放级别+中心纬度实时换算 1/2/5×10ⁿ 整距离；默认显示） |
| `SetShowDiagnostics(bool)` | — | 叠显线程/缓存诊断信息 |
| `SetFollowMouse(bool)` | — | 鼠标跟随模式 |
| `SetMouseWheelZoomType(Types)` | `MousePositionAndCenter` / `Center` | 滚轮缩放锚点 |
| `SetUseOpenGL(bool)` | — | OpenGL 渲染开关 |
| `ReloadMap()` | — | 清空内存缓存强制重载 |
| `isStarted()` | — | 引擎是否已启动 |
| `SetSelectedArea(RectLatLng)` / `SelectedArea()` | WGS-84 矩形 | 框选区域（供离线下载，见 §9） |

**地图源枚举**（`opmap::MapType`，数值是缓存索引不可改）：

| 枚举 | 说明 |
|------|------|
| `GoogleMap` / `GoogleSatellite` / `GoogleLabels` / `GoogleTerrain` / `GoogleHybrid` | Google 系（maxzoom 20） |
| `OpenStreetMap` | OSM（maxzoom 19，全球连续，验证循环/低缩放用） |
| `ArcGIS_Map` / `ArcGIS_Satellite` / `ArcGIS_WorldTopo` | ArcGIS（maxzoom 19） |
| `AutoNaviRoad` / `AutoNaviSatellite` / `AutoNaviLabels` / `AutoNaviHybrid` | 高德（GCJ-02 自动纠偏，minzoom 3 / maxzoom 18；境内数据好，境外/极低缩放有空白） |

辅助类 `opmap::Helper`：`MapTypeFromString` / `StrFromMapType` 等枚举↔字符串互转（AccessMode、LanguageType、UAVTrailType 同理）。

## 2. 坐标与几何换算

| 方法 | 参数 → 返回 | 功能 |
|------|------------|------|
| `currentMousePosition()` | — → `PointLatLng` | 当前鼠标位置的经纬度 |
| `GetFromLocalToLatLng(QPointF)` | 视口像素 → 经纬度 | 像素→地理 |
| `GetFromLatLngToLocal(PointLatLng)` | 经纬度 → `QPointF` | 地理→视口像素 |
| `metersToPixels(double)` | 米 → 像素 | 按当前缩放换算 |
| `bearing(from, to)` | 两点 → 角度（度） | 方位角（正北 0 顺时针） |
| `destPoint(source, bearing, dist)` | 源点+方位角+**千米** → `PointLatLng` | 极线推算目标点 |

## 3. 航点管理

航点是带编号的地图图元，可拖拽编辑、右键菜单内置（删除/属性）。编号由库自动连锁维护。

| 方法 | 参数 | 功能 |
|------|------|------|
| `WPCreate()` | 无 | 在地图中心建航点，返回 `WayPointItem*` |
| `WPCreate(coord, altitude)` / `WPCreate(coord, altitude, description)` | 坐标 / 高度 m / 描述 | 建航点（共 5 种重载，含直接传 item） |
| `WPInsert(coord, altitude, position)` | 同上 + 插入位置 | 指定位置插入，后续编号自动连锁 |
| `WPDelete(WayPointItem*)` / `WPDeleteAll()` | — | 删除单个 / 全部 |
| `WPAll()` | — → `QMap<int, WayPointItem*>` | 全部航点（按编号升序） |
| `WPSelected()` | — → `QList<WayPointItem*>` | 当前选中的航点 |
| `WPRenumber(item, newnumber)` | 航点、新编号 | 重编号（其余自动连锁） |

**WayPointItem 属性**（经返回指针或 `WPAll()` 取用）：

| 方法 | 功能 |
|------|------|
| `SetCoord(PointLatLng)` / `Coord()` | 坐标 |
| `SetAltitude(double)` / `Altitude()` | 高度（米） |
| `SetDescription(QString)` / `Description()` | 描述文字 |
| `SetNumber(int)` / `Number()` | 编号 |
| `SetReached(bool)` | 到达打勾（任务引擎自动调用，一般无需手动） |
| `SetAction(WayPointAction)` | 到达动作：`WayPointActionNone` / `WayPointActionPhoto`（拍照）/ `WayPointActionHover`（悬停） |
| `SetHoverTime(int)` | 悬停时长（秒，仅 Hover 动作生效） |

航点变化信号：`WPInserted(编号, 航点)` / `WPDeleted(编号)` / `WPNumberChanged(旧, 新, 航点)` / `WPValuesChanged(航点)` / `WPReached(航点)`。

**航点任务文件**（JSON 格式 `opmap-waypoints`，缩进可读可手改；旧 AP 列式 `.wp` 仍可导入）：

| 方法 | 功能 |
|------|------|
| `WPExportToFile(path, *error)` | 导出全部航点为 JSON：编号/经纬度/海拔/描述/到达动作/悬停时长**全属性**保留 |
| `WPImportFromFile(path, *error)` | 导入航点（先清空现有）；按文件头自动嗅探 JSON 或旧 `.wp` |

```json
{
    "format": "opmap-waypoints",
    "version": 1,
    "count": 2,
    "waypoints": [
        { "number": 1, "lat": 34.34, "lng": 108.94, "alt": 100,
          "description": "起飞点", "action": "hover", "hoverTime": 30 },
        { "number": 2, "lat": 34.35, "lng": 108.95, "alt": 120,
          "description": "", "action": "photo" }
    ]
}
```

## 4. 位置元素与喂点（核心机制）

地图不产生位置——**位置永远由外部喂入**，喂点同时驱动：UAV 图标移动、轨迹采样、导航进度/偏航检测、围栏越界判定、任务状态机推进。

| 方法 | 参数 | 功能 |
|------|------|------|
| `UpdateVehiclePosition(PointLatLng)` | WGS-84 位置 | **运动位置流**喂点（行车模拟/系统 GPS/MAVLink 遥测）：移动车辆图标 + 驱动导航引擎 + 围栏判定 + 任务推进 |
| `SetUAVPos(int id, PointLatLng, int alt)` | 机 id（主机 0）、位置、高度 m | **任务飞行**喂点（等价于 UpdateVehiclePosition + 围栏判定的语义入口；**任务状态机只认主机 0**，僚机喂点不推主机任务进度；轨迹按机分道记录） |
| `GetUAV(int id)` → `UAVItem*` | — | 取 UAV 图元；`GetUAV(0)->SetUAVHeading(角度)` 设置航向 |
| `AddUAV(int id)` / `DeleteUAV(int id)` / `GetUAVS()` | — | 多机管理（僚机等；多机轨迹分道见 §8） |
| `SetShowUAV(bool)` | — | UAV 图元开关（注意：当前实现会连带创建/删除 GPSItem，多机场景用 `AddUAV(0)` 规避） |
| `SetShowHome(bool)` / `ShowHome()` | — | Home 返航点开关 |
| `SetUavPic(路径)` | qrc 路径（`/uavs` 前缀） | UAV 默认图标；单架换图标用 `GetUAV(0)->SetIcon(全路径)` |
| `SetShowRoute(bool)` / `ShowRoute()` | — | 导航路线显示开关（导航开始自动显示） |

**UAVItem 轨迹/到达配置**（经 `GetUAV(0)` 取用）：

| 方法 | 功能 |
|------|------|
| `SetTrailType(UAVTrailType::NoTrail / ByTimeElapsed / ByDistance)` | 轨迹采样方式 |
| `SetShowTrail(bool)` / `SetShowTrailLine(bool)` | 轨迹点 / 轨迹连线显示 |
| `DeleteTrail()` | 清空轨迹（从头记录） |
| `SetAutoSetReached(true)` + `SetAutoSetDistance(米)` | 进入半径自动打勾 + 发 `UAVReachedWayPoint`（上层不用算距离） |
| `SetIcon(路径)` | 换图标（如四旋翼 `:/uavs/images/mapquad.png`） |

**HomeItem**（经 `Home` 指针）：`SetCoord(PointLatLng)`（返航点位置）/ `SetSafeArea(米)`（圆形安全围栏半径，超出发 `UAVLeftSafetyBouble`）/ `SetShowSafeArea(bool)`。

**通用标记 MapMarkerItem**（纯装饰图元：任意图片/文字钉在坐标上，不进任务序列/导航/围栏，与航点编号互不干扰）：

| 方法 | 功能 |
|------|------|
| `AddMarker(pos, 图片路径=空)` → `MapMarkerItem*` | 创建标记并返回句柄；可选中（青色虚线框反馈，供"删除选中标记"类操作），不可拖动 |
| `RemoveMarker(marker)` / `ClearMarkers()` | 删除单个 / 清除全部 |
| `marker->SetImage(路径)` / `SetImageSize(宽, 高)` | 图片内容与显示尺寸（任一维 0 = 等比/原始；文件或 qrc 路径） |
| `marker->SetText(文本)` / `SetFontSize(磅)` | 文字标签（锚点下方，黑描边白字，亮暗底图可读） |
| `marker->SetCoord(pos)` | 移动标记到新坐标（开启轨迹后每次移动自动记录足迹） |
| `marker->SetShowTrail(bool)` | 移动轨迹显示开关：足迹连为橙色折线（独立轨迹图元，随地图拖动/缩放/旋转跟随，上限 1000 点，随标记删除清除） |

**贝塞尔弧线航线**（迁徙图/航线可视化：地理空间拱弧 + 沿弧方向箭头 + 流动光效，ECharts 迁徙图风格）：

| 方法 | 功能 |
|------|------|
| `AddArcLine(from, to, color=橙, side=+1)` → `ArcLineItem*` | 添加弧线航线：拱顶自动按端点距离 0.18 倍生成；`side`=+1/-1 控制右/左拱（相邻段交替呈 S 形） |
| `line->ArcPointAt(t)` | 弧上参数 t∈[0,1] 的地理坐标——**上层驱动标记沿弧飞行用**，位置与弧线严格重合 |
| `line->SetColor / SetArrowCount(n) / SetFlowEnabled(bool)` | 调颜色 / 沿弧箭头数量 / 流动光效开关 |
| `RemoveArcLine(line)` / `ClearArcLines()` | 删除单条 / 清除全部 |

## 5. 车载导航（一行式已达成）

规划、画线、沿路中文转向指引、偏航自动重规划、到达判定、已走/未走着色**全部在库内**。上层只做三件事：规划 → 喂点 → 听信号。

| 方法 | 参数 | 功能 |
|------|------|------|
| `PlanRoute(from, to)` | 两点 | **仅规划画线**（看距离/时间），不进导航状态机、不喂点 |
| `SelectRoute(int index)` | `routeAlternativesReady` 给出的索引，0=推荐 | 切换备选路线：画布立即换线并发 `routeSelected`，之后的导航/偏航重规划沿选中的走 |
| `NavigateTo(PointLatLng dest)` | 目的地 | **规划并开始导航**：起点=最近喂入的车位置（无则地图中心），成功后自动沿路指引 |
| `StopNavigation()` | — | 停止并清除路线绘制 |
| `IsNavigating()` / `CurrentNavigationRoute()` | — | 状态查询 |
| `HasVehiclePosition()` / `VehiclePosition()` | — | 最近一次喂入的车位置 |
| `SetRouteProvider(AbstractRouteProvider*)` | provider（接管所有权） | 换规划服务：默认 `OsrmRouteProvider`（免 key）；`AmapRouteProvider`（需高德 key） |
| `GetNavigationEngine()` → `NavigationEngine*` | — | 调整引擎默认参数（下表） |

**NavigationEngine 参数**（均有合理默认，一般不用动）：

| 方法 | 默认 | 功能 |
|------|------|------|
| `SetAutoReroute(bool)` | 开 | 偏航自动重规划 |
| `SetOffRouteThresholdM(double)` | 50 | 偏离多少米算偏离 |
| `SetOffRouteConsecutiveFixes(int)` | 3 | 连续几次偏离才确认（防 GPS 抖动） |
| `SetArrivalThresholdM(double)` | 30 | 到达半径，到点自动停止 |
| `SetMinRerouteIntervalMs(int)` | 5000 | 两次重规划最小间隔 |

**导航信号**：`navigationRouteReady(Route)` / `navigationProgress(剩余米, 剩余秒, 中文转向指令)` / `offRouteDetected(位置, 偏离米)` / `rerouteReady(Route)` / `navigationArrived()` / `navigationFailed(原因)` / `routeAlternativesReady(备选路线全集)` / `routeSelected(索引, 路线)`。

`opmap::Route` 数据结构：`polyline`（WGS-84 完整折线）、`steps`（分步转向指令，`startIndex` 指向折线）、`totalDistanceMeters`、`totalDurationSeconds`、`isValid()`。

## 6. 航点任务飞行（任务状态机，喂点驱动）

任务的全部状态逻辑（到达判定、悬停计时、动作触发、航点推进、完成）在库内 `WaypointMissionEngine`。**纯喂点驱动无内部定时器**：真机（飞控自己飞，地面站只喂遥测）与模拟器同构接入。

```cpp
map->StartWaypointMission(map->WPAll().values(), 15.0);  // 15m 到达半径
// 此后每次喂点（SetUAVPos / UpdateVehiclePosition）库自动推进状态机
```

| 方法 | 参数 | 功能 |
|------|------|------|
| `StartWaypointMission(QList<WayPointItem*>, double arrivalRadiusMeters=15.0)` | 航点列表、到达半径 | 组装并启动任务（坐标/悬停/动作从航点图元自动提取） |
| `StopWaypointMission()` | — | 中止 |
| `IsWaypointMissionActive()` | — | 任务是否进行中 |

**任务信号**（上层在此响应业务动作，如拍照=下发相机指令）：

| 信号 | 触发时机 |
|------|---------|
| `missionStarted()` | 任务启动 |
| `missionCurrentWaypointChanged(int index)` | 目标航点切换（0 起） |
| `missionWaypointReached(int index, int action)` | 抵达某航点 |
| `missionHoverStateChanged(bool hovering, int seconds)` | 悬停开始/结束 |
| `missionActionTriggered(int index, int action)` | 到达动作触发（拍照立即发、悬停进入时发） |
| `missionFinished()` | 全部航点完成 |

## 7. 多边形地理围栏

语义：**多边形内部 = 允许飞行区**（与 Home 圆形 SafeArea 互补）。越界判定内置在喂点链路里，对所有位置源统一生效，上层无需调用判定。

| 方法 | 参数 | 功能 |
|------|------|------|
| `SetGeofence(QList<PointLatLng>)` | 顶点列表 | 空列表=清除；1~2 点=取点预览（不判定）；≥3 点=生效 |
| `ClearGeofence()` / `HasGeofence()` | — | 移除 / 是否已设置 |
| `CheckGeofence(PointLatLng)` | 位置 | 手动判定（喂点链路已自动调用，一般无需调用） |

信号：`geofenceBreach(位置)`（出界，状态翻转才发一次）/ `geofenceEntered(位置)`（回界内复位）。

## 8. 地图测距与运动轨迹（记录/保存/回放）

**多点测距**（复用取点模式机制，防抖/橡皮筋预览/每段与总距离标注全在库内）：

```cpp
map->SetPickMode(opmap::OPMapWidget::PickMeasure);  // 进入：逐点点击画折线
// 右键 / SetPickMode(PickNone) 结束一段 → measureFinished 发结果，画面保留可继续追加
map->ClearMeasurements();                           // 清除全部测距折线
```

| 成员 | 功能 |
|------|------|
| `PickMeasure`（PickMode 枚举值） | 测距模式：每段大圆距离（Haversine）实时标注，右键结束一段 |
| `ClearMeasurements()` / `HasMeasurements()` | 清除全部 / 是否有测距内容 |
| 信号 `measureFinished(totalMeters, points)` | 一段测距结束（总距离米 + 顶点序列） |

**运动轨迹记录与回放**（engine 层 `TrailRecorder`，纯数据 + 定时回放，**按机分道**）：

```cpp
map->StartTrailRecording();               // 开始：主机 0 的喂点自动入库（含时间戳）
map->StopTrailRecording();                // 停止（缓冲保留）
map->SaveTrailToFile("flight.json");      // 存盘（opmap-trail JSON：位置+高度+相对毫秒）
map->LoadTrailFromFile("flight.json");    // 加载
map->StartTrailReplay(2.0);               // 2 倍速回放：插值喂 SetUAVPos，图标/轨迹/围栏全联动

// 多机场景：全部 API 带可选 uavId（默认 0，老代码零改动）
map->StartTrailRecording(1);              // 单独记录 1 号机（各机同时记录互不混流）
map->SaveTrailToFile("uav1.json", &err, 1);   // 保存 1 号机轨迹
map->StartTrailReplay(2.0, 1);            // 回放 1 号机（插值喂 SetUAVPos(1)）
```

| 方法 | 功能 |
|------|------|
| `StartTrailRecording(uavId=0)` / `StopTrailRecording(uavId=0)` / `IsTrailRecording(uavId=0)` | 记录控制（按机分道，多机同时记录互不混流；与回放互斥） |
| `ClearTrailRecording()` / `TrailPointCount(uavId=0)` | 清空全部道 / 指定机采样点数 |
| `SaveTrailToFile(path, *error, uavId=0)` / `LoadTrailFromFile(path, *error, uavId=0)` | JSON 文件存取（格式 `opmap-trail`，人类可读；加载目标机由参数决定） |
| `StartTrailReplay(speed=1.0, uavId=0)` / `StopTrailReplay()` / `IsTrailReplaying()` | 时间轴变速回放指定机（播完自动停） |
| 信号 `trailReplayFinished()` | 回放自然播完（主动中止不发） |

## 9. IP 定位兜底 / MAVLink 遥测 / 离线下载

**IP 定位**（城市级兜底，双源自动回退 + 8 秒超时，库内置）：

| 方法 | 功能 |
|------|------|
| `RequestIpLocation()` 槽 | 发起一次（在途重复调用被忽略），结果经信号返回 |
| `RequestIpLocation(IpLocationCallback)` | 回调式一步到位：完成/失败自动回调一次 `cb(ok, pos, city)`，信号仍并行发射，两种消费方式任选 |
| `IsIpLocationBusy()` | 是否在途 |

信号：`ipLocationReady(pos, city)` / `ipLocationFailed(reason)`。坐标为城市级精度（约数公里），**只可作显示/兜底，不可喂导航引擎**。

**地面海拔查询**（open-meteo 免 key，8 秒超时，库内置；地图瓦片不含高程数据，真实海拔只能来自在线高程服务）：

| 方法 | 功能 |
|------|------|
| `RequestElevation(pos)` 槽 | 查询 WGS-84 坐标处地面海拔（米），结果经信号返回；在途重复调用被忽略 |
| `RequestElevation(pos, ElevationCallback)` | 回调式一步到位：完成/失败自动回调一次 `cb(ok, pos, altM)`，信号仍并行发射，两种消费方式任选 |
| `IsElevationBusy()` | 是否在途 |

信号：`elevationReady(pos, altitudeMeters)` / `elevationFailed(reason)`（pos 为请求坐标回显）。

**MAVLink 遥测**（`opmap::MavlinkTelemetryProvider`，独立类，真机/SITL 接入点）：

```cpp
auto *mav = new opmap::MavlinkTelemetryProvider(this);
mav->start(14550);                                  // 默认 14550
connect(mav, SIGNAL(positionUpdated(double,double,double,double)),
        this, SLOT(onMavPos(double,double,double,double)));  // → 喂 UpdateVehiclePosition
```

| 成员 | 功能 |
|------|------|
| `start(quint16 port=14550)` / `stop()` / `isListening()` | 监听控制 |
| 信号 `positionUpdated(lat, lon, altM, headingDeg)` | GLOBAL_POSITION_INT 解析结果（hdg 无效为 -1） |
| 信号 `linkAlive()` / `linkTimeout()` | 首包到达 / 5 秒无包 |

**离线地图下载**：

| 方法 | 功能 |
|------|------|
| `SetSelectedArea(RectLatLng)` | 框选区域（地图上按 Ctrl 拖动亦可） |
| `RipMap()` 槽 | 抓取框选区域瓦片入 SQLite 缓存 |
| 信号 `mapDownloadProgress(百分比)` / `mapDownloadTiles(总数, 已完成)` / `mapDownloadFinished()` | 下载进度 |

## 10. 缓存与配置（Configuration）

构造时注入或经 `map->configuration` 访问。

| 方法 | 功能 |
|------|------|
| `SetAccessMode(AccessMode::ServerOnly / ServerAndCache / CacheOnly)` | 访问模式（默认网络+缓存） |
| `SetUseMemoryCache(bool)` / `SetTileMemorySize(int MB)` / `TileMemoryUsed()` | 内存缓存 |
| `DeleteTilesOlderThan(int 天)` | 清理旧瓦片 |
| `ExportMapDataToDB(源库, 目标库)` | 缓存库增量导出 |
| `CacheLocation()` / `SetCacheLocation(路径)` | 缓存目录 |
| `SetLanguage(LanguageType::Types)` | 瓦片语言 |
| 外观类：`EmptytileBrush` / `EmptyTileBorders` / `EmptyTileText` / `MissingDataFont` / `SelectionPen` / `ScalePen` / `DragButton` | 样式定制（低频） |

## 11. 扩展点

| 扩展点 | 用法 |
|--------|------|
| 自定义路径规划服务 | 继承 `AbstractRouteProvider`，实现 `requestRoute(from, to)` + `isBusy()`，完成后发 `routeReady(Route)` / `routeFailed(reason)`；`SetRouteProvider()` 注入 |
| 自定义地理锚定图元 | `GetMap()` 取内部画布；QGraphicsItem 子类实现 `MapAnchoredItem` 接口（`RefreshPos()` 内做 `FromLatLngToLocal` 换算）即自动跟随地图拖动/缩放，**无需改 MapGraphicItem 分派链**；仍需 `enum { Type = UserType+N }` + 重写 `type()`（qgraphicsitem_cast 依据，参考 `GeofenceItem` / `MarkerTrailItem`） |
| 坐标纠偏 | 声明新地图源时在 `MapType::DatumByType` 登记 `TileDatum`（WGS84/GCJ02），其余自动 |

## 12. 信号总表

| 分类 | 信号 |
|------|------|
| 鼠标/视图 | `mouseMove/Press/Release(QMouseEvent*)`、`zoomChanged`（双签名）、`OnCurrentPositionChanged`、`OnMapDrag`、`OnMapZoomChanged`、`OnMapTypeChanged` |
| 瓦片加载 | `OnTileLoadStart`、`OnTileLoadComplete`、`OnTilesStillToLoad(n)`、`OnEmptyTileError` |
| 航点 | `WPInserted` / `WPDeleted` / `WPNumberChanged` / `WPValuesChanged` / `WPReached` |
| UAV | `UAVReachedWayPoint`、`UAVLeftSafetyBouble` |
| 导航 | `navigationRouteReady` / `navigationProgress` / `offRouteDetected` / `rerouteReady` / `navigationArrived` / `navigationFailed` / `routeAlternativesReady` / `routeSelected` |
| 任务 | `missionStarted` / `missionCurrentWaypointChanged` / `missionWaypointReached` / `missionHoverStateChanged` / `missionActionTriggered` / `missionFinished` |
| 围栏 | `geofenceBreach` / `geofenceEntered` |
| IP 定位 | `ipLocationReady` / `ipLocationFailed` |
| 海拔查询 | `elevationReady(pos, 海拔米)` / `elevationFailed` |
| 位置源 | `positionUpdated(pos, altM, headingDeg, source)` / `positionSourceError(reason, fatal)` / `positionLinkAlive` / `positionLinkTimeout` |
| 取点 | `positionPicked(mode, pos)` / `pickFinished(mode, points)` / `mapContextMenuRequested(pos)` |
| 测距/轨迹 | `measureFinished(总米数, 点列表)` / `trailReplayFinished` |
| 跟随 | `mapFollowChanged(on)` |
| 下载 | `mapDownloadProgress` / `mapDownloadTiles` / `mapDownloadFinished` |

---

## 13. 简化用法差距分析（已全部实施）

目标形态：**传一个参数库做大部分工作**——调用一个方法显示地图、传两个点路径规划。逐条对照：

### 13.1 已达成的一行式

| 需求 | 现状 |
|------|------|
| 显示地图 | `new OPMapWidget(this)` ✅（默认 GoogleHybrid z2，放进布局即用） |
| 两点路径规划 | `PlanRoute(from, to)` ✅ |
| 一键导航 | `NavigateTo(dest)` ✅ |
| 航点任务 | `StartWaypointMission(WPAll().values())` ✅（图标/起飞点跳转/到达参数均由库自动编排） |

### 13.2 demo 目前替库干的活（已下沉）

| # | demo 现状（位置/行数） | 问题 | 建议的库 API |
|---|----------------------|------|-------------|
| 1 | **位置源管理**：`onPosSourceChanged` 64 行四源互斥 + GPS 惰性创建/回退弹窗 + IP 轮询定时器 + MAVLink 生命周期（mainwindow.cpp:1265-1328，成员 m_gpsSource/m_mavProvider/m_ipTimer/stopGps） | 互斥、惰性创建、轮询、超时全是通用逻辑，每个使用方都要重写一遍 | `SetPositionSource(PositionSource)` 枚举（命名空间级）`SourceNone / SourceExternal(手动喂点) / SourceSystemGps / SourceIpLocation / SourceMavlink`（MAVLink 固定 14550），库内持有并互斥管理全部源；统一信号 `positionUpdated(pos, altM, headingDeg, source)` + `positionSourceError(reason, fatal)` + 链路 `positionLinkAlive/Timeout`。demo 删约 126 行 |
| 2 | **地图点选交互**：PickMode 五态枚举 + `applyPickPoint` 106 行 + 按下/抬起 6px 防抖（mainwindow.cpp:129-137, 1455-1568, 910-931） | 防抖、取点分发是纯库侧能力，却要求每个使用方自建状态机 | `SetPickMode(PickWaypoint / PickOrigin / PickDest / PickFence / PickPosition / PickNone)` + 信号 `positionPicked(mode, pos)`、`pickFinished(mode, points)`；防抖、围栏多点累积与 1~2 点预览库内处理；取点中右键由库拦截为"结束取点"，正常右键经 `mapContextMenuRequested` 转发上层。demo 删约 130 行 |
| 3 | **一键定位**：`onLocateClicked` 三级优先级 + `m_locatePending` + IP 结果处理（mainwindow.cpp:1372-1438） | 定位优先级策略是通用语义（有运动位置→居中车辆；否则 IP 兜底），不该每家重写 | `LocateCurrentPosition()` 槽：库内按优先级取用、IP 兜底、居中并切街区级缩放；结果经现有信号通知。demo 删约 50 行 |
| 4 | **UAV 惰性创建**：`ensureUAV()` + 首次喂点前手动建图标/设轨迹/设图标语义（mainwindow.cpp:1546-1568 及各喂点入口） | `SetUAVPos` 喂点前 UAV 未创建则静默无效，语义陷阱 | `SetUAVPos` 首次喂点自动惰性创建 UAV（默认图标/轨迹），`SetUAVHeading(id, 角度)` 补成槽。demo 删 ensureUAV 全部调用 |
| 5 | **航点飞行前奏**：`onFlightClicked` 105 行里约一半在铺 UI 前置（挑起飞点、图标切换、SetAutoSetReached、地图跳起飞点、暂停跟随）（mainwindow.cpp:671-776） | 状态机已下沉（912e002），剩下的都是"启动一次任务的通用编排" | `StartWaypointMission` 增加默认行为：自动 `ensureUAV`+设到达参数、自动跳转起飞点；库内加 `SetFollowVehicle(bool)`（每次喂点居中，替代 demo 自实现的跟随复选框）。demo 保留的只剩：按钮文案、模拟遥测源 connect、动作日志 |
| 6 | **起终点临时标记**：选点阶段手动 `new WayPointItem` + 关拖拽/选中标志（applyPickPoint 内） | 预览起终点是路线功能的自然组成 | `PlanRoute`/`NavigateTo` 时自动挂起终点图钉（`WayPointItem` 新增 auxiliary 标志：装饰航点不进任务序列、WPAll/WPDeleteAll 跳过、不可拖选；`StopNavigation` 自动清理）。demo 删 m_originMarker/m_destMarker |

### 13.3 建议不下沉（保持在上层）

| 内容 | 理由 |
|------|------|
| 模拟遥测源（`WaypointFlightSimulator` / `NavigationSimulator`） | 假数据属演示性质；真机接入=整体删除模拟器、遥测直喂 `SetUAVPos`/`UpdateVehiclePosition`，链路不变 |
| 业务动作响应（拍照=下发相机指令、悬停=下发悬停指令） | 库只发 `missionActionTriggered` 信号，动作的物理执行因机型而异 |
| 事件日志/状态栏/横幅等 UI 反馈 | 上层自有 UI 风格，库不该绑死表现层 |

### 13.4 下沉后的用法（实际形态）

```cpp
// 全部业务 = 4 行 connect + 1 行喂点
auto *map = new opmap::OPMapWidget(this);
map->SetPositionSource(opmap::SourceMavlink);            // 位置源：库管互斥/生命周期
connect(map, &opmap::OPMapWidget::positionPicked, ...);  // 点选（航点/起终点/围栏）库防抖
map->StartWaypointMission(map->WPAll().values());        // 航点任务：图标/跳转/到达自动
connect(map, &opmap::OPMapWidget::missionActionTriggered, this, &MainWindow::onAction);
```

> 以上 6 项已全部实施：④ UAV 惰性创建（2927fdf）→ ⑤ 任务默认编排（0c315ea）→ ② 点选模式下沉（3cd96a1）→ ③ 一键定位（3449069）→ ① 位置源管理器（af32f95）→ ⑥ 路线端点图钉（6cdfa63）。每项独立成提交、库与 demo 同步重编，既有 API 语义不变（全部为新增）。
