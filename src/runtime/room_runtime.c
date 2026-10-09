/* SPDX-License-Identifier: GPL-3.0-only */
/* Early C11/SDL3 runtime experiment, NOT faithful MZM player physics. */
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MARKS 32768
#define MAX_FILE_BYTES 4000000L

typedef struct { int x, y, w, h, code; } Collision;
typedef struct { int width, height, resolution, room; char world[16], area[40];
    Collision collisions[MAX_MARKS]; size_t count; } Room;

static bool parse_room(const char *path, Room *room) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return false; }
    char line[256], extra; int version; bool ended = false;
    if (fseek(f, 0, SEEK_END) || ftell(f) < 0 || ftell(f) > MAX_FILE_BYTES ||
        fseek(f, 0, SEEK_SET)) goto failure;
    if (!fgets(line, sizeof line, f) || !strchr(line, '\n') ||
        sscanf(line, "MVROOM-PREVIEW\t%d\t%15[^\t]\t%39[^\t]\t%d\t%d\t%d\t%d %c",
            &version, room->world, room->area, &room->room, &room->width,
            &room->height, &room->resolution, &extra) != 7 || version != 1 ||
        (strcmp(room->world, "mzm") && strcmp(room->world, "aria")) ||
        room->room < 0 || room->room > 999 ||
        room->width < 16 || room->height < 16 ||
        room->width > 16384 || room->height > 16384 ||
        room->resolution != 16 || room->width % 16 || room->height % 16 ||
        (long long)room->width * room->height > 6144LL * 256) goto failure;
    while (fgets(line, sizeof line, f)) {
        char kind; int x, y, w, h, code;
        if (!strchr(line, '\n') && !feof(f)) goto failure;
        if (!strcmp(line, "END\n") || !strcmp(line, "END")) {
            ended = true; break;
        }
        if (sscanf(line, "%c\t%d\t%d\t%d\t%d\t%d %c",
                   &kind, &x, &y, &w, &h, &code, &extra) != 6 ||
            (kind != 'C' && kind != 'D' && kind != 'E' && kind != 'V') ||
            x < 0 || y < 0 || w <= 0 || h <= 0 ||
            x > room->width - w || y > room->height - h ||
            (kind == 'C' && (code < 1 || code > 7 || w != 16 || h != 16 ||
                 x % 16 || y % 16)) ||
            (kind != 'C' && code != 0)) goto failure;
        if (kind == 'C') {
            if (room->count >= MAX_MARKS) goto failure;
            room->collisions[room->count++] = (Collision){x,y,w,h,code};
        }
    }
    if (!ended || fgetc(f) != EOF) goto failure;
    fclose(f); return true;
failure:
    fclose(f); fprintf(stderr, "Invalid room preview: %s\n", path); return false;
}

static bool blocked(const Room *r, float x, float y, float w, float h) {
    if (x < 0 || y < 0 || x + w > r->width || y + h > r->height) return true;
    for (size_t i=0; i<r->count; ++i) {
        const Collision *c=&r->collisions[i];
        /* Only code=1 is treated as a solid rectangle. No native Clipdata guesses. */
        if (c->code == 1 && x < c->x+c->w && x+w > c->x &&
            y < c->y+c->h && y+h > c->y) return true;
    }
    return false;
}

static float clampf(float x, float low, float high) {
    return x < low ? low : x > high ? high : x;
}

