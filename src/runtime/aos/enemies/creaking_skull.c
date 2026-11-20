/* Creaking Skull (id 0x21) and Giant Skeleton (id 0x5C): the shared
 * implementation of EnemyCreakingSkullCreate 0x0809549C /
 * EnemyCreakingSkullUpdate 0x08095320 (the Giant Skeleton thunks
 * forward to them).  The boss is a head body driving a container of
 * thirteen bone records (stride 0x30) whose children run the
 * sub_0809808C segmented-follower machine.
 *
 * Native layout folded into AosEnemy:
 * - the + 0x1C container entity and its thirteen child slots fold
 *   into `cs_rec[13]` plus the AOS_ROLE_CS_BONE persistent children
 *   emitted over the first updates under the 8-child cap, chained in
 *   slot order like the native container records;
 * - each child's + 0x20 record index, + 0x21 aim target and + 0x65
 *   frame byte come from the create tables 0x08119056/0x08119049/
 *   0x0811903C indexed through 0x08119056; the + 0x36 variant byte
 *   and the sub_0804277C hurt/death callbacks (sub_08098C54 /
 *   sub_08098C84) fold into the shared collide and the bone role;
 * - the sub_0809884C/sub_080989DC rotating attack entity becomes an
 *   AOS_ROLE_CS_SHOT child and the sub_08098CF0 debris spawns
 *   AOS_ROLE_CS_GORE;
 * - the + 0xB state-0 machine, the state-1 attack machine and the
 *   + 0xD death countdown fold into step/substep/timer with the cs_*
 *   counters. Only the cases named below are translated so far.
 *
 * The container record machinery is ported: the create's variant
 * home tables (0x08527FD8 x/y pairs and the 0x085280B0 bone-2 angle
 * dwords), the sub_08097D84/sub_08097EC4 record arming, the
 * sub_08097B94 orbit/unlaunched-lerp step driven in the 0x08119063
 * order by sub_08098BE0, and the sub_0809808C bone brain's state 0
 * record ride with the ArcTan2 aim of records {0,1,11} and the held
 * angle of records {2,8}.
 *
 * The translated paths retain their native case order and song
 * stingers. The decoded per-case record re-arms are ported at the
 * points those paths traverse: the sub_08095898 case-0 alert arms on aggro,
 * the sub_08096A94 case-0 entry arms on the state-0->1 transition,
 * the case-13 windup arms (song 0x86), the case-14 volley arms
 * (song 0x80) and the case-20 reset arms on the return, each with
 * its decoded + 0xD divider (the 0x08119070/0x08119078 timing bytes
 * selected by entity + 0x24, or the per-case constants 2/0xE);
 * cs_step_perm counts + 0xD down like the native cases.  The
 * remaining case arms — the state-0 case-3 locomotion outcomes and
 * cases 4-12/20-24 strike tables,
 * the state-1 cases 4-12/15-19/21-37 including the sub_08097EC4
 * pairs and the RNG-driven projectile cases — stay unported because
 * the ported case structure does not reproduce them.  The terminal
 * cases 9/27/37 restore the sub_08098C1C pose snapshot through
 * sub_08098C38; that position-only snapshot/restore is ported.  The
 * sub_0809808C state-1/state-2 death machines keep their timers and
 * deletes while the sub_08042848/428B4/429D0 palette writes, the
 * sub_0806D3D8/sub_0806D5C0/sub_0806D518 spark helpers, the
 * sub_0803E058 affine submission for records {0,1,2,11} is connected
 * to the SDL draw path; its native pivot and OAM rounding are not yet
 * pixel-compared.  The sub_0806DFF8 child creation flags are not
 * modeled; the sub_08098E00 room gate, the
 * gEwramData flag checks and the + 0x5A palette merge are covered by
 * the shared spawn/collide paths. */

#include "../core/aos_enemy_internal.h"
#include "../core/aos_audio.h"
#include "../core/aos_collision.h"
#include "../core/aos_soma.h"
#include "../math/aos_math.h"
#include <string.h>

#define copy_result aos_enemy_copy_collision_result
#define collide aos_enemy_collide
#define face_player aos_enemy_face_player
#define target_x(...) aos_enemy_target_x(__VA_ARGS__)
#define target_y(...) aos_enemy_target_y(__VA_ARGS__)
#define outside_margins aos_enemy_outside_margins
#define step_anim aos_enemy_step_anim
#define play_loop aos_enemy_play_loop
#define play aos_enemy_play
#define integrate aos_enemy_integrate
#define projectile_collide aos_enemy_projectile_collide
#define player_near aos_enemy_player_near

#define CS_BONES 13

static bool cs_is_boss(uint8_t id) {
    return id == 0x21 || id == 0x5C;
}

/* The five 13-byte create tables of EnemyCreakingSkullCreate:
 * 0x0811903C is the child + 0x65 frame byte by record index (20 for
 * the fifth record), 0x08119049 the + 0x21 aim target, 0x08119056
 * the + 0x20 record index (bones 8/9 swapped), 0x08119063 the
 * sub_08098BE0 record step order and 0x08119081 the state-1 entry
 * timer by record index. */
static const uint8_t cs_frame_tab[CS_BONES] = {
    0, 1, 2, 3, 4, 20, 5, 6, 7, 8, 9, 10, 11,
};
static const uint8_t cs_tgt_tab[CS_BONES] = {
    1, 2, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 12,
};
static const uint8_t cs_rec_tab[CS_BONES] = {
    0, 1, 2, 3, 4, 5, 6, 7, 9, 8, 10, 11, 12,
};
static const uint8_t cs_perm_tab[CS_BONES] = {
    4, 5, 6, 7, 8, 9, 10, 3, 0, 1, 2, 11, 12,
};
static const uint8_t cs_death_tab[CS_BONES] = {
    58, 50, 42, 96, 88, 88, 88, 80, 72, 64, 60, 58, 50,
};

