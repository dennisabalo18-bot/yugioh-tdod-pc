/*
 * ygo-port: the duel dialogues in Latin American Spanish, text and voice.
 *
 * The game's texts: 6 tables (jp en de fr it es) of 3680 strings, pointer
 * array 0x5FC2DC, base array 0x5FC2C4, current language 0x5FC2C0 (5 =
 * Spanish). sub_0002C5C0(n) returns text n in the current language,
 * sub_0002C590(n, lang) in the one given. Texts 317-848 are the 28 duelists'
 * lines, 19 each (tools/dialogos.py).
 *
 * YGO_VOCES = a folder (modding\voces_latino, made by tools/voces_mod.py):
 * - textos.txt: "N<TAB>text" in Latin-1, the new lines without the emotion
 *   tags. At the first lookup they are written over the Spanish table's own
 *   317-848 strings (they fit: 26 KB in 27 KB; checked here, else nothing
 *   is changed) and its offsets moved to them;
 * - N.mp3: played when the game asks for text n (317-848) in Spanish, the
 *   first time it asks after another line or a 2 s pause (the balloon may
 *   ask again while it is up). On its own thread (MCI), a new line stops
 *   the one playing.
 * - A or B pressed twice within VOZ_DOBLE ms (pad, keyboard or mouse: what
 *   the game reads through mando_directo.c) while a voice plays fades it out
 *   in VOZ_FUNDIDO ms; one press only fills the balloon and the voice goes
 *   on. Wall-clock, so the same at 60, 90 or 120 fps. A press in the first
 *   VOZ_GRACIA ms of a line is the one that brought it up and is ignored.
 * - A or B (or a click) while the balloon's text is already whole closes
 *   it, and the voice fades out too. The balloon is the text object the
 *   line was given to (sub_0002B9D0, its base SetText: `this` = ecx); its
 *   [+0x17C0] is 0 new, 1 typing, 2 whole (sub_0004DE00 / sub_0004DED0).
 *   Read when the press is first seen, before the game acts on it.
 * - F2 "Voces": "No" plays none (the texts stay the new ones);
 *   "Sigue los FPS" (the default) plays
 *   x1.5/N.mp3 or x2/N.mp3 (the same voice faster, same pitch: voces_mod.py)
 *   when the game runs at ~90 or ~120 fps (F8), so the voice keeps up with
 *   the balloons, which count frames; "Normal" always plays N.mp3. The rate
 *   is the frames presented a second (nv2a_d3d11_presents), measured on the
 *   voice thread every half second, and picked when a line starts.
 * - The mouth follows the voice (lip sync): tools/voces_envolvente.py writes
 *   N.env beside each N.mp3, one byte (0..255, the voice's loudness) per
 *   20 ms. The voice thread loads it when the voice starts. The portraits
 *   (cf_anim, table 0x60F518 of 81 scripts) are run by the generic sprite
 *   animator sub_00018790 (`this` = ecx): [+0x4B4] the script, [+0x508] the
 *   animation, [+0x50C] bits that let a track run (bit 1: the mouth, set by
 *   the game while the balloon types), per control track (the fixed ones take no slot) [+0x53C + 4t] the command
 *   and [+0x514 + 4t] the frames left. The mouth is slot 1 (track 2): a control
 *   command (bit 1) whose loop is rest (the face's own mouth), half, open,
 *   half. Wrapped: while a voice plays, on the portrait that was talking,
 *   bit 1 is kept on and the mouth's command is picked from the loudness
 *   (rest / half / open, each held at least BOCA_MIN ms), so the mouth
 *   moves with the voice and stops when it stops, not with the typing.
 * [VOZ] in the log: each line asked for, and whether it had a voice; when
 *   the next one comes, how many times the last was asked for and over how
 *   long (whether the balloon asks every frame).
 *
 * recomp's definitions are renamed _pad (tools/rename_pads.py).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <mmsystem.h>
#include "recomp/gen/recomp_types.h"

void sub_0002C5C0_pad(void);
void sub_0002C590_pad(void);

#define FIRST 317u
#define LAST  848u
#define LANG_ES 5u

static int s_on = -1;              /* -1 not read yet, 0 off, 1 on */
static wchar_t s_dir[MAX_PATH];
#define VOZ_STOP (-2)
#define VOZ_GRACIA 250u
#define VOZ_DOBLE 400u     /* two presses this close together: skip the voice */
#define VOZ_FUNDIDO 300    /* ms the voice takes to fade out */
#define VOZ_PASOS 12
static volatile LONG s_want = -1;  /* the line the voice thread should play, or VOZ_STOP */
static volatile LONG s_playing = -1;
static volatile LONG64 s_started;  /* GetTickCount64 of the last line asked to play */
static HANDLE s_wake;
/* the balloon: the line last asked for, when, and how many times. A line
 * asked for VOZ_SEGUIDO or more times is redrawn from its text every frame;
 * when such a line is no longer asked for (VOZ_CERRADO ms), its balloon
 * closed and the voice fades out. A line asked for once never counts. */
