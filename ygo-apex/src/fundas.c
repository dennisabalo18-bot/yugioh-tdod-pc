/* Fundas (idea 13 de Master Duel, 2026-11-16; el usuario: "sumar el de exodia",
 * "5 marcos diferentes que se tengan que poder elegir").
 *
 * El reverso de las cartas es una sola textura del juego (card/normal/
 * card_ura.tga, P8 128x256, la carta en sus 128x185 de abajo, guardada dada
 * vuelta; huella E86E1EE8D605D332): la de todas las cartas boca abajo, las
 * tuyas y las del rival. Con una funda elegida se arma su imagen 4x (512 x
 * 1024, la carta en las 740 filas de abajo, dada vuelta como en el pack) con
 * la ilustracion y el marco elegidos (Mods-Extras\fundas\arte\<nombre>.png y
 * marco\<nombre>.png, 512 x 740, el marco con su ventana transparente;
 * tools/fundas.py) y el kit la muestra en lugar de esa textura
 * (nv2a_tex_pack_override). "Clasica" la deja como esta. Se cambia en vivo
 * desde F2 o Configuracion. */
#define COBJMACROS
#include "mods_carpeta.h"
#include <windows.h>
#include <wincodec.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "recomp/gen/recomp_types.h"

void nv2a_tex_pack_override(uint64_t hash, uint32_t w, uint32_t h, const uint32_t *texels);

#define REVERSO 0xE86E1EE8D605D332ull   /* card_ura.tga's hash */
#define TW 512
#define TH 1024
#define CW 512
#define CH 740                          /* the card's rows, at the bottom */

/* 2026-11-19 (the user): in the mascots' order (mascota.c k_modelo), so
 * sleeve / tapete N is mascot N's art: index i + 1 = mascot i, 0 Clásica */
static const char *const k_arte[] = { NULL,
                                      "ojos_azules",
                                      "kuriboh",
                                      "mago_oscuro",
                                      "mago_oscuro_2",
                                      "jinzo",
                                      "craneo",
                                      "ojos_rojos",
                                      "chica_maga",
                                      "gaia",
                                      "guardian_celta",
                                      "arpia",
                                      "mago_tiempo",
                                      "dragon_bebe",
                                      "espadachin_llama",
                                      "soldado_brillo",
                                      "espadachin",
                                      "exodia",
                                      "dragon_barril",
                                      "rey_rex",
                                      "gran_polilla",
                                      "pescador",
                                      "mil_ojos",
                                      "necrofear",
                                      "bestia_enmascarada",
                                      "maquina_tragamonedas",
                                      "dragon_zombi",
                                      "maha_vailo",
                                      "golem_milenio",
                                      "definitivo",
                                      "dragon_craneo_negro",
                                      "mago_caos_negro",
                                      "dragon_meteoro_negro",
                                      "valkyrion",
                                      "dragon_toon",
                                      "gaia_campeon",
                                      "sabio_oscuro",
                                      "dragon_trueno",
                                      "dragon_tres_cuernos",
                                      "reina_cosmos",
                                      "zera",
                                      "caballero_craneo",
                                      "dragon_mascota_harpie",
                                      "guardian_puerta",
                                      "renunciado",
                                      "craneo_toon",
                                      "maldicion_dragon",
                                      "mil_dragones",
                                      "alfa",
                                      "beta",
                                      "gamma",
                                      "dragon_meteoro",
                                      "elfa_mistica",
                                      "bruja_bosque",
                                      "tomate",
                                      "sangan",
                                      "gardna",
                                      "payaso_zombi",
                                      "fantasma",
                                      "mago_fe",
                                      "legamo_humanoide",
                                      "princesa_fuego",
                                      "hermanas_arpia",
                                      "sanga",
                                      "kazejin",
                                      "suijin",
                                      "dokurorider",
                                      "dunames",
                                      "guerrero_pizarra",
                                      "gusano_humanoide",
                                      "vorse",
                                      "goblin",
                                      "elfas_gemelas",
                                      "santa_juana",
                                      "zoa",
                                      "melchid",
                                      "bickuribox",
                                      "elfo_oscuro",
                                      "tortuga_cangrejo",
                                      "espada_garma",
                                      "escarabajo_jabalina",
                                      "tanque_laberinto",
                                      "ballena_fortaleza",
                                      "dragon_nocturno",
                                      "chakra",
                                      "megacyber",
                                      "bicho_devorador",
                                      "jarra_cibernetica",
                                      "parshath",
                                      "soldado_pinguino",
                                      "jarra_cambiante",
                                      "muka_muka",
                                      "soldado_piedra",
                                      "muro_laberinto",
                                      "ninja_armado",
                                      "aprendiz_bruja",
                                      "gilasaurus",
                                      "battleguard_lava",
                                      "seiyaryu",
                                      "guerrero_pantera",
                                      "jirai_gumo" };
