/*
 * onda.c - a ring that spreads from the pointer at each left click, as the
 * PC Yu-Gi-Oh games do (the user's video of Power of Chaos, 2026-11-10: a
 * thin light ring at the fingertip, growing about five times and fading
 * within a fifth of a second, left where the click was while the hand moves
 * on).
 *
 * Drawn in a small layered window of its own (per-pixel alpha,
 * UpdateLayeredWindow), topmost, click-through and never focused: the game's picture is not touched, and
 * it works in every mouse mode. One thread: it watches the left button
 * (GetAsyncKeyState, so the game's own input path is not involved), and on a
 * press over the game window's client area, the game in front and the F2
 * menu closed, plays the ring. A new click restarts it at the new place.
 *
 * Its size follows the game's: the game draws its glove 64 lines tall in a
 * 480-line picture, and the ring ends at RING_END of that.
 *
 * F2 "Onda al hacer clic" (onda_clic in ygo_menu.ini): Sí (the default) / No.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "capas.h"   /* the recording mode: panels into the picture */

int xbox_MenuIsOpen(void);
/* kit nv2a_pb_d3d11.c: a client point in guest px; *dev device px a guest px */
int nv2a_d3d11_window_to_guest(int px, int py, int cw, int ch, float *gx, float *gy, float *dev);

#define DURATION_MS 220.0
#define RING_START 0.12     /* of the glove's size: the ring's first radius */
#define RING_END   0.42     /* and its last */

static volatile LONG s_on = 1;

int ygo_onda_get(void) { return s_on ? 0 : 1; }
void ygo_onda_set(int i) { InterlockedExchange(&s_on, i == 0); }

static HWND game_window(void)
{
    return FindWindowA("RecompNv2aD3D11", NULL);
}

/* The ring at radius r (pixels), stroke w, opacity a (0..1), centred in a
 * size x size premultiplied BGRA picture: white, with a faint dark edge
 * outside so it shows on white too. Anti-aliased by distance. */
static void draw_ring(unsigned char *px, int size, double r, double w, double a)
{
    double c = (size - 1) / 2.0;
    int x, y;
    for (y = 0; y < size; y++)
        for (x = 0; x < size; x++) {
            double d = sqrt((x - c) * (x - c) + (y - c) * (y - c));
            double ring = w / 2 + 0.5 - fabs(d - r);
            double edge = w / 2 + 0.5 - fabs(d - (r + w));
            double ca, ea, al, lum;
            unsigned char *p = px + 4 * (y * size + x);
            ca = ring < 0 ? 0 : ring > 1 ? 1 : ring;
            ea = edge < 0 ? 0 : edge > 1 ? 1 : edge;
            ca *= 0.85 * a;
            ea *= 0.30 * a;
            al = ca + ea * (1 - ca);
            lum = 255.0 * ca;                 /* white ring over a black edge */
            p[0] = p[1] = p[2] = (unsigned char)(lum + 0.5);
            p[3] = (unsigned char)(al * 255.0 + 0.5);
        }
}

static DWORD WINAPI onda_thread(LPVOID unused)
{
    WNDCLASSA wc;
    HWND win = NULL, owner = NULL;
    HDC screen, mem;
    HBITMAP bmp = NULL, old = NULL;
    unsigned char *bits = NULL;
    int size = 0, was = 0, playing = 0;
    POINT at = { 0, 0 };
    LARGE_INTEGER freq, t0;
    double glove = 64;

    (void)unused;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "YgoOndaClic";
    RegisterClassA(&wc);
    QueryPerformanceFrequency(&freq);
    screen = GetDC(NULL);
    mem = CreateCompatibleDC(screen);

    for (;;) {
        MSG m;
        int down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        HWND g = game_window();

        while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE))
            DispatchMessageA(&m);

        if (down && !was && s_on && g && GetForegroundWindow() == g && !xbox_MenuIsOpen()) {
            RECT cr;
            POINT p, tl = { 0, 0 };
            GetCursorPos(&p);
            GetClientRect(g, &cr);
            ClientToScreen(g, &tl);
            if (p.x >= tl.x && p.y >= tl.y && p.x < tl.x + cr.right && p.y < tl.y + cr.bottom
                && cr.bottom > 0) {
                int want;
                float gx, gy, dev = 0.f;
                /* the glove's size where the kit really shows the frame
                 * (any aspect, integer scaling, overscan); before the
                 * first frame, the window's height */
                if (nv2a_d3d11_window_to_guest(p.x - tl.x, p.y - tl.y, cr.right, cr.bottom,
                                               &gx, &gy, &dev) >= 0 && dev > 0.f)
                    glove = 64.0 * dev;
                else
                    glove = cr.bottom * 64.0 / 480.0;
                want = (int)(2 * (glove * RING_END + glove / 20 + 4)) | 1;
                /* Topmost, not owned by the game window: an owner on another
                 * thread would tie the two threads' input together. It shows
                 * only for DURATION_MS after a click on the game in front. */
                owner = g;
                if (!win)
                    win = CreateWindowExA(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE
                                          | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                                          wc.lpszClassName, "", WS_POPUP, 0, 0, 1, 1,
                                          NULL, NULL, wc.hInstance, NULL);
                if (want != size) {
                    BITMAPINFO bi;
                    if (bmp) {
                        SelectObject(mem, old);
                        DeleteObject(bmp);
                    }
                    memset(&bi, 0, sizeof bi);
                    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
                    bi.bmiHeader.biWidth = want;
                    bi.bmiHeader.biHeight = -want;
                    bi.bmiHeader.biPlanes = 1;
                    bi.bmiHeader.biBitCount = 32;
                    bi.bmiHeader.biCompression = BI_RGB;
                    bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
                    old = (HBITMAP)SelectObject(mem, bmp);
                    size = bmp ? want : 0;
                }
                at = p;
                playing = win && bmp;
                QueryPerformanceCounter(&t0);
            }
        }
        was = down;

        if (playing) {
            LARGE_INTEGER now;
            double t, e, r, w;
            POINT dst, src = { 0, 0 };
            SIZE sz;
            BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
            QueryPerformanceCounter(&now);
            t = (double)(now.QuadPart - t0.QuadPart) * 1000.0 / (double)freq.QuadPart / DURATION_MS;
            if (t >= 1.0 || !IsWindow(owner)) {
                ShowWindow(win, SW_HIDE);
                playing = 0;
            } else {
                e = 1 - (1 - t) * (1 - t);              /* fast, then slowing */
                r = glove * (RING_START + (RING_END - RING_START) * e);
                w = glove / 32.0 < 1.5 ? 1.5 : glove / 32.0;
                draw_ring(bits, size, r, w, 1 - t);
                GdiFlush();
                dst.x = at.x - size / 2;
                dst.y = at.y - size / 2;
                sz.cx = sz.cy = size;
                UpdateLayeredWindow(win, screen, &dst, &sz, mem, &src, 0, &bf, ULW_ALPHA);
                if (!IsWindowVisible(win))
                    SetWindowPos(win, HWND_TOPMOST, 0, 0, 0, 0,
                                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
            }
        }
        Sleep(playing ? 8 : 10);
    }
}

void ygo_onda_start(void)
{
    static int started;
    if (started)
        return;
    started = 1;
    CloseHandle(CreateThread(NULL, 0, onda_thread, NULL, 0, NULL));
}