#define VOZ_SEGUIDO 3
#define VOZ_CERRADO 300u
static volatile LONG s_ask_line = -1, s_ask_calls;
static volatile LONG64 s_ask_seen;
static volatile LONG s_sigue_fps = 1;  /* F2: 1 Sigue los FPS, 0 Normal */
static volatile LONG s_mudo;           /* F2: "No", no voices */
/* the voiced line's text (sub_0002C5C0's answer), and the balloon given it */
static volatile LONG s_line_txt, s_line_n = -1;
static volatile LONG s_globo, s_globo_n = -1;
static volatile LONG s_fps = 60;       /* the game's frame rate, measured */

uint64_t nv2a_d3d11_presents(void);    /* xboxrecomp-apex nv2a_pb_d3d11.c */

/* the playing voice's loudness, 50 a second (N.env), from s_env_t0 */
#define ENV_MAX 8192
static unsigned char s_env[ENV_MAX];
static volatile LONG s_env_len;
static volatile LONG64 s_env_t0;
static unsigned char s_quien[ENV_MAX];
static volatile LONG s_quien_len;

static void env_load(const wchar_t *mp3)
{
    wchar_t p[MAX_PATH];
    size_t l = wcslen(mp3);
    FILE *f;
    InterlockedExchange(&s_env_len, 0);
    if (l < 4 || l >= MAX_PATH)
        return;
    wcscpy(p, mp3);
    wcscpy(p + l - 4, L".env");
    f = _wfopen(p, L"rb");
    if (!f)
        return;
    l = fread(s_env, 1, ENV_MAX, f);
    fclose(f);
    {
        /* N.quien (tools/voces_quien.py, Lumis & Umbra's lines): who talks
         * each 20 ms, 1 Lumis, 2 Umbra, 3 both; none: 0 (both mouths) */
        size_t q = 0;
        wcscpy(p + wcslen(p) - 4, L".quien");
        f = _wfopen(p, L"rb");
        if (f) {
            q = fread(s_quien, 1, ENV_MAX, f);
            fclose(f);
        }
        InterlockedExchange(&s_quien_len, (LONG)q);
    }
    InterlockedExchange64(&s_env_t0, (LONG64)GetTickCount64());
    InterlockedExchange(&s_env_len, (LONG)l);
}

/* who talks now: 1 Lumis, 2 Umbra, 3 both (0: not known) */
static int voz_quien(void)
{
    LONG n = s_quien_len;
    ULONGLONG i;
    if (n <= 0)
        return 0;
    i = (GetTickCount64() - (ULONGLONG)s_env_t0) / 20u;
    return i < (ULONGLONG)n ? s_quien[i] : 0;
}

/* 0..255 now, or -1 when no voice (or no N.env) is playing */
static int voz_nivel(void)
{
    LONG n = s_env_len;
    ULONGLONG i;
    if (n <= 0 || s_playing < 0)
        return -1;
    i = (GetTickCount64() - (ULONGLONG)s_env_t0) / 20u;
    return i < (ULONGLONG)n ? s_env[i] : -1;
}

