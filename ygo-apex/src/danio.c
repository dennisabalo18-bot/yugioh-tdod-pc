/* El número de daño (idea 5 de Master Duel, 2026-11-15; el usuario: "tiene
 * que parecer parte del juego").
 *
 * Cuando bajan (o suben) los PV de un jugador, arriba de su barra de PV sale
 * "-1500" (o "+500") con los mismos números del juego: la tira "0123456789-+"
 * del atlas del duelo, la que dibuja los PV ("8ooo"), pegada a la barra de PV
 * en el atlas (tools/danio_digitos.py -> danio_digitos.h). Lo dibuja el kit
 * sobre la imagen final (nv2a_pb_d3d11.c dano()). Rojo anaranjado al perder,
 * verde al ganar.
 *
 * Va con el juego (2026-11-15 18:3x, el usuario: "en 120 fps baja rapidísimo
 * donde tiene la vida y nuestro número se queda mucho tiempo ... que vaya con
 * el juego"):
 * - todo se cuenta en cuadros del juego (este tick corre una vez por cuadro
 *   del duelo), no en segundos: el juego avanza por cuadro, a 120 fps su
 *   contador baja el doble de rápido y el número también;
 * - el número dura lo que tarda el contador del juego: al primer cambio de PV
 *   de cada duelo se busca en la memoria del juego el valor que va de los PV
 *   viejos a los nuevos de a poco (el que muestra el contador); hallado, el
 *   número queda mientras ese valor no llegó a los PV y se va apenas llega.
 *   Sin hallar todavía, CUADROS_SIN_CONTADOR.
 *
 * PV: u32 en 0x65D0A4 + jugador * 0x13F8 (cartas.c; jugador 0 = vos). Estado
 * del duelo [0x657148]: 6 el duelo, 7 el resultado (mascota.c). Se llama en
 * cada cuadro de la escena del duelo (mascota.c, sub_00056FC0).
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "recomp/gen/recomp_types.h"
#include "danio_digitos.h"

extern volatile float g_nv2a_dano[8][8];   /* 0, 1 the damage; 2..7 the chain numbers (cadena.c) */
extern const unsigned char *volatile g_nv2a_dano_px;
extern volatile int g_nv2a_dano_w, g_nv2a_dano_h;
extern const int (*volatile g_nv2a_dano_caja)[2];

#define VIDA_DIR(p) (0x0065D0A4u + (uint32_t)(p) * 0x13F8u)
#define VIDA(p) MEM32(VIDA_DIR(p))

/* in game frames (the game's own pace: 60 a second at 60 fps) */
#define ENTRA 10                        /* comes in big and settles */
#define SALE 18                         /* rises a little and fades */
#define CUADROS_SIN_CONTADOR 50         /* shown this long while the counter is not known */
#define SUMA 15                         /* a second change this soon after: one number */
#define BUSCA_MAX 900                   /* gives up looking for the counter */

/* el medio del número, en el cuadro de 640x480: justo arriba de los números
 * de cada barra de PV (captura 2026-11-15: la tuya columnas 177..257, filas
 * 191..218; la del rival 383..463, 176..202) */
static const float k_lugar[2][2] = { { 217.f, 174.f }, { 423.f, 162.f } };

static volatile LONG s_on = 1;
int ygo_danio_get(void) { return s_on ? 0 : 1; }
void ygo_danio_set(int i) { InterlockedExchange(&s_on, i == 1 ? 0 : 1); }

static struct {
    uint32_t vida;
    int valida;
    int activo;                         /* a number on screen */
    unsigned cuadro;                    /* frames since it came */
    int valor;
    int sale;                           /* frames into its way out (0: not yet) */
} s_j[2];

/* the game's counter (the shown life points) per player: an address of the
 * game's memory, as u32 (tipo 0) or float (tipo 1); 0 not known */
static struct { uint32_t dir; int tipo; } s_cont[2];

#define CAND_MAX 4096
static struct {
    int p;                              /* player being looked for; -1 none */
    uint32_t viejo, nuevo;
    unsigned cuadros;
    int n;
    uint32_t dir[CAND_MAX];
    unsigned char tipo[CAND_MAX], movio[CAND_MAX];
} s_busca = { -1 };

void ygo_danio_batalla(void) { }        /* mascota.c still calls it: nothing now */

/* sub_000C8BC0(p, n): lose n life points (it queues order 0x27; battle damage
 * and card effects). 2026-11-15, the user: on the finishing hit (500 left, a
 * 1600 attack) the number said -500. As Master Duel, the hit's whole damage:
 * when the life points end at 0, the last asked amount if it was more. */
void sub_000C8BC0_pad(void);
static uint32_t s_pedido[2];
void sub_000C8BC0(void)
{
    uint32_t p = MEM32(g_esp + 4) & 1u, n = MEM32(g_esp + 8);
    if (n < 100000u)
        s_pedido[p] = n;
    sub_000C8BC0_pad();
}

