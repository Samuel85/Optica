// Optica on SDL3 + Dear ImGui (Linux; Android next). An optical bench: place light
// sources, objects, lenses and mirrors on an optical axis and see how the rays travel.
//
// SDL's main callbacks instead of a main loop: SDL calls these four functions, which is
// how an Android app must be driven.

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <memory>

#include "sdl_app.h"

SDL_AppResult SDL_AppInit(void** appState, int, char*[])
{
    auto app = std::make_unique<SdlApp>();
    const SDL_AppResult result = app->init();
    *appState = app.release();          /* SDL_AppQuit deletes it, even when init failed */
    return result;
}

SDL_AppResult SDL_AppEvent(void* appState, SDL_Event* event)
{
    return static_cast<SdlApp*>(appState)->handleEvent(*event);
}

SDL_AppResult SDL_AppIterate(void* appState)
{
    return static_cast<SdlApp*>(appState)->iterate();
}

void SDL_AppQuit(void* appState, SDL_AppResult)
{
    delete static_cast<SdlApp*>(appState);
}