/* F2 "Voces": 0 Sigue los FPS, 1 Normal, 2 No */
int ygo_voces_fps_get(void) { return s_mudo ? 2 : s_sigue_fps ? 0 : 1; }
void ygo_voces_fps_set(int i)
{
    InterlockedExchange(&s_sigue_fps, i == 0);
    InterlockedExchange(&s_mudo, i == 2);
    if (i == 2 && s_wake) {
        InterlockedExchange(&s_want, VOZ_STOP);
        SetEvent(s_wake);
    }
}

/* The frames presented a second, over the last half second or more; a
 * stretch with none (F7's pause, a load) keeps the last rate. */
static void measure_fps(void)
{
    static ULONGLONG t0;
    static uint64_t f0;
    ULONGLONG now = GetTickCount64();
    uint64_t f = nv2a_d3d11_presents();
    if (!t0) {
        t0 = now;
        f0 = f;
        return;
    }
    if (now - t0 < 500u)
        return;
    if (f > f0)
        InterlockedExchange(&s_fps, (LONG)((f - f0) * 1000u / (now - t0)));
    t0 = now;
    f0 = f;
}

/* The subfolder of s_dir with the voice for the game's rate: "" (x1),
 * "x1.5\" or "x2\"; a faster one missing falls back to the slower. */
static const wchar_t *speed_dir(LONG n, const wchar_t **name)
{
    static const wchar_t *const dirs[] = { L"", L"x1.5\\", L"x2\\" };
    static const wchar_t *const names[] = { L"x1", L"x1.5", L"x2" };
    LONG fps = s_fps;
    int k = !s_sigue_fps || fps < 75 ? 0 : fps < 105 ? 1 : 2;
    for (; k > 0; --k) {
        wchar_t p[MAX_PATH];
        _snwprintf(p, MAX_PATH, L"%ls\\%ls%ld.mp3", s_dir, dirs[k], n);
        p[MAX_PATH - 1] = 0;
        if (GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES)
            break;
    }
    *name = names[k];
    return dirs[k];
}

static void voice_thread_body(void)
{
    LONG playing = -1;
    for (;;) {
        LONG n;
        WaitForSingleObject(s_wake, 100);
        measure_fps();
        n = InterlockedExchange(&s_want, -1);
        if (n == -1 && playing >= 0 && s_ask_line == playing && s_ask_calls >= VOZ_SEGUIDO
            && GetTickCount64() - (ULONGLONG)s_ask_seen > VOZ_CERRADO) {
            fprintf(stderr, "[VOZ] %ld: el globo se cerró solo\n", playing);
            n = VOZ_STOP;
        }
        if (n == -1)
            continue;
        if (n == VOZ_STOP && playing >= 0) {
            InterlockedExchange(&s_env_len, 0);     /* the mouth closes now */
            /* fades out over VOZ_FUNDIDO ms, quickly but not a click:
             * volume (1-t)^2, so most of the drop comes first */
            int i;
            for (i = 1; i <= VOZ_PASOS; ++i) {
                wchar_t c[64];
                double t = 1.0 - (double)i / VOZ_PASOS;
                _snwprintf(c, 64, L"setaudio ygovoz volume to %d", (int)(1000.0 * t * t));
                c[63] = 0;
                mciSendStringW(c, NULL, 0, NULL);
                Sleep(VOZ_FUNDIDO / VOZ_PASOS);
            }
        }
        if (playing >= 0)
            mciSendStringW(L"close ygovoz", NULL, 0, NULL);
        if (n == VOZ_STOP) {
            if (playing >= 0) {
                fprintf(stderr, "[VOZ] %ld: voz desvanecida\n", playing);
                fflush(stderr);
            }
            playing = -1;
            InterlockedExchange(&s_playing, -1);
            continue;
        }
        playing = -1;
        {
            wchar_t cmd[MAX_PATH + 64];
            MCIERROR e;
            const wchar_t *vel;
            const wchar_t *sub = speed_dir(n, &vel);
            _snwprintf(cmd, MAX_PATH + 64, L"open \"%ls\\%ls%ld.mp3\" type mpegvideo alias ygovoz",
                       s_dir, sub, n);
            if (*sub) {
                fprintf(stderr, "[VOZ] %ld a %ls (%ld fps)\n", n, vel, (long)s_fps);
                fflush(stderr);
            }
            cmd[MAX_PATH + 63] = 0;
            e = mciSendStringW(cmd, NULL, 0, NULL);
            if (!e)
                e = mciSendStringW(L"play ygovoz", NULL, 0, NULL);
            if (!e) {
                wchar_t f[MAX_PATH];
                _snwprintf(f, MAX_PATH, L"%ls\\%ls%ld.mp3", s_dir, sub, n);
                f[MAX_PATH - 1] = 0;
                env_load(f);
            }
            if (e) {
                wchar_t m[128];
                mciGetErrorStringW(e, m, 128);
                fprintf(stderr, "[VOZ] %ld.mp3 no se pudo reproducir: %ls\n", n, m);
                fflush(stderr);
                mciSendStringW(L"close ygovoz", NULL, 0, NULL);
            } else {
                playing = n;
                InterlockedExchange(&s_playing, n);
            }
        }
    }
}

