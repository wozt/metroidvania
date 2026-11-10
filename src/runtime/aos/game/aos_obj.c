/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria's OBJ layer: see aos_obj.h. */
#include "aos_obj.h"

#include <stdlib.h>
#include <string.h>

void aos_obj_layer_clear(AosObjLayer *layer) {
    memset(layer, 0, sizeof *layer);
}

static uint8_t frame_pixel(const AosObjSheet *sheet, const AosObjFrame *frame, int fx, int fy) {
    for (unsigned k = 0; k < frame->count; ++k) {
        const AosObjPiece *p = &frame->pieces[k];
        int px = fx - p->x, py = fy - p->y;
        if (px < 0 || py < 0 || px >= p->width || py >= p->height) continue;
        int sx = p->source_x + (p->hflip ? p->width - 1 - px : px);
        int sy = p->source_y + (p->vflip ? p->height - 1 - py : py);
        uint8_t c = sheet->pixels[sy * AOS_OBJ_SHEET_SIZE + sx];
        if (c) return c;
    }
    return 0;
}

void aos_obj_layer_draw(AosObjLayer *layer, const AosObjSheet *sheets, const AosObjSprite *s) {
    const AosObjSheet *sheet = &sheets[s->sheet];
    if (s->record >= sheet->frame_count) return;
    const AosObjFrame *frame = &sheet->frames[s->record];
    int reach = 0;
    for (unsigned k = 0; k < frame->count; ++k) {
        const AosObjPiece *p = &frame->pieces[k];
        int e[4] = {abs(p->x), abs(p->y), abs(p->x + p->width), abs(p->y + p->height)};
        for (int j = 0; j < 4; ++j)
            if (e[j] > reach) reach = e[j];
    }
    if (s->affine) reach *= 3;
    for (int dy = -reach; dy <= reach; ++dy) {
        int y = s->y + dy;
        if (y < 0 || y >= AOS_OBJ_SCREEN_H) continue;
        for (int dx = -reach; dx <= reach; ++dx) {
            int x = s->x + dx;
            if (x < 0 || x >= AOS_OBJ_SCREEN_W) continue;
            int fx = dx, fy = dy;
            if (s->affine) {
                int32_t tx = s->pa * dx + s->pb * dy, ty = s->pc * dx + s->pd * dy;
                fx = tx >> 8;
                fy = ty >> 8;
            }
            uint8_t c = frame_pixel(sheet, frame, fx, fy);
            if (!c) continue;
            size_t at = (size_t)y * AOS_OBJ_SCREEN_W + (size_t)x;
            if (s->mode == 2) {
                layer->window[at] = true;
            } else if (!layer->index[at] || s->priority < layer->priority[at]) {
                layer->index[at] = (uint8_t)(s->palette_row * 16 + c);
                layer->priority[at] = s->priority;
                layer->semi[at] = s->mode == 1;
            }
        }
    }
}
