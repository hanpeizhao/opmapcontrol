# 任务功能说明：航点 / UAV / Home / 安全围栏 / 框选

> 本文档回答"库里的航点、无人机图标、Home、半径圈、框选区域这些功能是干什么用的、现实中怎么用"。
> 本库源自 OpenPilot 无人机地面站（Ground Control Station, GCS）项目，以下功能均为无人机作业领域的标准概念。

## 1. 全景：两类任务模式

| 模式 | 面向对象 | 交互方式 | 库内实现 |
|------|---------|---------|---------|
| **航点任务** | 无人值守的机器（无人机/船/车） | 预先规划一串目的地，机器按顺序自动逐点执行 | WayPointItem + UAVItem + Home |
| **路径导航** | 人（驾驶员/行人） | 设一个目的地，沿道路实时指引 | NavigationEngine（NavigateTo） |

手机地图导航属于第二类，所以平时"见不到"航点任务——它出现在大疆 DJI Pilot 的"航点飞行"、开源 Mission Planner 等地面站软件里。

## 2. 航点 WayPointItem —— 任务目的地序列

**是什么**：地图上编号的目的地标点（1、2、3…），飞行器按编号顺序自动逐个飞抵。

**现实场景**：
- 电力巡线：每座铁塔一个航点，无人机自动沿塔串飞整条线路拍照
- 航测测绘：规划"S"形航带，每个航点自动触发相机
- 农业植保：沿地块边界打点，自动覆盖喷洒

**库 API**（facade OPMapWidget）：
- `WPInsert(坐标, 高度, 描述, 位置)` / `WPDelete` / `WPRenumber` / `WPAll()` / `WPSelected()`
- 航点项：`SetDescription`（说明文字）、`SetAltitude`（该点高度）、`SetReached(bool)`（打勾）
- 航点动作：`SetAction(WayPointAction)`——`WayPointActionNone`（无）/ `WayPointActionPhoto`（到达即拍照）/ `WayPointActionHover`（到达后悬停），配合 `SetHoverTime(秒)`；枚举定义在任务引擎 `WaypointMissionEngine::WaypointAction`（WayPointItem 为引用别名），动作触发统一由任务引擎信号表达
- 信号：`WPInserted` / `WPDeleted` / `WPNumberChanged` / `WPValuesChanged` / `WPReached`

**demo 状态**：✅ 地图点选添加、删除、`.wp` 文件导入导出、中点插入、编号重排（航点面板）；✅ 航点面板"到达动作"下拉（无/拍照/悬停 30 秒），点选航点前先选动作，飞行日志中显示"【动作】触发拍照 / 原地悬停 30 秒"。

### 2.1 航点任务飞行：WaypointMissionEngine（库内状态机）

**是什么**：任务飞行的全部状态逻辑——到达判定、悬停计时、动作触发、航点推进、任务完成——都在库内完成（`src/engine/waypointmissionengine.h/.cpp`）。**纯喂点驱动，无内部定时器**：真机（飞控自己沿航点飞，地面站只喂遥测）与模拟器（插值生成位置流）同构接入。

**库 API**（facade OPMapWidget）：
- `StartWaypointMission(WPAll() 航点列表, 到达半径米=15)`：从航点图元提取坐标/悬停时长/动作组装任务并启动
- `StopWaypointMission()` / `IsWaypointMissionActive()`
- 喂点即驱动：`UpdateVehiclePosition` 或 `SetUAVPos` 每喂一个位置，库自动推进状态机
- 信号：`missionStarted` / `missionCurrentWaypointChanged(下标)`（目标切换）/ `missionWaypointReached(下标, 动作)` / `missionHoverStateChanged(bool, 秒)` / `missionActionTriggered(下标, 动作)`（拍照立即发、悬停进入时发）/ `missionFinished`

**demo 状态**：✅ 模拟器已退化为**纯假遥测源**（`WaypointFlightSimulator` 只剩"朝库下发的目标匀速推进 + 悬停原地心跳"），目标切换/悬停/动作/完成全部由库信号驱动。**真机接入 = 整体删除模拟器，遥测直接喂 `SetUAVPos`，任务判定链路不变。**

## 3. UAVItem —— 遥测实时位置图标

**是什么**：机载 GPS 通过数传链路（电台/4G）每秒回传的位置与航向，画在地图上就是那架小飞机。地图不产生位置——位置永远由外部"喂"进来。

**库 API**：
- `SetUAVPos(位置, 高度[, 颜色])`、`SetUAVHeading(航向)`、`SetUAVPic`（自定义图标）
- 自动到达判定（核心机制）：`SetAutoSetReached(true)` + `SetAutoSetDistance(米)` 后，每次喂点库内自动计算与各航点的 3D 距离，进入半径即自动 `SetReached(true)` 并发出 `UAVReachedWayPoint` 信号——**上层不需要自己算距离**
- 多机：`AddUAV(id)` / `DeleteUAV(id)` / `GetUAV(id)`（编队演示）
- 轨迹尾迹自动绘制

**demo 状态**：✅ 僚机绕飞（多机）、**航点飞行按钮**（航点面板）：点"航点飞行"后，UAV 从 Home 位置起飞、以 25 m/s 依次飞过全部航点，库自动打勾并发 `UAVReachedWayPoint`，拍照/悬停航点同步触发动作日志。将来接真机时，只需把模拟器的 `positionChanged` 换成真机遥测回调，喂点链路（`SetUAVPos`）完全不变。

