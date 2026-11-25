/* ygo-port 2026-11-21 (the user: "deja voces, texturas por separado y las
 * demas juntalas en 1 sola"): the port's own extras (auto, buscador, cartas,
 * fundas, perfil, tapetes) live in Mods-Extras\Contenido; a folder unpacked
 * the old way (each one straight in Mods-Extras) still works.
 * "\Mods-Extras\Contenido" when that folder is there, else "\Mods-Extras",
 * to put after the exe's folder. */
#ifndef YGO_MODS_CARPETA_H
#define YGO_MODS_CARPETA_H
#include <wchar.h>
const char *ygo_mods_a(void);
const wchar_t *ygo_mods_w(void);
#endif
