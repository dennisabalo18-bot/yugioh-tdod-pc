/* El paso 1 del combate (sub_0009A540, el que sigue a la declaración del
 * ataque; máquina de estados 0x65F8DE, tabla 0x6010F8, sub_0009E5C0).
 *
 * 2026-11-15: acá estuvo el rayo dorado del ataque (Master Duel), que retenía
 * este paso mientras se dibujaba. El usuario lo probó (18:2x): "quedó
 * horrible, sucede antes y a veces después, recontra a destiempo". Sacado: el
 * paso vuelve a ser el del juego. Queda el envoltorio (está en PADS de
 * tools/rename_pads.py) por si se vuelve a necesitar el momento del ataque.
 */
#include "recomp/gen/recomp_types.h"

void sub_0009A540_pad(void);
void ygo_cartas_paso_ataque(void);
void ygo_cartas_paso_ataque_fin(void);

void sub_0009A540(void)
{
    ygo_cartas_paso_ataque();      /* Chinche Hedionda Naturia (cartas.c) */
    sub_0009A540_pad();
    ygo_cartas_paso_ataque_fin();
}
