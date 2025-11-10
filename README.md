# rast

Software rasterizer written in C. Uses a right-handed coordinate system: +X right, +Y down, +Z in.

## Building

Replace `$CONFIG` with your build configuration. `Debug` or `Release`.

```bash
# build
cmake . -B build -DCMAKE_BUILD_TYPE=$CONFIG
cmake --build build -j 8

# run
build/demo
```
