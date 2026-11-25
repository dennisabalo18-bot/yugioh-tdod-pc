/*
 * perfil_dir.c - profiles, one folder each, and inside it one folder per
 * game slot (2026-11-25, the user: "Carpeta por perfil", then "quiero por
 * ranura"; mock-up approved).
 *
 *   Partidas\<profile>\UDATA, TDATA     the game's save (all its slots),
 *                                       RECOMP_SAVE_DIR
 *   Partidas\<profile>\respaldos\       savedata.dat copies (guardado.c)
 *   Partidas\<profile>\Ranura N\        ours for the game's slot N - 1:
 *       perfil.ini, duelos.txt, prestados.txt, cartas_nuevas.txt,
 *       premios_registro.txt
 *
 * YGO_PERFIL is Partidas\<profile>. The active profile's name is in
 * Ajustes\perfil_activo.txt (UTF-8, one line; Configuración.exe writes it).
 * The game's slot: u32 0x65189C (the one sub_0002D830(-1) saves to, read in
 * guardado.c), 0..4 (savedata.dat 0x14A00 = 5 slots of 0x41C8 + 0x1C).
 * Each module reloads its files when that slot changes (ygo_ranura()).
 *
 * The old layout (Partidas\UDATA, Partidas\Perfil\...) moves into the first
 * profile, named after perfil.ini's name ("Jugador 1" if none): the save
 * and respaldos to the profile, the rest to Ranura 1 (the user played slot
 * 0: "[GUARDADO] ... (ranura 0)"). Configuración.exe does the same move.
 */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "recomp/gen/recomp_types.h"

unsigned ygo_ranura(void)
{
    uint32_t r = MEM32(0x0065189Cu);
    return r < 5u ? r : 0u;
}

/* YGO_PERFIL\Ranura N\<file> (the folder made) */
void ygo_perfil_ruta(const char *archivo, char *out, size_t cap)
{
    const char *dir = getenv("YGO_PERFIL");
    char d[MAX_PATH];
    if (!dir || !*dir) {
        snprintf(out, cap, "%s", archivo);
        return;
    }
    snprintf(d, sizeof d, "%s\\Ranura %u", dir, ygo_ranura() + 1u);
    CreateDirectoryA(d, NULL);
    snprintf(out, cap, "%s\\%s", d, archivo);
}

void ygo_perfil_wruta(const wchar_t *archivo, wchar_t *out, size_t cap)
{
    const wchar_t *dir = _wgetenv(L"YGO_PERFIL");
    wchar_t d[MAX_PATH];
    if (!dir || !*dir) {
        _snwprintf(out, cap, L"%ls", archivo);
        out[cap - 1] = 0;
        return;
    }
    _snwprintf(d, MAX_PATH, L"%ls\\Ranura %u", dir, ygo_ranura() + 1u);
    d[MAX_PATH - 1] = 0;
    CreateDirectoryW(d, NULL);
    _snwprintf(out, cap, L"%ls\\%ls", d, archivo);
    out[cap - 1] = 0;
}

/* 2026-11-25 (Informe de error): the game closed on an error. A mark in
 * YGO_AJUSTES\ultimo_error.txt; Configuración.exe sees it the next time it
 * opens and offers to build the report (registro.txt and the rest in a zip). */
void ygo_marcar_error(const char *motivo)
{
    const char *dir = getenv("YGO_AJUSTES");
    char p[MAX_PATH];
    SYSTEMTIME t;
    FILE *f;
    if (!dir || !*dir)
        return;
    snprintf(p, sizeof p, "%s\\ultimo_error.txt", dir);
    GetLocalTime(&t);
    if ((f = fopen(p, "wb")) != NULL) {
        fprintf(f, "fecha=%04u-%02u-%02u %02u:%02u:%02u\nmotivo=%s\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
                t.wSecond, motivo ? motivo : "?");
        fclose(f);
    }
}

/* 2026-11-25 (the package for players): a few of the port's files must sit
 * inside the game's own folder (the rival's card back card\normal\
 * card_ura_rival.tga, fundas.c; Exodia whole for the mascot and the attack,
 * effect\special_igb\se002_mascota.igb / se002_ataque.igb). They ship in
 * Mods-Extras\Contenido\juego\ with the same layout and are copied into the
 * folder that has default.xbe (wherever it is inside Game\) when missing or
 * different. */
