# Zee's Mini Tilemap Tool

A small and simple tilemap editor (C++17, SDL3).
I use this primarily for testing purposes.
I can test tilesets as I'm creating them, quickly create tilemaps, and export corresponding .csv files with barely any prior setup.

Head to Releases if you'd like to grab the executable directly.

1. Load a tileset PNG and enter the tile size in pixels.
2. The tileset is sliced into tiles numbered left-to-right, top-to-bottom (0, 1, 2, ...).
3. Paint tiles onto a map of the size you choose. Empty cells are allowed.
4. Export a CSV where each value is a tile key and empty cells are `-1`. The CSV layout will mirror the map, one line per map row.

Tiles must be packed edge-to-edge (no spacing/margins). If the image size isn't a multiple of the tile size, the leftover edge pixels are ignored and a warning is shown.

## Controls (editor)

- Select tile : Click in the palette (left) 
- Paint / erase : Left / right mouse button on the map 
- Pan : Middle-drag, Space + left-drag, or arrow keys 
- Zoom : Mouse wheel over the map 
- Scroll palette : Mouse wheel over the palette, pinch/widen on touchpad
- Fit map to window : `F` 
- Show tile numbers/keys : `K` 
- Export CSV : `Ctrl+S` (`Cmd+S` on macOS)
- Back to setup : `Esc` (asks first if there are unsaved changes)

## Build

Requirements: CMake 3.16+, a C++17 compiler, git.

```
git clone https://github.com/zeeenaab/Tilemap-Tool.git
cd tilemap-tool
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

With Visual Studio generators the exe ends up in `build/Release/`; otherwise in `build/`.
SDL3 is compiled from `third_party/SDL` and linked statically, so the result is a single
executable (`TilemapTool` / `TilemapTool.exe`).
