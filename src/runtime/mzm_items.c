/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission tank pickups; comments name the pinned decompilation's
 * routines. */
#include "mzm_items.h"

static int tank_index(uint16_t behavior) {
    /* BEHAVIOR_TO_TANK as an unsigned comparison: behaviors below the
     * tanks wrap around. */
    unsigned index = (unsigned)behavior - MZM_TANK_BEHAVIOR_START;
    return index < MZM_TANK_BEHAVIORS ? (int)index : -1;
}

uint8_t mzm_tank_item_type(const MzmTankTables *tables, uint16_t behavior) {
    int index = tank_index(behavior);
    return index < 0 ? MZM_ITEM_TYPE_NONE : tables->item_type[index];
}

uint16_t mzm_tank_cleared_clip(const MzmTankTables *tables, uint16_t behavior) {
    int index = tank_index(behavior);
    return index >= 0 && tables->underwater[index] ? tables->water_clip : tables->air_clip;
}

/* "max number of tanks * increase + starting >= current max + increase" */
static bool below_limit(const MzmTankTables *t, int column, int max) {
    return t->total[column] * t->increase[column] + t->start[column] >= max + t->increase[column];
}

int mzm_tank_touch(const MzmTankTables *tables, MzmTankCollection *collection,
                   const MzmTouchProbes *probes, MzmEquipment *equipment) {
    if ((probes->behaviors[0] | probes->behaviors[1] | probes->behaviors[2] |
         probes->behaviors[3]) == 0)
        return -1;
    /* Abort if already collecting a tank */
    if (collection->collecting) return -1;
    for (int j = 3; j >= 0; --j) {
        uint16_t behavior = probes->behaviors[j];
        if (behavior == 0) continue;
        int index = tank_index(behavior);
        if (index < 0) continue;
        uint8_t type = tables->item_type[index];
        /* Hidden tanks not broken aren't tanks yet; MinimapCheckIsTileExplored
         * is taken as true. */
        if (type == MZM_ITEM_TYPE_NONE) continue;
        collection->collecting = true;
        collection->behavior = behavior;
        collection->x = probes->x[j];
        collection->y = probes->y[j];
        bool first = false;
        if (type == MZM_ITEM_TYPE_MISSILE) {
            if (below_limit(tables, MZM_TANK_MISSILE, equipment->max_missiles)) {
                if (equipment->max_missiles == 0) first = true;
                equipment->max_missiles += tables->increase[MZM_TANK_MISSILE];
                equipment->missiles += tables->increase[MZM_TANK_MISSILE];
            }
        } else if (type == MZM_ITEM_TYPE_ENERGY) {
            if (below_limit(tables, MZM_TANK_ENERGY, equipment->max_energy)) {
                equipment->max_energy += tables->increase[MZM_TANK_ENERGY];
                equipment->energy = equipment->max_energy;
            }
        } else if (type == MZM_ITEM_TYPE_SUPER_MISSILE) {
            if (below_limit(tables, MZM_TANK_SUPER_MISSILE, equipment->max_super_missiles)) {
                if (equipment->max_super_missiles == 0) first = true;
                equipment->max_super_missiles += tables->increase[MZM_TANK_SUPER_MISSILE];
                equipment->super_missiles += tables->increase[MZM_TANK_SUPER_MISSILE];
            }
        } else if (type == MZM_ITEM_TYPE_POWER_BOMB) {
            if (below_limit(tables, MZM_TANK_POWER_BOMB, equipment->max_power_bombs)) {
                if (equipment->max_power_bombs == 0) first = true;
                equipment->max_power_bombs += tables->increase[MZM_TANK_POWER_BOMB];
                equipment->power_bombs += tables->increase[MZM_TANK_POWER_BOMB];
            }
        }
        /* The message banner: MESSAGE_NONE stays none. */
        int message = tables->message[index];
        return message < 0 ? -1 : message + (first ? 1 : 0);
    }
    return -1;
}

void mzm_items_set_collected(MzmCollectedItems *items, uint8_t area, uint8_t room, int x, int y,
                             uint8_t type) {
    if (area >= MZM_MAX_AREAS || items->count[area] >= MZM_MAX_ITEMS_PER_AREA) return;
    int n = items->count[area]++;
    items->items[area][n].room = room;
    items->items[area][n].type = type;
    items->items[area][n].x = (uint8_t)x;
    items->items[area][n].y = (uint8_t)y;
}
