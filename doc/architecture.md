# 实现原理

本文说明 opmapcontrol_ex 的内部实现。API 功能清单见 [features.md](features.md)。

## 1. 总体架构

库分为五层，依赖方向自上而下单向传递，禁止反向依赖：

```
┌─────────────────────────────────────────────────────┐
│ ui      OPMapWidget / MapGraphicItem / 各地图元素     │  Qt 界面层
├─────────────────────────────────────────────────────┤
│ engine  MapEngine(加载调度) / MapService(数据服务)     │  核心引擎
│         TileMatrix / Tile / PureProjection           │
├──────────────────────────────┬──────────────────────┤
│ providers                    │ cache                │
│ UrlFactory(URL工厂)           │ TileMemoryCache(LRU) │
│ MapType/TileDatum(地图源声明) │ TileDiskCache(SQLite) │
│ ProviderStrings/LanguageType │ TileCacheQueue(写库)  │
├──────────────────────────────┴──────────────────────┤
│ platform  Point/PointLatLng/RectLatLng/Size/Rectangle │
│           CoordTransform(WGS-84↔GCJ-02) / Diagnostics │
└─────────────────────────────────────────────────────┘
```

- **ui → engine**：`OPMapWidget`（QGraphicsView）持有 `MapGraphicItem`（场景中心图形项），后者持有 `MapEngine` 调度瓦片加载。
- **engine → providers + cache**：`MapEngine` 只调用 `MapService` 的接口；`MapService` 是组合服务，内部聚合 `UrlFactory`、三级缓存与写库队列，通过依赖注入由 `OPMapWidget` 构造时创建并传入，无全局单例。
- **cache/platform 无 Qt 业务依赖**：platform 层是纯值类型，可独立复用。

## 2. 瓦片加载流程

```
OPMapWidget (QGraphicsView)
  └─ MapGraphicItem::paint()                    每次重绘请求
       └─ MapEngine::GetPositionbackImage()
            ├─ 计算可视区内的瓦片坐标列表
            ├─ 为每块瓦片创建 LoadTask
            ├─ QThreadPool 并发执行（runningThreads 计数）
            │    └─ LoadTask::run()
            │         ├─ MapService::GetImageFrom()
            │         │    ├─ ① 内存缓存 TileMemoryCache 命中 → 直接返回
            │         │    ├─ ② 磁盘缓存 TileDiskCache 命中 → 回填内存缓存
            │         │    └─ ③ UrlFactory 获取瓦片 URL → QNetworkAccessManager
            │         │         下载（跟随重定向）→ 回填两级缓存
            │         └─ 发射信号通知 UI 线程刷新
            └─ 拼合已就绪瓦片为 QImage，未就绪的画占位空瓦片
```

关键点：

- **UI 不阻塞**：下载在 `QThreadPool` 工作线程执行，`RetryLoadTile` 控制失败重试次数；超时由 `MapService::Timeout()`（默认 5s）控制。
- **请求去重**：同一瓦片在缓存中存在即跳过网络请求，避免重复下载。
- **完成通知**：所有瓦片就绪后发射 `OnTileLoadComplete()`，队列变化实时发射 `OnTilesStillToLoad(n)`。

## 3. 三级缓存设计

| 层级 | 实现 | 特点 |
|------|------|------|
| 内存缓存 | `TileMemoryCache` | 链表 + 哈希的 LRU 结构，容量以 MB 计（`setMemoryCacheCapacity`），超出淘汰最旧瓦片；`QReadWriteLock` 保护并发访问 |
| 磁盘缓存 | `TileDiskCache`（SQLite） | 表 `Tiles`（主键索引：地图类型、缩放级别、x、y）+ 表 `TilesData`（Tile 字段存图片原始字节 BLOB）；支持 `deleteOlderTiles` 按天数清理、`ExportMapDataToDB` 库间增量导出 |
| 网络下载 | `UrlFactory` | 按地图源生成瓦片 URL；支持 HTTPS（需 OpenSSL）与 HTTP 重定向；不同源使用不同的 Referer 策略 |

写入路径独立于读取：磁盘写入由 `TileCacheQueue`（独立 `QThread`）异步完成——下载完成的瓦片先进入内存缓存立即可用，写库请求排入队列由后台线程批量落库，避免磁盘 IO 阻塞 UI。

缓存位置默认 `QDir::homePath() + "/mapscache/"`，example 配置为 `example/demo/data/OPMaps.qmdb`。缓存键中包含**地图类型枚举数值**，因此 `MapType::Types` 的数值一旦改变，历史缓存即无法命中。

## 4. 坐标系纠偏

核心约束：**用户坐标恒为 WGS-84**，地图源瓦片按其声明的基准坐标系渲染。

```
用户坐标(WGS-84) ──┐
                   │  MapGraphicItem 持有 PureProjection
瓦片坐标系(GCJ-02/WGS-84) ──┤
                   │
                   └─ 投影边界处调用 CoordTransform 自动纠偏
```

- `MapType::TileDatum` 枚举声明每个地图源的瓦片对齐坐标系（`DatumByType()`），OSM/ArcGIS/Google 为 WGS-84，高德为 GCJ-02。
- `CoordTransform`（platform 层）实现 WGS-84 ↔ GCJ-02 的互转算法，仅在两者坐标系的边界处做转换，避免逐点纠偏的累积误差。
- 因此同一 WGS-84 位置在不同地图源上都渲染到正确位置，鼠标读出的经纬度恒为 WGS-84，无需上层程序关心地图源差异。

## 5. 投影体系

`PureProjection` 抽象基类定义经纬度 ↔ 平面像素的转换契约：

- `MercatorProjection`（Web 墨卡托，高德/OSM/ArcGIS/Google 默认）
- `PlateCarreeProjection`（等距圆柱投影）

`MapGraphicItem` 根据当前投影完成：可视区瓦片范围计算、`PointLatLng` 与屏幕像素互转（`FromLocalToLatLng`）、比例尺计算、地图旋转。

## 6. 离线下载原理

```
用户框选区域(SelectedArea, WGS-84)
  └─ OPMapWidget::RipMap()
       └─ MapRipper
            ├─ 区域先转换到瓦片坐标系，再计算每级缩放的瓦片列表
            ├─ 逐级逐瓦片走标准三级缓存链路（已有缓存直接跳过）
            └─ 全部完成后提示，数据即落 SQLite，可离线浏览
```

由于离线数据与在线浏览共用同一缓存表，下载完成后重启程序、断网浏览均直接命中磁盘缓存。

## 7. 扩展指南

- **新增地图源**（三步）：
  1. `MapType::Types` 添加枚举值（勿改既有数值）；
  2. `UrlFactory` 的 URL 工厂添加对应 case；
  3. `MapType::DatumByType()` 声明该源的瓦片坐标系。
- **新增地图元素**：继承 `MapGraphicItem`（基类提供拖动、旋转、缩放联动），参考 `WayPointItem`/`UAVItem` 的实现。
- **更换投影**：继承 `PureProjection` 并在 `MapGraphicItem` 中替换。
