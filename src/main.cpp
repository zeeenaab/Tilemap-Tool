#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "ui.h"

int main(int /*argc*/, char** /*argv*/) 
{
    if (!SDL_Init(SDL_INIT_VIDEO)) 
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("Zee's Mini Tilemap Tool", 1100, 720, SDL_WINDOW_RESIZABLE, &window, &renderer)) 
    {
        SDL_Log("Could not create window/renderer: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowMinimumSize(window, 840, 600);
    SDL_SetRenderVSync(renderer, 1);

    SDL_Surface* icon = SDL_LoadBMP("../icon.ico");
    SDL_SetWindowIcon(window, icon);

    {
        App app(window, renderer);
        while (app.running) 
        {
            // sleeps until something occurs or 100ms passes (for the blinking caret), so near-zero CPU while idle.
            SDL_Event e;
            if (SDL_WaitEventTimeout(&e, 100)) 
            {
                do 
                {
                    app.handleEvent(e);
                } while (SDL_PollEvent(&e));
            }
            app.render();
        }
    }  // App (and its tileset texture) is destroyed here, before the renderer

    SDL_DestroySurface(icon);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
