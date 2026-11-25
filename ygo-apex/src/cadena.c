/* Ideas 2 y 8 de Master Duel (2026-11-15, el usuario: "Brillo al activar una
 * mágica o trampa. Cadena numerada trabajemos en estas 2").
 *
 * Medido (registro y video del usuario, 19:44): el juego ya tiene las dos a su
 * manera. Al activar (orden 0x4D, carta, 1) muestra la carta grande en el
 * centro con un círculo mágico (celeste las mágicas, magenta las trampas); con
 * una cadena abre su panel "Chain List" (sub_000D8600). Así que no se suma
 * otro brillo (se vería dos veces), y lo que se agrega es (el usuario: "si,
 * tambien la zona de fusion estaba en ingles"):
 *
 * 1. Los números de la cadena sobre las cartas del campo, con los números del
 *    juego (los de los PV, el pase del número de daño del kit, lugares 2..7),
 *    mientras la cadena tiene 2 eslabones o más. La cadena es la del juego:
 *    0x66BD18, eslabones de 0x14 bytes (+0 u16 carta, +2 bit 0 jugador),
 *    cantidad u16 en 0x66BF98 (lo que sub_00082890 / sub_00082F80 le pasan al
 *    panel por sub_000D8790). Cada número está mientras su carta sigue en su
 *    casilla (0x65D0C8 + p * 0x13F8 + z * 0x9C, u16 +0 carta con el bit 15
 *    del dueño). No se dibujan mientras corre la orden 0x4D (la carta grande:
 *    u8 0x65F90C != 0 es "corriendo", sub_000817D0) ni con el panel abierto.
 *
 * 2. Los títulos de las listas del juego, que estaban en inglés en todos los
 *    idiomas ("Chain List", "Fusion List", "Grave List", "Deck List",
 *    "Exclude List", "Remove List", "Target List"): sub_00047CD0(texto) pone el
 *    título; se cambia el puntero por el traducido. "Card Status" (0x42989C,
 *    lo usan sub_00055CB0 / sub_000560D0) se reescribe en su lugar (11 letras).
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "recomp/gen/recomp_types.h"

uint32_t xbox_HeapAlloc(uint32_t size, uint32_t alignment);
int f2_idioma(void);                    /* f2_idiomas.c: -1 Spanish, 0 en, 1 pt, 2 fr, 3 de, 4 it */
extern volatile float g_nv2a_dano[8][8];   /* kit: 0, 1 the damage (danio.c); 2..7 ours */
extern volatile float g_nv2a_cadena[8][12]; /* kit: chains x0 y0 x1 y1; alpha, drawn, thickness,
                                             * phase; sag, sway */
extern const unsigned char *volatile g_nv2a_cadena_px;
extern volatile int g_nv2a_cadena_w, g_nv2a_cadena_h;
extern volatile float g_nv2a_cadena_cartas[9][8];  /* kit: the cards' boxes hide the chain */
#include "cadena_eslabones.h"               /* tools/cadena_tira.py: the user's chain */

#define CASILLA(p, z) ((uint32_t)(p) * 0x13F8u + (uint32_t)(z) * 0x9Cu + 0x0065D0C8u)
#define CADENA 0x0066BD18u
#define CADENA_N 0x0066BF98u
#define LUGARES 6                       /* chain numbers drawn at once (kit slots 2..7) */
#define ENTRA 8                         /* frames to come in */
#define PANEL_ESPERA 12u                /* the panel slides in first */

static uint32_t s_cuadro;               /* the duel's frames */
static uint32_t s_panel;                /* last frame the chain panel ran */
static uint16_t s_visto_n;

/* ---- the effect machine and messages: passthroughs (the 19:33 log found the
 * spells and traps do not go through them) ---------------------------------- */
void sub_000A1990_pad(void);
void sub_000A1990(void) { sub_000A1990_pad(); }
void sub_000A1A10_pad(void);
void sub_000A1A10(void) { sub_000A1A10_pad(); }
void sub_000A1AA0_pad(void);
void sub_000A1AA0(void) { sub_000A1AA0_pad(); }
/* sub_000C07F0: auto.c (AUTO answers its question) */
void sub_00085720_pad(void);
void sub_00085720(void) { sub_00085720_pad(); }
void sub_00085780_pad(void);
void sub_00085780(void) { sub_00085780_pad(); }
void sub_00081150_pad(void);
void sub_00081150(void) { sub_00081150_pad(); }

