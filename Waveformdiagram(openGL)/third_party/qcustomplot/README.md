# QCustomPlot 2.1.1 (vendored)

The waveform page is implemented with [QCustomPlot](https://www.qcustomplot.com/),
as required by the project README. The library is kept as source so the
application builds with the same MSVC/Qt toolchain as the rest of the project.

- Source archive: `https://www.qcustomplot.com/release/2.1.1/QCustomPlot-source.tar.gz`
- License: GPLv3, see `GPL.txt`

## Local changes

1. `qcustomplot.h`: `QCUSTOMPLOT_USE_OPENGL` is defined by default so every
   translation unit (including the generated moc file) sees the same class
   layout when the OpenGL paint buffer backend is enabled.
2. `qcustomplot.cpp`: MSVC warning C4996 is disabled. The upstream 2.1.1
   release still calls a few APIs that Qt 6.11 deprecated, and the project
   enables `/sdl`, which turns those warnings into errors.
3. `src/functionManager/Waveform.cpp` links `opengl32.lib` explicitly because
   QCustomPlot's OpenGL backend calls `glClear`/`glClearColor` directly.
