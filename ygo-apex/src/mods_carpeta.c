/* see mods_carpeta.h */
#include <windows.h>
#include "mods_carpeta.h"

static int contenido(void)
{
    static int s = -1;
    if (s < 0) {
        char p[MAX_PATH + 32], *barra;
        DWORD n = GetModuleFileNameA(NULL, p, MAX_PATH);
        DWORD a;
        s = 0;
        if (n && n < MAX_PATH && (barra = strrchr(p, '\\')) != NULL) {
            strcpy(barra, "\\Mods-Extras\\Contenido");
            a = GetFileAttributesA(p);
            s = a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
        }
    }
    return s;
}

const char *ygo_mods_a(void)
{
    return contenido() ? "\\Mods-Extras\\Contenido" : "\\Mods-Extras";
}

const wchar_t *ygo_mods_w(void)
{
    return contenido() ? L"\\Mods-Extras\\Contenido" : L"\\Mods-Extras";
}

/* 2026-11-25 (the user: "para instalar las cosas descargadas ... es medio
 * confuso"): the Drive zips start with Mods-Extras\, to unpack into the port's
 * folder, but Windows' "Extraer todo" proposes a new folder named after the
 * zip (root\YuGiOh-Contenido\Mods-Extras\...), and unpacking into Mods-Extras
 * gives Mods-Extras\Mods-Extras\... Every folder named Mods-Extras found that
 * way (3 levels down, not under Game\, Partidas\ or Ajustes\) is merged into
 * root\Mods-Extras and the folders it leaves empty are removed. Called before
 * anything reads Mods-Extras (main.c portable_defaults). */
#include <stdio.h>
#include <string.h>

static void juntar(const char *src, const char *dst)
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
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            juntar(s, d);
            RemoveDirectoryA(s);
        } else if (!MoveFileExA(s, d, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
            fprintf(stderr, "[MODS] no se pudo mover %s (error %lu)\n", s, GetLastError());
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

static int g_n;
static char g_hallados[16][MAX_PATH];

static void buscar(const char *dir, const char *mods, int prof)
{
    char pat[MAX_PATH], s[MAX_PATH];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    snprintf(pat, sizeof pat, "%s\\*", dir);
    if ((h = FindFirstFileA(pat, &fd)) == INVALID_HANDLE_VALUE)
        return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || !strcmp(fd.cFileName, ".")
                || !strcmp(fd.cFileName, ".."))
            continue;
        if (prof == 0 && (!_stricmp(fd.cFileName, "Game") || !_stricmp(fd.cFileName, "Partidas")
                          || !_stricmp(fd.cFileName, "Ajustes")))
            continue;
        snprintf(s, sizeof s, "%s\\%s", dir, fd.cFileName);
        if (!_stricmp(fd.cFileName, "Mods-Extras") && _stricmp(s, mods) && g_n < 16)
            strcpy(g_hallados[g_n++], s);
        else if (prof < 2)
            buscar(s, mods, prof + 1);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

void ygo_mods_acomodar(const char *root)
{
    char mods[MAX_PATH], p[MAX_PATH], *b;
    int i;
    snprintf(mods, sizeof mods, "%s\\Mods-Extras", root);
    g_n = 0;
    buscar(root, mods, 0);
    for (i = 0; i < g_n; i++) {
        fprintf(stderr, "[MODS] acomodado: %s -> %s\n", g_hallados[i], mods);
        juntar(g_hallados[i], mods);
        strcpy(p, g_hallados[i]);
        /* the folder and its empty parents up to the port's folder */
        while (strlen(p) > strlen(root) && _stricmp(p, mods) && RemoveDirectoryA(p) && (b = strrchr(p, '\\')))
            *b = 0;
    }
}
