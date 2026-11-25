/*
 * raton.c - the mouse in Yu-Gi-Oh! The Dawn of Destiny (ygo-port).
 *
 * Three modes (F2 "Modo del ratón", YGO_MOUSE_MODE, ygo_menu.ini raton_modo):
 *
 *   Cursor del juego (default since 2026-11-08: one pointer, the game's own
 *     glove; the user did not want the Windows pointer and the glove both):
 *     see "Cursor por pasos" below.
 *   Puntero (experimental, in the style of the PC card games): the Windows
 *     cursor stays visible over the game. On the duel field, pointing at a
 *     square takes the game's cursor there; the left button is A, the right
 *     one B, the middle one Y, the wheel steps up and down (menus, lists).
 *   Cursor por pasos: the cursor is captured and hidden; its motion taps the
 *     d-pad one square per s_step counts (ratón nivel 1).
 *   Solo clics: buttons and wheel only.
 *
 * The duel cursor is the game's own: sub_00086F50(player, zone, square)
 * stores it at [0x6633D4], [0x6633D8], [0x6633DC] (buscar_cursor.c found
 * it). Zones: 0 monsters, 5 spells/traps, 11 the hand, 10 and 12..15 the
 * side squares. Pointing does not write it: the d-pad is pressed toward the
 * square until the game's cursor is there, so the game's own rules decide
 * what is reachable. Three presses that leave the cursor where it was (each
 * given ANSWER_MS) mean a menu has the d-pad or the square is out of reach:
 * nothing more is pressed until the pointer goes to another square.
 *
 * Screen places, in the game's 640x480 frame, measured from the
 * fotos_cursor\ pictures (2026-11-08): the game's glove points there.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "recomp/gen/recomp_types.h"
#include "input/xinput_xbox.h"
#include "input/mouse.h"
#include "input/input_names.h"

int xbox_VideoWidescreen(void);   /* kernel: "Panorámico (nativo del juego)" */
/* kit nv2a_pb_d3d11.c: a client point in guest px of the presented frame */
int nv2a_d3d11_window_to_guest(int px, int py, int cw, int ch, float *gx, float *gy, float *dev);

enum { MODE_POINTER, MODE_STEPS, MODE_CLICKS };   /* F2 order */
static volatile int s_mode = MODE_POINTER;
static volatile int s_step = 120;           /* counts (or 0.4 px) per step */

#define PRESS_MS 45u
#define GAP_MS   50u
#define IDLE_MS  250u
#define B_HOLD_MS 80u      /* a right click's B lasts at least this long */

/* ---- where each square is ---------------------------------------------- */

typedef struct { int player, zone, square; float x, y; } Slot;

#define CUR_PLAYER 0x006633D4u
#define CUR_ZONE   0x006633D8u
#define CUR_SQUARE 0x006633DCu

/* Square centres measured on a grid over fotos_cursor\p0_z0_i2 (the
 * squares themselves; the glove's fingertip sits ~10 px below and right).
 * The far side's squares are listed from the near player's view: its
 * square 0 is on the right. */
/* Where the game showed each place, learnt while it was the cursor (the
 * glove's fingertip there, field_learn): exact for this camera, and right
 * for the hand at any number of cards. Unlearnt places use the measured
 * centres below. */
/* A hand can hold more than 8 cards (10 after a ritual summon, 2026-11-10:
 * the pointer stopped at the 8th). */
#define HAND_MAX 16
static float s_learn[2][16][HAND_MAX][2];
static unsigned char s_learnt[2][16][HAND_MAX];

/* Cards in a player's hand (duel state, 0x13F8 bytes a player). */
static int hand_count(int player)
{
    return MEM8(0x0065D0A8u + (uint32_t)(player & 1) * 0x13F8u);
}

/* Distance between the centres of two hand cards (measured, see below). */
static float hand_step(void)
{
    int n = hand_count(0);
    return n <= 5 ? 88.f : 490.f / (float)n;
}

static int slot_place(int player, int zone, int sq, float *x, float *y)
{
    /* The hand is not learnt any more (2026-11-08 video 18-37-31): a card
     * learnt at the wrong place moved the cursor to another card, which was
     * learnt wrong in turn, and with a still pointer the selection went
     * round the hand and the set cards for seconds. Measured on the videos
     * at 1920 px (18-37-31, 18-50-57): up to 5 cards the centres are at
     * 100 + 88 i; 6 cards at 115 + 81.7 i (115..524). 10 cards (screenshot
     * 2026-11-10) at 97 + 49 i: from 6 on the step is 490 / n, centred on
     * x 319.5. A card is 74 wide (HAND_HALF_W each side). */
    if (zone == 11 && player == 0 && sq >= 0 && sq < HAND_MAX) {
        int n = hand_count(0);
        if (n <= 5)
            *x = 100.f + 88.f * (float)sq;
        else
            *x = 319.5f + 490.f / (float)n * ((float)sq - (float)(n - 1) / 2.f);
        *y = 365.f;
        return 1;
    }
    if (player >= 0 && player < 2 && zone >= 0 && zone < 16 && sq >= 0 && sq < HAND_MAX
            && s_learnt[player][zone][sq]) {
        *x = s_learn[player][zone][sq][0];
        *y = s_learn[player][zone][sq][1];
        return 1;
    }
    /* A hand card not seen yet: from the ones seen, at their spacing (the
     * game spreads the hand by its card count). */
    if (zone == 11 && player >= 0 && player < 2 && sq >= 0 && sq < HAND_MAX) {
        int i, a = -1, b = -1;
        for (i = 0; i < HAND_MAX; i++)
            if (s_learnt[player][11][i]) {
                if (a < 0) a = i; else { b = i; break; }
            }
        if (a >= 0) {
            float step = b >= 0 ? (s_learn[player][11][b][0] - s_learn[player][11][a][0]) / (float)(b - a)
                                : 87.f;
            *x = s_learn[player][11][a][0] + step * (float)(sq - a);
            *y = s_learn[player][11][a][1];
            return 1;
        }
    }
    static const float near_mon[5] = { 231.f, 276.f, 318.f, 361.f, 405.f };
    static const float near_spl[5] = { 231.f, 275.f, 321.f, 365.f, 410.f };
    static const float far_mon[5]  = { 398.f, 359.f, 318.f, 281.f, 243.f };
    static const float far_spl[5]  = { 397.f, 357.f, 320.f, 282.f, 245.f };
    if (sq < 0 || sq > 5)
        return 0;
    if (!player) {
        switch (zone) {
        case 0:  if (sq > 4) return 0; *x = near_mon[sq]; *y = 252.f; return 1;
        case 5:  if (sq > 4) return 0; *x = near_spl[sq]; *y = 287.f; return 1;
        case 10: *x = 185.f; *y = 252.f; return 1;
        case 12: *x = 184.f; *y = 287.f; return 1;
        case 13: *x = 458.f; *y = 285.f; return 1;
        case 14: *x = 452.f; *y = 252.f; return 1;
        case 15: *x = 495.f; *y = 252.f; return 1;
        case 11: *x = 100.f + 87.f * sq; *y = 365.f; return 1;   /* the hand */
        }
        return 0;
    }
    switch (zone) {
    case 0:  if (sq > 4) return 0; *x = far_mon[sq]; *y = 142.f; return 1;
    case 5:  if (sq > 4) return 0; *x = far_spl[sq]; *y = 110.f; return 1;
    case 10: *x = 439.f; *y = 142.f; return 1;
    case 12: *x = 439.f; *y = 110.f; return 1;
    case 13: *x = 205.f; *y = 112.f; return 1;
    case 14: *x = 201.f; *y = 142.f; return 1;
    case 15: *x = 165.f; *y = 142.f; return 1;
    }
    return 0;
}

/* The square under the pointer, or 0. */
static int slot_at(float px, float py, Slot *out)
{
    static const struct { int zone, n; } zones[] = {
        { 0, 5 }, { 5, 5 }, { 10, 1 }, { 12, 1 }, { 13, 1 }, { 14, 1 }, { 15, 1 }, { 11, HAND_MAX },
    };
    float best = 1e9f;
    int p, z, i, found = 0;
    for (p = 0; p < 2; p++)
        for (z = 0; z < (int)(sizeof zones / sizeof zones[0]); z++)
            for (i = 0; i < zones[z].n; i++) {
                if (zones[z].zone == 11 && i >= hand_count(p))
                    break;              /* no card there */
                float x, y, d, rx, ry;
                if (!slot_place(p, zones[z].zone, i, &x, &y))
                    continue;
                /* own hand: half the spacing, so the nearest card wins and the
                 * switch is at the middle between two cards */
                rx = zones[z].zone == 11 ? (p ? 43.f : hand_step() / 2.f) : 22.f;
                ry = zones[z].zone == 11 ? 45.f : p ? 16.f : 18.f;
                if (fabsf(px - x) > rx || fabsf(py - y) > ry)
                    continue;
                d = (px - x) * (px - x) + (py - y) * (py - y);
                if (d < best) {
                    best = d;
                    out->player = p; out->zone = zones[z].zone; out->square = i;
                    out->x = x; out->y = y;
                    found = 1;
                }
            }
    return found;
}

/* ---- the pointer over the game window ---------------------------------- */

/* ---- the pointer is the game's glove ----------------------------------- */

/* In pointer mode the Windows pointer is drawn as the game's own glove
 * (comm_13_00, raton_guante.h), at the size the game draws it (64 frame
 * pixels square, scaled with the picture), and the game's glove itself is
 * cleared from the common atlas (mods_puntero\, tools/guante.py; main.c
 * points RECOMP_MODS_DIR there): one glove on screen, moving freely. The
 * class cursor of the game window is swapped, so it shows over the game
 * only. `px` 0 puts the arrow back. */
#include "raton_guante.h"

static HWND s_wnd;
/* The left button's last press (the hook), for the glove's press frame and
 * the click that brings the window back. */
static volatile ULONGLONG s_lpress_at;
static volatile LONG s_lfocus;          /* this press only brought the window back */
static volatile ULONGLONG s_lfocus_end; /* and was let go then */

/* `pressed`: the click frame (2026-11-14, the user: "una animación al dedo
 * cuando se hace click"): the glove 86 % of its size toward the fingertip
 * (the hot spot stays put, the hand dips onto it) and a little darker. */
