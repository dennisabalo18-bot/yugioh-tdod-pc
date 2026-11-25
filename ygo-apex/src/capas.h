/*
 * capas.h - the panels (layered windows) into the game's picture when the
 * recording mode is on (capas.c). Included after <windows.h> by every file
 * that draws a panel with UpdateLayeredWindow: the call goes through
 * ygo_capa_ulw, which sends the picture to the kit and leaves the window
 * itself clear, so OBS (game or window capture) records the panels too.
 */
#ifndef YGO_CAPAS_H
#define YGO_CAPAS_H
#include <windows.h>

BOOL ygo_capa_ulw(HWND hwnd, HDC dst, POINT *pos, SIZE *size, HDC src, POINT *src_pos,
                  COLORREF key, BLENDFUNCTION *blend, DWORD flags);
void ygo_capas_set(int on);
int  ygo_capas_on(void);

#define UpdateLayeredWindow ygo_capa_ulw
#endif
