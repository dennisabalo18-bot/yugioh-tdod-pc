/*
 * ygo-port: la vibracion del control sin la pila USB de la Xbox (XPP).
 *
 * sub_001FE410 (thiscall, ecx = el objeto del control del juego; args:
 * motor 0/1, float 0..1; ret 8) guarda el valor del motor en +0x2C / +0x30
 * y, si el pedido anterior termino (XINPUT_FEEDBACK en +0x62: dwStatus !=
 * ERROR_IO_PENDING 0x3E5), pone los motores en +0xA4 / +0xA6 (x 65535) y
 * llama XInputSetState (0x41C396) con el handle de +0x44 (XInputOpen del
 * puerto +0x48, 0x1FF560).
 *
 * 2026-11-11: dos cierres (0xC0000005) al terminar un duelo ("Siguiente
 * oponente"), dentro de XPP (sub_00422B22, la cola de transferencias USB en
 * memoria contigua 0x80E0xxxx, con eax = -1) bajo XInputSetState. Aca la
 * vibracion va directo al control (usb_gamepad_rumble, lo mismo que hace el
 * OHCI emulado cuando le llega el pedido) y el pedido queda terminado al
 * instante: XPP no interviene. RECOMP_RUMBLE=0 la apaga, como en ohci.c.
 *
 * recomp's definition is renamed _pad (tools/rename_pads.py).
 */
#include <stdlib.h>
#include <string.h>
#include "recomp/gen/recomp_types.h"

void usb_gamepad_rumble(int pad, uint16_t left, uint16_t right);

static uint16_t motor(uint32_t bits)
{
    float f;
    memcpy(&f, &bits, sizeof f);
    f *= 65535.0f;                      /* el juego: fmul [0x457974] y _ftol */
    if (!(f > 0.0f))
        return 0;
    if (f >= 65535.0f)
        return 65535;
    return (uint16_t)(int32_t)f;
}

void sub_001FE410(void)
{
    static int activa = -1;
    uint32_t o = g_ecx, cual = MEM32(g_esp + 4), valor = MEM32(g_esp + 8);
    if (activa < 0) {
        const char *e = getenv("RECOMP_RUMBLE");
        activa = !(e && *e == '0');
    }
    if (MEM32(o + 0x44)) {
        if (cual == 0)
            MEM32(o + 0x2C) = valor;
        else if (cual == 1)
            MEM32(o + 0x30) = valor;
        if (MEM32(o + 0x62) != 0x3E5u) {
            uint16_t izq = motor(MEM32(o + 0x2C)), der = motor(MEM32(o + 0x30));
            MEM16(o + 0xA4) = izq;
            MEM16(o + 0xA6) = der;
            MEM32(o + 0x66) = 0;        /* hEvent */
            MEM32(o + 0x62) = 0;        /* dwStatus: ERROR_SUCCESS, terminado */
            if (activa)
                usb_gamepad_rumble((int)(MEM32(o + 0x48) & 3u), izq, der);
        }
    }
    g_eax = 0;
    g_esp += 12;                        /* ret 8 */
}
