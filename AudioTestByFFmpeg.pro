QT       += core multimedia

QT       -= gui

TARGET = AudioTestByFFmpeg
CONFIG   += console
CONFIG   -= app_bundle
         QT += testlib

TEMPLATE = app

SOURCES += main.cpp \
    ffmpegstream.cpp

INCLUDEPATH +=  ffmpeg/include
LIBS += ffmpeg/lib/libavcodec.dll.a \
        ffmpeg/lib/libavdevice.dll.a \
        ffmpeg/lib/libavfilter.dll.a \
        ffmpeg/lib/libavformat.dll.a \
        ffmpeg/lib/libavutil.dll.a \
        ffmpeg/lib/libpostproc.dll.a \
        ffmpeg/lib/libswscale.dll.a \

HEADERS += \
    decodeAndPlay.h \
    ffmpegstream.h