### 3.1 真机遥测接入：MavlinkTelemetryProvider

**是什么**：库内置的 MAVLink v1/v2 UDP 遥测接收器（`src/providers/mavlinktelemetryprovider.h/.cpp`），监听 UDP 14550 端口（地面站标准端口），解析 `GLOBAL_POSITION_INT`（msgid 33）帧，CRC16-CCITT X.25 校验后发出位置信号——这是飞控（Pixhawk/ArduPilot/PX4、Mission Planner SITL 模拟）向地面站回传位置的行业默认链路。

**库 API**：
- `start(端口=14550)` / `stop()` / `isListening()`
- 信号：`positionUpdated(lat, lon, altM, headingDeg)`（hdg 无效值 65535 → -1）、`linkAlive()`（首包到达）、`linkTimeout()`（5 秒无包，只发一次）

**demo 状态**：✅ 行车模拟面板"位置源"新增"MAVLink (UDP)"项，选择后开始监听 14550，位置喂入 `UpdateVehiclePosition`（与模拟/GPS 源互斥）。**测试无需真机**：装 Mission Planner 开 SITL 模拟，或任意 MAVLink 遥测源指向本机 14550 即可在地图上看到飞机动起来。

## 4. Home 返航点 + 安全围栏

**是什么**：Home 是**返航点**（起飞位置），其上的 `SafeArea` 半径是一个**以返航点为圆心的安全围栏**。库在每次喂点时自动判定：飞机距返航点超出半径 → 发出 `UAVLeftSafetyBouble` 信号（围栏圈同时变红警示）。

**现实场景**：
- 失联保护：飞机飞出安全半径告警，提醒飞手它离返航点过远
- 真实地面站的"一键返航"（RTL）就基于 Home 位置
- 这是**圆形围栏**；不规则形状的禁飞区轮廓用多边形地理围栏（库已内置，见 4.1 节）

**库 API**：`m_map->Home->SetCoord()` / `SetSafeArea(米)` / `SetShowSafeArea(bool)`；信号 `UAVLeftSafetyBouble(PointLatLng)`。

**demo 状态**：✅ 航点飞行起飞时自动把 Home 设在起飞位置并打开 3000 m 安全圈；僚机演示用 400 m 圈。

### 4.1 多边形地理围栏 GeofenceItem

**是什么**：**禁飞区/作业区轮廓**围栏——在地图上取 3 个以上顶点围成多边形，语义为"多边形内部为允许飞行区"。与 Home 的圆形 SafeArea 互补：圆形围栏回答"飞得离我多远"，多边形围栏回答"有没有飞进不该去的地方"（河湖、机场净空区、军事区边界都是不规则形状）。

**库 API**（facade OPMapWidget）：
- `SetGeofence(QList<PointLatLng>)`（<3 点视作清除）/ `ClearGeofence()` / `HasGeofence()`
- 越界判定内置在 `UpdateVehiclePosition` 喂点链路里（射线法，奇偶规则），对所有位置源（模拟/GPS/MAVLink）统一生效；出界发一次 `geofenceBreach(PointLatLng)` 信号，回到界内自动复位、再次出界可再次触发
- 地图上以红色虚线边界 + 半透明红色填充绘制，随地图拖动/缩放自动换算

**demo 状态**：✅ 演示面板"绘制围栏"按钮三态：绘制围栏（地图连续取点）→ 结束围栏（≥3 点闭合生效）→ 清除围栏。配合行车模拟或 MAVLink 遥测，飞机飞出多边形即触发 `geofenceBreach` 信号（事件日志可见）。

## 5. 框选区域 —— 离线地图下载

**是什么**：按住 Alt/Shift 在地图上拖一个矩形（`SetSelectedArea`），配合"下载框选区域离线瓦片"把该区域的瓦片批量下载进本地缓存（SQLite）。库会自动把经纬度框换算成瓦片坐标系再枚举下载列表。

**现实场景**：外场作业没有网络（山区/海上/机舱内），起飞前在有网环境把任务区域地图下载到本地，飞行中全部命中磁盘缓存。

**demo 状态**：✅ 菜单"地图 → 下载框选区域离线瓦片…"（`mapDownloadProgress/Tiles/Finished` 信号 → 事件日志）。

## 6. 待补充评估（用户决策）

| 功能 | 现状 | 补充价值 |
|------|------|---------|
| 真机接入（MAVLink 遥测） | ✅ 已完成：库内置 `MavlinkTelemetryProvider`（UDP 14550，v1/v2 帧 + CRC 校验），demo 位置源接入，见 3.1 节 | 接串口/4G 遥测后即为真实地面站 |
| 多边形地理围栏 | ✅ 已完成：库内置 `GeofenceItem` + facade 越界判定信号，demo 三态按钮取点，见 4.1 节 | 禁飞区场景需要；与圆形 SafeArea 互补 |
| 航点动作触发（拍照/悬停） | ✅ 已完成：`WayPointItem::SetAction/SetHoverTime` 携带动作数据，demo 到达信号处触发日志，见第 2 节 | 测绘/巡线场景需要 |
| GPSItem（GPS 定位标记） | 库内已实现，未演示 | 单点定位可视化，价值一般 |