static DWORD WINAPI voice_thread(LPVOID unused)
{
    (void)unused;
    voice_thread_body();
    return 0;
}

/* The new lines over the Spanish table's 317-848 strings, once. */
static void patch_texts(void)
{
    wchar_t path[MAX_PATH];
    FILE *f;
    static char line[2048];
    static char *text[LAST + 1];
    uint32_t tab = MEM32(0x5FC2DCu + 4u * LANG_ES);
    uint32_t base = MEM32(0x5FC2C4u + 4u * LANG_ES);
    uint32_t start, end, at, need = 0, n, got = 0;

    _snwprintf(path, MAX_PATH, L"%ls\\textos.txt", s_dir);
    path[MAX_PATH - 1] = 0;
    f = _wfopen(path, L"rb");
    if (!f) {
        fprintf(stderr, "[VOZ] sin textos.txt: los textos quedan como estaban\n");
        return;
    }
    while (fgets(line, sizeof line, f)) {
        char *tabc = strchr(line, '\t'), *e;
        unsigned long k = strtoul(line, NULL, 10);
        if (!tabc || k < FIRST || k > LAST)
            continue;
        e = tabc + strlen(tabc);
        while (e > tabc + 1 && (e[-1] == '\n' || e[-1] == '\r'))
            *--e = 0;
        free(text[k]);
        text[k] = _strdup(tabc + 1);
    }
    fclose(f);
    if (!tab || MEM32(tab) <= LAST + 1u) {
        fprintf(stderr, "[VOZ] la tabla de textos no es la esperada: sin cambios\n");
        return;
    }
    start = base + MEM32(tab + 4u + 4u * FIRST);
    end = base + MEM32(tab + 4u + 4u * (LAST + 1u));
    for (n = FIRST; n <= LAST; ++n)
        need += (uint32_t)strlen(text[n] ? text[n] : (const char *)XBOX_PTR(base + MEM32(tab + 4u + 4u * n))) + 1u;
    if (end <= start || need > end - start) {
        fprintf(stderr, "[VOZ] los textos nuevos no entran (%u de %u bytes): sin cambios\n",
                need, end - start);
        return;
    }
    {
        /* the originals kept for the lines without a new text */
        static char keep[64 * 1024];
        uint32_t used = 0;
        for (n = FIRST; n <= LAST; ++n)
            if (!text[n]) {
                const char *o = (const char *)XBOX_PTR(base + MEM32(tab + 4u + 4u * n));
                size_t l = strlen(o) + 1u;
                if (used + l > sizeof keep)
                    return;
                memcpy(keep + used, o, l);
                text[n] = keep + used;
                used += (uint32_t)l;
            } else
                ++got;
    }
    at = start;
    for (n = FIRST; n <= LAST; ++n) {
        size_t l = strlen(text[n]) + 1u;
        memcpy((void *)XBOX_PTR(at), text[n], l);
        MEM32(tab + 4u + 4u * n) = at - base;
        at += (uint32_t)l;
    }
    fprintf(stderr, "[VOZ] %u textos en español latino (%u de %u bytes)\n", got, need, end - start);
}