/* The per-variant home offsets: the 0x08527FD8 pointer table selects
 * one of thirteen 26-byte rows of signed (x, y) pixel pairs, stored
 * << 16 like the native ldrsb << 16 writes. */
static const int8_t cs_home[13][CS_BONES][2] = {
    {{-13,-31},{-41,-30},{-69,-31},{22,-25},{6,-32},{10,-34},{6,-32},
     {-11,-39},{-26,-44},{-26,-44},{12,-36},{-6,-31},{-35,-31}},
    {{-13,-29},{-41,-28},{-69,-29},{22,-25},{6,-30},{10,-32},{6,-30},
     {-11,-37},{-26,-44},{-26,-44},{12,-35},{-6,-31},{-35,-31}},
    {{-13,-29},{-41,-28},{-69,-29},{22,-25},{6,-30},{10,-32},{6,-30},
     {-11,-37},{-26,-42},{-26,-42},{12,-35},{-6,-31},{-35,-31}},
    {{-13,-31},{-41,-30},{-69,-31},{22,-25},{6,-32},{10,-34},{6,-32},
     {-11,-39},{-26,-42},{-26,-42},{12,-36},{-6,-31},{-35,-31}},
    {{-13,-31},{-41,-30},{-69,-31},{22,-25},{7,-32},{10,-34},{5,-32},
     {-11,-39},{-26,-44},{-26,-44},{11,-36},{-7,-31},{-35,-31}},
    {{-13,-29},{-41,-28},{-69,-29},{22,-25},{6,-30},{10,-32},{6,-30},
     {-11,-37},{-26,-42},{-26,-42},{12,-34},{-6,-29},{-35,-29}},
    {{-13,-31},{-41,-30},{-69,-31},{22,-25},{7,-32},{10,-34},{5,-32},
     {-11,-39},{-26,-44},{-26,-44},{11,-36},{-7,-31},{-35,-31}},
    {{-13,-29},{-41,-28},{-69,-29},{22,-25},{8,-30},{10,-32},{4,-30},
     {-11,-37},{-26,-42},{-26,-42},{10,-34},{-8,-29},{-35,-29}},
    {{-13,-29},{-41,-28},{-69,-29},{22,-25},{7,-30},{10,-32},{5,-30},
     {-11,-37},{-26,-42},{-26,-42},{11,-37},{-7,-32},{-35,-29}},
    {{-13,-31},{-41,-30},{-69,-31},{22,-25},{7,-32},{10,-34},{5,-32},
     {-11,-39},{-26,-42},{-26,-42},{11,-36},{-7,-31},{-35,-31}},
    {{-13,-29},{-41,-28},{-69,-29},{22,-25},{6,-30},{10,-32},{6,-30},
     {-11,-37},{-26,-44},{-26,-44},{12,-34},{-6,-29},{-35,-29}},
    {{-13,-31},{-41,-30},{-69,-31},{22,-25},{7,-32},{10,-34},{5,-32},
     {-11,-39},{-26,-42},{-26,-42},{11,-36},{-7,-31},{-35,-31}},
    {{-13,-29},{-41,-28},{-69,-29},{22,-25},{8,-30},{10,-32},{4,-30},
     {-11,-37},{-26,-44},{-26,-44},{10,-34},{-8,-29},{-35,-29}},
};

/* The 0x085280B0 pointer table: the dword record 2's + 8 angle eases
 * toward (0x20000 - value when mirrored). */
static const int32_t cs_bone2_ang[13] = {
    0x15000, 0x14E00, 0x15000, 0x15200, 0x17000, 0x15000,
    0x15800, 0x15000, 0x15800, 0x17000, 0x15000, 0x15800, 0x15000,
};

static int32_t cs_div(int32_t v, int d) {
    return d ? v / d : 0;
}
/* The ROM's sign-fix >> 16: truncation toward zero, like C division. */
static int32_t cs_px(int32_t v) { return v / 0x10000; }

/* EnemyCreakingSkullUpdate 0x080953DC builds one container rectangle
 * from record 5 every frame and submits that same rectangle to
 * sub_08042848 (hurt) and sub_080428B4 (contact).  The ordinary body
 * animation deliberately has zero boxes, so using frame_boxes here
 * makes the assembled boss intangible. */
static AosBox cs_container_box(const AosEnemy *e) {
    int x = cs_px(e->cs_rec[5].x);
    if (e->mirrored) x = -x;
    x -= 0x14;
    int y = cs_px(e->cs_rec[5].y) - 0x14;
    return (AosBox){(int8_t)x, (int8_t)y, 0xC0,
                    (uint8_t)(y < 0 ? -y : y)};
}

static AosHitReport cs_container_collide(
    AosEnemy *e, AosSoma *soma, const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def) {
    bool own = e->own_boxes;
    AosBox old_hurt = e->own_hurt, old_attack = e->own_attack;
    AosBox box = cs_container_box(e);
    e->own_boxes = true;
    e->own_hurt = e->own_attack = box;
    AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                               soma_atk, soma_def);
    e->own_boxes = own;
    e->own_hurt = old_hurt;
    e->own_attack = old_attack;
    return hit;
}

