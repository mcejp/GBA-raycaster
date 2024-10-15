#include "game.hpp"

static int DistSquared(Vec2i const a, Vec2i const b) {
    int dx = b.x - a.x,
            dy = b.y - a.y;
    return dx * dx + dy * dy;
}

void entThink(Entity& ent) {
    if (g_freezeai || ent.state == ANIM_DEAD) {
        return;
    }

    // Trace line towards player
    auto res = Trace(*currmap, ent.x, ent.y, x_plr, y_plr);

    if (res == TRACE_SUCCESS) {
        ent.see_player = 1;
    }
    else {
        ent.see_player = 0;
    }

    int const attk_range = MAP_TILE_SIZE * 8 / 8;
    bool in_attk_range = ent.see_player &&
                         DistSquared({ent.x, ent.y}, {x_plr, y_plr}) <= attk_range * attk_range;

    if (ent.attk_cooldown > 0) {
        ent.attk_cooldown--;
    }

    switch (ent.ai_state) {
        case AI_IDLE:
            if (ent.see_player) {
                // start chasing player
                ent.ai_state = AI_PRST;
                ent.StartAnim(ANIM_WALK);
            }
            break;
        case AI_PRST:
            if (!ent.see_player) {
                // LoS lost
                ent.ai_state = AI_IDLE;
                ent.StartAnim(ANIM_IDLE);
            }
                // close enough?
            else if (in_attk_range) {
                ent.ai_state = AI_ATTK;
            }
                // chase
            else {
                int const run_speed = MAP_TILE_SIZE / 30;
                int dist = Sqrt(DistSquared({ent.x, ent.y}, {x_plr, y_plr}));

                int dx = DivSafe((x_plr - ent.x) * run_speed, dist),
                        dy = DivSafe((y_plr - ent.y) * run_speed, dist);      // TODO: this or GCC division?

                ent.x += dx;
                ent.y += dy;
            }
            break;

        case AI_ATTK:
            if (!in_attk_range) {
                ent.ai_state = AI_PRST;
            }
            // in attack range
            else {
                // ready to attack?
                if ((ent.state == ANIM_IDLE || ent.state == ANIM_WALK) && ent.attk_cooldown == 0) {     // this smells..
                    ent.attk_cooldown = 30;

                    ent.StartAnim(ANIM_ATTK);
                }
                else if (ent.state == ANIM_ATTK && ent.frame == 0 && ent.sub_frame == Entity::SUBFRAMES_PER_FRAME - 1) {
                    frames_since_hit = 0;   // should re-check if still in range
                    hitpoints_plr = max(hitpoints_plr - 25, 0);
                }
                // finished attacking?
                else if (ent.state == ANIM_ATTK && ent.frame == 1 && ent.sub_frame == Entity::SUBFRAMES_PER_FRAME - 1) {
                    ent.StartAnim(ANIM_IDLE);
                }
            }
            break;
    }
}