/* ---- Lumis & Umbra: Umbra's mouth (2026-11-20) ------------------------------
 * The user: "le sumamos una boca usando como base las que ya estan en el
 * juego". Their portrait (scripts 19, 46, 73 of the table 0x60F518) animates
 * Lumis's mouth only (track bit 1); Umbra's face has eyes only (the 4th
 * track). tools/boca_umbra.py draws Umbra's mouth half open and open, from
 * his own teeth, in the textures' free corner (textures 98 / 28, 101 of the
 * pack). Here each script gets, once at start (before any portrait loads its
 * sprites, sub_00018290), a 5th track: control bit 3 (slot 3) looping rest
 * (sprite 5, empty, as Lumis's) / half (sprite 8) / open (9) / half with
 * Lumis's mouth's steps (1, 1, 4, 3), and rects 8 and 9 for the pieces. The
 * rects and commands are copied to guest heap with the new ones after
 * (the script's arrays are packed together in the xbe), the count of the
 * track list goes 4 -> 5. The loader marks the sprites a loop reaches (its
 * @4 steps from @6), so 8 and 9 get their objects. Bit 3 follows bit 1 (the
 * game's "talking" bit) in sub_00018790 below: both mouths move together,
 * and with a voice both follow its loudness. Rect frame: the piece's place
 * in the upright view T is the command's (@4, @6); its texels at T (300 or
 * 386, 410), rect (y0, x0, y1, x1) of T = (410, 300 / 386, 410 + h, + w). */
uint32_t xbox_HeapAlloc(uint32_t size, uint32_t alignment);
static const struct { int s; uint16_t x, y, w, h; } k_boca[3] = {
    { 19, 304, 178, 72, 54 }, { 46, 302, 194, 82, 58 }, { 73, 302, 194, 82, 58 } };
static uint32_t s_boca_a[3], s_boca_c[3];   /* the script; its new control command's index */

static void boca_umbra(void)
{
    int i;
    for (i = 0; i < 3; ++i) {
        uint32_t a = MEM32(0x60F518u + 4u * (uint32_t)k_boca[i].s), r, heads, cmds, k, last = 0, nc = 0, nr, c;
        uint32_t nrects, ncmds, j, t;
        if (!a)
            continue;
        r = MEM32(a);
        heads = MEM32(a + 4u);
        cmds = MEM32(a + 8u);
        nr = (heads - r) / 8u;
        if (nr != 8u || MEM8(heads) != 4u) {
            fprintf(stderr, "[VOZ] boca de Umbra: guion %d no es el esperado (%u rects, %u pistas)\n", k_boca[i].s,
                    nr, MEM8(heads));
            continue;
        }
        /* the track list and every command it reaches: the highest index */
        k = MEM8(heads + 1u);
        for (j = 0; j < 4u; ++j) {
            c = cmds + 8u * k;
            if (k + 1u > nc)
                nc = k + 1u;
            if (MEM8(c + 1u) & 0x80u) {
                uint32_t s = MEM16(c + 6u);
                for (t = 0; t < MEM16(c + 4u) && s != 0xFFu; ++t) {
                    if (s + 1u > nc)
                        nc = s + 1u;
                    s = MEM8(cmds + 8u * s);
                }
            }
            last = k;
            k = MEM8(c);
        }
        if (k != 0xFFu || nc > 40u) {
            fprintf(stderr, "[VOZ] boca de Umbra: guion %d, lista rara (fin %u, %u comandos)\n", k_boca[i].s, k, nc);
            continue;
        }
        nrects = xbox_HeapAlloc(10u * 8u, 4);
        ncmds = xbox_HeapAlloc((nc + 5u) * 8u, 4);
        if (!nrects || !ncmds)
            continue;
        memcpy((void *)XBOX_PTR(nrects), (const void *)XBOX_PTR(r), 8u * 8u);
        for (j = 0; j < 2u; ++j) {
            uint32_t q = nrects + 8u * (8u + j);
            uint16_t fx = (uint16_t)(j ? 386u : 300u);
            MEM16(q) = 410u;
            MEM16(q + 2u) = fx;
            MEM16(q + 4u) = (uint16_t)(410u + k_boca[i].h);
            MEM16(q + 6u) = (uint16_t)(fx + k_boca[i].w);
        }
        memcpy((void *)XBOX_PTR(ncmds), (const void *)XBOX_PTR(cmds), nc * 8u);
        MEM8(ncmds + 8u * last) = (uint8_t)nc;                  /* the old last track -> ours */
        {
            /* next, steps (or 0x80 | bit), sprite, x, y */
            static const uint8_t paso[4][3] = { { 1, 1, 5 }, { 2, 1, 8 }, { 3, 4, 9 }, { 0xFF, 3, 8 } };
            c = ncmds + 8u * nc;
            MEM8(c) = 0xFFu;
            MEM8(c + 1u) = 0x80u;
            MEM16(c + 2u) = 3u;
            MEM16(c + 4u) = 4u;
            MEM16(c + 6u) = (uint16_t)(nc + 1u);
            for (j = 0; j < 4u; ++j) {
                c = ncmds + 8u * (nc + 1u + j);
                MEM8(c) = (uint8_t)(paso[j][0] == 0xFFu ? 0xFFu : nc + 1u + paso[j][0]);
                MEM8(c + 1u) = paso[j][1];
                MEM16(c + 2u) = paso[j][2];
                MEM16(c + 4u) = paso[j][2] == 5u ? 0u : k_boca[i].x;
                MEM16(c + 6u) = paso[j][2] == 5u ? 0u : k_boca[i].y;
            }
        }
        MEM32(a) = nrects;
        MEM32(a + 8u) = ncmds;
        MEM8(heads) = 5u;
        s_boca_a[i] = a;
        s_boca_c[i] = nc;
        fprintf(stderr, "[VOZ] boca de Umbra en el guion %d (%08X): comandos %u -> %u, rects 8 -> 10\n", k_boca[i].s, a,
                nc, nc + 5u);
    }
}

