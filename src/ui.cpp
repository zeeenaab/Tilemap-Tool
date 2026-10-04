#include "ui.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace {

// ---------- constants ----------
constexpr SDL_Color kBg{34, 34, 34, 255};
constexpr SDL_Color kPanel{34, 34, 34, 255};
constexpr SDL_Color kPanel2{68, 68, 68, 255};
constexpr SDL_Color kText{253, 214, 225, 255};
constexpr SDL_Color kDim{253, 214, 255, 255};
constexpr SDL_Color kAccent{253, 214, 255, 255};
constexpr SDL_Color kOk{120, 220, 140, 255};
constexpr SDL_Color kWarn{255, 200, 60, 255};
constexpr SDL_Color kErr{255, 105, 105, 255};

constexpr float kGlyph = 8.0f;      // size of one SDL_RenderDebugText glyph
constexpr float kStatusH = 30.0f;   // status bar height
constexpr float kPad = 8.0f;        // palette inner padding
constexpr int kMaxMapSide = 2048;
constexpr Uint64 kNoticeMs = 10000;

constexpr float kZoomLevels[] = {0.125f, 0.25f, 0.5f, 1.f, 2.f, 3.f, 4.f, 6.f, 8.f};
constexpr int kNumZoom = static_cast<int>(sizeof(kZoomLevels) / sizeof(kZoomLevels[0]));

// Codes carried by the SDL_EVENT_USER events that the file-dialog callbacks push.
enum DialogCode : Sint32 { kOpenPicked = 1, kOpenFailed, kSavePicked, kSaveFailed, kDialogCancelled };

// ---------- small helpers ----------
bool inRect(float x, float y, const SDL_FRect& r) {
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

SDL_Rect toRect(const SDL_FRect& r) {
    return SDL_Rect{static_cast<int>(r.x), static_cast<int>(r.y),
                    static_cast<int>(r.w), static_cast<int>(r.h)};
}

SDL_Color colorFor(MsgKind k) 
{
    switch (k) {
        case MsgKind::Ok: return kOk;
        case MsgKind::Warn: return kWarn;
        case MsgKind::Error: return kErr;
        default: return kText;
    }
}

int digitsOf(int n) {
    int d = 1;
    while (n >= 10) { n /= 10; ++d; }
    return d;
}

int parseInt(const std::string& s) {
    long v = 0;
    for (char c : s) {
        v = v * 10 + (c - '0');
        if (v > 1000000) break;
    }
    return static_cast<int>(v);
}

// Trims spaces and surrounding quotes ("Copy as path" on Windows adds quotes).
std::string cleanPath(std::string s) {
    s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
    s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '"')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '"')) --e;
    return s.substr(b, e - b);
}

// Last `maxChars` bytes of s, without starting in the middle of a UTF-8 sequence.
std::string utf8Tail(const std::string& s, size_t maxChars) {
    if (s.size() <= maxChars) return s;
    size_t start = s.size() - maxChars;
    while (start < s.size() && (static_cast<unsigned char>(s[start]) & 0xC0) == 0x80) ++start;
    return s.substr(start);
}

// File-dialog callbacks may run on another thread, so they only push an event;
// the main loop does the actual work (see App::onUser).
void pushDialogResult(Sint32 pickedCode, Sint32 failedCode, const char* const* files) {
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = SDL_EVENT_USER;
    if (!files) {
        ev.user.code = failedCode;             // dialog error / unsupported
    } else if (!*files) {
        ev.user.code = kDialogCancelled;       // user cancelled
    } else {
        ev.user.code = pickedCode;
        ev.user.data1 = SDL_strdup(*files);    // freed in App::onUser
    }
    SDL_PushEvent(&ev);
}

void SDLCALL openCallback(void*, const char* const* files, int) {
    pushDialogResult(kOpenPicked, kOpenFailed, files);
}

void SDLCALL saveCallback(void*, const char* const* files, int) {
    pushDialogResult(kSavePicked, kSaveFailed, files);
}

}  // namespace

// =====================================================================
//  Construction, layout, drawing helpers
// =====================================================================

