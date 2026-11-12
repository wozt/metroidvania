#ifndef AOS_OBJECT1E_H
#define AOS_OBJECT1E_H
#include <stdbool.h>
#include <stdint.h>

/* Object1ECreate (0x08055734) / Object1EUpdate (0x08055738),
 * dispatch index 30 (0x1E): the save/warp room-walls slot is
 * an inert placeholder in the shipped ROM — both routines are
 * a bare bx lr, so the entity does nothing on create or per
 * frame. The module exists only to keep the 1:1 dispatch
 * traceability; there is no state to model. */
typedef struct {
    uint8_t reserved;
} AosObject1E;

void aos_object1e_create(AosObject1E *o);
void aos_object1e_update(AosObject1E *o);
#endif