/* the "Chain List" panel's step (it runs each frame while it is up) */
void sub_000D8600_pad(void);
static uint32_t s_panel_quieto;         /* frames in its step 1 (up, waiting) */
/* 23:5x (the user: "sale 2 veces la cadena, muestra se va y sale de nuevo"):
 * sub_000D8790(list, flag) opens it from two places, sub_00082890 (flag 0,
 * the chain being built) and sub_00082F80 (flag 1, about to resolve), both
 * with the chain 0x66BD18. The second is let through only when the first did
 * not show this chain: then sub_000D8600 ends at once, as its last step does
 * (sub_00088340, return 1). */
static int s_mostrada, s_saltar;
void sub_00088340(void);
void sub_000D8790_pad(void);
void sub_000D8790(void)
{
    uint32_t lista = MEM32(g_esp + 4u), flag = MEM32(g_esp + 8u) & 1u;
    s_saltar = 0;
    if (lista == CADENA) {
        if (flag == 0u)
            s_mostrada = 1;
        else if (s_mostrada)
            s_saltar = 1;
    }
    fprintf(stderr, "[CADENA] %u: panel %08X flag %u (%u eslabones)%s\n", s_cuadro, lista, flag,
            MEM16(CADENA_N), s_saltar ? " -> no se muestra (ya se vio)" : "");
    sub_000D8790_pad();
}

void sub_000D8600(void)
{
    uint32_t paso = (MEM8(0x0066C17Cu) >> 1) & 0x7Fu;
    if (s_saltar) {
        s_saltar = 0;
        sub_00088340();
        SET_LO16(g_eax, 1);
        return;
    }
    s_panel = s_cuadro ? s_cuadro : 1;
    s_panel_quieto = paso == 1u ? s_panel_quieto + 1u : 0u;
    sub_000D8600_pad();
}

/* ---- the lists' titles ----------------------------------------------------- */
/* es, en, pt, fr, de, it (en: the game's own) */
static const char *const k_titulos[][6] = {
    { "Cadena", "Chain List", "Corrente", "Chaîne", "Kette", "Catena" },
    { "Fusión", "Fusion List", "Fusão", "Fusion", "Fusion", "Fusione" },
    { "Cementerio", "Grave List", "Cemitério", "Cimetière", "Friedhof", "Cimitero" },
    { "Baraja", "Deck List", "Deck", "Deck", "Deck", "Deck" },
    { "Fuera de juego", "Exclude List", "Banidas", "Bannies", "Verbannt", "Bandite" },
    { "Fuera de juego", "Remove List", "Banidas", "Bannies", "Verbannt", "Bandite" },
    { "Objetivos", "Target List", "Alvos", "Cibles", "Ziele", "Bersagli" },
};
#define NTIT (sizeof k_titulos / sizeof k_titulos[0])
static uint32_t s_tit_g[NTIT][6];       /* each in guest memory (1252), made once */

static int idioma(void)
{
    int l = f2_idioma() + 1;            /* 0 es, 1 en, 2 pt, 3 fr, 4 de, 5 it */
    return l < 0 || l > 5 ? 0 : l;
}

static uint32_t a_guest(const char *utf8)
{
    wchar_t w[64];
    char a[128];
    int n;
    uint32_t g;
    if (!MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, 64))
        return 0;
    n = WideCharToMultiByte(1252, 0, w, -1, a, sizeof a, "?", NULL);
    if (n <= 0 || !(g = xbox_HeapAlloc((uint32_t)n, 4)))
        return 0;
    memcpy((void *)XBOX_PTR(g), a, (size_t)n);
    return g;
}

static int s_es_cadena;                 /* the list widget is showing the chain */
static uint32_t s_vacio;                /* "" in guest memory */

