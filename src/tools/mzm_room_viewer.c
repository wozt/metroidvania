/* SPDX-License-Identifier: GPL-3.0-only */
/* Read-only SDL3 view of locally extracted, partial original MZM BG preview. */
#include <SDL3/SDL.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Surface *surface = NULL;
    SDL_Texture *texture = NULL;
    bool running = true;
    int result = 1;
    int iw, ih;
    if (argc != 2) {
        fprintf(stderr,"Usage: %s assets/extracted/rooms/metroid/previews/brinstar_033_bg1.bmp\n",argv[0]);
        return 2;
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr,"SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }
    surface = SDL_LoadBMP(argv[1]);
    if (!surface) {
        fprintf(stderr,"Cannot open local original-room preview: %s\n",SDL_GetError());
        goto done;
    }
    iw = surface->w; ih = surface->h;
    if (iw < 1 || ih < 1 || iw > 4096 || ih > 4096) goto done;
    window = SDL_CreateWindow("Metroid Vania / original Zero Mission BG preview",
                              iw * 2 > 1280 ? 1280 : iw * 2,
                              ih * 2 > 900 ? 900 : ih * 2, SDL_WINDOW_RESIZABLE);
    renderer = window ? SDL_CreateRenderer(window,NULL) : NULL;
    texture = renderer ? SDL_CreateTextureFromSurface(renderer,surface) : NULL;
    if (!window || !renderer || !texture) {
        fprintf(stderr,"SDL preview setup failed: %s\n",SDL_GetError());
        goto done;
    }
    SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST);
    while (running) {
        SDL_Event event;
        int w,h;
        float scale;
        SDL_FRect target;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE))
                running = false;
        }
        if (!running) break;
        if (!SDL_GetRenderOutputSize(renderer,&w,&h)) break;
        scale = SDL_min((float)w/(float)iw,(float)h/(float)ih);
        target = (SDL_FRect){(w-iw*scale)*0.5f,(h-ih*scale)*0.5f,
                             iw*scale,ih*scale};
        SDL_SetRenderDrawColor(renderer,8,10,18,255);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer,texture,NULL,&target);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    result = 0;
done:
    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