#define GLOVE_PRESS_SCALE 0.86f
#define GLOVE_PRESS_SHADE 200u          /* of 255 */
static HCURSOR glove_make(int px, int pressed, uint32_t *copy)
{
    BITMAPV5HEADER bi;
    void *bits = NULL;
    HBITMAP color, mask;
    ICONINFO ii;
    HCURSOR hc;
    HDC dc;
    int x, y;

    memset(&bi, 0, sizeof bi);
    bi.bV5Size = sizeof bi;
    bi.bV5Width = px;
    bi.bV5Height = -px;                 /* top row first */
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;
    dc = GetDC(NULL);
    color = CreateDIBSection(dc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    ReleaseDC(NULL, dc);
    if (!color || !bits)
        return NULL;
    /* The x4 glove (k_guante, GUANTE_HD square) to px: each pointer pixel is
     * the average of the HD pixels it covers (premultiplied, so the outline
     * stays clean), or the nearest one when px is larger than the HD glove.
     * Smooth at any window size: the x1 sprite drawn nearest looked blocky. */
    for (y = 0; y < px; y++)
        for (x = 0; x < px; x++) {
            int x0 = x * GUANTE_HD / px, x1 = (x + 1) * GUANTE_HD / px;
            int y0 = y * GUANTE_HD / px, y1 = (y + 1) * GUANTE_HD / px;
            unsigned sb = 0, sg = 0, sr = 0, sa = 0, n = 0;
            int u, v;
            unsigned char *d = (unsigned char *)bits + 4 * (y * px + x);
            if (pressed) {
                /* the HD pixels under this one in the shrunk glove: the
                 * fingertip (hx, hy, HD pixels) is the fixed point */
                float hx = (float)GUANTE_TIP_X * GUANTE_HD / GUANTE_SIZE;
                float hy = (float)GUANTE_TIP_Y * GUANTE_HD / GUANTE_SIZE;
                float k = (float)GUANTE_HD / px / GLOVE_PRESS_SCALE;
                x0 = (int)floorf(hx + ((float)x - hx / GUANTE_HD * px) * k);
                x1 = (int)floorf(hx + ((float)x + 1.f - hx / GUANTE_HD * px) * k);
                y0 = (int)floorf(hy + ((float)y - hy / GUANTE_HD * px) * k);
                y1 = (int)floorf(hy + ((float)y + 1.f - hy / GUANTE_HD * px) * k);
                if (x1 <= x0) x1 = x0 + 1;
                if (y1 <= y0) y1 = y0 + 1;
                if (x0 < 0 || y0 < 0 || x1 > GUANTE_HD || y1 > GUANTE_HD) {
                    d[0] = d[1] = d[2] = d[3] = 0;
                    continue;
                }
            }
            if (x1 <= x0) x1 = x0 + 1;
            if (y1 <= y0) y1 = y0 + 1;
            for (v = y0; v < y1; v++)
                for (u = x0; u < x1; u++) {
                    const unsigned char *s = k_guante + 4 * (v * GUANTE_HD + u);
                    unsigned a = s[3];
                    sb += s[0] * a / 255;   /* premultiplied */
                    sg += s[1] * a / 255;
                    sr += s[2] * a / 255;
                    sa += a;
                    n++;
                }
            if (pressed) {
                sb = sb * GLOVE_PRESS_SHADE / 255u;
                sg = sg * GLOVE_PRESS_SHADE / 255u;
                sr = sr * GLOVE_PRESS_SHADE / 255u;
            }
            d[0] = (unsigned char)(sb / n);
            d[1] = (unsigned char)(sg / n);
            d[2] = (unsigned char)(sr / n);
            d[3] = (unsigned char)(sa / n);
        }
    if (copy)                           /* the picture drawn into the frame */
        memcpy(copy, bits, (size_t)px * px * 4u);
    mask = CreateBitmap(px, px, 1, 1, NULL);
    ii.fIcon = FALSE;
    ii.xHotspot = (DWORD)(GUANTE_TIP_X * px / GUANTE_SIZE);
    ii.yHotspot = (DWORD)(GUANTE_TIP_Y * px / GUANTE_SIZE);
    ii.hbmMask = mask;
    ii.hbmColor = color;
    hc = (HCURSOR)CreateIconIndirect(&ii);
    DeleteObject(color);
    DeleteObject(mask);
    return hc;
}

#define GLOVE_PRESS_MS 110u             /* a quick click shows the press this long */

/* "Guante en la imagen" (2026-11-23, the user: OBS records without the
 * glove): the Windows pointer goes blank and the kit draws the same glove
 * into the frame at its place (nv2a_d3d11_pointer_sprite), so any capture
 * (game, window or screen) has it. F2 / ygo_menu.ini raton_guante_imagen. */
void nv2a_d3d11_pointer_sprite(const uint32_t *bgra, unsigned w, unsigned h, int hx, int hy);
static volatile LONG s_glove_in_frame;
static volatile LONG s_glove_redo;      /* the option changed: set it all again */

static HCURSOR blank_cursor(void)
{
    static HCURSOR b;
    if (!b) {
        BYTE and_[128], xor_[128];
        memset(and_, 0xFF, sizeof and_);
        memset(xor_, 0, sizeof xor_);
        b = CreateCursor(GetModuleHandleA(NULL), 0, 0, 32, 32, and_, xor_);
    }
    return b;
}

static void glove_cursor(int px)
{
    static int have_px, have_pr, have_in, made_px;
    static HCURSOR hc[2];
    static uint32_t *pix[2];
    int pr = 0, in = s_glove_in_frame != 0;
    if (!s_wnd || !IsWindow(s_wnd))
        return;
    if (InterlockedExchange(&s_glove_redo, 0))
        have_px = -1;
    if (px > 0)
        pr = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0
             || GetTickCount64() - s_lpress_at < GLOVE_PRESS_MS;
    if (px == have_px && pr == have_pr && in == have_in)
        return;
    if (px > 0 && px != made_px) {
        HCURSOR n[2] = { NULL, NULL };
        uint32_t *q[2];
        int i;
        q[0] = (uint32_t *)malloc((size_t)px * px * 4u);
        q[1] = (uint32_t *)malloc((size_t)px * px * 4u);
        if (q[0] && q[1]) {
            n[0] = glove_make(px, 0, q[0]);
            n[1] = glove_make(px, 1, q[1]);
        }
        if (!n[0] || !n[1]) {
            if (n[0]) DestroyCursor(n[0]);
            if (n[1]) DestroyCursor(n[1]);
            free(q[0]);
            free(q[1]);
            return;
        }
        /* the new one in place before the old ones go */
        SetClassLongPtrA(s_wnd, GCLP_HCURSOR, (LONG_PTR)(in ? blank_cursor() : n[pr]));
        for (i = 0; i < 2; i++) {
            if (hc[i])
                DestroyCursor(hc[i]);
            hc[i] = n[i];
            free(pix[i]);
            pix[i] = q[i];
        }
        made_px = px;
    }
    if (px > 0 && in) {
        SetClassLongPtrA(s_wnd, GCLP_HCURSOR, (LONG_PTR)blank_cursor());
        nv2a_d3d11_pointer_sprite(pix[pr], (unsigned)px, (unsigned)px,
                                  GUANTE_TIP_X * px / GUANTE_SIZE, GUANTE_TIP_Y * px / GUANTE_SIZE);
    } else {
        nv2a_d3d11_pointer_sprite(NULL, 0, 0, 0, 0);
        SetClassLongPtrA(s_wnd, GCLP_HCURSOR,
                         (LONG_PTR)(px > 0 ? hc[pr] : LoadCursor(NULL, IDC_ARROW)));
    }
    have_px = px;
    have_pr = pr;
    have_in = in;
    /* The class cursor shows at the next move; a nudge of zero shows it now. */
    {
        POINT p;
        if (GetCursorPos(&p))
            SetCursorPos(p.x, p.y);
    }
}

void ygo_glove_in_frame_set(int on)
{
    InterlockedExchange(&s_glove_in_frame, on ? 1 : 0);
    InterlockedExchange(&s_glove_redo, 1);
}

int ygo_glove_in_frame(void) { return s_glove_in_frame != 0; }

/* The pointer in the game's 640x480 frame (the picture keeps 4:3 in the
 * middle of the window), or 0 when it is not over the game's picture or the
 * game is not the active window. */
static POINT s_pt;                  /* last pointer, client pixels */
static RECT s_rc;                    /* the client rectangle then */

static int pointer_pos(float *gx, float *gy)
{
    static HWND wnd;
    POINT pt;
    RECT rc;
    float sc, ox, oy, w, h;

    if (!wnd || !IsWindow(wnd))
        wnd = FindWindowA("RecompNv2aD3D11", NULL);
    s_wnd = wnd;
    if (!wnd || GetForegroundWindow() != wnd || !GetCursorPos(&pt))
        return 0;
    ScreenToClient(wnd, &pt);
    GetClientRect(wnd, &rc);
    s_pt = pt;
    s_rc = rc;
    w = (float)(rc.right - rc.left);
    h = (float)(rc.bottom - rc.top);
    if (w < 1 || h < 1 || pt.x < 0 || pt.y < 0 || pt.x >= rc.right || pt.y >= rc.bottom)
        return 0;
    {
        /* 2026-11-10: where the kit really put the last frame (window or
         * fullscreen, any size, aspect, hor+ pads, overscan, integer
         * scaling, shader chain): nv2a_d3d11_window_to_guest. The fit
         * below stays for before the first frame. */
        float dev = 0.f;
        int on = nv2a_d3d11_window_to_guest(pt.x, pt.y, (int)w, (int)h, gx, gy, &dev);
        if (on >= 0) {
            int px = ((int)(64.f * dev + 2.f) / 4) * 4;
            if (px < 24) px = 24;
            if (px > 256) px = 256;
            glove_cursor(px);
            return on;
        }
    }
    {
        /* The picture keeps the TV's shape in the middle of the window
         * (nv2a_present_rect): 4:3, or 16:9 with "Panorámico (nativo del
         * juego)", where the 640x480 frame is shown stretched. */
        float aspect = xbox_VideoWidescreen() ? 16.f / 9.f : 4.f / 3.f;
        float pw = w, ph = w / aspect;
        if (ph > h) {
            ph = h;
            pw = h * aspect;
        }
        ox = (w - pw) / 2.f;
        oy = (h - ph) / 2.f;
        *gx = ((float)pt.x - ox) * 640.f / pw;
        *gy = ((float)pt.y - oy) * 480.f / ph;
        (void)sc;
        /* The glove at the game's size: 64 frame pixels, 4 px steps so a
         * window being dragged does not remake it every pixel. */
        {
            int px = ((int)(64.f * ph / 480.f + 2.f) / 4) * 4;
            if (px < 24) px = 24;
            if (px > 256) px = 256;
            glove_cursor(px);
        }
    }
    return *gx >= 0 && *gy >= 0 && *gx < 640.f && *gy < 480.f;
}

/* ---- presses ----------------------------------------------------------- */

static WORD s_held;
static volatile ULONGLONG s_name_at;             /* the name screen ran then */
static volatile LONG s_name_finish;
static volatile LONG s_name_btn = -1;
static volatile LONG s_name_hover = -2;          /* pointer: 0..2 a button, -1 the letters */
static volatile LONG s_name_hover_seq;           /* bumps when s_name_hover changes */           /* clicked: 0 Atrás, 1 Próximo, 2 Aceptar */
static volatile ULONGLONG s_title_at;            /* the title menu waited then */
static volatile ULONGLONG s_start_at;            /* "Presiona START" (state 7) waited then */
static volatile LONG s_title_want = -1;         /* 0 Nueva partida, 1 Cargar partida */              /* Enter or a click: finish */
static int s_held_wheel;                 /* s_held is a mouse wheel step */
static ULONGLONG s_until;
/* The pointer walk (the glove stepped card by card toward a click): only
 * its first step ticks; the cursor sound of the next ones is muted
 * (2026-11-08, video 20-40-58: the Biblioteca ticked on every card). */
static volatile ULONGLONG s_walk_last, s_mute_until;
/* The Biblioteca (sub_0010F790): the card under the pointer, col | row << 8
 * (-1 none), set by the pointer and taken by the game's thread. */
static volatile ULONGLONG s_lib_at;
static volatile LONG s_lib_want = -1;
static volatile LONG s_lib_scroll;               /* wheel rows: + down, - up */
static volatile LONG s_lib_drag = -1;            /* scroll bar: the top row wanted */
static volatile uint32_t s_lib_scene;            /* sub_00038220's scene */
static volatile LONG s_dlg_want = -1;
static volatile LONG s_btn_want = -1;             /* open card's button under the pointer (0 lupa, 1 camara) */             /* exit box line under the pointer (0 Aceptar, 1 Cancelar) */
static volatile ULONGLONG s_lib_wheel_at;
static volatile LONG s_lib_open = -1;             /* grid card clicked: col | row << 8, opened by the wrapper */
static volatile ULONGLONG s_lib_open_at;
static volatile LONG s_lib_force;                /* take s_lib_want as a move even on the same place */
static ULONGLONG s_lib_tick_at;                  /* the pointer's card changed then */

/* Starts a press of `b` when none is held and the gap is over: 1 if it did. */
static int press(WORD b, ULONGLONG t)
{
    if (s_held || t < s_until)
        return 0;
    s_held = b;
    s_held_wheel = 0;
    s_until = t + PRESS_MS;
    return 1;
}

static void presses_tick(ULONGLONG t)
{
    if (s_held && t >= s_until) {
        s_held = 0;
        s_until = t + GAP_MS;
    }
}

/* Motion (counts, or pointer pixels) to d-pad steps: the dominant axis, at
 * most two steps queued, leftovers dropped after IDLE_MS. */
static void steps(int dx, int dy, int step, ULONGLONG t)
{
    static const WORD dir[2][2] = {
        { XBOX_GAMEPAD_DPAD_LEFT, XBOX_GAMEPAD_DPAD_RIGHT },
        { XBOX_GAMEPAD_DPAD_UP, XBOX_GAMEPAD_DPAD_DOWN },
    };
    static int acc[2];
    static ULONGLONG moved_at;
    int d[2] = { dx, dy }, a;

    if (dx || dy) {
        moved_at = t;
        for (a = 0; a < 2; a++) {
            acc[a] += d[a];
            /* One step waits at most: a quick flick moves one square, never
             * runs on past where the hand stopped. */
            if (acc[a] > step) acc[a] = step;
            if (acc[a] < -step) acc[a] = -step;
        }
    }
    if (t - moved_at > IDLE_MS)
        for (a = 0; a < 2; a++)
            if (acc[a] > -step && acc[a] < step)
                acc[a] = 0;
    a = abs(acc[1]) > abs(acc[0]) ? 1 : 0;
    if (abs(acc[a]) >= step && press(dir[a][acc[a] > 0], t)) {
        acc[a] += acc[a] > 0 ? -step : step;
        acc[!a] = 0;
    }
}

/* ---- pointer mode ------------------------------------------------------ */

/* The left button for a click test at one place in the code (`site`, one
 * per caller: LDOWN()). Down now, or pressed and already let go since that
 * place last looked (the hook counts presses): a quick click inside a slow
 * frame (a hitch, 2026-11-11 "que respondan lo más rápido posible") was
 * lost before, since GetAsyncKeyState only says whether it is down now. A
 * place that did not look in the last 250 ms only catches up (no click
 * carried over from another screen). */
static volatile LONG s_lpress;
static int lbutton(int site)
{
    static LONG seen[32];
    static ULONGLONG polled[32];
    ULONGLONG t = GetTickCount64();
    int d = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    LONG p = s_lpress;
    if (s_lfocus) {
        if (d)
            return 0;
        InterlockedExchange(&s_lfocus, 0);
        s_lfocus_end = t;
    }
    int fresh = polled[site] && t - polled[site] <= 250u;
    polled[site] = t;
    if (p != seen[site]) {
        seen[site] = p;
        if (!d && fresh)
            return 1;
    }
    return d;
}
#define LDOWN() lbutton(__COUNTER__ & 31)

/* Wheel notches (a low-level hook: the window is not captured in pointer
 * mode, so the kit drops the wheel); up is positive. */
static volatile LONG s_wheel;
static volatile LONG s_rpress;          /* right-button presses seen (hook) */
static volatile ULONGLONG s_rpress_at;   /* the last one then */
/* 2026-11-17 (mouse review): the right button's press, once per poll for
 * every screen (pointer_mode), as the left one's `ledge`. Each screen kept
 * its own "was down" before, stale from the last time it ran: a right click
 * that closed the command buttons was a fresh press again for the field
 * when it came back (its hook test looked 150 ms back), a second B there. */
static int s_redge;
static ULONGLONG s_btn_quiet;            /* a button was pressed: no glove walk until then */
static ULONGLONG s_voz_until;            /* a click on a voice balloon: A until then */
/* 2026-11-17 (the user, at 120 fps: "algun click fantasma o que me hace click
 * muy rapido"): the press whose kit A a screen took out (mouse_kit_off). It
 * stays out for that whole press, whatever screen comes next: a click on
 * "Atacar" ran the command, the buttons closed, and the message box after
 * them (no screen of its own here) got the kit's A of the same press, still
 * held, as a new click: the box closed at once. At 120 fps the next screen
 * comes sooner, inside the press. */
static volatile LONG s_a_press = -1;
static HHOOK s_hook;

static LRESULT CALLBACK wheel_hook(int code, WPARAM w, LPARAM l)
{
    /* every right press counts, even one released before the next poll
     * (GetAsyncKeyState only says whether it is down now) */
    if (code == HC_ACTION && w == WM_RBUTTONDOWN) {
        s_rpress_at = GetTickCount64();
        InterlockedIncrement(&s_rpress);
    }
    if (code == HC_ACTION && w == WM_LBUTTONDOWN) {
        /* 2026-11-14 (18:26 log, frame 92342): the click that brought the
         * game window back from another one (the game paused meanwhile)
         * reached the deck builder as a click (part 9 opened). A press
         * while another window is in front only activates the game. */
        HWND fg = GetForegroundWindow();
        char cls[64];
        if (fg && GetClassNameA(fg, cls, sizeof cls) && !strcmp(cls, "RecompNv2aD3D11")) {
            s_lpress_at = GetTickCount64();
            InterlockedIncrement(&s_lpress);
        } else
            InterlockedExchange(&s_lfocus, 1);
    }
    if (code == HC_ACTION && w == WM_MOUSEWHEEL && s_mode == MODE_POINTER) {
        HWND fg = GetForegroundWindow();
        char cls[64];
        if (fg && GetClassNameA(fg, cls, sizeof cls) && !strcmp(cls, "RecompNv2aD3D11")) {
            short d = (short)HIWORD(((MSLLHOOKSTRUCT *)l)->mouseData);
            InterlockedExchangeAdd(&s_wheel, d > 0 ? 1 : -1);
        }
    }
    return CallNextHookEx(s_hook, code, w, l);
}

static DWORD WINAPI wheel_thread(LPVOID unused)
{
    MSG m;
    (void)unused;
    s_hook = SetWindowsHookExA(WH_MOUSE_LL, wheel_hook, GetModuleHandleA(NULL), 0);
    while (GetMessageA(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageA(&m);
    }
    return 0;
}

/* ---- where the game's cursor really is ---------------------------------- */

/* Every 2D sprite of the game is an object of one base class whose
 * vtable +0x0C, sub_000F80D0 (thiscall: texture path, x, y, ...), builds it
 * from its picture and whose move-to, sub_000F85F0 (thiscall: x, y floats,
 * frames), sends it somewhere: the destination at +0x20/+0x24, the
 * position drawn at +0x38/+0x3C. The game's glove, whatever class wraps it
 * (the menus' 0x454FB0 one, the duel's own), is a sprite built from
 * comm/2d/comm_13_NN.png. So: the sprites built from that picture are
 * remembered, and the last destination any of them was sent to is where
 * the game's cursor is, on every screen. (The earlier reads, [0x6633D4],
 * [0x657130] and the 0x454FB0 glove alone, stayed still through whole
 * duels: 09:17, 10:50 and 11:03 logs.)
 *
 * Positions are the picture's middle in the 640x480 frame; the fingertip is
 * at (22,14) of the 64x64 picture (raton_guante.h), so it is the position
 * plus (22-32, 14-32). The Windows glove's hot spot is that same fingertip
 * (11:00 log: destination 430,233 -> fingertip 420,215, the pointer at
 * 417,213). recomp's definitions are renamed _pad (tools/rename_pads.py). */
void sub_000F80D0_pad(void);
void sub_000F85F0_pad(void);

#define TIP_DX ((float)GUANTE_TIP_X - GUANTE_SIZE / 2)
#define TIP_DY ((float)GUANTE_TIP_Y - GUANTE_SIZE / 2)
#define NGLOVES 64
#define MOVE_PX 6.f                     /* less is the glove's own wobble */

/* The glove sprites seen built. Each is watched where it is drawn
 * (+0x38/+0x3C, which move-to animates toward +0x20/+0x24 and a direct
 * placement sets), and the one that last moved more than MOVE_PX is the
 * game's cursor. Its vtable is taken at the first poll, not at the build:
 * the duel's glove classes put theirs after the base build (11:15 log: 52
 * gloves built, none followed, because each was dropped at once as
 * "something else now"). */
static struct {
    uint32_t obj, vt;
    float ax, ay;                       /* where it last moved to */
    ULONGLONG when;                     /* and when */
} s_gloves[NGLOVES];
static volatile float s_gx = -1.f, s_gy = -1.f;   /* the game's fingertip */
static volatile uint32_t s_gobj;                  /* the glove it is on */
static volatile LONG s_gseq;                     /* bumps on every move */
static volatile LONG s_ngloves, s_moves;         /* for the click log */
static volatile ULONGLONG s_gmove_at;            /* the cursor glove last moved then */
static CRITICAL_SECTION s_glock;
static INIT_ONCE s_gonce = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK glock_init(PINIT_ONCE o, PVOID p, PVOID *c)
{
    (void)o; (void)p; (void)c;
    InitializeCriticalSection(&s_glock);
    return TRUE;
}

static float fbits(uint32_t b)
{
    float f;
    memcpy(&f, &b, 4);
    return f;
}

static int on_screen(float x, float y)
{
    return x > -100.f && x < 740.f && y > -100.f && y < 580.f;
}

/* Every live glove, now: the one that moved last is the cursor. */
static void glove_poll(void)
{
    ULONGLONG now = GetTickCount64();
    int i, best = -1;
    InitOnceExecuteOnce(&s_gonce, glock_init, NULL, NULL);
    EnterCriticalSection(&s_glock);
    for (i = 0; i < NGLOVES; i++) {
        uint32_t o = s_gloves[i].obj;
        float x, y;
        if (!o)
            continue;
        if (!s_gloves[i].vt)
            s_gloves[i].vt = MEM32(o);
        else if (MEM32(o) != s_gloves[i].vt) {  /* freed, or something else now */
            s_gloves[i].obj = 0;
            continue;
        }
        /* Never placed (built, never moved: drawn position still 0,0, the
         * destination still the build's 320,240): not the cursor. */
        if (!MEM32(o + 0x38) && !MEM32(o + 0x3C))
            continue;
        /* The destination: set the moment the game moves its cursor, while
         * the drawn glove slides there over a few frames. */
        x = fbits(MEM32(o + 0x20));
        y = fbits(MEM32(o + 0x24));
        if (!on_screen(x, y))
            continue;
        if (fabsf(x - s_gloves[i].ax) > MOVE_PX || fabsf(y - s_gloves[i].ay) > MOVE_PX) {
            s_gloves[i].ax = x;
            s_gloves[i].ay = y;
            s_gloves[i].when = now;
        }
        if (s_gloves[i].when && (best < 0 || s_gloves[i].when > s_gloves[best].when))
            best = i;
    }
    if (best < 0) {
        if (s_gobj || s_gx >= 0.f) {    /* no placed glove: not known */
            s_gobj = 0;
            s_gx = s_gy = -1.f;
            InterlockedIncrement(&s_gseq);
        }
    } else {
        uint32_t o = s_gloves[best].obj;
        float x = fbits(MEM32(o + 0x20)) + TIP_DX, y = fbits(MEM32(o + 0x24)) + TIP_DY;
        if (o != s_gobj || fabsf(x - s_gx) > 0.5f || fabsf(y - s_gy) > 0.5f) {
            s_gobj = o;
            s_gx = x;
            s_gy = y;
            InterlockedIncrement(&s_gseq);
            InterlockedIncrement(&s_moves);
            s_gmove_at = now;
        }
    }
    LeaveCriticalSection(&s_glock);
}

void sub_000F80D0(void)
{
    static int next;
    uint32_t obj = g_ecx, p = MEM32(g_esp + 4);
    char name[48];
    int i, glove;
    for (i = 0; i < 47 && p && MEM8(p + i); i++)
        name[i] = (char)MEM8(p + i);
    name[i] = 0;
    glove = strstr(name, "comm_13_") != NULL;
    sub_000F80D0_pad();
    if (glove) {
        InitOnceExecuteOnce(&s_gonce, glock_init, NULL, NULL);
        EnterCriticalSection(&s_glock);
        for (i = 0; i < NGLOVES && s_gloves[i].obj != obj; i++)
            ;
        if (i == NGLOVES) {
            i = next;
            next = (next + 1) % NGLOVES;
            InterlockedIncrement(&s_ngloves);
        }
        s_gloves[i].obj = obj;
        s_gloves[i].vt = 0;             /* taken at the first poll */
        s_gloves[i].ax = fbits(MEM32(obj + 0x38));
        s_gloves[i].ay = fbits(MEM32(obj + 0x3C));
        /* Built is not moved: ranked below any move (tick counts are far
         * larger), the latest built first, so a fresh screen has a cursor
         * before its glove first moves. */
        {
            static ULONGLONG built;
            s_gloves[i].when = ++built;
        }
        LeaveCriticalSection(&s_glock);
    }
}

void sub_000F85F0(void)
{
    /* who moves a sprite (the glove among them): each caller once, to find
     * the Biblioteca's cursor code (2026-11-08) */
    static uint32_t seen[64];
    static unsigned nseen;
    uint32_t ret = MEM32(g_esp), obj = g_ecx;
    unsigned i;
    for (i = 0; i < nseen && seen[i] != ret; i++)
        ;
    if (i == nseen && nseen < 64) {
        seen[nseen++] = ret;
        fprintf(stderr, "[RATON] move-to from %08X obj %08X args %08X %08X%s\n", ret, obj,
                MEM32(g_esp + 4), MEM32(g_esp + 8), obj == s_gobj ? " (the cursor glove)" : "");
        fflush(stderr);
    }
    sub_000F85F0_pad();
}


/* ---- taking the game's cursor to the pointer ----------------------------- */

/* The game's cursor sits on a few fixed places per screen (squares, cards,
 * letters, buttons), and which d-pad key leads where is the game's own
 * business: down from the spells row may not reach the hand from every
 * column, the hand wraps, a menu skips. Steering one axis at a time and
 * giving an axis up when a key did not bring the cursor closer (until
 * 11:29) left it on the wrong row (the pointer on the hand at y 366, the
 * cursor on the field at y 287). So the places are learnt as the cursor
 * visits them, with what each key did from each: a map. Each decision
 * takes the known place nearest the pointer, follows the shortest known
 * way there, and from it tries the keys not yet known that point toward
 * the pointer. A key's outcome is relearnt whenever it turns out other
 * than the map says; "did nothing" is forgotten after BLOCK_MS (the game
 * ignores the pad during its animations). */
#define ANSWER_MS 250u                  /* a press's answer, at the latest */
#define SETTLE_MS 30u                   /* no further change: the answer */
#define NPLACES 256
#define PLACE_PX 4.f                    /* the same place */
#define EXPLORE_PX 14.f                 /* the pointer is that way */
#define BLOCK_MS 3000u

enum { K_UP, K_DOWN, K_LEFT, K_RIGHT };
static const WORD k_key[4] = {
    XBOX_GAMEPAD_DPAD_UP, XBOX_GAMEPAD_DPAD_DOWN, XBOX_GAMEPAD_DPAD_LEFT, XBOX_GAMEPAD_DPAD_RIGHT };

static struct {
    float x, y;
    short next[4];                      /* -1 unknown; itself: did nothing */
    ULONGLONG blocked_at[4];
} s_pl[NPLACES];
static int s_npl, s_plnext;
/* The smallest step seen across (0) and up/down (1): a pointer less than
 * half of it away is on the cursor's place already, nothing to try. */
static float s_space[2];

static int place_of(float x, float y)
{
    int i;
    for (i = 0; i < s_npl; i++)
        if (fabsf(s_pl[i].x - x) <= PLACE_PX && fabsf(s_pl[i].y - y) <= PLACE_PX)
            return i;
    if (s_npl < NPLACES)
        i = s_npl++;
    else {                              /* full: the oldest goes */
        int j, k;
        i = s_plnext;
        s_plnext = (s_plnext + 1) % NPLACES;
        for (j = 0; j < s_npl; j++)
            for (k = 0; k < 4; k++)
                if (s_pl[j].next[k] == i)
                    s_pl[j].next[k] = -1;
    }
    s_pl[i].x = x;
    s_pl[i].y = y;
    s_pl[i].next[0] = s_pl[i].next[1] = s_pl[i].next[2] = s_pl[i].next[3] = -1;
    return i;
}

/* What key k does from place p, as far as known now: -1 unknown. */
static int edge(int p, int k, ULONGLONG t)
{
    int q = s_pl[p].next[k];
    if (q == p && t - s_pl[p].blocked_at[k] > BLOCK_MS)
        return -1;
    return q;
}

/* The key to press now to bring the cursor (place c) toward the pointer
 * (px, py), or -1: it is as near as it can be. The places reachable from c
 * by known keys are searched (breadth first: fewest presses); the nearest
 * to the pointer is the target (places left from other screens are never
 * reached, so never chosen). There, the keys not known yet that point
 * toward the pointer are tried, the one with the most to cover first. */
static int next_key(int c, float px, float py, ULONGLONG t)
{
    static short prev[NPLACES], key[NPLACES], queue[NPLACES];
    int head = 0, tail = 0, i, k, target = c, pick = -1;
    float best = 1e9f, dx, dy, most = 0.f;

    for (i = 0; i < s_npl; i++)
        prev[i] = -1;
    prev[c] = (short)c;
    queue[tail++] = (short)c;
    while (head < tail) {
        int p = queue[head++];
        float d = (s_pl[p].x - px) * (s_pl[p].x - px) + (s_pl[p].y - py) * (s_pl[p].y - py);
        if (d < best - 1.f) {           /* ties: the one fewer presses away */
            best = d;
            target = p;
        }
        for (k = 0; k < 4; k++) {
            int q = edge(p, k, t);
            if (q < 0 || q == p || prev[q] >= 0)
                continue;
            prev[q] = (short)p;
            key[q] = (short)k;
            queue[tail++] = (short)q;
        }
    }
    if (target != c) {
        for (i = target; prev[i] != c; i = prev[i])
            ;
        return key[i];
    }
    dx = px - s_pl[c].x;
    dy = py - s_pl[c].y;
    for (k = 0; k < 4; k++) {
        float along = k == K_UP ? -dy : k == K_DOWN ? dy : k == K_LEFT ? -dx : dx;
        float need = s_space[k >= K_LEFT ? 0 : 1] > 0.f
                     ? s_space[k >= K_LEFT ? 0 : 1] / 2.f : EXPLORE_PX;
        if (along < need || edge(c, k, t) >= 0)
            continue;
        if (along > most) {
            most = along;
            pick = k;
        }
    }
    return pick;
}

/* ---- the duel field: the pointer sets the game's cursor ----------------- */

/* The duel's field cursor (player [0x6633D4], zone [0x6633D8], square
 * [0x6633DC]) is moved by one function, sub_00092FA0, which the field's
 * per-frame update (sub_00099660) calls whenever the field cursor has the
 * pad: it reads the keys pressed this frame ([0x6589E6]: 0x10..0x80 the
 * directions, 1 A), works out the neighbouring square, sets it with
 * sub_00086F50(player, zone, square) and plays the cursor tick
 * (sub_0007E280(0x10000001)); it returns 0, or 1 when A was pressed.
 *
 * It is wrapped: while it runs the field cursor is live (s_field_at), and
 * when the pointer has gone over another square and no key is down this
 * frame, the wrapper does what the game does for a d-pad press, but to the
 * square under the pointer: sub_00086F50 with it, the tick, return 0. No
 * steering, no pad presses: the cursor is where the pointer is, at once.
 * Everything else (keys, A, other screens) is the game's own handler.
 * recomp's definition is renamed _pad (tools/rename_pads.py). */
void sub_00092FA0_pad(void);
void sub_00086F50(void);
void sub_0007E280(void);

static volatile ULONGLONG s_field_at;            /* the field cursor ran then */
static volatile LONG s_want_seq;                 /* bumps on a new square */
static volatile int s_want_p, s_want_z, s_want_q;
static volatile LONG s_done_seq;

static int field_valid(int p, int z, int q)
{
    if (p < 0 || p > 1)
        return 0;
    switch (z) {
    case 0: case 5:  return q >= 0 && q < 5;
    case 11:         return q >= 0 && q < hand_count(p);
    case 10: case 12: case 13: case 14: case 15: return q == 0;
    }
    return 0;
}

/* Learn where the game shows the place the cursor is on: the glove's
 * fingertip once the cursor has stayed there a little. A hand whose card
 * count changed has its places relearnt. */
static void field_learn(ULONGLONG t)
{
    static uint32_t last[3];
    static ULONGLONG since;
    static LONG seq_at;
    static int counts[2];
    uint32_t c[3];
    int p;
    for (p = 0; p < 2; p++)
        if (hand_count(p) != counts[p]) {
            counts[p] = hand_count(p);
            memset(s_learnt[p][11], 0, sizeof s_learnt[p][11]);
        }
    c[0] = MEM32(CUR_PLAYER); c[1] = MEM32(CUR_ZONE); c[2] = MEM32(CUR_SQUARE);
    if (memcmp(c, last, sizeof c)) {
        memcpy(last, c, sizeof c);
        since = t;
        seq_at = s_gseq;
        return;
    }
    /* Only the hand is learnt (the field's measured centres match the game
     * within a few pixels; 11:47: relearning while the glove was still on
     * its way, or from another glove, put a card's place between two cards,
     * and a still pointer made the cursor go back and forth). A hand card
     * is learnt once per card count, with the glove at rest for 400 ms
     * since it moved there, and only near where the card should be. */
    {
        static LONG rest_seq;
        static ULONGLONG rest_at;
        float dx, dy;
        if (s_gseq != rest_seq) {
            rest_seq = s_gseq;
            rest_at = t;
        }
        if (c[1] == 11 && c[0] < 2 && c[2] < HAND_MAX && !s_learnt[c[0]][11][c[2]]
                && s_gseq != seq_at && t - rest_at >= 400u && t - since >= 400u
                && s_gx >= 0.f && slot_place((int)c[0], 11, (int)c[2], &dx, &dy)
                && fabsf(s_gx - dx) < 60.f && fabsf(s_gy - dy) < 50.f) {
            s_learn[c[0]][11][c[2]][0] = s_gx;
            s_learn[c[0]][11][c[2]][1] = s_gy;
            s_learnt[c[0]][11][c[2]] = 1;
        }
    }
}

void sub_00092FA0(void)
{
    ULONGLONG t = GetTickCount64();
    s_field_at = t;
    field_learn(t);
    /* Evidence for the next video: the cursor leaving the wanted square
     * with no new want (the game itself moved it). */
    if (s_mode == MODE_POINTER && s_want_seq == s_done_seq) {
        static uint32_t was[3];
        static int logged;
        uint32_t c[3] = { MEM32(CUR_PLAYER), MEM32(CUR_ZONE), MEM32(CUR_SQUARE) };
        if (memcmp(c, was, sizeof c)) {
            if (was[1] == (uint32_t)s_want_z && was[2] == (uint32_t)s_want_q
                    && was[0] == (uint32_t)s_want_p && logged < 60) {
                logged++;
                fprintf(stderr, "[RATON] the game moved the cursor p%u z%u i%u -> p%u z%u i%u\n",
                        was[0], was[1], was[2], c[0], c[1], c[2]);
            }
            memcpy(was, c, sizeof c);
        }
    }
    /* A wanted square stays pending until it is done: a frame with a key
     * down only delays it (until 15:xx it was dropped there, and the cursor
     * stayed put until the pointer went over another square). */
    if (s_mode == MODE_POINTER && s_want_seq != s_done_seq) {
        LONG seq = s_want_seq;
        int p = s_want_p, z = s_want_z, q = s_want_q;
        int there = (int)MEM32(CUR_PLAYER) == p && (int)MEM32(CUR_ZONE) == z
                    && (int)MEM32(CUR_SQUARE) == q;
        if (there || !field_valid(p, z, q))
            s_done_seq = seq;
        else if ((MEM16(0x006589E6u) & 0xFFu) == 0) {
            s_done_seq = seq;
            uint32_t sb = g_ebx, si = g_esi, di = g_edi;
            PUSH32(g_esp, (uint32_t)q);
            PUSH32(g_esp, (uint32_t)z);
            PUSH32(g_esp, (uint32_t)p);
            PUSH32(g_esp, 0x0009342Au);          /* where the game returns to */
            sub_00086F50();
            g_esp += 12;
            PUSH32(g_esp, 0x10000001u);
            PUSH32(g_esp, 0x00093434u);
            sub_0007E280();
            g_esp += 4;
            g_ebx = sb; g_esi = si; g_edi = di;
            g_eax &= 0xFFFF0000u;               /* ax = 0: moved, no A */
            g_esp += 4;                         /* ret */
            return;
        }
    }
    sub_00092FA0_pad();
}

/* Choosing a card on the field (the target of an attack, of an effect...):
 * 2026-11-09, the user: "al seleccionar un monstruo enemigo para atacar
 * quedaron desfasados". That cursor is not sub_00092FA0's: it is
 * sub_00092E20(mask), called from 30 places (0x847B4, 0x96E27...), which
 * only stops on the squares sub_00092620 accepts (eax = zone; player,
 * square, mask) and moves with sub_00092B20 to the next acceptable one; it
 * returns 1 on A. The pointer did not reach it at all: the cursor stayed on
 * the game's first target and the click confirmed that one.
 * Wrapped like sub_00092FA0: the square under the pointer, if the game
 * accepts it, becomes the cursor (sub_00086F50, tick, return 0); one it does
 * not accept is dropped (s_pick_bad), and field_pointer then presses no A. */
void sub_00092E20_pad(void);
void sub_00092620(void);
static volatile ULONGLONG s_pick_at;             /* the target cursor ran then */
static volatile LONG s_pick_bad = -1;            /* want seq the game refused */

uint32_t ygo_cartas_mascara_cadena(uint32_t mask);   /* cartas.c: Naturia Vanidosa */
void sub_00092E20(void)
{
    if (MEM32(g_esp) == 0x000824A9u)          /* the chain pick (sub_00082280) */
        MEM32(g_esp + 4u) = ygo_cartas_mascara_cadena(MEM32(g_esp + 4u));
    ULONGLONG t = GetTickCount64();
    s_field_at = t;
    s_pick_at = t;
    if (s_mode == MODE_POINTER && s_want_seq != s_done_seq) {
        LONG seq = s_want_seq;
        int p = s_want_p, z = s_want_z, q = s_want_q;
        uint32_t mask = MEM32(g_esp + 4u);
        int there = (int)MEM32(CUR_PLAYER) == p && (int)MEM32(CUR_ZONE) == z
                    && (int)MEM32(CUR_SQUARE) == q;
        if (there)
            s_done_seq = seq;
        else if (!field_valid(p, z, q)) {
            s_done_seq = seq;
            s_pick_bad = seq;
        } else if ((MEM16(0x006589E6u) & 0xFFu) == 0) {
            uint32_t sa = g_eax, sc = g_ecx, sd = g_edx;
            uint32_t sb = g_ebx, si = g_esi, di = g_edi;
            int ok;
            s_done_seq = seq;
            g_eax = (uint32_t)z;
            PUSH32(g_esp, mask);
            PUSH32(g_esp, (uint32_t)q);
            PUSH32(g_esp, (uint32_t)p);
            PUSH32(g_esp, 0x00092E82u);
            sub_00092620();
            g_esp += 12;
            ok = (g_eax & 0xFFFFu) != 0;
            g_eax = sa; g_ecx = sc; g_edx = sd;
            if (ok) {
                PUSH32(g_esp, (uint32_t)q);
                PUSH32(g_esp, (uint32_t)z);
                PUSH32(g_esp, (uint32_t)p);
                PUSH32(g_esp, 0x00092EBDu);
                sub_00086F50();
                g_esp += 12;
                PUSH32(g_esp, 0x10000001u);
                PUSH32(g_esp, 0x00092EF7u);
                sub_0007E280();
                g_esp += 4;
                g_ebx = sb; g_esi = si; g_edi = di;
                g_eax &= 0xFFFF0000u;           /* ax = 0: moved, no A */
                g_esp += 4;                     /* ret */
                return;
            }
            g_ebx = sb; g_esi = si; g_edi = di;
            s_pick_bad = seq;
            fprintf(stderr, "[RATON] target: p%d z%d i%d not a choice here (mask %08X)\n", p, z, q, mask);
            fflush(stderr);
        }
    }
    sub_00092E20_pad();
}

/* The field's part of pointer mode: the square under the pointer is what
 * the wrapper above puts the cursor on; a left click presses A once that
 * is done. 1 when the field cursor is live (the rest is skipped). */
/* Joey the Passion's clicks on a card in the hand: left does its main
 * action (summon, else activate or another, else set), right sets it face
 * down, with no round buttons in between. The click selects the card and
 * presses A as before, which opens the buttons; s_auto tells the buttons'
 * wrapper (sub_00080D40) to pick that command there and confirm it at once
 * (AUTO_MAIN / AUTO_SET), within AUTO_MS of the click. */
#define AUTO_MAIN 1
#define AUTO_SET  2
#define AUTO_INFO 3                     /* middle button: "Información de la carta" */
#define AUTO_MS   700u
static volatile LONG s_auto;
static volatile ULONGLONG s_auto_at;
/* The card's info panel ("Información de la carta": the buttons' step
 * [0x65F8FC] = 0xA after A on command 0) was up then; the field does not
 * run under it. Clicks there only go back (B), and for INFO_QUIET_MS after
 * it closes a click on the hand does nothing: players clicked to leave the
 * panel and summoned the card under the pointer (2026-11-08). */
#define INFO_QUIET_MS 500u
static volatile ULONGLONG s_info_at;

/* Fast card plays (F2 "Cartas rápidas", on by default): from the moment the
 * game takes a play (a command other than info confirmed on the buttons, or
 * A on a field square while choosing where) the guest clock runs at
 * BOOST_SPEED (xbox_SpeedSet, the same warp as RDR's Ctrl+F8), until the
 * field cursor waits for keys again (sub_00092FA0 running for BOOST_IDLE_MS)
 * or the buttons do, at most BOOST_MAX_MS. The summon, set or activation
 * animation goes by in a moment; the rest of the duel keeps its pace. */
extern void xbox_SpeedSet(unsigned speed);
extern unsigned xbox_Speed(void);
#define BOOST_SPEED   4u
#define BOOST_IDLE_MS 120u
#define BOOST_MIN_MS  250u
#define BOOST_MAX_MS  4000u
/* 2026-11-09: off for good (the user: the Joey clicks were removed but the
 * speed-up on a card click stayed); the F2 row is gone too. */
static volatile int s_fast_cards = 0;
static volatile LONG s_boost;                    /* 1 while sped up */
static volatile ULONGLONG s_boost_at;

void ygo_fast_cards_set(int on) { (void)on; }   /* a saved "Sí" does nothing */
int ygo_fast_cards_get(void) { return s_fast_cards; }

static void boost_start(const char *why)
{
    if (!s_fast_cards || s_mode != MODE_POINTER)
        return;
    s_boost_at = GetTickCount64();
    if (InterlockedExchange(&s_boost, 1) == 0) {
        if (xbox_Speed() == 1u)
            xbox_SpeedSet(BOOST_SPEED);
        fprintf(stderr, "[RATON] fast play on (%s)\n", why);
        fflush(stderr);
    }
}

static void boost_stop(const char *why)
{
    if (InterlockedExchange(&s_boost, 0) == 1) {
        if (xbox_Speed() == BOOST_SPEED)
            xbox_SpeedSet(1u);
        fprintf(stderr, "[RATON] fast play off (%s)\n", why);
        fflush(stderr);
    }
}

static void mouse_a_off(XBOX_INPUT_STATE *n, ULONGLONG t);

static int field_pointer(XBOX_INPUT_STATE *n, ULONGLONG t, int over, int ledge, float gx, float gy)
{
    static int have, wp, wz, wq, rset, click_pending, on;
    static float lx, ly;
    static ULONGLONG click_at, a_until;
    if (t - s_field_at > 150u) {
        click_pending = 0;              /* never kept for the next visit */
        return 0;
    }
    /* 2026-11-17 (mouse review): the kit's own A of the left button stays
     * out here, as on the menus: it went in at once on whatever square the
     * cursor was on, so a click between the squares acted on the last card
     * hovered, and a target the game does not offer was confirmed anyway
     * ("target: click dropped" stopped only our A). Only the A below, once
     * the cursor is on the clicked square, reaches the game. */
    mouse_a_off(n, t);
    /* No direction reaches the field from anywhere while the mouse runs
     * it (2026-11-08 18:57 log: with the pointer still on hand card 1, the
     * field update itself stepped the cursor to card 2 ten times in a row:
     * sub_00092FA0 moves the hand only on a direction edge in [0x6589E6];
     * those edges also held our own moves back). The pointer's square is the
     * only way the cursor moves; the wheel's steps (s_held, added after
     * this) still go through. */
    {
        WORD dirs = XBOX_GAMEPAD_DPAD_UP | XBOX_GAMEPAD_DPAD_DOWN
                    | XBOX_GAMEPAD_DPAD_LEFT | XBOX_GAMEPAD_DPAD_RIGHT;
        static ULONGLONG said_at;
        if ((n->Gamepad.wButtons & dirs) || n->Gamepad.sThumbLX || n->Gamepad.sThumbLY
                || n->Gamepad.sThumbRX || n->Gamepad.sThumbRY) {
            if (t - said_at >= 1000u) {
                said_at = t;
                fprintf(stderr, "[RATON] field: direction input dropped (buttons %04X, left %d,%d,"
                        " right %d,%d)\n", n->Gamepad.wButtons & dirs,
                        n->Gamepad.sThumbLX, n->Gamepad.sThumbLY,
                        n->Gamepad.sThumbRX, n->Gamepad.sThumbRY);
            }
            n->Gamepad.wButtons &= (WORD)~dirs;
            n->Gamepad.sThumbLX = n->Gamepad.sThumbLY = 0;
            n->Gamepad.sThumbRX = n->Gamepad.sThumbRY = 0;
        }
        if ((s_held & dirs) && !s_held_wheel) {
            fprintf(stderr, "[RATON] field: our own d-pad press %04X dropped\n", s_held);
            s_held = 0;
        }
    }
    if (over) {
        Slot s;
        float cx, cy;
        int moved = fabsf(gx - lx) > 3.f || fabsf(gy - ly) > 3.f;
        /* Still inside the square already wanted: it stays (no flipping
         * between two neighbours at a border). */
        int keep = have && slot_place(wp, wz, wq, &cx, &cy)
                   /* hand: the middle between two cards, 2 px of margin */
                   && fabsf(gx - cx) <= (wz == 11 ? hand_step() / 2.f + 2.f : 22.f)
                   && fabsf(gy - cy) <= (wz == 11 ? 55.f : 18.f);
        if (!keep) {
            on = slot_at(gx, gy, &s);
            if (on && (!have || s.player != wp || s.zone != wz || s.square != wq)) {
                have = 1;
                wp = s.player; wz = s.zone; wq = s.square;
                moved = 1;
                fprintf(stderr, "[RATON] hover game %.0f,%.0f -> p%d z%d i%d (hand %d), cursor was p%u z%u i%u\n",
                        gx, gy, wp, wz, wq, hand_count(0),
                        MEM32(CUR_PLAYER), MEM32(CUR_ZONE), MEM32(CUR_SQUARE));
            }
        } else
            on = 1;
        /* Sent again whenever the pointer moves on a square: if the game
         * moved the cursor meanwhile, it comes back under the pointer. */
        if (on && have && moved) {
            lx = gx;
            ly = gy;
            s_want_p = wp; s_want_z = wz; s_want_q = wq;
            InterlockedIncrement(&s_want_seq);
        }
        {
            int quiet = t - s_info_at < INFO_QUIET_MS;  /* the info panel just closed */
            int my_hand = on && have && wp == 0 && wz == 11 && !quiet;
            int rdown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
            if (ledge && quiet) {
                fprintf(stderr, "[RATON] field click ignored (card info just closed)\n");
                fflush(stderr);
            } else if (ledge && !on) {
                /* 2026-11-17: off every square (the middle of the field,
                 * the life points...) a click does nothing */
                fprintf(stderr, "[RATON] field click game %.0f,%.0f off the squares, nothing\n", gx, gy);
                fflush(stderr);
            } else if (ledge) {
                click_pending = 1;
                click_at = t;
                if (on && have && wp == 0 && (wz == 0 || wz == 5))
                    boost_start("square chosen");   /* where a card goes */
                fprintf(stderr, "[RATON] field click game %.0f,%.0f -> want p%d z%d i%d,"
                        " cursor p%u z%u i%u, hand %d%s\n", gx, gy, wp, wz, wq,
                        MEM32(CUR_PLAYER), MEM32(CUR_ZONE), MEM32(CUR_SQUARE), hand_count(0),
                        my_hand ? " (hand card)" : "");
                fflush(stderr);
            }
            /* Right on a card in the hand: set it face down (A, then the
             * wrapper picks "Colocar"); anywhere else, B as before. */
            if (s_redge) {
                rset = 0;       /* one-click set removed 2026-11-08: right is B */
                if (rset) {
                    click_pending = 1;
                    click_at = t;
                    s_auto_at = GetTickCount64();
                    s_auto = AUTO_SET;
                    fprintf(stderr, "[RATON] field right click on hand card %d -> set\n", wq);
                    fflush(stderr);
                }
            }
            /* 2026-11-09 ("a veces tengo que apretar 2 veces el segundo
             * clic"; log 02:26: a quick right click, down and up in one
             * poll, opened nothing, a held one did): B is held at least
             * B_HOLD_MS from the press, as A is, so the game's frame sees it */
            {
                static ULONGLONG b_until;
                /* the press seen once per poll for every screen (s_redge) */
                if (s_redge && !rset && !quiet) {
                    b_until = t + B_HOLD_MS;
                    fprintf(stderr, "[RATON] field right click -> B (%s)\n",
                            rdown ? "held" : "quick, from the hook");
                    fflush(stderr);
                }
                if ((rdown && !rset && !quiet) || t < b_until)
                    n->Gamepad.bAnalogButtons[XBOX_BUTTON_B] = 255;
            }
            if (!rdown)
                rset = 0;
            (void)my_hand;
        }
        /* Middle button on any card: its "Información de la carta" at
         * once (A opens the buttons, the wrapper picks command 0). */
        {
            static int mwas_down;
            int mdown = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
            if (mdown && !mwas_down && on && have) {
                click_pending = 1;
                click_at = t;
                s_auto_at = GetTickCount64();
                s_auto = AUTO_INFO;
                fprintf(stderr, "[RATON] field middle click p%d z%d i%d -> card info\n", wp, wz, wq);
                fflush(stderr);
            }
            mwas_down = mdown;
        }
    }
    /* A once the cursor is on the clicked square (the wrapper ran), at most
     * 150 ms later. */
    if (click_pending && (s_done_seq == s_want_seq || t - click_at >= 150u)) {
        click_pending = 0;
        /* choosing a target (sub_00092E20): a click on a square the game
         * does not offer confirms nothing (it confirmed the game's own
         * first target before) */
        if (t - s_pick_at < 150u && (!on || s_pick_bad == s_want_seq
                || (int)MEM32(CUR_PLAYER) != wp || (int)MEM32(CUR_ZONE) != wz
                || (int)MEM32(CUR_SQUARE) != wq)) {
            fprintf(stderr, "[RATON] target: click on p%d z%d i%d dropped (cursor p%u z%u i%u)\n",
                    wp, wz, wq, MEM32(CUR_PLAYER), MEM32(CUR_ZONE), MEM32(CUR_SQUARE));
            fflush(stderr);
        } else
            a_until = t + PRESS_MS + 15u;
    }
    if (t < a_until)
        n->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = 255;
    /* The wheel still steps (lists over the field). */
    if (s_wheel && s_held == 0) {
        LONG w = s_wheel;
        if (press(w > 0 ? XBOX_GAMEPAD_DPAD_UP : XBOX_GAMEPAD_DPAD_DOWN, t))
            { InterlockedExchangeAdd(&s_wheel, w > 0 ? -1 : 1); s_held_wheel = 1; }
    }
    return 1;
}

/* ---- the card lists over the field (graveyard, fusion deck...) ---------- */

/* 2026-11-09, the user: "en el campo de batalla los cementerios no se pueden
 * usar con el mouse". A on zones 12/14/15 (sub_00099660, table at 0x99928)
 * opens a list with sub_000C5AE0 when it has cards: [0x6589DC] how many,
 * 8 bytes each from 0x65855C. sub_000C5710 runs it: [0x658552] is its step
 * (table 0xC5A4C: 0 browsing, 1 a card picked); in step 0 key 0x20 goes one
 * card back and 0x10 one on ([0x658556], sub_000483E0 and the tick), 1 picks
 * or closes, 2 (B) closes. The field cursor does not run meanwhile, so the
 * wheel went to the generic up/down, which this list ignores.
 * Wrapped: a wheel step becomes 0x20/0x10 for one call of the handler (the
 * game's own move, bounds and sound); list_pointer gives the clicks A and B. */
void sub_000C5710_pad(void);
static volatile ULONGLONG s_list_at;             /* the list ran then */
static volatile LONG s_list_steps;               /* wheel steps: + on, - back */
/* step 1 (a card picked): its two round buttons, as the Target List's:
 * [0x658558] & 3 the one marked, bit 0x20 that there are two (0x10 / 0x20
 * switch), A on 0 (the magnifier) the info, on 1 (the gear) the card. */
static volatile LONG s_list_want = -1;           /* 0/1 a button, then A */
static volatile LONG s_list_hover = -1;

void sub_000C5710(void)
{
    uint16_t keys = MEM16(0x006589E6u);
    uint16_t before = MEM16(0x00658556u);
    int inj = 0;
    s_list_at = GetTickCount64();
    if ((MEM32(0x00658552u) & 0xFFu) == 1u && (keys & 0xFFu) == 0u) {
        uint32_t b = MEM8(0x00658558u) & 3u, two = (MEM8(0x00658558u) & 0x20u) != 0u;
        LONG w = s_list_want, hv = s_list_hover;
        uint16_t k = 0;
        if (w >= 0) {
            k = (uint32_t)w == b || !two ? 1 : 0x10;
            if (k == 1)
                InterlockedExchange(&s_list_want, -1);
        } else if (two && hv >= 0 && (uint32_t)hv != b)
            k = 0x10;
        if (k) {
            MEM16(0x006589E6u) = (uint16_t)(keys | k);
            sub_000C5710_pad();
            MEM16(0x006589E6u) = keys;
            fprintf(stderr, "[RATON] list buttons: key %02X (button %u)\n", k, b);
            fflush(stderr);
            return;
        }
    }
    if ((MEM32(0x00658552u) & 0xFFu) == 0u && (keys & 0xFFu) == 0u && s_list_steps) {
        inj = s_list_steps > 0 ? 0x10 : 0x20;
        InterlockedExchangeAdd(&s_list_steps, inj == 0x10 ? -1 : 1);
        MEM16(0x006589E6u) = (uint16_t)(keys | inj);
    }
    sub_000C5710_pad();
    if (inj) {
        uint16_t after = MEM16(0x00658556u);
        int edge = inj == 0x20 ? before == 0 : (int)before + 1 >= (int)MEM16(0x006589DCu);
        MEM16(0x006589E6u) = keys;
        /* not taken (the list still sliding): the step waits for the next call */
        if (after == before && !edge)
            InterlockedExchangeAdd(&s_list_steps, inj == 0x10 ? 1 : -1);
        else {
            fprintf(stderr, "[RATON] list: wheel %s -> card %u of %u\n",
                    inj == 0x10 ? "on" : "back", after, MEM16(0x006589DCu));
            fflush(stderr);
        }
    }
}

static void mouse_a_off(XBOX_INPUT_STATE *n, ULONGLONG t);

/* Where the list's cards are (screenshots 2026-11-10, Grave List, picture
 * 1919x1079 = 640x480 game px): the card shown, x 273..368, y 70..208; the
 * next one to its right, x 395..490, y 91..229; the one before it is the
 * same on the left (x 151..246). A click on the middle card is A, on a side
 * card a step there (the game's own, as the wheel), anywhere else nothing.
 * Off step 0 (a card was picked) a click is A, as before. */
#define LIST_MID_X0 273.f
#define LIST_MID_X1 368.f
#define LIST_MID_Y0 70.f
#define LIST_MID_Y1 208.f
#define LIST_SIDE_X0 395.f
#define LIST_SIDE_X1 490.f
#define LIST_SIDE_Y0 91.f
#define LIST_SIDE_Y1 229.f

static int list_pointer(XBOX_INPUT_STATE *n, ULONGLONG t, int over, int ledge, float gx, float gy)
{
    static ULONGLONG a_until, b_until, seen;
    int rdown;
    if (t - s_list_at > 150u)
        return 0;
    mouse_a_off(n, t);
    if (t - seen > 500u) {
        InterlockedExchange(&s_list_steps, 0);
        InterlockedExchange(&s_list_want, -1);   /* no pick left from a list before */
        fprintf(stderr, "[RATON] list open (%u cards, step %u)\n",
                MEM16(0x006589DCu), MEM32(0x00658552u) & 0xFFu);
        fflush(stderr);
    }
    seen = t;
    if (s_wheel) {
        LONG w = InterlockedExchange(&s_wheel, 0);
        InterlockedExchangeAdd(&s_list_steps, -w);     /* wheel up = back */
    }
    rdown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    {
        /* step 1: the magnifier x 272..315, the gear x 324..367, y 238..280 */
        int hv = -1, abajo = -1;
        if (over && (MEM32(0x00658552u) & 0xFFu) == 1u && gy >= 238.f && gy <= 280.f)
            hv = gx >= 272.f && gx <= 315.f ? 0 : gx >= 324.f && gx <= 367.f ? 1 : -1;
        /* 2026-11-20 22:1x (the user's capture of the graveyard: "no funciona
         * el boton de abajo que dice Ver carta"): here the two buttons are in
         * the bar at the bottom left, "Ver carta" (button 0, the card's info)
         * x 58..205, y 346..381 and the tombstone (button 1) x 213..249 (game
         * px of the 1919x1079 capture). Step 1: that button, then A; step 0
         * (the bar under the card being browsed): A picks the card and the
         * button is pressed once step 1 shows them. */
        if (over && gy >= 346.f && gy <= 381.f)
            abajo = gx >= 58.f && gx <= 205.f ? 0 : gx >= 213.f && gx <= 249.f ? 1 : -1;
        if (hv < 0 && abajo >= 0 && (MEM32(0x00658552u) & 0xFFu) == 1u)
            hv = abajo;
        InterlockedExchange(&s_list_hover, hv);
        if (over && ledge && hv >= 0) {
            InterlockedExchange(&s_list_want, hv);
            fprintf(stderr, "[RATON] list click game %.0f,%.0f -> %s\n", gx, gy,
                    abajo >= 0 ? (hv ? "tombstone (bottom bar)" : "Ver carta (bottom bar)") : hv ? "gear" : "magnifier");
            fflush(stderr);
            ledge = 0;
        } else if (over && ledge && abajo >= 0 && (MEM32(0x00658552u) & 0xFFu) == 0u) {
            InterlockedExchange(&s_list_want, abajo);
            a_until = t + PRESS_MS + 15u;
            fprintf(stderr, "[RATON] list click game %.0f,%.0f -> A, then %s\n", gx, gy,
                    abajo ? "tombstone (bottom bar)" : "Ver carta (bottom bar)");
            fflush(stderr);
            ledge = 0;
        }
    }
    {
        /* 2026-11-20 22:2x (the user's capture: "podes hacer que la flecha
         * funcione? ... si se deja apretado vaya rapido"): the yellow arrow
         * left of the cards, x 47..68, y 142..178 game px (some margin
         * here), is one card back; the side cards one step there. Held down
         * on any of them, after 400 ms a step every 90 ms (only once the
         * last one was taken, so nothing piles up after letting go). */
        static ULONGLONG hold_from, hold_last;
        static int hold_dir;
        int step0 = (MEM32(0x00658552u) & 0xFFu) == 0u;
        int ldown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        int dir = 0;
        int steps = 0;
        /* 22:2x log: clicks on the second card to the right (581,152 and
         * 525,132) did nothing; the capture shows two cards each side:
         * x 37..133 and 150..246 left, 395..490 and 508..603 right */
        if (over && step0) {
            if (gx >= 42.f && gx <= 74.f && gy >= 136.f && gy <= 184.f)
                steps = -1;
            /* the right arrow, mirrored (x 572..593): the 22:23 log, clicks
             * at 581,156 and 581,152 with the list on card 0 gave nothing */
            else if (gx >= 566.f && gx <= 598.f && gy >= 136.f && gy <= 184.f)
                steps = 1;
            else if (gy >= LIST_SIDE_Y0 && gy <= LIST_SIDE_Y1)
                steps = gx >= 37.f && gx <= 133.f ? -2 : gx >= 150.f && gx <= 246.f ? -1
                      : gx >= 395.f && gx <= 490.f ? 1 : gx >= 508.f && gx <= 603.f ? 2 : 0;
        }
        dir = steps > 0 ? 1 : steps < 0 ? -1 : 0;
        if (ledge && dir) {
            InterlockedExchangeAdd(&s_list_steps, steps);
            hold_from = hold_last = t;
            hold_dir = dir;
            fprintf(stderr, "[RATON] list click game %.0f,%.0f -> %s (card %u)\n", gx, gy,
                    steps == 1 && gx > 540.f ? "next card (arrow)" : dir > 0 ? "cards on" : steps == -1 && gx < 100.f ? "previous card (arrow)" : "cards back",
                    MEM16(0x00658556u));
            fflush(stderr);
            ledge = 0;
        } else if (!ldown || dir != hold_dir)
            hold_from = 0;
        else if (hold_from && t - hold_from >= 400u && t - hold_last >= 90u && s_list_steps == 0) {
            InterlockedExchangeAdd(&s_list_steps, dir);
            hold_last = t;
        }
    }
    if (over && ledge) {
        const char *what;
        if ((MEM32(0x00658552u) & 0xFFu) != 0u
                || (gx >= LIST_MID_X0 && gx <= LIST_MID_X1 && gy >= LIST_MID_Y0 && gy <= LIST_MID_Y1)) {
            what = "A";
            a_until = t + PRESS_MS + 15u;
        } else if (gx >= LIST_SIDE_X0 && gx <= LIST_SIDE_X1 && gy >= LIST_SIDE_Y0 && gy <= LIST_SIDE_Y1) {
            what = "next card";
            InterlockedExchangeAdd(&s_list_steps, 1);
        } else if (gx >= 640.f - LIST_SIDE_X1 && gx <= 640.f - LIST_SIDE_X0
                   && gy >= LIST_SIDE_Y0 && gy <= LIST_SIDE_Y1) {
            what = "previous card";
            InterlockedExchangeAdd(&s_list_steps, -1);
        } else
            what = "nothing";
        fprintf(stderr, "[RATON] list click game %.0f,%.0f -> %s (card %u, step %u)\n",
                gx, gy, what, MEM16(0x00658556u), MEM32(0x00658552u) & 0xFFu);
        fflush(stderr);
    }
    if (over && s_redge) {
        b_until = t + B_HOLD_MS;
        fprintf(stderr, "[RATON] list right click -> B\n");
        fflush(stderr);
    }
    if (t < a_until)
        n->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = 255;
    if (t < b_until || (over && rdown))
        n->Gamepad.bAnalogButtons[XBOX_BUTTON_B] = 255;
    return 1;
}

/* ---- the "Target List" (bring a monster, take one back...) ------------- */

/* 2026-11-10, the user (screenshots vs. Yami Yugi): "no anda el click ni en
 * esta ni los botones de abajo". sub_000920B0 runs it (same card-list
 * widget as the graveyard's, sub_00047xxx): [0x65F92E] is its step, the
 * cards at 0x65F91E (16-bit ids), keys the same word [0x6589E6]:
 *   2  browsing: 0x20 one card back, 0x10 one on, 1 (A) -> 3;
 *   3  the two round buttons under it: 0x10 / 0x20 switch (1 - button),
 *      2 (B) back to 2, 1 (A) on button 0 (the magnifier) -> 0xA, the
 *      card's info; on button 1 (the gear) the card is taken;
 *   0xA the info: 1 or 2 back.
 * The button marked is sub_00047C60() (the widget [[0x656F88]+0x5C]).
 * Wrapped: what the pointer asked for goes in as those keys, one per call
 * of the handler (the game's own moves, sounds and bounds). */
void sub_000920B0_pad(void);
void sub_00047C60(void);
static volatile ULONGLONG s_tl_at;               /* the list ran then */
static volatile LONG s_tl_steps;                 /* card steps: + on, - back */
static volatile LONG s_tl_want = -1;             /* 0/1 a button, then A; 2 A; 3 B */
static volatile LONG s_tl_hover = -1;            /* button under the pointer */

static uint32_t tl_button(void)
{
    uint32_t sp = g_esp, sa = g_eax, sc = g_ecx, sd = g_edx, b;
    PUSH32(g_esp, 0x000922D1u);
    sub_00047C60();
    b = g_eax & 0xFFu;
    g_esp = sp;
    g_eax = sa;
    g_ecx = sc;
    g_edx = sd;
    return b;
}

/* 2026-11-17 05:0x (the user: the Target List's buttons "que sea con la
 * derecha principal"): on A in step 2 the game shows the two round buttons
 * and marks button 0, the magnifier: sub_00047C30(0, 1) -> sub_00054E60 ->
 * sub_000ED100(button, animate) on the widget's row [[[0x656F88]+0x5C]
 * +0x6F8]. While step 2 runs the row is hidden: it is set on button 1 (the
 * gear, the card is taken) at once (animate 0), and the game's "button 0"
 * then is made 1 (sub_000ED100's wrapper): the row opens on the gear, with
 * no slide from the magnifier. */
static volatile uint32_t s_tl_fila;                  /* the row, while step 2 runs */
void sub_000ED100_pad(void);
static uint32_t tl_fila(void)
{
    uint32_t g = MEM32(0x00656F88u), w = g ? MEM32(g + 0x5Cu) : 0u;
    return w ? MEM32(w + 0x6F8u) : 0u;
}

void sub_000920B0(void)
{
    uint8_t keys = MEM8(0x006589E6u), step = MEM8(0x0065F92Eu), inj = 0;
    s_tl_at = GetTickCount64();
    if (step == 2u && s_mode == MODE_POINTER) {
        uint32_t row = tl_fila();
        if (row && MEM32(row + 0xACu) == 2u) {
            if (MEM32(row + 8u) != 1u) {
                uint32_t sp = g_esp, sa = g_eax, sc = g_ecx, sd = g_edx, sb = g_ebx, si = g_esi, di = g_edi, bp = g_ebp;
                PUSH32(g_esp, 0u);
                PUSH32(g_esp, 1u);
                PUSH32(g_esp, 0x00054E66u);
                g_ecx = row;
                sub_000ED100_pad();             /* ret 8 */
                g_esp = sp;
                g_eax = sa; g_ecx = sc; g_edx = sd; g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
            }
            s_tl_fila = row;
        }
    }
    if (keys == 0u) {
        LONG w = s_tl_want;
        if (step == 2u) {
            if (w == 2)
                inj = 1;
            else if (s_tl_steps)
                inj = s_tl_steps > 0 ? 0x10 : 0x20;
        } else if (step == 3u) {
            uint32_t b = tl_button();
            if (w == 0 || w == 1)
                inj = (uint32_t)w == b ? 1 : 0x10;
            else if (w == 3)
                inj = 2;
            else if (w == 2)
                inj = 1;
            else if (s_tl_hover >= 0 && (uint32_t)s_tl_hover != b)
                inj = 0x10;
        } else if (step == 0xAu && w >= 0)
            inj = 1;
        if (inj == 1 || inj == 2)
            InterlockedExchange(&s_tl_want, -1);
        else if (inj && step == 2u)
            InterlockedExchangeAdd(&s_tl_steps, inj == 0x10 ? -1 : 1);
        if (inj) {
            fprintf(stderr, "[RATON] target list: step %u key %02X\n", step, inj);
            fflush(stderr);
            MEM8(0x006589E6u) = inj;
        }
    }
    sub_000920B0_pad();
    s_tl_fila = 0;
    if (inj)
        MEM8(0x006589E6u) = keys;
}

/* Where things are (screenshots 2026-11-10, 1919x1079 = 640x480 game px):
 * the card shown x 260..380, y 53..227; the one before it x 150..245,
 * y 91..228 (the next one the same on the right); the magnifier x 272..315
 * and the gear x 324..367, y 238..280. */
static int tl_pointer(XBOX_INPUT_STATE *n, ULONGLONG t, int over, int ledge, float gx, float gy)
{
    static ULONGLONG seen;
    int hover = -1;
    uint8_t step;
    if (t - s_tl_at > 150u)
        return 0;
    step = MEM8(0x0065F92Eu);
    mouse_a_off(n, t);
    if (t - seen > 500u) {
        InterlockedExchange(&s_tl_steps, 0);
        InterlockedExchange(&s_tl_want, -1);
        fprintf(stderr, "[RATON] target list open (step %u)\n", step);
        fflush(stderr);
    }
    seen = t;
    if (over && gy >= 238.f && gy <= 280.f)
        hover = gx >= 272.f && gx <= 315.f ? 0 : gx >= 324.f && gx <= 367.f ? 1 : -1;
    InterlockedExchange(&s_tl_hover, step == 3u ? hover : -1);
    if (s_wheel) {
        LONG w = InterlockedExchange(&s_wheel, 0);
        if (step == 2u)
            InterlockedExchangeAdd(&s_tl_steps, -w);   /* wheel up = back */
    }
    if (over && ledge) {
        const char *what = "nothing";
        int mid = gx >= 260.f && gx <= 380.f && gy >= 53.f && gy <= 227.f;
        if (step == 2u) {
            if (mid) {
                what = "A";
                InterlockedExchange(&s_tl_want, 2);
            } else if (gx >= 395.f && gx <= 490.f && gy >= 91.f && gy <= 228.f) {
                what = "next card";
                InterlockedExchangeAdd(&s_tl_steps, 1);
            } else if (gx >= 150.f && gx <= 245.f && gy >= 91.f && gy <= 228.f) {
                what = "previous card";
                InterlockedExchangeAdd(&s_tl_steps, -1);
            }
        } else if (step == 3u) {
            if (hover >= 0) {
                what = hover ? "gear" : "magnifier";
                InterlockedExchange(&s_tl_want, hover);
            } else if (mid) {
                what = "card (gear)";
                InterlockedExchange(&s_tl_want, 1);
            }
        } else if (step == 0xAu) {
            what = "close info";
            InterlockedExchange(&s_tl_want, 2);
        }
        fprintf(stderr, "[RATON] target list click game %.0f,%.0f -> %s (step %u)\n", gx, gy, what, step);
        fflush(stderr);
    }
    if (over && s_redge && (step == 3u || step == 0xAu)) {
        InterlockedExchange(&s_tl_want, 3);
        fprintf(stderr, "[RATON] target list right click -> B (step %u)\n", step);
        fflush(stderr);
    }
    (void)n;
    return 1;
}

/* ---- the duel's command buttons (look, set, summon...) ----------------- */

/* Picking a card opens a row of round buttons. sub_00080D40 runs them
 * from the field's update: [0x65F8FC] is its step (2: waiting for keys),
 * [0x65F8FA] the commands offered (bit per command, 13), bits 2..5 of
 * [0x65F8F8] the command chosen. The row itself is the object
 * [[0x656F88]+0x4C]: +8 the button chosen, +0xAC how many, +0x84[k] the
 * command of button k, and button k is the object at +0xC[command], its
 * place in the frame at +0x10/+0x14 (floats, the button's middle: the
 * glove is sent 4,30 from there). A key moves to the next command and
 * calls sub_000ED100(button, 1) on the row, with the cursor tick.
 *
 * Wrapped: while it waits for keys, a pointer over another button makes
 * that button the chosen one the same way (command bits, sub_000ED100,
 * tick), then the game's own step runs as usual. */
void sub_00080D40_pad(void);
void sub_000ED100(void);

static volatile ULONGLONG s_cmd_at;              /* the buttons waited then */
static volatile int s_cmd_want = -1;
static volatile LONG s_cmd_seq, s_cmd_done;
static volatile int s_cmd_mudo;                  /* the next mark without the tick (the default) */

static uint32_t cmd_row(void)
{
    uint32_t g = MEM32(0x00656F88u);
    return g ? MEM32(g + 0x4C) : 0;
}

/* Button k's middle in the frame: 1 if there is one. */
static int cmd_button(uint32_t row, int k, float *x, float *y)
{
    uint32_t id, b;
    if (!row || k < 0 || k >= (int)MEM32(row + 0xAC) || k >= 13)
        return 0;
    id = MEM32(row + 0x84 + 4u * (uint32_t)k);
    if (id >= 13)
        return 0;
    b = MEM32(row + 0xC + 4u * id);
    if (!b)
        return 0;
    *x = fbits(MEM32(b + 0x10));
    *y = fbits(MEM32(b + 0x14));
    return on_screen(*x, *y);
}

/* A click on a button: A is handed to the handler from here, frame after
 * frame, until it takes it. The handler ignores A while the row is still
 * gliding (sub_000482C0 -> sub_000EC9D0), and every hover starts that
 * glide: a 60 ms pad press from the input side was lost there (12:26 log:
 * three clicks on "Llamar", none taken, each followed by B). */
static volatile LONG s_cmd_click;
static volatile ULONGLONG s_cmd_click_at;

volatile ULONGLONG g_ygo_botones_t;     /* auto.c: the buttons or the card's info were up then */

void sub_00080D40(void)
{
    int inject = 0, inject_cmd = 0;
    uint8_t pressed_was = 0;
    {
        /* 2026-11-19 07:3x (the user, capture: the card's info panel, the
         * field dimmed and AUTO bright over it, "se sigue viendo"): AUTO hides
         * while the round buttons or the info panel are up (step 1..0xA) */
        /* 14:2x (the user, capture: AUTO gone with a hand card's info bar
         * and its buttons up): only the full info panel (0xA) covers it */
        uint32_t st = MEM32(0x0065F8FCu) & 0xFFu;
        if (st == 0xAu)
            g_ygo_botones_t = GetTickCount64();
    }
    if ((MEM32(0x0065F8FCu) & 0xFFu) == 0xAu)
        s_info_at = GetTickCount64();           /* the card's info panel is up */
    if ((MEM32(0x0065F8FCu) & 0xFFu) != 2u)
        s_cmd_click = 0;
    {
        /* 2026-11-17 (the user: "que por default se ponga en el boton de la
         * derecha cuando vas a invocar una carta de monstruo o magica"): the
         * row opening on a card of your hand marks its rightmost button
         * (Llamar / Activar: the game marks the first, the card's info), the
         * same way as the pointer over a button, without the tick */
        static int abierta;
        if ((MEM32(0x0065F8FCu) & 0xFFu) != 2u)
            abierta = 0;
        else if (!abierta) {
            uint32_t r0 = cmd_row();
            abierta = 1;
            if (s_mode == MODE_POINTER && r0 && !s_auto && MEM32(CUR_PLAYER) == 0u && MEM32(CUR_ZONE) == 11u) {
                int cnt = (int)MEM32(r0 + 0xAC);
                if (cnt >= 2 && cnt <= 13) {
                    s_cmd_want = cnt - 1;
                    s_cmd_mudo = 1;
                    InterlockedIncrement(&s_cmd_seq);
                    fprintf(stderr, "[RATON] buttons open on a hand card: button %d of %d marked\n", cnt - 1, cnt);
                    fflush(stderr);
                }
            }
        }
    }
    if ((MEM32(0x0065F8FCu) & 0xFFu) == 2u) {
        uint32_t row = cmd_row();
        s_cmd_at = GetTickCount64();
        /* A click on a card in the hand opened these buttons: its command
         * is picked and confirmed here (field_pointer's s_auto). Commands:
         * 0 card info, 4 "Colocar" (set) and 5 "Llamar" (summon), from the
         * logs of 12:31-12:52; the rest (activate and the like) by elimination.
         * Main: 5, else the lowest other offered one, else 4, else 0. Set: 4,
         * or B (the buttons close) when the card cannot be set. */
        if (s_auto && s_mode == MODE_POINTER && row) {
            LONG mode = s_auto;
            s_auto = 0;
            if (s_cmd_at - s_auto_at <= AUTO_MS) {
                uint32_t mask = MEM16(0x0065F8FAu);
                int c = -1, k = 0, j;
                if (mode == AUTO_INFO)
                    c = mask & 1u ? 0 : -1;
                else if (mode == AUTO_SET)
                    c = (mask >> 4) & 1u ? 4 : -1;
                else if ((mask >> 5) & 1u)
                    c = 5;
                else {
                    for (j = 1; j < 13 && c < 0; j++)
                        if (j != 4 && (mask >> j) & 1u)
                            c = j;
                    if (c < 0)
                        c = (mask >> 4) & 1u ? 4 : 0;
                }
                if (c >= 0) {
                    for (j = 0; j < c; j++)
                        if ((mask >> j) & 1u)
                            k++;
                    s_cmd_want = k;
                    InterlockedIncrement(&s_cmd_seq);
                    s_cmd_click = 1;            /* A once it is chosen */
                } else
                    s_cmd_click = 2;            /* cannot be set: close */
                s_cmd_click_at = s_cmd_at;
                fprintf(stderr, "[RATON] hand card %s: offered %04X -> command %d (button %d)\n",
                        mode == AUTO_SET ? "set" : mode == AUTO_INFO ? "info" : "main action",
                        mask, c, c >= 0 ? k : -1);
                fflush(stderr);
            }
        }
        if (s_cmd_click && (s_cmd_click == 2 || s_cmd_seq == s_cmd_done)
                && s_mode == MODE_POINTER) {
            if (s_cmd_at - s_cmd_click_at > 1500u)
                s_cmd_click = 0;
            else if ((MEM8(0x006589E6u)) == 0) {
                pressed_was = MEM8(0x006589E6u);
                inject_cmd = (int)((MEM16(0x0065F8F8u) >> 2) & 0xFu);
                /* A (1) or B (2), just for this step */
                MEM8(0x006589E6u) = (uint8_t)(s_cmd_click == 2 ? 2 : 1);
                inject = 1;
            }
        }
        if (s_mode == MODE_POINTER && s_cmd_seq != s_cmd_done && row) {
            LONG seq = s_cmd_seq;
            int k = s_cmd_want;
            /* The command of button k: the k-th command offered (set bit of
             * [0x65F8FA], from 0 up), as the game's own d-pad steps walk them
             * (one bit and one button at a time). The row's +0x84 is the
             * picture's number, not the command: until 12:31 it was written as
             * the command, and the middle button ("Colocar") was never chosen
             * because that number's bit was not offered. */
            uint32_t id = 99u, mask = MEM16(0x0065F8FAu);
            {
                int c, seen = 0;
                for (c = 0; c < 13; c++)
                    if ((mask >> c) & 1u) {
                        if (seen == k) {
                            id = (uint32_t)c;
                            break;
                        }
                        seen++;
                    }
            }
            /* pending until done; a frame with a key down only delays it */
            if (k < 0 || k >= (int)MEM32(row + 0xAC) || k == (int)MEM32(row + 8) || id >= 13u) {
                s_cmd_done = seq;
                s_cmd_mudo = 0;
            }
            else if ((MEM16(0x006589E6u) & 0xFFu) == 0) {
                s_cmd_done = seq;
                {
                    uint32_t sa = g_eax, sc = g_ecx, sd = g_edx;
                    uint32_t sb = g_ebx, si = g_esi, di = g_edi;
                    MEM16(0x0065F8F8u) = (uint16_t)((MEM16(0x0065F8F8u) & ~0x3Cu) | (id << 2));
                    PUSH32(g_esp, 1u);
                    PUSH32(g_esp, (uint32_t)k);
                    PUSH32(g_esp, 0x000EDB73u);
                    g_ecx = row;
                    sub_000ED100();                  /* ret 8 */
                    if (!s_cmd_mudo) {
                        PUSH32(g_esp, 0x10000001u);
                        PUSH32(g_esp, 0x00080ED3u);
                        sub_0007E280();
                        g_esp += 4;
                    }
                    s_cmd_mudo = 0;
                    g_eax = sa; g_ecx = sc; g_edx = sd;
                    g_ebx = sb; g_esi = si; g_edi = di;
                }
            }
        }
    }
    sub_00080D40_pad();
    if (inject) {
        MEM8(0x006589E6u) = pressed_was;        /* nobody else sees it */
        if ((MEM32(0x0065F8FCu) & 0xFFu) != 2u) {
            fprintf(stderr, "[RATON] buttons: %s taken\n", s_cmd_click == 2 ? "B" : "A");
            /* a play (anything but the card's info) runs fast */
            if (s_cmd_click == 1 && inject_cmd != 0)
                boost_start("play confirmed");
            s_cmd_click = 0;                    /* taken */
            fflush(stderr);
        }
    }
}

/* 2026-11-17 04:3x (the user: "aparece un segundo aun en el boton izquierdo
 * ... que aparezca en el derecho de una vez"): marked once the row waited
 * for keys, the light slid over from the first button (sub_000ED100's 10
 * frames). The row's opening, sub_00048010 (from the buttons' step 1,
 * 0x80E23, after the reset sub_000ED520 put [row+8] = 0 and the buttons
 * were added), lights [row+8] (sub_000ECA80 reads it) and sends the glove
 * there (sub_000ECFB0). For a card of your hand, [row+8] and the chosen
 * command (bits 2..5 of [0x65F8F8], the game's own pair) are the rightmost
 * button's before it runs: the row opens on it. */
void sub_00048010_pad(void);
void sub_000ED100_pad(void);
void sub_00048010(void)
{
    if (MEM32(g_esp) == 0x00080E2Au && s_mode == MODE_POINTER && !s_auto
            && MEM32(CUR_PLAYER) == 0u && MEM32(CUR_ZONE) == 11u) {
        uint32_t row = cmd_row(), mask = MEM16(0x0065F8FAu), id = 99u;
        int cnt = row ? (int)MEM32(row + 0xAC) : 0, c, seen = 0;
        for (c = 0; c < 13; c++)
            if ((mask >> c) & 1u) {
                if (seen == cnt - 1)
                    id = (uint32_t)c;
                seen++;
            }
        if (cnt >= 2 && cnt <= 13 && seen == cnt && id < 13u) {
            /* 04:4x (the user's capture: the magnifier still lit beside the
             * right one): adding the first button lit it; [row+8] alone left
             * it lit. The game's own change of button, sub_000ED100 (thiscall:
             * button, animate), puts the old one out and lights the new;
             * with animate 0 it is at once (0 frames), before the row opens
             * (no glide yet, so its busy test lets it through) */
            uint32_t sp = g_esp, sa = g_eax, sc = g_ecx, sd = g_edx, sb = g_ebx, si = g_esi, di = g_edi, bp = g_ebp;
            PUSH32(g_esp, 0u);
            PUSH32(g_esp, (uint32_t)(cnt - 1));
            PUSH32(g_esp, 0x000EDB73u);
            g_ecx = row;
            sub_000ED100_pad();                 /* ret 8 */
            g_esp = sp;
            g_eax = sa; g_ecx = sc; g_edx = sd; g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
            if ((int)MEM32(row + 8) != cnt - 1)    /* refused: the old way */
                MEM32(row + 8) = (uint32_t)(cnt - 1);
            MEM16(0x0065F8F8u) = (uint16_t)((MEM16(0x0065F8F8u) & ~0x3Cu) | (id << 2));
            fprintf(stderr, "[RATON] buttons open on a hand card: button %d of %d (command %u) lit from the start\n",
                    cnt - 1, cnt, id);
            fflush(stderr);
        }
    }
    sub_00048010_pad();
}

/* The buttons' part of pointer mode: 1 while they wait for keys. */
static int cmd_pointer(XBOX_INPUT_STATE *n, ULONGLONG t, int over, int ledge, float gx, float gy)
{
    static int last_k = -1;
    static float lx, ly;
    uint32_t row;
    if (t - s_cmd_at > 150u)
        return 0;
    /* 2026-11-17 (mouse review): the kit's A of the left button stays out;
     * a click off the buttons (meant to close them, B below) went in as A
     * too and ran the marked command. The A that reaches the game is the
     * one the wrapper hands over (s_cmd_click). */
    mouse_a_off(n, t);
    row = cmd_row();
    if (over && row) {
        int k, best = -1, cnt = (int)MEM32(row + 0xAC);
        float bd = 1e9f;
        for (k = 0; k < cnt && k < 13; k++) {
            float x, y, d;
            if (!cmd_button(row, k, &x, &y))
                continue;
            if (fabsf(gx - x) > 34.f || fabsf(gy - y) > 34.f)
                continue;
            d = (gx - x) * (gx - x) + (gy - y) * (gy - y);
            if (d < bd) {
                bd = d;
                best = k;
            }
        }
        /* sent on a new button, and again when the pointer moves on one */
        if (best >= 0 && (best != last_k || fabsf(gx - lx) > 3.f || fabsf(gy - ly) > 3.f)) {
            last_k = best;
            lx = gx;
            ly = gy;
            s_cmd_want = best;
            InterlockedIncrement(&s_cmd_seq);
        } else if (best < 0)
            last_k = -1;
        {
            /* the press seen once for every screen (ledge): this screen's own
             * "was down" was stale from its last time, so the click that
             * opened the buttons, still held, could count as a new one */
            if (ledge) {
                char where[160];
                int len = 0;
                for (k = 0; k < cnt && k < 6; k++) {
                    float x = 0, y = 0;
                    cmd_button(row, k, &x, &y);
                    len += snprintf(where + len, sizeof where - (size_t)len, " %.0f,%.0f", x, y);
                }
                /* A from the game's side on a button; off every button, B
                 * (back to the duel, like the right button) */
                s_cmd_click_at = GetTickCount64();
                s_cmd_click = best >= 0 ? 1 : 2;
                fprintf(stderr, "[RATON] buttons click game %.0f,%.0f -> button %d of %u,"
                        " chosen %u (command %u, offered %04X); buttons at%s\n", gx, gy, best,
                        MEM32(row + 0xAC), MEM32(row + 8), (MEM16(0x0065F8F8u) >> 2) & 0xFu,
                        MEM16(0x0065F8FAu), where);
                fflush(stderr);
            }
        }
        if (GetAsyncKeyState(VK_RBUTTON) & 0x8000)
            n->Gamepad.bAnalogButtons[XBOX_BUTTON_B] = 255;
    }
    return 1;
}

/* ---- the main menu's wheel of modes ------------------------------------- */

/* The main menu (Duelo individual, triple, ...) has no glove: its modes are
 * 3D icons on a half circle and the sun marks the chosen one. The wheel's
 * object (thiscall update sub_0010D6C0): +0x6C state (0 = waiting for keys),
 * +0x9C step animation (0 = still), +0xA0 chosen icon, +0xA4 where the step
 * goes, +0xAC how many icons. Icon k rests at the angle
 * 180 - k*180/(n-1) degrees, the point (8cos, 8sin, 0), which
 * sub_0010CA40 (thiscall on [wheel], args: point in, x,y out; ret 8) turns
 * into the 640x480 frame: the sun is placed that way (sub_0010D3B0).
 * sub_0010D460 / sub_0010D590 start a step to the next / previous icon
 * (tick sound, help text); the step's state 2 then makes +0xA4 the chosen
 * one. So a step started toward the pointer's icon with +0xA4 set to it
 * lands there directly, with the game's own animation and sound.
 *
 * Wrapped: the icons' places are measured there (the game's thread), and a
 * pending pick is started while the wheel is still. */
void sub_0010D6C0_pad(void);
/* perfil.c: the Perfil button in the wheel's middle and its panel */
void ygo_perfil_rueda(float x, float y);
void ygo_perfil_fuera(void);
void ygo_perfil_rendija(float f);
int ygo_perfil_raton(int over, float gx, float gy, int clic, int derecho);
int ygo_buscador_raton(int over, float gx, float gy, int clic, LONG rueda);
void sub_0010CA40(void);
void sub_0010D460(void);
void sub_0010D590(void);

#define WHEEL_MAX 12
static volatile uint32_t s_wh_obj;
static volatile ULONGLONG s_wh_at;               /* it waited for keys then */
static volatile int s_wh_n;                      /* icons measured */
static volatile float s_wh_x[WHEEL_MAX], s_wh_y[WHEEL_MAX];
static volatile int s_wh_want = -1;
static volatile LONG s_wh_seq, s_wh_done;

static uint32_t ubits(float f)
{
    uint32_t b;
    memcpy(&b, &f, 4);
    return b;
}

/* Icon k's place in the frame, by the game's own projection. */
static void wheel_place(uint32_t w, int k, int n, float *x, float *y)
{
    uint32_t base;
    float ang = (float)((180.0 - (double)k / (double)(n - 1) * 180.0) * 0.01745329238474369);
    g_esp -= 0x20;
    base = g_esp;
    MEM32(base) = ubits((float)cos(ang) * 8.f);
    MEM32(base + 4) = ubits((float)sin(ang) * 8.f);
    MEM32(base + 8) = 0;
    MEM32(base + 0x10) = MEM32(base + 0x14) = 0;
    PUSH32(g_esp, base + 0x10);         /* out */
    PUSH32(g_esp, base);                /* in */
    PUSH32(g_esp, 0x0010D418u);
    g_ecx = MEM32(w);
    sub_0010CA40();                     /* ret 8: back at base */
    *x = fbits(MEM32(base + 0x10));
    *y = fbits(MEM32(base + 0x14));
    g_esp = base + 0x20;
}

static float s_ult_x, s_ult_y;                  /* the button's last place (frame px) */
/* 2026-11-20 (the user: "usar el Boton de duelo vinculado para el de salir a
 * windows asi nos queda todo simetrico"): the wheel's last icon (Duelo
 * vinculado: there is no network in the port) is Salir a Windows. Its text
 * (24 in each language's table) says so, its A closes the game, and
 * perfil.c draws the exit medallion over it (unlit, or lit with its sun
 * while the wheel has it chosen) at the place handed here each frame */
volatile float g_ygo_sal_x, g_ygo_sal_y;     /* game px */
volatile int g_ygo_sal_sel;
volatile int g_ygo_sel_k = -1;               /* the wheel's chosen icon and its place (game px) */
volatile float g_ygo_sel_x, g_ygo_sel_y;
volatile ULONGLONG g_ygo_sal_at;
uint32_t xbox_HeapAlloc(uint32_t size, uint32_t alignment);
#define TEXTO_SALIR 24u
/* 2026-11-20 (the user: "un cartel cuando se apriete el boton de Salir que
 * diga Salir a Windows? y tenga si y no todo con tematica y fuente de letra
 * del juego"): the game's own box, the one B opens on the wheel (state 0,
 * 0x10D8C5: message text 27, buttons texts 9 and 10; state 5 at 0x10D9F1:
 * the cursor [box +0xBD8], 0 the left one; A there on it = to the title).
 * For the exit its texts point, while it opens, to "¿Salir a Windows?" and
 * the game's own Sí / No (texts 7, 8); then A on Sí closes the game, No or B
 * close the box as always. */
static int s_caja;                      /* 1: B sent for our box; 2: our box is up */
static ULONGLONG s_caja_at;
static uint32_t s_caja_w;
static uint32_t s_caja_viejo[6][3];
static void caja_textos(int poner)
{
    static const char *const k_preg[6] = { "Exit to Windows?", "Exit to Windows?", "Zu Windows beenden?",
                                          "Quitter vers Windows ?", "Uscire a Windows?", "\xBFSalir a Windows?" };
    static uint32_t g_preg[6];
    static const uint32_t k_id[3] = { 27u, 9u, 10u };
    int l, i;
    for (l = 0; l < 6; l++) {
        uint32_t tab = MEM32(0x005FC2DCu + 4u * (uint32_t)l), base = MEM32(0x005FC2C4u + 4u * (uint32_t)l);
        if (!tab || MEM32(tab) <= 27u)
            continue;
        if (poner) {
            for (i = 0; i < 3; i++)
                s_caja_viejo[l][i] = MEM32(tab + 4u + 4u * k_id[i]);
            if (!g_preg[l] && (g_preg[l] = xbox_HeapAlloc((uint32_t)strlen(k_preg[l]) + 1u, 4)) != 0)
                memcpy((void *)XBOX_PTR(g_preg[l]), k_preg[l], strlen(k_preg[l]) + 1u);
            if (g_preg[l])
                MEM32(tab + 4u + 4u * 27u) = g_preg[l] - base;
            MEM32(tab + 4u + 4u * 9u) = MEM32(tab + 4u + 4u * 7u);     /* Sí */
            MEM32(tab + 4u + 4u * 10u) = MEM32(tab + 4u + 4u * 8u);    /* No */
        } else if (s_caja_viejo[l][0]) {
            for (i = 0; i < 3; i++)
                MEM32(tab + 4u + 4u * k_id[i]) = s_caja_viejo[l][i];
        }
    }
}

/* 2026-11-20 (the user, with the box up: "revisa que ande bien el click y
 * lea bien ese cartel de si y no"): the wheel's box (state 5, [w + 0x64])
 * is the list widget of the duel's boxes (sub_000F0040): +0xBC4 lines, line
 * k the object at +0x1B0 + 4k placed at its +0xC / +0x10 (game px), the
 * marked line [+0xBD8] (sub_000EF040), set by sub_000EEFE0(k) with the tick
 * 0x10000001 (0x10DA1E / 0x10DA57, the game's up / down). The pointer never
 * saw it: a click anywhere was A on the marked line. Now the line under the
 * pointer is marked the game's way and a click answers that one. */
void sub_0011A400(void);
void sub_000EEFE0(void);
static void mouse_a_off(XBOX_INPUT_STATE *n, ULONGLONG t);
static volatile ULONGLONG s_cb_at, s_cb_click_at, s_cb_a_until;
static volatile int s_cb_n;
static volatile float s_cb_x, s_cb_y[3];
static volatile LONG s_cb_want = -1, s_cb_click = -1;

static void caja_raton(uint32_t w)
{
    uint32_t box = MEM32(w + 0x64u);
    ULONGLONG t = GetTickCount64();
    int k, n, got = 0, cur;
    if (!box || s_mode != MODE_POINTER)
        return;
    n = (int)MEM32(box + 0xBC4u);
    if (n < 1 || n > 3)
        return;
    for (k = 0; k < n; k++) {
        uint32_t o = MEM32(box + 0x1B0u + 4u * (uint32_t)k);
        float x, y;
        if (!o)
            break;
        x = fbits(MEM32(o + 0xCu));
        y = fbits(MEM32(o + 0x10u));
        if (!on_screen(x, y))
            break;
        if (k == 0)
            s_cb_x = x;
        s_cb_y[k] = y;
        got++;
    }
    if (t - s_cb_at > 500u) {
        s_cb_want = s_cb_click = -1;            /* a new box */
        fprintf(stderr, "[RATON] wheel box: %d lines (%d placed), x %.0f, y %.0f %.0f, marked %u\n", n, got,
                s_cb_x, s_cb_y[0], got > 1 ? s_cb_y[1] : -1.f, MEM32(box + 0xBD8u));
        fflush(stderr);
    }
    s_cb_n = got == n ? n : 0;
    s_cb_at = t;
    cur = (int)MEM32(box + 0xBD8u);
    k = s_cb_want;
    if (k >= 0 && k < n && k != cur) {
        uint32_t sa = g_eax, sc = g_ecx, sd = g_edx, sb = g_ebx, si = g_esi, di = g_edi, bp = g_ebp, sp = g_esp;
        PUSH32(g_esp, 0x10000001u);
        PUSH32(g_esp, 0x0010DA28u);
        sub_0011A400();
        g_esp = sp;
        g_ecx = box;
        PUSH32(g_esp, (uint32_t)k);
        PUSH32(g_esp, 0x0010DA35u);
        sub_000EEFE0();
        g_esp = sp;
        g_eax = sa; g_ecx = sc; g_edx = sd; g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
        cur = (int)MEM32(box + 0xBD8u);
    }
    if (s_cb_click >= 0) {
        if (cur == s_cb_click) {
            fprintf(stderr, "[RATON] wheel box: line %ld answered\n", (long)s_cb_click);
            fflush(stderr);
            s_cb_a_until = t + PRESS_MS + 15u;
            s_cb_click = -1;
        } else if (t - s_cb_click_at > 1500u)
            s_cb_click = -1;
    }
}

static int caja_pointer(XBOX_INPUT_STATE *n, ULONGLONG t, int over, int ledge, float gx, float gy)
{
    static float lx = -1.f, ly = -1.f;
    int k, best = -1, lines = s_cb_n;
    if (t - s_cb_at > 150u)
        return 0;
    mouse_a_off(n, t);                  /* the kit's A: a click off the lines answers nothing */
    /* the bar is 450 wide; +0xC taken as its left end, with room for it
     * being the centre (the log line above tells) */
    if (over && lines > 0 && gx >= s_cb_x - 230.f && gx <= s_cb_x + 460.f)
        for (k = 0; k < lines; k++) {
            float y0 = s_cb_y[k] - (k == 0 ? 8.f : 0.f);
            float y1 = s_cb_y[k] + 30.f + (k == lines - 1 ? 8.f : 0.f);
            if (gy >= y0 && gy < y1)
                best = k;
        }
    if (best >= 0 && (gx != lx || gy != ly))
        InterlockedExchange(&s_cb_want, best);
    lx = gx;
    ly = gy;
    if (ledge && over) {
        fprintf(stderr, "[RATON] wheel box click game %.0f,%.0f -> line %d (x %.0f, y %.0f %.0f)\n", gx, gy, best,
                s_cb_x, s_cb_y[0], lines > 1 ? s_cb_y[1] : -1.f);
        fflush(stderr);
        if (best >= 0) {
            InterlockedExchange(&s_cb_want, best);
            s_cb_click_at = t;
            InterlockedExchange(&s_cb_click, best);
        }
    }
    if (over && (GetAsyncKeyState(VK_RBUTTON) & 0x8000))
        n->Gamepad.bAnalogButtons[XBOX_BUTTON_B] = 255;
    if (t < s_cb_a_until)
        n->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = 255;
    InterlockedExchange(&s_wheel, 0);
    n->Gamepad.sThumbLX = n->Gamepad.sThumbLY = 0;
    n->Gamepad.sThumbRX = n->Gamepad.sThumbRY = 0;
    return 1;
}

static void salir_textos(void)
{
    /* the tables (cartas_mod.py texto_juego): 1 English, 2 German, 3 French,
     * 4 Italian, 5 Spanish (0: English too) */
    static const char *const k_txt[6] = { "Exit to Windows", "Exit to Windows", "Zu Windows beenden",
                                         "Quitter vers Windows", "Esci a Windows", "Salir a Windows" };
    static int hecho;
    int l;
    if (hecho)
        return;
    hecho = 1;
    for (l = 0; l < 6; l++) {
        uint32_t tab = MEM32(0x005FC2DCu + 4u * (uint32_t)l), base = MEM32(0x005FC2C4u + 4u * (uint32_t)l), g;
        size_t k = strlen(k_txt[l]) + 1;
        if (!tab || MEM32(tab) <= TEXTO_SALIR || !(g = xbox_HeapAlloc((uint32_t)k, 4)))
            continue;
        memcpy((void *)XBOX_PTR(g), k_txt[l], k);
        MEM32(tab + 4u + 4u * TEXTO_SALIR) = g - base;
    }
    fprintf(stderr, "[RATON] el ícono Duelo vinculado ahora es Salir a Windows\n");
    fflush(stderr);
}
/* 2026-11-20 (the user's capture: "Perfil" in the banner looked pasted on,
 * "ponelo como los demas"): the game writes the banner itself. On a new
 * icon (0x10D50F) it calls sub_0010C6B0 on the banner [wheel +0x54] with the
 * icon's text (ids 0x10..0x13, 0x15..0x18 by the icon's kind [wheel +0x74 +
 * 4 sel]), and that builds the banner's letters from the text at once. While
 * perfil.c says the pointer is on the Perfil button, the same call is made
 * with the icon's id pointing (for that call only) to "Perfil" in the six
 * tables; when the pointer leaves, the icon's own text again. Game thread
 * (this wrapper), state 0 only. */
void sub_0010C6B0(void);
extern volatile ULONGLONG g_ygo_cartel_perfil_hasta;   /* perfil.c */
volatile ULONGLONG g_ygo_cartel_perfil_ok;             /* the banner says Perfil (for perfil.c) */
/* 22:0x (the 21:59 sonda, the user: "dice otra cosa"): the banner holds two
 * texts, both class 0x427A20: +8 is the one that stays (its letters come in
 * one by one) and +C the one going out (sub_0010C6B0 puts the old text there
 * and sub_001034A0 fades it to nothing, then turns its letters off). For a
 * new icon the game puts the old text in +C (sub_0010C6B0) and the new one in
 * +8 (sub_0010CDE0's other path: [+8] vt+4 = sub_0002B490(text)). "Perfil"
 * had gone in +C, the fading one, so the old icon's name stayed. */
static int s_cp_activo, s_cp_nuestra;  /* Perfil is up / our own call runs */
static void cartel_llamar(uint32_t w, uint32_t id)
{
    uint32_t sa = g_eax, sc = g_ecx, sd = g_edx, sb = g_ebx, si = g_esi, di = g_edi, bp = g_ebp, sp = g_esp;
    g_ecx = MEM32(w + 0x54u);
    PUSH32(g_esp, id);
    PUSH32(g_esp, 0x0010D566u);
    s_cp_nuestra = 1;
    sub_0010C6B0();
    s_cp_nuestra = 0;
    g_esp = sp;
    g_eax = sa; g_ecx = sc; g_edx = sd; g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
}
void sub_0002B490(void);
static void cartel_vista(uint32_t w, uint32_t id)
{
    uint32_t sa = g_eax, sc = g_ecx, sd = g_edx, sb = g_ebx, si = g_esi, di = g_edi, bp = g_ebp, sp = g_esp;
    uint32_t b = MEM32(w + 0x54u), o = b ? MEM32(b + 8u) : 0u;
    if (!o)
        return;
    g_ecx = o;
    PUSH32(g_esp, id);
    PUSH32(g_esp, 0x0010CE53u);         /* as from sub_0010CDE0 */
    sub_0002B490();
    g_esp = sp;
    g_eax = sa; g_ecx = sc; g_edx = sd; g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
}
/* 2026-11-20 21:0x (the user: "el cartel no funciona el de perfil"; the 20:56
 * log has "el cartel dice Perfil" but the banner kept the icon's name): the
 * banner reads the text after the call (its letters are made while it fades
 * in), and the text was put back right after the call. Now "Perfil" stays in
 * that text while the pointer is on the button and goes back when it leaves,
 * or when the wheel leaves state 0 (cartel_perfil_restaurar). */
static uint32_t s_cp_id = 0xFFFFFFFFu, s_cp_viejo[6];
static void cartel_perfil_restaurar(void)
{
    int l;
    if (s_cp_id == 0xFFFFFFFFu)
        return;
    for (l = 0; l < 6; l++) {
        uint32_t tab = MEM32(0x005FC2DCu + 4u * (uint32_t)l);
        if (s_cp_viejo[l] && tab)
            MEM32(tab + 4u + 4u * s_cp_id) = s_cp_viejo[l];
        s_cp_viejo[l] = 0;
    }
    s_cp_id = 0xFFFFFFFFu;
}
static void cartel_perfil_poner(uint32_t id)
{
    static const char *const k_txt[6] = { "Profile", "Profile", "Profil", "Profil", "Profilo", "Perfil" };
    static uint32_t g_txt[6];
    int l;
    cartel_perfil_restaurar();
    for (l = 0; l < 6; l++) {
        uint32_t tab = MEM32(0x005FC2DCu + 4u * (uint32_t)l), base = MEM32(0x005FC2C4u + 4u * (uint32_t)l);
        if (!tab || MEM32(tab) <= id)
            continue;
        if (!g_txt[l] && (g_txt[l] = xbox_HeapAlloc((uint32_t)strlen(k_txt[l]) + 1u, 4)) != 0)
            memcpy((void *)XBOX_PTR(g_txt[l]), k_txt[l], strlen(k_txt[l]) + 1u);
        if (!g_txt[l])
            continue;
        s_cp_viejo[l] = MEM32(tab + 4u + 4u * id);
        MEM32(tab + 4u + 4u * id) = g_txt[l] - base;
        s_cp_id = id;                   /* only if some table has that text */
    }
}
/* 2026-11-20 21:1x (the user: "perfil sigue sin salir ... solo se ve la
 * animacion como que quiere iniciar"; 21:08 log: our call is made each time):
 * the game writes the banner again on its own (sub_0010CDE0, called from the
 * wheel's frame and from 0xF9A20 / 0xFBB40) with the icon's text, right over
 * ours. The banner's function itself is wrapped (tools/rename_pads.py): while
 * "Perfil" is up, every call to it asks for text 20 (ours). */
void sub_0010C6B0_pad(void);
void sub_0010C6B0(void)
{
    /* 21:4x (the user: "perfil no se ve la letra nada de nada"; before:
     * "solo se ve la animacion como que quiere iniciar"): each call starts
     * the banner's fade-in again (+0x20 = 1, every letter restarted), and the
     * game kept calling with its icon's text while Perfil was up, so the
     * letters never got to show. Its calls are now skipped (ret 4) while
     * Perfil is up; only ours (cartel_llamar, text 20) go through. */
    if (s_cp_activo && !s_cp_nuestra && GetTickCount64() < g_ygo_cartel_perfil_hasta) {
        static ULONGLONG dicho;
        static unsigned saltadas;
        saltadas++;
        if (GetTickCount64() - dicho > 2000u) {
            dicho = GetTickCount64();
            fprintf(stderr, "[RATON] el juego quiso volver a poner el texto %u en el cartel (%u veces): no se lo deja, queda Perfil\n",
                    MEM32(g_esp + 4u), saltadas);
            saltadas = 0;
        }
        g_esp += 8u;                    /* ret 4 */
        return;
    }
    sub_0010C6B0_pad();
}

static void cartel_perfil(uint32_t w)
{
    static const uint32_t k_ids[8] = { 0x10u, 0x11u, 0x12u, 0x13u, 0x15u, 0x16u, 0x17u, 0x18u };
    static int puesto = 0, puesto_sel = -1;
    ULONGLONG t = GetTickCount64();
    int encima = t < g_ygo_cartel_perfil_hasta;
    uint32_t sel = MEM32(w + 0xA0u), kind, id;
    if (!MEM32(w + 0x54u) || sel >= 16u || (kind = MEM32(w + 0x74u + 4u * sel)) >= 8u)
        return;
    id = k_ids[kind];
    /* 21:1x (the user: the letters are drawn again but the other button's name
     * stays): the banner skips the text when the id is the one it shows, so
     * "Perfil" goes in text 20 (0x14, no icon's) and that id is asked */
    /* 22:0x: as the game changes icons: the old text to +C (fades out), the
     * new one to +8 (stays). Text 20 holds "Perfil" only during the call (the
     * text object keeps its own copy of the letters, sub_0002B560). */
    if (encima && !puesto) {
        cartel_perfil_poner(0x14u);
        if (s_cp_id != 0x14u) {         /* no table has a text 20: the fallback plate (perfil.c) */
            static int dicho;
            if (!dicho++) {
                fprintf(stderr, "[RATON] el cartel no tiene texto 20: queda el cartel de respaldo\n");
                fflush(stderr);
            }
            puesto = 0;
            return;
        }
        cartel_perfil_restaurar();
        cartel_llamar(w, id);           /* the icon's name goes out */
        cartel_perfil_poner(0x14u);
        cartel_vista(w, 0x14u);         /* "Perfil" comes in and stays */
        cartel_perfil_restaurar();
        s_cp_activo = 1;
        fprintf(stderr, "[RATON] el cartel dice Perfil (letra del juego, en el texto que queda; icono %u, su texto %u)\n", sel, id);
        fflush(stderr);
        puesto = 1;
        puesto_sel = (int)sel;
    } else if (encima && puesto && puesto_sel != (int)sel) {
        /* the wheel turned under the pointer: Perfil stays (no fade again) */
        cartel_perfil_poner(0x14u);
        cartel_vista(w, 0x14u);
        cartel_perfil_restaurar();
        puesto_sel = (int)sel;
    } else if (!encima && puesto) {
        cartel_perfil_poner(0x14u);
        cartel_llamar(w, 0x14u);        /* "Perfil" goes out */
        cartel_perfil_restaurar();
        cartel_vista(w, id);            /* the icon's name comes back */
        s_cp_activo = 0;
        fprintf(stderr, "[RATON] el cartel vuelve a su texto (icono %u)\n", sel);
        fflush(stderr);
        puesto = 0;
        puesto_sel = -1;
    }
    if (puesto)
        g_ygo_cartel_perfil_ok = t;
}

void sub_0010D6C0(void)
{
    uint32_t w = g_ecx;
    /* perfil.c's button (2026-11-16, the user: "tarda un poco en aparecer"):
     * the wheel's states (the jump table at 0x10DF00): 0 waits for keys, 1
     * the menu coming in (0x10D969: waits for the fade, then 0), 5 the
     * "¿Seguro?" box after B (0x10D9F1), 6 that box going, 2/3/4/7 a mode
     * chosen or another page. The button shows in 0 and 1 only. */
    if (w) {
        static uint32_t antes = 0xFFFFFFFFu;
        /* 2026-11-17 (the user: the button "desaparece bastante antes de que
         * la puerta se cierra"; log: wheel 0 -> 2 and doors 0 -> 1 in the same
         * frame, the button hid then, the doors shut ~0.67 s later): on a mode
         * chosen the button stays at its place while the doors close (their
         * state 1) and goes when they are shut (2). The "¿Seguro?" box (5, 6)
         * still hides it at once. */
        static int cerrando;
        static ULONGLONG cerrando_at;
        uint32_t st = MEM32(w + 0x6C);
        int n = (int)MEM32(w + 0xAC);
        {
            extern void ygo_guardado_menu(uint32_t st);
            ygo_guardado_menu(st);      /* guardado.c: a won duel saved here */
        }
        if (st != antes) {
            fprintf(stderr, "[RATON] wheel state %u -> %u %llu ms\n", antes, st,
                    (unsigned long long)GetTickCount64());
            fflush(stderr);
            if (st != 0u && st != 1u) {
                if (st != 5u && st != 6u && antes == 0u) {
                    cerrando = 1;
                    cerrando_at = GetTickCount64();
                } else {
                    cerrando = 0;
                    ygo_perfil_fuera();
                }
            } else
                cerrando = 0;
            antes = st;
        }
        /* 2026-11-16 (video: the button showed on the closed doors, before
         * the other icons): the doors are the transition at [0x66DCD8]
         * (sub_000F49D0): +0x10 = 1 closing (0.67 s), 2 closed, 3 opening
         * (1.33 s, then 0). The doors stay shut ~0.33 s of the 3, then slide
         * ~0.27 s (video, 15 fps); the wheel's 1 lasts until the 3 ends.
         * So in 1 the button waits until 330 ms into the 3. */
        uint32_t tr = MEM32(0x66DCD8), ts = tr ? MEM32(tr + 0x10) : 0u;
        static uint32_t ts_antes = 0xFFFFFFFFu;
        static ULONGLONG cierra_at, cuadro_antes;
        static unsigned abre_ms;
        ULONGLONG ahora = GetTickCount64();
        /* 2026-11-17 (the user's capture: the button on the shut doors when
         * back from a menu): the 3's first frame loads the wheel, ~350 ms
         * (log: HITCH 340-351 ms right after "doors 2 -> 3"), and the doors
         * still stay shut ~0.33 s after it. So the 330 ms of the 3 count
         * frames, each at most 34 ms, not the clock. */
        if (ts == 3u && ts_antes == 3u) {
            ULONGLONG dt = ahora - cuadro_antes;
            abre_ms += dt > 34u ? 34u : (unsigned)dt;
        }
        cuadro_antes = ahora;
        if (ts != ts_antes) {
            fprintf(stderr, "[RATON] doors state %u -> %u (wheel %u)\n", ts_antes, ts, st);
            fflush(stderr);
            ts_antes = ts;
            if (ts == 3u)
                abre_ms = 0;
            if (ts == 1u)
                cierra_at = ahora;
        }
        if (cerrando) {
            /* 2026-11-17 (the user's capture: the button on the shut doors):
             * sub_000F49D0 plays the slam (0x1000000A) at 500000 us of the 1,
             * when the doors meet; the 2 comes at 666667. It goes at 500 ms. */
            /* 18:2x (the user: "que quede atras de la puerta"): the button
             * is cut to the doors' gap. Video 15-06-50 at 60/s (the slam
             * sound 500 ms into the 1 gives its start): half the gap over
             * half the window, every ~16.7 ms from 30 ms; shut at 280 ms. */
            static const float rend[] = { 1.f, 1.f, .96f, .83f, .69f, .56f, .44f, .32f, .19f, .115f,
                                          .0875f, .067f, .046f, .033f, .021f, .0125f, .006f, 0.f };
            /* 18:3x (the user's video 18-09-26 at 60/s: the button narrowed
             * well inside the gap, ~70-95 ms ahead of the doors as shown; the
             * game's frame reaches the screen later than our window): 80 ms later */
            float e = ts == 1u ? (float)(ahora - cierra_at) - 80.f : 0.f, f = 1.f;
            if (e > 30.f) {
                float q = (e - 30.f) / (250.f / 15.f) + 2.f;
                int i = (int)q;
                f = i >= 17 ? 0.f : rend[i] + (rend[i + 1] - rend[i]) * (q - i);
            } else if (e > 20.f)
                f = 1.f - (e - 20.f) * 0.004f;
            if ((ts == 1u && f > 0.f) || (ts == 0u && ahora - cerrando_at < 150u)) {
                ygo_perfil_rendija(f);
                ygo_perfil_rueda(s_ult_x, s_ult_y);         /* still there, behind the closing doors */
                g_ygo_sal_at = GetTickCount64();            /* the exit medallion too (its icon is still drawn) */
            } else {
                ygo_perfil_rendija(1.f);
                cerrando = 0;
                fprintf(stderr, "[RATON] doors shut: the Perfil button goes (doors %u, %llu ms after)\n", ts,
                        (unsigned long long)(ahora - cerrando_at));
                fflush(stderr);
                ygo_perfil_fuera();
            }
        }
        if (st == 1u && n >= 2 && n <= WHEEL_MAX && MEM32(w)      /* its camera object there */
            /* 2026-11-17 17:5x (the user: still on the doors when a menu
             * closes; video 15-06-50 at 60/s: back from the Biblioteca the
             * doors stay shut until ~0.15 s before the 3 ends, then slide):
             * only once the 3 is over. 18:1x: undone (the user: now it goes
             * missing back at the menu); back to 330 ms of frames in the 3. */
            && (ts == 0u || (ts == 3u && abre_ms >= 330u))) {
            uint32_t sa = g_eax, sc = g_ecx, sd = g_edx;
            uint32_t sb = g_ebx, si = g_esi, di = g_edi;
            float x0, y0, x1, y1;
            wheel_place(w, 0, n, &x0, &y0);
            wheel_place(w, n - 1, n, &x1, &y1);
            g_ygo_sal_x = x1;
            g_ygo_sal_y = y1;
            g_ygo_sal_sel = (int)MEM32(w + 0xA0) == n - 1;
            g_ygo_sal_at = GetTickCount64();
            s_ult_x = (x0 + x1) * 0.5f;
            s_ult_y = (y0 + y1) * 0.5f;
            ygo_perfil_rueda(s_ult_x, s_ult_y);
            g_eax = sa; g_ecx = sc; g_edx = sd;
            g_ebx = sb; g_esi = si; g_edi = di;
        }
    }
    if (w && MEM32(w + 0x6C) != 0)
        cartel_perfil_restaurar();      /* "Perfil" only in state 0 */
    if (w && MEM32(w + 0x6C) == 0) {
        int n = (int)MEM32(w + 0xAC);
        uint32_t sa = g_eax, sc = g_ecx, sd = g_edx;
        uint32_t sb = g_ebx, si = g_esi, di = g_edi;
        static unsigned frame;
        if (n >= 2 && n <= WHEEL_MAX && (w != s_wh_obj || n != s_wh_n || ++frame % 30u == 0)) {
            int k;
            float x, y;
            for (k = 0; k < n; k++) {
                wheel_place(w, k, n, &x, &y);
                s_wh_x[k] = x;
                s_wh_y[k] = y;
            }
            if (w != s_wh_obj || n != s_wh_n) {
                fprintf(stderr, "[RATON] wheel %08X: %d icons, first %.0f,%.0f last %.0f,%.0f\n",
                        w, n, s_wh_x[0], s_wh_y[0], s_wh_x[n - 1], s_wh_y[n - 1]);
                fflush(stderr);
            }
            s_wh_obj = w;
            s_wh_n = n;
        }
        s_wh_at = GetTickCount64();
        salir_textos();
        cartel_perfil(w);
        if (s_wh_n >= 2) {
            g_ygo_sal_x = s_wh_x[s_wh_n - 1];
            g_ygo_sal_y = s_wh_y[s_wh_n - 1];
            g_ygo_sal_sel = (int)MEM32(w + 0xA0) == s_wh_n - 1;
            g_ygo_sal_at = s_wh_at;
            {
                int k = (int)MEM32(w + 0xA0);
                if (k >= 0 && k < s_wh_n) {
                    g_ygo_sel_x = s_wh_x[k];
                    g_ygo_sel_y = s_wh_y[k];
                    g_ygo_sel_k = k;
                }
            }
        }
        if (s_wh_n >= 2)                /* icons 0 and n-1 are (-8,0,0) and (8,0,0): the middle */
        {
            s_ult_x = (s_wh_x[0] + s_wh_x[s_wh_n - 1]) * 0.5f;
            s_ult_y = (s_wh_y[0] + s_wh_y[s_wh_n - 1]) * 0.5f;
            ygo_perfil_rueda(s_ult_x, s_ult_y);
        }
        if (s_mode == MODE_POINTER && s_wh_seq != s_wh_done && MEM32(w + 0x9C) == 0) {
            LONG seq = s_wh_seq;
            int k = s_wh_want, cur = (int)MEM32(w + 0xA0);
            if (k < 0 || k >= n || k == cur)
                s_wh_done = seq;
            else {
                PUSH32(g_esp, 0x0010D710u);
                g_ecx = w;
                if (k > cur)
                    sub_0010D460();
                else
                    sub_0010D590();
                if (g_eax & 0xFFu) {            /* started: it goes to k */
                    MEM32(w + 0xA4) = (uint32_t)k;
                    s_wh_done = seq;
                }                               /* else: next frame */
            }
        }
        g_eax = sa; g_ecx = sc; g_edx = sd;
        g_ebx = sb; g_esi = si; g_edi = di;
    }
    if (w && MEM32(w + 0x6C) == 5u)
        caja_raton(w);                  /* its box: the pointer's line */
    sub_0010D6C0_pad();
}

/* The wheel's part of pointer mode: 1 while it waits for keys. */
static int wheel_pointer(XBOX_INPUT_STATE *n, ULONGLONG t, int over, int ledge, float gx, float gy)
{
    static int click_pending, last_k = -1;
    static float lx, ly;
    static ULONGLONG click_at, a_until;
    uint32_t w = s_wh_obj;
    int cnt = s_wh_n;
    if (t - s_wh_at > 150u || !w || cnt < 2) {
        click_pending = 0;              /* never kept for the next visit */
        return 0;
    }
    /* 2026-11-15 (the user: on the main menu a click anywhere went into a
     * mode; "solo cuando presione los botones"): the kit's own A of the left
     * button is held back here too; only the A set below (a click on an
     * icon) reaches the game. */
    mouse_a_off(n, t);
    /* 2026-11-16: the Perfil (perfil.c) first: on its button the wheel does
     * nothing; with its panel open the game sees no button or key */
    {
        /* the presses seen once for every screen (a right click that left
         * a page back to this menu was a fresh one here before) */
        int pr = ygo_perfil_raton(over, gx, gy, ledge, s_redge);
        if (pr == 2) {
            memset(&n->Gamepad, 0, sizeof n->Gamepad);
            click_pending = 0;
            return 1;
        }
        if (pr == 1)
            return 1;
    }
    if (over) {
        int k, best = -1;
        float bd = 1e9f, gap = 1e9f, r;
        for (k = 1; k < cnt; k++) {
            float dx = s_wh_x[k] - s_wh_x[k - 1], dy = s_wh_y[k] - s_wh_y[k - 1];
            float d = sqrtf(dx * dx + dy * dy);
            if (d > 1.f && d < gap)
                gap = d;
        }
        r = gap * 0.6f;
        if (r > 70.f)
            r = 70.f;
        for (k = 0; k < cnt; k++) {
            float d = (gx - s_wh_x[k]) * (gx - s_wh_x[k]) + (gy - s_wh_y[k]) * (gy - s_wh_y[k]);
            if (d < bd) {
                bd = d;
                best = k;
            }
        }
        /* the icon kept while the pointer is still on it */
        if (last_k >= 0 && last_k < cnt) {
            float dx = gx - s_wh_x[last_k], dy = gy - s_wh_y[last_k];
            if (dx * dx + dy * dy <= r * r)
                best = last_k;
        }
        if (best >= 0 && bd > r * r && best != last_k)
            best = -1;
        /* sent on a new icon, and again when the pointer moves on one */
        if (best >= 0 && (best != last_k || fabsf(gx - lx) > 3.f || fabsf(gy - ly) > 3.f)) {
            last_k = best;
            lx = gx;
            ly = gy;
            s_wh_want = best;
            InterlockedIncrement(&s_wh_seq);
        } else if (best < 0)
            last_k = -1;
        if (ledge) {
            click_pending = best >= 0;
            click_at = t;
            fprintf(stderr, "[RATON] wheel click game %.0f,%.0f -> icon %d of %d, chosen %u\n",
                    gx, gy, best, cnt, MEM32(w + 0xA0));
            fflush(stderr);
        }
        if (GetAsyncKeyState(VK_RBUTTON) & 0x8000)
            n->Gamepad.bAnalogButtons[XBOX_BUTTON_B] = 255;
    }
    /* A once the wheel has got to the icon and stands still */
    if (click_pending && ((s_wh_done == s_wh_seq && MEM32(w + 0x9C) == 0
                && (int)MEM32(w + 0xA0) == s_wh_want) || t - click_at >= 800u)) {
        click_pending = 0;
        a_until = t + PRESS_MS + 15u;
    }
    if (t < a_until)
        n->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = 255;
    return 1;
}

/* ---- screens with an arrow on each side --------------------------------- */

/* Choices shown one at a time with an arrow at each side and no glove:
 * - the duel setup screen, sub_00109EB0 (thiscall update, state at +0x70):
 *   state 3 is rock-paper-scissors ("Seleccionar tu mano"; d-pad 0x800 to
 *   sub_00107C30, the previous hand, 0x400 to sub_00107BD0, the next, A
 *   confirms), state 9 another left/right choice (sub_00108A70 /
 *   sub_00108A10, A);
 * (the duel's three-way box, sub_00087A80, was here until 2026-11-09: it is
 * a list, now ask_box.)
 * A click left of the middle presses left, right of it presses right, on
 * the middle presses A, so the arrows and the choice work as they look.
 * Pressing the d-pad goes through the game's own handling (sound, wrap). */
void sub_00109EB0_pad(void);
void sub_00087A80_pad(void);

#define SIDE_PX 60.f                    /* the middle choice: |x - 320| below */
static volatile ULONGLONG s_side_at;
static volatile int s_side_state;

void sub_00109EB0(void)
{
    uint32_t st = g_ecx ? MEM32(g_ecx + 0x70) : 0;
    if (st == 3u || st == 9u) {
        s_side_at = GetTickCount64();
        s_side_state = (int)st;
    }
    sub_00109EB0_pad();
}

/* Other menus that read left/right (0x800/0x400 from sub_00028DE0) with no
 * glove: the opponent select ("Seleccionar oponente": big arrows each side
 * of the name) is one of them. Arrow clicks there only while no glove is
 * tracked, so a screen with a glove keeps its hover. */
void sub_000F3DE0_pad(void);
void sub_00101FD0_pad(void);
void sub_00106CE0_pad(void);
void sub_0010B0C0_pad(void);

/* The opponent select ran then (sub_00106CE0). It has no glove of its own,
 * so it does not wait for "no glove": 2026-11-14, another PC, after the
 * deck builder (whose glove is tracked: 18:26 log, fingertip 408,342 inside
 * it) the opponents did not follow the mouse, as on 2026-11-12. A glove of
 * the editor still alive in memory (glove_poll drops one only when its
 * vtable changes) kept s_gx >= 0 and the arrows off. */
static volatile ULONGLONG s_opp_at;

static void side_if_gloveless(int screen)
{
    ULONGLONG now = GetTickCount64();
    if ((screen == 0x106 || s_gx < 0.f) && now - s_wh_at > 150u) {   /* not under the wheel */
        s_side_at = now;
        s_side_state = screen;
    }
}

void sub_000F3DE0(void) { side_if_gloveless(0xF3); sub_000F3DE0_pad(); }
/* sub_00101FD0 is the gods' choice after a win ("Cual quieres?"): premios.c's sign */
void ygo_premios_dioses_tick(uint32_t scr);
void sub_00101FD0(void)
{
    uint32_t scr = g_ecx;
    side_if_gloveless(0x101);
    sub_00101FD0_pad();
    ygo_premios_dioses_tick(scr);
}
/* the opponent select (premios.c: the marked opponent's rewards sign) */
void ygo_premios_tick(uint32_t scr);
void sub_00106CE0(void)
{
    uint32_t scr = g_ecx;
    s_opp_at = GetTickCount64();
    side_if_gloveless(0x106);
    sub_00106CE0_pad();
    ygo_premios_tick(scr);
}
void sub_0010B0C0(void) { side_if_gloveless(0x10B); sub_0010B0C0_pad(); }

/* sub_00087A80 (the duel's three-way box) was here as left/right arrows
 * (state 100); it is a list of lines one under the other (its marker moves
 * in y): see ask_box below (2026-11-09). */

/* 1 while such a screen waits for keys. */
static int side_pointer(XBOX_INPUT_STATE *n, ULONGLONG t, int over, int ledge, float gx, float gy)
{
    static ULONGLONG a_until;
    if (t - s_side_at > 150u)
        return 0;
    if (over) {
        if (ledge) {
            float dx = gx - 320.f;
            const char *what;
            if (dx < -SIDE_PX) {
                what = press(XBOX_GAMEPAD_DPAD_LEFT, t) ? "left" : "left (busy)";
            } else if (dx > SIDE_PX) {
                what = press(XBOX_GAMEPAD_DPAD_RIGHT, t) ? "right" : "right (busy)";
            } else {
                what = "A";
                a_until = t + PRESS_MS + 15u;
            }
            fprintf(stderr, "[RATON] arrows click game %.0f,%.0f (screen %d) -> %s\n",
                    gx, gy, s_side_state, what);
            fflush(stderr);
        }
        if (GetAsyncKeyState(VK_RBUTTON) & 0x8000)
            n->Gamepad.bAnalogButtons[XBOX_BUTTON_B] = 255;
    }
    /* 2026-11-12 (video 22-53-27): a click on the opponent select's right
     * arrow moved to the next one AND went into the duel: the kit's own A of
     * the left button reached the game with the arrow's d-pad. Only the A
     * set here (a click on the middle) goes through. */
    mouse_a_off(n, t);
    if (t < a_until)
        n->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = 255;
    if (s_wheel && s_held == 0) {       /* the mouse wheel steps too */
        LONG w = s_wheel;
        if (press(w > 0 ? XBOX_GAMEPAD_DPAD_LEFT : XBOX_GAMEPAD_DPAD_RIGHT, t))
            { InterlockedExchangeAdd(&s_wheel, w > 0 ? -1 : 1); s_held_wheel = 1; }
    }
    return 1;
}

/* ---- the triple duel's order (2026-11-15) --------------------------------- */

/* The user: the arrows between the three opponents' portraits ("Cambia el
 * orden de la batalla") work with the keyboard, not the mouse (a click was
 * only the kit's A: the battle started). The screen is sub_001057D0
 * (thiscall update, called by sub_000F94B0 / sub_000FF800; no glove): keys
 * from sub_00028DE0 | sub_00028DC0, state at +0x3D8:
 * - 0: up (0x100) / down (0x200) the level (+0x3E8, the "9" on the left);
 *   right (0x400) goes to the order (+0x3F4 = 0, help text 43, state 1);
 *   A (0x2) starts the battle, B (0x4) goes back;
 * - 1: right marks the second pair of arrows (+0x3F4 = 1), left the first
 *   one (or, on the first, back to state 0, help text 42); A (0x2) swaps
 *   the two opponents of the marked arrows (+0x404 = 0, state 5).
 * Where they are (the user's capture, 1920x1080 = 640x480 x 3, x 2.25): the
 * two pairs of arrows at x 343 and 453, y 213; the level's red triangles at
 * x 83 and 190, y 173.
 * A click on a pair of arrows walks there with the game's own keys (right
 * from state 0, then left/right to it) and then presses A: the swap with its
 * own sound and animation. On a level triangle: up / down. Anywhere else: A,
 * as before. Right click: B.
 * 2026-11-15 (the user: "cuando aprieto con el click el 9 no me deja
 * seleccionar y tampoco ir hacia atras cuando tengo una flecha seleccionada
 * con el segundo click"): a click on the level (the "9", x 105..165) while an
 * arrow is marked goes back to it (left, the game's own way out of state 1);
 * the right click there does the same (B only from the level). */
void sub_001057D0_pad(void);
static volatile uint32_t s_tri_obj;
static volatile ULONGLONG s_tri_at;

void ygo_premios_triple_tick(uint32_t scr);   /* premios.c: the Tournament rewards sign */
void sub_001057D0(void)
{
    uint32_t scr = g_ecx;
    s_tri_obj = scr;
    s_tri_at = GetTickCount64();
    sub_001057D0_pad();
    ygo_premios_triple_tick(scr);
}

static const float k_tri_x[2] = { 343.f, 453.f };
#define TRI_Y 213.f
#define TRI_DX 38.f
#define TRI_DY 32.f
#define TRI_LV_Y0 148.f
#define TRI_LV_Y1 198.f
#define TRI_NIVEL (-2)                   /* want: back to the level (state 0) */

static int tri_pointer(XBOX_INPUT_STATE *n, ULONGLONG t, int over, int ledge, float gx, float gy)
{
    static int want = -1;
    static ULONGLONG want_at, a_until, p_at;
    static uint32_t p_st = 0xFFFFFFFFu, p_sel;
    static int rb;
    uint32_t o = s_tri_obj, st, sel;
    int k, rdown;
    if (t - s_tri_at > 150u || !o) {
        want = -1;
        return 0;
    }
    st = MEM32(o + 0x3D8u);
    sel = MEM32(o + 0x3F4u);
    mouse_a_off(n, t);
    if (over && ledge) {
        const char *what = "A";
        k = -1;
        for (int i = 0; i < 2; i++)
            if (fabsf(gx - k_tri_x[i]) <= TRI_DX && fabsf(gy - TRI_Y) <= TRI_DY)
                k = i;
        if (k >= 0) {
            want = k;
            want_at = t;
            p_st = 0xFFFFFFFFu;
            what = k ? "second arrows" : "first arrows";
        } else if (gy >= TRI_LV_Y0 && gy <= TRI_LV_Y1 && gx >= 55.f && gx <= 110.f) {
            what = press(XBOX_GAMEPAD_DPAD_UP, t) ? "level -" : "level - (busy)";
        } else if (gy >= TRI_LV_Y0 && gy <= TRI_LV_Y1 && gx >= 165.f && gx <= 220.f) {
            what = press(XBOX_GAMEPAD_DPAD_DOWN, t) ? "level +" : "level + (busy)";
        } else if (gy >= TRI_LV_Y0 - 10.f && gy <= TRI_LV_Y1 + 10.f && gx > 110.f && gx < 165.f) {
            if (st == 1u) {
                want = TRI_NIVEL;
                want_at = t;
                p_st = 0xFFFFFFFFu;
            }
            what = st == 1u ? "back to the level" : "level (already)";
        } else {
            a_until = t + PRESS_MS + 15u;
        }
        fprintf(stderr, "[RATON] triple click game %.0f,%.0f (state %u, arrows %u) -> %s\n",
                gx, gy, st, sel, what);
        fflush(stderr);
    }
    /* walk to the arrows wanted, one key at a time: the next one only once
     * the game took the last (its state or marked arrows changed) */
    if (want >= 0 || want == TRI_NIVEL) {   /* 2026-11-15: the level (-2) was left out: never walked back */
        if (want == TRI_NIVEL && st != 1u) {
            want = -1;                  /* on the level (or past it) */
        } else if (t - want_at > 2000u || st > 1u) {
            if (t - want_at > 2000u)
                fprintf(stderr, "[RATON] triple: arrows %d not reached (state %u)\n", want, st);
            want = st > 1u && t - want_at <= 2000u ? want : -1;
        } else if (st != p_st || sel != p_sel || t - p_at > 400u) {
            WORD key = 0;
            if (want == TRI_NIVEL)
                key = XBOX_GAMEPAD_DPAD_LEFT;
            else if (st == 0u)
                key = XBOX_GAMEPAD_DPAD_RIGHT;
            else if ((int)sel < want)
                key = XBOX_GAMEPAD_DPAD_RIGHT;
            else if ((int)sel > want)
                key = XBOX_GAMEPAD_DPAD_LEFT;
            if (!key) {
                a_until = t + PRESS_MS + 15u;
                want = -1;
            } else if (press(key, t)) {
                p_st = st;
                p_sel = sel;
                p_at = t;
            }
        }
    }
    rdown = over && (GetAsyncKeyState(VK_RBUTTON) & 0x8000);
    if (over && s_redge)
        rb = st != 1u;                  /* B only for a press begun on the level */
    if (!rdown)
        rb = 0;
    if (over && s_redge && st == 1u) {
        want = TRI_NIVEL;
        want_at = t;
        p_st = 0xFFFFFFFFu;
        fprintf(stderr, "[RATON] triple: right click -> back to the level\n");
        fflush(stderr);
    }
    if (rdown && rb)
        n->Gamepad.bAnalogButtons[XBOX_BUTTON_B] = 255;
    if (t < a_until)
        n->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = 255;
    /* 2026-11-16 (the user: "no puedo usar la ruedita como antes para subir y
     * bajar"): this screen returns before the generic wheel step, so it is
     * here too: wheel up / down = up / down (the level, in state 0), while
     * no walk to the arrows is under way */
    if (s_wheel && s_held == 0 && want == -1) {
        LONG w = s_wheel;
        if (press(w > 0 ? XBOX_GAMEPAD_DPAD_UP : XBOX_GAMEPAD_DPAD_DOWN, t))
            { InterlockedExchangeAdd(&s_wheel, w > 0 ? -1 : 1); s_held_wheel = 1; }
    }
    return 1;
}

/* Fast card plays: back to normal speed once the game waits for the player
 * again (the field cursor running for BOOST_IDLE_MS, or the buttons
 * waiting), at most BOOST_MAX_MS after the play. */
static void boost_tick(ULONGLONG t)
{
    static ULONGLONG run_since;
    int field_live = t - s_field_at <= 60u;
    if (!field_live)
        run_since = 0;
    else if (!run_since)
        run_since = t;
    if (!s_boost)
        return;
    if (!s_fast_cards || s_mode != MODE_POINTER)
        boost_stop("turned off");
    else if (t - s_boost_at > BOOST_MAX_MS)
        boost_stop("timeout");
    else if (t - s_boost_at > BOOST_MIN_MS
             && ((run_since && t - run_since >= BOOST_IDLE_MS) || t - s_cmd_at <= 60u))
        boost_stop("waiting for the player");
}

/* The card's info panel: any click is "back" (B), nothing else runs. */
static int info_pointer(XBOX_INPUT_STATE *n, ULONGLONG t, int over, int ledge)
{
    static ULONGLONG b_until;
    if (t - s_info_at > 150u)
        return 0;
    /* 2026-11-17: the presses seen once for every screen; the kit's A of
     * the left button stays out (the click is "back" only) */
    mouse_a_off(n, t);
    if (over && (ledge || s_redge)) {
        b_until = t + PRESS_MS + 15u;
        fprintf(stderr, "[RATON] card info: click -> back\n");
        fflush(stderr);
    }
    if (t < b_until)
        n->Gamepad.bAnalogButtons[XBOX_BUTTON_B] = 255;
    return 1;
}

/* ---- the duel's boxes of options (phases, yes/no) ------------------------ */

/* 2026-11-09, the user: the menu B opens on the field (Main Phase 2, end of
 * turn, battle) "no se puede usar con el mouse". B there (sub_0009A540,
 * 0x9A65E) opens sub_00087FA0's box: title 0xD11 and three lines (0xD44 or
 * 0xD41, 0xD45, 0xD42). The box is the block at 0x660B70: [0x660B74] lines,
 * [0x660B8A] the line marked, [0x660B8C] the answer, [0x660B9A] its step (0
 * waiting for keys), and two handlers by pointer: 3 lines sub_00087A80
 * (keys) / sub_00087A10 (draws the marker), 2 lines sub_000878C0 /
 * sub_00087850. The marker (sprite 0x41BB) of line k is drawn at
 * x [0x660B78]*8 + 8, y [0x660B82 + 2k] + ([0x660B99] - [0x660B7E])*8 - 14.
 * Up/down there just change [0x660B8A] (with the tick), A answers it.
 * Wrapped: the line under the pointer is marked the same way; a click marks
 * it and hands A to the handler for that one call (as the command buttons). */
void sub_00087A80_pad(void);
void sub_000878C0_pad(void);

#define ASK_MAX 3
static volatile ULONGLONG s_ask_at;              /* a box waited for keys then */
static volatile int s_ask_n;
static volatile float s_ask_x, s_ask_y[ASK_MAX];
static volatile LONG s_ask_want = -1, s_ask_click = -1;
static volatile ULONGLONG s_ask_click_at;

static void ask_box(int lines, int wait, void (*pad)(void))
{
    int k, inject = 0;
    uint8_t was = 0;
    if (MEM8(0x00660B9Au) == (uint32_t)wait && s_mode == MODE_POINTER) {
        ULONGLONG t = GetTickCount64();
        /* 2026-11-09 (screenshots, log: the marker's numbers gave y -142 for
         * every line): the box on screen is the list widget
         * [[0x656F88]+0x58] (sub_00047850/47880 fill it, sub_00047A40 marks
         * [0x660B8A] there; the Biblioteca's exit box is the same widget).
         * Its draw (sub_000F0040): +0xBC4 lines, line k the object at
         * +0x1B0 + 4k, placed by sub_000F6FD0 at its +0xC/+0x10 (floats,
         * game px); the marked line's bar goes at the same place, 450 wide
         * (x = centre - 225) and 30 a line (the step, 0x42417C). */
        uint32_t g = MEM32(0x00656F88u), w = g ? MEM32(g + 0x58u) : 0u;
        int got = 0;
        if (w && (int)MEM32(w + 0xBC4u) == lines)
            for (k = 0; k < lines; k++) {
                uint32_t o = MEM32(w + 0x1B0u + 4u * (uint32_t)k);
                float x, y;
                if (!o)
                    break;
                x = fbits(MEM32(o + 0xCu));
                y = fbits(MEM32(o + 0x10u));
                if (!on_screen(x, y))
                    break;
                if (k == 0)
                    s_ask_x = x;
                s_ask_y[k] = y;
                got++;
            }
        if (t - s_ask_at > 500u) {
            s_ask_want = s_ask_click = -1;          /* a new box */
            /* 2026-11-17 (the user: "en los menu de entrar en batalla, main
             * phase 2 ... que por default este el de arriba del todo"): the
             * phases' box (3 lines, B on the field) opens on its top line */
            if (lines == 3 && MEM16(0x00660B8Au) != 0u) {
                fprintf(stderr, "[RATON] phases box: top line marked (was %u)\n", MEM16(0x00660B8Au));
                MEM16(0x00660B8Au) = 0;
            }
            fprintf(stderr, "[RATON] box of %d lines (widget %08X, %u lines), x %.0f, lines y", lines,
                    w, w ? MEM32(w + 0xBC4u) : 0u, s_ask_x);
            for (k = 0; k < got; k++)
                fprintf(stderr, " %.0f", s_ask_y[k]);
            fprintf(stderr, ", marked %u\n", MEM16(0x00660B8Au));
            fflush(stderr);
        }
        s_ask_n = got == lines ? lines : 0;
        s_ask_at = t;
        k = s_ask_want;
        if (k >= 0 && k < lines && (MEM8(0x006589E6u)) == 0u) {
            if (MEM16(0x00660B8Au) != (uint32_t)k) {
                uint32_t sa = g_eax, sc = g_ecx, sd = g_edx;
                MEM16(0x00660B8Au) = (uint16_t)k;
                PUSH32(g_esp, 0x10000001u);
                PUSH32(g_esp, 0x00087AADu);
                sub_0007E280();
                g_esp += 4;
                g_eax = sa; g_ecx = sc; g_edx = sd;
            } else if (s_ask_click == k) {
                if (t - s_ask_click_at > 1500u)
                    s_ask_click = -1;
                else {
                    was = MEM8(0x006589E6u);
                    MEM8(0x006589E6u) = 1u;         /* A, just for this call */
                    inject = 1;
                }
            }
        }
    }
    pad();
    if (inject) {
        MEM8(0x006589E6u) = was;
        fprintf(stderr, "[RATON] box: line %ld answered\n", (long)s_ask_click);
        fflush(stderr);
        s_ask_click = -1;
    }
}

void sub_00087A80(void) { ask_box(3, 0, sub_00087A80_pad); }
void sub_000878C0(void) { ask_box(2, 0, sub_000878C0_pad); }

/* 2026-11-12 (screenshot, "Selecciona la posición de la carta."): that box
 * does not use the handlers above. sub_00088160(flags, text, draw, keys)
 * opens a box with its own handlers; this one (text 0xD2C, from 0x9468E /
 * 0x954D9) has keys sub_00093EE0: step [0x660B9A] 0 fills the widget's two
 * lines (Pos. Ataque / Pos. Defensa, sub_00047850/47880), 1 waits for keys
 * (0xC0 up/down flips [0x660B8A], 1 = A answers it). The pointer never saw
 * it, so only the wheel (up/down) moved the bar. Same widget, same marking:
 * ask_box waiting on step 1. */
void sub_00093EE0_pad(void);
void sub_00093EE0(void) { ask_box(2, 1, sub_00093EE0_pad); }

/* The other boxes of sub_00088160 (2026-11-12, read from the code, no
 * screenshot yet):
 *   "Selecciona Tipo." (0xD26) keys sub_00090030, 20 lines;
 *   "Selecciona Atributo." (0xD27) keys sub_000904B0, 6 lines.
 * Both wait for keys when [0x660B9B] == 1 and [0x660B9A] != 0: 0x40 marks
 * the line before ([0x660B8A] - 1), 0x80 the next (no wrap), A (1) answers
 * it ([0x660B8C]) once the widget is still (sub_00047A20). With more than 5
 * lines the widget (sub_000F0040, 0xF0384) shows only the marked line, in
 * its line object 0 ([w + 0x1B0], placed at +0xC / +0x10), with an arrow
 * above and below (sub_000F68E0 on [w + 0xBD0] / [w + 0xBD4]).
 *   "Declara el nivel del monstruo" (0xD35) keys sub_00090300: a number
 * 1..12 in [0x660B8A], 0x20 (left) / 0x40 one less, 0x10 (right) / 0x80 one
 * more, A answers; no widget lines.
 * Pointer: the wheel steps (up: the line before / one level more); a click
 * on the shown line answers it, above it the line before, below it the
 * next; on the level box a click answers. The kit's own A (the left
 * button) stays out, so a click never answers what was marked before.
 * ("Descarta de tu mano." 0xD20 picks with sub_00092E20, the target cursor
 * that already has the pointer; "Cambia el orden de tus cartas" 0xD2A,
 * sub_00091A90, has five cards and no places known yet.) */
#define SPIN_LIST  1
#define SPIN_LEVEL 2
static volatile ULONGLONG s_sp_at;
static volatile int s_sp_kind;
static volatile float s_sp_x, s_sp_y;            /* the shown line, -1 none */
static volatile LONG s_sp_steps;                 /* + next / one more */
static volatile LONG s_sp_answer;                /* a click: A */
static volatile ULONGLONG s_sp_answer_at;

static void spin_box(int kind, int waiting, void (*pad)(void))
{
    uint8_t was = MEM8(0x006589E6u), put = 0;
    uint16_t b8c = 0;
    if (waiting && s_mode == MODE_POINTER) {
        ULONGLONG t = GetTickCount64();
        uint32_t g = MEM32(0x00656F88u), w = g ? MEM32(g + 0x58u) : 0u, o = 0;
        float x = -1.f, y = -1.f;
        if (kind == SPIN_LIST && w)
            o = MEM32(w + 0x1B0u);
        if (o) {
            x = fbits(MEM32(o + 0xCu));
            y = fbits(MEM32(o + 0x10u));
            if (!on_screen(x, y))
                x = y = -1.f;
        }
        if (t - s_sp_at > 500u) {
            InterlockedExchange(&s_sp_steps, 0);
            InterlockedExchange(&s_sp_answer, 0);
            fprintf(stderr, "[RATON] %s box: shown line at %.0f,%.0f (widget %08X, %u lines), marked %u\n",
                    kind == SPIN_LIST ? "list" : "level", x, y, w, w ? MEM32(w + 0xBC4u) : 0u,
                    MEM16(0x00660B8Au));
            fflush(stderr);
        }
        s_sp_kind = kind;
        s_sp_x = x;
        s_sp_y = y;
        s_sp_at = t;
        if (was == 0u) {
            LONG s = s_sp_steps;
            if (s) {
                /* one step a call, the game's own (bounds, sound) */
                put = kind == SPIN_LIST ? (s > 0 ? 0x80u : 0x40u) : (s > 0 ? 0x10u : 0x20u);
                InterlockedExchangeAdd(&s_sp_steps, s > 0 ? -1 : 1);
            } else if (s_sp_answer) {
                if (t - s_sp_answer_at > 1500u)
                    InterlockedExchange(&s_sp_answer, 0);
                else
                    put = 1u;           /* A, until the widget takes it */
            }
            if (put)
                MEM8(0x006589E6u) = put;
        }
    }
    /* the answer goes to [0x660B8C] only when the widget took the A: a
     * mark there tells (put back if not) */
    if (put == 1u) {
        b8c = MEM16(0x00660B8Cu);
        MEM16(0x00660B8Cu) = 0xFFFFu;
    }
    pad();
    if (put) {
        MEM8(0x006589E6u) = was;
        if (put == 1u) {
            if (MEM16(0x00660B8Cu) == 0xFFFFu)
                MEM16(0x00660B8Cu) = b8c;       /* the widget was moving: again */
            else {
                fprintf(stderr, "[RATON] %s box: answered %u\n", kind == SPIN_LIST ? "list" : "level",
                        MEM16(0x00660B8Cu));
                fflush(stderr);
                InterlockedExchange(&s_sp_answer, 0);
            }
        }
    }
}

void sub_00090030_pad(void);
void sub_000904B0_pad(void);
void sub_00090300_pad(void);
void sub_00090030(void)
{
    spin_box(SPIN_LIST, MEM8(0x00660B9Bu) == 1u && MEM8(0x00660B9Au) != 0u, sub_00090030_pad);
}
void sub_000904B0(void)
{
    spin_box(SPIN_LIST, MEM8(0x00660B9Bu) == 1u && MEM8(0x00660B9Au) != 0u, sub_000904B0_pad);
}
void sub_00090300(void)
{
    /* not while the CPU picks ([0x65F91C] & 4) */
    spin_box(SPIN_LEVEL, MEM8(0x00660B9Au) != 0u && !(MEM8(0x0065F91Cu) & 4u), sub_00090300_pad);
}

/* The pointer's part: 1 while one of those boxes waits for keys. */
static int spin_pointer(XBOX_INPUT_STATE *n, ULONGLONG t, int over, int ledge, float gx, float gy)
{
    float x = s_sp_x, y = s_sp_y;
    if (t - s_sp_at > 150u)
        return 0;
    mouse_a_off(n, t);
    if (ledge && over) {
        const char *what = "nothing";
        if (s_sp_kind == SPIN_LEVEL) {
            what = "answer";
            s_sp_answer_at = t;
            InterlockedExchange(&s_sp_answer, 1);
        } else if (y >= 0.f && gx >= x - 20.f && gx <= x + 470.f) {
            if (gy >= y && gy < y + 30.f) {
                what = "answer";
                s_sp_answer_at = t;
                InterlockedExchange(&s_sp_answer, 1);
            } else if (gy >= y - 45.f && gy < y) {
                what = "line before";
                InterlockedExchangeAdd(&s_sp_steps, -1);
            } else if (gy >= y + 30.f && gy < y + 75.f) {
                what = "next line";
                InterlockedExchangeAdd(&s_sp_steps, 1);
            }
        }
        fprintf(stderr, "[RATON] spin box click game %.0f,%.0f -> %s (line at %.0f,%.0f)\n",
                gx, gy, what, x, y);
        fflush(stderr);
    }
    if (s_wheel) {
        /* up (+): the line before, or one level more */
        LONG w = InterlockedExchange(&s_wheel, 0);
        InterlockedExchangeAdd(&s_sp_steps, s_sp_kind == SPIN_LEVEL ? w : -w);
    }
    n->Gamepad.sThumbLX = n->Gamepad.sThumbLY = 0;
    n->Gamepad.sThumbRX = n->Gamepad.sThumbRY = 0;
    return 1;
}

/* The box's part of pointer mode: 1 while one waits for keys. Line k is its
 * bar: x .. x + 450, y .. y + 30 from the line's place. */
static int ask_pointer(XBOX_INPUT_STATE *n, ULONGLONG t, int over, int ledge, float gx, float gy)
{
    static float lx = -1.f, ly = -1.f;
    int k, best = -1, lines = s_ask_n;
    if (t - s_ask_at > 150u)
        return 0;
    /* 2026-11-17 (mouse review): the kit's A of the left button stays out:
     * a click off the lines answered the marked one with it. And the box
     * takes a margin (logs of 2026-11-12: clicks 2 and 4 px above the first
     * bar, on its text's top, got "line -1"): 8 px above the first line,
     * below the last and at each end. */
    mouse_a_off(n, t);
    if (over && lines > 0 && gx >= s_ask_x - 8.f && gx <= s_ask_x + 458.f)
        for (k = 0; k < lines && k < ASK_MAX; k++) {
            float y0 = s_ask_y[k] - (k == 0 ? 8.f : 0.f);
            float y1 = s_ask_y[k] + 30.f + (k == lines - 1 ? 8.f : 0.f);
            if (gy >= y0 && gy < y1)
                best = k;
        }
    if (best >= 0 && (gx != lx || gy != ly))
        InterlockedExchange(&s_ask_want, best);
    lx = gx;
    ly = gy;
    if (ledge && over) {
        fprintf(stderr, "[RATON] box click game %.0f,%.0f -> line %d (marker x %.0f, lines y %.0f %.0f %.0f)\n",
                gx, gy, best, s_ask_x, s_ask_y[0], s_ask_y[1], lines > 2 ? s_ask_y[2] : -1.f);
        fflush(stderr);
        if (best >= 0) {
            InterlockedExchange(&s_ask_want, best);
            s_ask_click_at = t;
            InterlockedExchange(&s_ask_click, best);
        }
    }
    if (over && (GetAsyncKeyState(VK_RBUTTON) & 0x8000))
        n->Gamepad.bAnalogButtons[XBOX_BUTTON_B] = 255;
    InterlockedExchange(&s_wheel, 0);
    /* the mouse's motion is on the left stick (kit): kept off the box */
    n->Gamepad.sThumbLX = n->Gamepad.sThumbLY = 0;
    n->Gamepad.sThumbRX = n->Gamepad.sThumbRY = 0;
    return 1;
}

void ygo_caja_nota(const char *fmt, ...);   /* caja_negra.c */
void ygo_caja_toque(void);

/* ---- the coin toss --------------------------------------------------------
 * 2026-11-17 (the user, with Abare Ushioni: "no deja mover con el mouse que
 * elegir, si con el teclado"). The request of mode 0x13 (sub_00084D00(p,
 * 0x13); Time Wizard, Fiend Comedian, Gamble) is sub_00090D20, step u8
 * [0x65F92E]: 0 opens the coin (sub_000475C0, object [[0x656F8C]+0x48]; the
 * CPU picks at random and skips step 1), 1 waits for keys: 0x30 (left /
 * right) flips the side u16 [0x660B8C] (sub_00047600 shows it on the coin),
 * A answers; then [0x65F930] = the side called == the toss. On screen
 * (screenshot 1919x1079 = 640x480 game px) the coin is the big one in the
 * middle, centre ~322,213, ~57 x 75 px of radius, with arrows at ~225,224
 * and ~414,224. Pointer: a click on an arrow (or the wheel) flips, a click on
 * the coin answers; given to the handler as 0x10 / A for one call, kept
 * until the handler takes it (it waits while the coin turns, sub_00047630). */
void sub_00090D20_pad(void);
static volatile ULONGLONG s_coin_at, s_coin_cmd_at;
static volatile LONG s_coin_flip, s_coin_ok;

void sub_00090D20(void)
{
    int inj = 0;
    uint8_t was = 0, st = MEM8(0x0065F92Eu);
    uint16_t sel = MEM16(0x00660B8Cu);
    if (st == 1u && s_mode == MODE_POINTER) {
        ULONGLONG t = GetTickCount64();
        if (t - s_coin_at > 500u) {
            s_coin_flip = s_coin_ok = 0;            /* a new toss */
            fprintf(stderr, "[RATON] coin toss: waiting (side %u)\n", sel);
            fflush(stderr);
            ygo_caja_nota("moneda: espera la cara (cara %u)", sel);
        }
        s_coin_at = t;
        if ((s_coin_flip || s_coin_ok) && t - s_coin_cmd_at > 1500u)
            s_coin_flip = s_coin_ok = 0;
        if ((s_coin_flip || s_coin_ok) && MEM8(0x006589E6u) == 0u) {
            was = MEM8(0x006589E6u);
            MEM8(0x006589E6u) = s_coin_flip ? 0x10u : 1u;
            inj = 1;
        }
    }
    sub_00090D20_pad();
    if (inj) {
        MEM8(0x006589E6u) = was;
        if (s_coin_flip && MEM16(0x00660B8Cu) != sel) {
            InterlockedExchange(&s_coin_flip, 0);
            fprintf(stderr, "[RATON] coin toss: side %u\n", MEM16(0x00660B8Cu));
            fflush(stderr);
        } else if (!s_coin_flip && MEM8(0x0065F92Eu) != st) {
            InterlockedExchange(&s_coin_ok, 0);
            fprintf(stderr, "[RATON] coin toss: side %u called\n", sel);
            fflush(stderr);
            ygo_caja_nota("moneda: elegida la cara %u (el mouse)", sel);
        }
    }
}

static int coin_pointer(XBOX_INPUT_STATE *n, ULONGLONG t, int over, int ledge, float gx, float gy)
{
    if (t - s_coin_at > 150u)
        return 0;
    mouse_a_off(n, t);
    if (InterlockedExchange(&s_wheel, 0) && !s_coin_ok) {
        s_coin_cmd_at = t;
        InterlockedExchange(&s_coin_flip, 1);
    }
    if (ledge && over) {
        float dx = (gx - 322.f) / 62.f, dy = (gy - 213.f) / 80.f;
        int coin = dx * dx + dy * dy <= 1.f;
        int arrow = fabsf(gy - 224.f) <= 32.f && (fabsf(gx - 225.f) <= 32.f || fabsf(gx - 414.f) <= 32.f);
        fprintf(stderr, "[RATON] coin toss click game %.0f,%.0f -> %s\n", gx, gy,
                arrow ? "flip" : coin ? "call" : "nothing");
        fflush(stderr);
        if (arrow || coin) {
            s_coin_cmd_at = t;
            InterlockedExchange(arrow ? &s_coin_flip : &s_coin_ok, 1);
        }
    }
    n->Gamepad.sThumbLX = n->Gamepad.sThumbLY = 0;
    n->Gamepad.sThumbRX = n->Gamepad.sThumbRY = 0;
    return 1;
}

/* ---- Opciones and Guardar (the main menu's pages) ------------------------ */

/* 2026-11-10, screenshots of both pages (1919x1079 = 640x480 game px).
 *
 * Opciones: sub_0010C2E0 (thiscall, page = ecx). [page+0x40] is its state,
 * 3 waiting for keys (from sub_00028DE0: 0x100 up / 0x200 down step the row,
 * sub_0010C0E0 / sub_0010C0C0; 0x400 / 0x800 change the value,
 * sub_0010BD80 / sub_0010BE50; B leaves). The rows' model is [page]: the
 * marked row [model+0x3C] (0..4), sub_0010BF20(k) (thiscall on the model)
 * marks row k (the bar, the label colours); the step's sound is
 * sub_0011A400(0x10000001). On screen (game px): row k from y 173.5 + 44 k,
 * 44 high, x 97..543; the marked row's bar y 194 + 44 k .. 219 + 44 k with
 * its arrows at x 97..120 (left) and 520..543 (right).
 * Pointer: the row under it is marked; a click on an arrow presses that
 * side; the wheel steps rows; right click is B.
 *
 * Guardar: sub_0010E8E0 (thiscall, page = ecx), state [page+0x54], 6
 * waiting for keys: 0x400 / 0x800 (sub_00028DE0) slide to the next /
 * previous slot (states 8 / 7, then [page+0x48] +- 1, 0..4), A saves to
 * [page+0x48], B leaves. The slot shown sits in the middle, x 258..382,
 * y 133..333; the others every 150.7 px to each side.
 * Pointer: a click on the middle slot is A; on another slot, the slides to
 * it (one at a time, each after the game took the last); the wheel slides;
 * right click is B. Other states (the yes/no box) keep the usual clicks. */
void sub_0010C2E0_pad(void);
void sub_0010E8E0_pad(void);
void sub_0010BF20(void);
void sub_0011A400(void);

#define OPT_Y0 173.5f
#define OPT_DY 44.f
static volatile ULONGLONG s_opt_at, s_save_at;
static volatile LONG s_opt_want = -1, s_opt_row = -1;
static volatile LONG s_save_slot = -1, s_save_steps;

void sub_0010C2E0(void)
{
    uint32_t page = g_ecx, m = page ? MEM32(page) : 0u;
    if (m && MEM32(page + 0x40u) == 3u && s_mode == MODE_POINTER) {
        LONG k = InterlockedExchange(&s_opt_want, -1);
        s_opt_at = GetTickCount64();
        if (k >= 0 && k <= 4 && (LONG)MEM32(m + 0x3Cu) != k) {
            uint32_t sp = g_esp, sa = g_eax, sc = g_ecx, sd = g_edx;
            g_ecx = m;
            PUSH32(g_esp, (uint32_t)k);
            PUSH32(g_esp, 0x0010C0EEu);
            sub_0010BF20();
            g_esp = sp;
            PUSH32(g_esp, 0x10000001u);
            PUSH32(g_esp, 0x0010C461u);
            sub_0011A400();
            g_esp = sp;
            g_eax = sa; g_ecx = sc; g_edx = sd;
        }
        s_opt_row = (LONG)MEM32(m + 0x3Cu);
    }
    sub_0010C2E0_pad();
}

void sub_0010E8E0(void)
{
    uint32_t page = g_ecx, st = page ? MEM32(page + 0x54u) : 0u;
    /* 6 waiting, 7 / 8 sliding: the page keeps the clicks during a slide
     * too (a click then must not end as A on the slot after it) */
    if (st >= 6u && st <= 8u) {
        s_save_at = GetTickCount64();
        s_save_slot = (LONG)MEM32(page + 0x48u);
    }
    sub_0010E8E0_pad();
}

static void page_b_and_wheel(XBOX_INPUT_STATE *n, ULONGLONG t, int over, WORD less, WORD more)
{
    static ULONGLONG b_until;
    int rdown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    if (over && s_redge)
        b_until = t + B_HOLD_MS;
    if (t < b_until || (over && rdown))
        n->Gamepad.bAnalogButtons[XBOX_BUTTON_B] = 255;
    if (s_wheel && s_held == 0) {
        LONG w = s_wheel;
        if (press(w > 0 ? less : more, t))
            { InterlockedExchangeAdd(&s_wheel, w > 0 ? -1 : 1); s_held_wheel = 1; }
    }
    n->Gamepad.sThumbLX = n->Gamepad.sThumbLY = 0;
    n->Gamepad.sThumbRX = n->Gamepad.sThumbRY = 0;
}

static int opt_pointer(XBOX_INPUT_STATE *n, ULONGLONG t, int over, int ledge, float gx, float gy)
{
    static float lx = -1.f, ly = -1.f;
    int row = -1;
    if (t - s_opt_at > 150u)
        return 0;
    mouse_a_off(n, t);
    if (over && gx >= 90.f && gx <= 550.f && gy >= OPT_Y0 && gy < OPT_Y0 + 5.f * OPT_DY)
        row = (int)((gy - OPT_Y0) / OPT_DY);
    if (row >= 0 && (gx != lx || gy != ly))
        InterlockedExchange(&s_opt_want, row);
    lx = gx;
    ly = gy;
    if (ledge && over) {
        const char *what = "nothing";
        float bar = OPT_Y0 + 20.5f + OPT_DY * (float)row;     /* 194 + 44 k */
        if (row >= 0 && gy >= bar - 4.f && gy <= bar + 29.f) {
            if (gx <= 132.f)
                what = press(XBOX_GAMEPAD_DPAD_LEFT, t) ? "left arrow" : "left arrow (busy)";
            else if (gx >= 508.f)
                what = press(XBOX_GAMEPAD_DPAD_RIGHT, t) ? "right arrow" : "right arrow (busy)";
            else
                what = "row";
        } else if (row >= 0)
            what = "row";
        fprintf(stderr, "[RATON] options click game %.0f,%.0f -> %s (row %d, marked %ld)\n",
                gx, gy, what, row, (long)s_opt_row);
        fflush(stderr);
    }
    page_b_and_wheel(n, t, over, XBOX_GAMEPAD_DPAD_UP, XBOX_GAMEPAD_DPAD_DOWN);
    return 1;
}

static int save_pointer(XBOX_INPUT_STATE *n, ULONGLONG t, int over, int ledge, float gx, float gy)
{
    static ULONGLONG a_until, step_at;
    static LONG step_from = -1;
    LONG cur = s_save_slot;
    if (t - s_save_at > 150u)
        return 0;
    mouse_a_off(n, t);
    if (ledge && over) {
        const char *what = "nothing";
        float d = (gx - 320.f) / 150.7f;
        LONG k = (LONG)floorf(d + 0.5f);
        if (gy >= 133.f && gy <= 333.f && fabsf(gx - 320.f - 150.7f * (float)k) <= 62.f
                && cur + k >= 0 && cur + k <= 4) {
            if (k == 0) {
                what = "A (this slot)";
                a_until = t + PRESS_MS + 15u;
            } else {
                what = "slide";
                InterlockedExchange(&s_save_steps, k);
            }
        }
        fprintf(stderr, "[RATON] save click game %.0f,%.0f -> %s (slot %ld, %+ld)\n",
                gx, gy, what, (long)cur, (long)k);
        fflush(stderr);
    }
    /* one slide at a time: the next once the slot changed (or 1 s) */
    if (s_save_steps && s_held == 0 && (step_from != cur || t - step_at > 1000u)) {
        LONG s = s_save_steps;
        if (press(s > 0 ? XBOX_GAMEPAD_DPAD_RIGHT : XBOX_GAMEPAD_DPAD_LEFT, t)) {
            InterlockedExchangeAdd(&s_save_steps, s > 0 ? -1 : 1);
            step_from = cur;
            step_at = t;
        }
    } else if (!s_save_steps)
        step_from = -1;
    if (t < a_until)
        n->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = 255;
    page_b_and_wheel(n, t, over, XBOX_GAMEPAD_DPAD_LEFT, XBOX_GAMEPAD_DPAD_RIGHT);
    return 1;
}

/* ---- the deck builder (Armar el mazo) ----------------------------------- */

/* 2026-11-09, the user: "se mueve para todos lados y es inusable"; video
 * 02-50-05: tabs and the buttons up top acted on the marked card; video
 * 03-01-16: after a tab the card "se selecciona sola", the deck side did not
 * work. The log of 03-01 showed why the card picked itself: the kit also
 * turns the left button into the pad's A, key 14 here, which in the list
 * opens the card's buttons. In this screen the pad's A and B from the mouse
 * are dropped; only the keys below go in.
 *
 * Two key handlers, by [0x66D1EA] (sub_000DBA20): 0 the chest, sub_000D9A10;
 * 1..2 the deck, sub_000DA730. Both read their key from 0x66DBB0 (0x10
 * none, put back after). sub_000D87D0 fills it from the pad: 4 up, 6 down,
 * 5 / 7 right / left, 14 A (0x2), 13 B (0x4), 12 (0x10), 15 (0x8), 8 / 9
 * (0x4000 / 0x2000), 10 (0x80), 11 (0x40).
 *
 * Chest ([0x66D1EC] is the part with the cursor):
 *   1 the list: [0x66DB76] row 0..5, [0x66DB78] first card shown,
 *     [0x66D1E8] cards, [0x66DB74] the tab frame (drawn at 70 + 72 k).
 *     14 opens the card's five round buttons (part 9), 13 part 11, 12 the
 *     info (part 4), 15 the "Cartas nuevas" menu (part 10).
 *   9 the card's buttons: [0x66D1F3] 0..4, 14 runs it (table 0xDA700), 13
 *     back to the list.
 *   10 the menu's six buttons: [0x66D1F6] 0..5, 14 runs it (table 0xDA714;
 *     2 = next deck, [0x66D1F0] + 1 mod 3), 13 back.
 *   0 the tab row: 14 applies the tab ([0x66D1FB], sub_000D97F0).
 *   8 / 9 send the card to the main / side deck (sub_000DEF20): never sent
 *   from here (until 03-01 the deck I / II / III clicks sent them).
 *   10: [0x66D1EA] = 1 - [0x66D1EA], chest <-> deck.
 * Deck (sub_000DA730), part 1 the grid: i = [0x66DB7A + 2 d] + 2 d (d the
 * deck [0x66D1F0], [0x66DB7A + 2 d] its tab 0 / 1): [0x66DB80 + 2 i] the
 * column 0..9, [0x66DB8C + 2 i] the row, [0x66DB98 + 2 i] the first row
 * shown; a step: [0x66C880] |= 0x288, tick 0x1000009A. Key 11 there flips
 * the tab (0xDB035).
 * Scrolling the chest: [0x66DB78] += n, [0x66DBC0] = n, [0x66DBBC] |=
 * 0x100000 (| 0x4000 going up), [0x66DBB8] = 0xB: the game's own glide.
 *
 * Places in game px (videos 02-50-05 and 03-01-16, picture x 63..1857, y
 * 0..1009): list rows y 118 + 50 r, 50 high, x 52..608; chest tabs x 71 +
 * 72 k .. +62, y 84..114; chest box x 58..146, y 36..70; deck box x
 * 169..240; deck I / II / III x 250..299 / 304..352 / 360..411, y 26..45;
 * "Cartas nuevas" / "Nada" x 456..601, y 36..80; the card's buttons y 317,
 * x 320 + 50.25 (k - 2); the menu's buttons y 238, x 320 + 50.4 (k - 2.5);
 * the deck grid x 72 + 40 c (20 each side), y 147 + 74.7 r (37); the deck's
 * tabs x 78..132 / 146..203, y 87..111. */
void sub_000D9A10_pad(void);
void sub_000DA730_pad(void);
static volatile ULONGLONG s_deck_at;
static volatile LONG s_deck_row = -1, s_deck_cell = -1, s_deck_key = -1, s_deck_wheel;
static volatile LONG s_deck_tab = -1, s_deck_pick = -1, s_deck_hover = -1, s_deck_hover_part;
static volatile LONG s_deck_deck = -1;          /* deck I/II/III asked for */
static volatile LONG s_deck_back;               /* then back to the deck side */
static int s_deck_tab_back;                     /* a tab went through part 0 */
static volatile ULONGLONG s_deck_key_at;
static volatile LONG s_deck_drag = -1;          /* scroll bar: the first row wanted */
static volatile uint32_t s_deck_scene;          /* the scene (edi in the handlers) */

#define DECK_TAB_ROW 100                        /* s_deck_hover_part: the deck's tab row */
#define DECK_ROW_Y0 118.f
#define DECK_ROW_H  50.f

static void deck_tick(uint32_t id)
{
    uint32_t sp = g_esp;
    PUSH32(g_esp, id);
    PUSH32(g_esp, 0x000DA5ADu);
    sub_0007E280();
    g_esp = sp;
}

/* The deck grid's index for the deck and tab shown. */
static uint32_t deck_grid_i(void)
{
    uint32_t d = MEM8(0x0066D1F0u) % 3u;
    return MEM16(0x0066DB7Au + 2u * d) + 2u * d;
}

/* A round-button menu's choice byte and size, by part (0 if none). Each
 * side has its own byte (2026-11-09, Ctrl+F9 / screenshots 03:11: on the
 * deck side the buttons did not follow the pointer): the deck's handler
 * steps [0x66D1F3 + side] (0xDAABE) and [0x66D1F6 + side], side being
 * [0x66D1EA]; its "Nada" menu has 7 buttons (0..6), the chest's 6. */
static uint32_t deck_menu(int part, int *count)
{
    uint32_t side = MEM16(0x0066D1EAu);
    if (side > 2u)
        side = 0;
    if (part == 9)  { *count = 5; return 0x0066D1F3u + side; }
    if (part == 10) { *count = side ? 7 : 6; return 0x0066D1F6u + side; }
    /* "Terminar construcción de Deck. ¿Estás seguro?": 0 Aceptar, 1
     * Cancelar (starts on 1), one byte for both sides; the game flips it
     * with [0x66C880] |= 0x40000 (0xD9B5C) */
    if (part == 11) { *count = 2; return 0x0066D1F2u; }
    /* 2026-11-19 (the user: "hay que hacer funcionar ese menú con el mouse ...
     * cuando pones ordenar"): the chest's sort bar, part 3 of sub_000D9A10:
     * right / left step [0x66D1F9] 0..7 (0xD9C81, wraps at 8; [0x66C880] |=
     * 0x280, [0x66D210] = 0), up / down set its order [0x66D1FD + it] (1 / 0),
     * A sorts by it (0xDA2A1: [0x66D1FA] = it, sub_000D97F0, state 0xC) */
    if (part == 3 && !side) { *count = 8; return 0x0066D1F9u; }
    /* 2026-11-19 (the user: the menu's 2nd button, "no me deja presionar
     * nada ahi con el mouse"): it goes to the chest's part 0, the tab row:
     * right / left step the frame [0x66DB74] 0..6 (0xD9B9F, |= 0x280), A
     * shows that tab (0xDA1D7: [0x66D1FB] = it, sub_000D97F0); the u16's low
     * byte (0..6) */
    if (part == 0 && !side) { *count = 7; return 0x0066DB74u; }
    *count = 0;
    return 0;
}

static void deck_keys(int side)
{
    int part = (int)MEM16(0x0066D1ECu), cnt;
    uint32_t sa = g_eax, sc = g_ecx, sd = g_edx, menu = deck_menu(part, &cnt);
    LONG key = s_deck_key, w = s_deck_wheel, r = s_deck_row, h = s_deck_hover;
    if (key >= 0 && s_deck_at - s_deck_key_at > 1500u)
        InterlockedExchange(&s_deck_key, key = -1);
    if (part == 1 && s_deck_drag >= 0) {
        /* the scroll bar held (2026-11-12): the first row the pointer asks
         * for. Chest: a few rows glide as the wheel does; farther, the top
         * row is set and the list fills again as after a tab (state 8's end,
         * 0x3DB47: [0x66DBC0] = [0x66DBC4] = 8, [0x66DBBC] |= 0x20000000,
         * state 0xC). Deck grid: the wheel's step, a row at a time. */
        static int tries;
        LONG asked = s_deck_drag, want = asked;
        if (!side) {
            int len = (int)MEM16(0x0066D1E8u), top = (int)MEM16(0x0066DB78u);
            int max = len - 6, d;
            if (want > max) want = max;
            if (want < 0) want = 0;
            d = (int)want - top;
            if (d == 0)
                InterlockedCompareExchange(&s_deck_drag, -1, asked);
            else if (d >= -6 && d <= 6) {
                int n = d < 0 ? -d : d;
                MEM16(0x0066DB78u) = (uint16_t)want;
                MEM32(0x0066DBC0u) = (uint32_t)n;
                MEM32(0x0066DBBCu) |= 0x100000u | (d < 0 ? 0x4000u : 0u);
                MEM32(0x0066DBB8u) = 0xBu;
                deck_tick(0x1000009Au);
            } else {
                MEM16(0x0066DB78u) = (uint16_t)want;
                MEM32(0x0066DBC0u) = 8u;
                MEM32(0x0066DBC4u) = 8u;
                MEM32(0x0066DBBCu) |= 0x20000000u;
                MEM32(0x0066DBB8u) = 0xCu;
                deck_tick(0x1000009Au);
                fprintf(stderr, "[RATON] deck: scroll bar, top %d -> %ld (refill)\n", top, (long)want);
                fflush(stderr);
            }
        } else {
            static int first_was = -1;
            uint32_t i = deck_grid_i();
            int first = (int)MEM16(0x0066DB98u + 2u * i);
            if (first == (int)want || (first == first_was && ++tries > 3)) {
                InterlockedCompareExchange(&s_deck_drag, -1, asked);
                tries = 0;
                first_was = -1;
            } else {
                if (first != first_was)
                    tries = 0;
                first_was = first;
                MEM16(0x0066DB8Cu + 2u * i) = (int)want > first ? 3u : 0u;
                MEM32(0x0066DBB0u) = (int)want > first ? 6u : 4u;
            }
        }
    } else if (!side && part == 1 && w) {
        /* the wheel: one glide of every notch so far */
        int len = (int)MEM16(0x0066D1E8u), top = (int)MEM16(0x0066DB78u);
        int n = w < 0 ? -w : (int)w, room = w < 0 ? len - 6 - top : top;
        InterlockedExchangeAdd(&s_deck_wheel, -w);
        if (room > 0) {
            if (n > room) n = room;
            MEM16(0x0066DB78u) = (uint16_t)(w < 0 ? top + n : top - n);
            MEM32(0x0066DBC0u) = (uint32_t)n;
            MEM32(0x0066DBBCu) |= 0x100000u | (w > 0 ? 0x4000u : 0u);
            MEM32(0x0066DBB8u) = 0xBu;
            deck_tick(0x1000009Au);
        }
    } else if (side && part == 1 && w) {
        /* the grid: the cursor to the edge row, then the game's own step */
        uint32_t i = deck_grid_i();
        InterlockedExchangeAdd(&s_deck_wheel, w > 0 ? -1 : 1);
        MEM16(0x0066DB8Cu + 2u * i) = w > 0 ? 0u : 3u;
        MEM32(0x0066DBB0u) = w > 0 ? 4u : 6u;
    } else if (!side && part == 3 && w) {
        /* the sort bar: the wheel is up / down there (the order of the one
         * marked: up 1, down 0, 0xD9B0F / 0xD9DA8) */
        InterlockedExchangeAdd(&s_deck_wheel, -w);
        MEM32(0x0066DBB0u) = w > 0 ? 4u : 6u;
    } else if (!side && part == 1 && s_deck_tab >= 0) {
        /* a tab: the frame there, then the tab row's own "apply" (14) */
        LONG k = InterlockedExchange(&s_deck_tab, -1);
        MEM16(0x0066DB74u) = (uint16_t)k;
        MEM16(0x0066D1ECu) = 0;
        MEM32(0x0066DBB0u) = 14u;
        s_deck_tab_back = 1;
        fprintf(stderr, "[RATON] deck: tab %ld (part 1 -> 0, key 14)\n", (long)k);
        fflush(stderr);
    } else if (side && part == 1 && s_deck_tab >= 0) {
        /* the deck's two tabs: key 11 flips them (0xDB035) */
        LONG k = InterlockedExchange(&s_deck_tab, -1);
        uint32_t d = MEM8(0x0066D1F0u) % 3u;
        if ((LONG)MEM16(0x0066DB7Au + 2u * d) != k)
            MEM32(0x0066DBB0u) = 11u;
    } else if (!side && s_deck_deck >= 0) {
        /* deck I / II / III: "Cartas nuevas" (15), its button 2 (14), until
         * the deck is that one */
        static int steps;
        LONG want = s_deck_deck;
        int cur = (int)MEM8(0x0066D1F0u);
        if (cur == want || ++steps > 8) {
            fprintf(stderr, "[RATON] deck: deck %ld %s (now %d)\n", (long)want,
                    cur == want ? "reached" : "not reached", cur);
            fflush(stderr);
            InterlockedExchange(&s_deck_deck, -1);
            steps = 0;
            if (part == 10)
                MEM32(0x0066DBB0u) = 13u;           /* close the menu */
        } else if (part == 1)
            MEM32(0x0066DBB0u) = 15u;
        else if (part == 10) {
            MEM8(0x0066D1F6u) = 2u;
            MEM32(0x0066DBB0u) = 14u;
        }
    } else if (!side && part == 1 && s_deck_back && s_deck_deck < 0) {
        /* the deck was changed from the deck side: back there (10) */
        InterlockedExchange(&s_deck_back, 0);
        MEM32(0x0066DBB0u) = 10u;
        fprintf(stderr, "[RATON] deck: back to the deck side\n");
        fflush(stderr);
    } else if (!side && part == 1 && r >= 0 && r <= 5
               && (int)MEM16(0x0066DB78u) + r < (int)MEM16(0x0066D1E8u)
               && (int)MEM16(0x0066DB76u) != r) {
        MEM16(0x0066DB76u) = (uint16_t)r;
        MEM32(0x0066C880u) |= 0x280u;
        deck_tick(0x1000009Au);
    } else if (side && part == 1 && s_deck_cell >= 0) {
        LONG c = InterlockedExchange(&s_deck_cell, -1);
        uint32_t i = deck_grid_i();
        if ((LONG)MEM16(0x0066DB80u + 2u * i) != (c & 0xFF)
                || (LONG)MEM16(0x0066DB8Cu + 2u * i) != (c >> 8)) {
            MEM16(0x0066DB80u + 2u * i) = (uint16_t)(c & 0xFF);
            MEM16(0x0066DB8Cu + 2u * i) = (uint16_t)(c >> 8);
            MEM32(0x0066C880u) |= 0x288u;
            deck_tick(0x1000009Au);
        }
    } else if (side && part == 0 && h >= 0 && s_deck_hover_part == DECK_TAB_ROW) {
        /* the deck's tab row (2026-11-12: the "Nada" menu's button 1, "Deck
         * Principal/Deck Auxiliar", 0xDB684, puts the cursor there; the
         * mouse did nothing in it). Its keys (sub_000DA730, part 0): 5 / 7
         * flip the tab shown ([0x66DB7A + 2 d] = 1 - it, 0xDA9B2), 14 keeps
         * it (state 9), 13 puts back the one it had ([0x66D1FC]) and goes
         * back to the menu (part 10). The tab under the pointer is shown. */
        uint32_t d = MEM8(0x0066D1F0u) % 3u;
        InterlockedExchange(&s_deck_hover, -1);
        if (h <= 1 && (LONG)MEM16(0x0066DB7Au + 2u * d) != h)
            MEM32(0x0066DBB0u) = 5u;
    } else if (menu && h >= 0 && s_deck_hover_part == part) {
        /* the round buttons: the one under the pointer is chosen (as
         * left / right do: [0x66C880] |= 0xB80, tick) */
        InterlockedExchange(&s_deck_hover, -1);
        if (h < cnt && (int)MEM8(menu) != h) {
            MEM8(menu) = (uint8_t)h;
            if (part == 3)
                MEM32(0x0066D210u) = 0u;
            MEM32(0x0066C880u) |= part == 11 ? 0x40000u : part == 3 || part == 0 ? 0x280u : 0xB80u;
            deck_tick(0x10000001u);
        }
    } else if (key >= 0) {
        LONG pick = s_deck_pick;
        /* a row click waits until that row is the marked one; a round
         * button until it is the chosen one */
        int ready = 1;
        if (!side && part == 1 && pick >= 0 && pick <= 5)
            ready = (int)MEM16(0x0066DB76u) == pick;
        else if (side && part == 0 && pick >= 200)
            ready = (LONG)MEM16(0x0066DB7Au + 2u * (MEM8(0x0066D1F0u) % 3u)) == pick - 200;
        else if (menu && pick >= 100)
            ready = (int)MEM8(menu) == pick - 100;
        if (ready) {
            MEM32(0x0066DBB0u) = (uint32_t)key;
            InterlockedExchange(&s_deck_key, -1);
            InterlockedExchange(&s_deck_pick, -1);
            fprintf(stderr, "[RATON] deck: key %ld (side %d, part %d, row %u, first %u of %u, buttons %u/%u)\n",
                    (long)key, side, part, MEM16(0x0066DB76u), MEM16(0x0066DB78u),
                    MEM16(0x0066D1E8u), MEM8(0x0066D1F3u), MEM8(0x0066D1F6u));
            fflush(stderr);
        }
    }
    g_eax = sa; g_ecx = sc; g_edx = sd;
}

/* Every key the handler gets and the part it leaves the cursor in (the
 * pad's and ours), for the next video. */
static void deck_run(int side, void (*pad)(void))
{
    static int part_was = -1;
    uint32_t key;
    s_deck_at = GetTickCount64();
    s_deck_scene = g_edi;           /* sub_0003D9D0's edi, untouched on the way here */
    if (s_mode == MODE_POINTER && MEM32(0x0066DBB0u) == 0x10u)
        deck_keys(side);
    key = MEM32(0x0066DBB0u);
    pad();
    /* a tab click went through the tab row (part 0): the cursor goes back
     * to the list, where it was */
    if (key == 14u && !side && MEM16(0x0066D1ECu) == 0u && s_deck_tab_back) {
        MEM16(0x0066D1ECu) = 1;
        s_deck_tab_back = 0;
    }
    if (key != 0x10u || (int)MEM16(0x0066D1ECu) != part_was) {
        fprintf(stderr, "[RATON] deck %s: key %u -> part %u (deck %u, side %u, state %u)\n",
                side ? "side" : "chest", key, MEM16(0x0066D1ECu), MEM8(0x0066D1F0u),
                MEM16(0x0066D1EAu), MEM32(0x0066DBB8u));
        fflush(stderr);
        part_was = (int)MEM16(0x0066D1ECu);
    }
}

/* The round buttons' light: the scene (sub_0003B9E0) calls sub_000ED100
 * (thiscall: button, animate) on [0x66C880] & 0x800 with [0x66D1F3 / F6 +
 * side]; it puts out the lit one ([this+8]) and lights the new one. Logged
 * in the deck builder (2026-11-09: "no se iluminan"), to see if it runs. */
void sub_000ED100(void)
{
    /* the Target List opening its buttons: the gear, not the magnifier */
    if (s_tl_fila && g_ecx == s_tl_fila && MEM32(g_esp + 4u) == 0u) {
        MEM32(g_esp + 4u) = 1u;
        fprintf(stderr, "[RATON] target list: buttons open on the gear\n");
        fflush(stderr);
    }
    if (GetTickCount64() - s_deck_at < 300u) {
        uint32_t self = g_ecx;
        fprintf(stderr, "[RATON] deck light: button %d animate %u (lit %d of %d, part %u)\n",
                (int)MEM32(g_esp + 4u), MEM32(g_esp + 8u) & 0xFFu,
                (int)MEM32(self + 8u), (int)MEM32(self + 0xACu), MEM16(0x0066D1ECu));
        fflush(stderr);
    }
    sub_000ED100_pad();
}

void sub_000D9A10(void) { deck_run(0, sub_000D9A10_pad); }
void sub_000DA730(void) { deck_run(1, sub_000DA730_pad); }

static void deck_send(LONG key, LONG pick, ULONGLONG t)
{
    s_deck_key_at = t;
    InterlockedExchange(&s_deck_pick, pick);
    InterlockedExchange(&s_deck_key, key);
}

/* The kit gives the left button to the game as A by itself (its default
 * binding), whatever raton.c decides: on the title screens a click off the
 * texts still went through (2026-11-10 log: "click off the options,
 * nothing", then state 12). This takes the mouse's A out while the button
 * is down and 300 ms after (the kit's A trails the button a little, as in
 * deck_pointer), so only the A raton.c sets itself after it reaches the
 * game. Space / Enter / a pad's A are not affected beyond that window. */
/* 2026-11-17 (mouse review): a quick click, down and up between two polls,
 * was never seen down here, and its kit A went through; the hook's press
 * time counts too now. Signed difference: the hook's clock can be a few ms
 * ahead of `t`. `b`: the kit's B (the right button) too. */
static void mouse_kit_off(XBOX_INPUT_STATE *n, ULONGLONG t, int a, int b)
{
    static ULONGLONG l_at, r_at;
    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
        l_at = t;
    if (s_lpress_at > l_at)
        l_at = s_lpress_at;
    if (GetAsyncKeyState(VK_RBUTTON) & 0x8000)
        r_at = t;
    if (s_rpress_at > r_at)
        r_at = s_rpress_at;
    if (a && l_at && (LONGLONG)(t - l_at) < 300) {
        n->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = 0;
        s_a_press = s_lpress;           /* this press is ours to the end */
    }
    if (b && r_at && (LONGLONG)(t - r_at) < 300)
        n->Gamepad.bAnalogButtons[XBOX_BUTTON_B] = 0;
}

static void mouse_a_off(XBOX_INPUT_STATE *n, ULONGLONG t)
{
    mouse_kit_off(n, t, 1, 0);
}

/* The deck builder's part of pointer mode: 1 while one of its handlers
 * runs. Left click: on a card its round buttons (A), on a round button that
 * button (A), on a tab that tab, up top the chest / deck / I II III / "Cartas
 * nuevas"; on the info, back; anywhere else nothing. Right click: back (B)
 * out of a menu or the info; nothing on the list. */
/* ---- scroll bars (2026-11-12, the user: "si hacés clic donde baja y sube a
 * la izquierda, que puedas subir o bajar como quieras") ------------------ */

/* A scroll bar object (thiscall sub_000E3480(rows, shown) sets it up,
 * sub_000E3510(first row) puts the thumb): +0x1C x and +0x2C the track's
 * top (floats, game px), +0x38 the track's length, +0x3C the thumb's, +0x40
 * the rows, +0x44 the rows shown; the thumb's top is at +0x2C + (length -
 * thumb) * first / (rows - shown). The deck builder's: [scene + 0x1264] the
 * chest's list (rows = cards, 6 shown; 0x3DCFE), [scene + 0x1340] the deck
 * grid (0x3DE53); the Biblioteca's [scene + 0x1260]. */
typedef struct { float x, y0, len, thumb; int rows, shown; } ScrollBar;

static int bar_read(uint32_t b, ScrollBar *o)
{
    if (!b)
        return 0;
    o->x = fbits(MEM32(b + 0x1Cu));
    o->y0 = fbits(MEM32(b + 0x2Cu));
    o->len = (float)(int)MEM32(b + 0x38u);
    o->thumb = (float)(int)MEM32(b + 0x3Cu);
    o->rows = (int)MEM32(b + 0x40u);
    o->shown = (int)MEM32(b + 0x44u);
    return on_screen(o->x, o->y0) && o->len >= 40.f && o->len <= 480.f && o->thumb >= 0.f
        && o->thumb <= o->len && o->shown > 0 && o->rows > o->shown && o->rows < 5000;
}

/* A press on the bar starts a drag; while the button is held the first row
 * is the one that puts the thumb's middle under the pointer. 1 while
 * dragging (the click is the bar's, nothing else takes it). The bar is a
 * few px wide: the press may land 10 px left of x to 18 px right of it. */
static int bar_drag(const ScrollBar *b, const char *who, int ledge, int over, float gx, float gy,
                    int *dragging, LONG *row)
{
    float span = b->len - b->thumb, f;
    int max = b->rows - b->shown;
    if (ledge && over && gx >= b->x - 10.f && gx <= b->x + 18.f
            && gy >= b->y0 - 10.f && gy <= b->y0 + b->len + 10.f) {
        *dragging = 1;
        fprintf(stderr, "[RATON] %s: scroll bar grabbed at %.0f,%.0f (bar x %.1f, y %.1f, length %.0f, thumb %.0f, rows %d, shown %d)\n",
                who, gx, gy, b->x, b->y0, b->len, b->thumb, b->rows, b->shown);
        fflush(stderr);
    }
    if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000))
        *dragging = 0;
    if (!*dragging)
        return 0;
    f = span > 1.f ? (gy - b->y0 - b->thumb / 2.f) / span : 0.f;
    if (f < 0.f) f = 0.f;
    if (f > 1.f) f = 1.f;
    *row = (LONG)floorf(f * (float)max + 0.5f);
    return 1;
}