/* sub_08097D84 (snap) and sub_08097EC4 (no snap): arm record idx —
 * flag 0x80000000, anchor sub, orbit radius (explicit or measured to
 * the anchor through two sub_0803E86C squares and Sqrt << 8) and the
 * + 0x10/+ 0x14 launch pair.  The angle toward the anchor reversed by
 * 0x10000 seeds ang2; a speed within 0xFFFF offsets that angle by the
 * facing sign, a larger one is an absolute target (mirrored speeds
 * cap through 0x28000/0x38000).  97D84 compares the signed speed and
 * snaps ang2 to spd; 97EC4 compares |speed| and leaves ang2. */
static void cs_config_ex(AosEnemy *e, int idx, int sub, int32_t speed,
                         int32_t dist, bool snap, bool abs_cmp) {
    AosCsRec *r = &e->cs_rec[idx];
    r->flag = 0x80000000u;
    r->sub = sub;
    if (dist) {
        r->dist = dist;
    } else {
        int32_t dx = e->cs_rec[sub].x - r->x;
        int32_t dy = e->cs_rec[sub].y - r->y;
        int64_t d2 = (int64_t)aos_fixed_mul(dx, dx) +
                     (int64_t)aos_fixed_mul(dy, dy);
        r->dist = d2 > 0 ? (int32_t)(aos_isqrt((uint32_t)d2) << 8) : 0;
    }
    int32_t dx = r->x - e->cs_rec[sub].x;
    int32_t dy = r->y - e->cs_rec[sub].y;
    int32_t base =
        (int32_t)aos_arctan2((int16_t)cs_px(dx), (int16_t)cs_px(dy)) +
        0x10000;
    r->ang2 = base;
    int32_t cmp = abs_cmp ? (speed < 0 ? -speed : speed) : speed;
    int32_t spd;
    if (cmp <= 0xFFFF) {
        spd = e->mirrored ? base - speed : base + speed;
    } else if (e->mirrored) {
        spd = speed > 0x18000 ? 0x38000 - speed : 0x28000 - speed;
    } else {
        spd = speed;
    }
    r->spd = spd;
    if (snap)
        r->ang2 = r->spd;
}

/* One sub_08097D84 record arm as the choreography machines issue
 * them: the record index, its anchor record, the signed speed
 * target and the explicit orbit radius (the machines always pass a
 * nonzero radius here, so the measured fallback is unused).  Every
 * arm ported below is a sub_08097D84 (snap ang2 to spd, signed
 * speed compare); the sub_08097EC4 arms of state-1 cases 8/18 are
 * documented but not wired (they belong to the unported bone-throw
 * attack). */
typedef struct {
    uint8_t rec, sub;
    int32_t spd, dist;
} CsArm;

/* sub_08095898 case 0: the alert re-arm on state-0 entry — records
 * {12->11, 1->0, 2->1} at distances 0x1F0000/0x1E0000/0x1C0000 with
 * speeds -0x1000/-0x800/-0x1000. */
static const CsArm cs_arms_s0_alert[] = {
    {0xC, 0xB, (int32_t)0xFFFFF000, 0x1F0000},
    {1, 0, (int32_t)0xFFFFF800, 0x1E0000},
    {2, 1, (int32_t)0xFFFFF000, 0x1C0000},
};

/* sub_08095898 case 2 -> 3 (shared tail _08096544): keep record 12
 * at -0x1000 and drive records 1/2 at -0x1800 for the next stride. */
static const CsArm cs_arms_s0_stride[] = {
    {0xC, 0xB, (int32_t)0xFFFFF000, 0x1F0000},
    {1, 0, (int32_t)0xFFFFE800, 0x1E0000},
    {2, 1, (int32_t)0xFFFFE800, 0x1C0000},
};

/* sub_08095898 case 3 first-entry tail _080966B4: one decision
 * pose is held before the distance/RNG branches, with the three
 * records accelerating forward at 0x13000/0x17000/0x1D000. */
static const CsArm cs_arms_s0_decide[] = {
    {0xC, 0xB, 0x13000, 0x1F0000},
    {1, 0, 0x17000, 0x1E0000},
    {2, 1, 0x1D000, 0x1C0000},
};

/* sub_08096A94 case 0: the state-1 entry re-arm — record 12 holds
 * (speed 0 snaps ang2 to the anchor bearing), records 1/2 orbit at
 * 0x5000/0x1000. */
static const CsArm cs_arms_s1_entry[] = {
    {0xC, 0xB, 0, 0x1F0000},
    {1, 0, 0x5000, 0x1E0000},
    {2, 1, 0x1000, 0x1C0000},
};

/* sub_08096A94 case 13 (the 0x86 windup strum): records 1/2 reverse
 * orbit at -0x5A00/-0x7200. */
static const CsArm cs_arms_s1_windup[] = {
    {1, 0, (int32_t)0xFFFFA600, 0x1E0000},
    {2, 1, (int32_t)0xFFFF8E00, 0x1C0000},
};

/* sub_08096A94 case 14 (the 0x80 volley): record 1 holds, record 2
 * eases to a slow 0x400 orbit — the same pair cases 4/5/15/16 use
 * around their projectile spawns. */
static const CsArm cs_arms_s1_volley[] = {
    {1, 0, 0, 0x1E0000},
    {2, 1, 0x400, 0x1C0000},
};

/* sub_08096A94 case 20: the return-to-state-0 reset — all three
 * records re-armed with speed 0 after the sub_08098C1C snapshot. */
static const CsArm cs_arms_s1_exit[] = {
    {0xC, 0xB, 0, 0x1F0000},
    {1, 0, 0, 0x1E0000},
    {2, 1, 0, 0x1C0000},
};

