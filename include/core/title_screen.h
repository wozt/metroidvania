#ifndef FUSION_TITLE_SCREEN_H
#define FUSION_TITLE_SCREEN_H

#include "core/types.h"
#include <SDL3/SDL.h>

/* Title menu belongs to the PC frontend, never to the GBA diagnostic mode.
 * False means the user closed the window or pressed Escape. */
bool fusion_title_choose(SDL_Renderer *renderer, WorldKind *world);

#endif