int main(int argc, char **argv) {
    const char *room_path=NULL, *background=NULL; bool check=false;
    for (int i=1;i<argc;i++) {
        if (!strcmp(argv[i],"--check")) { if(check) return 2; check=true; }
        else if (!strcmp(argv[i],"--background") && !background && i+1<argc) background=argv[++i];
        else if (argv[i][0]=='-' || room_path) {
            fprintf(stderr,"Usage: %s [--check] [--background image.bmp] preview.tsv\n",argv[0]);
            return 2;
        } else room_path=argv[i];
    }
    if (!room_path || (check && background)) {
        fprintf(stderr,"Usage: %s [--check] [--background image.bmp] preview.tsv\n",argv[0]);
        return 2;
    }
    Room *room=calloc(1,sizeof *room);
    if (!room) return 1;
    if (!parse_room(room_path,room)) { free(room); return 2; }
    printf("Runtime room %s/%s/%d %dx%d: %zu project collision entries\n",
           room->world,room->area,room->room,room->width,room->height,room->count);
    if (check) { free(room); return 0; }
    if (!SDL_Init(SDL_INIT_VIDEO)) { fprintf(stderr,"SDL_Init: %s\n",SDL_GetError());free(room);return 1; }
    SDL_Window *window=SDL_CreateWindow("Metroid Vania - experimental C11 runtime",
                                       960,640,SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer=window ? SDL_CreateRenderer(window,NULL) : NULL;
    SDL_Surface *surface=NULL; SDL_Texture *texture=NULL;
    int rc=1;
    if (!renderer) { fprintf(stderr,"SDL renderer: %s\n",SDL_GetError()); goto cleanup; }
    if (background) {
        surface=SDL_LoadBMP(background);
        if (!surface || surface->w != room->width || surface->h != room->height) {
            fprintf(stderr,"Background BMP must match room dimensions: %dx%d\n",
                    room->width,room->height); goto cleanup;
        }
        texture=SDL_CreateTextureFromSurface(renderer,surface);
        if (!texture) { fprintf(stderr,"Texture: %s\n",SDL_GetError());goto cleanup; }
        SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST);
    }
    float px=16,py=16, pw=12,ph=16; const float speed=110.f;
    /* Search for a nonblocked spawn; this is a test avatar, not extracted Samus. */
    bool spawn=false;
    for (int y=0;y<=room->height-16 && !spawn;y+=16)
        for (int x=0;x<=room->width-16 && !spawn;x+=16)
            if (!blocked(room,(float)x,(float)y,pw,ph)) {
                px=(float)x;py=(float)y;spawn=true;
            }
    if (!spawn) { fprintf(stderr,"No free avatar spawn found\n"); goto cleanup; }
    printf("Controls: arrows/WASD = move test avatar; Escape = exit. No gravity/combat yet.\n");
    Uint64 previous=SDL_GetTicks(); bool running=true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) running=false;
        }
        Uint64 current=SDL_GetTicks();
        float dt=clampf((float)(current-previous)/1000.f,0.f,0.05f);
        previous=current;
        const bool *keys=SDL_GetKeyboardState(NULL);
        float dx=((keys[SDL_SCANCODE_RIGHT]||keys[SDL_SCANCODE_D]) ? 1.f:0.f)-
                 ((keys[SDL_SCANCODE_LEFT]||keys[SDL_SCANCODE_A]) ? 1.f:0.f);
        float dy=((keys[SDL_SCANCODE_DOWN]||keys[SDL_SCANCODE_S]) ? 1.f:0.f)-
                 ((keys[SDL_SCANCODE_UP]||keys[SDL_SCANCODE_W]) ? 1.f:0.f);
        if (dx && dy) { dx*=0.70710678f;dy*=0.70710678f; }
        float nx=px+dx*speed*dt,ny=py+dy*speed*dt;
        if (!blocked(room,nx,py,pw,ph)) px=nx;
        if (!blocked(room,px,ny,pw,ph)) py=ny;
        int ow=0,oh=0;
        if (!SDL_GetRenderOutputSize(renderer,&ow,&oh) || ow<=0 || oh<=0) continue;
        float scale=(float)ow/320.f;
        if ((float)oh/224.f < scale) scale=(float)oh/224.f;
        SDL_FRect viewport={(ow-320.f*scale)*0.5f,(oh-224.f*scale)*0.5f,
                            320.f*scale,224.f*scale};
        float cx=clampf(px+pw*.5f-160.f,0.f,(float)(room->width>320?room->width-320:0));
        float cy=clampf(py+ph*.5f-112.f,0.f,(float)(room->height>224?room->height-224:0));
        SDL_SetRenderDrawColor(renderer,12,14,24,255);SDL_RenderClear(renderer);
        if (texture) {
            SDL_FRect src={cx,cy,320.f,224.f};
            if (src.w>room->width) src.w=(float)room->width;
            if (src.h>room->height) src.h=(float)room->height;
            SDL_FRect dst={viewport.x,viewport.y,src.w*scale,src.h*scale};
            SDL_RenderTexture(renderer,texture,&src,&dst);
        }
        SDL_SetRenderDrawColor(renderer,80,205,115,255);
        SDL_FRect avatar={viewport.x+(px-cx)*scale, viewport.y+(py-cy)*scale,
                          pw*scale,ph*scale};
        SDL_RenderFillRect(renderer,&avatar);
        SDL_RenderPresent(renderer);
        SDL_Delay(8);
    }
    rc=0;
cleanup:
    if (texture) SDL_DestroyTexture(texture);
    if (surface) SDL_DestroySurface(surface);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();free(room);return rc;
}