static void overlay_dir(const char *src, const char *dst)
{
    char pat[MAX_PATH], s[MAX_PATH], d[MAX_PATH];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    snprintf(pat, sizeof pat, "%s\\*", src);
    if ((h = FindFirstFileA(pat, &fd)) == INVALID_HANDLE_VALUE)
        return;
    CreateDirectoryA(dst, NULL);
    do {
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, ".."))
            continue;
        snprintf(s, sizeof s, "%s\\%s", src, fd.cFileName);
        snprintf(d, sizeof d, "%s\\%s", dst, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            overlay_dir(s, d);
        else {
            WIN32_FILE_ATTRIBUTE_DATA b;
            if (GetFileAttributesExA(d, GetFileExInfoStandard, &b) && b.nFileSizeLow == fd.nFileSizeLow
                    && b.nFileSizeHigh == fd.nFileSizeHigh)
                continue;
            if (CopyFileA(s, d, FALSE))
                fprintf(stderr, "[JUEGO] copiado del port: %s\n", d);
            else
                fprintf(stderr, "[JUEGO] no se pudo copiar %s (error %lu)\n", d, GetLastError());
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

void ygo_juego_overlay(const char *game_dir)
{
    extern const char *ygo_mods_a(void);
    char exe[MAX_PATH], src[MAX_PATH], *barra;
    DWORD k = GetModuleFileNameA(NULL, exe, MAX_PATH);
    if (!game_dir || !*game_dir || !k || k >= MAX_PATH || !(barra = strrchr(exe, '\\')))
        return;
    *barra = 0;
    snprintf(src, sizeof src, "%s%s\\juego", exe, ygo_mods_a());
    overlay_dir(src, game_dir);
}

/* ---- at start (main.c, before the save folder is handed to the kit) ------ */

/* a folder name from a profile name: the letters Windows refuses out, the
 * spaces and dots at the ends too */
static void limpiar(wchar_t *n)
{
    wchar_t *s = n, *d = n;
    size_t L;
    for (; *s; ++s)
        if (*s >= 32 && !wcschr(L"\\/:*?\"<>|", *s)) {
            /* 2026-11-25 (review): the save folder reaches the kit as ANSI
             * (RECOMP_SAVE_DIR): a letter this PC's code page lacks would
             * break it, so it becomes '_' (Perfiles.cs does the same) */
            char b[8];
            BOOL usado = FALSE;
            if (WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, s, 1, b, sizeof b, NULL, &usado) != 1 || usado)
                *d++ = L'_';
            else
                *d++ = *s;
        }
    *d = 0;
    while (*n == L' ' || *n == L'.')
        memmove(n, n + 1, wcslen(n) * sizeof *n);
    L = wcslen(n);
    while (L && (n[L - 1] == L' ' || n[L - 1] == L'.'))
        n[--L] = 0;
    if (!*n)
        wcscpy(n, L"Jugador 1");
}

static void mover(const wchar_t *de, const wchar_t *a)
{
    if (GetFileAttributesW(de) == INVALID_FILE_ATTRIBUTES)
        return;
    if (!MoveFileExW(de, a, MOVEFILE_COPY_ALLOWED))
        fprintf(stderr, "[PERFILES] no se pudo mover %ls -> %ls (error %lu)\n", de, a, GetLastError());
    else
        fprintf(stderr, "[PERFILES] movido %ls -> %ls\n", de, a);
}

/* the old Partidas\UDATA + Partidas\Perfil into Partidas\<name> */
static void migrar(const wchar_t *partidas, wchar_t *nombre, size_t cap)
{
    wchar_t p[MAX_PATH], q[MAX_PATH], base[MAX_PATH], ran[MAX_PATH];
    static const wchar_t *const k_arch[] = { L"perfil.ini", L"duelos.txt", L"prestados.txt", L"cartas_nuevas.txt",
                                             L"premios_registro.txt" };
    FILE *f;
    size_t i;
    _snwprintf(p, MAX_PATH, L"%ls\\UDATA", partidas);
    _snwprintf(q, MAX_PATH, L"%ls\\Perfil", partidas);
    if (GetFileAttributesW(p) == INVALID_FILE_ATTRIBUTES && GetFileAttributesW(q) == INVALID_FILE_ATTRIBUTES)
        return;
    /* the name: perfil.ini's, else Jugador 1 */
    wcscpy(nombre, L"Jugador 1");
    _snwprintf(p, MAX_PATH, L"%ls\\Perfil\\perfil.ini", partidas);
    if ((f = _wfopen(p, L"rb")) != NULL) {
        char l[128];
        while (fgets(l, sizeof l, f))
            if (!strncmp(l, "nombre=", 7)) {
                char *e = l + strlen(l);
                while (e > l + 7 && (e[-1] == '\n' || e[-1] == '\r'))
                    *--e = 0;
                if (l[7])
                    MultiByteToWideChar(CP_UTF8, 0, l + 7, -1, nombre, (int)cap);
            }
        fclose(f);
    }
    limpiar(nombre);
    _snwprintf(base, MAX_PATH, L"%ls\\%ls", partidas, nombre);
    CreateDirectoryW(base, NULL);
    _snwprintf(ran, MAX_PATH, L"%ls\\Ranura 1", base);
    CreateDirectoryW(ran, NULL);
    _snwprintf(p, MAX_PATH, L"%ls\\UDATA", partidas);
    _snwprintf(q, MAX_PATH, L"%ls\\UDATA", base);
    mover(p, q);
    _snwprintf(p, MAX_PATH, L"%ls\\TDATA", partidas);
    _snwprintf(q, MAX_PATH, L"%ls\\TDATA", base);
    mover(p, q);
    _snwprintf(p, MAX_PATH, L"%ls\\Perfil\\respaldos", partidas);
    _snwprintf(q, MAX_PATH, L"%ls\\respaldos", base);
    mover(p, q);
    for (i = 0; i < sizeof k_arch / sizeof k_arch[0]; ++i) {
        _snwprintf(p, MAX_PATH, L"%ls\\Perfil\\%ls", partidas, k_arch[i]);
        _snwprintf(q, MAX_PATH, L"%ls\\%ls", ran, k_arch[i]);
        mover(p, q);
    }
    _snwprintf(p, MAX_PATH, L"%ls\\Perfil", partidas);
    RemoveDirectoryW(p);                /* only if nothing else was left in it */
    fprintf(stderr, "[PERFILES] partida vieja pasada al perfil %ls (Ranura 1)\n", nombre);
}

/* sets RECOMP_SAVE_DIR and YGO_PERFIL to Partidas\<active profile>; root is
 * the port's folder (ANSI, as main.c has it) */
void ygo_perfiles_iniciar(const char *root)
{
    wchar_t wroot[MAX_PATH], partidas[MAX_PATH], activo[MAX_PATH], nombre[128] = L"", dir[MAX_PATH];
    char a[MAX_PATH * 2];
    FILE *f;
    if (!MultiByteToWideChar(CP_ACP, 0, root, -1, wroot, MAX_PATH))
        return;
    _snwprintf(partidas, MAX_PATH, L"%ls\\Partidas", wroot);
    CreateDirectoryW(partidas, NULL);
    _snwprintf(dir, MAX_PATH, L"%ls\\Ajustes", wroot);
    CreateDirectoryW(dir, NULL);
    _snwprintf(activo, MAX_PATH, L"%ls\\Ajustes\\perfil_activo.txt", wroot);
    /* the active one (Configuración.exe) */
    if ((f = _wfopen(activo, L"rb")) != NULL) {
        char l[256];
        if (fgets(l, sizeof l, f)) {
            char *s = l, *e;
            if ((unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF)
                s += 3;
            e = s + strlen(s);
            while (e > s && (e[-1] == '\n' || e[-1] == '\r'))
                *--e = 0;
            MultiByteToWideChar(CP_UTF8, 0, s, -1, nombre, 128);
        }
        fclose(f);
    }
    /* the old layout first (its name wins only when no profile was chosen) */
    {
        wchar_t viejo[128];
        _snwprintf(dir, MAX_PATH, L"%ls\\UDATA", partidas);
        if (GetFileAttributesW(dir) != INVALID_FILE_ATTRIBUTES
                || (_snwprintf(dir, MAX_PATH, L"%ls\\Perfil", partidas), GetFileAttributesW(dir) != INVALID_FILE_ATTRIBUTES)) {
            migrar(partidas, viejo, 128);
            if (!nombre[0])
                wcscpy(nombre, viejo);
        }
    }
    limpiar(nombre);
    _snwprintf(dir, MAX_PATH, L"%ls\\%ls", partidas, nombre);
    CreateDirectoryW(dir, NULL);
    if ((f = _wfopen(activo, L"wb")) != NULL) {
        char u8[512];
        WideCharToMultiByte(CP_UTF8, 0, nombre, -1, u8, sizeof u8, NULL, NULL);
        fprintf(f, "%s\r\n", u8);
        fclose(f);
    }
    _wputenv_s(L"YGO_PERFIL_NOMBRE", nombre);
    if (WideCharToMultiByte(CP_ACP, 0, dir, -1, a, sizeof a, NULL, NULL)) {
        SetEnvironmentVariableA("RECOMP_SAVE_DIR", a);
        SetEnvironmentVariableA("YGO_PERFIL", a);
        _putenv_s("RECOMP_SAVE_DIR", a);
        _putenv_s("YGO_PERFIL", a);
    }
    _wputenv_s(L"YGO_PERFIL", dir);
    fprintf(stderr, "[PERFILES] perfil activo: %ls (%ls)\n", nombre, dir);
}
