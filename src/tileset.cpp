#include "tileset.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"

void Tileset::destroy() {
    if (tex) {
        SDL_DestroyTexture(tex);
        tex = nullptr;
    }
    imgW = imgH = tileW = tileH = cols = rows = 0;
}

bool Tileset::load(SDL_Renderer* renderer, const std::string& path, int tw, int th, std::string& err, std::string& warn) {
    warn.clear();

    size_t fileSize = 0;
    void* file = SDL_LoadFile(path.c_str(), &fileSize);
    if (!file) {
        err = "Cannot read \"" + path + "\" (" + SDL_GetError() + ")";
        return false;
    }

    int w = 0, h = 0, comp = 0;
    unsigned char* pixels = stbi_load_from_memory(static_cast<const unsigned char*>(file), static_cast<int>(fileSize), &w, &h, &comp, 4);
    SDL_free(file);
    if (!pixels) {
        err = "That file isn't a valid PNG image.";
        return false;
    }

    if (tw > w || th > h) {
        stbi_image_free(pixels);
        err = "Tile size " + std::to_string(tw) + "x" + std::to_string(th) +
              " is larger than the image (" + std::to_string(w) + "x" + std::to_string(h) + ").";
        return false;
    }

    const int newCols = w / tw;
    const int newRows = h / th;
    std::vector<bool> newBlank(static_cast<size_t>(newCols * newRows), true);
    for (int r = 0; r < newRows; ++r) {
        for (int c = 0; c < newCols; ++c) {
            bool opaque = false;
            for (int y = 0; y < th && !opaque; ++y) {
                const unsigned char* row = pixels + (static_cast<size_t>(r * th + y) * w + c * tw) * 4;
                for (int x = 0; x < tw; ++x) {
                    if (row[x * 4 + 3] != 0) { opaque = true; break; }
                }
            }
            newBlank[static_cast<size_t>(r * newCols + c)] = !opaque;
        }
    }

    SDL_Texture* t = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
    if (!t) {
        stbi_image_free(pixels);
        err = std::string("Could not create texture: ") + SDL_GetError();
        return false;
    }
    const bool uploaded = SDL_UpdateTexture(t, nullptr, pixels, w * 4);
    stbi_image_free(pixels);
    if (!uploaded) {
        err = std::string("Could not upload texture: ") + SDL_GetError();
        SDL_DestroyTexture(t);
        return false;
    }
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);  // keep pixel art not blurry

    destroy();  // release the previous tileset only once the new one is ready
    tex = t;
    imgW = w;
    imgH = h;
    tileW = tw;
    tileH = th;
    cols = w / tw;
    rows = h / th;
    blank = std::move(newBlank);

    const int leftW = w % tw;
    const int leftH = h % th;
    if (leftW || leftH) {
        warn = "Warning: image size isn't a multiple of the tile size - ignored";
        if (leftW) warn += " " + std::to_string(leftW) + "px (right)";
        if (leftW && leftH) warn += ",";
        if (leftH) warn += " " + std::to_string(leftH) + "px (bottom)";
        warn += ".";
    }
    return true;
}

SDL_FRect Tileset::srcRect(int key) const {
    return SDL_FRect
    {
        static_cast<float>((key % cols) * tileW), static_cast<float>((key / cols) * tileH), static_cast<float>(tileW), static_cast<float>(tileH)
    };
}