static void cs_run_arms(AosEnemy *e, const CsArm *arms, int n) {
    for (int i = 0; i < n; ++i)
        cs_config_ex(e, arms[i].rec, arms[i].sub, arms[i].spd,
                     arms[i].dist, true, false);
}

/* The + 0xD ease divider the machines load from the stacked timing
 * bytes through entity + 0x24 (0 for id 0x21, 1 for id 0x5C): state 0
 * reads the {0x20,0x18} pair at 0x08119070, state 1 the
 * {0x30,0x28,0x28} triple at 0x08119078. */
static uint8_t cs_cool_tab(int state, uint8_t id) {
    return state == 0 ? (id == 0x5C ? 0x18 : 0x20)
                      : (id == 0x5C ? 0x28 : 0x30);
}

/* sub_08097B94: one record's per-frame step.  An unlaunched record
 * (spd and ang2 both 0) eases toward its variant home pair by the + 0xD
 * divider — record 2 also eases its + 8 angle toward the variant's
 * 0x085280B0 dword (0x20000 - value when mirrored).  A launched
 * record eases ang2 toward spd and sits at cos/sin * dist around its
 * anchor — the anchor's x/y pixel-aligned when the 0x80000000 flag
 * is set, else + 0x18/+ 0x1C double as the x/y base offsets. */
static void cs_step_rec(AosEnemy *e, int idx) {
    int d = e->cs_cool;
    if (!d)
        return;
    AosCsRec *r = &e->cs_rec[idx];
    int var = e->cs_var < 13 ? e->cs_var : 0;
    if (!r->spd && !r->ang2) {
        int32_t hx = (int32_t)cs_home[var][idx][0] << 16;
        int32_t hy = (int32_t)cs_home[var][idx][1] << 16;
        if (e->mirrored)
            hx = -hx;
        r->x += cs_div(hx - r->x, d);
        r->y += cs_div(hy - r->y, d);
        if (idx == 2) {
            int32_t t = cs_bone2_ang[var];
            if (e->mirrored)
                t = 0x20000 - t;
            e->cs_rec[2].ang += cs_div(t - e->cs_rec[2].ang, d);
        }
        return;
    }
    r->ang2 += cs_div(r->spd - r->ang2, d);
    int32_t c = aos_fixed_mul(aos_sine((uint32_t)r->ang2 + 0x4000u),
                              r->dist);
    int32_t s = aos_fixed_mul(aos_sine((uint32_t)r->ang2), r->dist);
    if (r->sub >= 0 && r->sub < CS_BONES) {
        AosCsRec *anc = &e->cs_rec[r->sub];
        if (r->flag == 0x80000000u) {
            r->x = (anc->x & (int32_t)0xFFFF0000) + c;
            r->y = (anc->y & (int32_t)0xFFFF0000) + s;
        } else {
            r->x = (int32_t)r->flag + c;
            r->y = r->sub + s;
        }
    }
}

/* sub_08098BE0: step every record once in the permuted order (the
 * create's 0x08119063 table); sub_08098BC0 is the identity order —
 * both gate on the + 0xD divider through cs_cool. */
static void cs_step_perm(AosEnemy *e, const uint8_t *perm) {
    /* The cases count + 0xD down once per update and re-arm it on
     * case entry; the ported machines keep the decoded entry values
     * and let the divider decay to 1 (never 0 — a zero would freeze
     * the records where native advances the case the same frame). */
    if (e->cs_cool > 1)
        e->cs_cool--;
    for (int i = 0; i < CS_BONES; ++i)
        cs_step_rec(e, perm[i]);
}

/* sub_08098C1C/sub_08098C38: the attack machine brackets its
 * destructive record arms with a complete thirteen-record position
 * snapshot.  Only +0/+4 are copied; held angles and orbit parameters
 * deliberately survive the restore. */
static void cs_snapshot_records(AosEnemy *e) {
    for (int i = 0; i < CS_BONES; ++i) {
        e->cs_rec[i].sx = e->cs_rec[i].x;
        e->cs_rec[i].sy = e->cs_rec[i].y;
    }
}

static void cs_restore_records(AosEnemy *e) {
    for (int i = 0; i < CS_BONES; ++i) {
        e->cs_rec[i].x = e->cs_rec[i].sx;
        e->cs_rec[i].y = e->cs_rec[i].sy;
    }
}

/* sub_08097D1C: when the facing bit disagrees with sub_0806CF2C the
 * native loop negates every record x.  Its two address comparisons
 * mirror +8 (0x20000 - ang) for records 0, 1, 2, 8 and 11.  The port
 * applies that exact record transform whenever the reduced
 * choreography changes the body facing. */
static void cs_mirror_records(AosEnemy *e) {
    for (int i = 0; i < CS_BONES; ++i) {
        e->cs_rec[i].x = -e->cs_rec[i].x;
        if (i <= 2 || i == 8 || i == 11)
            e->cs_rec[i].ang = 0x20000 - e->cs_rec[i].ang;
    }
}

static void cs_face_records(AosEnemy *e, const AosSoma *soma) {
    bool was_mirrored = e->mirrored;
    face_player(e, soma);
    if (e->mirrored != was_mirrored)
        cs_mirror_records(e);
}

/* EnemyCreakingSkullCreate's record bake: home pairs for the + 0x20
 * variant (0 at create), record 2's angle dword, then the two
 * literal sub_08097D84 passes over records {12->11, 1->0, 2->1}
 * separated by a sub_08098BE0 step. */