/* the script's new control command, or -1 */
static int boca_de(uint32_t a)
{
    int i;
    for (i = 0; i < 3; ++i)
        if (a && s_boca_a[i] == a)
            return (int)s_boca_c[i];
    return -1;
}

static void voces_init(void)
{
    const char *e = getenv("YGO_VOCES");
    boca_umbra();
    s_on = 0;
    if (!e || !*e)
        return;
    MultiByteToWideChar(CP_ACP, 0, e, -1, s_dir, MAX_PATH);
    if (GetFileAttributesW(s_dir) == INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "[VOZ] no existe la carpeta YGO_VOCES=%s\n", e);
        return;
    }
    s_on = 1;
    patch_texts();
    s_wake = CreateEventW(NULL, FALSE, FALSE, NULL);
    CloseHandle(CreateThread(NULL, 0, voice_thread, NULL, 0, NULL));
    fflush(stderr);
}

/* raton.c (2026-11-17): a voice line's balloon is up (typing or whole) and
 * its voice plays: a left click anywhere is A for it (fill / close), not
 * what the screen under it would do with the click (the rock-paper-scissors
 * screen runs under the balloon: a click off its middle was a hand step). */
int ygo_voz_globo_arriba(void)
{
    LONG playing = s_playing;
    uint32_t g = (uint32_t)s_globo, st;
    if (s_on != 1 || playing < 0 || !g || s_globo_n != playing)
        return 0;
    st = MEM32(g + 0x17C0u);
    return st == 1u || st == 2u;
}

/* mando_directo.c: A or B now (pad state the game reads). One press only
 * fills the balloon (the game's own); a second within VOZ_DOBLE ms while a
 * voice plays fades it out. */
void ygo_voz_botones(int a_or_b)
{
    static int was;
    static ULONGLONG last_press;
    if (s_on == 1 && a_or_b && !was) {
        ULONGLONG now = GetTickCount64();
        LONG playing = s_playing;
        uint32_t g = (uint32_t)s_globo;
        if (playing >= 0 && now - (ULONGLONG)s_started > VOZ_GRACIA) {
            if (g && s_globo_n == playing && MEM32(g + 0x17C0u) == 2u) {
                fprintf(stderr, "[VOZ] %ld: globo completo cerrado\n", playing);
                InterlockedExchange(&s_want, VOZ_STOP);
                SetEvent(s_wake);
                last_press = 0;
            } else if (last_press && now - last_press <= VOZ_DOBLE) {
                fprintf(stderr, "[VOZ] doble pulsación\n");
                InterlockedExchange(&s_want, VOZ_STOP);
                SetEvent(s_wake);
                last_press = 0;
            } else
                last_press = now;
        } else
            last_press = 0;
    }
    was = a_or_b;
}