static const char *const k_marco[] = { "dorado", "plata", "obsidiana", "milenio", "neon" };
#define NARTE (int)(sizeof k_arte / sizeof k_arte[0])
#define NMARCO (int)(sizeof k_marco / sizeof k_marco[0])
/* perfil.c: the tapetes' files and how many (0 = "clasico") */
const char *ygo_arte_archivo(int i) { return i > 0 && i < NARTE ? k_arte[i] : "clasico"; }
int ygo_arte_n(void) { return NARTE; }

static volatile LONG s_funda, s_marco;

static const GUID k_clsid_wic = { 0xcacaf262, 0x9370, 0x4615, { 0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a } };
static const GUID k_iid_wic = { 0xec5ec8a9, 0xc395, 0x4314, { 0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70 } };
static const GUID k_pbgra = { 0x6fddc324, 0x4e03, 0x4bfe, { 0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x10 } };

/* a 512 x 740 PNG as premultiplied BGRA, or NULL */
static unsigned char *leer(const char *carpeta, const char *nombre)
{
    static IWICImagingFactory *wic;
    char a[MAX_PATH];
    wchar_t p[MAX_PATH];
    const char *mods = getenv("RECOMP_MODS_DIR");
    IWICBitmapDecoder *dec = NULL;
    IWICBitmapFrameDecode *fr = NULL;
    IWICBitmapSource *cv = NULL;
    UINT w = 0, h = 0;
    unsigned char *buf = NULL;
    if (!wic) {
        CoInitializeEx(NULL, COINIT_MULTITHREADED);
        if (FAILED(CoCreateInstance(&k_clsid_wic, NULL, CLSCTX_INPROC_SERVER, &k_iid_wic, (void **)&wic)))
            wic = NULL;
    }
    /* 2026-11-16 (first try: "no se vio nada"): RECOMP_MODS_DIR is a list of
     * mod folders ("mods_puntero;Mods-Extras\cartas\archivos"), not
     * Mods-Extras: the sleeves are next to the exe, in Mods-Extras\fundas */
    {
        char exe[MAX_PATH], *barra;
        DWORD n = GetModuleFileNameA(NULL, exe, sizeof exe);
        (void)mods;
        if (n == 0 || n >= sizeof exe || !(barra = strrchr(exe, '\\')))
            strcpy(exe, ".");
        else
            *barra = 0;
        snprintf(a, sizeof a, "%s%s\\fundas\\%s\\%s.png", exe, ygo_mods_a(), carpeta, nombre);
    }
    if (!wic || !MultiByteToWideChar(CP_ACP, 0, a, -1, p, MAX_PATH))
        goto fin;
    if (FAILED(IWICImagingFactory_CreateDecoderFromFilename(wic, p, NULL, GENERIC_READ,
                                                            WICDecodeMetadataCacheOnDemand, &dec))
            || FAILED(IWICBitmapDecoder_GetFrame(dec, 0, &fr))
            || FAILED(WICConvertBitmapSource(&k_pbgra, (IWICBitmapSource *)fr, &cv))
            || FAILED(IWICBitmapSource_GetSize(cv, &w, &h)) || w != CW || h != CH)
        goto fin;
    buf = (unsigned char *)malloc(CW * CH * 4);
    if (buf && FAILED(IWICBitmapSource_CopyPixels(cv, NULL, CW * 4, CW * CH * 4, buf))) {
        free(buf);
        buf = NULL;
    }
fin:
    if (cv)
        IWICBitmapSource_Release(cv);
    if (fr)
        IWICBitmapFrameDecode_Release(fr);
    if (dec)
        IWICBitmapDecoder_Release(dec);
    if (!buf)
        fprintf(stderr, "[FUNDAS] no pude leer %s\n", a);
    return buf;
}

/* the opponent's card back (2026-11-16, the user: "armala con una fija
 * distinta a exodia por ahora"): Ojos Rojos in Obsidiana, handed to the kit
 * (nv2a_reverso_rival), which draws the face-down cards above frame row
 * MITAD with it (see ygo_fundas_tick) */