static void cs_init_records(AosEnemy *e) {
    int var = e->cs_var < 13 ? e->cs_var : 0;
    for (int i = 0; i < CS_BONES; ++i) {
        AosCsRec *r = &e->cs_rec[i];
        r->x = (int32_t)cs_home[var][i][0] << 16;
        r->y = (int32_t)cs_home[var][i][1] << 16;
        r->ang = i == 2 ? cs_bone2_ang[var] : 0x10000;
    }
    e->cs_cool = 1; /* + 0xD = 1 at create; state 3 clears it below */
    /* Bake A: the three armed records at their launch offsets. */
    cs_config_ex(e, 0xC, 0xB, (int32_t)0xFFFFC000, 0x1F0000,
                 true, false);
    cs_config_ex(e, 1, 0, (int32_t)0xFFFFF800, 0x1E0000,
                 true, false);
    cs_config_ex(e, 2, 1, 0x6000, 0x1C0000, true, false);
    cs_step_perm(e, cs_perm_tab);
    /* Bake B: the same arms with a zero speed target. */
    cs_config_ex(e, 0xC, 0xB, 0, 0x1F0000, true, false);
    cs_config_ex(e, 1, 0, 0, 0x1E0000, true, false);
    cs_config_ex(e, 2, 1, 0, 0x1C0000, true, false);
    cs_step_perm(e, cs_perm_tab);
    /* The bake already reads the current facing in sub_08097B94 and
     * sub_08097D84.  sub_08097D1C is a change-of-facing transform,
     * not an unconditional second mirror of the baked pose. */
    if (e->state == 3)
        e->cs_cool = 0; /* the dormant branch clears + 0xD */
}

/* Emit the thirteen bone children over the first updates, at most
 * the room child cap per call like the other multipart ports.  Each
 * child takes its + 0x20 record index, + 0x21 aim target and + 0x65
 * frame from the create tables indexed by that record. */
static void cs_emit_bones(AosEnemy *e, AosHitReport *hit) {
    while (e->spawned_count < CS_BONES &&
           hit->child_count < AOS_ENEMY_MAX_CHILDREN) {
        int i = e->spawned_count++;
        int rec = cs_rec_tab[i];
        AosEnemy c;
        memset(&c, 0, sizeof c);
        c.id = e->id;
        c.role = AOS_ROLE_CS_BONE;
        c.parent = e;
        c.x = e->x;
        c.y = e->y;
        c.cs_bone = (uint8_t)i;
        c.cs_pose = (uint8_t)rec;
        c.cs_tgt = cs_tgt_tab[rec];
        c.hp = 1;
        c.static_frame = (int16_t)cs_frame_tab[rec];
        hit->children[hit->child_count++] = c;
    }
}

/* sub_0809884C: after its eight particle calls, create the linked
 * sub_080989DC entity at the literal facing-dependent x offset and
 * common -25 px y offset.  +0x48 carries an angle whose low half is
 * initially zero; the signed high half is retained for traceability. */
static void cs_fire_shot(AosEnemy *e, AosSoma *soma, AosHitReport *hit) {
    (void)soma;
    if (hit->child_count >= AOS_ENEMY_MAX_CHILDREN)
        return;
    AosEnemy c;
    memset(&c, 0, sizeof c);
    c.id = e->id;
    c.role = AOS_ROLE_CS_SHOT;
    c.parent = e;
    c.mirrored = e->mirrored;
    c.x = e->x + (e->mirrored ? 0x240000 : -0x240000);
    c.y = e->y - 0x190000;
    c.angle = e->mirrored ? 0x30000u : (uint32_t)-0x30000;
    c.static_frame = 0;
    c.hp = INT16_MAX;
    c.stats = e->stats;
    c.combat.type = 0x0A;
    hit->children[hit->child_count++] = c;
}

/* The sub_08098CF0 debris spawn of the death machine. */
static void cs_spawn_gore(AosEnemy *e, AosHitReport *hit) {
    if (hit->child_count >= AOS_ENEMY_MAX_CHILDREN)
        return;
    AosEnemy c;
    memset(&c, 0, sizeof c);
    c.id = e->id;
    c.role = AOS_ROLE_CS_GORE;
    c.parent = e;
    c.x = e->x + (int32_t)(e->cs_hits & 1 ? 0x40000 : -0x40000);
    c.y = e->y - 0x200000;
    c.vx = (int32_t)((e->cs_hits & 3) - 2) * 0x100000;
    c.vy = -0x300000;
    c.hp = 1;
    hit->children[hit->child_count++] = c;
    e->cs_hits++;
}

/* sub_0809808C state 2: the thrown bone — step 1 falls toward the
 * + 0x14 landing line while + 0x18 drifts by + 0x10, step 2 flashes
 * frame 0x17 and hops, step 3 drifts and fades out on the + 0xD
 * timer.  The sub_0806D3D8/sub_0806D518 spark helpers, the PlaySong
 * 0x7C stinger keep, the palette writes and the sub_0803E6BC slot
 * free are folded into the timer; no ported path enters this state
 * yet (documented gap). */
static void cs_bone_state2(AosEnemy *e) {
    if (!e->timer)
        e->timer = cs_death_tab[e->cs_pose < CS_BONES ? e->cs_pose : 0];
    if (e->step == 0)
        e->step = 1;
    e->mirrored = false; /* + 0x58 &= ~0x40 every state-2 update */
    if (e->step == 1) {
        if (e->timer <= 0x32)
            e->angle = (uint32_t)((int32_t)e->angle + e->spin);
        if (e->y >= e->cs_land) {
            e->y = e->cs_land;
            e->vx = e->vy = e->ax = e->ay = 0;
            e->step = 2;
        }
    } else if (e->step == 2) {
        e->step = 3;
        e->substep = 0;
        e->static_frame = 0x17;
        e->x -= 0x110000; /* the + 0x58 bit 6 was just cleared */
        e->y += 0x80000;
    } else if (e->step == 3) {
        if (!e->substep) {
            e->substep = 1;
            aos_audio_play_song(0x7C);
        }
        if (e->angle > 0xE00000u)
            e->angle -= 0x400;
        else if (e->angle <= 0x11FFFu)
            e->angle += 0x400;
    }
    e->timer = (uint8_t)(e->timer - 1);
    if ((int8_t)e->timer <= 0)
        e->removed = true;
}

