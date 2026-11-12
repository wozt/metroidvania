#ifndef AOS_OBJECT1A_H
#define AOS_OBJECT1A_H
#include <stdbool.h>
#include <stdint.h>

/* Object1ACreate (0x080540A0) / Object1AUpdate (0x080540A4),
 * dispatch index 26 (0x1A): the patrolling-skull slot is an
 * inert placeholder in the shipped ROM — both routines are a
 * bare bx lr, so the entity does nothing on create or per
 * frame. The module exists only to keep the 1:1 dispatch
 * traceability; there is no state to model. */
typedef struct {
    uint8_t reserved;
} AosObject1A;

void aos_object1a_create(AosObject1A *o);
void aos_object1a_update(AosObject1A *o);
#endif
