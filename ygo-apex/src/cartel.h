/* cartel.c: small signs over the game, each in a layered window of its own
 * (as premios.c's: the game's picture is not touched). */
#ifndef YGO_CARTEL_H
#define YGO_CARTEL_H

#define CARTEL_LINEAS 4
#define CARTEL_ITEMS 32

typedef struct {
    char titulo[160];                    /* first line, gold (UTF-8) */
    char lineas[CARTEL_LINEAS][200];     /* white lines under it */
    int nlin;
    int nitems;                          /* names: a list wrapped to the width, */
    char item[CARTEL_ITEMS][48];         /* or tiles with a portrait when fotos */
    int foto[CARTEL_ITEMS];              /* character 0..27 (cf_anim/small/png/cfsNN.png), -1 none */
    int fotos;
    int lado;                            /* 0 right, 1 left, 2 centre */
    int y_mil;                           /* top, in thousandths of the game's height */
} ygo_cartel;

#define CARTEL_BIBLIOTECA 0
#define CARTEL_NUEVA 1
#define CARTEL_N 2

/* shown for ms from now (a screen calling each frame keeps it up) */
void ygo_cartel_poner(int slot, const ygo_cartel *c, unsigned ms);
void ygo_cartel_start(void);

/* small "new" badges centred on game points (640 x 480 frame px), shown for
 * ms from now (a screen calling each frame keeps them up); ancho: the most
 * width of the word, in frame px */
#define CARTEL_MARCAS 64
void ygo_cartel_marcas(const float *gx, const float *gy, int n, const char *texto, float ancho, unsigned ms);
void ygo_cartel_marcas_quitar(void);

#endif
