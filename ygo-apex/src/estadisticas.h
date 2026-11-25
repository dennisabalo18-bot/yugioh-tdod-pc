/* estadisticas.c: your duels (YGO_AJUSTES\duelos.txt), for the Perfil */
#ifndef YGO_ESTADISTICAS_H
#define YGO_ESTADISTICAS_H

#define YGO_ULTIMOS 5

typedef struct { int ch, dia, mes; char res; } ygo_duelo_vis;   /* res 'V', 'D', 'E' */

typedef struct {
    int duelos, ganados, perdidos, racha, mejor_racha, exodia;
    int dificil_ch, dificil_g, dificil_p;                         /* -1: none yet */
    int n_ult;
    ygo_duelo_vis ult[YGO_ULTIMOS];                               /* newest first */
} ygo_stats;

void ygo_estadisticas(ygo_stats *s);
unsigned ygo_estadisticas_version(void);
void ygo_estadisticas_fin(int exodia);             /* mascota.c: the duel scene left 6 */
void ygo_estadisticas_mirar(void);                 /* mascota.c: each frame of state 6 */
void ygo_estadisticas_revisar(void);               /* often: the game saved -> the pending duels kept */
void ygo_estadisticas_nombre(int ch, char *out, int cap);
int ygo_estadisticas_retrato(int ch);              /* cfsNN */
void ygo_estadisticas_rival(int ch, int *g, int *p); /* won / lost against one opponent */
void ygo_texto_juego(unsigned n, char *out, int cap); /* the game's text n, UTF-8 */

#endif