void ygo_danio_limpiar(void)
{
    memset(s_j, 0, sizeof s_j);
    memset((void *)g_nv2a_dano, 0, 2 * sizeof g_nv2a_dano[0]);   /* the damage ones; 2..7 are cadena.c's */
    memset(s_pedido, 0, sizeof s_pedido);
    s_busca.p = -1;
    /* s_cont stays: checked again at each change */
}

static float suave(float x)
{
    x = x < 0.f ? 0.f : x > 1.f ? 1.f : x;
    return x * x * (3.f - 2.f * x);
}

static int leer(uint32_t dir, int tipo, double *v)
{
    uint32_t b = MEM32(dir);
    if (tipo) {
        float f;
        memcpy(&f, &b, 4);
        if (f != f)
            return 0;
        *v = f;
    } else {
        *v = (double)(int32_t)b;
    }
    return 1;
}

/* every u32 / float of the game's memory equal to the old life points: the
 * candidates for its counter (one pass, at the first change of a duel) */
static void buscar(int p, uint32_t viejo, uint32_t nuevo)
{
    uintptr_t base = (uintptr_t)XBOX_PTR(0), a = base + 0x10000u, fin = base + 0x04000000u;   /* the 64 MB of RAM */
    float fv = (float)viejo;
    uint32_t fb;
    LARGE_INTEGER t0, t1, qf;
    memcpy(&fb, &fv, 4);
    QueryPerformanceCounter(&t0);
    s_busca.p = p;
    s_busca.viejo = viejo;
    s_busca.nuevo = nuevo;
    s_busca.cuadros = 0;
    s_busca.n = 0;
    while (a < fin && s_busca.n < CAND_MAX) {
        MEMORY_BASIC_INFORMATION mi;
        if (!VirtualQuery((void *)a, &mi, sizeof mi))
            break;
        if (mi.State == MEM_COMMIT && !(mi.Protect & (PAGE_NOACCESS | PAGE_GUARD))
                && (mi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY))) {
            const uint32_t *w = (const uint32_t *)mi.BaseAddress;
            size_t k, n = mi.RegionSize / 4u;
            for (k = 0; k < n && s_busca.n < CAND_MAX; k++) {
                if (w[k] == viejo || w[k] == fb) {
                    uint32_t g = (uint32_t)((uintptr_t)&w[k] - base);
                    if (g == VIDA_DIR(0) || g == VIDA_DIR(1))
                        continue;
                    s_busca.dir[s_busca.n] = g;
                    s_busca.tipo[s_busca.n] = w[k] != viejo;
                    s_busca.movio[s_busca.n] = 0;
                    s_busca.n++;
                }
            }
        }
        a = (uintptr_t)mi.BaseAddress + mi.RegionSize;
    }
    QueryPerformanceCounter(&t1);
    QueryPerformanceFrequency(&qf);
    fprintf(stderr, "[DANIO] buscando el contador del jugador %d (%u -> %u): %d candidatos en %.1f ms\n", p,
            viejo, nuevo, s_busca.n, (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)qf.QuadPart);
}

/* each frame while looking: a candidate that goes from the old life points to
 * the new ones through values in between is the counter */
static void seguir_busqueda(void)
{
    int i, vivos = 0;
    double lo, hi;
    if (s_busca.p < 0)
        return;
    lo = s_busca.viejo < s_busca.nuevo ? s_busca.viejo : s_busca.nuevo;
    hi = s_busca.viejo < s_busca.nuevo ? s_busca.nuevo : s_busca.viejo;
    for (i = 0; i < s_busca.n; i++) {
        double v;
        if (!s_busca.dir[i])
            continue;
        if (!leer(s_busca.dir[i], s_busca.tipo[i], &v) || v < lo - 0.5 || v > hi + 0.5) {
            s_busca.dir[i] = 0;         /* not it */
            continue;
        }
        if (fabs(v - (double)s_busca.nuevo) < 0.5) {
            if (s_busca.movio[i]) {
                int p = s_busca.p;
                s_cont[p].dir = s_busca.dir[i];
                s_cont[p].tipo = s_busca.tipo[i];
                fprintf(stderr, "[DANIO] contador del jugador %d: %08X (%s), llegó en %u cuadros\n", p,
                        s_cont[p].dir, s_cont[p].tipo ? "float" : "u32", s_busca.cuadros);
                s_busca.p = -1;
                return;
            }
            s_busca.dir[i] = 0;         /* jumped straight there: a copy, not the counter */
            continue;
        }
        if (fabs(v - (double)s_busca.viejo) >= 0.5)
            s_busca.movio[i] = 1;
        vivos++;
    }
    if (!vivos || ++s_busca.cuadros > BUSCA_MAX) {
        fprintf(stderr, "[DANIO] contador del jugador %d no hallado\n", s_busca.p);
        s_busca.p = -1;
    }
}