static int deck_pointer(XBOX_INPUT_STATE *n, ULONGLONG t, int over, int ledge, float gx, float gy)
{
    static float lx = -1.f, ly = -1.f, rx, ry;
    static int replay;
    static ULONGLONG r_at;
    int part = (int)MEM16(0x0066D1ECu), side = MEM16(0x0066D1EAu) != 0, cnt, k;
    LONG row = -1, tab = -1, btn = -1, cell = -1;
    const char *what = "nothing";
    /* the opponent select on screen: the deck builder's clicks are over */
    if (t - s_deck_at > 300u || t - s_opp_at < 150u)
        return 0;
    /* the scroll bar left of the list / grid: held, it scrolls (deck_keys);
     * the click and the pointer are the bar's meanwhile. Without a bar the
     * game's numbers allow, the chest's measured one (2026-11-12 screenshots:
     * x 35..42, y 114..414). */
    {
        static int dragging;
        ScrollBar b;
        uint32_t sc = s_deck_scene;
        int ok = 0;
        LONG r;
        if (part == 1 && sc)
            ok = bar_read(MEM32(sc + (side ? 0x1340u : 0x1264u)), &b);
        if (!ok && part == 1 && !side && MEM16(0x0066D1E8u) > 6u) {
            b.x = 35.f; b.y0 = 114.f; b.len = 300.f; b.thumb = 0.f;
            b.rows = (int)MEM16(0x0066D1E8u); b.shown = 6;
            ok = 1;
        }
        if (part != 1 && s_deck_drag >= 0)
            InterlockedExchange(&s_deck_drag, -1);
        if (!ok)
            dragging = 0;
        else if (bar_drag(&b, side ? "deck grid" : "deck chest", ledge, over, gx, gy, &dragging, &r)) {
            if (r != s_deck_drag)
                InterlockedExchange(&s_deck_drag, r);
            ledge = 0;
            over = 0;
        }
    }
    /* a click that closed a menu, again on the cards (within 1 s) */
    if (replay && (part == 1 || t - r_at > 1000u)) {
        if (part == 1 && s_deck_key < 0 && !ledge) {
            ledge = 1;
            over = 1;
            gx = rx;
            gy = ry;
        }
        if (part == 1 || t - r_at > 1000u)
            replay = 0;
    }
    if (over && part == 11) {
        /* Aceptar / Cancelar: bars 450 x 30 centred on x 320 (as the duel's
         * boxes), y measured on the 2026-11-09 screenshot against the deck
         * grid: Aceptar 231..261, Cancelar 261..291 */
        if (gx >= 95.f && gx <= 545.f && gy >= 226.f && gy < 296.f)
            btn = gy < 261.f ? 0 : 1;
    } else if (over && part == 0 && !side) {
        /* the chest's tab row on its own (part 0): the tabs as in the list */
        if (gy >= 84.f && gy <= 114.f) {
            LONG kk = (LONG)floorf((gx - 71.f) / 72.f);
            if (kk >= 0 && kk <= 6 && gx - (71.f + 72.f * (float)kk) <= 62.f)
                btn = kk;
        }
    } else if (over && part == 3 && deck_menu(part, &cnt)) {
        /* the sort bar's 8 buttons, measured on the user's 2026-11-19
         * capture against the list rows (x 52..608, y 118 + 50 r): 64 wide,
         * 70.7 apart from x 75.6, y 407..440 */
        for (k = 0; k < cnt; k++) {
            float cx = 75.6f + 70.7f * (float)k;
            if (gx >= cx - 35.f && gx < cx + 35.f && gy >= 402.f && gy <= 446.f)
                btn = k;
        }
    } else if (over && deck_menu(part, &cnt)) {
        float y = part == 9 ? 317.f : 238.f, dx = part == 9 ? 50.25f : 50.4f;
        for (k = 0; k < cnt; k++) {
            float cx = 320.f + dx * ((float)k - (float)(cnt - 1) / 2.f);
            if ((gx - cx) * (gx - cx) + (gy - y) * (gy - y) <= 24.f * 24.f)
                btn = k;
        }
    }
    if (over && part == 1 && !side && gx >= 52.f && gx <= 608.f && gy >= DECK_ROW_Y0
            && gy < DECK_ROW_Y0 + 6.f * DECK_ROW_H)
        row = (LONG)((gy - DECK_ROW_Y0) / DECK_ROW_H);
    if (over && part == 1 && side) {
        int c = (int)floorf((gx - 72.f + 20.f) / 40.f);
        int r = (int)floorf((gy - 147.f + 37.35f) / 74.7f);
        if (c >= 0 && c < 10 && r >= 0 && r < 4)
            cell = (LONG)(c | (r << 8));
    }
    if (over && !side && gy >= 84.f && gy <= 114.f) {
        LONG kk = (LONG)floorf((gx - 71.f) / 72.f);
        if (kk >= 0 && kk <= 6 && gx - (71.f + 72.f * (float)kk) <= 62.f)
            tab = kk;
    }
    if (over && side && gy >= 84.f && gy <= 114.f) {
        if (gx >= 78.f && gx <= 132.f) tab = 0;
        else if (gx >= 146.f && gx <= 203.f) tab = 1;
    }
    if (gx != lx || gy != ly) {
        if (row >= 0)
            InterlockedExchange(&s_deck_row, row);
        if (cell >= 0)
            InterlockedExchange(&s_deck_cell, cell);
        if (btn >= 0) {
            s_deck_hover_part = part;
            InterlockedExchange(&s_deck_hover, btn);
        }
        if (tab >= 0 && side && part == 0) {
            s_deck_hover_part = DECK_TAB_ROW;
            InterlockedExchange(&s_deck_hover, tab);
        }
    }
    lx = gx;
    ly = gy;
    if (ledge && over) {
        if (btn >= 0) {
            what = "round button";
            s_deck_hover_part = part;
            InterlockedExchange(&s_deck_hover, btn);
            deck_send(14, 100 + btn, t);
        } else if (side && part == 0) {
            /* the deck's tab row: a tab is shown and kept (14); anywhere
             * else back to the menu (13), as B */
            if (tab >= 0) {
                what = "deck tab row: that tab";
                s_deck_hover_part = DECK_TAB_ROW;
                InterlockedExchange(&s_deck_hover, tab);
                deck_send(14, 200 + tab, t);
            } else {
                what = "deck tab row: back";
                deck_send(13, -1, t);
            }
        } else if (part == 4) {
            what = "card info: back";
            deck_send(13, -1, t);
        } else if (part != 1 && (part != 0 || !side)) {
            /* off the round buttons: the menu closes (B), and the click is
             * done again once the cursor is back on the cards, so a tab or
             * a card there takes it at once */
            what = "off the buttons: close, then again";
            deck_send(13, -1, t);
            replay = 1;
            rx = gx;
            ry = gy;
            r_at = t;
        } else if (tab >= 0) {
            what = "tab";
            InterlockedExchange(&s_deck_tab, tab);
        } else if (row >= 0) {
            what = "card row";
            InterlockedExchange(&s_deck_row, row);
            deck_send(14, row, t);
        } else if (cell >= 0) {
            what = "deck card";
            InterlockedExchange(&s_deck_cell, cell);
            deck_send(14, -1, t);
        } else if (gy >= 26.f && gy <= 45.f && gx >= 250.f && gx <= 411.f) {
            LONG d = gx < 302.f ? 0 : gx < 356.f ? 1 : 2;
            /* the deck side's handler never changes [0x66D1F0] (only the
             * chest's menu, 0xDA484): over to the chest (10), the deck
             * there, and back (s_deck_back) */
            what = side ? "deck I/II/III (via the chest)" : "deck I/II/III";
            if ((int)MEM8(0x0066D1F0u) == d)
                what = "deck I/II/III: already that one";
            else {
                InterlockedExchange(&s_deck_deck, d);
                if (side) {
                    s_deck_back = 1;
                    deck_send(10, -1, t);
                }
            }
        } else if (gy >= 36.f && gy <= 70.f && gx >= 58.f && gx <= 146.f) {
            what = "chest box";
            if (side)
                deck_send(10, -1, t);
        } else if (gy >= 36.f && gy <= 70.f && gx >= 169.f && gx <= 240.f) {
            what = "deck box";
            if (!side)
                deck_send(10, -1, t);
        } else if (gy >= 36.f && gy <= 80.f && gx >= 456.f && gx <= 601.f) {
            what = side ? "Nada" : "Cartas nuevas";
            deck_send(15, -1, t);
        }
        fprintf(stderr, "[RATON] deck click game %.0f,%.0f -> %s (part %d, side %d, row %ld, cell %lx, tab %ld, button %ld)\n",
                gx, gy, what, part, side, (long)row, (long)cell, (long)tab, (long)btn);
        fflush(stderr);
    }
    {
        /* back everywhere: in the cards (part 1) it is the game's own B,
         * "Terminar construcción de Deck" (part 11); there, Cancelar */
        /* 2026-11-19 (the user: "con el segundo click vayas hacia atras"):
         * the chest's tab row on its own too (part 0: B goes back to the
         * menu, 0xDA07E, and there B again to the list); not while a tab
         * clicked in the list passes through it */
        if (over && s_redge && (part != 0 || side || !s_deck_tab_back)) {
            deck_send(13, -1, t);
            fprintf(stderr, "[RATON] deck right click -> back (part %d)\n", part);
            fflush(stderr);
        }
    }
    if (s_wheel) {
        LONG w = InterlockedExchange(&s_wheel, 0);
        InterlockedExchangeAdd(&s_deck_wheel, w);
    }
    /* the mouse's own pad buttons (the kit: left A, right B) and motion
     * (sticks) stay out; Space / Esc (A / B) still work. The kit's button
     * comes from the window's messages, a little behind the button itself:
     * a quick click's A was still up after GetAsyncKeyState saw it released
     * (03:20 log: every click on nothing followed by key 14, the card's
     * buttons opening), so A / B stay out 300 ms after the button. */
    mouse_kit_off(n, t, 1, 1);
    n->Gamepad.sThumbLX = n->Gamepad.sThumbLY = 0;
    n->Gamepad.sThumbRX = n->Gamepad.sThumbRY = 0;
    return 1;
}