static void voice_for(uint32_t lang, uint32_t n)
{
    static uint32_t last = ~0u;
    static ULONGLONG seen, first;
    static unsigned calls;
    ULONGLONG now;
    if (lang != LANG_ES || n < FIRST || n > LAST)
        return;
    now = GetTickCount64();
    if (n != last || now - seen > 2000u) {
        if (last != ~0u)
            fprintf(stderr, "[VOZ]   (%u pedido %u veces en %llu ms)\n", last, calls,
                    (unsigned long long)(seen - first));
        calls = 0;
        first = now;
        wchar_t p[MAX_PATH];
        int has;
        _snwprintf(p, MAX_PATH, L"%ls\\%u.mp3", s_dir, n);
        p[MAX_PATH - 1] = 0;
        has = GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES;
        fprintf(stderr, "[VOZ] texto %u%s\n", n, has ? "" : " (sin audio)");
        fflush(stderr);
        if (has && !s_mudo) {
            InterlockedExchange64(&s_started, (LONG64)now);
            InterlockedExchange(&s_want, (LONG)n);
            SetEvent(s_wake);
        }
    }
    last = n;
    seen = now;
    ++calls;
    InterlockedExchange64(&s_ask_seen, (LONG64)now);
    InterlockedExchange(&s_ask_calls, (LONG)calls);
    InterlockedExchange(&s_ask_line, (LONG)n);
}

void sub_0002C5C0(void)
{
    uint32_t n = MEM32(g_esp + 4);
    if (s_on < 0)
        voces_init();
    sub_0002C5C0_pad();
    {
        extern uint32_t ygo_cartas_texto(uint32_t n, uint32_t txt);
        g_eax = ygo_cartas_texto(n, g_eax);
    }
    if (s_on) {
        voice_for(MEM32(0x5FC2C0u), n);
        if (MEM32(0x5FC2C0u) == LANG_ES && n >= FIRST && n <= LAST) {
            InterlockedExchange(&s_line_txt, (LONG)g_eax);
            InterlockedExchange(&s_line_n, (LONG)n);
        }
    }
}

/* The text objects' SetText: the one given the voiced line is its balloon
 * (by the same pointer, or the same text if the caller copied it). */
void sub_0002B9D0_pad(void);
void sub_0002B9D0(void)
{
    uint32_t self = g_ecx, str = MEM32(g_esp + 4), want = (uint32_t)s_line_txt;
    if (s_on == 1 && want && str
        && (str == want || strcmp((const char *)XBOX_PTR(str), (const char *)XBOX_PTR(want)) == 0)) {
        InterlockedExchange(&s_globo, (LONG)self);
        InterlockedExchange(&s_globo_n, s_line_n);
    }
    sub_0002B9D0_pad();
}

void sub_0002C590(void)
{
    uint32_t n = MEM32(g_esp + 4), lang = MEM32(g_esp + 8);
    if (s_on < 0)
        voces_init();
    sub_0002C590_pad();
    if (s_on) {
        voice_for(lang, n);
        if (lang == LANG_ES && n >= FIRST && n <= LAST) {
            InterlockedExchange(&s_line_txt, (LONG)g_eax);
            InterlockedExchange(&s_line_n, (LONG)n);
        }
    }
}

/* ---- the mouth with the voice ------------------------------------------ */
#define BOCA_MEDIA 40      /* loudness from which the mouth is half open */
#define BOCA_ABIERTA 140   /* and open */
#define BOCA_MIN 60u       /* ms each mouth is held at least */

void sub_00018790_pad(void);

