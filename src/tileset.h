#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <vector>

// A tileset image sliced into a grid of equally sized tiles.
// Tile keys are row-major: key = row * cols + col, from 0 to count() - 1.
// No pixels are copied per tile: a tile is just a source rectangle into one texture.
struct Tileset {
    SDL_Texture* tex = nullptr;
    int imgW = 0, imgH = 0;    // full image size in pixels
    int tileW = 0, tileH = 0;  // size of one tile in pixels
    int cols = 0, rows = 0;    // whole tiles that fit in the image

    std::vector<bool> blank;

    Tileset() = default;
    Tileset(const Tileset&) = delete;
    Tileset& operator=(const Tileset&) = delete;
    ~Tileset() { destroy(); }

    bool loaded() const { return tex != nullptr; }
    
    int count() const { return cols * rows; }

        bool isBlank(int key) const {
        return key >= 0 && key < static_cast<int>(blank.size()) && blank[static_cast<size_t>(key)];
    }

    void destroy();

    // Loads a PNG and slices it. On failure returns false and fills `err` (the previous
    // tileset stays loaded). On success `warn` is non-empty if leftover edge pixels
    // (image size not a multiple of the tile size) are being ignored.
    bool load(SDL_Renderer* renderer, const std::string& path, int tw, int th,
              std::string& err, std::string& warn);

    // Source rectangle of a tile inside the texture.
    SDL_FRect srcRect(int key) const;
};