/* Which of pointer_mode's screens has the mouse now (the same tests and
 * order as there): 0 the generic one. */
static int screen_now(ULONGLONG t)
{
    if (t - s_deck_at <= 300u && t - s_opp_at >= 150u) return 1;
    if (t - s_lib_at < 300u) return 2;
    if (t - s_start_at < 300u) return 3;
    if (t - s_title_at < 300u) return 4;
    if (t - s_name_at < 300u) return 5;
    if (t - s_ask_at <= 150u) return 6;
    if (t - s_sp_at <= 150u) return 7;
    if (t - s_opt_at <= 150u) return 8;
    if (t - s_save_at <= 150u) return 9;
    if (t - s_info_at <= 150u) return 10;
    if (t - s_cmd_at <= 150u) return 11;
    if (t - s_wh_at <= 150u && s_wh_obj && s_wh_n >= 2) return 12;
    if (t - s_tri_at <= 150u && s_tri_obj) return 13;
    if (t - s_side_at <= 150u) return 14;
    if (t - s_tl_at <= 150u) return 15;
    if (t - s_list_at <= 150u) return 16;
    if (t - s_field_at <= 150u) return 17;
    return 0;
}

/* 2026-11-17 03:5x (the user: "si moves y apretas el segundo click no te lo
 * hace"; log: "right press began on screen 17, now 6 / 6, now 17" again and
 * again): the duel's boxes, buttons, lists and info come and go over the
 * field, which keeps running under them, so the screen flips between them
 * frame to frame and every right click was "from another screen". They all
 * read the keys from the same duel word ([0x6589E6], sub_0007D880): a button
 * held from one to another is no new press there. One family for them. */
