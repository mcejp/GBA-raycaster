#include "game.hpp"

#include <tonc.h>

void dbgmenuInput() {
    if (key_hit(1<<KI_START)) {
        g_dbgmenu = false;
    }
}

void sbmp8_roundrect(const TSurface *dst, int left, int top, int right, int bottom, u32 clr) {
    sbmp8_rect(dst, left, top + 1, right, bottom - 1, clr);
    sbmp8_hline(dst, left + 1, top, right - 2, clr);
    sbmp8_hline(dst, left + 1, bottom - 1, right - 2, clr);
}

void dbgmenuDraw() {
    static int menu_sel = 0;

    int x = 6, y = 6;
    int right = M4_WIDTH * 3 / 4, bot = M4_HEIGHT - y;
    sbmp8_roundrect(&m4_surface, x + 1, y + 1, right + 1, bot + 1, DB16_BLACK);

    x += 6;
    y += 4;

    tte_set_ink(DB16_GREEN);
    tte_set_pos(x, y);
    char buf[100];
    sprintf(buf, "x=%4d y=%4d dir=%4d", x_plr, y_plr, dir_plr);
    tte_write(buf);
    y += tte_get_font()->charH + 2;

#define BEGIN() \
    int nopt = 0;

#define DISPLAY_OPT(text_) \
    if (menu_sel == nopt)  { tte_set_ink(DB16_WHITE); } else { tte_set_ink(DB16_LTGREY); }                      \
    nopt++; \
    tte_set_pos(x, y); \
    tte_write((text_)); \
    y += tte_get_font()->charH + 1;

#define IF_USED() if (key_hit(1<<KI_A) && menu_sel == nopt - 1)

    //////

#define ANCHOR(anchor_name) a_##anchor_name

#define WARP_ANCHOR(anchor_name) \
    DISPLAY_OPT("Warp " #anchor_name);    \
    IF_USED() {                  \
        extern Anchor ANCHOR(anchor_name); \
        x_plr = ANCHOR(anchor_name).x; \
        y_plr = ANCHOR(anchor_name).y; \
        dir_plr = ANCHOR(anchor_name).dir; \
        g_dbgmenu = false; \
    }

    ///////

    BEGIN();

    DISPLAY_OPT(g_freezeai == false ? "Freeze AI" : "Unfreeze AI");
    IF_USED() { g_freezeai = !g_freezeai; }
//
//    DISPLAY_OPT("Option 2");
//    IF_USED() {  }

    WARP_ANCHOR(bm_closeup);
    WARP_ANCHOR(bm_corner);
    WARP_ANCHOR(bm_overlap);
    WARP_ANCHOR(bm_walls);

    ///////

    if (key_hit(1<<KI_UP)) {
        menu_sel = (nopt + menu_sel - 1) % nopt;
    }
    if (key_hit(1<<KI_DOWN)) {
        menu_sel = (menu_sel + 1) % nopt;
    }
}
