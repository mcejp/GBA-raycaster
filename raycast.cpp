// Note: wolf3d: https://github.com/id-Software/wolf3d/blob/master/WOLFSRC/WL_DR_A.ASM

#include "game.hpp"

#include <tonc.h>

#include <limits.h>

#include "spr_bugbear.h"
#include "spr_spider.h"
#include "tea_fantasy.h"

#define MAP map.tiles
#define MAP_W map.w
#define MAP_H map.h

// 2pi = 0x10000

enum {
    HFOV = 90 * 0x10000 / 360,
    // tan(90/2) * 256
    TAN_HALF_HFOV = 256,        // convenient...
    //VFOV = 60 * 0x10000 / 360,
    // tan(60/2) * 256
    TAN_HALF_VFOV = 148,

    GROUND_COLOR = DB16_DARKVIOLET,
    SKY_COLOR = DB16_SKY,
};

static const Sprite* sprite_images[] = {&spr_bugbear, &spr_spider };

static int frame_cnt;

// 16-byte alignment shaves off a few cycles in the hot path
struct alignas(16) RayInfo {
    // Intersection point in world coordinates
    uint16_t x_hit, y_hit;

    // This should be good for up to ~255 map tiles
    uint16_t wall_dist;

    // Map-tile coordinates
//    uint8_t x_map, y_map;
    // It seems ~1 % faster to store this directly
    // (since we already have it at hand when we detect the hit)
    uint8_t tex_id;

    // U-coordinates (texture column)
    uint8_t u[2];
};

[[gnu::section(".ewram")]]
static RayInfo rays[M4_WIDTH / 2];

struct VisibleSprite {
    uint16_t dist;
    int8_t entity;
    uint8_t min_x, max_x;   // always even, matching the 16-bit framebuffer access
    // moreover, max_x is *inclusive*
};

enum { MAX_VISIBLE_SPRITES = 4 };
VisibleSprite visible_sprites[MAX_VISIBLE_SPRITES];
uint8_t num_visible_sprites;

// hitscan results
Vec2i hitscan_pos;
int8_t hitscan_spr_index;     // -1 if none

static void UpdateSprites(Map const& map, Entity* entities, int proj_cos, int proj_sin);

