#include <SDL3/SDL.h>

#include "aria/preview.h"
#include "core/backend.h"
#include "core/rom.h"
#include "core/save.h"
#include "core/session.h"
#include "gba/runtime.h"

#include <stdio.h>
#include <string.h>

#define WINDOW_WIDTH 960
#define WINDOW_HEIGHT 540

static void usage(const char *program)
{
    fprintf(stderr, "Usage: %s [--aria path.gba] [--metroid path.gba] [--validate-only] "
                    "[--aria-video-test | --authentic-video-test | --authentic-probe]\n",
            program);
}

static uint64_t hash_frame(const GbaFrameView *frame)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    uint32_t y;
    for (y = 0; y < frame->height; ++y) {
        uint32_t x;
        for (x = 0; x < frame->width; ++x) {
            uint32_t pixel = frame->rgba32[y * frame->stride_pixels + x];
            unsigned byte;
            for (byte = 0; byte < sizeof(pixel); ++byte) {
                hash ^= (pixel >> (byte * 8u)) & 0xffu;
                hash *= UINT64_C(1099511628211);
            }
        }
    }
    return hash;
}

static bool run_authentic_probe(const char *label, const char *rom_path,
                                char *error, size_t error_size)
{
    GbaRuntime runtime = {0};
    GbaFrameView frame;
    unsigned index;
    size_t nonzero = 0;
    size_t pixel_count;

    if (!gba_runtime_open(&runtime, rom_path, error, error_size) ||
        !gba_runtime_enter(&runtime)) {
        gba_runtime_close(&runtime);
        return false;
    }
    for (index = 0; index < 300; ++index) {
        if (!gba_runtime_step(&runtime, 0)) {
            gba_runtime_close(&runtime);
            return false;
        }
    }
    if (!gba_runtime_frame(&runtime, &frame)) {
        gba_runtime_close(&runtime);
        return false;
    }
    pixel_count = (size_t)frame.width * frame.height;
    for (index = 0; index < pixel_count; ++index)
        if (frame.rgba32[index]) ++nonzero;
    printf("Authentic probe: %s frames=300 size=%ux%u nonzero=%zu hash=%016llx\n",
           label, frame.width, frame.height, nonzero,
           (unsigned long long)hash_frame(&frame));
    gba_runtime_close(&runtime);
    return nonzero != 0;
}

static uint16_t gba_keys(const bool *keys)
{
    return (uint16_t)(
        ((keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_X]) ? (1u << 0) : 0u) |
        ((keys[SDL_SCANCODE_J] || keys[SDL_SCANCODE_Z]) ? (1u << 1) : 0u) |
        (keys[SDL_SCANCODE_RSHIFT] ? (1u << 2) : 0u) |
        (keys[SDL_SCANCODE_RETURN] ? (1u << 3) : 0u) |
        ((keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) ? (1u << 4) : 0u) |
        ((keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_Q] ||
          keys[SDL_SCANCODE_A]) ? (1u << 5) : 0u) |
        (keys[SDL_SCANCODE_UP] ? (1u << 6) : 0u) |
        (keys[SDL_SCANCODE_DOWN] ? (1u << 7) : 0u) |
        (keys[SDL_SCANCODE_I] ? (1u << 8) : 0u) |
        (keys[SDL_SCANCODE_U] ? (1u << 9) : 0u));
}

