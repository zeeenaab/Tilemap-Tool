#pragma once

#include <SDL3/SDL.h>

#include <string>

#include "tilemap.h"
#include "tileset.h"

enum class MsgKind { Info, Ok, Warn, Error };

// The whole application: a setup screen (pick PNG, tile size, map size)
// and an editor screen (tile palette + map canvas + status bar).
class App {
public:
    App(SDL_Window* window, SDL_Renderer* renderer);

    void handleEvent(SDL_Event& e);
    void render();

    bool running = true;

private:
    enum class Screen { Setup, Editor };

    struct Field {
        const char* label;
        std::string text;
        bool numeric;
        size_t maxLen;
    };

    static constexpr int kNumFields = 5;  // 0 = path, 1/2 = tile w/h, 3/4 = map w/h
    static constexpr int kNoCell = -1000000;

    // ---- layout / helpers ----
    void updateLayout();
    void setSetupMsg(const std::string& msg, MsgKind kind);
    void setNotice(const std::string& msg, MsgKind kind);
    void fill(const SDL_FRect& r, SDL_Color c);
    void stroke(const SDL_FRect& r, SDL_Color c);
    void text(float x, float y, const std::string& s, float scale, SDL_Color c);
    void drawWrapped(float x, float y, float maxW, const std::string& s, float scale, SDL_Color c);
    void button(const SDL_FRect& r, const char* label, float scale = 2.f);

    // ---- events ----
    void onKey(const SDL_KeyboardEvent& k);
    void onText(const char* txt);
    void onMouseDown(const SDL_MouseButtonEvent& b);
    void onMouseUp(const SDL_MouseButtonEvent& b);
    void onMouseMove(const SDL_MouseMotionEvent& m);
    void onWheel(const SDL_MouseWheelEvent& w);
    void onUser(Sint32 code, void* data);

    // ---- setup screen ----
    void backspace();
    void browse();
    void start();
    void drawSetup();

    // ---- editor screen ----
    float zoom() const;
    void fitMap();
    void zoomBy(int steps, float mx, float my);
    bool cellAt(float mx, float my, int& cx, int& cy) const;  // true if inside the map
    SDL_FRect cellRect(int cx, int cy) const;
    int paletteKeyAt(float mx, float my) const;
    SDL_FRect paletteCellRect(int key) const;
    void selectFromPalette();
    void paintAt(float mx, float my);
    void paintLine(int x0, int y0, int x1, int y1, int key);
    void exportCsv();
    void saveTo(std::string path);
    void drawMap();
    void drawPalette();
    void drawStatus();
    void keyLabel(float x, float y, const std::string& s);

    // ---- shared state ----
    SDL_Window* window_;
    SDL_Renderer* renderer_;
    Screen screen_ = Screen::Setup;
    float W_ = 0, H_ = 0;
    float mouseX_ = 0, mouseY_ = 0;
    bool dialogOpen_ = false;
    Tileset tileset_;
    Tilemap tilemap_;

    // ---- setup state ----
    Field fields_[kNumFields];
    int focus_ = 0;
    float colW_ = 0;
    SDL_FRect fieldRect_[kNumFields] = {};
    SDL_FRect browseRect_ = {};
    SDL_FRect startRect_ = {};
    float msgY_ = 0;
    std::string setupMsg_;
    MsgKind setupKind_ = MsgKind::Info;

    // ---- editor state ----
    SDL_FRect palRect_ = {}, viewRect_ = {}, statusRect_ = {};
    int selected_ = 0;
    float palZoom_ = 1.f, palScroll_ = 0.f;
    int zoomIdx_ = 3;
    float zoomAccum_ = 0.f;
    float panX_ = 0.f, panY_ = 0.f;  // map origin, relative to the top-left of viewRect_
    int paintBtn_ = 0;               // SDL_BUTTON_LEFT / SDL_BUTTON_RIGHT while painting
    int panButton_ = 0;              // button that started a pan, 0 = not panning
    bool palDrag_ = false;
    int lastCX_ = kNoCell, lastCY_ = kNoCell;
    bool showKeys_ = false;
    bool dirty_ = false;
    bool confirmLeave_ = false;
    std::string notice_;
    MsgKind noticeKind_ = MsgKind::Info;
    Uint64 noticeUntil_ = 0;
};
