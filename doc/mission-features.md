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
- 信号：`WPInserted` / `WPDeleted` / `WPNumberChanged` / `WPValuesChanged` / `WPReached`

**demo 状态**：✅ 地图点选添加、删除、`.wp` 文件导入导出、中点插入、编号重排（航点面板）。

## 3. UAVItem —— 遥测实时位置图标

**是什么**：机载 GPS 通过数传链路（电台/4G）每秒回传的位置与航向，画在地图上就是那架小飞机。地图不产生位置——位置永远由外部"喂"进来。

**库 API**：
- `SetUAVPos(位置, 高度[, 颜色])`、`SetUAVHeading(航向)`、`SetUAVPic`（自定义图标）
- 自动到达判定（核心机制）：`SetAutoSetReached(true)` + `SetAutoSetDistance(米)` 后，每次喂点库内自动计算与各航点的 3D 距离，进入半径即自动 `SetReached(true)` 并发出 `UAVReachedWayPoint` 信号——**上层不需要自己算距离**
- 多机：`AddUAV(id)` / `DeleteUAV(id)` / `GetUAV(id)`（编队演示）
- 轨迹尾迹自动绘制

**demo 状态**：✅ 僚机绕飞（多机）、**航点飞行按钮**（航点面板）：点"航点飞行"后，UAV 从 Home 位置起飞、以 25 m/s 依次飞过全部航点，库自动打勾并发 `UAVReachedWayPoint`。将来接真机时，只需把模拟器的 `positionChanged` 换成真机遥测回调，喂点链路（`SetUAVPos`）完全不变。

## 4. Home 返航点 + 安全围栏

**是什么**：Home 是**返航点**（起飞位置），其上的 `SafeArea` 半径是一个**以返航点为圆心的安全围栏**。库在每次喂点时自动判定：飞机距返航点超出半径 → 发出 `UAVLeftSafetyBouble` 信号（围栏圈同时变红警示）。

**现实场景**：
- 失联保护：飞机飞出安全半径告警，提醒飞手它离返航点过远
- 真实地面站的"一键返航"（RTL）就基于 Home 位置
- 这是**圆形围栏**；多边形地理围栏（禁飞区轮廓）库未内置，需上层基于同一信号机制扩展

**库 API**：`m_map->Home->SetCoord()` / `SetSafeArea(米)` / `SetShowSafeArea(bool)`；信号 `UAVLeftSafetyBouble(PointLatLng)`。

**demo 状态**：✅ 航点飞行起飞时自动把 Home 设在起飞位置并打开 3000 m 安全圈；僚机演示用 400 m 圈。

## 5. 框选区域 —— 离线地图下载

**是什么**：按住 Alt/Shift 在地图上拖一个矩形（`SetSelectedArea`），配合"下载框选区域离线瓦片"把该区域的瓦片批量下载进本地缓存（SQLite）。库会自动把经纬度框换算成瓦片坐标系再枚举下载列表。

**现实场景**：外场作业没有网络（山区/海上/机舱内），起飞前在有网环境把任务区域地图下载到本地，飞行中全部命中磁盘缓存。

**demo 状态**：✅ 菜单"地图 → 下载框选区域离线瓦片…"（`mapDownloadProgress/Tiles/Finished` 信号 → 事件日志）。

## 6. 待补充评估（用户决策）

| 功能 | 现状 | 补充价值 |
|------|------|---------|
| 真机接入（MAVLink 遥测） | demo 用模拟器 | 接串口/4G 遥测后即为真实地面站 |
| 多边形地理围栏 | 仅圆形安全围栏 | 禁飞区场景需要；可基于 `UAVLeftSafetyBouble` 思路扩展 |
| 航点动作触发（拍照/悬停） | 航点仅位置+高度 | 测绘/巡线场景需要，可在 waypointPassed 处扩展 |
| GPSItem（GPS 定位标记） | 库内已实现，未演示 | 单点定位可视化，价值一般 |