/* sub_0809808C: a bone child of the + 0x1C container.  State 0 rides
 * its record — position is the parent's plus the record offset, the
 * + 0x58 bit 6 mirror and the + 0x5A palette bits fold into the
 * shared fields; records {0,1,11} aim at their + 0x21 target through
 * ArcTan2 and write the held angle back to the record's + 8, records
 * {2,8} hold it, and the Giant Skeleton's fifth record hides.  A
 * state-2 parent detaches the bone into the state-1 death fall whose
 * + 0xD timer is the 0x08119081 table; the palette flashes, spark
 * helpers and affine sprite setup are not modeled. */
static void cs_bone_update(AosEnemy *e, const AosAnimSet *anims,
                           uint32_t (*random)(void)) {
    (void)anims;
    AosEnemy *p = e->parent;
    if (e->state == 0) {
        if (!p || p->removed || !cs_is_boss(p->id) || p->id != e->id) {
            e->removed = true;
            return;
        }
        e->mirrored = p->mirrored;
        e->timer++; /* + 0xD frame counter */
        int rec = e->cs_pose < CS_BONES ? e->cs_pose : 0;
        AosCsRec *r = &p->cs_rec[rec];
        e->x = p->x + r->x;
        e->y = p->y + r->y;
        if (p->id == 0x5C && rec == 5)
            e->hidden = true; /* + 0x58 |= 0x20 */
        if (rec <= 1 || rec == 11) {
            int32_t dx = p->cs_rec[e->cs_tgt < CS_BONES ? e->cs_tgt : 0].x -
                         r->x;
            int32_t dy = p->cs_rec[e->cs_tgt < CS_BONES ? e->cs_tgt : 0].y -
                         r->y;
            int32_t ang = (int32_t)aos_arctan2((int16_t)cs_px(dx),
                                               (int16_t)cs_px(dy));
            e->vx = e->mirrored ? ang : ang - 0x8000; /* + 0x48 */
            r->ang = e->vx;
        } else if (rec == 2 || rec == 8) {
            e->vx = r->ang;
        }
        if (p->state == 2) {
            /* The native detach into the state-1 death fall. */
            e->state = 1;
            e->step = 0;
            e->substep = 0;
            e->angle = (uint32_t)e->vx; /* + 0x18 = the held + 0x48 */
            e->spin = (int32_t)((random ? random() : 0) & 0xFF) - 0x80;
            e->ax = 0;
            e->ay = 0x1800;
            e->vx = (int32_t)((random ? random() : 0) & 0x3FFF) - 0x2000;
            e->vy = 0;
            e->timer = cs_death_tab[rec];
            if (rec == 8)
                e->cs_land = p->y - 0xA0000;
        }
        return;
    }
    if (e->state == 1) {
        /* State 1: the death fall — + 0x18 drifts by + 0x10 while the
         * + 0xD timer runs out, then the bone deletes (the palette
         * fade through sub_0803E6BC is not modeled). */
        if (e->step == 0)
            e->step = 1;
        if (p && p->id == 0x5C && e->cs_pose == 5)
            e->hidden = true;
        if (e->timer <= 0x32)
            e->angle = (uint32_t)((int32_t)e->angle + e->spin);
        e->timer = (uint8_t)(e->timer - 1);
        if ((int8_t)e->timer <= 0)
            e->removed = true;
        return;
    }
    if (e->state == 2) {
        cs_bone_state2(e);
        return;
    }
    e->removed = true;
}

/* sub_080989DC: state 0 is a stationary affine hazard.  Its +0xD
 * counter advances every update and every even value adds 0x2400 to
 * the +0x18 angle.  Either native callback (sub_08098DB4 contact or
 * sub_08098DF0 weapon hit) changes to state 1 and plays song 0x76;
 * state 1 emits three sub_0806D894 plus three sub_0806D644 particles
 * and deletes immediately.  Those six distinct particle helpers are
 * still not represented by the shared effect model. */
static AosHitReport cs_shot_update(
    AosEnemy *e, AosSoma *soma, const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def, int cam_x, int cam_y) {
    AosHitReport hit;
    memset(&hit, 0, sizeof hit);
    if (e->state == 1) {
        e->removed = true;
        return hit;
    }
    if (outside_margins(kind, 1, (e->x >> 16) - cam_x,
                        (e->y >> 16) - cam_y)) {
        e->removed = true;
        return hit;
    }
    hit = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    if (hit.enemy_hit) {
        /* sub_08098DF0 does not damage the helper or emit a damage
         * number; it only starts the burst. */
        e->hp = INT16_MAX;
        e->defeated = false;
        hit.enemy_damage = 0;
        hit.has_damage_popup = false;
        hit.killed = false;
    }
    if (hit.soma_hit || hit.enemy_hit) {
        e->state = 1;
        aos_audio_play_song(0x76);
        return hit;
    }
    if ((++e->timer & 1) == 0)
        e->angle += 0x2400;
    return hit;
}

/* Gore debris flies on its launch velocity and lands. */
static void cs_gore_update(AosEnemy *e) {
    AosEnemy *p = e->parent;
    if (!p || p->state == 2 || p->removed || ++e->timer > 0xA0) {
        if (e->timer > 0xA0)
            e->removed = true;
        return;
    }
    e->vy += 0x8000;
    integrate(e);
}