App::App(SDL_Window* window, SDL_Renderer* renderer) : window_(window), renderer_(renderer) {
    fields_[0] = {"Tileset PNG (path, drop a file, or Browse)", "", false, 1024};
    fields_[1] = {"Tile width (px)", "16", true, 4};
    fields_[2] = {"Tile height (px)", "16", true, 4};
    fields_[3] = {"Map width (tiles)", "32", true, 4};
    fields_[4] = {"Map height (tiles)", "32", true, 4};

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_StartTextInput(window_);
    updateLayout();
}

void App::updateLayout() {
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(renderer_, &w, &h);
    W_ = static_cast<float>(w);
    H_ = static_cast<float>(h);

    // Setup screen
    colW_ = std::min(W_ - 40.f, 720.f);
    const float x0 = (W_ - colW_) * 0.5f;
    const float half = (colW_ - 20.f) * 0.5f;
    float y = 140.f;
    fieldRect_[0] = {x0, y, colW_ - 130.f, 32.f};
    browseRect_ = {x0 + colW_ - 120.f, y, 120.f, 32.f};
    y += 84.f;
    fieldRect_[1] = {x0, y, half, 32.f};
    fieldRect_[2] = {x0 + half + 20.f, y, half, 32.f};
    y += 84.f;
    fieldRect_[3] = {x0, y, half, 32.f};
    fieldRect_[4] = {x0 + half + 20.f, y, half, 32.f};
    y += 84.f;
    startRect_ = {x0, y, 180.f, 40.f};
    msgY_ = y + 64.f;

    // Editor screen
    const float palW = std::clamp(W_ * 0.28f, 200.f, 340.f);
    palRect_ = {0.f, 0.f, palW, H_ - kStatusH};
    viewRect_ = {palW, 0.f, W_ - palW, H_ - kStatusH};
    statusRect_ = {0.f, H_ - kStatusH, W_, kStatusH};

    if (tileset_.loaded()) {
        // Palette zoom: integer when it can be (crisp), fractional if the tileset is wider than the panel.
        const float z = (palRect_.w - 2.f * kPad) / static_cast<float>(tileset_.cols * tileset_.tileW);
        palZoom_ = z >= 1.f ? std::min(std::floor(z), 8.f) : z;
        const float contentH = tileset_.rows * tileset_.tileH * palZoom_;
        const float maxScroll = std::max(0.f, contentH - (palRect_.h - 2.f * kPad));
        palScroll_ = std::clamp(palScroll_, 0.f, maxScroll);
    }
}

void App::setSetupMsg(const std::string& msg, MsgKind kind) {
    setupMsg_ = msg;
    setupKind_ = kind;
}

void App::setNotice(const std::string& msg, MsgKind kind) {
    notice_ = msg;
    noticeKind_ = kind;
    noticeUntil_ = SDL_GetTicks() + kNoticeMs;
}

void App::fill(const SDL_FRect& r, SDL_Color c) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(renderer_, &r);
}

void App::stroke(const SDL_FRect& r, SDL_Color c) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    SDL_RenderRect(renderer_, &r);
}

// Built-in 8x8 debug font, optionally magnified. Only call with scale != 1 while
// no clip rectangle is active (the clip rect would be scaled too).
void App::text(float x, float y, const std::string& s, float scale, SDL_Color c) 
{
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    if (scale != 1.f) SDL_SetRenderScale(renderer_, scale, scale);
    SDL_RenderDebugText(renderer_, x / scale, y / scale, s.c_str());
    if (scale != 1.f) SDL_SetRenderScale(renderer_, 1.f, 1.f);
}

void App::drawWrapped(float x, float y, float maxW, const std::string& s, float scale, SDL_Color c) 
{
    const size_t perLine = static_cast<size_t>(std::max(1.f, std::floor(maxW / (kGlyph * scale))));
    for (size_t i = 0; i < s.size(); i += perLine) {
        text(x, y, s.substr(i, perLine), scale, c);
        y += kGlyph * scale + 6.f;
    }
}

void App::button(const SDL_FRect& r, const char* label, float scale) 
{
    const bool hot = inRect(mouseX_, mouseY_, r);
    fill(r, hot ? kAccent : kPanel2);
    stroke(r, kAccent);
    const float tw = static_cast<float>(std::strlen(label)) * kGlyph * scale;
    text(r.x + (r.w - tw) * 0.5f, r.y + (r.h - kGlyph * scale) * 0.5f, label, scale, hot ? kBg : kText);
}

