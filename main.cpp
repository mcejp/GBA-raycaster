#include "game.hpp"
#include "map_forest.h"
#include "pal_forest.h"
#include "pregenerated/crossbow.h"

#include <tonc.h>

#include "mgba/mgba.h"

#ifndef RETAIL
struct Scenario {
    const char* name;
    Anchor const& anchor;
};

static const Scenario scenarios[] {
//        { "test1", 0x1e0, 0x110, 90 * 0x10000 / 360 },
//        { "test2", 0x180, 0, 90 * 0x10000 / 360 },
//        { "empty", 0x180, -0x200, 270 * 0x10000 / 360 },
        {"bm_closeup", a_bm_closeup},
        {"bm_corner", a_bm_corner},
        {"bm_walls", a_bm_walls},
        {"bm_overlap", a_bm_overlap},
};
#endif

// Nasssty globalses
Map const* currmap = &map_forest;
int x_plr, y_plr;
int dir_plr = 90 * 0x10000 / 360;
uint8_t frames_since_hit = UINT8_MAX;
uint8_t hitpoints_plr = 100;
bool g_showdebug = false;
bool g_dbgmenu = false;
bool g_freezeai = false;
int auxv[4];
char auxstr[32];
static int frames_since_shot = 99;

static bool mapIsTileWall(Map const& map, int x_map, int y_map) {
    // the void outside the map is considered a wall
    if (x_map >= 0 && x_map < map.w && y_map >= 0 && y_map < map.h && map.tiles[y_map * map.w + x_map] == 0) {
        return false;
    }
    else {
        return true;
    }
}

static bool mapIsPointInsideWall(Map const& map, int x_world, int y_world) {
    return mapIsTileWall(map, x_world / MAP_TILE_SIZE, y_world / MAP_TILE_SIZE);
}

static void MovePlayer(int dx, int dy) {
    int new_x = x_plr + dx,
        new_y = y_plr + dy;
    // assuming that player size is less than 1 tile, it suffices to check the corners
    if (mapIsPointInsideWall(*currmap, new_x + PLAYER_HALF_SIZE, new_y + PLAYER_HALF_SIZE)
        || mapIsPointInsideWall(*currmap, new_x - PLAYER_HALF_SIZE, new_y + PLAYER_HALF_SIZE)
        || mapIsPointInsideWall(*currmap, new_x + PLAYER_HALF_SIZE, new_y - PLAYER_HALF_SIZE)
        || mapIsPointInsideWall(*currmap, new_x - PLAYER_HALF_SIZE, new_y - PLAYER_HALF_SIZE)) {
        return;
    }
    x_plr = new_x;
    y_plr = new_y;
}

static uint32_t SampleProfilingTimer() {
    // classic pattern for sampling a cascaded timer
    for (;;) {
        auto high = REG_TM3D;
        auto low = REG_TM2D;
        auto high2 = REG_TM3D;

        if (high == high2) {
            return (high << 16) | low;
        }
    }
}

static void SetScreenOverlay(uint16_t color, int opacity_0_to_16) {
    ((uint16_t*)MEM_PAL_BG)[PAL_BACKDROP] = color;
    REG_BLDCNT = (1<<2)|(1<<13)|(1<<6);     // alpha blend between BG2 & backdrop
    REG_BLDALPHA = (16 - opacity_0_to_16) | (opacity_0_to_16 << 8);
}

static void SetUpWeaponSprite() {
    OBJ_ATTR objs[2];
    oam_init(objs, 2);

    obj_set_attr(&objs[0],
                 ATTR0_SQUARE,
                 ATTR1_SIZE_64x64,
                 ATTR2_PALBANK(0) | 512);

    obj_set_attr(&objs[1],
                 ATTR0_SQUARE,
                 ATTR1_SIZE_64x64,
                 ATTR2_PALBANK(0) | (512 + 8));

    obj_set_pos(&objs[0], (M4_WIDTH - 128) / 2, M4_HEIGHT - 64);
    obj_set_pos(&objs[1], (M4_WIDTH) / 2, M4_HEIGHT - 64);

    oam_copy(oam_mem, objs, 2);

    memcpy32(pal_obj_mem, crossbowPal, crossbowPalLen / 4);

    // Do not actually copy the data... done later
}