static int screen_familia(int s)
{
    switch (s) {
    case 6: case 7: case 10: case 11: case 15: case 16: case 17:
        return 17;
    }
    return s;
}

static void pointer_mode(XBOX_INPUT_STATE *n, ULONGLONG t)
{
    static int waiting, from, key_sent, probes;
    static LONG seq_before, seen_seq;
    static ULONGLONG answer_by, changed_at, a_until, click_at;
    static int click_pending;
    static int idle;                    /* nothing more to press: there */
    float gx, gy;
    int over = pointer_pos(&gx, &gy);
    /* The left button's press, seen once per poll for everyone: the field
     * and the generic click below each kept their own "was down", so one
     * press the field had taken was taken again by the generic click when
     * the field paused next (the buttons opening), and its A came up to
     * CLICK_WAIT_MS later on whatever was then on screen: the ghost
     * clicks (19:10 log: field click and click game on one press). */
    static int lprev;
    int ldown = LDOWN();
    int ledge = ldown && !lprev;
    /* a press whose kit A a screen took out: out on every screen (s_a_press) */
    if (s_a_press == s_lpress)
        mouse_kit_off(n, t, 1, 0);
    lprev = ldown;
    /* the right button the same way (s_redge): a press the hook counted
     * since the last poll (a quick one, down and up in between) or one
     * seen down now that was up before */
    {
        static LONG rseen;
        static int rprev;
        int rd = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        LONG rp = s_rpress;
        /* as lbutton: a press counted but not down now is "down" for this
         * poll, so the next poll that sees it down is no second edge (the
         * hook sees a press before GetAsyncKeyState does) */
        if (rp != rseen && !rd)
            rd = 1;
        s_redge = rd && !rprev;
        rseen = rp;
        rprev = rd;
    }
    /* 2026-11-17 (the user: "revisa en todos los modos de 60, 90, 120 y elimina
     * todos los click fantasma"): a press acts on the screen it began on,
     * never on the next one. The game takes a key when it goes down, and
     * every screen keeps its own "was down" (the menus' reader and the
     * duel's are not the same): a button still held when the screen changes
     * was a new press there. The faster the frame rate, the sooner the next
     * screen comes, inside the same press. So the kit's A / B of a press
     * begun on another screen stay out until it is let go (+300 ms, the
     * kit's button trails the real one). */
    {
        static int a_scr = -1, b_scr = -1, said_a, said_b;
        static ULONGLONG a_seen, b_seen;
        int scr = screen_familia(screen_now(t));
        /* a press begun on the generic screen keeps the kit's A / B: that
         * is how it clicks, and a screen with its own test runs only some
         * frames under a balloon (01:40 log: "began on screen 0, now 14",
         * the click that should have skipped a voice line lost) */
        if (ledge) { a_scr = scr ? scr : -1; said_a = 0; }
        if (s_redge) { b_scr = scr ? scr : -1; said_b = 0; }
        if (ldown) a_seen = t;
        if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) b_seen = t;
        if (a_scr >= 0 && scr != a_scr && (LONGLONG)(t - a_seen) < 300) {
            n->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = 0;
            if (!said_a) {
                said_a = 1;
                fprintf(stderr, "[RATON] left press began on screen %d, now %d: its A stays out\n", a_scr, scr);
                fflush(stderr);
            }
        }
        if (b_scr >= 0 && scr != b_scr && (LONGLONG)(t - b_seen) < 300) {
            n->Gamepad.bAnalogButtons[XBOX_BUTTON_B] = 0;
            if (!said_b) {
                said_b = 1;
                fprintf(stderr, "[RATON] right press began on screen %d, now %d: its B stays out\n", b_scr, scr);
                fflush(stderr);
            }
        }
    }
    /* the press that brought the window back: the kit's own A of the left
     * button stays out too, while it is down and 300 ms after */
    if (s_lfocus || t - s_lfocus_end < 300u)
        n->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = 0;

    /* 2026-11-17 (the user: the voice went on however fast the clicks; one
     * click filled the balloon, two went on and the voice faded): while a
     * voice line's balloon is up a left click anywhere is A, for it, and no
     * screen under it takes the click (voces.c counts the A presses) */
    {
        extern int ygo_voz_globo_arriba(void);
        if (over && ledge && ygo_voz_globo_arriba()) {
            s_voz_until = t + PRESS_MS + 15u;
            fprintf(stderr, "[RATON] click on a voice line's balloon -> A\n");
            fflush(stderr);
            ledge = 0;
        }
    }

    /* The deck builder: rows, tabs, wheel; no d-pad walk (2026-11-09). */
    if (deck_pointer(n, t, over, ledge, gx, gy)) {
        waiting = 0;
        click_pending = 0;
        return;
    }

    /* The Biblioteca's grid (2026-11-08, video 20-40-58): the pointer over a
     * card puts the game's cursor straight on it (sub_0010F790 wrapper), no
     * walk card by card. Only inside a card's outline: measured on the
     * video's frame (picture 1794x1009 from x 63), cards 33.5 x 48.5 game
     * px, centres x 94.6 + 40 col (10 columns), y 120.4 + 57.1 row (5 rows
     * shown); in the gaps nothing changes. A click there: A once the
     * cursor is on that card. Right click B, the wheel scrolls (up/down). */
    if (t - s_lib_at < 300u) {
        static ULONGLONG la_until;
        static LONG click_want = -1;
        LONG hit = -1;
        /* 2026-11-16: the search (buscador.c) first: on its button or panel
         * the grid takes no hover, click or wheel; while typing (or a key of
         * its own) the game sees no button or key */
        static LONG busc_r = -1;        /* a right press the search took */
        {
            int br = ygo_buscador_raton(over, gx, gy, ledge, s_wheel);
            /* 2026-11-17: the right press that closed the search panel never
             * reaches the game (it was B once the panel had closed, still
             * held or the kit's trailing B: the exit box came up) */
            if (br == 2 && (s_redge || (GetAsyncKeyState(VK_RBUTTON) & 0x8000)))
                busc_r = s_rpress;
            if (br >= 1) {
                InterlockedExchange(&s_wheel, 0);
                if (br == 2)
                    memset(&n->Gamepad, 0, sizeof n->Gamepad);
                else {
                    n->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = 0;
                    n->Gamepad.bAnalogButtons[XBOX_BUTTON_B] = 0;
                    n->Gamepad.sThumbLX = n->Gamepad.sThumbLY = 0;
                    n->Gamepad.sThumbRX = n->Gamepad.sThumbRY = 0;
                }
                InterlockedExchange(&s_lib_want, -1);
                click_want = -1;
                waiting = 0;
                click_pending = 0;
                return;
            }
        }
        /* 2026-11-17 (mouse review): the kit's A of the left button stays
         * out of the whole Biblioteca. A card opens from the wrapper (the
         * log: "0 ms after the click") and the boxes take the A set below;
         * the kit's A opened the marked card on a click between the cards
         * or on the right side, and answered the exit box's marked line on
         * a click off it (Aceptar: out of the Biblioteca). */
        mouse_a_off(n, t);
        if (busc_r == s_rpress) {
            /* that press's B out, ours and the kit's, until let go (+300 ms) */
            mouse_kit_off(n, t, 0, 1);
            if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) {
                InterlockedExchange(&s_wheel, 0);
                waiting = 0;
                click_pending = 0;
                return;
            }
        }
        /* 2026-11-08 22:25: the exit box (Aceptar / Cancelar) is the key
         * handler's mode 4 ([0x66E684]); its choice is [0x66E688] (0
         * Aceptar, 1 Cancelar; up/down flip it at 0x10F839 / 0x10F9CC).
         * The grid's pointer kept looking for cards behind the box, and a
         * click that hit the cursor's own card pressed A on whatever was
         * marked: "a veces sí, a veces no". The line under the pointer is
         * marked (the wrapper, as the d-pad does); a click marks it and
         * presses A once it is marked. Rows (22:30, video 22-15-51, the
         * whole picture 1794 x 1009 from x 63: scale 2.803 across, 2.102
         * down; the 22:25 guess from a cropped screenshot took one scale
         * for both and sat ~24 px too high): the green bar of Aceptar
         * y 233..262, Cancelar 262..292, x 95..545. */
        if (MEM16(0x0066E684u) == 4u) {
            static float dx = -1.f, dy = -1.f;
            static LONG dlg_click = -1;
            LONG item = -1;
            if (over && gx >= 92.f && gx <= 548.f) {
                if (gy >= 231.f && gy < 262.f)
                    item = 0;
                else if (gy >= 262.f && gy <= 294.f)
                    item = 1;
            }
            if (item >= 0 && (gx != dx || gy != dy))
                InterlockedExchange(&s_dlg_want, item);
            dx = gx;
            dy = gy;
            if (ledge && item >= 0) {
                InterlockedExchange(&s_dlg_want, item);
                dlg_click = item;
                fprintf(stderr, "[RATON] library box: click %s\n", item ? "Cancelar" : "Aceptar");
                fflush(stderr);
            }
            if (dlg_click >= 0 && MEM8(0x0066E688u) == (uint32_t)dlg_click) {
                dlg_click = -1;
                la_until = t + PRESS_MS + 15u;
            }
            if (t < la_until)
                n->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = 255;
            if (over && (GetAsyncKeyState(VK_RBUTTON) & 0x8000))
                n->Gamepad.bAnalogButtons[XBOX_BUTTON_B] = 255;
            InterlockedExchange(&s_wheel, 0);
            InterlockedExchange(&s_lib_want, -1);
            click_want = -1;
            waiting = 0;
            click_pending = 0;
            return;
        }
        /* 2026-11-09 (screenshot: "quedó desfasado, las opciones no se pueden
         * elegir"): a card opened from the grid is mode 1 (opening) then 2,
         * two round buttons under it, the choice [0x66E689] (0 the
         * magnifier "Información de la carta", 1 the camera; A at 0x10FD67).
         * The grid's pointer kept looking for cards behind the open card,
         * and a click on one pressed A on whatever button was marked.
         * Buttons measured on the screenshot (grid cards and the header's
         * centre as the scale: 2.81 across, 2.14 down): circles of ~21 game
         * px at (292.6, 316) and (343.1, 316). Any mode but 0 and 4 leaves
         * the grid alone; mode 1 drops clicks, mode 3 takes one as a plain A. */
        if (MEM16(0x0066E684u) != 0u) {
            static float bx = -1.f, by = -1.f;
            static LONG btn_click = -1;
            int mode2 = MEM16(0x0066E684u) == 2u;
            LONG item = -1;
            if (over && mode2) {
                float d0 = (gx - 292.6f) * (gx - 292.6f) + (gy - 316.f) * (gy - 316.f);
                float d1 = (gx - 343.1f) * (gx - 343.1f) + (gy - 316.f) * (gy - 316.f);
                if (d0 <= 24.f * 24.f)
                    item = 0;
                else if (d1 <= 24.f * 24.f)
                    item = 1;
            }
            if (item >= 0 && (gx != bx || gy != by))
                InterlockedExchange(&s_btn_want, item);
            bx = gx;
            by = gy;
            if (ledge && over) {
                if (MEM16(0x0066E684u) == 1u) {
                    /* opening / closing: a click here would land on the
                     * next state, so it is dropped */
                } else if (!mode2) {
                    la_until = t + PRESS_MS + 15u;
                } else if (item >= 0) {
                    InterlockedExchange(&s_btn_want, item);
                    btn_click = item;
                    fprintf(stderr, "[RATON] library card: click button %ld at %.1f,%.1f\n",
                            (long)item, gx, gy);
                    fflush(stderr);
                } else {
                    fprintf(stderr, "[RATON] library card: click off the buttons at %.1f,%.1f (mode %u)\n",
                            gx, gy, MEM16(0x0066E684u));
                    fflush(stderr);
                }
            }
            if (btn_click >= 0 && mode2 && MEM8(0x0066E689u) == (uint32_t)btn_click) {
                btn_click = -1;
                la_until = t + PRESS_MS + 15u;
            }
            if (!mode2 && MEM16(0x0066E684u) != 1u)
                btn_click = -1;
            if (t < la_until)
                n->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = 255;
            if (over && (GetAsyncKeyState(VK_RBUTTON) & 0x8000))
                n->Gamepad.bAnalogButtons[XBOX_BUTTON_B] = 255;
            InterlockedExchange(&s_wheel, 0);
            InterlockedExchange(&s_lib_want, -1);
            click_want = -1;
            waiting = 0;
            click_pending = 0;
            return;
        }
        /* the scroll bar left of the grid, held: the sub_00038220 wrapper
         * puts its row on top at once (2026-11-12) */
        {
            static int dragging;
            static ULONGLONG bar_free_at;
            ScrollBar b;
            uint32_t sc = s_lib_scene;
            LONG r;
            /* the kit's A trails the button: out 300 ms after a drag */
            if (t - bar_free_at < 300u)
                n->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = 0;
            if (!sc || !bar_read(MEM32(sc + 0x1260u), &b))
                dragging = 0;
            else if (bar_drag(&b, "library", ledge, over, gx, gy, &dragging, &r)) {
                if (r != s_lib_drag && (LONG)MEM16(0x0066E690u) != r)
                    InterlockedExchange(&s_lib_drag, r);
                /* the kit's A (the left button) and the mouse's motion on
                 * the stick (the grid's own scroll) stay out */
                bar_free_at = t;
                n->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = 0;
                n->Gamepad.sThumbLX = n->Gamepad.sThumbLY = 0;
                n->Gamepad.sThumbRX = n->Gamepad.sThumbRY = 0;
                waiting = 0;
                click_pending = 0;
                return;
            }
        }
        if (over) {
            /* exact, from the game's own card quads (Ctrl+F9 21:26): 35x51
             * at x 76.5 + 40 col, y 94.5 + 57 row */
            int c = (int)floorf((gx - 94.f + 20.f) / 40.f);
            int r = (int)floorf((gy - 120.f + 28.5f) / 57.f);
            if (c >= 0 && c < 10 && r >= 0 && r < 5
                    && fabsf(gx - (94.f + 40.f * (float)c)) <= 17.5f
                    && fabsf(gy - (120.f + 57.f * (float)r)) <= 25.5f)
                hit = (LONG)(c | (r << 8));
        }
        /* 21:20: only when the pointer moves (or the card under it
         * changes): resent every frame, it fought the game putting the
         * cursor back after a scroll, and the tick played over and over */
        {
            static float lx = -1.f, ly = -1.f;
            static LONG lhit = -2;
            /* not while the wheel turns (21:25 log: the hand nudging the
             * mouse put the cursor back to its row after each notch) */
            if (hit >= 0 && (gx != lx || gy != ly || hit != lhit)
                    && t - s_lib_wheel_at > 150u)
                InterlockedExchange(&s_lib_want, hit);
            /* 21:55 (video 21-48-27): after the wheel stops, the card under
             * a still pointer did not show until the mouse moved: the grid
             * changed under the same cursor place and the game never took
             * it as a cursor move. Once the wheel has been quiet 150 ms,
             * the place under the pointer (or the cursor's own) is chosen
             * again, as a move (s_lib_force). */
            if (s_lib_wheel_at && t - s_lib_wheel_at > 150u) {
                s_lib_wheel_at = 0;
                if (hit >= 0)
                    InterlockedExchange(&s_lib_want, hit);
                else
                    InterlockedExchange(&s_lib_want, (LONG)(MEM16(0x0066E68Cu) | (MEM16(0x0066E68Eu) << 8)));
                InterlockedExchange(&s_lib_force, 1);
            }
            lx = gx;
            ly = gy;
            lhit = hit;
        }
        if (ledge && hit >= 0) {
            /* 21:40: a click while the wheel turns takes its card at once
             * (the hover pause after the wheel held it ~0.5 s) */
            s_lib_wheel_at = 0;
            InterlockedExchange(&s_lib_scroll, 0);
            InterlockedExchange(&s_lib_want, hit);
            /* 2026-11-09 (video 01-59-06): the card opens from the wrapper,
             * the A key put in the game's own key slot (s_lib_open); no pad
             * press, which the scene dropped while it loaded the new card's
             * preview (clicks while the mouse moved, on a card the cursor
             * had only just reached, did nothing) */
            s_lib_open_at = t;
            InterlockedExchange(&s_lib_open, hit);
            click_want = -1;
            fprintf(stderr, "[RATON] library: click card col %ld row %ld\n",
                    (long)(hit & 0xFF), (long)(hit >> 8));
            fflush(stderr);
        }
        if (click_want >= 0 && MEM16(0x0066E68Cu) == (uint32_t)(click_want & 0xFF)
                && MEM16(0x0066E68Eu) == (uint32_t)(click_want >> 8)) {
            click_want = -1;
            la_until = t + PRESS_MS + 15u;
        }
        if (t < la_until)
            n->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = 255;
        if (over && (GetAsyncKeyState(VK_RBUTTON) & 0x8000))
            n->Gamepad.bAnalogButtons[XBOX_BUTTON_B] = 255;
        /* 2026-11-09 ("no me da el clic si estoy moviendo el mouse, tengo
         * que dejarlo quieto encima de la carta"): the kit puts the mouse's
         * motion on the left stick (keyboard.c, mouse look), and the key
         * handler's first test in the grid (0x10F7AE: sub_00028F50(0) = pad
         * 0 stick X, against +-0.3 at 0x427C18 / 0x4289A0) takes a stick
         * past 0.3 as the stick's own scroll: [0x66E694] = 9 and out, the A
         * gone. The pointer already places the cursor, so in the grid the
         * stick goes to zero while the mouse moves (and 200 ms after) or a
         * click is under way; a pad's stick with the mouse still works. */
        {
            static float mvx = -1.f, mvy = -1.f;
            static ULONGLONG mv_at;
            if (gx != mvx || gy != mvy) {
                mvx = gx;
                mvy = gy;
                mv_at = t;
            }
            if (t - mv_at < 200u || click_want >= 0 || t < la_until) {
                n->Gamepad.sThumbLX = n->Gamepad.sThumbLY = 0;
                n->Gamepad.sThumbRX = n->Gamepad.sThumbRY = 0;
            }
        }
        /* the wheel (21:30, "like a PC game"): a row a notch, at once.
         * The game's own scroll plays a ~120 ms transition per row (state
         * 7, sub_000353D0); the scroll bar instead sets the top row and
         * redraws at once (state 8, 0x383B3: sub_000381E0). The wheel does
         * the same (the sub_00038220 wrapper), so nothing queues. */
        if (s_wheel) {
            LONG w = InterlockedExchange(&s_wheel, 0);
            InterlockedExchangeAdd(&s_lib_scroll, -w);
            s_lib_wheel_at = t;
        }
        waiting = 0;
        click_pending = 0;
        return;
    }
    /* The title screen, "Presiona START" (sub_001125C0 states 3, 6, 10): a
     * click on the text is START (A); anywhere else a click does
     * nothing, so the screen can be clicked freely (the click ring shows)
     * without leaving it (2026-11-10, the user). The text, logged as title
     * pieces +14 / +18: 278x27 at 183,371, lit 330x29 at 155,365. */
    if (t - s_start_at < 300u) {
        static ULONGLONG st_until;
        mouse_a_off(n, t);
        if (ledge && over) {
            int on = gx >= 152.f && gx <= 488.f && gy >= 361.f && gy <= 398.f;
            if (on)
                st_until = t + PRESS_MS + 15u;
            fprintf(stderr, "[RATON] press start: click (%.0f,%.0f)%s\n", gx, gy,
                    on ? " on the text" : ", nothing");
            fflush(stderr);
        }
        if (t < st_until)
            n->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = 255;
        waiting = 0;
        click_pending = 0;
        return;
    }
    /* The title menu (Nueva partida / Cargar partida): the option under the
     * pointer is chosen, a click on it confirms (A); anywhere else the
     * pointer and the click do nothing (2026-11-10, the user: a click
     * anywhere in the bottom half picked one). Each option is its arrow and
     * its text: the arrow (titl_07_00, 32x24) is drawn at x 85 or 340, y
     * 371..378 (sub_00111A10: 0x42AA0000 / 0x43AA0000, 371 + 0..7 bounce),
     * the texts after it are titl_06 "Nueva partida" 145x33 (160x36 lit)
     * and titl_05 "Cargar partida" 140x33 (155x36), from the title atlas
     * (euro/sp/title/2d/texdb/texdb000.txt). */
    if (t - s_title_at < 300u) {
        static ULONGLONG ta_until;
        LONG opt = -1;
        mouse_a_off(n, t);
        /* Where the pieces really are (the game's own, logged 2026-11-10
         * as "title piece": +04 x, +08 y of each): arrow 85,371 (32x24);
         * Nueva 120,371 (145x33), lit 100,366 (160x36); Cargar 375,371
         * (140x33), lit 380,366 (155x36). Each box is the arrow at 85 / 340
         * and the larger text, a few px of margin. */
        /* 2026-11-17 (the user: "responde un poco lento"; log: the menu ran
         * 120 updates/s, clicks took 0-16 ms, but 2 of 5 clicks fell off the
         * boxes, (296,374) between the options and (482,412) just under
         * Cargar, and did nothing): the row's band, split at the middle */
        if (over && gy >= 356.f && gy <= 414.f && gx >= 70.f && gx <= 560.f)
            opt = gx < 305.f ? 0 : 1;
        if (opt >= 0)
            InterlockedExchange(&s_title_want, opt);
        if (ledge && opt >= 0) {
            ta_until = t + PRESS_MS + 15u;
            fprintf(stderr, "[RATON] title: click %s (%.0f,%.0f) %llu ms (pressed %lld ms before)\n",
                    opt == 0 ? "Nueva partida" : "Cargar partida", gx, gy, (unsigned long long)t,
                    (long long)(t - s_lpress_at));
            fflush(stderr);
        } else if (ledge && over) {
            fprintf(stderr, "[RATON] title: click off the options (%.0f,%.0f), nothing\n", gx, gy);
            fflush(stderr);
        }
        if (t < ta_until)
            n->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = 255;
        waiting = 0;
        click_pending = 0;
        return;
    }
    /* The name screen: the pointer moves freely and changes nothing (it
     * walked the on-screen keyboard's cursor around under the typing); a
     * left click is the end button (as Enter). */
    if (t - s_name_at < 300u) {
        /* 2026-11-17 (mouse review): the kit's A of the left button stays
         * out: a click off the buttons typed the on-screen keyboard's
         * marked letter with it, and one on a button could press it twice
         * (the wrapper presses it itself) */
        mouse_a_off(n, t);
        /* The three buttons, from the game's own layout (sub_00110190: the
         * green bar of button b at x 127 + 150 b, y 355; icons from about
         * x 97 + 150 b): button b is x 90 + 150 b .. 240 + 150 b, y 345..420,
         * icon included (2026-11-08 22:10: the icons were outside before).
         * Above y 340 is the letters. */
        /* 2026-11-08 22:30: only the part that lights green counts (the
         * bar from x 127 + 150 b, about 115 wide; the icons left of it do
         * not), and the pointer acts on nothing else: off the buttons it
         * changes nothing (no going back to the letters). Every pointer
         * move over a button asks for it (not only a change of button), so
         * after typing the bar comes back at once. */
        {
            static float lx = -1.f, ly = -1.f;
            LONG hv = -2;
            if (over && gy >= 345.f && gy <= 400.f && gx >= 122.f && gx < 542.f) {
                LONG bb = (LONG)((gx - 122.f) / 150.f);
                if (gx - 122.f - 150.f * (float)bb < 125.f)
                    hv = bb;
            }
            if (hv >= 0 && (gx != lx || gy != ly || hv != s_name_hover)) {
                s_name_hover = hv;
                InterlockedIncrement(&s_name_hover_seq);
            }
            lx = gx;
            ly = gy;
            if (ledge && hv >= 0) {
                InterlockedExchange(&s_name_btn, hv);
                fprintf(stderr, "[RATON] name: click button %ld\n", (long)hv);
                fflush(stderr);
            }
        }
        waiting = 0;
        click_pending = 0;
        return;
    }
    glove_poll();
    boost_tick(t);
    /* 2026-11-19 14:2x (the user: "el Boton Auto solo podes apretarlo en tu
     * Turno"): the AUTO plate was read only by field_pointer, which runs
     * while the player has the field; it is read here, before any screen of
     * the duel, in both turns (auto.c checks it is on screen) */
    {
        int ygo_auto_raton(float x, float y);
        if (over && ledge && ygo_auto_raton(gx, gy)) {
            mouse_a_off(n, t);
            waiting = 0;
            click_pending = 0;
            return;
        }
    }
    /* The duel's boxes of options (the phases after B): the line under the
     * pointer is marked, a click answers it. */
    if (ask_pointer(n, t, over, ledge, gx, gy)) {
        waiting = 0;
        click_pending = 0;
        return;
    }
    /* Tipo / Atributo / Nivel: one line shown, arrows above and below. */
    if (spin_pointer(n, t, over, ledge, gx, gy)) {
        waiting = 0;
        click_pending = 0;
        return;
    }
    /* the coin toss: arrows flip, the coin answers */
    if (coin_pointer(n, t, over, ledge, gx, gy)) {
        waiting = 0;
        click_pending = 0;
        return;
    }
    /* The main menu's Opciones and Guardar pages. */
    if (opt_pointer(n, t, over, ledge, gx, gy) || save_pointer(n, t, over, ledge, gx, gy)) {
        waiting = 0;
        click_pending = 0;
        return;
    }
    /* The card's info panel: clicks only close it. */
    if (info_pointer(n, t, over, ledge)) {
        waiting = 0;
        click_pending = 0;
        return;
    }
    /* 2026-11-16: a click on your mascot in the duel makes it react
     * (mascota.c); the game does not get it */
    if (over && ledge) {
        extern int ygo_mascota_clic(float gx, float gy);
        /* 22:2x log: the graveyard's arrow (54,157) went to the mascot
         * under it; not while a card list is open */
        if (t - s_list_at > 150u && t - s_tl_at > 150u && ygo_mascota_clic(gx, gy)) {
            waiting = 0;
            click_pending = 0;
            return;
        }
    }
    /* The duel's command buttons: the one under the pointer is chosen. */
    if (cmd_pointer(n, t, over, ledge, gx, gy)) {
        waiting = 0;
        click_pending = 0;
        return;
    }
    /* The main menu's wheel: the icon under the pointer is chosen. First:
     * one of the left/right menus below also runs under the main menu (no
     * glove there), and when it came first it took the wheel's hover away
     * (12:41). */
    if (caja_pointer(n, t, over, ledge, gx, gy) || wheel_pointer(n, t, over, ledge, gx, gy)) {
        waiting = 0;
        click_pending = 0;
        return;
    }
    /* The triple duel's order: the arrows between the portraits. */
    if (tri_pointer(n, t, over, ledge, gx, gy)) {
        waiting = 0;
        click_pending = 0;
        return;
    }
    /* Rock-paper-scissors and the like: the arrows are clicked. */
    if (side_pointer(n, t, over, ledge, gx, gy)) {
        waiting = 0;
        click_pending = 0;
        return;
    }
    /* The Target List (bring a monster...): cards and its two buttons. */
    if (tl_pointer(n, t, over, ledge, gx, gy)) {
        waiting = 0;
        click_pending = 0;
        return;
    }
    /* A card list over the field (graveyard...): wheel and clicks. */
    if (list_pointer(n, t, over, ledge, gx, gy)) {
        waiting = 0;
        click_pending = 0;
        return;
    }
    /* The duel field: the cursor goes straight to the pointer's square. */
    if (field_pointer(n, t, over, ledge, gx, gy)) {
        waiting = 0;
        click_pending = 0;              /* the field takes its own clicks */
        return;
    }

    /* Buttons: the window is not captured in this mode, so the kit sees no
     * clicks; read them here, only over the game's picture. A left click
     * waits for the cursor to get where the pointer is (at most
     * CLICK_WAIT_MS), so it picks what is under the pointer. */