#define REVERSO_RIVAL 1ull              /* not a texture: componer's "the opponent's" */
#define RIVAL_ARTE 7                    /* ojos_rojos: an opponent with no card below */
/* 2026-11-16 (the user: "la de los personajes clasicos de cada uno como
 * hicimos con las mascotas"): each opponent's sleeve is their mascot's card
 * (mascota.c k_rival, by ygo_mascota_rival: k_modelo's index) -> k_arte */
/* 2026-11-19: k_arte is in the mascots' order: mascot m's art is m + 1 */
static const signed char k_por_mascota[100] = {
    1,    2,    3,    4,    5,    6,    7,    8,    9,    10,
    11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
    21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
    31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
    41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
    51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
    61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
    71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
    81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
    91,    92,    93,    94,    95,    96,    97,    98,    99,    100,
};
int ygo_mascota_rival(void);
#define RIVAL_MARCO 2                   /* obsidiana */
void nv2a_reverso_rival(const uint32_t *texels, uint32_t w, uint32_t h);
extern volatile uint64_t g_nv2a_reverso_hash;
extern volatile float g_nv2a_reverso_fila;
/* 2026-11-20 (the user: the opponent's set card shown big on the left had my
 * sleeve; the left one shows only the opponent's cards, mine go right): left
 * of this column (game px) the face-down cards are the opponent's at any row.
 * The user's capture: the right one at x 514..587 of 640, so the left one
 * ~53..126; my field's first card starts ~173. */
extern volatile float g_nv2a_reverso_col, g_nv2a_reverso_col2;
extern volatile float g_nv2a_reverso_ancho;   /* 21:4x: a HEIGHT (game px): the big views ~110 tall, a drawn card / your deck 34 (21:31 log); a height, since the big view may narrow while it turns */
#define ANCHO_VISTA 50.f
#define COLUMNA_RIVAL 150.f
/* 2026-11-20 21:0x (the user's capture: the opponent's face-down card shown
 * big on the RIGHT, x 512..588 of 640, had my sleeve): your own face-down
 * cards are shown to you face up, so a back in either big view is always the
 * opponent's: right of this column too. My deck ends ~477. */
/* 21:2x: 480 (the big view with Duo delictivo at x 497 in the 21:08 and 21:24 logs, 2 px from the cut; your deck: x 444..477, middle ~458) */
#define COLUMNA_RIVAL2 480.f
extern volatile uint64_t g_nv2a_reverso_ms;      /* kit: when the tick last said so */

static void componer_sin(uint64_t hash, int f, int m);
/* 2026-11-20: the kit's override is one writer at a time (the loaned
 * deck's sleeve goes in and out from the pad hook too) */
static SRWLOCK s_componer = SRWLOCK_INIT;
static void componer(uint64_t hash, int f, int m)
{
    AcquireSRWLockExclusive(&s_componer);
    componer_sin(hash, f, m);
    ReleaseSRWLockExclusive(&s_componer);
}

static void componer_sin(uint64_t hash, int f, int m)
{
    unsigned char *arte, *marco;
    uint32_t *tex;
    int x, y;
    if (f <= 0 || f >= NARTE) {
        nv2a_tex_pack_override(hash, 0, 0, NULL);
        fprintf(stderr, "[FUNDAS] clasica\n");
        return;
    }
    if (m < 0 || m >= NMARCO)
        m = 0;
    arte = leer("arte", k_arte[f]);
    marco = leer("marco", k_marco[m]);
    tex = (uint32_t *)malloc(TW * TH * 4);
    if (arte && marco && tex) {
        for (y = 0; y < TH - CH; y++)
            for (x = 0; x < TW; x++)
                tex[y * TW + x] = 0xFF000000u;
        for (y = 0; y < CH; y++) {
            /* texture row TH-1-y holds card row y (the texture is upside down) */
            const unsigned char *pa = arte + (size_t)y * CW * 4, *pm = marco + (size_t)y * CW * 4;
            uint32_t *o = tex + (size_t)(TH - 1 - y) * TW;
            for (x = 0; x < CW; x++) {
                unsigned inv = 255u - pm[4 * x + 3], c[3], k;
                for (k = 0; k < 3; k++)
                    c[k] = pm[4 * x + k] + (pa[4 * x + k] * inv + 127u) / 255u;
                o[x] = 0xFF000000u | (c[2] > 255 ? 255u : c[2]) << 16 | (c[1] > 255 ? 255u : c[1]) << 8
                     | (c[0] > 255 ? 255u : c[0]);
            }
        }
        if (hash == REVERSO_RIVAL)
            nv2a_reverso_rival(tex, TW, TH);
        else
            nv2a_tex_pack_override(hash, TW, TH, tex);
        fprintf(stderr, "[FUNDAS] %016llX: %s con marco %s\n", (unsigned long long)hash, k_arte[f], k_marco[m]);
    }
    free(arte);
    free(marco);
    free(tex);
}