// Small key number with a dark shadow so it's readable on any tile (scale 1: safe under a clip rect).
void App::keyLabel(float x, float y, const std::string& s) {
    text(x + 1.f, y + 1.f, s, 1.f, SDL_Color{0, 0, 0, 255});
    text(x, y, s, 1.f, SDL_Color{255, 255, 255, 255});
}

// =====================================================================
//  Main event dispatch
// =====================================================================

void App::handleEvent(SDL_Event& e) {
    SDL_ConvertEventToRenderCoordinates(renderer_, &e);  // window coords -> render coords
    updateLayout();

    switch (e.type) {
        case SDL_EVENT_QUIT: running = false; break;
        case SDL_EVENT_USER: onUser(e.user.code, e.user.data1); break;
        case SDL_EVENT_DROP_FILE:
            if (screen_ == Screen::Setup && e.drop.data) 
            {
                fields_[0].text = cleanPath(e.drop.data);
                focus_ = 0;
                setupMsg_.clear();
            }
            break;
        case SDL_EVENT_TEXT_INPUT: onText(e.text.text); break;
        case SDL_EVENT_KEY_DOWN: onKey(e.key); break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN: onMouseDown(e.button); break;
        case SDL_EVENT_MOUSE_BUTTON_UP: onMouseUp(e.button); break;
        case SDL_EVENT_MOUSE_MOTION: onMouseMove(e.motion); break;
        case SDL_EVENT_MOUSE_WHEEL: onWheel(e.wheel); break;
        default: break;
    }
}

void App::onUser(Sint32 code, void* data) {
    dialogOpen_ = false;
    std::string s;
    if (data) {
        s = static_cast<const char*>(data);
        SDL_free(data);
    }
    switch (code) {
        case kOpenPicked:
            fields_[0].text = cleanPath(s);
            focus_ = 0;
            setupMsg_.clear();
            break;
        case kOpenFailed:
            setSetupMsg("The file dialog isn't available here. Type or paste the path, "
                        "or drop the PNG onto this window.", MsgKind::Warn);
            break;
        case kSavePicked: saveTo(s); break;
        case kSaveFailed: saveTo("tilemap.csv"); break;  // no dialog: save next to the program
        default: break;                                  // cancelled
    }
}

void App::onText(const char* txt) {
    if (screen_ != Screen::Setup || !txt) return;
    Field& f = fields_[focus_];
    for (const char* p = txt; *p; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        if (c < 0x20) continue;                          // no control characters
        if (f.numeric && (c < '0' || c > '9')) continue; // digits only in numeric fields
        if (f.text.size() >= f.maxLen) break;
        f.text.push_back(static_cast<char>(c));
    }
    setupMsg_.clear();
}

void App::backspace() {
    std::string& s = fields_[focus_].text;
    while (!s.empty()) {
        const unsigned char c = static_cast<unsigned char>(s.back());
        s.pop_back();
        if ((c & 0xC0) != 0x80) break;  // removed a whole UTF-8 character
    }
    setupMsg_.clear();
}