void sub_00047CD0_pad(void);
void sub_00047CD0(void)
{
    uint32_t t = MEM32(g_esp + 4);
    int l = idioma();
    s_es_cadena = 0;
    if (t >= 0x00011000u && t < 0x04000000u) {
        char s[16];
        unsigned i;
        for (i = 0; i < sizeof s - 1; i++)
            if (!(s[i] = (char)MEM8(t + i)))
                break;
        s[i] = 0;
        if (!strcmp(s, k_titulos[0][1])) {
            /* the chain shows Master Duel's way (escalera()): no title */
            s_es_cadena = 1;
            if (!s_vacio && (s_vacio = xbox_HeapAlloc(4, 4)) != 0)
                MEM32(s_vacio) = 0;
            if (s_vacio) {
                MEM32(g_esp + 4) = s_vacio;
                sub_00047CD0_pad();
                return;
            }
        }
        for (i = 0; i < NTIT && l != 1; i++)
            if (!strcmp(s, k_titulos[i][1])) {
                if (!s_tit_g[i][l])
                    s_tit_g[i][l] = a_guest(k_titulos[i][l]);
                if (s_tit_g[i][l])
                    MEM32(g_esp + 4) = s_tit_g[i][l];
                break;
            }
    }
    sub_00047CD0_pad();
}

/* "Card Status" in its place (12 bytes there: 11 letters and the end) */
void ygo_cadena_textos(void)
{
    static const char *const k[6] = { "Ver carta", "Card Status", "Ver carta", "Voir carte", "Kartendaten", "Vedi carta" };
    static int hecho = -1;
    int l = idioma(), i;
    char a[12];
    if (l == hecho)
        return;
    if (MEM8(0x0042989Cu) != (uint8_t)'C' && MEM8(0x0042989Cu) != (uint8_t)'V' && MEM8(0x0042989Cu) != (uint8_t)'K')
        return;                         /* not the text we know: leave it */
    memset(a, 0, sizeof a);
    strncpy(a, k[l], 11);
    for (i = 0; i < 12; i++)
        MEM8(0x0042989Cu + (uint32_t)i) = (uint8_t)a[i];
    hecho = l;
}

/* ---- the chain as a staircase (Master Duel) ---------------------------------- */
/* 2026-11-15 (the user: "viste como es que quedan las cartas en grande y se van
 * encadenando"): the panel is the game's list widget ([[0x656F88] + 0x5C],
 * built by sub_000560D0): 9 slots, each a card sprite (+0x678 + 4s, 96 x 139.5),
 * an arrow (+0x69C + 4s) and the gem under it (+0x6C0 + 4s); the title's banner
 * sprite +0x658 (placed once at 140,60). Items: count +0x5B0, cursor +0x5B4;
 * slot s shows item cursor + s - 4 (slot 4 is the middle). sub_00055530 places
 * them each frame (sprite vtable +0x2C (x, y, 0); the place is at +0x20/+0x24;
 * units are the 640x480 frame: the middle card at 320,160, the gems 90 lower).
 * After it, with the chain up: the cards in a staircase in the middle, each
 * link a step right and up over the one before, the newest coming in; the gem
 * on each card's top left corner; no arrows, no banner. */
#define ESC_DX 50.f                     /* a step, frame px */
#define ESC_DY (-12.f)
#define ESC_ENTRA 10u                   /* frames the newest link comes in */
static float s_gema[LUGARES][2];        /* where the numbers go (frame px) */
static int s_gema_ok[LUGARES];
static uint32_t s_esc_id[9], s_esc_desde[9];
static int s_titulo_fuera;

static uint32_t llama(recomp_func_t f, uint32_t self, int n, const uint32_t *a)
{
    uint32_t sp = g_esp, ra = g_eax, rc = g_ecx, rd = g_edx, r;
    int i;
    for (i = n - 1; i >= 0; i--)
        PUSH32(g_esp, a[i]);
    PUSH32(g_esp, 0u);
    g_ecx = self;
    f();
    r = g_eax;
    g_esp = sp;
    g_eax = ra; g_ecx = rc; g_edx = rd;
    return r;
}

static float fl(uint32_t v) { float f; memcpy(&f, &v, 4); return f; }
static uint32_t lf(float f) { uint32_t v; memcpy(&v, &f, 4); return v; }

static void poner(uint32_t spr, float x, float y)
{
    recomp_func_t f;
    uint32_t a[3];
    if (spr < 0x10000u || MEM32(spr) < 0x10000u)
        return;
    if (!(f = recomp_lookup(MEM32(MEM32(spr) + 0x2Cu))))
        return;
    a[0] = lf(x);
    a[1] = lf(y);
    a[2] = 0;
    llama(f, spr, 3, a);
}

/* what sub_00055530 left, read right after it (later the sprites hold ours):
 * each slot's lift over slot 3 (the middle card's bob) and the gem's place
 * from its card's less where it shows (90 lower) */
static float s_sube[9], s_dgx, s_dgy;
static int s_medido;

