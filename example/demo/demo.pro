######################################################################
# opmapcontrol 示例程序
# 演示地图浏览、航点管理、路径规划与航点飞行导航
######################################################################

TEMPLATE = app
TARGET = opmapcontrol_example
CONFIG  += release
CONFIG  -= debug_and_release
# positioning：demo 代码已不直接使用（位置源下沉库内），但静态链接 libopmapwidget.a
# 时其 QtPositioning 符号需在本工程链接期解析，故必须保留
QT    += core gui widgets opengl sql svg network positioning


UI_DIR       = ./build
MOC_DIR      = ./build
RCC_DIR      = ./build
OBJECTS_DIR  = ./build


HEADERS += \
    mainwindow.h \
    waypoint_store.h \
    navigation_simulator.h \
    waypoint_flight_simulator.h \
    uas_types.h

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    waypoint_store.cpp \
    navigation_simulator.cpp \
    waypoint_flight_simulator.cpp \
    uas_types.cpp


################################################################################
# opmapcontrol
################################################################################
OPMAPCONTROL_DIR = ../..
INCLUDEPATH +=  $$OPMAPCONTROL_DIR/src \
                $$OPMAPCONTROL_DIR/src/platform \
                $$OPMAPCONTROL_DIR/src/cache \
                $$OPMAPCONTROL_DIR/src/providers \
                $$OPMAPCONTROL_DIR/src/engine \
                $$OPMAPCONTROL_DIR/src/engine/projections \
                $$OPMAPCONTROL_DIR/src/ui
LIBS += $$OPMAPCONTROL_DIR/libopmapwidget.a
# 强依赖：库更新后自动重链 demo（否则 Makefile 不感知 .a 变化，跑的还是旧库）
PRE_TARGETDEPS += $$OPMAPCONTROL_DIR/libopmapwidget.a
RESOURCES   += $$OPMAPCONTROL_DIR/src/ui/mapresources.qrc
