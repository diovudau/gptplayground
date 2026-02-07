# IkiGUI Gain (CLAP)

A simple **stereo gain CLAP plugin** for Linux focused on real-time safety:

- No heap allocation in the audio callback.
- No locks in the audio callback.
- Parameter transfer via atomics.
- Smoothed gain changes to avoid zipper noise.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The built plugin is:

```text
build/ikigui_gain.so
```

## Real-time notes

The processing callback only:

- Reads atomics.
- Applies event values.
- Performs simple multiply operations.

No system calls, mutexes, logging, dynamic allocation, or blocking operations are performed on the audio thread.

## GUI

The plugin exposes a minimal CLAP GUI extension backed by an embedded `IkiGUI` abstraction in `include/ikigui/ikigui.hpp`.
