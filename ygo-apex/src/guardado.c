/*
 * guardado.c - the game saved after a won duel (2026-11-18, the user's list:
 * "que se guarde la partida cuando se gane un duelo o torneo"; chose: the
 * game's own save, overwriting, with backups).
 *
 * The game's save, read in the code: sub_0002D830(slot) (the save page calls
 * it through sub_0010E050 with its slot; slot < 0: the current one, u32
 * 0x65189C): it reads savedata.dat into a 0x14A00 buffer, copies the playing
 * block (0x6518A8, 0x1072 dwords) into slot * 0x41C8 + 0x1C and writes the
 * file again, waiting for it (its loops pump the file thread, sub_0001BF20);
 * al = 1 when it went well. The prize cards are already in the chest by then
 * (sub_000D9720, one per prize, before the menu comes back).
 *
 * When: a won duel (estadisticas.c, result 0x40 of a finished duel) leaves it
 * pending; it is done on the main menu's wheel (sub_0010D6C0, raton.c) once it
 * has been waiting for keys (state 0) for a second and no duel scene is up, so
 * a tournament is saved when it is over and you are back at the menu. First
 * the file is copied to YGO_PERFIL\respaldos\savedata_<date>.dat (the last 5
 * kept). F2 / Configuracion "Guardar al ganar" (Si by default).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "recomp/gen/recomp_types.h"

BOOL xbox_translate_path(const char *xbox_path, WCHAR *host_path_buf, DWORD buf_size);   /* the kit */
void sub_0002D830(void);

static volatile LONG s_off;             /* 1: "Guardar al ganar" No */
static volatile LONG s_pend;            /* a won duel not saved yet */

int ygo_guardado_get(void) { return s_off ? 1 : 0; }     /* F2: "Sí", "No" */
void ygo_guardado_set(int i) { InterlockedExchange(&s_off, i == 1); }

/* estadisticas.c: a finished duel (2026-11-19, the user: "que se guarde cuando
 * gane el duelo o pierda? o empate? ... a veces te jugas 2 duelos seguidos sin
 * pasar por el menu"): any result. gano: the prizes still to come (they go to
 * the chest on the prize screen, after the scene, sub_000D9720) */
static volatile LONG s_gano, s_premios, s_respaldado;
void ygo_guardado_terminado(int gano)
{
    InterlockedExchange(&s_gano, gano != 0);
    InterlockedExchange(&s_premios, gano != 0);
    InterlockedExchange(&s_respaldado, 0);
    InterlockedExchange(&s_pend, 1);
    fprintf(stderr, "[GUARDADO] duelo terminado (%s): se guarda al cerrar la escena%s%s\n",
            gano ? "ganado" : "perdido o empate", gano ? " y con cada premio" : "", s_off ? " (apagado en F2)" : "");
    fflush(stderr);
}

static int por_fecha(const void *a, const void *b) { return wcscmp((const wchar_t *)a, (const wchar_t *)b); }

/* savedata.dat into YGO_PERFIL\respaldos, the last 5 kept */
static void respaldo(void)
{
    WCHAR src[MAX_PATH], dir[MAX_PATH], dst[MAX_PATH], pat[MAX_PATH];
    static WCHAR nombres[64][MAX_PATH];
    const char *aj = getenv("YGO_PERFIL");
    WIN32_FIND_DATAW fd;
    HANDLE h;
    SYSTEMTIME t;
    int n = 0, i;
    if (!aj || !*aj
            || !xbox_translate_path("\\Device\\Harddisk0\\partition1\\UDATA\\4b4e001e\\0061006D1822\\savedata.dat",
                                    src, MAX_PATH))
        return;
    MultiByteToWideChar(CP_ACP, 0, aj, -1, dir, MAX_PATH);
    wcscat(dir, L"\\respaldos");
    CreateDirectoryW(dir, NULL);
    GetLocalTime(&t);
    _snwprintf(dst, MAX_PATH, L"%ls\\savedata_%04u%02u%02u_%02u%02u%02u.dat", dir, t.wYear, t.wMonth, t.wDay,
               t.wHour, t.wMinute, t.wSecond);
    dst[MAX_PATH - 1] = 0;
    if (!CopyFileW(src, dst, FALSE)) {
        fprintf(stderr, "[GUARDADO] no se pudo copiar el respaldo (error %lu)\n", GetLastError());
        return;
    }
    fprintf(stderr, "[GUARDADO] respaldo: %ls\n", dst);
    _snwprintf(pat, MAX_PATH, L"%ls\\savedata_*.dat", dir);
    pat[MAX_PATH - 1] = 0;
    h = FindFirstFileW(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (n < 64)
                wcscpy(nombres[n++], fd.cFileName);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    qsort(nombres, (size_t)n, sizeof nombres[0], por_fecha);
    for (i = 0; i + 5 < n; i++) {
        _snwprintf(pat, MAX_PATH, L"%ls\\%ls", dir, nombres[i]);
        pat[MAX_PATH - 1] = 0;
        DeleteFileW(pat);
    }
}

/* the game's own save of the current slot (sub_0002D830(-1)), on the game's
 * thread; the file backed up first, once per duel */
static void guardar(const char *cuando)
{
    static int en_curso;
    uint32_t sa = g_eax, sb = g_ebx, sc = g_ecx, sd = g_edx, si = g_esi, di = g_edi, bp = g_ebp, sp = g_esp;
    int ok;
    if (s_off || en_curso)
        return;
    en_curso = 1;
    InterlockedExchange(&s_pend, 0);
    if (!InterlockedExchange(&s_respaldado, 1))
        respaldo();
    PUSH32(g_esp, 0xFFFFFFFFu);         /* slot: the current one */
    PUSH32(g_esp, 0u);
    sub_0002D830();
    ok = (g_eax & 0xFFu) != 0u;
    g_esp = sp;
    g_eax = sa; g_ebx = sb; g_ecx = sc; g_edx = sd; g_esi = si; g_edi = di; g_ebp = bp;
    fprintf(stderr, "[GUARDADO] partida guardada %s (ranura %u): %s\n", cuando, MEM32(0x0065189Cu),
            ok ? "bien" : "FALLO");
    fflush(stderr);
    en_curso = 0;
}

/* mascota.c, right after the duel scene is taken down (sub_00045750) */
void ygo_guardado_tras_escena(void)
{
    if (s_pend)
        guardar("al cerrar el duelo");
}

/* cartas.c, after each prize card goes to the chest (sub_000D9720) */
void ygo_guardado_premio(void)
{
    if (s_premios)
        guardar("con el premio");
}

/* raton.c, the main menu's wheel each frame (sub_0010D6C0, before the game's
 * own), st = its state (0: waiting for keys): whatever is still pending, and
 * the prizes are over */
void ygo_guardado_menu(uint32_t st)
{
    static ULONGLONG desde;
    ULONGLONG t = GetTickCount64();
    if (st != 0u || MEM32(0x00656F88u) != 0u) {
        desde = 0;
        return;
    }
    InterlockedExchange(&s_premios, 0);
    if (!desde)
        desde = t;
    /* 2026-11-19 07:1x (registro 07:14: la rueda estuvo en 0 solo 563 ms y no
     * se guardo): 150 ms, unos cuadros */
    if (!s_pend || t - desde < 150u)
        return;
    guardar("en el menu");
}
