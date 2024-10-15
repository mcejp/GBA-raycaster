#pragma once

#include <tonc.h>

// Game is never small enough to not benefit from Vec2
struct Vec2i {
    int x, y;
};

enum {
    IMG_OP_NOP,
    IMG_OP_BLT8,
    IMG_OP_END,
    IMG_OP_EVEN,
    IMG_OP_ODD,
    IMG_OP_SKIP8,
    IMG_OP_SKIP16,
};

struct ImageAnim {
    uint8_t const** frames;
    uint8_t num_frames;
};

struct SpriteSpan {
    int start, count;
    uint8_t const* data;
};

struct SpriteFrame {
    SpriteSpan const* spans;
};

struct SpriteAnim {
    SpriteFrame const* frames;
    uint8_t num_frames;
};

enum AnimKind {
    ANIM_IDLE,
    ANIM_WALK,
    ANIM_ATTK,
    ANIM_DEAD,
};

enum {
    AI_IDLE,
    AI_PRST,
    AI_ATTK,
};

struct Sprite {
    SpriteAnim const* anims;
};

struct Anchor {
    int16_t x, y;
    uint16_t dir;
};

struct Entity {
    int x, y, half_width, height;
    //COLOR color;
    uint8_t class_;
    int x_cam, y_cam;
    int16_t scr_x1, scr_x2;
    uint8_t state, frame, sub_frame;
    uint8_t see_player, ai_state, attk_cooldown;

    enum { SUBFRAMES_PER_FRAME = 6 };

    void StartAnim(uint8_t new_state) {
        state = new_state;
        frame = 0;
        sub_frame = 0;
    }
};

struct Map {
    uint8_t const* tiles;
    Entity const* entities;
    uint8_t w, h, num_sprites;
    uint16_t x_start, y_start;
};