#define CLICK_WAIT_MS 700u
/* 2026-11-17 (mouse review): a click while the game's glove was still on
 * its way to the pointer went in at once as the kit's A, on the place the
 * glove was passing, and this A came after it on the right one: two
 * presses. Such a click keeps the kit's A out and waits for the glove to
 * get there (WALK_WAIT_MS at most). With the glove already there, or no
 * glove known, the kit's A goes in at once as before. */
#define WALK_WAIT_MS 1200u
    static int walk_click;
    static ULONGLONG gen_b_until;
    if (over) {
        if (ledge) {
            click_pending = 1;
            click_at = t;
            /* 2026-11-17 01:0x (the user: "no funciona hacer click en OK"; log:
             * "A 1203 ms after the click"): the duel's message boxes (OK) have
             * no glove, and a glove left from the field (fingertip 545,218)
             * that never moved counted as "on its way". Only a glove that
             * moved in the last 300 ms is walking. */
            /* 03:5x (the user: "apretar ok con el primer click se siente
             * raro"; log: the OK after "Atacar" took its A 171-250 ms late,
             * "the glove still on its way"): that glove was the game's own,
             * sliding to the target by itself. A walk is ours: a step of it
             * pressed in the last 400 ms. */
            walk_click = !idle && s_gx >= 0.f && (LONGLONG)(t - s_gmove_at) < 300
                         && (LONGLONG)(t - s_walk_last) < 400;
            /* 2026-11-17 (ghost clicks at 120 fps): with no glove walking the
             * kit's A is the click, at once; a second A of ours up to
             * CLICK_WAIT_MS later (while the d-pad probed for a glove) landed
             * on whatever came after: a message box closed by the first, the
             * next one by this. */
            /* 2026-11-17 (the user: "a veces si presionas muy rapido no te lo
             * toma al instante"): the kit's A is the button held at the
             * moment the game reads the pad; a click let go before that read
             * (very quick, or in a slow frame) never got there. So our own A
             * goes in at once with it, on the same press (one edge: both
             * start together), and is the click when the kit missed it. */
            if (!walk_click) {
                click_pending = 0;
                a_until = t + PRESS_MS + 15u;
            }
            fprintf(stderr, "[RATON] click game %.0f,%.0f%s; game fingertip %.0f,%.0f"
                    " (gloves %ld, moves %ld, places %d)\n",
                    gx, gy, xbox_VideoWidescreen() ? " (16:9)" : " (4:3)",
                    s_gx, s_gy, (long)s_ngloves, (long)s_moves, s_npl);
            fflush(stderr);
            ygo_caja_toque(); ygo_caja_nota("clic %.0f,%.0f (dedo del juego %.0f,%.0f)", gx, gy, s_gx, s_gy);
        }
        /* 03:5x (the user: "si moves y apretas el segundo click no te lo
         * hace"): moving the mouse here walks the game's glove with the
         * d-pad, and the menus take one key a frame: a direction held with
         * the click won over it. A press (not a walk click) lets go of the
         * walk's step and holds the walk 250 ms; the right button's B lasts
         * B_HOLD_MS at least (a quick one, seen by the hook). */
        if ((ledge && !walk_click) || s_redge || (GetAsyncKeyState(VK_RBUTTON) & 0x8000)) {
            s_btn_quiet = t + 250u;
            if (s_held && !s_held_wheel) {
                s_held = 0;
                s_until = t;
            }
        }
        if (s_redge)
            gen_b_until = t + B_HOLD_MS;
        if ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) || t < gen_b_until)
            n->Gamepad.bAnalogButtons[XBOX_BUTTON_B] = 255;
        if (GetAsyncKeyState(VK_MBUTTON) & 0x8000)
            n->Gamepad.bAnalogButtons[XBOX_BUTTON_Y] = 255;
    }
    if (click_pending && ((idle && !waiting && !s_held)
                          || t - click_at >= (walk_click ? WALK_WAIT_MS : CLICK_WAIT_MS)
                          /* the glove stopped short of the pointer: no more waiting */
                          || (walk_click && (LONGLONG)(t - s_gmove_at) >= 400 && t - click_at >= 100u))) {
        click_pending = 0;
        a_until = t + PRESS_MS + 15u;
        if (walk_click) {
            fprintf(stderr, "[RATON] click game: A %llu ms after the click (the glove %s)\n",
                    (unsigned long long)(t - click_at), idle ? "got there" : "still on its way");
            fflush(stderr);
        }
    }
    if (click_pending && walk_click)
        mouse_a_off(n, t);              /* the kit's A waits with ours */
    else
        walk_click = 0;
    if (t < a_until)
        n->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = 255;

    /* The wheel steps up and down: lists, menus, the hand. */
    if (s_wheel && !waiting && s_held == 0) {
        LONG w = s_wheel;
        if (press(w > 0 ? XBOX_GAMEPAD_DPAD_UP : XBOX_GAMEPAD_DPAD_DOWN, t))
            { InterlockedExchangeAdd(&s_wheel, w > 0 ? -1 : 1); s_held_wheel = 1; }
    }

    /* Our last press: where did it take the cursor? */
    if (waiting) {
        if (s_gseq != seen_seq) {
            seen_seq = s_gseq;
            changed_at = t;
        }
        if (s_gseq != seq_before && t - changed_at >= SETTLE_MS) {
            waiting = 0;
            if (from >= 0 && key_sent >= 0 && s_gx >= 0.f) {
                int to = place_of(s_gx, s_gy);
                s_pl[from].next[key_sent] = (short)to;
                if (to == from)
                    s_pl[from].blocked_at[key_sent] = t;
                else {
                    int ax = key_sent >= K_LEFT ? 0 : 1;
                    float d = ax ? fabsf(s_pl[to].y - s_pl[from].y)
                                 : fabsf(s_pl[to].x - s_pl[from].x);
                    if (d > 8.f && (s_space[ax] <= 0.f || d < s_space[ax]))
                        s_space[ax] = d;
                }
            }
        } else if (s_gseq == seq_before && t >= answer_by) {
            waiting = 0;                /* it did nothing */
            if (from >= 0 && key_sent >= 0) {
                s_pl[from].next[key_sent] = (short)from;
                s_pl[from].blocked_at[key_sent] = t;
            }
        }
    }

    idle = 0;
    if (!over || waiting || s_held || t < s_until || t < s_btn_quiet)
        return;
    /* In the duel the glove walk below never presses: a pause of the field
     * update over 150 ms (an animation, a hitch) let it step the hand
     * cursor with the d-pad toward the pointer. */
    if (t - s_field_at < 2000u)
        return;

    /* Where the game's cursor is not known yet (a fresh screen: the duel
     * leaves its glove unplaced until the d-pad first moves it, 11:24), one
     * press makes the game place it; at most four per screen. */
    {
        static LONG probe_gen = -1;
        if (s_ngloves != probe_gen || s_gx >= 0.f) {
            probe_gen = s_ngloves;
            probes = 0;
        }
    }
    if (s_gx < 0.f) {
        static const WORD order[4] = {
            XBOX_GAMEPAD_DPAD_RIGHT, XBOX_GAMEPAD_DPAD_LEFT,
            XBOX_GAMEPAD_DPAD_DOWN, XBOX_GAMEPAD_DPAD_UP };
        if (probes < 4 && press(order[probes], t)) {
            probes++;
            waiting = 1;
            from = key_sent = -1;
            seq_before = seen_seq = s_gseq;
            answer_by = t + PRESS_MS + ANSWER_MS;
        } else if (probes >= 4)
            idle = 1;
        return;
    }

    {
        int c = place_of(s_gx, s_gy);
        int k = next_key(c, gx, gy, t);
        if (k < 0) {
            idle = 1;                   /* as near as it gets */
            return;
        }
        if (press(k_key[k], t)) {
            /* a step right after another is the same walk: no tick */
            if (t - s_walk_last < 400u)
                s_mute_until = t + PRESS_MS + 150u;
            s_walk_last = t;
            waiting = 1;
            from = c;
            key_sent = k;
            seq_before = seen_seq = s_gseq;
            answer_by = t + PRESS_MS + ANSWER_MS;
        }
    }
}