static bool present_gba_frame(SDL_Renderer *renderer, SDL_Texture *texture,
                              const GbaFrameView *frame)
{
    int output_width = WINDOW_WIDTH;
    int output_height = WINDOW_HEIGHT;
    float scale;
    SDL_FRect destination;

    if (!renderer || !texture || !frame || !frame->rgba32 ||
        frame->width != GBA_FRAME_WIDTH || frame->height != GBA_FRAME_HEIGHT)
        return false;
    if (!SDL_GetRenderOutputSize(renderer, &output_width, &output_height))
        return false;
    scale = SDL_min((float)output_width / GBA_FRAME_WIDTH,
                    (float)output_height / GBA_FRAME_HEIGHT);
    destination.w = GBA_FRAME_WIDTH * scale;
    destination.h = GBA_FRAME_HEIGHT * scale;
    destination.x = ((float)output_width - destination.w) * 0.5f;
    destination.y = ((float)output_height - destination.h) * 0.5f;
    return SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255) &&
           SDL_RenderClear(renderer) &&
           SDL_UpdateTexture(texture, NULL, frame->rgba32,
                             (int)(frame->stride_pixels * sizeof(uint32_t))) &&
           SDL_RenderTexture(renderer, texture, NULL, &destination);
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
    bool aria_video_test = false;
    bool authentic_video_test = false;
    bool authentic_probe = false;
    AriaPreview preview = {0};
    GbaRuntime gba_runtimes[FUSION_WORLD_COUNT] = {{0}};
    RomRequirement roms[2];
    char error[256];
    SessionState session;
    FusionBackend backends[2] = {{0}};
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *gba_texture = NULL;
    bool running = true;
    bool debug = true;
    int exit_code = 0;
    Uint64 previous;
    int i;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--aria") == 0 && i + 1 < argc) aria_path = argv[++i];
        else if (strcmp(argv[i], "--metroid") == 0 && i + 1 < argc) metroid_path = argv[++i];
        else if (strcmp(argv[i], "--validate-only") == 0) validate_only = true;
        else if (strcmp(argv[i], "--aria-video-test") == 0) aria_video_test = true;
        else if (strcmp(argv[i], "--authentic-video-test") == 0) authentic_video_test = true;
        else if (strcmp(argv[i], "--authentic-probe") == 0) authentic_probe = true;
        else { usage(argv[0]); return 2; }
    }
    if ((aria_video_test ? 1 : 0) + (authentic_video_test ? 1 : 0) +
        (authentic_probe ? 1 : 0) > 1) {
        fprintf(stderr, "Choose only one video test mode.\n");
        return 2;
    }
    roms[0] = (RomRequirement){ROM_ARIA_US, "Castlevania: Aria of Sorrow USA",
        "abd71fe01ebb201bcc133074db1dd8c5253776c7", aria_path};
    roms[1] = (RomRequirement){ROM_ZERO_MISSION_US, "Metroid: Zero Mission USA",
        "5de8536afe1f0078ee6fe1089f890e8c7aa0a6e8", metroid_path};
    for (i = 0; i < 2; ++i) {
        if (!rom_validate(&roms[i], error, sizeof(error))) {
            fprintf(stderr, "ROM rejected: %s\nSee roms/README.md. No file will be uploaded.\n", error);
            return 3;
        }
        printf("ROM validated locally: %s\n", roms[i].label);
    }
    if (validate_only) return 0;
    if (authentic_probe) {
        if (!run_authentic_probe("Metroid: Zero Mission", metroid_path,
                                 error, sizeof(error)) ||
            !run_authentic_probe("Castlevania: Aria of Sorrow", aria_path,
                                 error, sizeof(error))) {
            fprintf(stderr, "Authentic GBA probe failed: %s\n", error);
            return 4;
        }
        return 0;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 4;
    }
    window = SDL_CreateWindow("Metroidvania Fusion - integration prototype", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    renderer = window ? SDL_CreateRenderer(window, NULL) : NULL;
    if (!window || !renderer) {
        fprintf(stderr, "SDL creation failed: %s\n", SDL_GetError()); SDL_Quit(); return 4;
    }
    SDL_SetRenderVSync(renderer, 1);
    if (aria_video_test) {
        if (!aria_preview_open(&preview, renderer)) {
            fprintf(stderr, "Synthetic Aria preview initialization failed: %s\n", SDL_GetError());
            SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
            return 4;
        }
        SDL_SetWindowTitle(window, "Metroidvania Fusion - SYNTHETIC Aria video test");
        puts("SYNTHETIC VIDEO TEST ONLY - NO ORIGINAL ARIA ENGINE IS RUNNING");
    }

    session_init(&session);
    if (authentic_video_test) {
        const char *paths[FUSION_WORLD_COUNT] = {metroid_path, aria_path};
        for (i = 0; i < (int)FUSION_WORLD_COUNT; ++i) {
            if (!gba_runtime_open(&gba_runtimes[i], paths[i], error, sizeof(error))) {
                fprintf(stderr, "Authentic GBA runtime initialization failed: %s\n", error);
                running = false;
                exit_code = 4;
                break;
            }
        }
        if (running && !gba_runtime_enter(&gba_runtimes[session.active_world])) {
            fprintf(stderr, "Cannot activate the initial GBA runtime.\n");
            running = false;
            exit_code = 4;
        }
        if (running) {
            gba_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                            SDL_TEXTUREACCESS_STREAMING,
                                            (int)GBA_FRAME_WIDTH, (int)GBA_FRAME_HEIGHT);
            if (!gba_texture) {
                fprintf(stderr, "GBA texture creation failed: %s\n", SDL_GetError());
                running = false;
                exit_code = 4;
            }
            else SDL_SetTextureScaleMode(gba_texture, SDL_SCALEMODE_NEAREST);
        }
        SDL_SetWindowTitle(window, "Metroidvania Fusion - AUTHENTIC ROM video test");
        puts("AUTHENTIC ROM VIDEO TEST - mGBA execution, one active world at a time");
    } else {
        backends[WORLD_METROID] = metroid_backend_create();
        backends[WORLD_CASTLEVANIA] = castlevania_backend_create();
        for (i = 0; i < 2; ++i) {
            if (!backends[i].ops->init(&backends[i], &session)) {
                fprintf(stderr, "Backend initialization failed: %s\n", backends[i].name);
                running = false;
            }
        }
        if (running)
            backends[session.active_world].ops->enter_world(
                &backends[session.active_world], &session);
    }
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

        if (authentic_video_test) {
            GbaRuntime *runtime = &gba_runtimes[session.active_world];
            GbaFrameView frame;
            if (input.switch_world_pressed) {
                gba_runtime_leave(runtime);
                session.active_world = session.active_world == WORLD_METROID
                    ? WORLD_CASTLEVANIA : WORLD_METROID;
                runtime = &gba_runtimes[session.active_world];
                if (!gba_runtime_enter(runtime)) {
                    fprintf(stderr, "Cannot activate the selected GBA runtime.\n");
                    running = false;
                    exit_code = 5;
                }
            }
            if (running &&
                (!gba_runtime_step(runtime, gba_keys(keys)) ||
                 !gba_runtime_frame(runtime, &frame) ||
                 !present_gba_frame(renderer, gba_texture, &frame))) {
                fprintf(stderr, "Authentic GBA frame failed: %s\n", SDL_GetError());
                running = false;
                exit_code = 5;
            }
            SDL_RenderPresent(renderer);
            continue;
        }

        active = &backends[session.active_world];

        if (input.save_pressed) {
            if (!save_session("fusion-save-v1.bin", &session, error, sizeof(error)))
                fprintf(stderr, "Save failed: %s\n", error);
        }
        if (input.load_pressed) {
            SessionState candidate;
            if (!load_session("fusion-save-v1.bin", &candidate, error, sizeof(error))) {
                fprintf(stderr, "Load failed: %s\n", error);
            } else {
                /* Only commit after the file has passed validation. */
                active->ops->leave_world(active, &session);
                session = candidate;
                active = &backends[session.active_world];
                if (!active->ops->enter_world(active, &session)) {
                    fprintf(stderr, "Cannot enter restored world: %s\n", active->name);
                    running = false;
                }
            }
        }
        if (input.switch_world_pressed) {
            active->ops->leave_world(active, &session);
            session.active_world = session.active_world == WORLD_METROID
                ? WORLD_CASTLEVANIA : WORLD_METROID;
            active = &backends[session.active_world];
            active->ops->enter_world(active, &session);
        }
        active->ops->tick(active, &session, &input, dt);
        if (aria_video_test && session.active_world == WORLD_CASTLEVANIA) {
            uint16_t gba_keys = (uint16_t)((input.left ? (1u << 5) : 0u) |
                                           (input.right ? (1u << 4) : 0u) |
                                           (input.jump_pressed ? 1u : 0u));
            int output_w = WINDOW_WIDTH, output_h = WINDOW_HEIGHT;
            SDL_GetRenderOutputSize(renderer, &output_w, &output_h);
            if (!aria_preview_draw(&preview, renderer, gba_keys, output_w, output_h)) {
                fprintf(stderr, "Synthetic video preview failed: %s\n", SDL_GetError());
                running = false;
            }
        } else {
            active->ops->render(active, &session, renderer, debug, fps);
        }
        SDL_RenderPresent(renderer);
    }

    if (authentic_video_test) {
        for (i = 0; i < (int)FUSION_WORLD_COUNT; ++i)
            gba_runtime_close(&gba_runtimes[i]);
    } else {
        for (i = 0; i < 2; ++i) backends[i].ops->shutdown(&backends[i]);
    }
    aria_preview_close(&preview);
    if (gba_texture) SDL_DestroyTexture(gba_texture);
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
    return exit_code;
}
