#ifndef FUSION_CORE_SESSION_H
#define FUSION_CORE_SESSION_H

#include "core/types.h"

void session_init(SessionState *session);
bool session_damage_active(SessionState *session, int32_t damage);
bool session_switch_character(SessionState *session);

#endif