static void medir(uint32_t w)
{
    uint32_t c3 = MEM32(w + 0x678u + 12u), g3 = MEM32(w + 0x6C0u + 12u), s;
    float y3;
    if (!c3 || !g3)
        return;
    y3 = fl(MEM32(c3 + 0x24u));
    s_dgx = fl(MEM32(g3 + 0x20u)) - fl(MEM32(c3 + 0x20u));
    s_dgy = fl(MEM32(g3 + 0x24u)) - y3 - 90.f;
    for (s = 0; s < 9u; s++) {
        uint32_t carta = MEM32(w + 0x678u + 4u * s);
        s_sube[s] = carta ? fl(MEM32(carta + 0x24u)) - y3 : 0.f;
    }
    s_medido = 1;
}

/* 23:4x, the user's Master Duel video (ssstik.io_1794796920385.mp4): each
 * link is a big card spread over the screen (1 top right, 2 left, 3 bottom
 * right...), the newer over the older, the link's number big and light blue on
 * its bottom right corner; the newest slides in. Here: the cards 1.4 times the
 * game's (sub_000F69A0 (w, h) sets a sprite's size; it grows from the middle),
 * placed in a zigzag down (escalera(); the user: 1 on the left, 2 on the right
 * and lower, 3 under 1...), the newest sliding in from its side, the number
 * drawn by the kit (numeros()); the gems and the arrows put away. */
#define TAM 1.4f                        /* the biggest the cards get */
#define HUECO 80.f                      /* frame px between the two columns of cards */
#define INCLINA 0.12f                   /* the cards' tilt, radians (~7 degrees) */
static int s_grande;                    /* card sizes changed: put back off the chain */
static struct { float x, y, hw, hh, k, giro; int ok; } s_carta[9];   /* each link's card on screen */
void sub_000F69A0(void);

/* 2026-11-16 (the user's Master Duel capture: the cards tilted): a sprite's
 * picture is its node [sprite + 8]: +4 x, +8 y, +0xC turn (radians), +0x10 /
 * +0x14 scale, +0x4C "matrix to remake" (sub_00011FE0 makes the 2 x 3 matrix
 * from them: cos, -sin / sin, cos; sub_00012B80 places the 4 corners, ±half
 * the size, through it: it turns about the middle). Set before tamano(),
 * whose sub_000F69A0 remakes the corners. */
static void girar(uint32_t spr, float ang)
{
    uint32_t nodo;
    if (spr < 0x10000u || (nodo = MEM32(spr + 8u)) < 0x10000u)
        return;
    MEM32(nodo + 0xCu) = lf(ang);
    MEM8(nodo + 0x4Cu) = 1;
}

static void tamano(uint32_t spr, float a, float h)
{
    uint32_t v[2];
    if (spr < 0x10000u)
        return;
    v[0] = lf(a);
    v[1] = lf(h);
    llama(sub_000F69A0, spr, 2, v);
}

static void tamanos_del_juego(uint32_t w)
{
    uint32_t s;
    for (s = 0; s < 9u; s++) {
        girar(MEM32(w + 0x678u + 4u * s), 0.f);
        tamano(MEM32(w + 0x678u + 4u * s), 96.f, 139.5f);
    }
    s_grande = 0;
}

