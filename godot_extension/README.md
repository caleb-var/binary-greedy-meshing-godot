# Minimal Godot C++ GDExtension Menu Demo

This repository is intentionally reduced to a small Godot C++ setup:

- A basic `BinaryMesherGDExtension` C++ class exposed through GDExtension.
- A Godot project with a main menu scene that shows rich update text.
- Debug test buttons that call into the extension and print results.

## Build

```bash
cmake -S . -B build -DGODOT_CPP_PATH=/absolute/path/to/godot-cpp
cmake --build build
```

The compiled extension is placed in `godot_extension/godot/bin`.

## Run

Open `godot_extension/godot/project.godot` in Godot 4.x and run the project.