static void aplicar(void)
{
    componer(REVERSO, (int)s_funda, (int)s_marco);
}

int ygo_funda_get(void) { return (int)s_funda; }
void ygo_funda_set(int i)
{
    LONG v = i >= 0 && i < NARTE ? i : 0;
    if (InterlockedExchange(&s_funda, v) != v)
        aplicar();
}
int ygo_funda_marco_get(void) { return (int)s_marco; }
void ygo_funda_marco_set(int i)
{
    LONG v = i >= 0 && i < NMARCO ? i : 0;
    if (InterlockedExchange(&s_marco, v) != v && s_funda)
        aplicar();
}

/* ---- the tapete (2026-11-16, the user: "Tapetes ... como master duel") ------
 * The duel board (TABLERO, one texture for both halves) is drawn by the kit
 * below frame row MITAD (your half) with Mods-Extras\tapetes\<k_arte>.png
 * (tools/tapetes.py: 2048 x 512, the texture's layout, straight alpha);
 * 0 "Clasico": the game's. Read on a worker thread (a click must not wait). */
#define TABLERO 0xBFBD8339E5E8D7E0ull   /* the board's hash (512 x 128) */
static const GUID k_bgra = { 0x6fddc324, 0x4e03, 0x4bfe, { 0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0f } };
void nv2a_tapete(const uint32_t *texels, uint32_t w, uint32_t h);
void ygo_tapete_fx_base(const uint32_t *px, uint32_t w, uint32_t h);   /* tapete_fx.c: its light */
extern volatile uint64_t g_nv2a_tapete_hash;
static volatile LONG s_tapete, s_tapete_pedido;
static volatile LONG s_tap_forzado = -1;   /* 2026-11-20: the loaned deck's (prestado.c), -1 yours */
static LONG tapete_efectivo(void) { LONG f = s_tap_forzado; return f >= 0 ? f : s_tapete; }

/* Mods-Extras\tapetes\<nombre>.png as straight BGRA (malloc'd), NULL if not */
static uint32_t *tapete_leer(IWICImagingFactory *wic, const char *nombre, UINT *pw, UINT *ph)
{
    char a[MAX_PATH], exe[MAX_PATH], *barra;
    wchar_t p[MAX_PATH];
    IWICBitmapDecoder *dec = NULL;
    IWICBitmapFrameDecode *fr = NULL;
    IWICBitmapSource *cv = NULL;
    UINT w = 0, h = 0;
    uint32_t *px = NULL;
    DWORD n = GetModuleFileNameA(NULL, exe, sizeof exe);
    if (n == 0 || n >= sizeof exe || !(barra = strrchr(exe, '\\')))
        strcpy(exe, ".");
    else
        *barra = 0;
    snprintf(a, sizeof a, "%s%s\\tapetes\\%s.png", exe, ygo_mods_a(), nombre);
    if (!(wic && MultiByteToWideChar(CP_ACP, 0, a, -1, p, MAX_PATH)
            && SUCCEEDED(IWICImagingFactory_CreateDecoderFromFilename(wic, p, NULL, GENERIC_READ,
                                                                      WICDecodeMetadataCacheOnDemand, &dec))
            && SUCCEEDED(IWICBitmapDecoder_GetFrame(dec, 0, &fr))
            && SUCCEEDED(WICConvertBitmapSource(&k_bgra, (IWICBitmapSource *)fr, &cv))
            && SUCCEEDED(IWICBitmapSource_GetSize(cv, &w, &h)) && w && h && w <= 4096 && h <= 4096
            && (px = (uint32_t *)malloc((size_t)w * h * 4)) != NULL
            && SUCCEEDED(IWICBitmapSource_CopyPixels(cv, NULL, w * 4, w * h * 4, (BYTE *)px)))) {
        free(px);
        px = NULL;
        fprintf(stderr, "[TAPETE] no pude leer %s\n", a);
    }
    if (cv) IWICBitmapSource_Release(cv);
    if (fr) IWICBitmapFrameDecode_Release(fr);
    if (dec) IWICBitmapDecoder_Release(dec);
    *pw = w;
    *ph = h;
    return px;
}

