/*
 * diag_escala.c - registro de la escala y la posicion de los objetos 3D
 * (2026-11-13, para el visor de modelos: el juego agranda o achica cada
 * monstruo de la batalla y no se encontro la regla leyendo el codigo).
 *
 * Alchemy arma la matriz de un objeto asi: sub_001E55B0 identidad,
 * sub_001F13C0 rotacion (x, y, z), sub_001E62D0 escala (ecx = matriz, arg =
 * puntero a x, y, z), sub_001E6060 traslacion. Aca se envuelve la escala; el
 * objeto esta en esi y segun quien llama:
 *   0x4A9B8 (sub_0004A950): rotacion en grados +0x20, escala +0x2C, posicion +0x14
 *   0x5BC41 (sub_0005BC10): rotacion +0x20, escala +0x2C, posicion +0x14
 *   0x69031 (sub_00068FE0): rotacion en grados +0x50, escala +0x84, posicion +0x1C
 *     (muchos objetos: solo se anota durante una batalla)
 * Una entrada por objeto; se anota cuando cambia (escala 0.02, posicion 0.5,
 * rotacion 1). Al empezar cada batalla ([0x658524] +0x18 atacante, +0x1C
 * atacado) se anotan sus primeras 6 camaras ([0x658524]+0x20+i*0x4C, 19
 * floats). Tope de 6000 lineas por partida.
 * Apagado salvo con la variable de entorno YGO_DIAG_ESCALA=1.
 *
 * Resultado (2 duelos del usuario, 2026-11-13): ninguna escala por monstruo;
 * las 6 camaras de la batalla son fijas (las mismas para cualquier modelo):
 * 19 floats = 0, posicion A, destino A, arriba A, posicion B, destino B,
 * arriba B (la camara va de A a B). Camara 0: (-13.03, 24.16, 34.84) ->
 * (8.20, 24.46, 2.17) hasta (-7.41, 27.13, 10.34) -> (0.95, 27.25, -2.52).
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "recomp/gen/recomp_types.h"

void sub_001E62D0_pad(void);

#define N_OBJ 256
static struct { uint32_t ret, obj; float v[9]; } s_vistos[N_OBJ];
static int s_lineas, s_prox;
static uint32_t s_batalla = 0xFFFFFFFFu;

static int distinto(const float *a, const float *b)
{
    static const float tol[9] = { .02f, .02f, .02f, .5f, .5f, .5f, 1.f, 1.f, 1.f };
    int i;
    for (i = 0; i < 9; ++i) {
        float d = a[i] - b[i];
        if (d > tol[i] || d < -tol[i])
            return 1;
    }
    return 0;
}

static void camaras(uint32_t b, int ma, int md)
{
    int i, j;
    fprintf(stderr, "[ESCALA] batalla: modelo atacante %d, atacado %d\n", ma, md);
    for (i = 0; i < 6; ++i) {
        uint32_t c = b + 0x20u + (uint32_t)i * 0x4Cu;
        fprintf(stderr, "[ESCALA]   camara %d:", i);
        for (j = 0; j < 19; ++j)
            fprintf(stderr, " %.3f", MEMF(c + (uint32_t)j * 4u));
        fprintf(stderr, "\n");
    }
    s_lineas += 7;
}

void sub_001E62D0(void)
{
    uint32_t ret = MEM32(g_esp), vec = MEM32(g_esp + 4), obj = g_esi, i, libre = N_OBJ;
    uint32_t b, orot = 0, oesc = 0, opos = 0;
    int ma = -2, md = -2;
    float v[9] = { 0 };
    static int activo = -1;
    sub_001E62D0_pad();
    if (activo < 0) {
        const char *e = getenv("YGO_DIAG_ESCALA");
        activo = e && *e == '1';
    }
    if (!activo || s_lineas >= 6000 || !vec)
        return;
    b = MEM32(0x00658524u);
    if (b) {
        ma = (int)MEM32(b + 0x18);
        md = (int)MEM32(b + 0x1C);
    }
    if (ret == 0x0004A9B8u || ret == 0x0005BC41u) {
        orot = 0x20; oesc = 0x2C; opos = 0x14;
    } else if (ret == 0x00069031u) {
        if (ma < 0 || ma == 0xFFFF)
            return;                     /* fuera de batalla: particulas */
        orot = 0x50; oesc = 0x84; opos = 0x1C;
    }
    if (b && ma >= 0 && ma != 0xFFFF && (uint32_t)((ma << 16) | (md & 0xFFFF)) != s_batalla) {
        s_batalla = (uint32_t)((ma << 16) | (md & 0xFFFF));
        camaras(b, ma, md);
    } else if (b && (ma < 0 || ma == 0xFFFF))
        s_batalla = 0xFFFFFFFFu;
    v[0] = MEMF(vec); v[1] = MEMF(vec + 4); v[2] = MEMF(vec + 8);
    if (oesc && obj) {
        v[3] = MEMF(obj + opos); v[4] = MEMF(obj + opos + 4); v[5] = MEMF(obj + opos + 8);
        v[6] = MEMF(obj + orot); v[7] = MEMF(obj + orot + 4); v[8] = MEMF(obj + orot + 8);
    } else if (v[0] == 1.0f && v[1] == 1.0f && v[2] == 1.0f)
        return;
    else
        obj = 0;
    for (i = 0; i < N_OBJ; ++i) {
        if (s_vistos[i].ret == ret && s_vistos[i].obj == obj) {
            if (!distinto(s_vistos[i].v, v))
                return;
            break;
        }
        if (!s_vistos[i].ret && libre == N_OBJ)
            libre = i;
    }
    if (i == N_OBJ)
        i = libre < N_OBJ ? libre : (uint32_t)(s_prox++ % N_OBJ);
    s_vistos[i].ret = ret;
    s_vistos[i].obj = obj;
    memcpy(s_vistos[i].v, v, sizeof v);
    if (obj)
        fprintf(stderr, "[ESCALA] desde %08X obj %08X escala %.3f %.3f %.3f pos %.2f %.2f %.2f rot %.1f %.1f %.1f"
                " (batalla %d y %d)\n", ret, obj, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], ma, md);
    else
        fprintf(stderr, "[ESCALA] desde %08X escala %.3f %.3f %.3f (batalla %d y %d)\n",
                ret, v[0], v[1], v[2], ma, md);
    ++s_lineas;
}