/* ---- the entry from the pad read (mando_directo.c, host_state) ---------- */

/* F8: the frame rate (the kit's vblank rate). Read here,
 * on every pad read, with the game window in front. */
void     xbox_VblankHzSet(unsigned hz);
unsigned xbox_VblankHz(void);

/* 2026-11-09 (the user): F8 alone, 60 -> 90 -> 120 -> 60, and the new rate
 * shown on screen (the kit's toast, as F2's notices). */
void xbox_MenuToast(const char *utf8, unsigned ms);
void xbox_MenuSaveSoon(void);

static void fps_key(void)
{
    static int was;
    int now = (GetAsyncKeyState(VK_F8) & 0x8000) && s_wnd && GetForegroundWindow() == s_wnd;
    if (now && !was) {
        unsigned cur = xbox_VblankHz();
        /* 2026-11-20 (the user: "agrega 144 fps, 165 fps y 200 fps") */
        unsigned hz = cur < 90u ? 90u : cur < 120u ? 120u : cur < 144u ? 144u : cur < 165u ? 165u : cur < 200u ? 200u : 60u;
        char msg[32];
        xbox_VblankHzSet(hz);
        xbox_MenuSaveSoon();            /* 2026-11-20: kept for the next start (main.c fps_juego) */
        snprintf(msg, sizeof msg, "%u FPS", hz);
        xbox_MenuToast(msg, 1500u);
        fprintf(stderr, "[YGO] F8: %u fps\n", hz);
        fflush(stderr);
    }
    was = now;
}

/* ---- the name screen: typed on the keyboard ----------------------------- */

/* The first screen's name entry is an on-screen keyboard: sub_00110FD0 runs
 * it every frame (thiscall, the screen object); A takes the letter under
 * its cursor from the table at 0x456460 and calls sub_001103D0(letter)
 * (thiscall, ret 4: appends to the name at +0x158, length +0x140, at most
 * 12), the delete key calls sub_00110480() (removes the last). Wrapped: the
 * letters typed on the PC keyboard are appended the same way, Backspace
 * deletes; Enter stays START as the kit maps it. While a typing key is
 * down the kit's key map (Z = A, X = B, I/J/K/L = stick...) is kept out of
 * the pad state so typing does not also move or press on that screen. */
void sub_00110FD0_pad(void);
void sub_001103D0(void);
void sub_00110480(void);
void sub_00110050(void);
void sub_0011A400(void);
void sub_00117F70(void);
void sub_0002DB90(void);

static SRWLOCK s_type_lock = SRWLOCK_INIT;
static unsigned char s_typed[32];
static int s_typed_n;
static int s_typing;                             /* a typing key is down */

static void type_push(unsigned char c)
{
    AcquireSRWLockExclusive(&s_type_lock);
    if (s_typed_n < (int)sizeof s_typed)
        s_typed[s_typed_n++] = c;
    ReleaseSRWLockExclusive(&s_type_lock);
}

/* The keys typed since the last poll (edges), as Latin-1 letters; 8 is a
 * delete. Only with the game window in front and the name screen up. */
static void type_poll(ULONGLONG t)
{
    static unsigned char was[256];
    int vk, shift, caps;
    s_typing = 0;
    if (t - s_name_at > 300u || !s_wnd || GetForegroundWindow() != s_wnd) {
        memset(was, 0, sizeof was);
        return;
    }
    shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    caps = (GetKeyState(VK_CAPITAL) & 1) != 0;
    {
        int down = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
        if (down && !was[VK_RETURN]) {
            InterlockedExchange(&s_name_finish, 1);
            fprintf(stderr, "[RATON] name: Enter -> finish\n");
            fflush(stderr);
        }
        was[VK_RETURN] = (unsigned char)down;
    }
    for (vk = 8; vk < 256; vk++) {
        int typing = (vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')
                     || vk == VK_SPACE || vk == VK_BACK || vk == VK_OEM_3;
        int down;
        if (!typing)
            continue;
        down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        if (down)
            s_typing = 1;
        /* Backspace held: keeps deleting, like Windows (after 400 ms, one
         * letter every 50 ms; 2026-11-08). */
        if (vk == VK_BACK) {
            static ULONGLONG back_next;
            if (down && !was[vk])
                back_next = t + 400u;
            else if (down && t >= back_next) {
                back_next = t + 50u;
                type_push(8);
            }
        }
        if (down && !was[vk]) {
            unsigned c;
            if (vk == VK_BACK)
                c = 8;
            else if (vk == VK_SPACE)
                c = ' ';
            else if (vk >= '0' && vk <= '9')
                c = (unsigned)vk;
            else {
                c = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_CHAR) & 0xFFu;   /* 'A', or 'Ñ' (0xD1) */
                if (!c)
                    c = vk < 'A' ? 0 : (unsigned)vk;
                if (c && !(shift ^ caps)) {                              /* lower case */
                    if (c >= 'A' && c <= 'Z')
                        c += 0x20;
                    else if (c >= 0xC0 && c <= 0xDE && c != 0xD7)
                        c += 0x20;
                }
            }
            if (c) {
                type_push((unsigned char)c);
                fprintf(stderr, "[RATON] name: typed %s0x%02X\n", c == 8 ? "delete " : "", c);
                fflush(stderr);
            }
        }
        was[vk] = (unsigned char)down;
    }
}