static void escalera(uint32_t w)
{
    uint32_t n = MEM32(w + 0x5B0u), c = MEM32(w + 0x5B4u), s;
    int j;
    if (!s_medido || n == 0u || n > 9u || c >= n)
        return;
    for (j = 0; j < LUGARES; j++)
        s_gema_ok[j] = 0;
    for (j = 0; j < 9; j++)
        s_carta[j].ok = 0;
    for (s = 0; s < 9u; s++) {
        uint32_t carta = MEM32(w + 0x678u + 4u * s), gema = MEM32(w + 0x6C0u + 4u * s);
        uint32_t flecha = MEM32(w + 0x69Cu + 4u * s), id;
        float x, y, k, t, lado, giro;
        j = (int)c + (int)s - 4;
        if (flecha)
            poner(flecha, -400.f, -400.f);
        if (gema)
            poner(gema, -400.f, -400.f);
        if (j < 0 || j >= (int)n || !carta)
            continue;
        id = MEM32(w + 0x10u + 4u * (uint32_t)j);
        if (id != s_esc_id[j]) {
            s_esc_id[j] = id;
            s_esc_desde[j] = s_cuadro;
        }
        k = (float)(s_cuadro - s_esc_desde[j]) / (float)ESC_ENTRA;
        if (k > 1.f || j + 1 < (int)n)
            k = 1.f;
        k = 1.f - (1.f - k) * (1.f - k);
        {
            /* 2026-11-16 (the user: "la tercera tiene que ir abajo de la
             * primera y asi continuamente"): a zigzag down, odd links on the
             * left (x 190), even on the right (x 450), each half a card lower
             * than the one before; the cards as big as fits: n + 1 half
             * heights in the 284 rows above the bottom bar (at most TAM) */
            /* then ("porque tan larga la cadena cuando son 3 o 4, tiene que
             * mantenerse del mismo tamano"): the columns follow the cards'
             * width, HUECO frame px between them, whatever their size; and
             * tilted as in Master Duel, the left ones one way, the right ones
             * the other, the newest coming in turned more */
            float hh = 270.f / (float)(n + 1u), esc = hh / 69.75f, bx, by;
            if (esc > TAM)
                esc = TAM, hh = 69.75f * TAM;
            bx = n == 1u ? 320.f : (j & 1) ? 320.f + 48.f * esc + HUECO * 0.5f : 320.f - 48.f * esc - HUECO * 0.5f;
            by = n == 1u ? 140.f : 10.f + hh + (float)j * hh;   /* 10..280: room for the tilted corners */
            lado = bx >= 320.f ? 1.f : -1.f;
            x = bx + lado * (1.f - k) * 260.f;          /* from off its side */
            y = by + s_sube[s];
            t = esc * (1.f + 0.25f * (1.f - k));        /* a little bigger as it comes */
            giro = (n == 1u ? -INCLINA : lado * INCLINA) + lado * (1.f - k) * 0.35f;
        }
        girar(carta, giro);
        tamano(carta, 96.f * t, 139.5f * t);
        poner(carta, x, y);
        s_carta[j].giro = giro;
        s_carta[j].x = x;
        s_carta[j].y = y;
        s_carta[j].hw = 48.f * t;
        s_carta[j].hh = 69.75f * t;
        s_carta[j].k = k;
        s_carta[j].ok = 1;
        if (j < LUGARES) {
            /* bottom right corner, turned with the card (x' = cos x - sin y,
             * y' = sin x + cos y, as sub_00011FE0's matrix) */
            float ox = 96.f * t * 0.5f - 12.f, oy = 139.5f * t * 0.5f - 16.f;
            s_gema[j][0] = x + cosf(giro) * ox - sinf(giro) * oy;
            s_gema[j][1] = y + sinf(giro) * ox + cosf(giro) * oy;
            s_gema_ok[j] = k >= 1.f;
        }
    }
    s_grande = 1;
}

void sub_00055530_pad(void);
void sub_00055530(void)
{
    uint32_t w = g_ecx, ban;
    sub_00055530_pad();
    if (w < 0x10000u || w != MEM32(MEM32(0x00656F88u) + 0x5Cu))
        return;
    ban = MEM32(w + 0x658u);
    if (s_es_cadena && s_panel && s_cuadro - s_panel < 3u) {
        medir(w);
        escalera(w);
        poner(ban, -400.f, -400.f);
        s_titulo_fuera = 1;
    } else {
        if (s_titulo_fuera) {
            poner(ban, 140.f, 60.f);    /* where sub_000560D0 put it */
            s_titulo_fuera = 0;
        }
        if (s_grande)
            tamanos_del_juego(w);       /* the other lists: the game's size */
    }
}

/* ---- the chain numbers ------------------------------------------------------ */
/* the frame point (640x480) of square z of player p (raton.c slot_place) */
static int lugar(int p, int z, float *x, float *y)
{
    static const float near_mon[5] = { 231.f, 276.f, 318.f, 361.f, 405.f };
    static const float near_spl[5] = { 231.f, 275.f, 321.f, 365.f, 410.f };
    static const float far_mon[5]  = { 398.f, 359.f, 318.f, 281.f, 243.f };
    static const float far_spl[5]  = { 397.f, 357.f, 320.f, 282.f, 245.f };
    if (z < 0 || z > 10)
        return 0;
    if (!p) {
        if (z < 5) { *x = near_mon[z]; *y = 252.f; }
        else if (z < 10) { *x = near_spl[z - 5]; *y = 287.f; }
        else { *x = 185.f; *y = 252.f; }
    } else {
        if (z < 5) { *x = far_mon[z]; *y = 142.f; }
        else if (z < 10) { *x = far_spl[z - 5]; *y = 110.f; }
        else { *x = 439.f; *y = 142.f; }
    }
    return 1;
}

