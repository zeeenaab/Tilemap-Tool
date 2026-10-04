#include "tilemap.h"
#include <SDL3/SDL.h>

void Tilemap::reset(int width, int height) 
{
    w = width;
    h = height;
    cells.assign(static_cast<size_t>(w) * static_cast<size_t>(h), kEmpty);
}

int Tilemap::get(int x, int y) const 
{
    if (x < 0 || y < 0 || x >= w || y >= h) return kEmpty;
    return cells[static_cast<size_t>(y) * w + x];
}

bool Tilemap::set(int x, int y, int key) 
{
    if (x < 0 || y < 0 || x >= w || y >= h) return false;
    int& cell = cells[static_cast<size_t>(y) * w + x];
    if (cell == key) return false;
    cell = key;
    return true;
}

bool Tilemap::saveCsv(const std::string& path) const 
{
    std::string out;
    out.reserve(cells.size() * 3);
    for (int y = 0; y < h; ++y) 
    {
        for (int x = 0; x < w; ++x) 
        {
            if (x) out += ',';
            out += std::to_string(cells[static_cast<size_t>(y) * w + x]);
        }
        out += '\n';
    }

    SDL_IOStream* io = SDL_IOFromFile(path.c_str(), "wb");
    if (!io) return false;
    const size_t written = SDL_WriteIO(io, out.data(), out.size());
    const bool closed = SDL_CloseIO(io);
    return written == out.size() && closed;
}