/* the counter still on its way to the life points */
static int contando(int p)
{
    double v;
    if (!s_cont[p].dir || !leer(s_cont[p].dir, s_cont[p].tipo, &v))
        return -1;                      /* unknown */
    return fabs(v - (double)VIDA(p)) >= 0.5;
}

void ygo_danio_tick(void)
{
    uint32_t e = MEM32(0x00657148u);
    int p;
    if (!g_nv2a_dano_px) {
        g_nv2a_dano_w = DANIO_W;
        g_nv2a_dano_h = DANIO_H;
        g_nv2a_dano_caja = k_danio_caja;
        g_nv2a_dano_px = k_danio_px;
    }
    seguir_busqueda();
    for (p = 0; p < 2; p++) {
        uint32_t v = VIDA(p);
        if (e != 6u && e != 7u) {
            s_j[p].valida = 0;          /* the opening sets them: no number */
        } else if (!s_j[p].valida || !s_j[p].vida) {
            /* 2026-11-15 17:02 log: at state 6 the life points are still 0,
             * then set to 8000 ("+8000" on both): that is not a change */
            s_j[p].vida = v;
            s_j[p].valida = 1;
        } else if (v != s_j[p].vida) {
            int d = (int)v - (int)s_j[p].vida;
            uint32_t viejo = s_j[p].vida;
            s_j[p].vida = v;
            if (d > -100000 && d < 100000 && s_on) {
                if (d < 0 && v == 0u && s_pedido[p] > (uint32_t)-d) {
                    fprintf(stderr, "[DANIO] jugador %d: le quedaban %d, el golpe era de %u\n", p, -d, s_pedido[p]);
                    d = -(int)s_pedido[p];
                }
                if (d < 0)
                    s_pedido[p] = 0;
                if (s_j[p].activo && !s_j[p].sale && s_j[p].cuadro < SUMA && (d < 0) == (s_j[p].valor < 0)) {
                    s_j[p].valor += d;
                } else {
                    s_j[p].valor = d;
                    s_j[p].activo = 1;
                    s_j[p].cuadro = 0;
                    s_j[p].sale = 0;
                }
                /* the counter: known and still right (it showed the old life
                 * points), or looked for */
                if (s_cont[p].dir) {
                    double c;
                    if (!leer(s_cont[p].dir, s_cont[p].tipo, &c) || fabs(c - (double)viejo) >= 0.5)
                        s_cont[p].dir = 0;
                }
                /* 2026-11-21 (the user: "se tranca ... al atacar"): no more
                 * search. Each one read the game's 64 MB in 12-35 ms, at every
                 * life change, and it never found the counter (all logs, runs/
                 * included: "no hallado"): the number always went by its 50
                 * frames, which is how the user approved it. */
                (void)buscar;
                fprintf(stderr, "[DANIO] jugador %d: %+d (PV %u)\n", p, d, v);
            }
        }
        /* its frame */
        if (s_j[p].activo && s_on) {
            volatile float *n = g_nv2a_dano[p];
            unsigned c = ++s_j[p].cuadro;
            float esc = 1.f, alfa = 1.f, sube = (float)(c < 60u ? c : 60u) * 0.06f;
            if (!s_j[p].sale && c > ENTRA) {
                int k = contando(p);
                if (k == 0 || (k < 0 && c >= CUADROS_SIN_CONTADOR))
                    s_j[p].sale = 1;    /* the game's counter got there: it goes */
            }
            if (s_j[p].sale) {
                float u = (float)s_j[p].sale / (float)SALE;
                if (s_j[p].sale++ > SALE) {
                    s_j[p].activo = 0;
                    n[3] = 0.f;
                    continue;
                }
                sube += 10.f * suave(u);
                alfa = 1.f - suave(u);
            }
            if (c < ENTRA) {
                float u = (float)c / (float)ENTRA;
                esc = 1.f + 0.6f * (1.f - u) * (1.f - u);   /* comes in big, settles */
                alfa *= u;
            }
            n[0] = k_lugar[p][0];
            n[1] = k_lugar[p][1] - sube;
            n[2] = esc;
            n[4] = (float)s_j[p].valor;
            if (s_j[p].valor < 0) {
                n[5] = 1.f; n[6] = 0.42f; n[7] = 0.26f;     /* lost: red-orange */
            } else {
                n[5] = 0.55f; n[6] = 1.f; n[7] = 0.55f;     /* gained: green */
            }
            n[3] = alfa;                /* last: the kit draws from it */
        } else {
            g_nv2a_dano[p][3] = 0.f;
        }
    }
}
