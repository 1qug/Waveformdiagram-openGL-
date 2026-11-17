#-------------------------------------------------
#
# Project created from Waveformdiagram(openGL).vcxproj
# Qt 6 / qmake project file
#
#-------------------------------------------------

QT += core gui widgets printsupport opengl

TEMPLATE = app

# 避免文件名中的括号进入生成的 Makefile
TARGET = Waveformdiagram_openGL

CONFIG += c++17

# 源码与第三方库（qcustomplot）的包含路径
INCLUDEPATH += $$PWD/src \
               $$PWD/src/functionManager \
               $$PWD/third_party/qcustomplot

# MSVC 下 qcustomplot 的 OpenGL 后端需要显式链接系统 OpenGL 库
win32: LIBS += -lopengl32

SOURCES += \
    src/main.cpp \
    src/WaveformdiagramopenGL.cpp \
    src/functionManager/AmplitudeSpectrum.cpp \
    src/functionManager/ConstellationDiagram.cpp \
    src/functionManager/EighthPowerSpectrum.cpp \
    src/functionManager/EyeDiagram.cpp \
    src/functionManager/FourthPowerSpectrum.cpp \
    src/functionManager/InstAmpHistogram.cpp \
    src/functionManager/InstEnvelope.cpp \
    src/functionManager/InstFreqHistogram.cpp \
    src/functionManager/InstFrequency.cpp \
    src/functionManager/InstPhase.cpp \
    src/functionManager/InstPhaseHistogram.cpp \
    src/functionManager/NonlinearInstPhase.cpp \
    src/functionManager/PowerSpectrum.cpp \
    src/functionManager/Spectrogram.cpp \
    src/functionManager/SquareSpectrum.cpp \
    src/functionManager/WaterfallPlot.cpp \
    src/functionManager/Waveform.cpp \
    src/functionManager/WaveformDataLoader.cpp \
    third_party/qcustomplot/qcustomplot.cpp

HEADERS += \
    src/WaveformdiagramopenGL.h \
    src/functionManager/AmplitudeSpectrum.h \
    src/functionManager/ConstellationDiagram.h \
    src/functionManager/EighthPowerSpectrum.h \
    src/functionManager/EyeDiagram.h \
    src/functionManager/FourthPowerSpectrum.h \
    src/functionManager/InstAmpHistogram.h \
    src/functionManager/InstEnvelope.h \
    src/functionManager/InstFreqHistogram.h \
    src/functionManager/InstFrequency.h \
    src/functionManager/InstPhase.h \
    src/functionManager/InstPhaseHistogram.h \
    src/functionManager/NonlinearInstPhase.h \
    src/functionManager/PowerSpectrum.h \
    src/functionManager/Spectrogram.h \
    src/functionManager/SquareSpectrum.h \
    src/functionManager/WaterfallPlot.h \
    src/functionManager/Waveform.h \
    src/functionManager/WaveformDataLoader.h \
    third_party/qcustomplot/qcustomplot.h

FORMS += \
    src/WaveformdiagramopenGL.ui \
    src/functionManager/AmplitudeSpectrum.ui \
    src/functionManager/ConstellationDiagram.ui \
    src/functionManager/EighthPowerSpectrum.ui \
    src/functionManager/EyeDiagram.ui \
    src/functionManager/FourthPowerSpectrum.ui \
    src/functionManager/InstAmpHistogram.ui \
    src/functionManager/InstEnvelope.ui \
    src/functionManager/InstFreqHistogram.ui \
    src/functionManager/InstFrequency.ui \
    src/functionManager/InstPhase.ui \
    src/functionManager/InstPhaseHistogram.ui \
    src/functionManager/NonlinearInstPhase.ui \
    src/functionManager/PowerSpectrum.ui \
    src/functionManager/Spectrogram.ui \
    src/functionManager/SquareSpectrum.ui \
    src/functionManager/WaterfallPlot.ui \
    src/functionManager/Waveform.ui

RESOURCES += \
    resources/WaveformdiagramopenGL.qrc

DISTFILES += \
    doc/readme.md \
    third_party/qcustomplot/README.md \
    third_party/qcustomplot/GPL.txt