/* the square holding card id of player p, spells and traps first; -1 */
static int casilla_de(int p, uint32_t id)
{
    static const int orden[11] = { 5, 6, 7, 8, 9, 10, 0, 1, 2, 3, 4 };
    int i;
    for (i = 0; i < 11; i++)
        if ((MEM16(CASILLA(p, orden[i])) & 0x7FFFu) == id)
            return orden[i];
    return -1;
}

static struct { uint32_t id; int p, z, cuadros; } s_n[LUGARES];

static void numeros(void)
{
    uint32_t n = MEM16(CADENA_N), i;
    int tapado = 0, panel = 0;
    /* the activation's big card (order 0x4D running) and the chain panel */
    if ((MEM32(0x0065F948u) & 0xFFFu) == 0x4Du && MEM8(0x0065F90Cu) != 0u)
        tapado = 1;
    if (s_panel && s_cuadro - s_panel < 3u)
        panel = 1;
    if (n > 32u)
        n = 0;                          /* not a chain (0x280 / 0x14 = 32 links) */
    if (n != s_visto_n) {
        char t[200];
        int k = 0;
        for (i = 0; i < n && i < 8u && k < 180; i++)
            k += snprintf(t + k, sizeof t - (size_t)k, " %04X/%u/%02X", MEM16(CADENA + 0x14u * i),
                          MEM8(CADENA + 0x14u * i + 2u), MEM8(CADENA + 0x14u * i + 3u));
        if (k == 0)
            t[0] = 0;
        if (n == 0u)
            s_mostrada = 0;
        fprintf(stderr, "[CADENA] %u: la cadena tiene %u eslabones (66BF9A %u):%s\n", s_cuadro, n, MEM16(0x0066BF9Au), t);
        s_visto_n = (uint16_t)n;
    }
    for (i = 0; i < LUGARES; i++) {
        volatile float *d = g_nv2a_dano[2 + i];
        float x = 0.f, y = 0.f, esc = 0.6f;
        int ok = 0;
        if ((n >= 2u || panel) && i < n) {   /* the chain's cards: from link 1, as Master Duel */
            uint32_t id = MEM16(CADENA + 0x14u * i) & 0x7FFFu;
            int p = MEM8(CADENA + 0x14u * i + 2u) & 1;
            int z;
            if (id != s_n[i].id || p != s_n[i].p) {
                s_n[i].id = id;
                s_n[i].p = p;
                s_n[i].cuadros = 0;
            }
            if (panel) {
                /* the panel's cards (the 19:44 video, 1080p: 450..735 and
                 * 818..1103 wide, rows 205..515): 95 frame px wide, every
                 * 122.7, the first from column 150. 20:1x captures ("tapan la
                 * carta"): the number on the game's own gem under each card
                 * (its owner's colour; 1080p row ~563 -> frame 250), once the
                 * panel stands still */
                /* 20:4x capture: the gems' coloured faces at 1080p (591,562)
                 * and (958,562), 64x48: centred on the face, smaller. Now the
                 * gems follow the staircase (escalera()) */
                if (s_panel_quieto > PANEL_ESPERA && s_gema_ok[i]) {
                    x = s_gema[i][0];
                    y = s_gema[i][1];
                    esc = 1.15f;
                    ok = 2;             /* Master Duel's light blue */
                }
            } else {
                z = casilla_de(p, id);
                if (z >= 0 && lugar(p, z, &x, &y)) {
                    /* off the card: its top right corner, small (the field
                     * cards are ~30 px wide, ~44 apart) */
                    s_n[i].z = z;
                    x += p ? 11.f : 14.f;
                    y -= p ? 10.f : 13.f;
                    esc = p ? 0.45f : 0.52f;
                    ok = 1;
                }
            }
        } else {
            s_n[i].id = 0;
            s_n[i].cuadros = 0;
        }
        if (ok && !tapado) {
            float u;
            if (s_n[i].cuadros < ENTRA)
                s_n[i].cuadros++;
            u = (float)s_n[i].cuadros / (float)ENTRA;
            d[0] = x;
            d[1] = y;
            d[2] = -esc * (1.f + 0.5f * (1.f - u) * (1.f - u));   /* no sign; comes in big, settles */
            d[3] = u;
            d[4] = (float)(i + 1);
            d[5] = ok == 2 ? 0.55f : 1.0f;   /* gold on the field, as the game's arrows; */
            d[6] = ok == 2 ? 0.95f : 0.82f;  /* light blue on the chain's cards */
            d[7] = ok == 2 ? 1.0f : 0.25f;
        } else {
            d[3] = 0.f;
            if (!ok)
                s_n[i].cuadros = 0;
        }
    }
}

