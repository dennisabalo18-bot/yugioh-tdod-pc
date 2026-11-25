/* La letra del juego para nuestros carteles (fuente.c). Reemplaza a
 * GetTextExtentPoint32W / DrawTextW de GDI: escribe la cobertura del texto en
 * los canales pedidos de un bitmap BGRA de 32 bits de arriba hacia abajo, como
 * hacía DrawTextW con SetTextColor de rojo / verde / blanco puros. */
#ifndef YGO_FUENTE_H
#define YGO_FUENTE_H
#include <windows.h>

#define FJ_B 1u
#define FJ_G 2u
#define FJ_R 4u
#define FJ_BLANCO 7u

#define FJ_CENTRO 1u
#define FJ_VCENTRO 2u
#define FJ_ELIPSIS 4u

/* h: el tamaño como el de CreateFont(-h): las mayúsculas miden 0,7 h */
int fj_linea(int h);
int fj_ancho(const wchar_t *s, int h);
void fj_medir(const wchar_t *s, int h, SIZE *sz);
void fj_dibujar(unsigned char *bits, int W, int H, const RECT *rc, const wchar_t *s, int h,
                unsigned canales, unsigned flags);

/* fuente_titulo.c: the game's title letters (the Perfil), same h */
int ft_linea(int h);
int ft_ancho(const wchar_t *s, int h);
void ft_dibujar(unsigned char *bits, int W, int H, const RECT *rc, const wchar_t *s, int h, unsigned canales);

#endif
