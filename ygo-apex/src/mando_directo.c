/*
 * mando_directo.c - the game's XInputGetState reads the host directly.
 *
 * ygo-port (from RDR; the same XDK 5849 code, found by its bytes in XPP).
 * The game asks for its controller with the XDK's XInputGetState
 * (sub_0041C323, stdcall: handle, XINPUT_STATE *; returns 0, or 0x48F when
 * the device is gone). That copies the last report the emulated USB pad
 * delivered (usb/ohci.c, usb_gamepad.c). On a slow PC (4 cores, hard disk)
 * the USB side stopped delivering after a minute of play: the game kept the
 * last report for good (walking forward, nothing else answered; the camera
 * still turned, its mouse read is mouselook.c's own), and once the call read
 * a device structure full of 0xCC and crashed. So the state comes from the
 * host here, the same one the emulated pad reported (xbox_InputGetState(0):
 * keyboard, mouse and host pads merged), and the USB pad keeps only the
 * enumeration the game expects (usb_gamepad_host_direct: it reports nothing
 * and polls nothing).
 *
 * The translated function is renamed sub_0041C323_pad in
 * recomp/gen/recomp_0142.c (tools/rename_pads.py). YGO_MANDO_USB=1 goes back
 * to it, for comparison.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "recomp/gen/recomp_types.h"
#include "input/xinput_xbox.h"

void sub_0041C323_pad(void);
void usb_gamepad_host_direct(int on);

#define ERR_NOT_CONNECTED 0x48Fu

static SRWLOCK s_lock = SRWLOCK_INIT;
static XBOX_INPUT_STATE s_last;
static LONGLONG s_last_at, s_freq;

/* ygo-port: the mouse's part of the pad (raton.c). */
void ygo_mouse_apply(XBOX_INPUT_STATE *n);
void ygo_caja_tick(unsigned botones);   /* caja_negra.c */

/* One host read per 4 ms: the game asks from two places a frame, and every
 * read takes the mouse motion and moves the keyboard's stick ramp. */
static void host_state(XBOX_INPUT_STATE *st)
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    AcquireSRWLockExclusive(&s_lock);
    if (!s_freq) {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        s_freq = f.QuadPart;
    }
    if (!s_last_at || now.QuadPart - s_last_at >= s_freq / 250) {
        XBOX_INPUT_STATE n;
        memset(&n, 0, sizeof n);
        if (xbox_InputGetState(0, &n) != 0) {
            /* Nothing on the host: the pad is still plugged, at rest. */
            memset(&n.Gamepad, 0, sizeof n.Gamepad);
            n.dwPacketNumber = s_last.dwPacketNumber;
        }
        ygo_mouse_apply(&n);
        {   /* 2026-11-24: the duel's black box (caja_negra.c), with what goes to the game */
            unsigned b = n.Gamepad.wButtons, i;
            for (i = 0; i < 8; ++i)
                if (n.Gamepad.bAnalogButtons[i] > 30)
                    b |= 0x10000u << i;
            ygo_caja_tick(b);
        }
        s_last = n;
        s_last_at = now.QuadPart;
    }
    *st = s_last;
    ReleaseSRWLockExclusive(&s_lock);
}

void ygo_voz_botones(int a_or_b);
void ygo_fundas_cuadro(void);            /* fundas.c: the loaned deck's sleeve out of the duel */

void sub_0041C323(void)
{
    static int mode = -1;               /* 1 direct, 0 the USB pad */
    uint32_t handle = MEM32(g_esp + 4), out = MEM32(g_esp + 8);
    XBOX_INPUT_STATE st;
    int i;

    if (mode < 0) {
        const char *e = getenv("YGO_MANDO_USB");
        mode = !(e && *e == '1');
        usb_gamepad_host_direct(mode);
        fprintf(stderr, "[INPUT] game controller read: %s\n",
                mode ? "host state (direct)" : "emulated USB pad");
        fflush(stderr);
    }
    ygo_fundas_cuadro();
    if (!mode) {
        sub_0041C323_pad();
        return;
    }
    if (!handle || !out) {
        g_eax = ERR_NOT_CONNECTED;
        g_esp += 12;                    /* ret 8 */
        return;
    }
    host_state(&st);
    /* voces.c: A or B skips the line, its voice stops */
    ygo_voz_botones(st.Gamepad.bAnalogButtons[0] > 30 || st.Gamepad.bAnalogButtons[1] > 30);
    MEM32(out) = st.dwPacketNumber;
    MEM16(out + 4) = st.Gamepad.wButtons;
    for (i = 0; i < 8; i++)
        MEM8(out + 6 + i) = st.Gamepad.bAnalogButtons[i];
    MEM16(out + 14) = (uint16_t)st.Gamepad.sThumbLX;
    MEM16(out + 16) = (uint16_t)st.Gamepad.sThumbLY;
    MEM16(out + 18) = (uint16_t)st.Gamepad.sThumbRX;
    MEM16(out + 20) = (uint16_t)st.Gamepad.sThumbRY;
    g_eax = 0;
    g_esp += 12;                        /* ret 8 */
}