/* 2026-11-20 (the user: "el tapete numero 0 tiene que ser el comun del juego.
 * aplicale el brillo y las lineas a ese tambien"): 0 is the game's own board
 * (clasico.png: its 4x upscale, modding/texturas/mejoradas), handed over as
 * the others so tapete_fx.c lights it too; the game's texture if it is missing */
static DWORD WINAPI tapete_hilo(LPVOID unused)
{
    IWICImagingFactory *wic = NULL;
    (void)unused;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(CoCreateInstance(&k_clsid_wic, NULL, CLSCTX_INPROC_SERVER, &k_iid_wic, (void **)&wic)))
        wic = NULL;
    for (;;) {
        LONG t = tapete_efectivo();
        UINT w = 0, h = 0;
        uint32_t *px = t >= 0 && t < NARTE ? tapete_leer(wic, t > 0 ? k_arte[t] : "clasico", &w, &h) : NULL;
        if (tapete_efectivo() == t) {   /* still the one asked for */
            nv2a_tapete(px, w, h);
            ygo_tapete_fx_base(px, w, h);
            fprintf(stderr, "[TAPETE] %s%s\n", t > 0 && t < NARTE ? k_arte[t] : "clasico", px ? "" : " (el del juego)");
        }
        free(px);
        /* another one asked meanwhile: go again; else done */
        if (InterlockedCompareExchange(&s_tapete_pedido, 0, 1) == 1 && tapete_efectivo() == t)
            break;
    }
    if (wic)
        IWICImagingFactory_Release(wic);
    return 0;
}

/* 2026-11-20 (the user: "que el Rival Tenga su Tapete de su mascota
 * Tambien"): the opponent's half with its mascot's tapete (the art as its
 * sleeve's, k_por_mascota), read here when it changes; the kit draws it above
 * the row (nv2a_tapete_rival) */
void nv2a_tapete_rival(const uint32_t *texels, uint32_t w, uint32_t h);
static volatile LONG s_tap_rival = -1;   /* the art asked for */
static DWORD WINAPI tapete_rival_hilo(LPVOID unused)
{
    IWICImagingFactory *wic = NULL;
    LONG hecho = -1;
    (void)unused;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(CoCreateInstance(&k_clsid_wic, NULL, CLSCTX_INPROC_SERVER, &k_iid_wic, (void **)&wic)))
        wic = NULL;
    for (;;) {
        LONG f = s_tap_rival;
        if (f != hecho) {
            UINT w = 0, h = 0;
            uint32_t *px = f > 0 && f < NARTE ? tapete_leer(wic, k_arte[f], &w, &h) : NULL;
            hecho = f;
            nv2a_tapete_rival(px, w, h);
            fprintf(stderr, "[TAPETE] rival: %s\n", f > 0 && f < NARTE ? k_arte[f] : "el del juego");
            free(px);
        }
        Sleep(100);
    }
}

int ygo_tapete_get(void) { return (int)s_tapete; }
static void tapete_pedir(void)
{
    if (InterlockedExchange(&s_tapete_pedido, 1) == 0) {
        HANDLE th = CreateThread(NULL, 0, tapete_hilo, NULL, 0, NULL);
        if (th)
            CloseHandle(th);
        else
            s_tapete_pedido = 0;
    }
}
void ygo_tapete_set(int i)
{
    static volatile LONG hilo;
    LONG v = i >= 0 && i < NARTE ? i : 0;
    g_nv2a_tapete_hash = TABLERO;
    if (InterlockedExchange(&s_tapete, v) == v && hilo)
        return;
    /* one worker at a time: a running one sees the change and reads again */
    if (InterlockedExchange(&s_tapete_pedido, 1) == 0) {
        HANDLE th;
        hilo = 1;
        th = CreateThread(NULL, 0, tapete_hilo, NULL, 0, NULL);
        if (th)
            CloseHandle(th);
        else
            s_tapete_pedido = 0;
    }
}