void App::onKey(const SDL_KeyboardEvent& k) {
    const bool mod = (k.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;  // Ctrl, or Cmd on macOS

    if (screen_ == Screen::Setup) {
        switch (k.key) {
            case SDLK_TAB:
                focus_ = (focus_ + ((k.mod & SDL_KMOD_SHIFT) ? kNumFields - 1 : 1)) % kNumFields;
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER: start(); break;
            case SDLK_BACKSPACE: backspace(); break;
            case SDLK_V:
                if (mod && SDL_HasClipboardText()) {
                    char* clip = SDL_GetClipboardText();
                    if (clip) {
                        onText(clip);
                        SDL_free(clip);
                    }
                }
                break;
            default: break;
        }
        return;
    }

    // Editor
    if (k.key != SDLK_ESCAPE) confirmLeave_ = false;
    switch (k.key) {
        case SDLK_S:
            if (mod && !k.repeat) exportCsv();
            break;
        case SDLK_K:
            if (!k.repeat) showKeys_ = !showKeys_;
            break;
        case SDLK_F: fitMap(); break;
        case SDLK_LEFT: panX_ += 64.f; break;
        case SDLK_RIGHT: panX_ -= 64.f; break;
        case SDLK_UP: panY_ += 64.f; break;
        case SDLK_DOWN: panY_ -= 64.f; break;
        case SDLK_ESCAPE:
            if (dirty_ && !confirmLeave_) {
                confirmLeave_ = true;
                setNotice("Unsaved changes! Press Esc again to discard them, or Ctrl+S to export.",
                          MsgKind::Warn);
            } else {
                screen_ = Screen::Setup;
                confirmLeave_ = false;
                paintBtn_ = panButton_ = 0;
                palDrag_ = false;
                setupMsg_.clear();
                SDL_StartTextInput(window_);
            }
            break;
        default: break;
    }
}

void App::onMouseDown(const SDL_MouseButtonEvent& b) {
    mouseX_ = b.x;
    mouseY_ = b.y;

    if (screen_ == Screen::Setup) {
        if (b.button != SDL_BUTTON_LEFT) return;
        for (int i = 0; i < kNumFields; ++i) {
            if (inRect(b.x, b.y, fieldRect_[i])) focus_ = i;
        }
        if (inRect(b.x, b.y, browseRect_)) browse();
        else if (inRect(b.x, b.y, startRect_)) start();
        return;
    }

    confirmLeave_ = false;
    if (inRect(b.x, b.y, palRect_)) {
        if (b.button == SDL_BUTTON_LEFT) {
            palDrag_ = true;
            selectFromPalette();
        }
    } else if (inRect(b.x, b.y, viewRect_)) {
        const bool* keys = SDL_GetKeyboardState(nullptr);
        if (b.button == SDL_BUTTON_MIDDLE ||
            (b.button == SDL_BUTTON_LEFT && keys[SDL_SCANCODE_SPACE])) {
            panButton_ = b.button;
        } else if (b.button == SDL_BUTTON_LEFT || b.button == SDL_BUTTON_RIGHT) {
            paintBtn_ = b.button;
            lastCX_ = lastCY_ = kNoCell;
            paintAt(b.x, b.y);
        }
    }
}

void App::onMouseUp(const SDL_MouseButtonEvent& b) {
    if (b.button == SDL_BUTTON_LEFT) palDrag_ = false;
    if (b.button == panButton_) panButton_ = 0;
    if (b.button == paintBtn_) {
        paintBtn_ = 0;
        lastCX_ = lastCY_ = kNoCell;
    }
}

void App::onMouseMove(const SDL_MouseMotionEvent& m) {
    const float dx = m.x - mouseX_;
    const float dy = m.y - mouseY_;
    mouseX_ = m.x;
    mouseY_ = m.y;
    if (screen_ != Screen::Editor) return;

    if (panButton_) {
        panX_ += dx;
        panY_ += dy;
    } else if (palDrag_) {
        selectFromPalette();
    } else if (paintBtn_) {
        paintAt(m.x, m.y);
    }
}

void App::onWheel(const SDL_MouseWheelEvent& w) {
    if (screen_ != Screen::Editor) return;
    if (inRect(w.mouse_x, w.mouse_y, palRect_)) {
        palScroll_ -= w.y * 48.f;  // clamped in updateLayout()
        updateLayout();
    } else if (inRect(w.mouse_x, w.mouse_y, viewRect_)) {
        zoomAccum_ += w.y;  // touchpads send fractional wheel values
        const int steps = static_cast<int>(zoomAccum_);
        zoomAccum_ -= static_cast<float>(steps);
        if (steps != 0) zoomBy(steps, w.mouse_x, w.mouse_y);
    }
}

// =====================================================================
//  Setup screen
// =====================================================================

void App::browse() {
    if (dialogOpen_) return;
    dialogOpen_ = true;
    static const SDL_DialogFileFilter filters[] = {{"PNG images", "png"}};
    SDL_ShowOpenFileDialog(openCallback, nullptr, window_, filters, 1, nullptr, false);
}

void App::start() {
    const std::string path = cleanPath(fields_[0].text);
    const int tw = parseInt(fields_[1].text);
    const int th = parseInt(fields_[2].text);
    const int mw = parseInt(fields_[3].text);
    const int mh = parseInt(fields_[4].text);

    if (path.empty()) {
        setSetupMsg("Choose a tileset PNG first.", MsgKind::Error);
        return;
    }
    if (tw < 1 || th < 1) {
        setSetupMsg("Tile width and height must be at least 1 pixel.", MsgKind::Error);
        return;
    }
    if (mw < 1 || mh < 1 || mw > kMaxMapSide || mh > kMaxMapSide) {
        setSetupMsg("Map width and height must be between 1 and " + std::to_string(kMaxMapSide) + ".",
                    MsgKind::Error);
        return;
    }

    std::string err, warn;
    if (!tileset_.load(renderer_, path, tw, th, err, warn)) {
        setSetupMsg(err, MsgKind::Error);
        return;
    }

    tilemap_.reset(mw, mh);
    selected_ = 0;
    palScroll_ = 0.f;
    dirty_ = false;
    confirmLeave_ = false;
    paintBtn_ = panButton_ = 0;
    palDrag_ = false;
    screen_ = Screen::Editor;
    SDL_StopTextInput(window_);
    updateLayout();
    fitMap();

    if (!warn.empty()) {
        SDL_Log("%s", warn.c_str());
        setNotice(warn, MsgKind::Warn);
    } else {
        noticeUntil_ = 0;
    }
}

void App::drawSetup() {
    const float x0 = (W_ - colW_) * 0.5f;
    text(x0, 34.f, "Zee's Mini Tilemap Tool", 4.f, kText);
    text(x0, 80.f, "Enter your tileset, paint a tilemap, and export it as CSV!", 1.5f, kText);
    text(980.f, H_ - 28.f, "Version 1.0", 1.f, kText);

    for (int i = 0; i < kNumFields; ++i) {
        const SDL_FRect& r = fieldRect_[i];
        text(r.x, r.y - 24.f, fields_[i].label, 2.f, kText);
        fill(r, kPanel2);
        stroke(r, i == focus_ ? kAccent : kDim);

        std::string shown = fields_[i].text;
        if (i == focus_ && (SDL_GetTicks() / 500) % 2 == 0) shown += '_';  // blinking caret
        const size_t maxChars =
            static_cast<size_t>(std::max(1.f, std::floor((r.w - 16.f) / (kGlyph * 2.f))));
        text(r.x + 8.f, r.y + 8.f, utf8Tail(shown, maxChars), 2.f, kText);
    }

    button(browseRect_, "Browse...", 1.5f);
    button(startRect_, "Start");

    if (!setupMsg_.empty()) {
        drawWrapped(fieldRect_[0].x, msgY_, colW_, setupMsg_, 2.f, colorFor(setupKind_));
    }
    text(fieldRect_[0].x, H_ - 28.f,
         "Tab: next field   Enter: start   Ctrl+V: paste path   or, drop a PNG onto the window", 1.f, kText);
}

// =====================================================================
//  Editor: view math
// =====================================================================

float App::zoom() const { return kZoomLevels[zoomIdx_]; }

// Pick the largest zoom at which the whole map fits, then center it.
void App::fitMap() {
    const float availW = viewRect_.w - 24.f;
    const float availH = viewRect_.h - 24.f;
    zoomIdx_ = 0;
    for (int i = 0; i < kNumZoom; ++i) {
        if (tilemap_.w * tileset_.tileW * kZoomLevels[i] <= availW &&
            tilemap_.h * tileset_.tileH * kZoomLevels[i] <= availH) {
            zoomIdx_ = i;
        }
    }
    const float z = zoom();
    panX_ = std::max(12.f, (viewRect_.w - tilemap_.w * tileset_.tileW * z) * 0.5f);
    panY_ = std::max(12.f, (viewRect_.h - tilemap_.h * tileset_.tileH * z) * 0.5f);
}

// Zoom in/out by `steps` levels, keeping the map point under (mx, my) fixed.
void App::zoomBy(int steps, float mx, float my) {
    const int next = std::clamp(zoomIdx_ + steps, 0, kNumZoom - 1);
    if (next == zoomIdx_) return;
    const float oldZ = zoom();
    const float wx = (mx - viewRect_.x - panX_) / oldZ;  // map-space point under the cursor
    const float wy = (my - viewRect_.y - panY_) / oldZ;
    zoomIdx_ = next;
    panX_ = mx - viewRect_.x - wx * zoom();
    panY_ = my - viewRect_.y - wy * zoom();
}

bool App::cellAt(float mx, float my, int& cx, int& cy) const {
    const float z = zoom();
    cx = static_cast<int>(std::floor((mx - viewRect_.x - panX_) / (tileset_.tileW * z)));
    cy = static_cast<int>(std::floor((my - viewRect_.y - panY_) / (tileset_.tileH * z)));
    return cx >= 0 && cy >= 0 && cx < tilemap_.w && cy < tilemap_.h;
}

// Screen rectangle of a map cell. Edges are floored so neighbouring cells never leave gaps.
SDL_FRect App::cellRect(int cx, int cy) const {
    const float z = zoom();
    const float cw = tileset_.tileW * z, ch = tileset_.tileH * z;
    const float ox = viewRect_.x + panX_, oy = viewRect_.y + panY_;
    const float x0 = std::floor(ox + cx * cw), x1 = std::floor(ox + (cx + 1) * cw);
    const float y0 = std::floor(oy + cy * ch), y1 = std::floor(oy + (cy + 1) * ch);
    return SDL_FRect{x0, y0, x1 - x0, y1 - y0};
}

SDL_FRect App::paletteCellRect(int key) const {
    const float cw = tileset_.tileW * palZoom_, ch = tileset_.tileH * palZoom_;
    const int col = key % tileset_.cols, row = key / tileset_.cols;
    return SDL_FRect{palRect_.x + kPad + col * cw, palRect_.y + kPad - palScroll_ + row * ch, cw, ch};
}

int App::paletteKeyAt(float mx, float my) const {
    if (!tileset_.loaded() || !inRect(mx, my, palRect_)) return -1;
    const float cw = tileset_.tileW * palZoom_, ch = tileset_.tileH * palZoom_;
    const int col = static_cast<int>(std::floor((mx - palRect_.x - kPad) / cw));
    const int row = static_cast<int>(std::floor((my - palRect_.y - kPad + palScroll_) / ch));
    if (col < 0 || col >= tileset_.cols || row < 0 || row >= tileset_.rows) return -1;
    return row * tileset_.cols + col;
}

void App::selectFromPalette() {
    const int key = paletteKeyAt(mouseX_, mouseY_);
    if (key >= 0) selected_ = key;
}

// =====================================================================
//  Editor: painting and export
// =====================================================================

void App::paintAt(float mx, float my) {
    if (!inRect(mx, my, viewRect_)) {  // dragged outside the canvas: stop the stroke
        lastCX_ = lastCY_ = kNoCell;
        return;
    }
    int cx = 0, cy = 0;
    cellAt(mx, my, cx, cy);
    const int key = (paintBtn_ == SDL_BUTTON_LEFT && !tileset_.isBlank(selected_)) ? selected_ : Tilemap::kEmpty;
    if (lastCX_ == kNoCell) paintLine(cx, cy, cx, cy, key);
    else paintLine(lastCX_, lastCY_, cx, cy, key);  // fill gaps when the mouse moves fast
    lastCX_ = cx;
    lastCY_ = cy;
}

// Bresenham line between two cells (out-of-map cells are ignored by Tilemap::set).
void App::paintLine(int x0, int y0, int x1, int y1, int key) {
    const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        if (tilemap_.set(x0, y0, key)) dirty_ = true;
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void App::exportCsv() {
    if (dialogOpen_) return;
    dialogOpen_ = true;
    static const SDL_DialogFileFilter filters[] = {{"CSV files", "csv"}};
    SDL_ShowSaveFileDialog(saveCallback, nullptr, window_, filters, 1, "tilemap.csv");
}

void App::saveTo(std::string path) {
    // Add .csv if the chosen filename has no extension at all.
    const size_t slash = path.find_last_of("/\\");
    const size_t dot = path.find('.', slash == std::string::npos ? 0 : slash + 1);
    if (dot == std::string::npos) path += ".csv";

    if (tilemap_.saveCsv(path)) {
        dirty_ = false;
        setNotice("Saved " + std::to_string(tilemap_.w) + "x" + std::to_string(tilemap_.h) +
                  " map to " + path, MsgKind::Ok);
    } else {
        setNotice("Could not write " + path + ": " + SDL_GetError(), MsgKind::Error);
    }
}

// =====================================================================
//  Editor: drawing
// =====================================================================

void App::render() {
    updateLayout();
    SDL_SetRenderDrawColor(renderer_, kBg.r, kBg.g, kBg.b, kBg.a);
    SDL_RenderClear(renderer_);
    if (screen_ == Screen::Setup) {
        drawSetup();
    } else {
        drawMap();
        drawPalette();
        drawStatus();
    }
    SDL_RenderPresent(renderer_);
}

void App::drawMap() {
    const SDL_Rect clip = toRect(viewRect_);
    SDL_SetRenderClipRect(renderer_, &clip);
    fill(viewRect_, kBg);

    const float z = zoom();
    const float cw = tileset_.tileW * z, ch = tileset_.tileH * z;
    const float ox = viewRect_.x + panX_, oy = viewRect_.y + panY_;
    const float mapW = tilemap_.w * cw, mapH = tilemap_.h * ch;
    fill(SDL_FRect{ox, oy, mapW, mapH}, kPanel);

    // Only visit cells that are actually on screen.
    const int cx0 = std::max(0, static_cast<int>(std::floor(-panX_ / cw)));
    const int cy0 = std::max(0, static_cast<int>(std::floor(-panY_ / ch)));
    const int cx1 = std::min(tilemap_.w - 1, static_cast<int>(std::floor((viewRect_.w - panX_) / cw)));
    const int cy1 = std::min(tilemap_.h - 1, static_cast<int>(std::floor((viewRect_.h - panY_) / ch)));

    for (int cy = cy0; cy <= cy1; ++cy) {
        for (int cx = cx0; cx <= cx1; ++cx) {
            const int key = tilemap_.get(cx, cy);
            if (key < 0 || key >= tileset_.count()) continue;
            const SDL_FRect src = tileset_.srcRect(key);
            const SDL_FRect dst = cellRect(cx, cy);
            SDL_RenderTexture(renderer_, tileset_.tex, &src, &dst);
        }
    }

    // Grid lines (skipped when cells are too small to read).
    if (cw >= 4.f && ch >= 4.f) {
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 34);
        const float top = std::max(oy, viewRect_.y);
        const float bottom = std::min(oy + mapH, viewRect_.y + viewRect_.h);
        const float left = std::max(ox, viewRect_.x);
        const float right = std::min(ox + mapW, viewRect_.x + viewRect_.w);
        for (int cx = cx0; cx <= cx1 + 1; ++cx) {
            const float x = std::floor(ox + cx * cw);
            SDL_RenderLine(renderer_, x, top, x, bottom);
        }
        for (int cy = cy0; cy <= cy1 + 1; ++cy) {
            const float y = std::floor(oy + cy * ch);
            SDL_RenderLine(renderer_, left, y, right, y);
        }
    }
    stroke(SDL_FRect{ox - 1.f, oy - 1.f, mapW + 2.f, mapH + 2.f}, SDL_Color{255, 255, 255, 90});

    // Optional tile-key numbers.
    if (showKeys_) {
        const float need = digitsOf(std::max(0, tileset_.count() - 1)) * kGlyph + 4.f;
        if (cw >= need && ch >= kGlyph + 4.f) {
            for (int cy = cy0; cy <= cy1; ++cy) {
                for (int cx = cx0; cx <= cx1; ++cx) {
                    const int key = tilemap_.get(cx, cy);
                    if (key < 0) continue;
                    const SDL_FRect d = cellRect(cx, cy);
                    keyLabel(d.x + 2.f, d.y + 2.f, std::to_string(key));
                }
            }
        }
    }

    // Ghost preview of the selected tile under the cursor.
    if (inRect(mouseX_, mouseY_, viewRect_) && panButton_ == 0) {
        int cx = 0, cy = 0;
        if (cellAt(mouseX_, mouseY_, cx, cy)) {
            const SDL_FRect d = cellRect(cx, cy);
            if (paintBtn_ != SDL_BUTTON_RIGHT) {
                const SDL_FRect s = tileset_.srcRect(selected_);
                SDL_SetTextureAlphaMod(tileset_.tex, 150);
                SDL_RenderTexture(renderer_, tileset_.tex, &s, &d);
                SDL_SetTextureAlphaMod(tileset_.tex, 255);
            }
            stroke(d, kWarn);
        }
    }

    SDL_SetRenderClipRect(renderer_, nullptr);
}

void App::drawPalette() {
    const SDL_Rect clip = toRect(palRect_);
    SDL_SetRenderClipRect(renderer_, &clip);
    fill(palRect_, kPanel);

    const float cw = tileset_.tileW * palZoom_, ch = tileset_.tileH * palZoom_;
    const SDL_FRect dst{palRect_.x + kPad, palRect_.y + kPad - palScroll_,
                        tileset_.cols * cw, tileset_.rows * ch};
    const SDL_FRect src{0.f, 0.f, static_cast<float>(tileset_.cols * tileset_.tileW),
                        static_cast<float>(tileset_.rows * tileset_.tileH)};
    fill(dst, kBg);
    SDL_RenderTexture(renderer_, tileset_.tex, &src, &dst);

    // Tile-key numbers on visible rows only.
    if (showKeys_) {
        const float need = digitsOf(std::max(0, tileset_.count() - 1)) * kGlyph + 3.f;
        if (cw >= need && ch >= kGlyph + 3.f) {
            const int r0 = std::max(0, static_cast<int>(std::floor(palScroll_ / ch)));
            const int r1 = std::min(tileset_.rows - 1,
                                    static_cast<int>(std::floor((palScroll_ + palRect_.h) / ch)));
            for (int r = r0; r <= r1; ++r) {
                for (int c = 0; c < tileset_.cols; ++c) {
                    const int key = r * tileset_.cols + c;
                    const SDL_FRect d = paletteCellRect(key);
                    keyLabel(d.x + 2.f, d.y + 2.f, std::to_string(key));
                }
            }
        }
    }

    // Hover and selection outlines.
    const int hover = paletteKeyAt(mouseX_, mouseY_);
    if (hover >= 0 && hover != selected_) stroke(paletteCellRect(hover), kDim);
    const SDL_FRect sel = paletteCellRect(selected_);
    stroke(sel, kWarn);
    if (sel.w > 4.f && sel.h > 4.f) {
        stroke(SDL_FRect{sel.x + 1.f, sel.y + 1.f, sel.w - 2.f, sel.h - 2.f}, kWarn);
    }

    SDL_SetRenderClipRect(renderer_, nullptr);
    fill(SDL_FRect{palRect_.x + palRect_.w - 1.f, 0.f, 1.f, palRect_.h}, kPanel2);
}

void App::drawStatus() {
    fill(statusRect_, kPanel);
    fill(SDL_FRect{0.f, statusRect_.y, W_, 1.f}, kPanel2);

    std::string line1;
    SDL_Color c1 = kText;
    if (SDL_GetTicks() < noticeUntil_) {
        line1 = notice_;
        c1 = colorFor(noticeKind_);
    } else {
        int cx = 0, cy = 0;
        const bool inMap = inRect(mouseX_, mouseY_, viewRect_) && cellAt(mouseX_, mouseY_, cx, cy);
        const int hover = paletteKeyAt(mouseX_, mouseY_);
        line1 = "Selected tile: " + std::to_string(selected_) + " (0-" +
                std::to_string(tileset_.count() - 1) + ")";
        if (hover >= 0) line1 += "   Under cursor: " + std::to_string(hover);
        if (inMap) line1 += "   Cell: " + std::to_string(cx) + "," + std::to_string(cy);
        line1 += "   Map: " + std::to_string(tilemap_.w) + "x" + std::to_string(tilemap_.h) +
                 "   Tile: " + std::to_string(tileset_.tileW) + "x" + std::to_string(tileset_.tileH) +
                 "px   Zoom: " + std::to_string(static_cast<int>(zoom() * 100.f + 0.5f)) + "%";
    }
    text(8.f, statusRect_.y + 5.f, line1, 1.f, c1);
    text(8.f, statusRect_.y + 17.f,
         "LMB: paint | RMB: erase | MMB/Arrow keys: pan | Wheel: zoom | F: fit map | K: toggle tile keys | Ctrl+S: export | Esc: back to setup",
         1.f, kText);
}