static int en_duelo(void)
{
    uint32_t e = MEM32(0x00657148u);
    return e == 6u || e == 7u;
}

/* 23:5x (the user: "dibujala dorada"): a golden chain from each link's card to
 * the next one's, from where the line between their middles leaves the first
 * card (85% of it, so it holds on its edge) to where it enters the next;
 * it grows with the newer card's slide. Drawn by the kit (g_nv2a_cadena) */
#define GROSOR 13.f                     /* the chain's thickness, frame px (~30 px at 1080p) */

static void cadenas(int ver)
{
    int j;
    if (!g_nv2a_cadena_px) {
        g_nv2a_cadena_w = ESLABONES_W;
        g_nv2a_cadena_h = ESLABONES_H;
        g_nv2a_cadena_px = k_eslabones_px;
    }
    /* 2026-11-16 (the user's Master Duel capture: "es mas fina y no es derecha
     * en ninguna parte, siempre con movimiento y en diagonal"; then "podriamos
     * dejar una sola cadena? con una de 3 se ven mal"): one thin chain per pair
     * of cards, middle to middle (the cards hide its ends), bowed and swaying
     * slowly (game frames), so link after link they make one chain */
    for (j = 0; j < 8; j++) {
        volatile float *c = g_nv2a_cadena[j];
        if (ver && s_carta[j].ok && s_carta[j + 1].ok) {
            c[0] = s_carta[j].x;
            c[1] = s_carta[j].y;
            c[2] = s_carta[j + 1].x;
            c[3] = s_carta[j + 1].y;
            c[5] = s_carta[j + 1].k;
            c[6] = GROSOR;
            c[7] = (float)s_cuadro * 0.05f + (float)j * 1.7f;
            c[8] = 14.f;                /* bows down */
            c[9] = 6.f;
            c[4] = 1.f;
        } else {
            c[4] = 0.f;
        }
    }
    for (j = 0; j < 9; j++) {
        volatile float *r = g_nv2a_cadena_cartas[j];
        r[0] = s_carta[j].x;
        r[1] = s_carta[j].y;
        r[2] = ver && s_carta[j].ok ? s_carta[j].hw : 0.f;
        r[3] = s_carta[j].hh;
        r[4] = cosf(s_carta[j].giro);
        r[5] = sinf(s_carta[j].giro);
    }
}

/* each frame of the duel scene (mascota.c) */
void ygo_cadena_tick(void)
{
    int i;
    if (!en_duelo()) {
        for (i = 0; i < LUGARES; i++)
            g_nv2a_dano[2 + i][3] = 0.f;
        cadenas(0);
        s_visto_n = 0;
        return;
    }
    s_cuadro++;
    /* sub_00055530 does not run every frame with the panel still (23:36: the
     * numbers never showed): the staircase and its newest link's slide are
     * kept from here too */
    if (s_es_cadena && s_panel && s_cuadro - s_panel < 3u) {
        uint32_t lw = MEM32(0x00656F88u);
        if (lw >= 0x10000u && MEM32(lw + 0x5Cu) >= 0x10000u)
            escalera(MEM32(lw + 0x5Cu));
    } else {
        s_medido = 0;
        for (i = 0; i < LUGARES; i++)
            s_gema_ok[i] = 0;
        for (i = 0; i < 9; i++)
            s_carta[i].ok = 0;
    }
    /* with the panel up and still, as the numbers; not over the big
     * activation card (order 0x4D running) */
    cadenas(s_es_cadena && s_panel && s_cuadro - s_panel < 3u && s_panel_quieto > PANEL_ESPERA
            && !((MEM32(0x0065F948u) & 0xFFFu) == 0x4Du && MEM8(0x0065F90Cu) != 0u));
    numeros();
}