// This must be compiled in ARM mode, otherwise register starvation (presumably) kills perf by 4-5x
[[gnu::section(".iwram.rRayCast")]]
[[gnu::target("arm")]]
void rRayCast(Map const& map, Entity* entities) {
    frame_cnt++;

    int proj_cos = lu_cos(dir_plr + 0*40'000),
        proj_sin = lu_sin(dir_plr + 0*40'000);

    // TODO: doesn't really belong here
    UpdateSprites(map, entities, proj_cos, proj_sin);

    num_visible_sprites = 0;

    for (int x = 0; x < M4_WIDTH; x += 2) {
        // cast ray starting from player pos
        // TODO: can we somehow exploit the fact that in most cases we will hit the same wall as in the previous step?

        // first, compute angle based on HFOV
        // linear map: for X = 0 .. M4_WIDTH
        //               dir = dir_plr + HFOV_DEG / 2 .. dir_plr - HFOV_DEG / 2
        int dir = dir_plr + HFOV / 2 - HFOV * x / M4_WIDTH;
        int dir_plus_1 = dir_plr + HFOV / 2 - HFOV * (x + 1) / M4_WIDTH;
        // lu_sin/cos return .12 fixed-point format, but we scale down to 1 step = 0x100 (2**8)
        // TODO: optimize step size
        int dx = lu_cos(dir) / 16,
            dy = lu_sin(dir) / 16;
        int xx, yy;

        // search for tile where the ray intersects a wall
        // if player position can be negative, it is important that this rounds towards -inf
        int x_map, y_map;
        x_map = (x_plr >> MAP_TILE_SIZE_LOG2);
        y_map = (y_plr >> MAP_TILE_SIZE_LOG2);

        // we use a pair variables, x_step & y_step to track how far we have come from the camera
        // in each step, one will be adjusted to reach the next closest tile edge

        // x_step sign follows that of dx
        int x_step = x_map * MAP_TILE_SIZE - x_plr;
        if (dx >= 0) {
            x_step += MAP_TILE_SIZE;
        }

        // dtto for y_step
        int y_step = y_map * MAP_TILE_SIZE - y_plr;
        if (dy >= 0) {
            y_step += MAP_TILE_SIZE;
        }

        // ARM mode, so plenty of registers
        RayInfo& ray = rays[x / 2];

        for (;;) {
            // TODO: ABS is probably no good for performance
            if (ABS(x_step * dy) < ABS(y_step * dx)) {
                // closest crossing is X
                if (dx > 0) {
                    x_map++;

                    if (x_map >= MAP_W) {
                        break;
                    }
                }
                else {
                    x_map--;

                    if (x_map < 0) {
                        break;
                    }
                }

                // opt: if all levels are enclosed, could drop the bounds checking
                if (x_map >= 0 && x_map < MAP_W && y_map >= 0 && y_map < MAP_H && MAP[y_map * MAP_W + x_map] > 0) {
                    // wall found; compute Y coordinate at crossing
                    xx = x_plr + x_step;
                    yy = y_plr + x_step * lu_dy_dx[(dir >> 7) & 0x1FF] / (1<<12);

                    ray.u[0] = yy % MAP_TILE_SIZE;
                    int yy2 = y_plr + x_step * lu_dy_dx[(dir_plus_1 >> 7) & 0x1FF] / (1<<12);
                    ray.u[1] = yy2 % MAP_TILE_SIZE;

                    ray.tex_id = MAP[y_map * MAP_W + x_map];  // note: already loaded at this point
                    break;
                }
                else {
                    x_step += MAP_TILE_SIZE * SGN(dx);
                }
            }
            else {
                // closest crossing is Y
                if (dy > 0) {
                    y_map++;

                    if (y_map >= MAP_H) {
                        break;
                    }
                }
                else {
                    y_map--;

                    if (y_map < 0) {
                        break;
                    }
                }

                if (x_map >= 0 && x_map < MAP_W && y_map >= 0 && y_map < MAP_H && MAP[y_map * MAP_W + x_map] > 0) {
                    // wall found; compute X coordinate at crossing
                    yy = y_plr + y_step;
                    xx = x_plr + y_step * lu_dx_dy[(dir >> 7) & 0x1FF] / (1<<12);

                    ray.u[0] = xx % MAP_TILE_SIZE;
                    int xx2 = x_plr + y_step * lu_dx_dy[(dir_plus_1 >> 7) & 0x1FF] / (1<<12);
                    ray.u[1] = xx2 % MAP_TILE_SIZE;

                    ray.tex_id = MAP[y_map * MAP_W + x_map];  // note: already loaded at this point
                    break;
                }
                else {
                    y_step += MAP_TILE_SIZE * SGN(dy);
                }
            }
        }

        ray.x_hit = xx;
        ray.y_hit = yy;

        // compute distance between point of intersection and camera plane
        // (project the intersection into camera space)
        ray.wall_dist = (proj_cos * (xx - x_plr) + proj_sin * (yy - y_plr)) >> 12;
    }
}

[[gnu::section(".iwram.rDrawWalls")]]
void rDrawWalls(int which, Map const& map, Entity* entities) {
    for (int x = 0; x < M4_WIDTH; x += 2) {
        // keeping a reference is marginally faster than copying it to stack
        auto const& ray = rays[x / 2];

        // compute FOV height at distance (h/dist = tan(VFOV) => h = dist * tan(VFOV))
        int fov_height = 2 * TAN_HALF_VFOV * (ray.wall_dist) / FIX_SCALE;

        auto wall_height = MAP_TILE_SIZE * M4_HEIGHT / fov_height;

        auto* fb = which == 0 ? (uint16_t*) MEM_VRAM_FRONT + x / 2 : (uint16_t*) MEM_VRAM_BACK + x / 2;

        if (wall_height > M4_HEIGHT) {
            wall_height = M4_HEIGHT;
        }

        int y_top = M4_HEIGHT / 2 - wall_height / 2;
        int y_bot = M4_HEIGHT / 2 + wall_height / 2;
        int y = 0;

        // Paint sky, wall, floor
        // TODO: could/should this be done with DMA?
        for (; y < y_top; y++) {
            *fb = SKY_COLOR * 0x0101;
            fb += M4_WIDTH / 2;
        }
        // Solid fill
//        for (; y < y_bot; y++) {
//            *fb = clr * 0x0101;
//            fb += M4_WIDTH / 2;
//        }

        // select texture
//        auto tex_id = MAP[ray.y_map * MAP_W + ray.x_map];
        auto tex_id = ray.tex_id;
        auto the_texture = tea_fantasy_textures[tex_id];

        auto tex_col1 = &the_texture[(ray.u[0] * TEX_WIDTH / MAP_TILE_SIZE) * TEX_HEIGHT];
        auto tex_col2 = &the_texture[(ray.u[1] * TEX_WIDTH / MAP_TILE_SIZE) * TEX_HEIGHT];

        kernelScale2Col32(tex_col1, tex_col2, fb, y_bot - y_top);
        fb += (y_bot - y_top) * M4_WIDTH / 2;

        for (y = y_bot; y < M4_HEIGHT; y++) {
            *fb = GROUND_COLOR * 0x0101;
            fb += M4_WIDTH / 2;
        }

        // so far, all that we're hitting is a wall
        if (x == M4_WIDTH / 2) {
            hitscan_pos = {ray.x_hit, ray.y_hit};
            hitscan_spr_index = -1;
        }

        // sort visible sprites by distance, closest first
        for (int i = 0; i < map.num_sprites; i++) {
            if (x < entities[i].scr_x1 || x >= entities[i].scr_x2) {
                continue;
            }

            int dist = entities[i].y_cam;

            if (dist > ray.wall_dist) {
                continue;
            }

            // place in the list
            // iterate, and stop as soon as we find a sprite that is farther -- we insert before it.
            int vs_index;
            for (vs_index = 0; vs_index < num_visible_sprites; vs_index++) {
                // TODO: avoid having to discover this by linear search
                if (visible_sprites[vs_index].entity == i) {
                    break;
                }

                if (visible_sprites[vs_index].dist > dist) {
                    // shift any farther sprites
                    // TODO: optimized memmove
                    for (int j = vs_index; j < num_visible_sprites && j + 1 < MAX_VISIBLE_SPRITES; j++) {
                        visible_sprites[j + 1] = visible_sprites[j];
                    }

                    break;
                }
            }

            if (vs_index == MAX_VISIBLE_SPRITES) {
                // we're full already, better luck next time
                continue;
            }

            // case 1: we found it already there (i < num && entity ==)
            // case 2: we're inserting (i < num but entity !=)
            // case 3: we're appending (i == num)

            auto& vs = visible_sprites[vs_index];

            if (vs_index == num_visible_sprites || vs.entity != i) {
                // appending or inserting -- initialize VisibleSprite
                vs.dist = dist;
                vs.entity = i;
                vs.min_x = x;

                // if we're already at max, farthest sprite will be discraded
                if (num_visible_sprites < MAX_VISIBLE_SPRITES) {
                    num_visible_sprites++;
                }
            }

            vs.max_x = x;
        }
    }
}

[[gnu::section(".iwram.rDrawSprites")]]
void rDrawSprites(int which, Entity* entities) {
    // draw sprites back-to-front
    // TODO: overdraw could be reduced by the use of clipping
    for (int vs_index = num_visible_sprites - 1; vs_index >= 0; vs_index--) {
        auto& vs = visible_sprites[vs_index];
        auto& spr = entities[vs.entity];
        auto const dist = spr.y_cam;
//        if (vs.entity == 1)
//            auxv[0] = dist;

        // compute FOV height at distance (h/dist = tan(VFOV) => h = dist * tan(VFOV))
        // sin/cos go up to 0x1000, dist is in units of MAP_TILE_SIZE,
        // so this overflows easily if we don't drop some bits
        // btw this is constant per sprite (TODO avoid recomputing maybe)
        int const fov_height = 2 * TAN_HALF_VFOV * dist / FIX_SCALE;

        // TODO: optimize division -- reciprocal distance already calculated earlier
        int y_top = M4_HEIGHT / 2 + (MAP_TILE_SIZE / 2 - spr.height) * M4_HEIGHT / fov_height;
        int y_bot = M4_HEIGHT / 2 + MAP_TILE_SIZE / 2 * M4_HEIGHT / fov_height;
        int const dv_dy = SPRITE_HEIGHT * 0x10000 / (y_bot - y_top);  // v-coordinate increment per scanline (2**16 ~ 1px)

        int v_offset_subpix = 0;        // 2**16 ~ 1 px

        if (y_top < 0) {
            v_offset_subpix = (-y_top) * dv_dy;
            y_top = 0;
        }
        if (y_bot > M4_HEIGHT) {
            y_bot = M4_HEIGHT;
        }

        auto const spans = sprite_images[spr.class_]->anims[spr.state].frames[spr.frame].spans;

        for (int x = vs.min_x; x <= vs.max_x; x += 2) {
            // sprite mapping coordinates
            // U ~ image X (left to right)
            // V ~ image Y (top to bottom)

            // texture u-coordinate in pixels
            auto u1 = (x - spr.scr_x1) * SPRITE_WIDTH / (spr.scr_x2 - spr.scr_x1);
            auto u2 = (x + 1 - spr.scr_x1) * SPRITE_WIDTH / (spr.scr_x2 - spr.scr_x1);
            // FIXME: is there any assurance that u2 doesn't not overflow??

            // Also note that often, span1 == span2, especially as we get up close to low-resolution sprites
            // This feels like big potential for optimization

            auto& span1 = spans[u1];
            auto& span2 = spans[u2];

            if (!span1.data && !span2.data) {
                continue;
            }

            // hitscan: we want the closest sprite that has a span crossing SCREEN_HEIGHT/2 in this column (ideally)
            // as an approximation it's enough to have a span at all
            // TODO: skip corpses
            if (x == M4_WIDTH / 2 /* && span1.start <= ... ? */) {
                hitscan_pos = {spr.x, spr.y};  // approximate -- we don't compute world-space x,y of the hit
                hitscan_spr_index = vs.entity;
            }

            auto* fb = which == 0 ? (uint16_t*) MEM_VRAM_FRONT + x / 2 : (uint16_t*) MEM_VRAM_BACK + x / 2;
            fb += y_top * M4_WIDTH / 2;

            if (y_bot - y_top < SPRITE_HEIGHT) {
                // "short" case (source image minified)

                for (int y = y_top; y < y_bot; y++) {
                    // texture v-coordinate (top to bottom)
                    auto v1 = (v_offset_subpix + (y - y_top) * dv_dy) / 0x10000;
                    auto v2 = v1;

                    v1 -= span1.start;
                    v2 -= span2.start;

                    uint8_t col1, col2;

                    // TODO: are there gains to be made by using 16-/32-bit loads of span data?

                    if (span1.data && v1 >= 0 && v1 < span1.count && span1.data[v1] != 0) {
                        col1 = span1.data[v1];
                    }
                    else {
                        col1 = *fb;
                    }

                    if (span2.data && v2 >= 0 && v2 < span2.count && span2.data[v2] != 0) {
                        col2 = span2.data[v2];
                    }
                    else {
                        // Note: in some cases we will needlessly load from framebuffer twice
                        col2 = *fb >> 8;
                    }

                    *fb = col1 | col2 << 8;

    //                *fb = u; //spr.color;
                    fb += M4_WIDTH / 2;
                }
            }
            else {
                // "tall" case (source image magnified)

                // texture v-coordinate (top to bottom) in integer pixels
                auto v1 = (v_offset_subpix + (y_top - y_top) * dv_dy) / 0x10000;
                auto v2 = v1;

                v1 -= span1.start;
                v2 -= span2.start;

                // texture v-coordinate subpixel, 2**16 ~ 1 px
                int v_frac_part = (v_offset_subpix + (y_top - y_top) * dv_dy) % 0x10000;

                // TODO: reduce y_bot if spans end earlier?
                for (int y = y_top; y < y_bot; ) {
                    int col;

                    // TODO: are there gains to be made by using 16-/32-bit loads of span data?

                    // TODO: are there gains to be made by splitting this into multiple loops
                    //       during which have_left & have_right are invariant?
                    bool have_left = span1.data && v1 >= 0 && v1 < span1.count && span1.data[v1] != 0;
                    bool have_right = span2.data && v2 >= 0 && v2 < span2.count && span2.data[v2] != 0;

                    if (have_left && have_right) {
                        col = span1.data[v1] | span2.data[v2] << 8;

                        // this is the critical path and would likely benefit from partial unrolling
                        for (; v_frac_part < 0x10000; v_frac_part += dv_dy) {
                            *fb = col;
                            fb += M4_WIDTH / 2;
                            y++;

                            if (y >= y_bot) {
                                break;
                            }
                        }
                    }
                    else if (have_left) {
                        col = span1.data[v1];

                        for (; v_frac_part < 0x10000; v_frac_part += dv_dy) {
                            *fb = (*fb & 0xff00) | col;
                            fb += M4_WIDTH / 2;
                            y++;

                            if (y >= y_bot) {
                                break;
                            }
                        }
                    }
                    else if (have_right) {
                        col = span2.data[v2] << 8;

                        for (; v_frac_part < 0x10000; v_frac_part += dv_dy) {
                            *fb = col | (*fb & 0xff);
                            fb += M4_WIDTH / 2;
                            y++;

                            if (y >= y_bot) {
                                break;
                            }
                        }
                    }
                    else {
                        for (; v_frac_part < 0x10000; v_frac_part += dv_dy) {
                            fb += M4_WIDTH / 2;
                            y++;

                            if (y >= y_bot) {
                                break;
                            }
                        }
                    }

                    v_frac_part -= 0x10000;
                    v1++;
                    v2++;
                }
            }
        }
    }

    // crosshair
    // (could also be an OBJ)
    auto fb = which == 0 ? (uint16_t*) MEM_VRAM_FRONT : (uint16_t*) MEM_VRAM_BACK;
    fb[(M4_WIDTH * M4_HEIGHT + M4_WIDTH) / 4] = DB16_WHITE * 0x0101;
}

void rDrawDebugOverlay(Map const& map, Entity* entities) {
#define PROJECT_X(x_world) (XOFF + (x_world) * SCALE / MAP_TILE_SIZE)
#define PROJECT_Y(y_world) (YOFF + MAP_H * SCALE - (y_world) * SCALE / MAP_TILE_SIZE - 1)

    int SCALE = 4;
    int XOFF = 0;
    int YOFF = M4_HEIGHT - MAP_H * SCALE;
    int COL = DB16_WHITE;
    // mini map
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {

            int clr = MAP[y * MAP_W + x];

            sbmp8_rect(&m4_surface,
                       XOFF + x * SCALE, YOFF + (MAP_H - y - 1) * SCALE,
                       XOFF + (x + 1) * SCALE, YOFF + (MAP_H - y) * SCALE, clr);
        }
    }
    // tracers
    for (int i = 0; i < map.num_sprites; i++) {
        if (entities[i].state == ANIM_DEAD) {
            continue;
        }

        int x_spr = entities[i].x;
        int y_spr = entities[i].y;

        sbmp8_line(&m4_surface,
                   PROJECT_X(x_spr),
                   PROJECT_Y(y_spr),
                   PROJECT_X(x_plr),
                   PROJECT_Y(y_plr),
                   entities[i].see_player ? DB16_GREEN : DB16_DARKGREY);
    }
    // intersection points
    for (int x = 0; x < M4_WIDTH; x += 2) {
        auto const& ray = rays[x / 2];
        if (ray.x_hit == 0 && ray.y_hit == 0) { continue; }
        sbmp8_plot(&m4_surface,
                   PROJECT_X(ray.x_hit),
                   PROJECT_Y(ray.y_hit),
                   COL);
    }
    // player
    if (x_plr >= 0 && y_plr >= 0 && x_plr < MAP_W * MAP_TILE_SIZE && y_plr < MAP_H * MAP_TILE_SIZE) {
        sbmp8_plot(&m4_surface,
                   PROJECT_X(x_plr),
                   PROJECT_Y(y_plr),
                   COL);
    }
    // sprites
    for (int i = 0; i < map.num_sprites; i++) {
        int x_spr = entities[i].x;
        int y_spr = entities[i].y;

        if (x_spr >= 0 && y_spr >= 0 && x_spr < MAP_W * MAP_TILE_SIZE && y_spr < MAP_H * MAP_TILE_SIZE) {
            sbmp8_plot(&m4_surface,
                       PROJECT_X(x_spr),
                       PROJECT_Y(y_spr),
                       DB16_RED);
        }
    }
    // hitscan
    sbmp8_plot(&m4_surface,
               PROJECT_X(hitscan_pos.x),
               PROJECT_Y(hitscan_pos.y),
               DB16_ORANGE);
    // finally, add border
    sbmp8_frame(&m4_surface, XOFF, YOFF, XOFF + MAP_W * SCALE, YOFF + MAP_H * SCALE, COL);
}

bool get_hitscan(int& sprite_index_out) {
    if (hitscan_spr_index >= 0) {
        sprite_index_out = hitscan_spr_index;
        return true;
    }
    else {
        return false;
    }
}

int Trace(Map const& map, int x1, int y1, int x2, int y2) {
    // This is just a distillation of the Raycast function

    int dx = x2 - x1,
        dy = y2 - y1;

    // search for tile where the ray intersects a wall
    int x_map, y_map;
    x_map = (x1 >> MAP_TILE_SIZE_LOG2);
    y_map = (y1 >> MAP_TILE_SIZE_LOG2);

    // x_step sign follows that of dx
    int x_step = x_map * MAP_TILE_SIZE - x1;
    if (dx >= 0) {
        x_step += MAP_TILE_SIZE;
    }

    // dtto for y_step
    int y_step = y_map * MAP_TILE_SIZE - y1;
    if (dy >= 0) {
        y_step += MAP_TILE_SIZE;
    }

    for (;;) {
        if (ABS(x_step * dy) < ABS(y_step * dx)) {
            // closest crossing is X
            if (dx > 0) {
                if (x1 + x_step >= x2) {
                    // read this as "by the time we reach the next wall, we're past the destination"
                    return TRACE_SUCCESS;
                }

                x_map++;
            }
            else {
                if (x1 + x_step <= x2) {
                    return TRACE_SUCCESS;
                }

                x_map--;
            }

            if (x_map >= 0 && x_map < MAP_W && y_map >= 0 && y_map < MAP_H && MAP[y_map * MAP_W + x_map] > 0) {
                return TRACE_WALL;
            }
            else {
                x_step += MAP_TILE_SIZE * SGN(dx);
            }
        }
        else {
            // closest crossing is Y
            if (dy > 0) {
                if (y1 + y_step >= y2) {
                    return TRACE_SUCCESS;
                }

                y_map++;
            }
            else {
                if (y1 + y_step <= y2) {
                    return TRACE_SUCCESS;
                }

                y_map--;
            }

            if (x_map >= 0 && x_map < MAP_W && y_map >= 0 && y_map < MAP_H && MAP[y_map * MAP_W + x_map] > 0) {
                return TRACE_WALL;
            }
            else {
                y_step += MAP_TILE_SIZE * SGN(dy);
            }
        }
    }
}

[[gnu::noinline]]
[[gnu::section(".ewram.UpdateSprites")]]
static void UpdateSprites(Map const& map, Entity* entities, int proj_cos, int proj_sin) {
    // project sprite into camera space
    for (int i = 0; i < map.num_sprites; i++) {
        auto& spr = entities[i];

        // sprite animation handling -- probably shouldn't be here
        if (++spr.sub_frame == Entity::SUBFRAMES_PER_FRAME) {
            spr.sub_frame = 0;
            auto& anim = sprite_images[spr.class_]->anims[spr.state];

            // for IDLE/WALK, loop animation
            // for DEAD/ATTK stop at last frame
            if (spr.state == ANIM_IDLE || spr.state == ANIM_WALK || spr.frame < anim.num_frames - 1) {
                spr.frame = (spr.frame + 1) % anim.num_frames;
            }
        }

        spr.x_cam = (proj_sin * (spr.x - x_plr) - proj_cos * (spr.y - y_plr)) >> 12;
        spr.y_cam = (proj_cos * (spr.x - x_plr) + proj_sin * (spr.y - y_plr)) >> 12;

        if (spr.y_cam > 0) {        // in front of camera?
            static_assert(TAN_HALF_HFOV == 256);    // this lets us take a shortcut
            // TODO: optimize the division (at least don't do it twice)
            spr.scr_x1 = SCREEN_WIDTH / 2 + SCREEN_WIDTH / 2 * (spr.x_cam - spr.half_width) / spr.y_cam;
            spr.scr_x2 = SCREEN_WIDTH / 2 + SCREEN_WIDTH / 2 * (spr.x_cam + spr.half_width) / spr.y_cam;
        }
        else {
            spr.scr_x1 = -1;
            spr.scr_x2 = -1;
        }
    }
}