void sub_001104F0(void);
void sub_001102A0(void);

/* The letter's place on the on-screen keyboard (table 0x456460: page,
 * row 0..3, column 0..14), so its green frame shows what was typed. */
static void name_cursor_to(uint32_t obj, unsigned c)
{
    int pg, row, col, cur = (int)MEM32(obj + 0x144);
    for (pg = 0; pg < 4; pg++) {
        int p = (cur + pg) & 3;                     /* the page shown first */
        for (row = 0; row < 4; row++)
            for (col = 0; col < 15; col++)
                if (MEM8(0x00456460u + (uint32_t)((p * 4 + row) * 15 + col)) == c) {
                    uint32_t sp = g_esp;
                    if (p != cur) {
                        MEM32(obj + 0x144) = (uint32_t)p;
                        g_ecx = obj;
                        PUSH32(g_esp, 0x001115DEu);
                        sub_001104F0();
                        g_esp = sp;
                    }
                    /* the game's own move (d-pad, 0x11152F): sub_001102A0(col,
                     * row) recolours the old and new letter and moves the
                     * green frame; writing +0x134/+0x138 moved neither */
                    g_ecx = obj;
                    PUSH32(g_esp, (uint32_t)row);
                    PUSH32(g_esp, (uint32_t)col);
                    PUSH32(g_esp, 0x00111538u);
                    sub_001102A0();             /* ret 8 */
                    g_esp = sp;
                    fprintf(stderr, "[RATON] name: cursor to page %d row %d col %d (state %u)\n",
                            p, row, col, MEM32(obj + 0x168));
                    return;
                }
    }
}

void sub_00110190(void);

/* One frame of the screen's own update with only `keys` down in its key
 * word [0x650424 + pad * 0x16C] (sub_00028DE0: 2 A, 0x100 up, 0x200 down,
 * 0x400 right, 0x800 left; pad [0x5FC1B0]) and no page keys in
 * [0x650420 + ...] (sub_00028DC0). The update returns for the wrapper. */
static void name_keys(uint32_t obj, uint32_t keys)
{
    int pad = (int)MEM32(0x005FC1B0u);
    uint32_t kw, kh, okw = 0, okh = 0;
    int ok = pad >= 0 && pad < 4;
    kw = 0x00650424u + (uint32_t)(ok ? pad : 0) * 0x16Cu;
    kh = 0x00650420u + (uint32_t)(ok ? pad : 0) * 0x16Cu;
    if (ok) {
        okw = MEM32(kw);
        okh = MEM32(kh);
        MEM32(kw) = keys;
        MEM32(kh) = 0;
    }
    g_ecx = obj;
    sub_00110FD0_pad();
    if (ok) {
        MEM32(kw) = okw;
        MEM32(kh) = okh;
    }
}

/* sub_00110190(b) (thiscall, ret 4): the green bar to button b, as the
 * game's left/right on the buttons (0x11164A / 0x11168C, with the sound). */
static void name_button_to(uint32_t obj, int b, int sound)
{
    uint32_t sb = g_ebx, si = g_esi, di = g_edi, bp = g_ebp, sp = g_esp;
    if (sound) {
        PUSH32(g_esp, 0x10000001u);
        PUSH32(g_esp, 0x00111654u);
        sub_0011A400();
        g_esp = sp;
    }
    g_ecx = obj;
    PUSH32(g_esp, (uint32_t)b);
    PUSH32(g_esp, 0x0011166Au);
    sub_00110190();
    g_esp = sp;
    g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
}

/* The name screen, sub_00110FD0 (thiscall update; +0x168 the state: 2 the
 * letters, 3 the buttons row, 4 done). What the user confirmed (typing,
 * the green frame on each typed letter, Enter) is kept as it was; the
 * three buttons go through the game's own moves (2026-11-08 22:10):
 * - the pointer onto a button from the letters: the frame to the bottom
 *   row (sub_001102A0(col, 3)) and down for one frame (0x1114BA: the
 *   game hides the letter frame, shows the green bar, state 3), then
 *   sub_00110190 to the button under the pointer;
 * - on the buttons: the bar follows the pointer (sub_00110190 + sound);
 * - a click there: A for one frame (0x111821: Atrás page -1, Próximo
 *   page +1, Aceptar finish);
 * - typing, or the pointer back over the letters, while on the buttons:
 *   up for one frame (0x1116C7: back to the letters, state 2), then the
 *   letters go in as before on the next frame. */
void sub_00110FD0(void)
{
    uint32_t obj = g_ecx;
    unsigned char c[32];
    int k, n, typed;
    LONG b, hov, hseq;
    static LONG seen_hseq;
    int hchg;
    s_name_at = GetTickCount64();
    if (!obj) {
        sub_00110FD0_pad();
        return;
    }
    b = InterlockedExchange(&s_name_btn, -1);
    hov = s_name_hover;
    hseq = s_name_hover_seq;
    hchg = hseq != seen_hseq;
    seen_hseq = hseq;
    AcquireSRWLockExclusive(&s_type_lock);
    typed = s_typed_n;
    ReleaseSRWLockExclusive(&s_type_lock);

    if (MEM32(obj + 0x168) == 3u && b < 0 && typed > 0) {
        name_keys(obj, 0x100u);                         /* up: to the letters */
        fprintf(stderr, "[RATON] name: back to the letters (state %u)\n", MEM32(obj + 0x168));
        fflush(stderr);
        return;
    }
    if (MEM32(obj + 0x168) == 2u && (b >= 0 || (hchg && hov >= 0 && typed == 0))) {
        int want = b >= 0 ? (int)b : (int)hov;
        if (MEM32(obj + 0x138) != 3u) {                 /* down works from the bottom row */
            uint32_t sp = g_esp;
            g_ecx = obj;
            PUSH32(g_esp, 3u);
            PUSH32(g_esp, MEM32(obj + 0x134));
            PUSH32(g_esp, 0x00111538u);
            sub_001102A0();                             /* ret 8 */
            g_esp = sp;
        }
        name_keys(obj, 0x200u);                         /* down: to the buttons */
        if (MEM32(obj + 0x168) == 3u && MEM32(obj + 0x13C) != (uint32_t)want)
            name_button_to(obj, want, 0);
        if (b >= 0)
            InterlockedCompareExchange(&s_name_btn, b, -1);   /* its A next frame */
        fprintf(stderr, "[RATON] name: to the buttons, button %d (state %u)\n",
                want, MEM32(obj + 0x168));
        fflush(stderr);
        return;
    }
    if (MEM32(obj + 0x168) == 3u) {
        int want = b >= 0 ? (int)b : (hchg && hov >= 0 ? (int)hov : -1);
        if (want >= 0 && MEM32(obj + 0x13C) != (uint32_t)want)
            name_button_to(obj, want, 1);
        if (b >= 0) {
            uint32_t opage = MEM32(obj + 0x144);
            name_keys(obj, 2u);                         /* A on the button */
            fprintf(stderr, "[RATON] name: button %ld by the game: page %u -> %u, state %u\n",
                    (long)b, opage, MEM32(obj + 0x144), MEM32(obj + 0x168));
            fflush(stderr);
            return;
        }
    }

    AcquireSRWLockExclusive(&s_type_lock);
    n = s_typed_n;
    memcpy(c, s_typed, (size_t)n);
    s_typed_n = 0;
    ReleaseSRWLockExclusive(&s_type_lock);
    for (k = 0; k < n; k++) {
        uint32_t sb = g_ebx, si = g_esi, di = g_edi, bp = g_ebp, sp = g_esp;
        g_ecx = obj;
        if (c[k] == 8) {
            PUSH32(g_esp, 0x00110FE3u);         /* a return address inside the screen */
            sub_00110480();                     /* ret */
        } else {
            if (c[k] != ' ' && MEM32(obj + 0x168) == 2u)   /* the letters, not the buttons */
                name_cursor_to(obj, c[k]);
            g_esp = sp;
            g_ecx = obj;
            PUSH32(g_esp, (uint32_t)c[k]);
            PUSH32(g_esp, 0x00110FE3u);
            sub_001103D0();                     /* ret 4 */
        }
        g_esp = sp;
        g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
    }
    /* Enter (or a click): what the screen's end button does with A
     * (0x11183D): the name must not be empty (sub_00110050), then the
     * sound, sub_0002DB90(name), state +0x16C = 2, +0x168 = 4, and the
     * frame's update returns there. */
    if (InterlockedExchange(&s_name_finish, 0) && obj) {
        uint32_t sb = g_ebx, si = g_esi, di = g_edi, bp = g_ebp, sp = g_esp;
        int ok;
        g_ecx = obj;
        PUSH32(g_esp, 0x00110FE3u);
        sub_00110050();
        ok = (g_eax & 0xFFu) != 0;
        g_esp = sp;
        if (ok) {
            PUSH32(g_esp, 0x10000002u);
            PUSH32(g_esp, 0x00111856u);
            sub_0011A400();
            PUSH32(g_esp, 0x453B8000u);
            PUSH32(g_esp, 0x00111860u);
            sub_00117F70();
            PUSH32(g_esp, obj + 0x158u);
            PUSH32(g_esp, 0x0011186Cu);
            sub_0002DB90();
            g_esp = sp;
            MEM32(obj + 0x16C) = 2;
            MEM32(obj + 0x168) = 4;
            fprintf(stderr, "[RATON] name: finished\n");
        } else
            fprintf(stderr, "[RATON] name: empty, not finished\n");
        fflush(stderr);
        g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
        if (ok) {
            g_esp += 4;                         /* ret, as the button's path */
            return;
        }
    }
    g_ecx = obj;
    sub_00110FD0_pad();
}

/* The title menu, sub_001125C0 (thiscall update, state +0x4C): in state
 * 8 left/right pick between Nueva partida and Cargar partida through
 * the selector at +0x48 (0x1129BC) (sub_000EF040 reads it, sub_000EEFE0(i) sets it,
 * with the sound 0x10000001), A confirms. Wrapped: the half under the
 * pointer is picked the same way. */
void sub_001125C0_pad(void);
void sub_000EF040(void);
void sub_000EEFE0(void);

void sub_00111A10(void);

void sub_001125C0(void)
{
    uint32_t obj = g_ecx;
    /* 2026-11-08 21:45: the menu is state 11 (0x112AA2), not 8 (that one is
     * a selector prompt): +0x70 is the option (0 Nueva, 1 Cargar), the
     * triangle sits at x 85 or 340 from it (sub_00111A10, every frame),
     * left/right do the sound 0x10000001, +0x70 = i, sub_00111A10
     * (0x112C46 / 0x112C7B), A or START confirms. */
    if (obj) {
        static uint32_t last = ~0u;
        if (MEM32(obj + 0x4C) != last) {
            last = MEM32(obj + 0x4C);
            fprintf(stderr, "[RATON] title: state %u %llu ms\n", last, (unsigned long long)GetTickCount64());
            fflush(stderr);
        }
    }
    /* The title screen with "Presiona START": state 10 waits for the press
     * (a click there played 0x10000002 from 0x112A93, then 7 and the menu,
     * 11); 3 is the title coming in and 6 the step to 10 (a click in 3 went
     * straight to 6). 2026-11-10 log. */
    if (obj && (MEM32(obj + 0x4C) == 3u || MEM32(obj + 0x4C) == 6u || MEM32(obj + 0x4C) == 10u))
        s_start_at = GetTickCount64();
    if (obj && MEM32(obj + 0x4C) == 11u) {
        LONG want = InterlockedExchange(&s_title_want, -1);
        static ULONGLONG cuenta_desde, peor_de, ultimo;
        static unsigned cuadros, peor;
        ULONGLONG ahora = GetTickCount64();
        s_title_at = ahora;
        if (ultimo && ahora - ultimo > peor)
            peor = (unsigned)(ahora - ultimo);
        ultimo = ahora;
        cuadros++;
        if (!cuenta_desde || ahora - cuenta_desde > 5000u)
            cuenta_desde = ahora, cuadros = 0, peor = 0;
        else if (ahora - cuenta_desde >= 1000u) {
            fprintf(stderr, "[RATON] title menu: %u updates in %llu ms (slowest gap %u ms)\n", cuadros,
                    (unsigned long long)(ahora - cuenta_desde), peor);
            fflush(stderr);
            cuenta_desde = ahora;
            cuadros = 0;
            peor = 0;
        }
        (void)peor_de;
        {
            /* Once: the menu's pieces (+0x14..+0x2C, the arrow last) with
             * the floats of their first 0xC0 bytes in screen range, to
             * check the option boxes against where the texts really are. */
            static int told;
            if (!told) {
                uint32_t k, o;
                told = 1;
                for (k = 0x14; k <= 0x2C; k += 4) {
                    uint32_t s = MEM32(obj + k);
                    char line[512];
                    int n = 0;
                    if (!s)
                        continue;
                    for (o = 0; o < 0xC0 && n < 480; o += 4) {
                        float f;
                        uint32_t v = MEM32(s + o);
                        memcpy(&f, &v, 4);
                        if (f >= 1.f && f <= 640.f && f == f)
                            n += snprintf(line + n, sizeof line - n, " +%02X=%.1f", o, f);
                    }
                    fprintf(stderr, "[RATON] title piece +%02X %08X:%s\n", k, s, n ? line : " -");
                }
                fflush(stderr);
            }
        }
        if ((want == 0 || want == 1) && MEM32(obj + 0x70) != (uint32_t)want) {
            uint32_t sb = g_ebx, si = g_esi, di = g_edi, bp = g_ebp, sp = g_esp;
            PUSH32(g_esp, 0x10000001u);
            PUSH32(g_esp, 0x00112C50u);
            sub_0011A400();
            g_esp = sp;
            MEM32(obj + 0x70) = (uint32_t)want;
            g_ecx = obj;
            PUSH32(g_esp, 0x00112C5Du);
            sub_00111A10();
            g_esp = sp;
            g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
            fprintf(stderr, "[RATON] title: hover -> %ld %llu ms\n", (long)want, (unsigned long long)GetTickCount64());
            fflush(stderr);
        }
        g_ecx = obj;
    }
    sub_001125C0_pad();
}

/* The menu sound, sub_0011A400(id) (cdecl, plain ret; 0x10000001 is the
 * cursor's tick). The pointer walk's middle steps play it muted. Who plays
 * the tick is logged, with the Biblioteca's candidate cursor globals
 * (0x66DB74/76/78, sub_000D9A10), to find the grid for a direct jump. */
void sub_0011A400_pad(void);
void sub_0011A400(void)
{
    uint32_t id = MEM32(g_esp + 4), ret = MEM32(g_esp);
    if (id == 0x10000001u) {
        static unsigned said;
        if (said < 120) {
            said++;
            fprintf(stderr, "[RATON] tick from %08X (66DB74 %u 66DB76 %u 66DB78 %u)%s\n",
                    ret, MEM16(0x0066DB74u), MEM16(0x0066DB76u), MEM32(0x0066DB78u),
                    GetTickCount64() < s_mute_until ? " muted" : "");
            fflush(stderr);
        }
    }
    sub_0011A400_pad();
}

/* buscador.c (2026-11-16): a result clicked, the game's cursor to grid
 * place idx: its row two below the top (the scroll bar's top row, the
 * sub_00038220 wrapper), the cursor there (the pointer's way, the preview
 * loaded: s_lib_force) */
void ygo_biblioteca_ir(int idx)
{
    int row = idx / 10, top = row - 2;
    if (idx < 0)
        return;
    if (top < 0)
        top = 0;
    if (top > 0x61)
        top = 0x61;
    if (row - top > 4)
        row = top + 4;
    if (top != (int)MEM16(0x0066E690u))
        InterlockedExchange(&s_lib_drag, top);
    InterlockedExchange(&s_lib_want, (LONG)((idx % 10) | ((row - top) << 8)));
    InterlockedExchange(&s_lib_force, 1);
}

/* The Biblioteca's key handler, sub_0010F790 (cdecl, no args; the key in
 * [player + 0x1330], 0x10 = none). In its grid mode ([0x66E684] = 0) the
 * cursor is column [0x66E68C] (0..9), shown row [0x66E68E] (0..4) and top
 * row [0x66E690]; a d-pad move sets them, ORs [0x66DD18] with 0x288 and 8
 * (redraw) and plays 0x1000009A (sub_0007E280). The pointer's card is put
 * there the same way, in one go. */
void sub_0010F790_pad(void);
void sub_0010F790(void)
{
    LONG w;
    static int said;
    s_lib_at = GetTickCount64();
    if (!said) {
        said = 1;
        fprintf(stderr, "[RATON] library: key handler runs (mode %u)\n", MEM16(0x0066E684u));
        fflush(stderr);
    }
    /* the exit box (mode 4): the pointer's line marked as the d-pad does
     * (0x10F9CC: choice set, [0x66DD18] |= 0x40000, tick 0x10000001) */
    w = InterlockedExchange(&s_dlg_want, -1);
    if (w >= 0 && MEM16(0x0066E684u) == 4u && MEM8(0x0066E688u) != (uint32_t)w) {
        uint32_t sb = g_ebx, si = g_esi, di = g_edi, bp = g_ebp, sp = g_esp;
        MEM8(0x0066E688u) = (uint8_t)w;
        MEM32(0x0066DD18u) |= 0x40000u;
        PUSH32(g_esp, 0x10000001u);
        PUSH32(g_esp, 0x0010FE9Du);             /* the game's own return spot */
        sub_0007E280();
        g_esp = sp;
        g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
    }
    /* the open card's two round buttons (mode 2): the d-pad flips
     * [0x66E689] at 0x10F94D, ORs [0x66DD18] with 0xB80 and ticks
     * 0x10000001; the pointer's button is marked the same way */
    w = InterlockedExchange(&s_btn_want, -1);
    if (w >= 0 && MEM16(0x0066E684u) == 2u && MEM8(0x0066E689u) != (uint32_t)w) {
        uint32_t sb = g_ebx, si = g_esi, di = g_edi, bp = g_ebp, sp = g_esp;
        MEM8(0x0066E689u) = (uint8_t)w;
        MEM32(0x0066DD18u) |= 0xB80u;
        PUSH32(g_esp, 0x10000001u);
        PUSH32(g_esp, 0x0010FE9Du);
        sub_0007E280();
        g_esp = sp;
        g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
    }
    w = InterlockedExchange(&s_lib_want, -1);
    if (w >= 0 && MEM16(0x0066E684u) == 0) {
        uint32_t c = (uint32_t)(w & 0xFF), r = (uint32_t)(w >> 8);
        LONG forced = InterlockedExchange(&s_lib_force, 0);
        if (MEM16(0x0066E68Cu) != c || MEM16(0x0066E68Eu) != r || forced) {
            if (forced)
                MEM32(0x0066E698u) |= 0x4000u;  /* the preview: load the card there */
            /* silent here: the tick waits for the pointer to rest (below).
             * 21:14 log: crossing cards on the way ticked on each one */
            MEM16(0x0066E68Cu) = c;
            MEM16(0x0066E68Eu) = r;
            MEM32(0x0066DD18u) |= 0x288u | 8u;
            s_lib_tick_at = GetTickCount64();
            {
                static unsigned n;
                if (n++ < 40) {
                    fprintf(stderr, "[RATON] library: cursor to col %u row %u (top %u)\n",
                            c, r, MEM16(0x0066E690u));
                    fflush(stderr);
                }
            }
        }
    }
    /* one tick once the pointer has rested 90 ms on its new card, and only
     * on a card the player has (21:30: the empty places ticked too). The A
     * handler's own test (0x10FB5E): card list 0x66DD20[(top + row) * 10 +
     * col], owned count at [save 0x651E70 + id * 2 + 0x3DA]. */
    if (s_lib_tick_at && GetTickCount64() - s_lib_tick_at >= 90u
            && MEM16(0x0066E684u) == 0) {
        uint32_t sb = g_ebx, si = g_esi, di = g_edi, bp = g_ebp, sp = g_esp;
        uint32_t idx = (MEM16(0x0066E690u) + MEM16(0x0066E68Eu)) * 10u + MEM16(0x0066E68Cu);
        uint32_t id = MEM16(0x0066DD20u + idx * 2u);
        s_lib_tick_at = 0;
        if (idx < 0x473u && MEM16(0x00651E70u + id * 2u + 0x3DAu) != 0)
        {
            PUSH32(g_esp, 0x1000009Au);
            PUSH32(g_esp, 0x0010FA6Cu);         /* the game's own return spot */
            sub_0007E280();
            g_esp = sp;
            g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
        }
    }
    /* the clicked card opens: the handler reads its key from the input
     * object (sub_000D87C0 = 0x66C880) at +0x1330 = 0x66DBB0, 0x10 = none,
     * and puts 0x10 back once handled; key 12 is A (jump table 0x10FEB0 ->
     * 0x10FB55: mode 0, the card under the cursor opens, mode 1). Put here,
     * right before the handler runs, it is read this very call, whatever
     * the scene was doing a frame earlier (a key the stick test at
     * 0x10F7AE turns away stays in the slot for the next call). */
    w = s_lib_open;
    if (w >= 0) {
        if (MEM16(0x0066E684u) != 0u || GetTickCount64() - s_lib_open_at > 1500u) {
            InterlockedExchange(&s_lib_open, -1);
        } else if (MEM16(0x0066E68Cu) == (uint32_t)(w & 0xFF)
                   && MEM16(0x0066E68Eu) == (uint32_t)(w >> 8)
                   && MEM32(0x0066DBB0u) == 0x10u) {
            MEM32(0x0066DBB0u) = 12u;
            InterlockedExchange(&s_lib_open, -1);
            fprintf(stderr, "[RATON] library: open col %ld row %ld (key A into the game's slot, %llu ms after the click)\n",
                    (long)(w & 0xFF), (long)(w >> 8),
                    (unsigned long long)(GetTickCount64() - s_lib_open_at));
            fflush(stderr);
        }
    }
    sub_0010F790_pad();
    {
        /* premios_extra.c: who gives the card under the cursor */
        extern void ygo_quien_da_tick(void);
        ygo_quien_da_tick();
    }
}

/* The Biblioteca scene's state machine, sub_00038220 (thiscall, the scene
 * in ecx; its state at [0x66DD18 + 0x97C] = [0x66E694]: 0 idle, 7 the
 * scroll's transition, 8 the scroll bar). The wheel's rows go in here, in
 * the idle state, as the scroll bar does: top row set, [0x66DD18] |= 8,
 * sub_000381E0(scene) (redraws the grid at once, state back to 0); then
 * [0x66E698] |= 0x4000 so the card preview follows the cursor's new card.
 * One tick a turn (the first notch after 400 ms quiet). */
void sub_00038220_pad(void);
void sub_000381E0(void);
void sub_000E3510(void);
void sub_00038220(void)
{
    uint32_t scene = g_ecx;
    s_lib_scene = scene;
    if ((s_lib_scroll || s_lib_drag >= 0) && MEM16(0x0066E684u) == 0 && MEM32(0x0066E694u) == 0u) {
        LONG d, pend = s_lib_scroll;
        int top = (int)MEM16(0x0066E690u), to;
        if (s_lib_drag >= 0) {
            /* the scroll bar held (2026-11-12): straight to its row */
            to = (int)InterlockedExchange(&s_lib_drag, -1);
            InterlockedExchange(&s_lib_scroll, 0);
        } else {
            /* a row a frame (a fast turn glides instead of jumping), at
             * most 4 pending (it stops within ~4 frames of the wheel) */
            if (pend > 4) { InterlockedExchange(&s_lib_scroll, 4); pend = 4; }
            if (pend < -4) { InterlockedExchange(&s_lib_scroll, -4); pend = -4; }
            d = pend > 0 ? 1 : -1;
            InterlockedExchangeAdd(&s_lib_scroll, -d);
            to = top + (int)d;
        }
        if (to < 0) to = 0;
        if (to > 0x61) to = 0x61;
        if (to != top) {
            static ULONGLONG last;
            ULONGLONG now = GetTickCount64();
            uint32_t sb = g_ebx, si = g_esi, di = g_edi, bp = g_ebp, sp = g_esp;
            MEM16(0x0066E690u) = (uint32_t)to;
            MEM32(0x0066DD18u) |= 8u;
            /* 2026-11-09 ("que la flecha amarilla de la izquierda suba o baje
             * al mismo nivel"): the scroll bar's thumb, as the game puts it
             * from the top row before a redraw (0x38332): sub_000E3510(top),
             * thiscall on the bar [scene + 0x1260], ret 4 */
            if (MEM32(scene + 0x1260u)) {
                g_ecx = MEM32(scene + 0x1260u);
                PUSH32(g_esp, (uint32_t)to);
                PUSH32(g_esp, 0x0003833Eu);
                sub_000E3510();
                g_esp = sp;
            }
            g_ecx = scene;
            PUSH32(g_esp, 0x00038424u);         /* the scroll bar's own return spot */
            sub_000381E0();
            g_esp = sp;
            MEM32(0x0066E698u) |= 0x4000u;
            if (now - last > 400u) {
                PUSH32(g_esp, 0x1000009Au);
                PUSH32(g_esp, 0x0010FA6Cu);
                sub_0007E280();
                g_esp = sp;
            }
            last = now;
            g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
            {
                static unsigned n;
                if (n++ < 40) {
                    fprintf(stderr, "[RATON] library: wheel top %d -> %d\n", top, to);
                    fflush(stderr);
                }
            }
        }
    }
    g_ecx = scene;
    sub_00038220_pad();
}

/* The sound engine's command entry, sub_001197F0(cmd, size, &arg) (cdecl,
 * plain ret): 0x100 plays a sound, 0x101 too (sub_0011A400 / 0011A430).
 * The Biblioteca's tick does not come through sub_0011A400 (20:47 log: no
 * tick there), so the walk's middle steps are muted here, every play in
 * their window. Each new (caller, id) is logged once. */
void sub_001197F0_pad(void);
void sub_001197F0(void)
{
    uint32_t ret = MEM32(g_esp), cmd = MEM32(g_esp + 4), pa = MEM32(g_esp + 12);
    if (cmd == 0x100u || cmd == 0x101u) {
        uint32_t id = pa ? MEM32(pa) : 0;
        /* only the cursor's own sounds (the ids seen ticking on a walk),
         * never anything else that plays in that short window */
        int muted = GetTickCount64() < s_mute_until
                    && (id == 0x10000001u || id == 0x10000004u || id == 0x1000009Au);
        static uint32_t seen[200][2];
        static unsigned nseen;
        unsigned i;
        for (i = 0; i < nseen && (seen[i][0] != ret || seen[i][1] != id); i++)
            ;
        /* every cursor tick, timed (the endless tick, 21:12) */
        if (id == 0x10000001u || id == 0x10000004u || id == 0x1000009Au) {
            static unsigned nt;
            if (nt < 600) {
                nt++;
                fprintf(stderr, "[RATON] tick %08X at %llu from %08X lib mode %u col %u row %u top %u%s\n",
                        id, (unsigned long long)GetTickCount64(), MEM32(g_esp + 0x14),
                        MEM16(0x0066E684u), MEM16(0x0066E68Cu), MEM16(0x0066E68Eu),
                        MEM16(0x0066E690u), muted ? " muted" : "");
            }
        }
        if (i == nseen && nseen < 200) {
            seen[nseen][0] = ret;
            seen[nseen][1] = id;
            nseen++;
            fprintf(stderr, "[RATON] sound cmd %X id %08X from %08X (caller's caller %08X)%s\n",
                    cmd, id, ret, MEM32(g_esp + 0x14), muted ? " muted" : "");
            fflush(stderr);
        }
        if (muted) {
            g_eax = 0;
            g_esp += 4;                 /* ret: the caller pops the args */
            return;
        }
        {
            /* 2026-11-16 (the first battle's stall, right after its first
             * battle sound): how long the engine takes with a play */
            LARGE_INTEGER f, a, b;
            double ms;
            QueryPerformanceFrequency(&f);
            QueryPerformanceCounter(&a);
            sub_001197F0_pad();
            QueryPerformanceCounter(&b);
            ms = (double)(b.QuadPart - a.QuadPart) * 1000.0 / (double)f.QuadPart;
            if (ms > 15.0)
                fprintf(stderr, "[SONIDO] cmd %X id %08X tardo %.0f ms\n", cmd, id, ms);
            return;
        }
    }
    sub_001197F0_pad();
}

/* The opponent's turn, faster (F2 / Configuración "Turno del rival rápido":
 * No, x2, x4; 2026-11-11). The guest clock runs at that speed (xbox_SpeedSet,
 * the warp the fast card plays used) while, in a duel, the duel's action
 * word [0x65F948] has bit 15 set (the acting player: bits 0-11 are the
 * action, sub_000811B0 switches on them; bit 15 picks the player's block,
 * 0x13F8 each, 1 = the opponent) and nothing of the player's waits for keys
 * (field cursor, target cursor, buttons, boxes, lists, info): a trap prompt
 * in the opponent's turn goes back to normal speed at once. "In a duel" =
 * one of those ran in the last 20 s. Turns off 500 ms after the opponent
 * stops acting at the earliest (no flicker between two actions). */
static volatile int s_rival_speed;               /* 0 off, 2 or 4 */
int ygo_rival_get(void) { return s_rival_speed == 2 ? 1 : s_rival_speed == 4 ? 2 : 0; }
void ygo_rival_set(int i) { s_rival_speed = i == 1 ? 2 : i == 2 ? 4 : 0; }

static void rival_tick(ULONGLONG t)
{
    static int on;
    static ULONGLONG on_at, rival_at;
    static int said;
    ULONGLONG last = s_field_at, v[8];
    int i, rival, waiting, want;
    v[0] = s_pick_at; v[1] = s_info_at; v[2] = s_list_at; v[3] = s_tl_at;
    v[4] = s_cmd_at; v[5] = s_wh_at; v[6] = s_side_at; v[7] = s_ask_at;
    for (i = 0; i < 8; i++)
        if (v[i] > last)
            last = v[i];
    waiting = last && t - last < 300u;
    rival = (MEM16(0x0065F948u) >> 15) & 1;
    if (rival)
        rival_at = t;
    want = s_rival_speed && !s_boost && last && t - last < 20000u && !waiting
           && (rival || (on && t - rival_at < 500u));
    if (want && !on) {
        on = 1;
        on_at = t;
        xbox_SpeedSet((unsigned)s_rival_speed);
        if (said < 400 && ++said)
            fprintf(stderr, "[RIVAL] fast x%d (action %04X)\n", s_rival_speed, MEM16(0x0065F948u));
    } else if (!want && on) {
        on = 0;
        if (xbox_Speed() != 1u)
            xbox_SpeedSet(1u);
        if (said < 400 && ++said)
            fprintf(stderr, "[RIVAL] normal after %llu ms (%s, action %04X)\n",
                    (unsigned long long)(t - on_at),
                    !s_rival_speed ? "turned off" : waiting ? "the player's turn to answer"
                    : t - last >= 20000u ? "no duel" : "the opponent stopped", MEM16(0x0065F948u));
    }
}

void ygo_mouse_apply(XBOX_INPUT_STATE *n)
{
    ULONGLONG t = GetTickCount64();
    int dx = 0, dy = 0;
    int mode = s_mode;

    fps_key();
    rival_tick(t);

    presses_tick(t);
    type_poll(t);
    if (s_typing)                       /* typing a name: no key map on the pad */
        memset(&n->Gamepad, 0, sizeof n->Gamepad);
    if (t - s_name_at < 300u)           /* Enter finishes by itself there */
        n->Gamepad.wButtons &= (WORD)~XBOX_GAMEPAD_START;
    /* The raw counts are taken in every mode, so none pile up for later. */
    xbox_InputMouseTakeLook(&dx, &dy);
    if (mode == MODE_STEPS)
        steps(dx, dy, s_step, t);
    else if (mode == MODE_POINTER) {
        /* 21:30 (video 21-25-51, log "[KEY] wheel -1"): in pointer mode the
         * kit takes the wheel too (capture NEVER) and pulses the d-pad 100 ms
         * a notch from its own queue (main.c binds the wheel to up/down), on
         * top of raton's own wheel (the hook): the Biblioteca ran on after
         * the wheel stopped and stopped hard. raton handles the wheel here,
         * so the kit's wheel pulses are taken off the pad. */
        if (xbox_InputMouseHeld(XBOX_MOUSE_WHEEL_UP))
            n->Gamepad.wButtons &= (WORD)~XBOX_GAMEPAD_DPAD_UP;
        if (xbox_InputMouseHeld(XBOX_MOUSE_WHEEL_DOWN))
            n->Gamepad.wButtons &= (WORD)~XBOX_GAMEPAD_DPAD_DOWN;
        pointer_mode(n, t);
        /* a click on a voice line's balloon: A, after every screen's own */
        if (t < s_voz_until)
            n->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = 255;
    }
    n->Gamepad.wButtons |= s_held;
    {
        /* the wheel standing on its last icon (Salir a Windows): its A closes
         * the game (F2's Salir a Windows; pending settings are saved) */
        static int a_antes;
        uint32_t w = s_wh_obj;
        int a = n->Gamepad.bAnalogButtons[XBOX_BUTTON_A] > 30;
        if (!s_caja && w && s_wh_n >= 2 && t - s_wh_at < 150u && MEM32(w + 0x6C) == 0 && MEM32(w + 0x9C) == 0
                && (int)MEM32(w + 0xA0) == s_wh_n - 1 && a) {
            n->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = 0;
            if (!a_antes) {
                /* the box: its texts ours, then B opens it */
                caja_textos(1);
                s_caja = 1;
                s_caja_at = t;
                s_caja_w = w;
                fprintf(stderr, "[RATON] Salir a Windows: el cartel\n");
                fflush(stderr);
            }
        }
        if (s_caja) {
            uint32_t st = MEM32(s_caja_w + 0x6C), box = MEM32(s_caja_w + 0x64);
            if (s_caja == 1) {
                if (st == 5u) {
                    caja_textos(0);             /* read by the box already: the game's again */
                    s_caja = 2;
                } else if (t - s_caja_at < 120u) {
                    n->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = 0;
                    n->Gamepad.bAnalogButtons[XBOX_BUTTON_B] = 255;
                } else if (t - s_caja_at > 1500u) {
                    caja_textos(0);             /* never opened */
                    s_caja = 0;
                }
            } else if (st != 5u) {
                s_caja = 0;                     /* No / B: closed */
            } else if (a && !a_antes && box && MEM32(box + 0xBD8u) == 0u) {
                HWND hw = FindWindowA("RecompNv2aD3D11", NULL);
                n->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = 0;
                fprintf(stderr, "[RATON] Salir a Windows: Sí\n");
                fflush(stderr);
                if (hw)
                    PostMessageA(hw, WM_CLOSE, 0, 0);
                else
                    ExitProcess(0);
            } else if (a && box && MEM32(box + 0xBD8u) == 0u)
                n->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = 0;   /* never the game's "to the title" */
        }
        a_antes = a;
    }
}

/* ---- settings ------------------------------------------------------------ */

static void apply_capture(int mode)
{
    xbox_mouse_opts o;
    xbox_InputMouseGetOpts(&o);
    o.capture = mode == MODE_POINTER ? XBOX_MOUSE_CAPTURE_NEVER : XBOX_MOUSE_CAPTURE_FOCUS;
    xbox_InputMouseOpts(&o);
}

void ygo_mouse_mode_set(int mode)
{
    if (mode < MODE_POINTER || mode > MODE_CLICKS)
        mode = MODE_POINTER;
    s_mode = mode;
    s_held = 0;
    apply_capture(mode);
    if (mode != MODE_POINTER)
        glove_cursor(0);                /* the arrow back */
    if (mode == MODE_POINTER) {
        static int started;
        if (!started) {
            HANDLE th = CreateThread(NULL, 0, wheel_thread, NULL, 0, NULL);
            if (th)
                CloseHandle(th);
            started = 1;
        }
    }
}
int  ygo_mouse_mode_get(void) { return s_mode; }
void ygo_mouse_step_set(int counts) { if (counts > 0) s_step = counts; }