/* ---- the opponent's sleeve (2026-11-16) ------------------------------------
 * The duel's face-down cards are 2D sprites (vtable 0x4236E4, sub_00013D10
 * with "card/normal/card_ura.tga"), and changing one's region (+0x50) does
 * not change what is drawn (tried: the 01:35 log swapped 10, the picture
 * stayed): sub_00014F90 builds, once, the draw batches, a run of sibling
 * sprites with the same texture sharing one (0x151F1..), with the texture
 * set in them (0x15082: the stage's +0x68). So the kit does it at the draw:
 * while the duel is on (state 6/7), every batch sampling card_ura's texture
 * draws above frame row MITAD (the opponent's field, hand and deck; the
 * life point boxes in between hold no card) with the opponent's back. */
#define MITAD 200.f
void sub_00013D10_pad(void);
void sub_00013D10(void)                 /* kept as it was (tools/rename_pads.py) */
{
    sub_00013D10_pad();
}

int ygo_prestado_en_duelo(void);       /* prestado.c */
int ygo_mascota_de_personaje(int ch);  /* mascota.c */
static volatile LONG s_prest_f;        /* the loaned deck's sleeve art on your cards, 0 none */

/* the game's pad read, every frame (mando_directo.c): out of the duel your
 * own sleeve and tapete again (the duel scene's tick stopped 1.5 s ago) */
void ygo_fundas_cuadro(void)
{
    if (s_prest_f && GetTickCount64() - g_nv2a_reverso_ms > 1500u
            && InterlockedExchange(&s_prest_f, 0) != 0) {
        aplicar();
        InterlockedExchange(&s_tap_forzado, -1);
        tapete_pedir();
        fprintf(stderr, "[FUNDAS] fuera del duelo: vuelven tu funda y tu tapete\n");
    }
}

/* each frame of the duel scene (mascota.c) */
void ygo_fundas_tick(void)
{
    static int hecho = -1;
    static ULONGLONG ultimo;
    ULONGLONG ahora = GetTickCount64();
    uint32_t e = MEM32(0x00657148u);
    /* 2026-11-16 (the user: "se vio un segundo mi funda y luego la de ella"):
     * the deck is on the field from the entrance on (states 3..5, ~1-2 s
     * before 6, the 02:1x log composed it at state 6), so the opponent's is
     * composed on the scene's first tick (a new scene: no tick for 1 s) and
     * drawn from then on, up to the duel's end (7) */
    /* 2026-11-17 (the user: "al final de cada duelo ... las fundas de sus
     * cartas se transforman en las mias"): the scene goes 6 -> 9 -> 10 at the
     * end (the result, the cards still on the field; 03:49 log), and the
     * split stopped after 7. The whole scene, every state. */
    int duelo = 1;
    (void)e;
    if (ahora - ultimo > 1000u) {
        /* each duel: the opponent's own (composed again only when it changes) */
        int pc = ygo_prestado_en_duelo(), pf = 0;
        if (pc >= 0) {
            int pm = ygo_mascota_de_personaje(pc);
            pf = pm >= 0 && pm < 100 && k_por_mascota[pm] ? k_por_mascota[pm] : RIVAL_ARTE;
        }
        if (pf != s_prest_f) {
            s_prest_f = pf;
            if (pf > 0) {
                componer(REVERSO, pf, RIVAL_MARCO);
                fprintf(stderr, "[FUNDAS] tuya: la del mazo prestado (personaje %d)\n", pc);
            } else
                aplicar();
            InterlockedExchange(&s_tap_forzado, pf > 0 ? pf : -1);
            tapete_pedir();
        }
        int m = ygo_mascota_rival(), f = m >= 0 && m < 100 && k_por_mascota[m] ? k_por_mascota[m] : RIVAL_ARTE;
        if (f != hecho) {
            static int hilo;
            hecho = f;
            componer(REVERSO_RIVAL, f, RIVAL_MARCO);
            InterlockedExchange(&s_tap_rival, f);           /* its tapete too (2026-11-20) */
            if (!hilo) {
                HANDLE th = CreateThread(NULL, 0, tapete_rival_hilo, NULL, 0, NULL);
                hilo = 1;
                if (th)
                    CloseHandle(th);
            }
        }
    }
    ultimo = ahora;
    g_nv2a_reverso_hash = REVERSO;
    g_nv2a_reverso_fila = duelo ? MITAD : 0.f;
    g_nv2a_reverso_col = COLUMNA_RIVAL;
    g_nv2a_reverso_col2 = COLUMNA_RIVAL2;
    g_nv2a_reverso_ancho = ANCHO_VISTA;
    /* the scene's tick stops with the scene: the kit stops too 1.5 s later (a pause menu keeps it) */
    g_nv2a_reverso_ms = ahora;
}