/* sub_08095898 state 0: the native multi-case approach and strike
 * machine. This translated prefix covers alert, stride and decision;
 * cases 4-12/20-24 remain explicitly open. */
static void cs_state0(AosEnemy *e, AosSoma *soma,
                      const AosCollision *layer, const AosAnimSet *anims,
                      uint32_t (*random)(void)) {
    switch (e->step) {
    case 0:
        /* Native case 0: aggro, face, enter case 2 and seed the
         * sub_0806D288 stride.  The x speed is +/-0x190000 divided
         * by (2 * entry_timer - 4); +0x4C/+0x54 become
         * 0x10000/0x2800 at the shared _08096A72 tail. */
        if (player_near(soma, e->x >> 16, e->y >> 16, 0xC0, 0x80)) {
            cs_face_records(e, soma);
            play(e, anims, 1);
            e->step = 2;
            e->timer = cs_cool_tab(0, e->id);
            cs_run_arms(e, cs_arms_s0_alert,
                        (int)(sizeof cs_arms_s0_alert /
                             sizeof cs_arms_s0_alert[0]));
            e->cs_cool = e->timer;
            int divisor = (int)e->timer * 2 - 4;
            e->vx = (e->mirrored ? 0x190000 : -0x190000) / divisor;
            e->vy = 0x10000;
            e->ax = 0;
            e->ay = 0x2800;
        }
        break;
    case 1:
    case 2:
        /* Native cases 1/2 both step records then run
         * sub_0806D288(0x10000, -0x300000).  Particle emission and
         * the secondary +0x19 animation countdown remain open. */
        (void)projectile_collide(e, layer, 0x10000,
                                 (int32_t)0xFFD00000);
        if (e->timer && --e->timer == 0) {
            e->step = 3;
            e->timer = cs_cool_tab(0, e->id);
            cs_run_arms(e, cs_arms_s0_stride,
                        (int)(sizeof cs_arms_s0_stride /
                             sizeof cs_arms_s0_stride[0]));
            e->cs_cool = e->timer;
        }
        break;
    case 3:
        /* Native case 3 first holds _080966B4's record pose for one
         * tick.  Its following decision can enter state 1 or the
         * still-unported state-0 locomotion cases 4, 7 and 10. */
        if (e->timer && --e->timer)
            break;
        if (e->substep == 0) {
            e->substep = 1;
            e->timer = 1;
            cs_run_arms(e, cs_arms_s0_decide,
                        (int)(sizeof cs_arms_s0_decide /
                             sizeof cs_arms_s0_decide[0]));
            e->cs_cool = 1;
            break;
        }

        /* _08095C5E gives Creaking Skull a 1/4 RNG2 route to native
         * state-1 case 20; the reduced state-1 controller cannot yet
         * distinguish that projectile schedule, so both native
         * state-1 entries start its existing bounded attack.  The
         * next close-range roll is the <=0x3F branch at _08095C94. */
        bool attack = false;
        if (e->id == 0x21 && random && (random() & 0x7F) <= 0x1F)
            attack = true;
        if (!attack && soma) {
            int dx = (int16_t)((target_x(soma) >> 16) - (e->x >> 16));
            if (dx < 0)
                dx = -dx;
            if (dx <= 0x63 && random && (random() & 0x7F) <= 0x3F)
                attack = true;
        }
        /* Cases 4/7/10 are not represented yet.  Preserve the old
         * bounded attack fallback for those outcomes, but keep this
         * divergence explicit rather than claiming their branch and
         * collision choreography was ported. */
        (void)attack;
        cs_snapshot_records(e);
        e->state = 1;
        e->step = 0;
        e->substep = 0;
        e->cs_hits = 0;
        cs_run_arms(e, cs_arms_s1_entry,
                    (int)(sizeof cs_arms_s1_entry /
                         sizeof cs_arms_s1_entry[0]));
        e->cs_cool = cs_cool_tab(1, e->id);
        (void)random;
        break;
    default:
        e->step = 1;
        break;
    }
}

/* sub_08096A94 state 1: the attack machine — windup, the shot
 * volleys with their songs (0x86 windup, 0x80/0x84/0x76/0x77/0x7A/
 * 0x97/0xE0/0x1C/0x29 stingers), then back to state 0. */
static void cs_state1(AosEnemy *e, AosSoma *soma,
                      const AosAnimSet *anims, AosHitReport *hit,
                      uint32_t (*random)(void)) {
    (void)random;
    switch (e->substep) {
    case 0:
        play(e, anims, 2);
        aos_audio_play_song(0x86);
        e->substep = 1;
        e->timer = 0;
        /* The 0x86 windup strum is native case 13: records 1/2
         * reverse-orbit while + 0xD counts down from the 2 the
         * preceding case assigned. */
        cs_run_arms(e, cs_arms_s1_windup,
                    (int)(sizeof cs_arms_s1_windup /
                         sizeof cs_arms_s1_windup[0]));
        e->cs_cool = 2;
        break;
    case 1:
        /* Volley: the case-14 re-arm on entry (the + 0xD = 0xE the
         * windup case assigned), two shots then the 0x80 stinger. */
        if (e->timer == 0) {
            cs_run_arms(e, cs_arms_s1_volley,
                        (int)(sizeof cs_arms_s1_volley /
                             sizeof cs_arms_s1_volley[0]));
            e->cs_cool = 0xE;
        }
        if (e->timer == 0x10 || e->timer == 0x28) {
            cs_fire_shot(e, soma, hit);
            if (e->timer == 0x28)
                aos_audio_play_song(0x80);
        }
        if (++e->timer > 0x40) {
            e->substep = 2;
            e->timer = 0;
        }
        break;
    case 2:
        /* Return to the approach — the case-20 reset re-arm with
         * the table + 0xD divider.  Native terminal cases 9/27/37
         * first call sub_08098C38, undoing positional drift from the
         * attack while retaining the new orbit parameters. */
        cs_restore_records(e);
        e->state = 0;
        e->step = 1;
        e->substep = 0;
        cs_run_arms(e, cs_arms_s1_exit,
                    (int)(sizeof cs_arms_s1_exit /
                         sizeof cs_arms_s1_exit[0]));
        e->cs_cool = cs_cool_tab(1, e->id);
        play_loop(e, anims, 0, true);
        break;
    default:
        e->substep = 0;
        break;
    }
    if (soma)
        cs_face_records(e, soma);
}

