#pragma once

#include "game_structs.hpp"

enum {
    PAL_BACKDROP = 0,
    DB16_BLACK = 1,
    DB16_DARKVIOLET,
    DB16_DARKBLUE,
    DB16_DARKGREY,
    DB16_BROWN,
    DB16_DARKGREEN,
    DB16_RED,
    DB16_GREY,
    DB16_BLUE,
    DB16_ORANGE,
    DB16_LTGREY,
    DB16_GREEN,
    DB16_PINK,
    DB16_SKY,
    DB16_YELLOW,
    DB16_WHITE,
};

enum {
    MAP_TILE_SIZE_LOG2 = 8,
    MAP_TILE_SIZE = (1<<MAP_TILE_SIZE_LOG2),

    SPRITE_WIDTH = 32,
    SPRITE_HEIGHT = 32,

    TEX_WIDTH = 32,
    TEX_HEIGHT = 32,

    // player is represented by a square of about 0.5x0.5 m (0.25 x 0.25 units)
    PLAYER_HALF_SIZE = (MAP_TILE_SIZE / 4) / 2,

    // GBA tile size
    TS = 8,
    WORDS_PER_TILE = 32 / 4,    // 8x8x4bpp = 32 bytes
    TILES_PER_ROW_VRAM = 256 / TS,

    WEAPON_SPRITE_TILE_INDEX = 512,
    WEAPON_SPRITE_W = 128,
    WEAPON_SPRITE_H = 64,
};

enum {
    TRACE_SUCCESS,
    TRACE_WALL,
    TRACE_OOB,
};

// render.cpp
// which: 0=front, 1=back
void rRayCast(Map const& map, Entity* entities);
void rDrawWalls(int which, Map const& map, Entity* entities);
void rDrawSprites(int which, Entity* entities);
void rDrawDebugOverlay(Map const& map, Entity* entities);
bool get_hitscan(int& sprite_index_out);  // TODO: probably can simplify to return int directly
int Trace(Map const& map, int x1, int y1, int x2, int y2);

// benchmark.cpp
void bmarkInit();
bool bmarkIsBenchmark();

// dbgmenu.cpp
void dbgmenuInput();
void dbgmenuDraw();

// draw.cpp
void drawImage(uint8_t const* frame, int x, int y, int h, uint16_t* fb);

// ent.cpp
void entThink(Entity& ent);

// kernels/

/**
 * Scale a 32-pixel column to the desired screen height
 * @param col1
 * @param col2
 * @param fb pointer to fb with the desired x and y offsets factored in
 * @param h height on screen
 */
void kernelScale2Col32(uint8_t const* column1, uint8_t const* column2, uint16_t* fb, int h);

// luts.cpp
extern const int32_t lu_dx_dy[513];
extern const int32_t lu_dy_dx[513];

// ASSUMPTION: Game is small enough that we can afford these globals
extern Map const* currmap;
extern int x_plr, y_plr, dir_plr;
extern uint8_t frames_since_hit;
extern uint8_t hitpoints_plr;
extern bool g_showdebug, g_dbgmenu, g_freezeai;
extern int auxv[4];     // auxiliary values for debugging
extern char auxstr[32];