static void CopyWeaponSpriteFrameToVram(unsigned int const* crossbowTiles) {
    static const int ROWS = WEAPON_SPRITE_H / TS;
    static const int COLS = WEAPON_SPRITE_W / TS;

    for (int row = 0; row < ROWS; row++) {
        dma_cpy(&tile_mem_obj[0][WEAPON_SPRITE_TILE_INDEX + row * TILES_PER_ROW_VRAM],
                &crossbowTiles[row * COLS * WORDS_PER_TILE],
                COLS * WORDS_PER_TILE, 3, DMA_CPY32);
    }
}

static int8_t psprite_frame_last = -1;

int main(void) {
    bmarkInit();

    x_plr = map_forest.x_start;
    y_plr = map_forest.y_start;

    REG_DISPCNT = DCNT_MODE4 | DCNT_BG2 | DCNT_OBJ | DCNT_OBJ_2D;

    tte_init_bmp_default(4);

    // 16-bit palette transfer
    CpuSet(palette, (void*) MEM_PAL_BG, sizeof(palette) / sizeof(*palette));

    irq_init(NULL);
    irq_enable(II_VBLANK);

    int scenario_counter = 0;
    const int RELOAD_TIME = 16;

    SetUpWeaponSprite();

    /*
     * Non-benchmark run:
     *   - start profiler
     *   - loop
     *     - process input
     *     - process AI
     *     - draw
     *     - sample profile
     *     - draw FPS
     *
     * Benchmark run
     *   - start profiler
     *   - loop
     *     - apply benchmark pose (NEW)
     *     - process input
     *     - process AI
     *     - draw
     *     - sample profile
     *     - on 2nd pass: print result & advance to next scenario (NEW)
     *     - draw FPS
     */

    profile_start();
    bool is_second_pass = false;
    uint32_t prev_timer_value = 0;

    while (1) {
        auto drawfb = (uint16_t *) ((REG_DISPCNT & DCNT_PAGE) != 0 ? MEM_VRAM_FRONT : MEM_VRAM_BACK);
        srf_set_ptr(tte_get_surface(), drawfb);
        srf_set_ptr(&m4_surface, drawfb);

        if (g_dbgmenu) {
            key_poll();
            dbgmenuInput();
            dbgmenuDraw();
            REG_DISPCNT ^= DCNT_PAGE;
            VBlankIntrWait();
            continue;
        }

        int const psprite_frame = (frames_since_shot < RELOAD_TIME) ? ((frames_since_shot < RELOAD_TIME * 3 / 4) ? 1 : 2) : 0;

        if (psprite_frame != psprite_frame_last) {
            // TODO: does this risk any visual glitching?
            CopyWeaponSpriteFrameToVram(&crossbowTiles[psprite_frame * WEAPON_SPRITE_W / 8 * WEAPON_SPRITE_H / 8 * (32 / 4)]);
        }

        psprite_frame_last = psprite_frame;

        if (bmarkIsBenchmark()) {
            auto& anchor = scenarios[scenario_counter].anchor;
            x_plr = anchor.x;
            y_plr = anchor.y;
            dir_plr = anchor.dir;
        }
        else {
            key_poll();
            if (key_is_down(1<<KI_LEFT)) {
                dir_plr += 0x400;
            }
            else if (key_is_down(1<<KI_RIGHT)) {
                dir_plr -= 0x400;
            }
            auto cos = lu_cos(dir_plr) / 16 / 8;
            auto sin = lu_sin(dir_plr) / 16 / 8;
            int dx = 0, dy = 0;
            if (key_is_down(1<<KI_UP))   { dx += cos; dy += sin; }
            if (key_is_down(1<<KI_DOWN)) { dx -= cos; dy -= sin; }
            if (key_is_down(1<<KI_B))    { dy += cos; dx -= sin; }
            if (key_is_down(1<<KI_A))    { dy -= cos; dx += sin; }
            MovePlayer(dx, dy);

            if (key_hit(1<<KI_R) && frames_since_shot > RELOAD_TIME) {
                // hitscan
                int spr_index;
                if (get_hitscan(spr_index) && entities[spr_index].state != ANIM_DEAD) {
                    entities[spr_index].StartAnim(ANIM_DEAD);
                }
                frames_since_shot = 0;
            }
            else if (key_hit(1<<KI_L)) {
                // resurrect everything
                for (int i = 0; i < currmap->num_sprites; i++) {
                    entities[i].StartAnim(ANIM_IDLE);
                }
            }

            if (key_hit(1<<KI_START)) {
                g_dbgmenu = true;
            }

            if (key_hit(1<<KI_SELECT)) {
                g_showdebug = !g_showdebug;
            }

            // Enemy AI
            for (int i = 0; i < currmap->num_sprites; i++) {
                auto& ent = entities[i];

                entThink(ent);
            }
        }

        frames_since_shot++;

        if (frames_since_hit < UINT8_MAX) {
            if (frames_since_hit <= 16) {
                SetScreenOverlay(RGB15(31, 8, 4), 16 - frames_since_hit);
            }

            frames_since_hit++;
        }

        auto before_rRayCast = SampleProfilingTimer();
        rRayCast(*currmap, entities);
        auto before_rDrawWalls = SampleProfilingTimer();
        rDrawWalls(!(REG_DISPCNT & DCNT_PAGE), *currmap, entities);
        auto before_rDrawSprites = SampleProfilingTimer();
        rDrawSprites(!(REG_DISPCNT & DCNT_PAGE), entities);

        if (g_showdebug) {
            rDrawDebugOverlay(*currmap, entities);
        }

        auto timer_value = SampleProfilingTimer();
        auto frame_time_cyc = timer_value - prev_timer_value;

        if (bmarkIsBenchmark()) {
            if (is_second_pass) {
                // TODO: can cut down code size by implementing simpler custom mGBA driver
                mgba_printf(MGBA_LOG_INFO, "(benchmark %s %d)", scenarios[scenario_counter].name, frame_time_cyc);
                mgba_printf(MGBA_LOG_INFO, "profile:\n  misc\t\t%7d\n  rRayCast\t%7d\n  rDrawWalls\t%7d\n  rDrawSprites\t%7d",
                            (int)(before_rRayCast - prev_timer_value),
                            (int)(before_rDrawWalls - before_rRayCast),
                            (int)(before_rDrawSprites - before_rDrawWalls),
                            (int)(timer_value - before_rDrawSprites));

                if (++scenario_counter == sizeof(scenarios) / sizeof(*scenarios)) {
                    Stop();
                }

                is_second_pass = false;
            }
            else {
                is_second_pass = true;
            }
        }

        prev_timer_value = timer_value;

        enum {
            LOW_FPS = 12,
            MEDIUM_FPS = 18,
            HIGH_FPS = 30,
        };

        int fps = 16 * 1024 * 1024 / frame_time_cyc;

        if (fps >= HIGH_FPS) {          tte_set_ink(DB16_GREEN); }
        else if (fps >= MEDIUM_FPS) {   tte_set_ink(DB16_YELLOW); }
        else if (fps >= LOW_FPS) {      tte_set_ink(DB16_ORANGE); }
        else {                          tte_set_ink(DB16_RED); }

        tte_set_pos(0, 0);
        char buf[100];
        sprintf(buf, "%lu cyc / %d FPS", frame_time_cyc, fps);
//        sprintf(buf, "%d", auxv[0]);
        tte_write(buf);

        tte_set_pos(0, 12);
        tte_write(auxstr);
        auxstr[0] = 0;

        // health bar
        sbmp8_rect(&m4_surface, 236 - 52, 4, 236, 10, DB16_BLACK);
        sbmp8_rect(&m4_surface, 236 - 51, 5, 236 - 51 + hitpoints_plr / 2, 9, DB16_GREEN);

        REG_DISPCNT ^= DCNT_PAGE;
    }

}