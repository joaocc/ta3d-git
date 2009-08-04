# -------------------------------------------------
# Project created by QtCreator 2009-03-30T19:47:07
# -------------------------------------------------
QT += opengl \
    webkit
TARGET = 3DMEditor2
LIBS += -lz -L../ta3d/src/lua -llua
win32:LIBS += -lglew32.dll
unix:LIBS += -lGLEW
macos:LIBS += -lGLEW
TEMPLATE = app
SOURCES += src/main.cpp \
    src/mainwindow.cpp \
    src/gfx.cpp \
    src/misc/vector.cpp \
    src/misc/matrix.cpp \
    src/misc/camera.cpp \
    src/misc/math.cpp \
    src/aboutwindow.cpp \
    src/misc/material.light.cpp \
    src/mesh.cpp \
    src/3ds.cpp \
    src/obj.cpp \
    src/geometrygraph.cpp \
    src/program.cpp \
    src/textureviewer.cpp \
    src/surfaceproperties.cpp \
    src/shadereditor.cpp \
    src/helpviewer.cpp \
    src/imagelistview.cpp \
    src/progressdialog.cpp \
    src/meshtree.cpp \
    src/ambientocclusionthread.cpp \
    src/springmodelloader.cpp \
    src/toolbox.cpp \
    src/flowlayout.cpp \
    src/animation.cpp \
    src/luaeditor.cpp \
    src/scripts/lua.thread.cpp \
    src/scripts/script.interface.cpp \
    src/scripts/unit.script.cpp \
    src/scripts/unit.script.func.cpp \
    src/scripts/unit.script.interface.cpp
HEADERS += src/mainwindow.h \
    src/config.h \
    src/gfx.h \
    src/misc/vector.h \
    src/misc/matrix.h \
    src/misc/camera.h \
    src/misc/math.h \
    src/types.h \
    src/aboutwindow.h \
    src/misc/material.light.h \
    src/mesh.h \
    src/3ds.h \
    src/obj.h \
    src/geometrygraph.h \
    src/program.h \
    src/textureviewer.h \
    src/surfaceproperties.h \
    src/shadereditor.h \
    src/helpviewer.h \
    src/imagelistview.h \
    src/progressdialog.h \
    src/meshtree.h \
    src/ambientocclusionthread.h \
    src/springmodelloader.h \
    src/toolbox.h \
    src/flowlayout.h \
    src/animation.h \
    src/luaeditor.h \
    src/scripts/lua.thread.h \
    src/scripts/script.h \
    src/scripts/script.interface.h \
    src/scripts/unit.script.h \
    src/scripts/unit.script.interface.h
TRANSLATIONS = i18n/3dmeditor_fr.ts \
    i18n/3dmeditor_en.ts
