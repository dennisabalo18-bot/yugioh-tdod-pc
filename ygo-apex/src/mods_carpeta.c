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