/* the portrait scripts (table 0x60F518): is a one of them? */
static int es_retrato(uint32_t a)
{
    static uint32_t t[81];
    static int n = -1;
    int i;
    if (n < 0) {
        for (i = 0; i < 81; ++i)
            t[i] = MEM32(0x60F518u + 4u * i);
        n = 81;
    }
    for (i = 0; i < n; ++i)
        if (t[i] == a)
            return 1;
    return 0;
}

void sub_00018790(void)
{
    static uint32_t face;          /* the portrait that talks the current line */
    static LONG face_line = -1;
    static ULONGLONG held_at;
    static int held = -1;
    uint32_t o = g_ecx, a = o ? MEM32(o + 0x4B4u) : 0u;
    LONG line = s_playing;
    int lvl, bc = o ? boca_de(a) : -1;
    if (bc >= 0) {
        /* Umbra's mouth (bit 3) talks when Lumis's (bit 1, the game's) does */
        uint32_t v = MEM32(o + 0x50Cu);
        MEM32(o + 0x50Cu) = (v & ~8u) | ((v & 2u) << 2);
    }
    if (s_on != 1 || !a || MEM32(o + 0x508u) != 0u || !es_retrato(a)) {
        sub_00018790_pad();
        return;
    }
    if (line != face_line) {
        face_line = line;
        face = 0;
        held = -1;
    }
    /* the one the game makes talk for this line */
    if (line >= 0 && !face && (MEM32(o + 0x50Cu) & 2u))
        face = o;
    lvl = voz_nivel();
    if (o != face || lvl < 0) {
        sub_00018790_pad();
        return;
    }
    {
        /* the mouth track's loop: rest -> half -> open (all 81 scripts) */
        uint32_t heads = MEM32(a + 4u), cmds = MEM32(a + 8u), c, rest, half, open, want, old;
        uint32_t k = MEM8(heads + 1u), t;
        int shape, quien;
        ULONGLONG now = GetTickCount64();
        for (t = 0; t < 2u; ++t)
            k = MEM8(cmds + 8u * k);
        c = cmds + 8u * k;
        if (!(MEM8(c + 1u) & 0x80u) || MEM8(c + 2u) != 1u) {
            sub_00018790_pad();
            return;
        }
        rest = MEM16(c + 6u);
        half = MEM8(cmds + 8u * rest);
        open = MEM8(cmds + 8u * half);
        shape = lvl >= BOCA_ABIERTA ? 2 : lvl >= BOCA_MEDIA ? 1 : 0;
        if (held >= 0 && shape != held && now - held_at < BOCA_MIN)
            shape = held;
        if (shape != held) {
            held = shape;
            held_at = now;
        }
        want = shape == 2 ? open : shape == 1 ? half : rest;
        quien = bc >= 0 ? voz_quien() : 0;
        if (quien == 2)
            want = rest;                /* Umbra's part: Lumis's mouth shut */
        old = MEM32(o + 0x50Cu);
        MEM32(o + 0x50Cu) = old | 2u;
        MEM32(o + 0x53Cu + 4u) = want;     /* slot 1: the fixed face track has none */
        MEM32(o + 0x514u + 4u) = 2u;     /* one frame down, not ended: drawn as is */
        /* 2026-11-20: Lumis & Umbra (scripts 19, 46, 73) have no second
         * mouth: their 4th track (sprites 6/7, 32x56, steps 1-3-6-1 as the
         * blink; script 73 runs it on the blink's bit 0) is the second
         * face's eyes. Driving it with the voice (02:3x) is undone; Umbra's
         * mouth is the 5th track boca_umbra() adds (slot 3: rest, half,
         * open at bc + 1..3), moved with the same shape. */
        if (bc >= 0) {
            MEM32(o + 0x50Cu) |= 8u;
            /* Lumis's part (tools/voces_quien.py): Umbra's mouth shut */
            MEM32(o + 0x53Cu + 12u) = (uint32_t)bc + 1u + (quien == 1 ? 0u : (uint32_t)shape);
            MEM32(o + 0x514u + 12u) = 2u;
        }
        sub_00018790_pad();
        MEM32(o + 0x50Cu) = (MEM32(o + 0x50Cu) & ~10u) | (old & 2u) | ((old & 2u) << 2);
    }
}
