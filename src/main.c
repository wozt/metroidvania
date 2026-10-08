#include <SDL3/SDL.h>

#include "core/backend.h"
#include "core/rom.h"
#include "core/save.h"
#include "core/session.h"

#include <stdio.h>
#include <string.h>

#define WINDOW_WIDTH 960
#define WINDOW_HEIGHT 540

static void usage(const char *program)
{
    fprintf(stderr, "Usage: %s [--aria chemin.gba] [--metroid chemin.gba] [--validate-only]\n", program);
}

static void key_event(FusionInput *input, SDL_Keycode key, bool *running, bool *debug)
{
    switch (key) {
        case SDLK_ESCAPE: *running = false; break;
        case SDLK_SPACE: input->jump_pressed = true; break;
        case SDLK_J: input->attack_pressed = true; break;
        case SDLK_TAB: input->switch_character_pressed = true; break;
        case SDLK_M: input->switch_world_pressed = true; break;
        case SDLK_F3: input->debug_pressed = true; *debug = !*debug; break;
        case SDLK_K: input->damage_pressed = true; break;
        case SDLK_F5: input->save_pressed = true; break;
        case SDLK_F9: input->load_pressed = true; break;
        default: break;
    }
}

int main(int argc, char **argv)
{
    const char *aria_path = "roms/Castlevania - Aria of Sorrow (USA).gba";
    const char *metroid_path = "roms/Metroid - Zero Mission (USA).gba";
    bool validate_only = false;
    RomRequirement roms[2];
    char error[256];
    SessionState session;
    FusionBackend backends[2];
    SDL_Window *window;
    SDL_Renderer *renderer;
    bool running = true;
    bool debug = true;
    Uint64 previous;
    int i;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--aria") == 0 && i + 1 < argc) aria_path = argv[++i];
        else if (strcmp(argv[i], "--metroid") == 0 && i + 1 < argc) metroid_path = argv[++i];
        else if (strcmp(argv[i], "--validate-only") == 0) validate_only = true;
        else { usage(argv[0]); return 2; }
    }
    roms[0] = (RomRequirement){ROM_ARIA_US, "Castlevania: Aria of Sorrow USA",
        "abd71fe01ebb201bcc133074db1dd8c5253776c7", aria_path};
    roms[1] = (RomRequirement){ROM_ZERO_MISSION_US, "Metroid: Zero Mission USA",
        "5de8536afe1f0078ee6fe1089f890e8c7aa0a6e8", metroid_path};
    for (i = 0; i < 2; ++i) {
        if (!rom_validate(&roms[i], error, sizeof(error))) {
            fprintf(stderr, "ROM refusée: %s\nConsultez roms/README.md. Aucun fichier ne sera envoyé.\n", error);
            return 3;
        }
        printf("ROM validée localement: %s\n", roms[i].label);
    }
    if (validate_only) return 0;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 4;
    }
    window = SDL_CreateWindow("Metroidvania Fusion — prototype d'intégration", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    renderer = window ? SDL_CreateRenderer(window, NULL) : NULL;
    if (!window || !renderer) {
        fprintf(stderr, "Création SDL: %s\n", SDL_GetError()); SDL_Quit(); return 4;
    }
    SDL_SetRenderVSync(renderer, 1);

    session_init(&session);
    backends[WORLD_METROID] = metroid_backend_create();
    backends[WORLD_CASTLEVANIA] = castlevania_backend_create();
    for (i = 0; i < 2; ++i) {
        if (!backends[i].ops->init(&backends[i], &session)) {
            fprintf(stderr, "Initialisation backend impossible: %s\n", backends[i].name);
            running = false;
        }
    }
    if (running) backends[session.active_world].ops->enter_world(&backends[session.active_world], &session);
    previous = SDL_GetTicks();
    while (running) {
        FusionInput input = {0};
        SDL_Event event;
        const bool *keys;
        Uint64 now = SDL_GetTicks();
        float dt = (float)(now - previous) / 1000.0f;
        float fps;
        FusionBackend *active;
        previous = now;
        if (dt > 0.05f) dt = 0.05f;
        fps = dt > 0 ? 1.0f / dt : 0;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat)
                key_event(&input, event.key.key, &running, &debug);
        }
        keys = SDL_GetKeyboardState(NULL);
        input.left = keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_Q] || keys[SDL_SCANCODE_A];
        input.right = keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D];
        active = &backends[session.active_world];

        if (input.save_pressed) {
            if (!save_session("fusion-save-v1.bin", &session, error, sizeof(error)))
                fprintf(stderr, "Sauvegarde: %s\n", error);
        }
        if (input.load_pressed) {
            WorldKind old_world = session.active_world;
            active->ops->leave_world(active, &session);
            if (!load_session("fusion-save-v1.bin", &session, error, sizeof(error))) {
                fprintf(stderr, "Chargement: %s\n", error);
                session.active_world = old_world;
            }
            active = &backends[session.active_world];
            active->ops->enter_world(active, &session);
        }
        if (input.switch_world_pressed) {
            active->ops->leave_world(active, &session);
            session.active_world = session.active_world == WORLD_METROID
                ? WORLD_CASTLEVANIA : WORLD_METROID;
            active = &backends[session.active_world];
            active->ops->enter_world(active, &session);
        }
        active->ops->tick(active, &session, &input, dt);
        active->ops->render(active, &session, renderer, debug, fps);
        SDL_RenderPresent(renderer);
    }

    for (i = 0; i < 2; ++i) backends[i].ops->shutdown(&backends[i]);
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
    return 0;
}
