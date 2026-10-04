#pragma once

#include <string>
#include <vector>

// The user's map: a w x h grid of tile keys, stored flat (row-major).
// An empty cell holds kEmpty (-1).
struct Tilemap {
    static constexpr int kEmpty = -1;

    int w = 0, h = 0;
    std::vector<int> cells;

    void reset(int width, int height);       // new map, every cell empty
    int get(int x, int y) const;             // kEmpty if out of bounds
    bool set(int x, int y, int key);         // true if the cell actually changed

    // One CSV line per map row, keys separated by commas, empty cells written as -1.
    bool saveCsv(const std::string& path) const;
};
