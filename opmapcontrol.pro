
# ============================================================
# opmapcontrol 地图控件静态库
# ------------------------------------------------------------
# 分层结构（依赖方向自上而下，禁止反向依赖）：
#   src/ui        Qt 界面控件（OPMapWidget 及地图 item）
#   src/engine    地图引擎（核心调度/瓦片矩阵/投影）
#   src/providers 地图源（MapType/UrlFactory/语言与版本串）
#   src/cache     瓦片缓存（内存 LRU + SQLite 磁盘缓存 + 下载队列）
#   src/platform  基础类型（几何点/矩形、坐标系转换、诊断）
# ============================================================

QT       += core gui network sql widgets
CONFIG   += staticlib release
CONFIG   -= debug_and_release
TEMPLATE  = lib

TARGET    = opmapwidget

DESTDIR      = .
UI_DIR       = ./build
MOC_DIR      = ./build
OBJECTS_DIR  = ./build

INCLUDEPATH += ./src ./src/platform ./src/cache ./src/providers ./src/engine ./src/engine/projections ./src/ui

DEFINES     += OPMAPWIDGET_LIBRARY EXTERNAL_USE


HEADERS += \
    ./src/opmapcontrol.h \
    ./src/platform/point.h \
    ./src/platform/size.h \
    ./src/platform/pointlatlng.h \
    ./src/platform/rectlatlng.h \
    ./src/platform/sizelatlng.h \
    ./src/platform/rectangle.h \
    ./src/platform/coordtransform.h \
    ./src/platform/diagnostics.h \
    ./src/platform/mousewheelzoomtype.h \
    ./src/platform/debugheader.h \
    ./src/cache/rawtile.h \
    ./src/cache/pureimage.h \
    ./src/cache/kibertilecache.h \
    ./src/cache/pureimagecache.h \
    ./src/cache/tilecachequeue.h \
    ./src/cache/cacheitemqueue.h \
    ./src/cache/accessmode.h \
    ./src/providers/maptype.h \
    ./src/providers/urlfactory.h \
    ./src/providers/providerstrings.h \
    ./src/providers/languagetype.h \
    ./src/engine/mapservice.h \
    ./src/engine/core.h \
    ./src/engine/tile.h \
    ./src/engine/tilematrix.h \
    ./src/engine/loadtask.h \
    ./src/engine/pureprojection.h \
    ./src/engine/alllayersoftype.h \
    ./src/engine/projections/mercatorprojection.h \
    ./src/engine/projections/platecarreeprojection.h \
    ./src/ui/gpsitem.h \
    ./src/ui/homeitem.h \
    ./src/ui/mapgraphicitem.h \
    ./src/ui/mapripform.h \
    ./src/ui/mapripper.h \
    ./src/ui/opmapwidget.h \
    ./src/ui/trailitem.h \
    ./src/ui/traillineitem.h \
    ./src/ui/uavitem.h \
    ./src/ui/uavmapfollowtype.h \
    ./src/ui/uavtrailtype.h \
    ./src/ui/waypointitem.h \
    ./src/ui/waypointlineitem.h \
    ./src/ui/omapconfiguration.h \


SOURCES += \
    ./src/platform/point.cpp \
    ./src/platform/size.cpp \
    ./src/platform/pointlatlng.cpp \
    ./src/platform/rectlatlng.cpp \
    ./src/platform/sizelatlng.cpp \
    ./src/platform/rectangle.cpp \
    ./src/platform/coordtransform.cpp \
    ./src/platform/diagnostics.cpp \
    ./src/platform/mousewheelzoomtype.cpp \
    ./src/cache/rawtile.cpp \
    ./src/cache/pureimage.cpp \
    ./src/cache/kibertilecache.cpp \
    ./src/cache/pureimagecache.cpp \
    ./src/cache/tilecachequeue.cpp \
    ./src/cache/cacheitemqueue.cpp \
    ./src/providers/urlfactory.cpp \
    ./src/providers/providerstrings.cpp \
    ./src/providers/languagetype.cpp \
    ./src/engine/mapservice.cpp \
    ./src/engine/core.cpp \
    ./src/engine/tile.cpp \
    ./src/engine/tilematrix.cpp \
    ./src/engine/loadtask.cpp \
    ./src/engine/pureprojection.cpp \
    ./src/engine/alllayersoftype.cpp \
    ./src/engine/projections/mercatorprojection.cpp \
    ./src/engine/projections/platecarreeprojection.cpp \
    ./src/ui/configuration.cpp \
    ./src/ui/gpsitem.cpp \
    ./src/ui/homeitem.cpp \
    ./src/ui/mapgraphicitem.cpp \
    ./src/ui/mapripform.cpp \
    ./src/ui/mapripper.cpp \
    ./src/ui/opmapwidget.cpp \
    ./src/ui/trailitem.cpp \
    ./src/ui/traillineitem.cpp \
    ./src/ui/uavitem.cpp \
    ./src/ui/waypointitem.cpp \
    ./src/ui/waypointlineitem.cpp \


FORMS       += ./src/ui/mapripform.ui
RESOURCES   += ./src/ui/mapresources.qrc