AosHitReport aos_enemy_creaking_skull_update(
    AosEnemy *e, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def, int cam_x, int cam_y,
    uint32_t (*random)(void)) {
    const AosAnimSet *anims = kind->anims;
    AosHitReport hit;
    memset(&hit, 0, sizeof hit);
    (void)soma_atk; (void)soma_def; (void)cam_x; (void)cam_y;

    /* Children dispatch on their roles. */
    if (e->role == AOS_ROLE_CS_BONE) {
        cs_bone_update(e, anims, random);
        /* sub_08098C54/sub_08098C84 make every live bone part of the
         * container's interaction surface.  The weapon callback saves
         * the parent +0x40/+0x44, temporarily substitutes the bone
         * position, runs the ordinary enemy damage routine on the
         * parent, then restores the body position.  Do the same here:
         * the parent's combat cooldowns and HP remain authoritative,
         * while hit sparks and contact damage originate at this bone. */
        if (!e->removed && e->state == 0 && e->parent &&
            cs_is_boss(e->parent->id) && !e->parent->removed &&
            e->parent->state != 2) {
            AosEnemy *parent = e->parent;
            int32_t px = parent->x, py = parent->y;
            parent->x = e->x;
            parent->y = e->y;
            hit = collide(parent, soma, kind, weapon, weapon_frames,
                          soma_atk, soma_def);
            parent->x = px;
            parent->y = py;
            if (hit.enemy_hit)
                hit.owner_forwarded = true;
        }
        return hit;
    }
    if (e->role == AOS_ROLE_CS_SHOT) {
        return cs_shot_update(e, soma, kind, weapon, weapon_frames,
                              soma_atk, soma_def, cam_x, cam_y);
    }
    if (e->role == AOS_ROLE_CS_GORE) {
        cs_gore_update(e);
        return hit;
    }
    if (!cs_is_boss(e->id))
        return hit;

    /* The create bakes the +0x1C record container before the first
     * update.  Do this before collision too: the custom native box is
     * derived from record 5 and cannot be replaced by the body's empty
     * animation-frame boxes. */
    if (!e->cs_init) {
        cs_init_records(e);
        e->cs_init = true;
    }

    /* EnemyCreakingSkullUpdate prologue: state 2 skips the activity
     * gates; the probe, collision and the container hit box fold
     * into the shared collide. */
    if (e->state != 2) {
        if (outside_margins(kind, 6, (e->x >> 16) - cam_x,
                            (e->y >> 16) - cam_y)) {
            e->hidden = true;
            return hit;
        }
        hit = cs_container_collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
    }
    /* EnemyCreakingSkullUpdate does not hide the body here.  The native
     * tail at 0x080953DC builds the container hit boxes and submits them;
     * 0x44000 was an erroneous fixed-point transcription that hid every
     * normally positioned boss (176 << 16 already exceeds it). */
    e->hidden = false;

    /* The create's record bake runs once, then the thirteen bone
     * children emit on the first updates. */
    if (!e->boss_parts_spawned) {
        cs_emit_bones(e, &hit);
        if (e->spawned_count >= CS_BONES)
            e->boss_parts_spawned = true;
    }

    switch (e->state) {
    case 0:
        cs_state0(e, soma, layer, anims, random);
        break;
    case 1:
        cs_state1(e, soma, anims, &hit, random);
        break;
    case 2:
        /* The native state-2 tail: + 0xD countdown, + 0x59 |= 8 and
         * the container delete through sub_0806C250 — the shared
         * death machine below covers it; gore debris every few
         * ticks approximates the sub_08098CF0 spawns. */
        if ((e->timer & 3) == 0)
            cs_spawn_gore(e, &hit);
        if (e->timer)
            e->timer--;
        else
            e->removed = true;
        break;
    default:
        /* State 3 dormant (sub_08097A14). */
        break;
    }

    /* The state machines step the container records through
     * sub_08098BE0 in their cases; the ported machines keep the case
     * order and fire the decoded entry/windup/volley/exit re-arms,
     * so the records take one sub_08098BE0 pass per update outside
     * the death state with the + 0xD divider counting down. */
    if (e->state != 2)
        cs_step_perm(e, cs_perm_tab);

    /* Lethal: hp 0 enters the state-2 death machine (the native
     * hurt callbacks sub_08098E74/sub_08098EA4 fold into the
     * shared hurt pass). */
    if (!e->defeated && e->hp <= 0 && e->state != 2) {
        e->defeated = true;
        aos_audio_play_song(0x6B);
        e->state = 2;
        e->step = e->substep = 0;
        e->timer = 0x3C;
        e->cs_hits = 0;
    }

    step_anim(e, anims);
    return hit;
}
