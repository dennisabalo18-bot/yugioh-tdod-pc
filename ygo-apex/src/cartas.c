/*
 * ygo-port: el mod de cartas (opcional): cartas del juego reemplazadas por
 * otras, con su nombre, descripcion, imagen y, si hace falta, un efecto
 * programado aca.
 *
 * YGO_CARTAS = la carpeta del mod (Mods-Extras\cartas, armada por
 * tools/cartas_mod.py desde modding/cartas/cartas.txt):
 * - cartas.txt: "[ID]" (el numero de la imagen de la carta) y debajo
 *   "nombre = ...", "descripcion = ...", "efecto = ..." (UTF-8);
 * - archivos\: las imagenes (la carta grande en los 5 idiomas y la hoja de
 *   miniaturas): main.c suma la carpeta a RECOMP_MODS_DIR, y el juego las lee
 *   de ahi en vez del disco;
 * - cartas.pack: las mismas en HD (RECOMP_TEXTURES_PACK_EXTRA, kit).
 * F2 "Cartas nuevas" (Si / No, se aplica al reiniciar): con No, el juego queda
 * como el original. La partida guarda las cartas por numero: con el mod
 * apagado, la carta vuelve a ser la original.
 *
 * Los datos del juego (default.xbe, en memoria; se cambian antes de que
 * arranque, ygo_cartas_init):
 * - k = 0..1138 el indice de la carta; u16 0x5FE692[k] su imagen (ID);
 * - textos (6 tablas, voces.c): nombre 849 + k, descripcion 1988 + k, en
 *   las 5 de Europa (1..5); el texto nuevo va en memoria del juego
 *   (xbox_HeapAlloc) y el desplazamiento de la tabla apunta a el;
 * - efectos: 742 entradas de 24 bytes en 0x601318, ordenadas por ID:
 *   {u32 ID, resolver, ?, condicion, ?, ?}. Se buscan por ID; una carta sin
 *   entrada no tiene efecto.
 *
 * Efectos programados:
 * - prision_dimensional: "Cuando un monstruo del adversario declara un
 *   ataque: destierra ese monstruo". La entrada de la carta toma la
 *   condicion y el resolver de Cilindro magico (5124): sub_000A6810 (el
 *   ataque declarado por el adversario) y sub_000AF600 (switch por ID:
 *   Fuerza de espejo, Cilindro, Jabalina, Mascara de la debilidad). Los dos
 *   van envueltos:
 *   * la condicion, con la carta tomada por Cilindro durante la llamada
 *     (0x5FE692[k] = 5124 y de vuelta);
 *   * el resolver, propio: el contexto (arg) trae en +6 el jugador y en +7
 *     la casilla del atacante (asi los usa Cilindro); si sigue ahi boca
 *     arriba (+0x65D0D2 bit 1), sub_000A1F20(k, p, z) (lo pregunta
 *     Agujero sin Fondo antes de actuar) y sub_000C69A0(p, z, 0): destruir
 *     y retirar de la partida (Agujero sin Fondo, sub_000B83E0).
 * - krebons: "Cuando esta carta es seleccionada para un ataque: paga 800 LP;
 *   niega el ataque". No pasa por la tabla de efectos (es un monstruo; el
 *   juego no ofrece activar monstruos al ser atacados): sub_0009A290 es la
 *   declaracion del ataque (estado del combate 6 -> 1, 0x65F8DE; arg el
 *   jugador que ataca). 0x6666E0: bit 0 ataque directo, bits 10-12 la
 *   casilla del atacante, 13-15 la del atacado (del otro jugador). Si el
 *   atacado es la carta boca arriba y su duenio tiene mas de 800 LP:
 *   sub_000C8BC0(duenio, 800) (perder LP, como Cilindro magico) y la orden
 *   0x23 (sub_00081150: negar el ataque, como Cilindro: estado 0xB). Siempre
 *   se activa (el "puedes" no tiene pregunta).
 * - sincronizador (Sincronizador de Basura): "Cuando esta carta es Llamada
 *   de Modo Normal: Llamada Especial de 1 monstruo de Nivel 2 o menor de tu
 *   Cementerio, en Defensa" (puedes: pregunta Si/No; eliges cual). Toma el camino de Senju de las mil manos (4916,
 *   "cuando es llamada al campo, excluyendo Llamada Especial"):
 *   * las maquinas de la llamada (sub_00095A30 elige por el tipo, 0x66BA8E
 *     bits 2-4; estado en bits 5-11; carta k en 0x66BA80, descripcion en
 *     0x66BA7C: bit 0 jugador, 1-5 casilla, 14 boca arriba, 15 defensa): la
 *     Llamada Normal es sub_00093FA0 (tipo 1, estado 2) y la otra
 *     sub_00094610 (tipo 2, estado 4). En ese estado comparan la imagen de
 *     la carta con las que se disparan al ser llamadas y registran el
 *     disparo (sub_00081EF0). Ahi la carta pasa por Senju (0x5FE692[k]).
 *   * la busqueda del efecto, sub_000C4DE0(k) (busca por la imagen de k): la
 *     carta pasa por Senju y recibe su entrada (resolver sub_000B1220).
 *   * el resolver de Senju, envuelto (ctx +0 = k, +2 bit 0 = jugador; estado
 *     en 0x66BFBC, devuelve el siguiente, 0 = termino): los mismos pasos que
 *     Senju (pregunta Si/No sub_000880B0, respuesta en 0x660B8C; mensaje
 *     sub_00087E20; lista sub_000C5AE0; lo elegido en 0x65855C[0x658556 +
 *     (0x658555 & 3)]), con la lista de Llamada del encantado (4989: los
 *     monstruos del propio Cementerio) filtrada a Nivel 1-2 en el envoltorio
 *     de sub_000C1B80 (entradas de 8 bytes en 0x65855C, cantidad u16
 *     0x6589DC). Luego lo de Llamada del encantado (sub_000B0970): orden 0xA0
 *     y sub_000961F0(p, &entrada, boca arriba 1, defensa 1, 0x20). Necesita
 *     casilla libre (sub_000D39D0(p)). Los textos de la pregunta y del
 *     mensaje son propios (en espanol).
 *     Los efectos del llamado NO se niegan: ninguna carta del juego niega los
 *     efectos de un monstruo (solo Jinzo, trampas; Orden imperial, magicas),
 *     asi que esa parte se saco del texto de la carta.
 * - escudo_drenador (Escudo drenador): "Si un monstruo del adversario declara
 *   un ataque: niega ese ataque y ganas LP iguales a su ATK". Misma entrada
 *   que prision_dimensional (condicion de Cilindro magico); el resolver hace
 *   lo de Cilindro (orden 0x23, ATK con sub_000D8130(p, z)) pero en vez de
 *   quitarle LP al adversario, el duenio gana: sub_000C8DF0(jugador, n), el
 *   mismo camino que Medicina roja.
 * - carga_poderoso (Carga del Poderoso): Magica Continua (clase 2, resolver
 *   vacio de Estandarte del Valor); el ATK lo baja el envoltorio de
 *   sub_000D5DD0 (al final del archivo).
 * - drillago: ataca directo si el adversario solo controla monstruos boca
 *   arriba con 1600 ATK o mas (envoltorio de sub_00099980, al final).
 * - soldado_brillo (Soldado del Brillo Negro - Enviado del Principio): se
 *   invoca como Alma de pureza y luz desterrando 1 LUZ y 1 OSCURIDAD; efecto 1
 *   el de Barrel Dragon (desterrar 1 monstruo, no ataca ese turno); efecto 2:
 *   si destruye un monstruo atacando, puede atacar otra vez (al final).
 * - goblin_entrenador: en la End Phase, si esta boca arriba: puedes devolver
 *   1 monstruo normal de tu mano a la parte inferior del Deck y robar 1 (una
 *   vez por turno). Envoltorios de sub_0009EC80 y sub_000B6DA0 (al final).
 * - abare_ushioni (Abare Ushioni): "Lanza una moneda y elige cara o cruz. Si
 *   aciertas, 1000 de dano al adversario; si fallas, 1000 a ti. Una vez por
 *   turno, en tu Main Phase." Pasa por Mago de tiempo (4022, alias): su
 *   condicion sub_000A5010 y su resolver sub_000B2D10, envueltos (al final).
 *   La condicion de Mago pide la bandera 0x2000 y monstruos del adversario;
 *   la de Abare, que este boca arriba y no haya usado el efecto este turno.
 *   El resolver de Mago: estado 0x80 pide la moneda (sub_00084D00(p, 0x13),
 *   el pedido comun: Comediante demoniaco, Gamble), 0x7F el resultado (u16
 *   0x65F930 != 0: acerto) y devuelve 0xA (fin, orden 0x6F). Abare hace en
 *   0x7F sub_000C8BC0(adversario o el, 1000) en vez de destruir monstruos.
 * - acantilado_naturia (Acantilado Naturia): "Cuando esta carta es mandada al
 *   Cementerio desde el Campo, puedes Invocar de Modo Especial, desde tu Deck,
 *   1 monstruo Naturia de Nivel 4 o menor en Posicion de Ataque boca arriba."
 *   El disparo es el de Sangan (4054): la carta pasa por Sangan mientras
 *   corren sub_000C7C50 (al Cementerio por batalla) y sub_000C5DA0 (por otra
 *   causa: efecto, sacrificio). El efecto es el de Rata gigante (4915, alias):
 *   resolver sub_000B0FF0 (pregunta, lista del Deck con sub_000C1B80, elegir,
 *   Invocacion Especial boca arriba en ataque), envuelto: mientras corre, la
 *   lista es la propia (Deck 0x65D9FC + p*0x13F8, cuenta u8 0x65D0A9, origen
 *   2): los monstruos de Nivel 1-4 cuyo nombre (cartas.txt) dice "Naturia".
 *   Toda carta del mod con "Naturia" en el nombre entra sola.
 * - berenjena_naturia (Berenjena Naturia): "Cuando esta carta es mandada del
 *   Campo al Cementerio, selecciona 1 monstruo Naturia de tu Cementerio,
 *   excepto Berenjena Naturia, y anadelo a tu mano." Disparo de Sangan (como
 *   el Acantilado). Efecto: alias de Mago de la fe (4434): eleccion del
 *   objetivo sub_000BABA0 (lista del Cementerio con sub_000C1B80, lo elegido
 *   en ctx +0xC/+0xE, +0xA & 7 = 2) y resolver sub_000A9570 (sigue en el
 *   Cementerio? sub_000D5050; orden 0x9F: del Cementerio a la mano), los dos
 *   envueltos: la lista es la propia (Cementerio 0x65DC7C + p*0x13F8, cuenta
 *   u8 0x65D0AA, origen 4): monstruos "Naturia" que no sean esta carta. Sin
 *   pregunta Si/No (Mago de la fe no la tiene): el texto no dice "puedes".
 * - naturia_vanidosa (Naturia Vanidosa): "Puedes Sacrificar esta carta y 1
 *   monstruo Naturia para negar la activacion de una Carta Magica o de Trampa
 *   de tu adversario, y destruirla." Es Maryokutai (5260, alias): el monstruo
 *   boca arriba que responde a una Magica del adversario sacrificandose.
 *   - sub_00082140(activacion, p, zona, z): 0x41 si la carta de (p, z) puede
 *     responder; en las casillas de monstruo (zona 0) solo Maryokutai (imagen
 *     0x148C, boca arriba, Magica 0x16). Envuelta: Vanidosa boca arriba
 *     responde a Magicas (0x16) y Trampas (0x15) si hay otro monstruo Naturia
 *     boca arriba en su Campo.
 *   - la condicion de Maryokutai sub_000A8500 (ctx, activacion, 0): lo mismo.
 *   - su resolver sub_000B71A0 (ctx, activacion): Maryokutai se sacrifica
 *     (sub_000C7980(p, z)) y encola la orden 0x88 (negar y destruir; la misma
 *     de Magic Jammer y Siete herramientas del bandido para Trampas). Vanidosa
 *     sacrifica tambien al otro Naturia (el de menos ATK, elegido solo).
 * - chinche_naturia (Chinche Hedionda Naturia): "Cuando un monstruo Naturia
 *   boca arriba que controlas es seleccionado como objetivo de un ataque,
 *   manda al Cementerio esta carta boca arriba para negar ese ataque y
 *   terminar la Battle Phase." Como Krebons (siempre, sin pregunta): en la
 *   declaracion del ataque (sub_0009A290, ya envuelta) si el atacado es un
 *   Naturia boca arriba y su duenio tiene la Chinche boca arriba: la Chinche
 *   al Cementerio con sub_000C7980 (el sacrificio de Maryokutai) y la orden
 *   0x1F (la de Anule el ataque: niega los ataques y termina la Battle Phase).
 * - cosmo_naturia (Cosmoremolacha Naturia): "Cuando tu adversario Invoca de
 *   Modo Normal o Coloca un monstruo, esta carta es Invocada de Modo Especial
 *   desde tu mano." Sin pregunta. El disparo: en la Llamada Normal del
 *   adversario (sub_00093FA0, estado 2, donde el juego registra los disparos
 *   al ser llamada) se registra como el del Goblin Entrenador (Garuda,
 *   sub_00081EF0) si la carta esta en la mano de su duenio y hay casilla libre.
 *   El resolver (el de Garuda, sub_000B6DA0, alias) la saca de la mano como la
 *   Llamada de la Momia (5435, sub_000B8AB0): orden 0x8E (k, palabra +2 de la
 *   entrada de la mano) y sub_000963C0(p, &entrada, 1, 0).
 * - "copias = n" (0-3): el limite de copias en el mazo (envoltorio de
 *   sub_0007EF00).
 * - "datos = 0x..." cambia el u32 de la carta en 0x5FEF78 (DEF/10 bits 0-8,
 *   ATK/10 9-17, clase 18-19, tipo 20-24, nivel 25-28, atributo 29-31).
 *
 * recomp's definitions are renamed _pad (tools/rename_pads.py).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "recomp/gen/recomp_types.h"

uint32_t xbox_HeapAlloc(uint32_t size, uint32_t alignment);

void sub_000A6810_pad(void);
void sub_0007EF00_pad(void);
void sub_000AF600_pad(void);
void sub_000A1F20(void);
void sub_000C69A0(void);
void sub_000D4B70(void);
void sub_00088970(void);
void sub_0009A290_pad(void);
void sub_000C8BC0(void);
void sub_00081150(void);
void sub_00093FA0_pad(void);
void sub_00094610_pad(void);
void sub_000C4DE0_pad(void);
void sub_000B1220_pad(void);
void sub_000D39D0(void);
void sub_000961F0(void);
void sub_000C1B80_pad(void);
void sub_000880B0(void);
void sub_00087E20(void);
void sub_000C5AE0(void);
void sub_000D8130(void);
void sub_000A6C90_pad(void);
void sub_000AB150_pad(void);
void sub_000A35D0_pad(void);
void sub_000A3FB0_pad(void);
void sub_000BD420_pad(void);
void sub_000BAA20(void);
void sub_00086F50(void);
void sub_00092E20(void);
void sub_0007E280(void);
void sub_000C8470(void);
void sub_000C8DF0(void);

#define NCARTAS    1139u
#define T_IMAGEN   0x005FE692u
#define T_DATOS    0x005FEF78u
#define T_EFECTOS  0x00601318u
#define N_EFECTOS  742u
#define TXT_TABLAS 0x005FC2DCu
#define TXT_BASES  0x005FC2C4u
#define TXT_NOMBRE 849u
#define TXT_DESC   1988u
#define ID_CILINDRO 5124u
#define FX_RESOLVER 0x000AF600u
#define FX_ATAQUE   0x000A6810u

#define ID_BUSCADOR 4506u               /* Buscador de dragon: disparo al ser Invocado (no Especial), elige 1 carta */
static int s_k_fort = -1;               /* k de la carta con fortaleza */
static uint32_t s_fort_usado[2][5];     /* ataque (clave) en que ya se salvo este turno; 0 = libre */
static int s_k_caius = -1;              /* k de la carta con caius */
static int s_k_isaac = -1;              /* k de la carta con isaac */
static struct { int activo, hecho; uint32_t p, z, k; } s_isaac;  /* el destruido en batalla por Isaac */
static int s_k_real = -1;               /* k de la carta con caballero_real */
static struct { int activo; uint32_t k, yo; } s_real[2];  /* [dueno del destruido]: lo destruyo Caballero Real */
static uint32_t s_txt_caius;            /* "Selecciona 1 carta del Campo..." */

static volatile LONG s_off;             /* F2: 0 Si, 1 No */
static int s_k_prision = -1;            /* k de la carta con prision_dimensional */
static int s_k_krebons = -1;            /* k de la carta con krebons */
static int s_k_escudo = -1;             /* k de la carta con escudo_drenador */
static int s_k_ciclon = -1;             /* k de la carta con ciclon_cosmos */
#define ID_TIFON 4909u                  /* Tifon de espacio esoterico: su entrada (elegir la carta) */
#define FX_COSTO_1000 0x000A3F90u       /* costo de Mundo de Toon: pagar 1000 LP al activar */
#define FX_COSTO_DESCARTE 0x000A3FB0u   /* costo de descartar (Magic Jammer: 1 carta) */
#define ID_JAMMER 4862u                 /* Magic Jammer */
static int s_k_fenix = -1;              /* k de la carta con golpe_fenix */
static uint32_t s_txt_fenix;            /* "Selecciona 1 carta que controle tu adversario..." */
static int s_k_sinc = -1;               /* k de la carta con sincronizador */
#define ID_SENJU 4916u                  /* Senju de las mil manos: su disparo y su resolver */
#define ID_LLAMADA 4989u                /* Llamada del encantado: su lista (monstruos del Cementerio) */
static uint32_t s_txt_pregunta, s_txt_elegir;   /* textos del Sincronizador (memoria del juego) */
static int s_k_cer = -1;                /* k de la carta con cerezas */
static int s_k_momonga = -1;            /* k de Momonga agil (se esconde mientras resuelven las cerezas) */
#define ID_MOMONGA 4922u                /* Momonga agil: su disparo (al Cementerio por batalla) y su resolver */

/* Cartas "como otra": mientras corren las funciones del duelo que miran la
 * imagen de la carta (0x5FE692[k]) para decidir su efecto, la carta pasa por
 * una del juego (fx) y, en el repaso del comienzo del turno (sub_000CFE70, que
 * marca a las que se pueden poner boca abajo una vez por turno), por otra
 * (turno; 0 = la suya). Fuera de esas funciones (dibujo, mazo, partida) es
 * ella misma. */
#define FX_CONTINUA 0x000C4DC0u         /* resolver vacio de las Magicas Continuas (Estandarte del Valor) */
static uint32_t s_id_carga;             /* imagen de la carta con carga_poderoso, o 0 */
static int s_k_drillago = -1;           /* k de la carta con drillago */
static int s_k_goblin = -1;             /* k de la carta con goblin_entrenador */
#define ID_GARUDA 5255u                 /* Garuda, el Espiritu del Viento: su disparo de la End Phase y su entrada */
static int s_k_bls = -1;                /* k de la carta con soldado_brillo */
#define ID_ALMA 0x1483u                 /* Alma de pureza y luz: Invocacion Especial desterrando 2 LUZ del Cementerio */
#define ID_BARREL 4749u                 /* Barrel Dragon: efecto 1 vez por turno que elige un monstruo */
#define ID_SANGAN 4054u                 /* Sangan: disparo al ir del Campo al Cementerio (Acantilado) */
#define ID_RATA 4915u                   /* Rata gigante: Invocar del Deck en ataque (Acantilado) */
static int s_k_acant = -1;              /* k de la carta con acantilado_naturia */
static int s_acant_lista;               /* 1 mientras resuelve el Acantilado (su lista del Deck) */
static uint8_t s_naturia[NCARTAS];
static uint8_t s_agregada[NCARTAS];     /* 1: carta agregada por el mod (no los textos corregidos) */
static int s_nagregadas;      /* 1: carta del mod con "Naturia" en el nombre */
#define ID_FE 4434u                     /* Mago de la fe: del Cementerio a la mano (Berenjena) */
static int s_k_beren = -1;              /* k de la carta con berenjena_naturia */
static int s_beren_lista;               /* 1 mientras la Berenjena elige (su lista del Cementerio) */
#define ID_MARYO 5260u                  /* Maryokutai: monstruo que se sacrifica para negar una Magica */
static int s_k_vani = -1;               /* k de la carta con naturia_vanidosa */
static int s_k_chinche = -1;            /* k de la carta con chinche_naturia */
static int s_k_cosmo = -1;              /* k de la carta con cosmo_naturia */
static void cosmo_llamada(void);
static void cosmo_maquina(uint32_t est, int listo, const char *que);
static int cosmo_resolver(uint32_t p);
static int cosmo_eslabon(void);
static int chinche_ataque(uint32_t p, uint32_t at);
/* las cartas que se disparan como Sangan al ir del Campo al Cementerio */
static int como_sangan(uint32_t c) { return c && (c == (uint32_t)s_k_acant || c == (uint32_t)s_k_beren); }
#define ID_MAGO 4022u                   /* Mago de tiempo: moneda 1 vez por turno (Abare Ushioni) */
static int s_k_abare = -1;              /* k de la carta con abare_ushioni */
static uint8_t s_abare_usado[2][5];     /* [jugador][casilla]: ya lanzo la moneda este turno */
static int s_bls_lista;                 /* 1 mientras se invoca el Soldado (la lista del Cementerio) */
static void bls_ataque(uint32_t p, uint32_t at);
static void isaac_cementerio(void);
static int s_isaac_redibujar;
static void bls_segundo(void);
static int s_bls_usado;                 /* ya uso uno de sus efectos este turno */
static int s_bls_ef1;                   /* uso el efecto 1 (desterrar) este turno: no ataca */
static int s_bls_ataco;                 /* el Soldado ya ataco este turno (bls_ataque): sin efecto 1 */
static void bls_lista(uint32_t p);
#define NALIAS 32
#define ID_LACOODA 5427u                /* Des Lacooda: robar al ser volteada; boca abajo 1 vez por turno */
#define ID_COMEHOMBRES 4507u            /* Come Hombres: VOLTEO, destruye 1 monstruo */
#define ID_GUARDATUMBA 5511u            /* Guardia del Guardatumba: VOLTEO, 1 monstruo del adversario a la mano */
#define ID_SKELENGEL 4546u              /* Skelengel: VOLTEO, puedes robar 1 (resolver sub_000ADDE0) */
static int s_k_vidente = -1;            /* k de la carta con vidente */
static uint32_t s_txt_vidente;          /* "Selecciona 1 carta de tu mano..." */
static int s_vid_sel;                   /* 1 mientras Vidente elige de la mano */
static struct { uint32_t k; uint16_t orig, fx, turno; } s_al[NALIAS];
static int s_nal;

static void al_set(int turno, uint16_t *prev)
{
    int i;
    for (i = 0; i < s_nal; ++i) {
        uint16_t id = turno ? s_al[i].turno : s_al[i].fx;
        prev[i] = (uint16_t)MEM16(T_IMAGEN + 2u * s_al[i].k);
        MEM16(T_IMAGEN + 2u * s_al[i].k) = id ? id : s_al[i].orig;
    }
}

static void al_restore(const uint16_t *prev)
{
    int i;
    for (i = 0; i < s_nal; ++i)
        MEM16(T_IMAGEN + 2u * s_al[i].k) = prev[i];
}

#define CON_ALIAS(turno, f) do {                         \
        uint16_t prev_[NALIAS];                          \
        if (!s_nal) { f(); break; }                      \
        al_set(turno, prev_);                            \
        f();                                             \
        al_restore(prev_);                               \
    } while (0)

static void alias(int k, uint32_t id, uint32_t fx, uint32_t turno, const char *que)
{
    if (s_nal >= NALIAS) {
        fprintf(stderr, "[CARTAS] %u: %s SIN alias (tabla llena, NALIAS %d)\n", id, que, NALIAS);
        return;
    }
    s_al[s_nal].k = (uint32_t)k;
    s_al[s_nal].orig = (uint16_t)id;
    s_al[s_nal].fx = (uint16_t)fx;
    s_al[s_nal].turno = (uint16_t)turno;
    ++s_nal;
    fprintf(stderr, "[CARTAS] %u: %s (como %u%s)\n", id, que, fx, turno ? ", boca abajo 1 vez por turno" : "");
}

int ygo_cartas_get(void) { return (int)s_off; }

/* buscador.c: "Agregadas" (2026-11-19), solo si el mod esta puesto */
int ygo_carta_agregada(int k) { return k > 0 && (uint32_t)k < NCARTAS && s_agregada[k]; }
int ygo_cartas_hay_agregadas(void) { return s_nagregadas > 0; }
void ygo_cartas_set(int i) { InterlockedExchange(&s_off, i ? 1 : 0); }

/* el texto en la memoria del juego (Latin-1 / 1252), o 0 */
static uint32_t texto_guest(const char *utf8)
{
    wchar_t w[1024];
    char a[2048];
    int n;
    uint32_t g;
    if (!MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, 1024))
        return 0;
    {
        /* 2026-11-20 (the user's capture: Soldado del Brillo Negro's two
         * effects showed "?" where the card has a big dot): the game's own
         * multi-effect cards mark each effect with 0xB7 "·" (Spanish and
         * English text 3052); "●" / "•" / "・" are not in its font */
        int i;
        for (i = 0; i < 1024 && w[i]; i++)
            if (w[i] == 0x25CF || w[i] == 0x2022 || w[i] == 0x30FB || w[i] == 0x25E6 || w[i] == 0x2219)
                w[i] = 0x00B7;
    }
    n = WideCharToMultiByte(1252, 0, w, -1, a, sizeof a, "?", NULL);
    if (n <= 0 || !(g = xbox_HeapAlloc((uint32_t)n, 4)))
        return 0;
    memcpy((void *)XBOX_PTR(g), a, (size_t)n);
    return g;
}

/* el texto idx en una tabla del juego (1 ingles, 2 aleman, 3 frances,
 * 4 italiano, 5 espanol) */
static void poner_texto_en(uint32_t idx, uint32_t g, uint32_t lang)
{
    uint32_t tab = MEM32(TXT_TABLAS + 4u * lang), base = MEM32(TXT_BASES + 4u * lang);
    if (tab && idx < MEM32(tab))
        MEM32(tab + 4u + 4u * idx) = g - base;
}

static void poner_texto(uint32_t idx, uint32_t g)
{
    uint32_t lang;
    for (lang = 1; lang <= 5; ++lang)
        poner_texto_en(idx, g, lang);
}

/* nombre_xx / descripcion_xx de la carta que se esta leyendo, por tabla
 * (1 en, 2 de, 3 fr, 4 it): si estan, reemplazan al espanol en ese idioma */
static char s_nom_l[5][512], s_desc_l[5][1536];
/* 2026-11-18 (el usuario: cartas "en espanol de Espana"): "solo_espanol = si":
 * una correccion del texto del juego en espanol; los otros idiomas, el suyo */
static int s_solo_es;
static const char *const k_suf_l[5] = { "", "en", "de", "fr", "it" };

static int k_de(uint32_t id)
{
    uint32_t k;
    for (k = 1; k < NCARTAS; ++k)
        if (MEM16(T_IMAGEN + 2u * k) == id)
            return (int)k;
    return -1;
}

static uint32_t efecto_de(uint32_t id)
{
    uint32_t i;
    for (i = 0; i < N_EFECTOS; ++i)
        if (MEM32(T_EFECTOS + 24u * i) == id)
            return T_EFECTOS + 24u * i;
    return 0;
}

static void trim(char *s)
{
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n'))
        *--e = 0;
    if (s != s + strspn(s, " \t"))
        memmove(s, s + strspn(s, " \t"), strlen(s + strspn(s, " \t")) + 1);
}

/* Una carta del archivo: sus datos sobre los del juego. */
/* "copias = n": el limite del mazo (sub_0007EF00) para esa carta */
#define NCOPIAS 64
static uint16_t s_copias_id[NCOPIAS], s_copias_n[NCOPIAS], s_copias_k[NCOPIAS];
static int s_ncopias;

static void aplicar(uint32_t id, const char *nombre, const char *desc, const char *efecto,
                    const char *datos, const char *copias)
{
    int k = k_de(id);
    uint32_t g;
    if (k < 0) {
        fprintf(stderr, "[CARTAS] %u: no es una carta del juego\n", id);
        return;
    }
    if (*datos)
        MEM32(T_DATOS + 4u * (uint32_t)k) = (uint32_t)strtoul(datos, NULL, 0);
    if (!s_solo_es && (uint32_t)k < NCARTAS && !s_agregada[k]) {
        s_agregada[k] = 1;
        ++s_nagregadas;
    }
    if (*copias && s_ncopias < NCOPIAS) {
        s_copias_id[s_ncopias] = (uint16_t)id;
        s_copias_k[s_ncopias] = (uint16_t)k;
        s_copias_n[s_ncopias] = (uint16_t)(strtoul(copias, NULL, 10) & 3u);
        fprintf(stderr, "[CARTAS] %u: hasta %u copias en el mazo\n", id, s_copias_n[s_ncopias]);
        ++s_ncopias;
    }
    if (*nombre && (g = texto_guest(nombre))) {
        if (s_solo_es)
            poner_texto_en(TXT_NOMBRE + (uint32_t)k, g, 5);
        else
            poner_texto(TXT_NOMBRE + (uint32_t)k, g);
    }
    {
        char low[256];
        size_t i;
        for (i = 0; nombre[i] && i < sizeof low - 1; ++i)
            low[i] = (char)((nombre[i] >= 'A' && nombre[i] <= 'Z') ? nombre[i] + 32 : nombre[i]);
        low[i] = 0;
        if (strstr(low, "naturia")) {
            s_naturia[k] = 1;
            fprintf(stderr, "[CARTAS] %u: es Naturia (Acantilado puede invocarla)\n", id);
        }
    }
    if (*desc && (g = texto_guest(desc))) {
        if (s_solo_es)
            poner_texto_en(TXT_DESC + (uint32_t)k, g, 5);
        else
            poner_texto(TXT_DESC + (uint32_t)k, g);
    }
    {
        uint32_t l;
        for (l = 1; l <= 4; ++l) {
            if (s_nom_l[l][0] && (g = texto_guest(s_nom_l[l])))
                poner_texto_en(TXT_NOMBRE + (uint32_t)k, g, l);
            if (s_desc_l[l][0] && (g = texto_guest(s_desc_l[l])))
                poner_texto_en(TXT_DESC + (uint32_t)k, g, l);
        }
    }
    if (!strcmp(efecto, "prision_dimensional")) {
        uint32_t e = efecto_de(id);
        if (!e) {
            fprintf(stderr, "[CARTAS] %u: la carta no tiene entrada de efecto\n", id);
            return;
        }
        MEM32(e + 4) = FX_RESOLVER;
        MEM32(e + 8) = 0;
        MEM32(e + 12) = FX_ATAQUE;
        MEM32(e + 16) = 0;
        MEM32(e + 20) = 0;
        s_k_prision = k;
    } else if (!strcmp(efecto, "escudo_drenador")) {
        uint32_t e = efecto_de(id);
        if (!e) {
            fprintf(stderr, "[CARTAS] %u: la carta no tiene entrada de efecto\n", id);
            return;
        }
        MEM32(e + 4) = FX_RESOLVER;
        MEM32(e + 8) = 0;
        MEM32(e + 12) = FX_ATAQUE;
        MEM32(e + 16) = 0;
        MEM32(e + 20) = 0;
        s_k_escudo = k;
    } else if (!strcmp(efecto, "ciclon_cosmos")) {
        uint32_t e = efecto_de(id), t = efecto_de(ID_TIFON);
        if (!e || !t) {
            fprintf(stderr, "[CARTAS] %u: la carta no tiene entrada de efecto\n", id);
            return;
        }
        memcpy((void *)XBOX_PTR(e + 4), (void *)XBOX_PTR(t + 4), 20);
        MEM32(e + 16) = FX_COSTO_1000;  /* el costo, al activarla (Mundo de Toon) */
        s_k_ciclon = k;
    } else if (!strcmp(efecto, "golpe_fenix")) {
        uint32_t e = efecto_de(id), t = efecto_de(ID_TIFON);
        if (!e || !t) {
            fprintf(stderr, "[CARTAS] %u: la carta no tiene entrada de efecto\n", id);
            return;
        }
        memcpy((void *)XBOX_PTR(e + 4), (void *)XBOX_PTR(t + 4), 20);
        MEM32(e + 16) = FX_COSTO_DESCARTE;  /* descartar 1 (Magic Jammer) */
        s_k_fenix = k;
        s_txt_fenix = texto_guest("Selecciona 1 carta que controle tu adversario para ponerla en la parte "
                                  "superior de su Deck.");
    } else if (!strcmp(efecto, "krebons"))
        s_k_krebons = k;
    else if (!strcmp(efecto, "des_lacooda"))
        alias(k, id, ID_LACOODA, ID_LACOODA, "Des Lacooda");
    else if (!strcmp(efecto, "hombre_nieve"))
        alias(k, id, ID_COMEHOMBRES, 0, "Hombre de Nieve Comilon");
    else if (!strcmp(efecto, "golem_centinela"))
        alias(k, id, ID_GUARDATUMBA, ID_LACOODA, "Golem Centinela");
    else if (!strcmp(efecto, "vidente")) {
        alias(k, id, ID_SKELENGEL, 0, "Vidente");
        s_k_vidente = k;
        s_txt_vidente = texto_guest("Selecciona 1 carta de tu mano para devolverla a tu Deck.");
    }
    else if (!strcmp(efecto, "cerezas")) {
        s_k_cer = k;
        s_k_momonga = k_de(ID_MOMONGA);
    }
    else if (!strcmp(efecto, "soldado_brillo")) {
        alias(k, id, ID_BARREL, ID_BARREL, "Soldado del Brillo Negro");
        s_k_bls = k;
    }
    else if (!strcmp(efecto, "cosmo_naturia")) {
        alias(k, id, ID_GARUDA, 0, "Cosmoremolacha Naturia");
        s_k_cosmo = k;
    }
    else if (!strcmp(efecto, "chinche_naturia"))
        s_k_chinche = k;
    else if (!strcmp(efecto, "naturia_vanidosa")) {
        alias(k, id, ID_MARYO, 0, "Naturia Vanidosa");
        s_k_vani = k;
    }
    else if (!strcmp(efecto, "berenjena_naturia")) {
        alias(k, id, ID_FE, 0, "Berenjena Naturia");
        s_k_beren = k;
    }
    else if (!strcmp(efecto, "acantilado_naturia")) {
        alias(k, id, ID_RATA, 0, "Acantilado Naturia");
        s_k_acant = k;
    }
    else if (!strcmp(efecto, "abare_ushioni")) {
        alias(k, id, ID_MAGO, ID_MAGO, "Abare Ushioni");
        s_k_abare = k;
    }
    else if (!strcmp(efecto, "goblin_entrenador")) {
        alias(k, id, ID_GARUDA, 0, "Goblin Entrenador");
        s_k_goblin = k;
    }
    else if (!strcmp(efecto, "drillago"))
        s_k_drillago = k;
    else if (!strcmp(efecto, "fortaleza"))
        s_k_fort = k;
    else if (!strcmp(efecto, "isaac"))
        s_k_isaac = k;
    else if (!strcmp(efecto, "caballero_real"))
        s_k_real = k;
    else if (!strcmp(efecto, "caius")) {
        s_k_caius = k;
        s_txt_caius = texto_guest("Selecciona 1 carta del Campo para desterrarla.");
    }
    else if (!strcmp(efecto, "carga_poderoso")) {
        uint32_t e = efecto_de(id);
        if (!e) {
            fprintf(stderr, "[CARTAS] %u: la carta no tiene entrada de efecto\n", id);
            return;
        }
        /* Magica Continua: como Estandarte del Valor (clase 2, resolver vacio) */
        MEM32(e + 4) = FX_CONTINUA;
        MEM32(e + 8) = MEM32(e + 12) = MEM32(e + 16) = MEM32(e + 20) = 0;
        MEM32(T_DATOS + 4u * (uint32_t)k) = (MEM32(T_DATOS + 4u * (uint32_t)k) & ~(3u << 18)) | (2u << 18);
        s_id_carga = id;
    }
    else if (!strcmp(efecto, "sincronizador")) {
        s_k_sinc = k;
        s_txt_pregunta = texto_guest("¿Deseas Invocar de Modo Especial un monstruo de Nivel 2 o menor "
                                     "de tu Cementerio en Posición de Defensa?");
        s_txt_elegir = texto_guest("Selecciona un monstruo de la lista para Invocarlo de Modo Especial.");
    }
    else if (*efecto)
        fprintf(stderr, "[CARTAS] %u: efecto desconocido \"%s\"\n", id, efecto);
    fprintf(stderr, "[CARTAS] %u (k %d): %s%s%s\n", id, k, nombre, *efecto ? ", efecto " : "", efecto);
}

/* ---- premios (2026-11-14, pedido del usuario: las agregadas mas dificiles) --
 * Leido en el codigo: el premio de una victoria es sub_0008CBB0(modo, d,
 * ranura) -> modo 0 sub_0008C7E0 (listas del rival d: 0x453830 y 0x453630,
 * 16 bytes por rival, u32 lista[variante] +0, u16 cantidad[variante] +8) o
 * modo != 0 sub_0008C9B0 (torneo: lista comun por nivel de torneo en 0x453F78
 * + 16t, t 0..4, y la del rival en 0x453FC8 + 16d). Entradas u32: u16 ID |
 * u16 (bits 0-12 peso, bit 15-r: puede salir en el premio r+1). Chance de
 * entrar al sorteo por pasada max(5, peso x factor[copias]) %, peso 0 = nunca.
 * Campos del mod: premio_peso (el peso de la carta en todas esas listas),
 * premio_ranuras ("3", "23"...: en que premios) y premio_rival (la suma a ese
 * rival en una entrada de su lista 0x453830 que tenia peso 0: no se pierde
 * nada que el juego diera). Se aplica una vez, al cargar el mod. */
#define NPREMIO 32
static struct { uint32_t id; int peso, bits, rival; } s_premio[NPREMIO];
static int s_npremio;

static void premio_campo(uint32_t id, const char *campo, const char *valor)
{
    int i;
    for (i = 0; i < s_npremio && s_premio[i].id != id; ++i)
        ;
    if (i == s_npremio) {
        if (!id || s_npremio >= NPREMIO)
            return;
        s_premio[s_npremio].id = id;
        s_premio[s_npremio].peso = s_premio[s_npremio].bits = s_premio[s_npremio].rival = -1;
        ++s_npremio;
    }
    if (!strcmp(campo, "peso"))
        s_premio[i].peso = (int)(strtoul(valor, NULL, 10) & 0x1FFFu);
    else if (!strcmp(campo, "ranuras")) {
        const char *c;
        s_premio[i].bits = 0;
        for (c = valor; *c; ++c)
            if (*c >= '1' && *c <= '3')
                s_premio[i].bits |= 1 << (15 - (*c - '1'));
    } else if (!strcmp(campo, "rival"))
        s_premio[i].rival = (int)strtoul(valor, NULL, 10);
}

static int premio_de(uint32_t cid)
{
    int i;
    for (i = 0; i < s_npremio; ++i)
        if (s_premio[i].id == cid)
            return i;
    return -1;
}

static int premio_bits(int i, uint32_t w)
{
    return s_premio[i].bits >= 0 ? s_premio[i].bits : (int)(w & 0xE000u);
}

/* una lista (u32 puntero, u16 cantidad) una sola vez aunque la compartan rivales */
static uint32_t s_listas_vistas[256];
static int s_nlistas;

static int premio_lista(uint32_t p, uint32_t n)
{
    uint32_t e;
    int i, cambios = 0;
    if (p < 0x00450000u || p >= 0x00460000u || !n || n > 400u)
        return 0;
    for (i = 0; i < s_nlistas; ++i)
        if (s_listas_vistas[i] == p)
            return 0;
    if (s_nlistas < 256)
        s_listas_vistas[s_nlistas++] = p;
    for (e = p; e < p + 4u * n; e += 4u) {
        int j = premio_de(MEM16(e));
        uint32_t w = MEM16(e + 2);
        if (j < 0)
            continue;
        MEM16(e + 2) = (uint16_t)(premio_bits(j, w) | (s_premio[j].peso >= 0 ? s_premio[j].peso : (int)(w & 0x1FFFu)));
        ++cambios;
    }
    return cambios;
}

static void premios_ajustar(void)
{
    uint32_t d, r, t, tab;
    int i, cambios = 0;
    if (!s_npremio)
        return;
    /* primero las cartas que se suman a un rival (despues reciben su peso como las demas) */
    for (i = 0; i < s_npremio; ++i) {
        uint32_t hecho[2] = { 0, 0 };
        if (s_premio[i].rival < 0 || s_premio[i].rival > 31)
            continue;
        for (r = 0; r < 2u; ++r) {
            uint32_t p = MEM32(0x00453830u + 16u * (uint32_t)s_premio[i].rival + 4u * r);
            uint32_t n = MEM16(0x00453838u + 16u * (uint32_t)s_premio[i].rival + 2u * r), e;
            if (p < 0x00450000u || p >= 0x00460000u || !n || n > 400u || p == hecho[0])
                continue;
            hecho[r] = p;
            for (e = p; e < p + 4u * n; e += 4u)
                if (!(MEM16(e + 2) & 0x1FFFu) && premio_de(MEM16(e)) < 0) {
                    fprintf(stderr, "[PREMIOS] %u se suma al rival %d (lista %08X, en lugar de la %u que tenia peso 0)\n",
                            s_premio[i].id, s_premio[i].rival, p, MEM16(e));
                    MEM16(e) = (uint16_t)s_premio[i].id;
                    MEM16(e + 2) = (uint16_t)(s_premio[i].bits >= 0 ? s_premio[i].bits : 0x2000);
                    break;
                }
            if (e >= p + 4u * n)
                fprintf(stderr, "[PREMIOS] %u: el rival %d no tiene entradas de peso 0 en la lista %08X\n",
                        s_premio[i].id, s_premio[i].rival, p);
        }
    }
    for (d = 0; d < 32u; ++d)
        for (tab = 0; tab < 2u; ++tab)
            for (r = 0; r < 2u; ++r) {
                uint32_t b = (tab ? 0x00453630u : 0x00453830u) + 16u * d;
                cambios += premio_lista(MEM32(b + 4u * r), MEM16(b + 8u + 2u * r));
            }
    for (t = 0; t < 5u; ++t)
        for (r = 0; r < 2u; ++r)
            cambios += premio_lista(MEM32(0x00453F78u + 16u * t + 4u * r), MEM16(0x00453F80u + 16u * t + 2u * r));
    for (d = 0; d < 32u; ++d)
        for (r = 0; r < 2u; ++r)
            cambios += premio_lista(MEM32(0x00453FC8u + 16u * d + 4u * r), MEM16(0x00453FD0u + 16u * d + 2u * r));
    fprintf(stderr, "[PREMIOS] pesos del mod: %d cartas, %d entradas cambiadas en %d listas\n", s_npremio, cambios, s_nlistas);
}

/* Antes de que arranque el juego (main.c): los datos del mod. */
void ygo_cartas_init(void)
{
    const char *dir = getenv("YGO_CARTAS");
    char path[MAX_PATH], line[2048], nombre[512] = "", desc[1536] = "", efecto[64] = "", datos[32] = "", copias[8] = "";
    uint32_t id = 0;
    FILE *f;
    if (!dir || !*dir || s_off)
        return;
    snprintf(path, sizeof path, "%s\\cartas.txt", dir);
    if (!(f = fopen(path, "r")))
        return;
    while (fgets(line, sizeof line, f)) {
        char *eq;
        if (!memcmp(line, "\xEF\xBB\xBF", 3))     /* el BOM del Bloc de notas */
            memmove(line, line + 3, strlen(line + 3) + 1);
        trim(line);
        if (!line[0] || line[0] == '#')
            continue;
        if (line[0] == '[') {
            if (id)
                aplicar(id, nombre, desc, efecto, datos, copias);
            id = (uint32_t)strtoul(line + 1, NULL, 10);
            nombre[0] = desc[0] = efecto[0] = datos[0] = copias[0] = 0;
            memset(s_nom_l, 0, sizeof s_nom_l);
            memset(s_desc_l, 0, sizeof s_desc_l);
            s_solo_es = 0;
            continue;
        }
        if (!(eq = strchr(line, '=')))
            continue;
        *eq = 0;
        trim(line);
        trim(eq + 1);
        if (!strcmp(line, "nombre"))
            snprintf(nombre, sizeof nombre, "%s", eq + 1);
        else if (!strcmp(line, "descripcion"))
            snprintf(desc, sizeof desc, "%s", eq + 1);
        else if (!strcmp(line, "efecto"))
            snprintf(efecto, sizeof efecto, "%s", eq + 1);
        else if (!strcmp(line, "datos"))
            snprintf(datos, sizeof datos, "%s", eq + 1);
        else if (!strcmp(line, "solo_espanol"))
            s_solo_es = (eq[1] == 's' || eq[1] == 'S' || eq[1] == '1');
        else if (!strcmp(line, "copias"))
            snprintf(copias, sizeof copias, "%s", eq + 1);
        else if (!strncmp(line, "premio_", 7))
            premio_campo(id, line + 7, eq + 1);
        else {
            int l;
            for (l = 1; l <= 4; ++l) {
                if (!strncmp(line, "nombre_", 7) && !strcmp(line + 7, k_suf_l[l]))
                    snprintf(s_nom_l[l], sizeof s_nom_l[l], "%s", eq + 1);
                else if (!strncmp(line, "descripcion_", 12) && !strcmp(line + 12, k_suf_l[l]))
                    snprintf(s_desc_l[l], sizeof s_desc_l[l], "%s", eq + 1);
            }
        }
    }
    if (id)
        aplicar(id, nombre, desc, efecto, datos, copias);
    fclose(f);
    premios_ajustar();
}

/* ---- copias ------------------------------------------------------------- */

/* el limite de copias de una carta (arg: imagen; lista de 61 u16 en 0x450BC0,
 * bits 14-15 las copias; si no esta, 3): lo usan el editor de mazo y la CPU
 * al armar sus mazos. Las cartas con "copias" en el mod, el suyo. */
void sub_0007EF00(void)
{
    uint32_t id = MEM32(g_esp + 4) & 0xFFFFu;
    int i;
    for (i = 0; i < s_ncopias; ++i)
        if (s_copias_id[i] == id) {
            g_eax = s_copias_n[i];
            g_esp += 4;                 /* ret */
            return;
        }
    sub_0007EF00_pad();
}

/* 2026-11-19 14:2x (the user: Sangan as a prize, "esta limitado a 1 y ya lo
 * tengo"): a card's limit as sub_0007EF00 gives it, read without calling the
 * game: the mod's first, then the game's list (61 u16 at 0x450BC0: bits 0-13
 * the id, 14-15 the copies; Sangan 4054 is 1), else 3 */
unsigned ygo_limite_carta(uint32_t id)
{
    uint32_t i;
    for (i = 0; i < (uint32_t)s_ncopias; ++i)
        if (s_copias_id[i] == id)
            return s_copias_n[i];
    for (i = 0; i < 0x3Du; ++i)
        if ((MEM16(0x00450BC0u + 2u * i) & 0x3FFFu) == (id & 0x3FFFu))
            return MEM16(0x00450BC0u + 2u * i) >> 14;
    return 3u;
}

/* 2026-11-15 (the user: 2 Golem Centinela in the deck, "se supone que esta a x1
 * copia"): the deck editor does not ask sub_0007EF00 each time. Each card's
 * limit is kept in the save (u8 at 0x665896 + k, in the save block 0x665090,
 * 0x14B0 bytes, checksummed when saved by sub_0007F630), filled from
 * sub_0007EF00 when the save is made (sub_0007F2C0): a save made before the
 * mod keeps 3 for its cards. Brought down to the mod's limit (never up: the
 * game's "Personalizado" limits stay), checked 10 times a second
 * (premios_extra.c's thread). */
void ygo_cartas_limites(void)
{
    int i;
    for (i = 0; i < s_ncopias; ++i) {
        uint32_t a = 0x00665896u + s_copias_k[i];
        if (s_copias_k[i] && MEM8(a) > s_copias_n[i] && MEM8(a) <= 3u) {
            fprintf(stderr, "[CARTAS] %u: el limite guardado era %u, ahora %u\n", s_copias_id[i], MEM8(a),
                    s_copias_n[i]);
            MEM8(a) = (uint8_t)s_copias_n[i];
        }
    }
    /* 2026-11-15 (the user: "tengo 2 copias al pedo, eliminá 1"): copies of a
     * limited mod card past its limit are useless; the chest count (u8 at
     * 0x651E70 + 0xCC1 + k, part of the save the game loads at 0x651E70) comes
     * down to the limit, never below the copies in the deck (u16 k list at
     * 0x66609A, count u16 0x66649A, as sub_000D9420 reads it). Kept once the
     * game saves. */
    for (i = 0; i < s_ncopias; ++i) {
        uint32_t k = s_copias_k[i], j, en_mazo = 0, tope, c;
        uint32_t nm = MEM16(0x0066649Au);
        if (!k || k >= NCARTAS || nm > 128u)
            continue;
        for (j = 0; j < nm; ++j)
            if (MEM16(0x0066609Au + 2u * j) == k)
                ++en_mazo;
        tope = s_copias_n[i] > en_mazo ? s_copias_n[i] : en_mazo;
        c = MEM8(0x00651E70u + 0xCC1u + k);
        if (c > tope && c <= 3u) {
            fprintf(stderr, "[CARTAS] %u: %u copias en el cofre, limite %u (en el mazo %u) -> %u\n",
                    s_copias_id[i], c, s_copias_n[i], en_mazo, tope);
            MEM8(0x00651E70u + 0xCC1u + k) = (uint8_t)tope;
        }
    }
}

/* 20:2x (the user: "lo saque lo puse de nuevo y me dejo completamente"): the
 * deck editor never asks for a limit. Putting a card in the deck,
 * sub_000DEE80 (may it go in?) and sub_000DEF20 (put it) compare the copies
 * already in the decks with the copies in the chest (u8 0x651E70 + 0xCC1 + k;
 * the card under the cursor: u16 at 0x66C880 + 8 + 2 * (u16 [+0x12F6] + u16
 * [+0x12F8])). While they run, a mod card with a limit shows the chest at most
 * that many copies; whatever they change there is kept. */
#define COFRE(k) (0x00651E70u + 0xCC1u + (uint32_t)(k))
static int limite_de_k(uint32_t k)
{
    int i;
    for (i = 0; i < s_ncopias; ++i)
        if (s_copias_k[i] == k)
            return s_copias_n[i];
    return -1;
}

static void con_limite(void (*f)(void), const char *que)
{
    uint32_t ui = 0x0066C880u;
    uint32_t k = MEM16(ui + 8u + 2u * (MEM16(ui + 0x12F6u) + MEM16(ui + 0x12F8u)));
    int n = k && k < NCARTAS ? limite_de_k(k) : -1;
    uint8_t antes, puesto;
    if (n < 0 || MEM8(COFRE(k)) <= (uint32_t)n) {
        f();
        return;
    }
    antes = (uint8_t)MEM8(COFRE(k));
    puesto = (uint8_t)n;
    MEM8(COFRE(k)) = puesto;
    f();
    MEM8(COFRE(k)) = (uint8_t)(antes + (uint8_t)(MEM8(COFRE(k)) - puesto));
    fprintf(stderr, "[CARTAS] mazo: k %u limitada a %d (en el cofre %u) en %s -> %u\n", k, n, antes, que,
            g_eax & 0xFFu);
}

void sub_000DEE80_pad(void);
void sub_000DEE80(void) { con_limite(sub_000DEE80_pad, "DEE80"); }
void sub_000DEF20_pad(void);
void sub_000DEF20(void) { con_limite(sub_000DEF20_pad, "DEF20"); }

/* ---- prision_dimensional ------------------------------------------------ */

/* la condicion de Cilindro magico: el adversario declaro un ataque */
void sub_000A6810(void)
{
    uint32_t ctx = MEM32(g_esp + 4);
    int k = -1;
    if (ctx && s_k_prision >= 0 && MEM16(ctx) == (uint32_t)s_k_prision)
        k = s_k_prision;
    else if (ctx && s_k_escudo >= 0 && MEM16(ctx) == (uint32_t)s_k_escudo)
        k = s_k_escudo;
    if (k >= 0) {
        uint16_t id = MEM16(T_IMAGEN + 2u * (uint32_t)k);
        MEM16(T_IMAGEN + 2u * (uint32_t)k) = ID_CILINDRO;
        sub_000A6810_pad();
        MEM16(T_IMAGEN + 2u * (uint32_t)k) = id;
        return;
    }
    sub_000A6810_pad();
}

/* escudo_drenador: niega el ataque como Cilindro magico (orden 0x23 con la
 * casilla del atacante) y el duenio de la trampa gana LP iguales al ATK del
 * atacante (sub_000D8130(p, z) = ATK; sub_000C8DF0(jugador, n) = ganar LP,
 * como Medicina roja) */
static void escudo_drenador(uint32_t ctx)
{
    uint32_t sb = g_ebx, si = g_esi, di = g_edi, bp = g_ebp;
    uint32_t p = MEM8(ctx + 6) & 1u, z = MEM8(ctx + 7), yo = MEM8(ctx + 2) & 1u, atk;
    uint32_t slot = p * 0x13F8u + z * 0x9Cu;
    if (!(MEM8(ctx + 4) & 4u) && z < 5u && (MEM32(slot + 0x0065D0C8u) & 0x7FFFu)) {
        PUSH32(g_esp, 0);
        PUSH32(g_esp, 0);
        PUSH32(g_esp, z);
        PUSH32(g_esp, 0x23u | (p ? 0x8000u : 0u));
        PUSH32(g_esp, 0x000AF74Eu);
        sub_00081150();
        g_esp += 16;
        PUSH32(g_esp, z);
        PUSH32(g_esp, p);
        PUSH32(g_esp, 0x000AF755u);
        sub_000D8130();
        g_esp += 8;
        atk = g_eax;                    /* Cilindro usa el eax entero */
        if ((int32_t)atk > 0) {
            PUSH32(g_esp, atk);
            PUSH32(g_esp, yo);
            PUSH32(g_esp, 0x000AA429u);
            sub_000C8DF0();
            g_esp += 8;
        }
        fprintf(stderr, "[CARTAS] Escudo drenador: ataque del jugador %u (casilla %u) negado; el jugador %u gana %u LP\n",
                p, z, yo, atk);
    } else
        fprintf(stderr, "[CARTAS] Escudo drenador: sin atacante (jugador %u, casilla %u)\n", p, z);
    g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
    g_eax &= 0xFFFF0000u;
    g_esp += 4;                         /* ret */
}

void sub_000AF600(void)
{
    uint32_t ctx = MEM32(g_esp + 4);
    int k = s_k_prision;
    if (s_k_escudo >= 0 && ctx && MEM16(ctx) == (uint32_t)s_k_escudo) {
        escudo_drenador(ctx);
        return;
    }
    if (k >= 0 && ctx && MEM16(ctx) == (uint32_t)k) {
        uint32_t sb = g_ebx, si = g_esi, di = g_edi, bp = g_ebp;
        uint32_t p = MEM8(ctx + 6), z = MEM8(ctx + 7);
        uint32_t slot = (p & 1u) * 0x13F8u + z * 0x9Cu;
        if (!(MEM8(ctx + 4) & 4u) && z < 5u && (MEM32(slot + 0x0065D0C8u) & 0x7FFFu)
                && (MEM8(slot + 0x0065D0D2u) & 2u)) {
            PUSH32(g_esp, z);
            PUSH32(g_esp, p);
            PUSH32(g_esp, (uint32_t)k);
            PUSH32(g_esp, 0x000B8439u);
            sub_000A1F20();
            g_esp += 12;
            if (g_eax & 0xFFFFu) {
                PUSH32(g_esp, 0);
                PUSH32(g_esp, z);
                PUSH32(g_esp, p);
                PUSH32(g_esp, 0x000B845Bu);
                sub_000C69A0();
                g_esp += 12;
                fprintf(stderr, "[CARTAS] Prision dimensional: atacante del jugador %u, casilla %u, desterrado\n",
                        p, z);
            } else
                fprintf(stderr, "[CARTAS] Prision dimensional: el atacante no puede ser afectado\n");
        } else
            fprintf(stderr, "[CARTAS] Prision dimensional: sin atacante (jugador %u, casilla %u)\n", p, z);
        g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
        g_eax &= 0xFFFF0000u;
        g_esp += 4;                     /* ret */
        return;
    }
    sub_000AF600_pad();
}

/* ---- ciclon_cosmos y golpe_fenix -------------------------------------- */
/* Las dos usan la entrada de Tifon de espacio esoterico (elegir una carta del
 * campo: condicion sub_000A6C90, casilla valida sub_000A35D0, eleccion
 * sub_000BD420, resolver sub_000AB150), con su costo en el campo 4 (se paga
 * al activarla): Ciclon el de Mundo de Toon (1000 LP), Golpe el de descartar
 * (sub_000A3FB0, la carta pasa por Magic Jammer: 1 carta). */

static int es_k(uint32_t ctx, int k) { return k >= 0 && ctx && MEM16(ctx) == (uint32_t)k; }

/* la carta de la casilla (p, z), 0..10, o 0 */
static uint32_t carta_en(uint32_t p, uint32_t z)
{
    uint32_t c;
    if (z > 10u)
        return 0;
    c = MEM16((p & 1u) * 0x13F8u + z * 0x9Cu + 0x0065D0C8u) & 0x7FFFu;
    return (c && c <= MEM16(0x005FE690u)) ? c : 0u;
}

static void ret16(uint32_t v)
{
    g_eax = (g_eax & 0xFFFF0000u) | (v & 0xFFFFu);
    g_esp += 4;                         /* ret */
}

/* la condicion: Ciclon, ademas, que le queden LP despues de pagar 1000;
 * Golpe: el adversario controla alguna carta y el duenio tiene mano para
 * descartar (cantidad u8 en 0x65D0A8 + jugador * 0x13F8) */
void sub_000A6C90(void)
{
    uint32_t ctx = MEM32(g_esp + 4), yo, z, hay = 0;
    if (es_k(ctx, s_k_ciclon)
            && MEM32(0x0065D0A4u + (MEM8(ctx + 2) & 1u) * 0x13F8u) <= 1000u) {
        ret16(0);
        return;
    }
    if (es_k(ctx, s_k_fenix)) {
        yo = MEM8(ctx + 2) & 1u;
        for (z = 0; z <= 10u && !hay; ++z)
            hay = carta_en(yo ^ 1u, z) != 0;
        ret16(hay && MEM8(0x0065D0A8u + yo * 0x13F8u) > 0);
        return;
    }
    sub_000A6C90_pad();
}

/* la casilla valida (args: ctx, u16 jugador | casilla << 8): Golpe, cualquier
 * carta del adversario */
void sub_000A35D0(void)
{
    uint32_t ctx = MEM32(g_esp + 4), pz = MEM16(g_esp + 8);
    if (es_k(ctx, s_k_fenix)) {
        uint32_t p = pz & 0xFFu, z = pz >> 8;
        ret16((p & 1u) != (MEM8(ctx + 2) & 1u) && carta_en(p, z) != 0);
        return;
    }
    sub_000A35D0_pad();
}

/* el costo de descartar: Golpe descarta 1, como Magic Jammer */
void sub_000A3FB0(void)
{
    uint32_t ctx = MEM32(g_esp + 4);
    if (es_k(ctx, s_k_fenix)) {
        uint32_t k = (uint32_t)s_k_fenix;
        uint16_t id = MEM16(T_IMAGEN + 2u * k);
        MEM16(T_IMAGEN + 2u * k) = (uint16_t)ID_JAMMER;
        sub_000A3FB0_pad();
        MEM16(T_IMAGEN + 2u * k) = id;
        return;
    }
    sub_000A3FB0_pad();
}

/* la eleccion (sub_000BD420, devuelve 1 cuando hay objetivo en ctx +0xC/+0xD;
 * sub_000BAA20(ctx, p, z) lo anota): Golpe, con las casillas del adversario.
 *   la CPU (jugador 1): la primera carta del adversario;
 *   el jugador: lo mismo que Tifon, con otro texto, el cursor en el campo
 *   del adversario y la mascara de sub_00092E20 con sus monstruos (0xF0) y
 *   sus magicas/trampas (0x0E) (16 bits por jugador). */
void sub_000BD420(void)
{
    uint32_t ctx = MEM32(g_esp + 4);
    uint32_t sa, sb, sc, sd, si, di, bp, yo, op, z, r = 0;
    if (!es_k(ctx, s_k_fenix)) {
        sub_000BD420_pad();
        return;
    }
    sa = g_eax; sb = g_ebx; sc = g_ecx; sd = g_edx; si = g_esi; di = g_edi; bp = g_ebp;
    (void)sa; (void)sc; (void)sd;
    yo = MEM8(ctx + 2) & 1u;
    op = yo ^ 1u;
    if (yo) {
        MEM8(ctx + 0xA) &= 0xF8u;
        for (z = 0; z <= 10u && !r; ++z)
            if (carta_en(op, z)) {
                PUSH32(g_esp, z);
                PUSH32(g_esp, op);
                PUSH32(g_esp, ctx);
                PUSH32(g_esp, 0x000BD482u);
                sub_000BAA20();
                g_esp += 12;
                r = (g_eax & 0xFFFFu) != 0;
            }
    } else if (!MEM8(0x0066BFC1u)) {
        uint32_t zona = 5u, cas = 0;
        for (z = 0; z < 5u; ++z)
            if (carta_en(op, z)) {
                zona = 0;
                cas = z;
                break;
            }
        PUSH32(g_esp, s_txt_fenix);
        PUSH32(g_esp, 0xBu);
        PUSH32(g_esp, 0x712u);
        PUSH32(g_esp, 0x206u);
        PUSH32(g_esp, 0x000BD569u);
        sub_00087E20();
        g_esp += 16;
        MEM8(ctx + 0xA) &= 0xF8u;
        PUSH32(g_esp, cas);
        PUSH32(g_esp, zona);
        PUSH32(g_esp, op);
        PUSH32(g_esp, 0x000BD586u);
        sub_00086F50();
        g_esp += 12;
        MEM8(0x0066BFC1u) = (uint8_t)(MEM8(0x0066BFC1u) + 1u);
    } else if (MEM8(0x006589E6u) & 2u)
        MEM8(0x0066BFC1u) = 0;
    else {
        PUSH32(g_esp, 0xFEu << (16u * op));
        PUSH32(g_esp, 0x000BD50Au);
        sub_00092E20();
        g_esp += 4;
        if (g_eax & 0xFFFFu) {
            PUSH32(g_esp, MEM32(0x006633D8u) + MEM32(0x006633DCu));
            PUSH32(g_esp, MEM32(0x006633D4u));
            PUSH32(g_esp, ctx);
            PUSH32(g_esp, 0x000BD52Du);
            sub_000BAA20();
            g_esp += 12;
            r = (g_eax & 0xFFFFu) != 0;
            if (!r) {
                PUSH32(g_esp, 0x10000004u);
                PUSH32(g_esp, 0x000BD545u);
                sub_0007E280();
                g_esp += 4;
            }
        }
    }
    g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
    ret16(r);
}

/* el resolver de Tifon (sub_000AB150), con las mismas comprobaciones (anulada,
 * ctx +0xA & 7 == 1, casilla con carta; objetivo en ctx +0xC jugador, +0xD
 * casilla):
 *   Ciclon: sub_000C69A0(p, z, 0), destierra (2026-11-13, pedido del usuario);
 *   Golpe: a la parte superior del Deck con sub_000C8470(p, z) (lo que usan
 *   Monstruo recuperacion y otras antes de barajar con la orden 0x3F; sin
 *   barajar queda arriba). */
void sub_000AB150(void)
{
    uint32_t ctx = MEM32(g_esp + 4);
    int cic = es_k(ctx, s_k_ciclon), fen = es_k(ctx, s_k_fenix);
    const char *nom = cic ? "Ciclon del cosmos" : "Golpe de viento de ala de fenix";
    uint32_t sb, si, di, bp, p, z, c;
    {   /* 2026-11-12: el Hombre de Nieve (como Come Hombres, mismo resolver) se
         * destruyo a si mismo; queda anotado a quien apunta al resolver */
        int i;
        for (i = 0; i < s_nal; ++i)
            if (es_k(ctx, (int)s_al[i].k))
                fprintf(stderr, "[CARTAS] resolver de la carta k %u (como %u): duenio %u, objetivo jugador %u "
                        "casilla %u carta k %u, anulada %u, estado %u\n", s_al[i].k, s_al[i].fx,
                        MEM8(ctx + 2) & 1u, MEM8(ctx + 0xC) & 1u, MEM8(ctx + 0xD),
                        carta_en(MEM8(ctx + 0xC), MEM8(ctx + 0xD)), (MEM8(ctx + 4) >> 2) & 1u,
                        MEM8(ctx + 0xA) & 7u);
    }
    if (!fen && !cic) {
        sub_000AB150_pad();
        return;
    }
    sb = g_ebx; si = g_esi; di = g_edi; bp = g_ebp;
    p = MEM8(ctx + 0xC) & 1u;
    z = MEM8(ctx + 0xD);
    c = carta_en(p, z);
    if (MEM8(ctx + 4) & 4u)
        fprintf(stderr, "[CARTAS] %s: anulada\n", nom);
    else if ((MEM8(ctx + 0xA) & 7u) != 1u || !c)
        fprintf(stderr, "[CARTAS] %s: sin objetivo (jugador %u, casilla %u)\n", nom, p, z);
    else if (cic) {
        PUSH32(g_esp, 0);
        PUSH32(g_esp, z);
        PUSH32(g_esp, p);
        PUSH32(g_esp, 0x000B845Bu);
        sub_000C69A0();
        g_esp += 12;
        fprintf(stderr, "[CARTAS] %s: destierra la carta k %u (jugador %u, casilla %u)\n", nom, c, p, z);
    } else {
        PUSH32(g_esp, z);
        PUSH32(g_esp, p);
        PUSH32(g_esp, 0x000B4D51u);
        sub_000C8470();
        g_esp += 8;
        fprintf(stderr, "[CARTAS] %s: la carta k %u (jugador %u, casilla %u) va arriba del Deck\n", nom, c, p, z);
    }
    g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
    ret16(0);
}

/* ---- krebons ------------------------------------------------------------ */

/* la declaracion del ataque (arg: el jugador que ataca) */
void sub_0009A290(void)
{
    uint32_t p = MEM32(g_esp + 4) & 1u, at = MEM32(0x006666E0u);
    int k = s_k_krebons;
    uint32_t d, z, slot;
    sub_0009A290_pad();
    if ((g_eax & 0xFFFFu) && MEM8(0x0065F8DEu) == 1) {
        bls_ataque(p, at);
    }
    if (k < 0 || !(g_eax & 0xFFFFu) || MEM8(0x0065F8DEu) != 1 || (at & 1u))
        return;
    d = p ^ 1u;
    z = (at >> 13) & 7u;
    slot = d * 0x13F8u + z * 0x9Cu;
    if (z < 5u && (MEM32(slot + 0x0065D0C8u) & 0x7FFFu) == (uint32_t)k
            && (MEM8(slot + 0x0065D0D2u) & 2u)) {
        uint32_t sa = g_eax, sb = g_ebx, sc = g_ecx, sd = g_edx, si = g_esi, di = g_edi, bp = g_ebp;
        uint32_t lp = MEM32(0x0065D0A4u + d * 0x13F8u);
        if (lp > 800u) {
            PUSH32(g_esp, 800u);
            PUSH32(g_esp, d);
            PUSH32(g_esp, 0x000AF75Cu);
            sub_000C8BC0();
            g_esp += 8;
            PUSH32(g_esp, 0);
            PUSH32(g_esp, 0);
            PUSH32(g_esp, (at >> 10) & 7u);
            PUSH32(g_esp, 0x23u | (p ? 0x8000u : 0u));
            PUSH32(g_esp, 0x000AF74Eu);
            sub_00081150();
            g_esp += 16;
            fprintf(stderr, "[CARTAS] Krebons (jugador %u, casilla %u): paga 800 LP (tenia %u), ataque negado\n",
                    d, z, lp);
        } else
            fprintf(stderr, "[CARTAS] Krebons (jugador %u, casilla %u): %u LP, no alcanza para pagar 800\n",
                    d, z, lp);
        g_eax = sa; g_ebx = sb; g_ecx = sc; g_edx = sd; g_esi = si; g_edi = di; g_ebp = bp;
    }
}

/* ---- sincronizador ------------------------------------------------------ */

/* la carta k pasa por Senju mientras corre f (y vuelve a su imagen) */
static void como_senju(uint32_t k, void (*f)(void))
{
    uint16_t id = MEM16(T_IMAGEN + 2u * k);
    MEM16(T_IMAGEN + 2u * k) = (uint16_t)ID_SENJU;
    f();
    MEM16(T_IMAGEN + 2u * k) = id;
}

/* las maquinas de la llamada (tipo en 0x66BA8E bits 2-4, sub_00095A30): en
 * el estado de los disparos "al ser llamada" la carta pasa por Senju */
static int es_disparo(uint32_t estado)
{
    int k = s_k_sinc;
    if (k < 0 || ((MEM16(0x0066BA8Eu) >> 5) & 0x7Fu) != estado
            || (MEM32(0x0066BA80u) & 0xFFFFu) != (uint32_t)k)
        return 0;
    fprintf(stderr, "[CARTAS] Sincronizador de Basura llamado (jugador %u, tipo %u): se registra el disparo\n",
            MEM32(0x0066BA7Cu) & 1u, (MEM16(0x0066BA8Eu) >> 2) & 7u);
    return 1;
}

/* Caius: en el mismo estado pasa por Buscador de dragon (4506), que el juego
 * dispara al ser llamado sin Llamada Especial (Caius es Nivel 6: siempre por
 * Tributo) */
static int es_caius(uint32_t estado)
{
    int k = s_k_caius;
    if (k < 0 || ((MEM16(0x0066BA8Eu) >> 5) & 0x7Fu) != estado
            || (MEM32(0x0066BA80u) & 0xFFFFu) != (uint32_t)k)
        return 0;
    fprintf(stderr, "[CARTAS] Caius llamado (jugador %u, tipo %u): se registra el disparo (como Buscador de dragon)\n",
            MEM32(0x0066BA7Cu) & 1u, (MEM16(0x0066BA8Eu) >> 2) & 7u);
    return 1;
}

static void como_buscador(void (*f)(void))
{
    uint32_t k = (uint32_t)s_k_caius;
    uint16_t id = MEM16(T_IMAGEN + 2u * k);
    MEM16(T_IMAGEN + 2u * k) = (uint16_t)ID_BUSCADOR;
    f();
    MEM16(T_IMAGEN + 2u * k) = id;
}

/* la Llamada Normal (tipo 1): el estado 2 */
void sub_00093FA0(void)
{
    /* Colocar: el estado 1 con la carta boca abajo termina sin pasar por el 2 */
    int colocar = ((MEM16(0x0066BA8Eu) >> 5) & 0x7Fu) == 1u && !(MEM32(0x0066BA7Cu) & 0x4000u);
    if (es_disparo(2u))
        como_senju((uint32_t)s_k_sinc, sub_00093FA0_pad);
    else if (es_caius(2u))
        como_buscador(sub_00093FA0_pad);
    else
        sub_00093FA0_pad();
    cosmo_llamada();
    if (colocar)
        cosmo_maquina(0u, 1, "Colocar");
}

/* Las maquinas de Llamada (despacho sub_00095A30; tipo = (u16 0x66BA8E >> 2)
 * & 7, estado = (>> 5) & 0x7F; jugador u32 0x66BA7C bit 0, carta u16 0x66BA80):
 *   1 sub_00093FA0 Llamada Normal Y Colocar (estado 1: con 0x66BA7C bit 14 =
 *     boca arriba sigue al 2, donde registra los disparos; boca abajo termina)
 *   2 sub_00094610 Llamada Normal boca arriba (fuerza 0x4000; disparos en el 4)
 *   3 sub_00094AF0 Llamada de Volteo (la arma sub_00096160)
 *   4 sub_000951D0 Llamada Especial desde Deck/Cementerio (sub_000961F0/962E0)
 *   5 sub_00095450 Llamada Especial desde la mano (sub_000963C0, la Momia)
 *   6 sub_000957D0 (la arma sub_000964A0)
 * 951D0 y 95450 quedaron envueltas (rename_pads) sin cambios: por si una carta
 * necesita la Llamada Especial del adversario. */
void sub_000951D0_pad(void);
void sub_00095450_pad(void);
void sub_000951D0(void) { sub_000951D0_pad(); }
void sub_00095450(void) { sub_00095450_pad(); }

/* la otra llamada (tipo 2): el estado 4 */
void sub_00094610(void)
{
    uint32_t est = (MEM16(0x0066BA8Eu) >> 5) & 0x7Fu;
    if (es_disparo(4u))
        como_senju((uint32_t)s_k_sinc, sub_00094610_pad);
    else if (es_caius(4u))
        como_buscador(sub_00094610_pad);
    else
        sub_00094610_pad();
    cosmo_maquina(est, est == 4u, "Llamada por Sacrificio");
}

/* la busqueda del efecto (arg: k): la entrada de Senju (las cerezas: la de
 * Momonga agil); las cartas "como otra", la de esa */
static void busqueda_efecto(void);

void sub_000C4DE0(void)
{
    CON_ALIAS(0, busqueda_efecto);
}

/* las funciones que comparan la imagen de la carta por su cuenta:
 * la Llamada de Volteo (sub_00094AF0, la maquina del tipo 3: Des Lacooda roba
 * ahi), el resolver de Des Lacooda/Skelengel (sub_000ADDE0), las casillas
 * validas de Come Hombres (sub_000BB9E0) y del Guardia del Guardatumba
 * (sub_000BAFE0); y el repaso del comienzo del turno (sub_000CFE70: marca
 * 0x2000 en las que se pueden poner boca abajo) */
void sub_00094AF0_pad(void);
void sub_000ADDE0_pad(void);
void sub_000BB9E0_pad(void);
void sub_000BAFE0_pad(void);
void sub_000CFE70_pad(void);
void sub_00098EF0_pad(void);
void sub_00094AF0(void) { CON_ALIAS(0, sub_00094AF0_pad); }
static void vidente(uint32_t ctx);
void sub_000ADDE0(void)
{
    uint32_t ctx = MEM32(g_esp + 4);
    if (es_k(ctx, s_k_vidente)) {
        vidente(ctx);
        return;
    }
    CON_ALIAS(0, sub_000ADDE0_pad);
}
void sub_000BB9E0(void) { CON_ALIAS(0, sub_000BB9E0_pad); }
void sub_000BAFE0(void)
{
    if (s_k_bls > 0 && es_k(MEM32(g_esp + 4), s_k_bls)) {
        CON_ALIAS(0, sub_000BB9E0_pad);     /* Soldado: cualquier monstruo del Campo */
        return;
    }
    CON_ALIAS(0, sub_000BAFE0_pad);
}
void sub_000CFE70(void) { CON_ALIAS(1, sub_000CFE70_pad); }
/* los comandos de una carta del campo (sub_00098EF0, tambien los de la CPU via
 * sub_0008A240): "boca abajo" (bit 0x10) si su imagen es 0x1527..0x1535 (tabla
 * 0x991F8) y tiene la marca 0x2000 del turno; el Golem pasa por Des Lacooda */
void sub_00098EF0(void) { bls_segundo(); CON_ALIAS(1, sub_00098EF0_pad); }

static void busqueda_efecto(void)
{
    int k = s_k_sinc;
    if (s_k_cer >= 0 && (MEM32(g_esp + 4) & 0xFFFFu) == (uint32_t)s_k_cer) {
        uint16_t id = MEM16(T_IMAGEN + 2u * (uint32_t)s_k_cer);
        MEM16(T_IMAGEN + 2u * (uint32_t)s_k_cer) = (uint16_t)ID_MOMONGA;
        sub_000C4DE0_pad();
        MEM16(T_IMAGEN + 2u * (uint32_t)s_k_cer) = id;
        return;
    }
    if (k >= 0 && (MEM32(g_esp + 4) & 0xFFFFu) == (uint32_t)k) {
        como_senju((uint32_t)k, sub_000C4DE0_pad);
        return;
    }
    if (s_k_caius >= 0 && (MEM32(g_esp + 4) & 0xFFFFu) == (uint32_t)s_k_caius) {
        como_buscador(sub_000C4DE0_pad);
        return;
    }
    sub_000C4DE0_pad();
}

/* la lista de candidatos (args: jugador, imagen, 0; deja las entradas de 8
 * bytes en 0x65855C, la cantidad en u16 0x6589DC y la devuelve): mientras el
 * Sincronizador elige, la de Llamada del encantado (los monstruos del propio
 * Cementerio) queda solo con los de Nivel 1 o 2 */
static int s_sinc_lista;                /* 1 mientras el Sincronizador elige */
static void acant_lista(uint32_t p);
static void beren_lista(uint32_t p);

void sub_000C1B80(void)
{
    uint32_t id = MEM32(g_esp + 8) & 0xFFFFu, n, i, m = 0, jug = MEM32(g_esp + 4) & 1u;
    sub_000C1B80_pad();
    if (s_acant_lista) {
        acant_lista(jug);
        return;
    }
    if (s_beren_lista) {
        beren_lista(jug);
        return;
    }
    if (s_bls_lista && id == ID_ALMA) {
        bls_lista(jug);
        return;
    }
    if (!s_sinc_lista || id != ID_LLAMADA)
        return;
    n = MEM16(0x006589DCu);
    for (i = 0; i < n && i < 128u; ++i) {
        uint32_t a = MEM32(0x0065855Cu + 8u * i), b = MEM32(0x00658560u + 8u * i);
        uint32_t c = a & 0x7FFFu, d, tipo, nivel;
        if (!c || c >= NCARTAS)
            continue;
        d = MEM32(T_DATOS + 4u * c);
        tipo = (d >> 20) & 0x1Fu;
        nivel = (d >> 25) & 0xFu;
        if (tipo == 21u || tipo == 22u || nivel < 1u || nivel > 2u)
            continue;
        MEM32(0x0065855Cu + 8u * m) = a;
        MEM32(0x00658560u + 8u * m) = b;
        ++m;
    }
    MEM16(0x006589DCu) = (uint16_t)m;
    g_eax = m;
}

/* el resolver de Senju, con nuestra carta: los mismos pasos que Senju (estado
 * en 0x66BFBC, cada llamada devuelve el siguiente; 0 = termino):
 *   0x80 lista (Llamada del encantado filtrada); vacia o sin casilla: nada.
 *        Si/No con sub_000880B0(texto) -> 0x7F
 *   0x7F respuesta en u16 0x660B8C (0 = No: termina); "Selecciona..." con
 *        sub_00087E20(0x206, 0x712, 0xB, texto) -> 0x7E
 *   0x7E la lista en pantalla, sub_000C5AE0(p, -1, imagen, 0) -> 0x7D
 *   0x7D lo elegido: 0x65855C[u16 0x658556 + (0x658555 & 3)]; Llamada
 *        Especial en defensa como Llamada del encantado -> 0x7C (fin) */
void sub_000B1220(void)
{
    uint32_t ctx = MEM32(g_esp + 4);
    int k = s_k_sinc;
    uint32_t sa, sb, sc, sd, si, di, bp, p, st, ret = 0;
    if (k < 0 || !ctx || MEM16(ctx) != (uint32_t)k) {
        sub_000B1220_pad();
        return;
    }
    sa = g_eax; sb = g_ebx; sc = g_ecx; sd = g_edx; si = g_esi; di = g_edi; bp = g_ebp;
    (void)sa;
    p = MEM8(ctx + 2) & 1u;
    st = MEM8(0x0066BFBCu);
    if (MEM8(ctx + 4) & 4u)
        st = 0;                         /* anulado */
    switch (st) {
    case 0x80:
        s_sinc_lista = 1;
        PUSH32(g_esp, 0);
        PUSH32(g_esp, ID_LLAMADA);
        PUSH32(g_esp, p);
        PUSH32(g_esp, 0x000B126Au);
        sub_000C1B80();
        g_esp += 12;
        if (!(g_eax & 0xFFFFu)) {
            fprintf(stderr, "[CARTAS] Sincronizador de Basura: no hay monstruos de Nivel 2 o menor en el Cementerio\n");
            break;
        }
        PUSH32(g_esp, p);
        PUSH32(g_esp, 0x000B0A01u);
        sub_000D39D0();
        g_esp += 4;
        if (!g_eax) {
            fprintf(stderr, "[CARTAS] Sincronizador de Basura: sin casilla libre\n");
            break;
        }
        PUSH32(g_esp, s_txt_pregunta);
        PUSH32(g_esp, 0x000B12A7u);
        sub_000880B0();
        g_esp += 4;
        ret = 0x7Fu;
        break;
    case 0x7F:
        if (!MEM16(0x00660B8Cu)) {
            fprintf(stderr, "[CARTAS] Sincronizador de Basura: el jugador %u dijo que no\n", p);
            break;
        }
        PUSH32(g_esp, s_txt_elegir);
        PUSH32(g_esp, 0xBu);
        PUSH32(g_esp, 0x712u);
        PUSH32(g_esp, 0x206u);
        PUSH32(g_esp, 0x000B12DAu);
        sub_00087E20();
        g_esp += 16;
        ret = 0x7Eu;
        break;
    case 0x7E:
        PUSH32(g_esp, 0);
        PUSH32(g_esp, ID_LLAMADA);
        PUSH32(g_esp, 0xFFFFFFFFu);
        PUSH32(g_esp, p);
        PUSH32(g_esp, 0x000B1301u);
        sub_000C5AE0();
        g_esp += 16;
        ret = 0x7Du;
        break;
    case 0x7D: {
        uint32_t i = MEM16(0x00658556u) + (MEM8(0x00658555u) & 3u), e, pd, c;
        if (i >= MEM16(0x006589DCu)) {
            fprintf(stderr, "[CARTAS] Sincronizador de Basura: eleccion %u fuera de la lista\n", i);
            break;
        }
        e = MEM32(0x0065855Cu + 8u * i);
        c = e & 0x7FFFu;
        PUSH32(g_esp, 0);
        PUSH32(g_esp, e >> 16);
        PUSH32(g_esp, e & 0xFFFFu);
        PUSH32(g_esp, 0xA0u | (p << 15));
        PUSH32(g_esp, 0x000B0A2Du);
        sub_00081150();
        g_esp += 16;
        g_esp -= 4;                     /* la entrada, en la pila del juego */
        MEM32(g_esp) = e;
        pd = g_esp;
        PUSH32(g_esp, 0x20u);
        PUSH32(g_esp, 1u);              /* defensa */
        PUSH32(g_esp, 1u);              /* boca arriba */
        PUSH32(g_esp, pd);
        PUSH32(g_esp, p);
        PUSH32(g_esp, 0x000B0A46u);
        sub_000961F0();
        g_esp += 20 + 4;
        fprintf(stderr, "[CARTAS] Sincronizador de Basura (jugador %u): llama del Cementerio la carta k %u "
                "(imagen %u) en defensa\n", p, c, c < NCARTAS ? MEM16(T_IMAGEN + 2u * c) : 0u);
        ret = 0x7Cu;
        break;
    }
    default:
        break;
    }
    if (ret == 0u || ret == 0x7Cu)
        s_sinc_lista = 0;
    g_ebx = sb; g_ecx = sc; g_edx = sd; g_esi = si; g_edi = di; g_ebp = bp;
    g_eax = (g_eax & 0xFFFF0000u) | ret;
    g_esp += 4;                         /* ret */
}

/* ---- cerezas ------------------------------------------------------------ */
/* Cerezas Naturia: al ir al Cementerio por batalla, invoca hasta 2 copias del
 * Deck boca abajo en defensa. Es lo de Momonga agil (4922) sin los 1000 LP y
 * con tope de 2:
 *   - sub_000C7C50 (una carta va al Cementerio; arg 4 = puntero a su entrada,
 *     k en los bits 0-14) compara la imagen con 0x133A y registra el disparo:
 *     con las cerezas, la carta pasa por Momonga mientras corre.
 *   - sub_000C4DE0: la entrada de efecto de Momonga (resolver sub_000B13A0).
 *   - sub_000B13A0 (estado 0x66BFBC): 0x80 da 1000 LP (Momonga) -> 0x7F lista
 *     del Deck con la misma imagen (sub_000C1B80), pregunta -> 0x7E saca la
 *     carta (sub_000C7070) -> 0x7D sub_000961F0(p, 0x66C130, 0 boca abajo,
 *     1 defensa, 0) -> 0x7F otra vez. Con las cerezas: 0x80 sin LP; en los
 *     demas la carta pasa por Momonga y la Momonga de verdad queda sin imagen
 *     (no entra en la lista); tras 2 invocaciones baraja (orden 0x3F) y fin. */
void sub_000C7C50_pad(void);
void sub_000B13A0_pad(void);
static int isaac_al_cementerio(uint32_t p, uint32_t z, uint32_t pe);
static void real_cementerio(uint32_t p, uint32_t pe);
static int s_cer_n;                     /* invocadas en esta resolucion */

void sub_000C7C50(void)
{
    uint32_t pe = MEM32(g_esp + 0x10);
    int k = s_k_cer;
    if (isaac_al_cementerio(MEM32(g_esp + 4) & 1u, MEM32(g_esp + 0xC), pe))
        return;                         /* ya llamo a la original */
    real_cementerio(MEM32(g_esp + 4) & 1u, pe);
    if (pe && como_sangan(MEM32(pe) & 0x7FFFu)) {
        uint32_t c = MEM32(pe) & 0x7FFFu;
        uint16_t ia = MEM16(T_IMAGEN + 2u * c);
        fprintf(stderr, "[CARTAS] %s va al Cementerio por batalla: pasa por Sangan\n",
                c == (uint32_t)s_k_acant ? "Acantilado Naturia" : "Berenjena Naturia");
        MEM16(T_IMAGEN + 2u * c) = (uint16_t)ID_SANGAN;
        sub_000C7C50_pad();
        MEM16(T_IMAGEN + 2u * c) = ia;
        return;
    }
    if (k >= 0 && pe && (MEM32(pe) & 0x7FFFu) == (uint32_t)k) {
        uint16_t id = MEM16(T_IMAGEN + 2u * (uint32_t)k);
        fprintf(stderr, "[CARTAS] Cerezas Naturia va al Cementerio (causa %u): pasa por Momonga agil\n",
                MEM32(g_esp + 0x14) & 0xFFFFu);
        MEM16(T_IMAGEN + 2u * (uint32_t)k) = (uint16_t)ID_MOMONGA;
        sub_000C7C50_pad();
        MEM16(T_IMAGEN + 2u * (uint32_t)k) = id;
        return;
    }
    sub_000C7C50_pad();
}

void sub_000B13A0(void)
{
    uint32_t ctx = MEM32(g_esp + 4), p, st, r;
    int k = s_k_cer, km = s_k_momonga;
    uint16_t id, idm = 0;
    if (!es_k(ctx, k) || (MEM8(ctx + 4) & 4u)) {
        sub_000B13A0_pad();
        return;
    }
    p = MEM8(ctx + 2) & 1u;
    st = MEM8(0x0066BFBCu);
    if (st == 0x80u) {
        s_cer_n = 0;
        fprintf(stderr, "[CARTAS] Cerezas Naturia (jugador %u): busca copias en el Deck\n", p);
        ret16(0x7Fu);
        return;
    }
    id = MEM16(T_IMAGEN + 2u * (uint32_t)k);
    MEM16(T_IMAGEN + 2u * (uint32_t)k) = (uint16_t)ID_MOMONGA;
    if (km > 0) {
        idm = MEM16(T_IMAGEN + 2u * (uint32_t)km);
        MEM16(T_IMAGEN + 2u * (uint32_t)km) = 0;
    }
    sub_000B13A0_pad();
    if (km > 0)
        MEM16(T_IMAGEN + 2u * (uint32_t)km) = idm;
    MEM16(T_IMAGEN + 2u * (uint32_t)k) = id;
    r = g_eax & 0xFFFFu;
    if (st == 0x7Du && r == 0x7Fu) {
        ++s_cer_n;
        fprintf(stderr, "[CARTAS] Cerezas Naturia (jugador %u): invoca la copia %d boca abajo en defensa\n",
                p, s_cer_n);
        if (s_cer_n >= 2) {
            uint32_t sa = g_eax, sb = g_ebx, sc = g_ecx, sd = g_edx, si = g_esi, di = g_edi, bp = g_ebp;
            (void)sa;
            PUSH32(g_esp, 0);
            PUSH32(g_esp, 0);
            PUSH32(g_esp, 1);
            PUSH32(g_esp, 0x3Fu | (p << 15));   /* barajar */
            PUSH32(g_esp, 0x000B171Cu);
            sub_00081150();
            g_esp += 16;
            g_ebx = sb; g_ecx = sc; g_edx = sd; g_esi = si; g_edi = di; g_ebp = bp;
            g_eax &= 0xFFFF0000u;           /* 0 = termino */
        }
    } else if (r == 0)
        fprintf(stderr, "[CARTAS] Cerezas Naturia (jugador %u): fin (estado 0x%X)\n", p, st);
}

/* ---- vidente --------------------------------------------------------------
 * "VOLTEO: Roba 2 cartas, y despues devuelve 1 carta de tu mano al Deck y
 * barajalo." El disparo y la entrada son los de Skelengel (VOLTEO, resolver
 * sub_000ADDE0, alias); el resolver es propio, con los pasos de Sociedad
 * benefica garbosa (sub_000AD620) y de El centinela energico (sub_000B0110):
 * estado en 0x66BFBC (& 0xFF), devuelve el siguiente:
 *   0x80: robar 2, sub_000C8EF0(p, 2) (la Caridad roba 3 asi);
 *   0x7F: pedir elegir 1 carta de una mano, sub_00084D00(p, 6, 0, 0) (modo 6,
 *         el del Centinela: lo atiende sub_00090AA0, envuelta abajo para que
 *         sea la mano propia); lo elegido queda en u16 0x65F930;
 *   0x7E: esa carta de la mano al Deck, sub_000C6D90(p, indice, 1), y barajar
 *         (orden 0x3F, 0x8000 si es la CPU), como el Centinela; fin (0x64).
 * ctx +2 bit 0 = el jugador; +4 bit 2 = anulada. */
void sub_000C8EF0(void);
void sub_00084D00(void);
void sub_000C6D90(void);
void sub_00090AA0_pad(void);

static void vidente(uint32_t ctx)
{
    uint32_t sb = g_ebx, si = g_esi, di = g_edi, bp = g_ebp;
    uint32_t p = MEM8(ctx + 2) & 1u, st = MEM32(0x0066BFBCu) & 0xFFu, r = 0;
    if (MEM8(ctx + 4) & 4u) {
        fprintf(stderr, "[CARTAS] Vidente: anulada\n");
    } else if (st == 0x80u) {
        PUSH32(g_esp, 2u);
        PUSH32(g_esp, p);
        PUSH32(g_esp, 0x000AD650u);
        sub_000C8EF0();
        g_esp += 8;
        fprintf(stderr, "[CARTAS] Vidente (jugador %u): roba 2\n", p);
        r = 0x7Fu;
    } else if (st == 0x7Fu) {
        if (MEM8(0x0065D0A8u + p * 0x13F8u) == 0) {
            fprintf(stderr, "[CARTAS] Vidente: sin cartas en la mano\n");
        } else {
            s_vid_sel = 1;
            PUSH32(g_esp, 0);
            PUSH32(g_esp, 0);
            PUSH32(g_esp, 6u);
            PUSH32(g_esp, p);
            PUSH32(g_esp, 0x000B0146u);
            sub_00084D00();
            g_esp += 16;
            r = 0x7Eu;
        }
    } else if (st == 0x7Eu) {
        uint32_t i = MEM16(0x0065F930u);
        s_vid_sel = 0;
        PUSH32(g_esp, 1u);
        PUSH32(g_esp, i);
        PUSH32(g_esp, p);
        PUSH32(g_esp, 0x000B016Eu);
        sub_000C6D90();
        g_esp += 12;
        PUSH32(g_esp, 0);
        PUSH32(g_esp, 0);
        PUSH32(g_esp, 1u);
        PUSH32(g_esp, 0x3Fu | (p ? 0x8000u : 0u));
        PUSH32(g_esp, 0x000B0196u);
        sub_00081150();
        g_esp += 16;
        fprintf(stderr, "[CARTAS] Vidente (jugador %u): la carta %u de la mano vuelve al Deck, barajado\n", p, i);
        r = 0x64u;
    }
    g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
    ret16(r);
}

/* El pedido modo 6 (arg: el jugador que elige). El juego muestra la mano del
 * adversario (Centinela, Confiscacion, Intercambio); para Vidente, la propia:
 *   la CPU: la ultima carta de su mano (la recien robada);
 *   el jugador: el texto propio y el cursor del campo (sub_00092E20) con la
 *   mascara 0x1 (su mano; el juego usa 0x10000, la del adversario), sin la
 *   orden 0x8098 (la que da vuelta la mano del adversario). Paso en u8
 *   0x65F92E; lo elegido (0x6633DC) en u16 0x65F930. Devuelve 1 al terminar. */
void sub_00090AA0(void)
{
    uint32_t p = MEM32(g_esp + 4) & 1u, sb, si, di, bp, r = 0;
    if (!s_vid_sel) {
        sub_00090AA0_pad();
        return;
    }
    sb = g_ebx; si = g_esi; di = g_edi; bp = g_ebp;
    if (p) {
        uint32_t n = MEM8(0x0065D0A8u + 0x13F8u);
        MEM16(0x0065F930u) = (uint16_t)(n ? n - 1u : 0u);
        r = 1;
    } else if (MEM8(0x0065F92Eu) == 0) {
        PUSH32(g_esp, s_txt_vidente);
        PUSH32(g_esp, 0xBu);
        PUSH32(g_esp, 0x712u);
        PUSH32(g_esp, 0x206u);
        PUSH32(g_esp, 0x00090BA0u);
        sub_00087E20();
        g_esp += 16;
        MEM8(0x0065F92Eu) = 1;
    } else if (MEM8(0x0065F92Eu) == 1) {
        MEM32(0x0065E4ACu) |= 1u;
        PUSH32(g_esp, 0x1u);
        PUSH32(g_esp, 0x00090B39u);
        sub_00092E20();
        g_esp += 4;
        if (g_eax & 0xFFFFu) {
            PUSH32(g_esp, 0x10000002u);
            PUSH32(g_esp, 0x00090B4Bu);
            sub_0007E280();
            g_esp += 4;
            MEM8(0x0065F92Eu) = 2;
        }
    } else {
        MEM32(0x0065E4ACu) &= ~1u;
        MEM16(0x0065F930u) = (uint16_t)MEM16(0x006633DCu);
        r = 1;
    }
    g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
    ret16(r);
}

/* ---- Carga del Poderoso ------------------------------------------------- */

/* El calculo de ATK/DEF de una carta del campo (lo usan la batalla, el
 * tablero y la CPU; Estandarte del Valor suma ahi): arg1 0x65D0A4, arg2 bits
 * 0-7 jugador y 8-15 casilla, arg3 la salida (+0 u16 k, +4 ATK, +8 DEF),
 * arg4 que calcular (bit 0 ATK). Con Carga del Poderoso boca arriba y sin
 * negar en el campo del otro jugador (como cuenta el juego a Estandarte,
 * sub_000D32A0: casillas 0..10, +0xA bit 1 boca arriba, +0x96 bit 5
 * negada), el monstruo boca arriba pierde 100 x su Nivel por cada una, sin
 * bajar de 0. */
static uint32_t contar_carga(uint32_t p)
{
    uint32_t z, n = 0, c;
    for (z = 0; z <= 10u; ++z) {
        uint32_t s = (p & 1u) * 0x13F8u + z * 0x9Cu + 0x0065D0D2u;
        c = carta_en(p, z);
        if (c && (MEM8(s) & 2u) && !(MEM8(s + 0x8Cu) & 0x20u) && MEM16(T_IMAGEN + 2u * c) == s_id_carga)
            ++n;
    }
    return n;
}

void sub_000D5DD0_pad(void);
void sub_000D5DD0(void)
{
    uint32_t a = MEM32(g_esp + 8), out = MEM32(g_esp + 12), que = MEM32(g_esp + 16);
    sub_000D5DD0_pad();
    if (s_id_carga && (que & 1u)) {
        uint32_t p = a & 1u, z = (a >> 8) & 0xFFu, k = MEM16(out), n, baja;
        int32_t atk;
        if (z < 5u && k && k < NCARTAS && (MEM8((p * 0x13F8u + z * 0x9Cu) + 0x0065D0D2u) & 2u)
            && (n = contar_carga(p ^ 1u)) != 0) {
            baja = n * 100u * ((MEM32(T_DATOS + 4u * k) >> 25) & 15u);
            atk = (int32_t)MEM32(out + 4);
            MEM32(out + 4) = (uint32_t)(atk > (int32_t)baja ? atk - (int32_t)baja : 0);
        }
    }
}

/* ---- Drillago ------------------------------------------------------------ */

/* sub_00099980(jugador, casilla): 1 (ax) si ese monstruo puede atacar
 * directo (switch por la imagen: Lampara mistica, Jinzo #7...; lo usan el
 * jugador y la CPU). Drillago (texto oficial): "si las unicas cartas que
 * controla tu adversario son monstruos boca arriba con 1600 ATK o mas":
 * casillas 5..10 del adversario vacias y cada monstruo boca arriba (+0xA
 * bit 1) con ATK (sub_000D8130) >= 1600. Sin cartas, el juego ya deja atacar directo. */
static int drillago_puede(uint32_t yo)
{
    uint32_t q = yo ^ 1u, z;
    for (z = 0; z <= 10u; ++z) {
        uint32_t c = carta_en(q, z), atk;
        if (!c)
            continue;
        if (z >= 5u || !(MEM8(q * 0x13F8u + z * 0x9Cu + 0x0065D0D2u) & 2u))
            return 0;
        PUSH32(g_esp, z);
        PUSH32(g_esp, q);
        PUSH32(g_esp, 0x00099987u);
        sub_000D8130();
        g_esp += 8;
        atk = g_eax;
        if ((int32_t)atk < 1600)
            return 0;
    }
    return 1;
}

void sub_00099980_pad(void);
void sub_00099980(void)
{
    bls_segundo();
    uint32_t p = MEM32(g_esp + 4) & 1u, z = MEM32(g_esp + 8);
    if (s_k_drillago > 0 && z < 5u && carta_en(p, z) == (uint32_t)s_k_drillago) {
        uint32_t sb = g_ebx, si = g_esi, di = g_edi, bp = g_ebp;
        int ok = drillago_puede(p);
        g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
        if (ok) {
            static int said;
            if (said < 20) {
                ++said;
                fprintf(stderr, "[CARTAS] Drillago (jugador %u, casilla %u): puede atacar directo\n", p, z);
                fflush(stderr);
            }
            g_eax = (g_eax & 0xFFFF0000u) | 1u;
            g_esp += 4;                 /* ret */
            return;
        }
    }
    sub_00099980_pad();
}

/* ---- Goblin Entrenador -------------------------------------------------- */

/* "Mientras esta carta este boca arriba en tu lado del Campo, durante tu End
 * Phase puedes devolver 1 monstruo normal de tu mano a la parte inferior de tu
 * Deck para robar 1 carta. Solo puedes usar este efecto una vez por turno."
 *
 * El disparo: el de Garuda, el Espiritu del Viento (5255), en los pasos de la
 * End Phase, sub_0009EC80 (paso en u8 0x65F8EC; esi = (0x65F8DA >> 1) & 1 =
 * el jugador del turno, como en Estandarte del Valor). Garuda actua "en la End
 * Phase del turno de tu adversario": en el paso 5 mira al otro (edi = 1 - esi),
 * pregunta y en el 6 registra el disparo:
 * sub_00081EF0((casilla | 0x840 | duenio << 15) << 16 | k). Aca, en el paso 5
 * (una vez por End Phase), lo mismo con el Goblin del jugador del turno (esi) si esta boca arriba y sin
 * negar en una casilla de monstruo y hay un monstruo normal en la mano.
 * La entrada de efecto: la de Garuda (alias en la busqueda, sub_000C4DE0), su
 * resolver sub_000B6DA0 envuelto (estado 0x66BFBC, empieza en 0x80):
 *   0x80 la CPU: el monstruo normal de menos ATK de su mano al fondo -> 0x7D;
 *        el jugador: Si/No (sub_000880B0, texto 0xC55 del juego: "Deseas
 *        devolver una carta de tu mano a la parte inferior de tu Baraja?",
 *        el de la carta 4503, resolver sub_000AB030) -> 0x7F
 *   0x7F No: fin; "Selecciona una carta de tu mano..." (0xC56) -> 0x7E
 *   0x7E el cursor en la propia mano, sub_00092E20(1) (lo elegido en
 *        0x6633DC); si no es monstruo normal, el mensaje otra vez; si lo es,
 *        sub_000C6D90(p, indice, 0): a la parte inferior del Deck -> 0x7D
 *   0x7D robar 1, sub_000C8EF0(p, 1) -> 0x7C -> fin.
 * La mano: u8 0x65D0A8 + p * 0x13F8 cuantas; la carta i en u32 0x65D77C +
 * p * 0x13F8 + 8i (bits 0-14 = k). Monstruo normal: clase 0 y tipo < 21. */
void sub_00081EF0(void);
void sub_0002C5C0(void);
void sub_0009EC80_pad(void);
void sub_000B6DA0_pad(void);

static int es_normal(uint32_t k)
{
    uint32_t d;
    if (!k || k >= NCARTAS)
        return 0;
    d = MEM32(T_DATOS + 4u * k);
    return ((d >> 18) & 3u) == 0u && ((d >> 20) & 0x1Fu) < 21u;
}

static uint32_t mano_k(uint32_t p, uint32_t i)
{
    return MEM32(0x0065D77Cu + (p & 1u) * 0x13F8u + 8u * i) & 0x7FFFu;
}

/* el indice del monstruo normal de menos ATK de la mano, o -1 */
static int mano_normal(uint32_t p)
{
    uint32_t n = MEM8(0x0065D0A8u + (p & 1u) * 0x13F8u), i, best_atk = 0xFFFFFFFFu;
    int best = -1;
    for (i = 0; i < n && i < 64u; ++i) {
        uint32_t k = mano_k(p, i), atk;
        if (!es_normal(k))
            continue;
        atk = (MEM32(T_DATOS + 4u * k) >> 9) & 511u;
        if (atk < best_atk) {
            best_atk = atk;
            best = (int)i;
        }
    }
    return best;
}

static uint32_t texto_juego(uint32_t n)
{
    PUSH32(g_esp, n);
    PUSH32(g_esp, 0x000AB07Cu);
    sub_0002C5C0();
    g_esp += 4;
    return g_eax;
}

void sub_0009EC80(void)
{
    static int hecho;
    uint32_t paso = MEM8(0x0065F8ECu);
    isaac_cementerio();
    if (paso == 0u) {
        hecho = 0;
        s_bls_usado = 0;
        memset(s_abare_usado, 0, sizeof s_abare_usado);
        s_bls_ef1 = 0;
        s_bls_ataco = 0;
        memset(s_fort_usado, 0, sizeof s_fort_usado);
        s_isaac.activo = s_isaac.hecho = 0;
        s_isaac_redibujar = 0;
        memset(s_real, 0, sizeof s_real);
    }
    if (paso == 5u && !hecho && s_k_goblin > 0) {
        uint32_t yo = (MEM8(0x0065F8DAu) >> 1) & 1u, z;   /* el del turno (Garuda mira al otro) */
        hecho = 1;
        for (z = 0; z < 5u; ++z) {
            uint32_t s = yo * 0x13F8u + z * 0x9Cu + 0x0065D0D2u;
            if (carta_en(yo, z) != (uint32_t)s_k_goblin || !(MEM8(s) & 2u) || (MEM8(s + 0x8Cu) & 0x20u))
                continue;
            if (mano_normal(yo) < 0) {
                fprintf(stderr, "[CARTAS] Goblin Entrenador (jugador %u): sin monstruos normales en la mano\n", yo);
                break;
            }
            {
                uint32_t sb = g_ebx, si = g_esi, di = g_edi, bp = g_ebp;
                PUSH32(g_esp, ((z | 0x840u | (yo << 15)) << 16) | (uint32_t)s_k_goblin);
                PUSH32(g_esp, 0x0009F174u);
                sub_00081EF0();
                g_esp += 4;
                g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
            }
            fprintf(stderr, "[CARTAS] Goblin Entrenador (jugador %u, casilla %u): disparo de la End Phase\n", yo, z);
            break;
        }
        fflush(stderr);
    }
    sub_0009EC80_pad();
}

void sub_000B6DA0(void)
{
    uint32_t ctx = MEM32(g_esp + 4);
    uint32_t sb, sc, sd, si, di, bp, p, st, ret = 0;
    if (s_k_cosmo > 0 && cosmo_eslabon() >= 0 && cosmo_resolver((uint32_t)cosmo_eslabon()))
        return;
    if (!es_k(ctx, s_k_goblin)) {
        sub_000B6DA0_pad();
        return;
    }
    sb = g_ebx; sc = g_ecx; sd = g_edx; si = g_esi; di = g_edi; bp = g_ebp;
    p = MEM8(ctx + 2) & 1u;
    st = MEM8(0x0066BFBCu);
    if (MEM8(ctx + 4) & 4u) {
        fprintf(stderr, "[CARTAS] Goblin Entrenador: anulado\n");
        st = 0;
    }
    switch (st) {
    case 0x80: {
        int i = mano_normal(p);
        if (i < 0) {
            fprintf(stderr, "[CARTAS] Goblin Entrenador (jugador %u): sin monstruos normales en la mano\n", p);
            break;
        }
        if (p) {
            PUSH32(g_esp, 0u);
            PUSH32(g_esp, (uint32_t)i);
            PUSH32(g_esp, p);
            PUSH32(g_esp, 0x000AB0DFu);
            sub_000C6D90();
            g_esp += 12;
            fprintf(stderr, "[CARTAS] Goblin Entrenador (CPU): la carta %d de la mano (k %u) al fondo del Deck\n",
                    i, mano_k(p, (uint32_t)i));
            ret = 0x7Du;
            break;
        }
        {
            uint32_t t = texto_juego(0xC55u);
            PUSH32(g_esp, t);
            PUSH32(g_esp, 0x000AB082u);
            sub_000880B0();
            g_esp += 4;
        }
        ret = 0x7Fu;
        break;
    }
    case 0x7F:
        if (!MEM16(0x00660B8Cu)) {
            fprintf(stderr, "[CARTAS] Goblin Entrenador: el jugador %u dijo que no\n", p);
            break;
        }
        {
            /* el texto propio: "monstruo" en vez de "carta" (0xC56 si falla) */
            static const char *const msj[6] = {
                "Select a monster in your hand.",
                "Select a monster in your hand.",
                "Wähle ein Monster in deiner Hand.",
                "Sélectionnez un monstre dans votre main.",
                "Seleziona un mostro nella tua mano.",
                "Selecciona un monstruo de tu mano.",
            };
            static uint32_t g_msj[6];
            uint32_t lg = MEM32(0x5FC2C0u), t;
            if (lg > 5u)
                lg = 5u;
            if (!g_msj[lg])
                g_msj[lg] = texto_guest(msj[lg]);
            t = g_msj[lg] ? g_msj[lg] : texto_juego(0xC56u);
            PUSH32(g_esp, t);
            PUSH32(g_esp, 0xBu);
            PUSH32(g_esp, 0x712u);
            PUSH32(g_esp, 0x206u);
            PUSH32(g_esp, 0x000AB0B1u);
            sub_00087E20();
            g_esp += 16;
        }
        ret = 0x7Eu;
        break;
    case 0x7E: {
        uint32_t i, k;
        PUSH32(g_esp, 1u);
        PUSH32(g_esp, 0x000AB0C1u);
        sub_00092E20();
        g_esp += 4;
        if (!(g_eax & 0xFFFFu)) {
            ret = 0x7Eu;                /* todavia eligiendo */
            break;
        }
        i = MEM32(0x006633DCu);
        k = mano_k(p, i);
        if (!es_normal(k)) {
            fprintf(stderr, "[CARTAS] Goblin Entrenador: la carta %u de la mano (k %u) no es un monstruo normal\n", i, k);
            ret = 0x7Fu;                /* el mensaje otra vez (la respuesta sigue en Si) */
            break;
        }
        PUSH32(g_esp, 0u);
        PUSH32(g_esp, i);
        PUSH32(g_esp, p);
        PUSH32(g_esp, 0x000AB0DFu);
        sub_000C6D90();
        g_esp += 12;
        fprintf(stderr, "[CARTAS] Goblin Entrenador (jugador %u): la carta %u de la mano (k %u) al fondo del Deck\n",
                p, i, k);
        ret = 0x7Du;
        break;
    }
    case 0x7D:
        PUSH32(g_esp, 1u);
        PUSH32(g_esp, p);
        PUSH32(g_esp, 0x000AD650u);
        sub_000C8EF0();
        g_esp += 8;
        fprintf(stderr, "[CARTAS] Goblin Entrenador (jugador %u): roba 1\n", p);
        ret = 0x7Cu;
        break;
    default:
        break;
    }
    fflush(stderr);
    g_ebx = sb; g_ecx = sc; g_edx = sd; g_esi = si; g_edi = di; g_ebp = bp;
    g_eax = (g_eax & 0xFFFF0000u) | ret;
    g_esp += 4;                         /* ret */
}

/* ---- Soldado del Brillo Negro - Enviado del Principio ------------------- */

/* "No puede ser Invocada de Modo Normal ni Colocada. Debe ser Invocada de Modo
 * Especial (desde tu mano) desterrando 1 monstruo de LUZ y 1 de OSCURIDAD de
 * tu Cementerio. Una vez por turno, puedes activar 1 de estos efectos:
 *  - Selecciona 1 monstruo en el Campo; destierralo. Esta carta no puede
 *    atacar durante el turno en que activas este efecto.
 *  - Si esta carta atacante destruye un monstruo de tu adversario en batalla:
 *    puede hacer un segundo ataque seguido."
 *
 * La invocacion: la de Alma de pureza y luz (0x1483, "desterrando 2 LUZ"),
 * con la carta pasando por Alma (0x5FE692[k]) mientras corren:
 *   sub_00093A30 (se puede Invocar de Modo Especial desde la mano? switch por
 *   la imagen: los Espiritus van a sub_000937E0(p, k), el Cementerio);
 *   sub_000968D0 (la maquina de la Invocacion, paso u8 0x65F8FD; Alma: 0x3C
 *   mensaje, 0x3D lista del Cementerio (sub_000C5AE0 -> sub_000C1B80 con la
 *   imagen), 0x3E destierra lo elegido (sub_000C7040), 0x3F mensaje, 0x40
 *   lista, 0x41 destierra, 0x49 la invoca).
 * Propios: sub_000937E0 (hay al menos 1 LUZ y 1 OSCURIDAD en el Cementerio),
 * sub_000D28B0 (solo de Modo Especial: 1), y la lista (envoltorio de
 * sub_000C1B80): los monstruos LUZ/OSCURIDAD del Cementerio (u8 0x65D0AA +
 * p * 0x13F8 cuantos; entradas de 8 bytes en 0x65DC7C + p * 0x13F8), en la
 * segunda sin el atributo del primero (anotado en el paso 0x3E).
 *
 * Efecto 1: la entrada de Barrel Dragon (alias; su condicion sub_000A5870 =
 * la marca 0x2000 de "una vez por turno" del repaso del turno, alias "turno"),
 * con las casillas validas de Come Hombres (cualquier monstruo; envoltorio de
 * sub_000BAFE0) y el resolver de Barrel envuelto (sub_000B3510): el objetivo
 * en ctx +0xC (jugador) / +0xD (casilla); sub_000A1F20 y sub_000C69A0(p, z,
 * 0) como Prision dimensional; y el Soldado queda como "ya ataco" (casilla
 * +0xB bit 2 y bit z de u16 0x65D0C4 + p * 0x13F8, lo que marca la
 * declaracion de ataque sub_0009A290). La condicion no deja usarlo si ya
 * ataco o ya uso un efecto este turno.
 *
 * Efecto 2: en la declaracion de ataque (sub_0009A290; 0x6666E0 bits 10-12
 * atacante, 13-15 atacado, bit 0 directo) se anota el ataque del Soldado.
 * Despues (antes de los comandos de una carta, sub_00098EF0, y de "puede
 * atacar directo", sub_00099980), si el monstruo atacado ya no esta y el
 * Soldado sigue: se le quita la marca de "ya ataco" (lo que hace el juego con
 * el Caballero Hayabusa), una vez por turno (se limpia en la End Phase). */
void sub_00093A30_pad(void);
void sub_000968D0_pad(void);
void sub_000937E0_pad(void);
void sub_000D28B0_pad(void);
void sub_000A5870_pad(void);
void sub_000B3510_pad(void);

static uint32_t s_bls_attr1;            /* atributo del primero desterrado (1 LUZ, 2 OSCURIDAD) */
static struct { uint32_t activo, p, za, zd, kdef; } s_bls_at;

static uint32_t atributo(uint32_t k)
{
    return k && k < NCARTAS ? (MEM32(T_DATOS + 4u * k) >> 29) & 7u : 0u;
}

static int es_monstruo(uint32_t k)
{
    return k && k < NCARTAS && ((MEM32(T_DATOS + 4u * k) >> 20) & 0x1Fu) < 21u;
}

static void como_alma(void (*f)(void))
{
    uint16_t prev;
    if (s_k_bls <= 0) {
        f();
        return;
    }
    prev = (uint16_t)MEM16(T_IMAGEN + 2u * (uint32_t)s_k_bls);
    MEM16(T_IMAGEN + 2u * (uint32_t)s_k_bls) = (uint16_t)ID_ALMA;
    f();
    MEM16(T_IMAGEN + 2u * (uint32_t)s_k_bls) = prev;
}

void sub_00093A30(void) { como_alma(sub_00093A30_pad); }

void sub_000968D0(void)
{
    uint32_t st = MEM8(0x0065F8FDu);
    if (s_k_bls <= 0 || MEM16(0x0065F8F4u) != (uint32_t)s_k_bls) {
        sub_000968D0_pad();
        return;
    }
    if (st == 0u || st == 0x3Cu)
        s_bls_attr1 = 0;
    if (st == 0x3Eu) {
        uint32_t i = MEM16(0x00658556u) + (MEM8(0x00658555u) & 3u);
        s_bls_attr1 = atributo(MEM32(0x0065855Cu + 8u * i) & 0x7FFFu);
        fprintf(stderr, "[CARTAS] Soldado del Brillo Negro: primer desterrado de atributo %u\n", s_bls_attr1);
        fflush(stderr);
    }
    s_bls_lista = 1;
    como_alma(sub_000968D0_pad);
    s_bls_lista = 0;
}

/* Los textos del juego que pide sub_0002C5C0 (desde voces.c): mientras se
 * invoca el Soldado, el de Alma de pureza y luz (0xD0C "Selecciona del
 * Cementerio %d monstruo(s) %s...", 0xD0E "...un monstruo %s del
 * Cementerio...") pasa a decir 1 LUZ y 1 OSCURIDAD (o el que falta). Sin
 * '%': los argumentos de sobra no se usan. Idioma en 0x5FC2C0 (0 jp, 1 en,
 * 2 de, 3 fr, 4 it, 5 es). */
uint32_t ygo_cartas_texto(uint32_t n, uint32_t txt)
{
    static const char *const msj[3][6] = {
        { "Select from Graveyard 1 LIGHT and 1 DARK monster for removal from play.",
          "Select from Graveyard 1 LIGHT and 1 DARK monster for removal from play.",
          "Wähle aus dem Friedhof 1 LICHT- und 1 FINSTERNIS-Monster, um sie aus dem Spiel zu entfernen.",
          "Sélectionnez dans le Cimetière 1 monstre LUMIÈRE et 1 monstre TÉNÈBRES à retirer du jeu.",
          "Seleziona dal Cimitero 1 mostro LUCE e 1 mostro OSCURITÀ da rimuovere dal gioco.",
          "Selecciona del Cementerio 1 monstruo de LUZ y 1 de OSCURIDAD para retirarlos de la partida." },
        { "Select from Graveyard 1 DARK monster for removal from play.",
          "Select from Graveyard 1 DARK monster for removal from play.",
          "Wähle aus dem Friedhof 1 FINSTERNIS-Monster, um es aus dem Spiel zu entfernen.",
          "Sélectionnez dans le Cimetière 1 monstre TÉNÈBRES à retirer du jeu.",
          "Seleziona dal Cimitero 1 mostro OSCURITÀ da rimuovere dal gioco.",
          "Selecciona del Cementerio 1 monstruo de OSCURIDAD para retirarlo de la partida." },
        { "Select from Graveyard 1 LIGHT monster for removal from play.",
          "Select from Graveyard 1 LIGHT monster for removal from play.",
          "Wähle aus dem Friedhof 1 LICHT-Monster, um es aus dem Spiel zu entfernen.",
          "Sélectionnez dans le Cimetière 1 monstre LUMIÈRE à retirer du jeu.",
          "Seleziona dal Cimitero 1 mostro LUCE da rimuovere dal gioco.",
          "Selecciona del Cementerio 1 monstruo de LUZ para retirarlo de la partida." },
    };
    static uint32_t g_msj[3][6];
    uint32_t lg, cual;
    if (!s_bls_lista || (n != 0xD0Cu && n != 0xD0Eu))
        return txt;
    lg = MEM32(0x5FC2C0u);
    if (lg > 5u)
        lg = 5u;
    cual = s_bls_attr1 == 1u ? 1u : s_bls_attr1 == 2u ? 2u : 0u;
    if (!g_msj[cual][lg])
        g_msj[cual][lg] = texto_guest(msj[cual][lg]);
    return g_msj[cual][lg] ? g_msj[cual][lg] : txt;
}

static void bls_lista(uint32_t p)
{
    uint32_t base = (p & 1u) * 0x13F8u, n = MEM8(0x0065D0AAu + base), i, m = 0;
    for (i = 0; i < n && m < 128u; ++i) {
        uint32_t e = MEM32(0x0065DC7Cu + base + 8u * i), e2 = MEM32(0x0065DC80u + base + 8u * i);
        uint32_t c = e & 0x7FFFu, a = atributo(c);
        if (!es_monstruo(c) || (a != 1u && a != 2u) || a == s_bls_attr1)
            continue;
        MEM32(0x0065855Cu + 8u * m) = e;
        MEM32(0x00658560u + 8u * m) = e2;
        MEM8(0x0065895Cu + m) = 4;
        ++m;
    }
    MEM16(0x006589DCu) = (uint16_t)m;
    g_eax = m;
}

/* (p, k): hay 1 LUZ y 1 OSCURIDAD en el Cementerio de p? */
void sub_000937E0(void)
{
    uint32_t p = MEM32(g_esp + 4) & 1u, k = MEM32(g_esp + 8) & 0xFFFFu;
    uint32_t base = p * 0x13F8u, n, i, luz = 0, osc = 0;
    if (s_k_bls <= 0 || k != (uint32_t)s_k_bls) {
        sub_000937E0_pad();
        return;
    }
    n = MEM8(0x0065D0AAu + base);
    for (i = 0; i < n; ++i) {
        uint32_t c = MEM32(0x0065DC7Cu + base + 8u * i) & 0x7FFFu;
        if (!es_monstruo(c))
            continue;
        luz += atributo(c) == 1u;
        osc += atributo(c) == 2u;
    }
    ret16(luz && osc ? 1u : 0u);
}

/* (k): no se puede Invocar de Modo Normal? */
void sub_000D28B0(void)
{
    if (s_k_bls > 0 && (MEM32(g_esp + 4) & 0xFFFFu) == (uint32_t)s_k_bls) {
        ret16(1);
        return;
    }
    sub_000D28B0_pad();
}

/* la casilla del Soldado de p, o -1 */
static int bls_casilla(uint32_t p)
{
    uint32_t z;
    for (z = 0; z < 5u; ++z)
        if (carta_en(p, z) == (uint32_t)s_k_bls)
            return (int)z;
    return -1;
}

static void marcar_ataco(uint32_t p, uint32_t z, int si)
{
    uint32_t b = (p & 1u) * 0x13F8u;
    if (si) {
        MEM8(b + z * 0x9Cu + 0x0065D0D3u) |= 4u;
        MEM16(b + 0x0065D0C4u) |= (uint16_t)(1u << z);
    } else {
        MEM8(b + z * 0x9Cu + 0x0065D0D3u) &= (uint8_t)~4u;
        MEM16(b + 0x0065D0C4u) &= (uint16_t)~(1u << z);
    }
}

/* la condicion de Barrel Dragon (ctx, ?, 0): para el Soldado, ademas, que no
 * haya atacado ni usado un efecto este turno */
void sub_000A5870(void)
{
    uint32_t ctx = MEM32(g_esp + 4);
    if (s_k_bls > 0 && es_k(ctx, s_k_bls)) {
        uint32_t p = MEM8(ctx + 2) & 1u;
        int z = bls_casilla(p);
        if (s_bls_usado || s_bls_ataco || z < 0 || MEM16(g_esp + 0xC)) {
            ret16(0);
            return;
        }
        ret16(1);                       /* sin la bandera 0x2000: tambien el turno en que se invoca */
        return;
    }
    sub_000A5870_pad();
}

/* el resolver de Barrel Dragon: el Soldado destierra el objetivo */
void sub_000B3510(void)
{
    uint32_t ctx = MEM32(g_esp + 4);
    uint32_t sb, si, di, bp, p, z, yo;
    int zs;
    if (s_k_bls <= 0 || !es_k(ctx, s_k_bls)) {
        sub_000B3510_pad();
        return;
    }
    sb = g_ebx; si = g_esi; di = g_edi; bp = g_ebp;
    yo = MEM8(ctx + 2) & 1u;
    p = MEM8(ctx + 0xC) & 1u;
    z = MEM8(ctx + 0xD);
    if (MEM8(ctx + 4) & 4u)
        fprintf(stderr, "[CARTAS] Soldado del Brillo Negro: efecto anulado\n");
    else if (z < 5u && carta_en(p, z)) {
        PUSH32(g_esp, z);
        PUSH32(g_esp, p);
        PUSH32(g_esp, (uint32_t)s_k_bls);
        PUSH32(g_esp, 0x000B8439u);
        sub_000A1F20();
        g_esp += 12;
        if (g_eax & 0xFFFFu) {
            PUSH32(g_esp, 0);
            PUSH32(g_esp, z);
            PUSH32(g_esp, p);
            PUSH32(g_esp, 0x000B845Bu);
            sub_000C69A0();
            g_esp += 12;
            fprintf(stderr, "[CARTAS] Soldado del Brillo Negro (jugador %u): destierra el monstruo del jugador %u, casilla %u\n",
                    yo, p, z);
        } else
            fprintf(stderr, "[CARTAS] Soldado del Brillo Negro: el objetivo no puede ser afectado\n");
    } else
        fprintf(stderr, "[CARTAS] Soldado del Brillo Negro: sin objetivo (jugador %u, casilla %u)\n", p, z);
    zs = bls_casilla(yo);
    if (zs >= 0)
        marcar_ataco(yo, (uint32_t)zs, 1);     /* no puede atacar este turno */
    s_bls_usado = 1;
    s_bls_ef1 = 1;
    s_bls_at.activo = 0;
    fflush(stderr);
    g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
    g_eax &= 0xFFFF0000u;
    g_esp += 4;                         /* ret */
}

static void bls_ataque(uint32_t p, uint32_t at)
{
    uint32_t za = (at >> 10) & 7u, zd = (at >> 13) & 7u;
    if (s_k_bls > 0 && za < 5u && carta_en(p, za) == (uint32_t)s_k_bls)
        s_bls_ataco = 1;
    if (s_k_bls <= 0 || (at & 1u) || za >= 5u || zd >= 5u || carta_en(p, za) != (uint32_t)s_k_bls)
        return;
    s_bls_at.activo = 1;
    s_bls_at.p = p;
    s_bls_at.za = za;
    s_bls_at.zd = zd;
    s_bls_at.kdef = carta_en(p ^ 1u, zd);
    fprintf(stderr, "[CARTAS] Soldado del Brillo Negro (jugador %u, casilla %u) ataca la casilla %u (carta k %u)\n",
            p, za, zd, s_bls_at.kdef);
    fflush(stderr);
}

static void bls_segundo(void)
{
    isaac_cementerio();
    if (!s_bls_at.activo || s_k_bls <= 0)
        return;
    if (carta_en(s_bls_at.p, s_bls_at.za) != (uint32_t)s_k_bls) {
        s_bls_at.activo = 0;            /* el Soldado ya no esta */
        fprintf(stderr, "[CARTAS] Soldado del Brillo Negro: ya no esta en el Campo (jugador %u, casilla %u), sin segundo ataque\n",
                s_bls_at.p, s_bls_at.za);
        return;
    }
    if (s_bls_at.kdef && carta_en(s_bls_at.p ^ 1u, s_bls_at.zd) == s_bls_at.kdef)
        return;                         /* el atacado sigue ahi (o la batalla no termino) */
    s_bls_at.activo = 0;
    if (s_bls_usado) {
        fprintf(stderr, "[CARTAS] Soldado del Brillo Negro: destruyo al atacado, pero ya uso un efecto este turno\n");
        return;
    }
    s_bls_usado = 1;
    marcar_ataco(s_bls_at.p, s_bls_at.za, 0);
    fprintf(stderr, "[CARTAS] Soldado del Brillo Negro (jugador %u): destruyo al atacado, puede atacar otra vez\n",
            s_bls_at.p);
    fflush(stderr);
}

/* sub_00099BC0(p, z): puede atacar el monstruo de (p, z)? (boca arriba, sin
 * la marca de ataque en u16 0x65D0C4...). Para el Soldado: antes, el segundo
 * ataque (bls_segundo); despues, no si uso el efecto 1 este turno. */
void sub_00099BC0_pad(void);
void sub_00099BC0(void)
{
    uint32_t p = MEM32(g_esp + 4) & 1u, z = MEM32(g_esp + 8);
    bls_segundo();
    sub_00099BC0_pad();
    if (s_bls_ef1 && s_k_bls > 0 && z < 5u && carta_en(p, z) == (uint32_t)s_k_bls)
        g_eax &= 0xFFFF0000u;
}

/* sub_00099940(p, z): marca que (p, z) ataco; el ataque del jugador pasa por
 * aca (sub_0009A7B0), con 0x6666E0 ya puesto. Para el efecto 2 del Soldado. */
void sub_00099940_pad(void);
void sub_00099940(void)
{
    uint32_t p = MEM32(g_esp + 4) & 1u, z = MEM32(g_esp + 8);
    sub_00099940_pad();
    if (s_k_bls > 0 && z < 5u && carta_en(p, z) == (uint32_t)s_k_bls)
        bls_ataque(p, MEM32(0x006666E0u));
}

/* ---- fortaleza (Guerrero de la Fortaleza) -------------------------------- */
/* sub_0007F8D0(0x65D0A4, B = 0x6666E0, atacante, ?) arma el resultado de la
 * batalla (lo llaman sub_00080170 y este los pasos del combate; sub_0009B5B0
 * lo usa de vista previa y despues borra las marcas). B+0: el ataque (bit 0
 * directo, bits 10-12 casilla del atacante, 13-15 la del atacado). Una entrada
 * de 12 bytes por jugador en B+6+12p: +0 u16 casilla (bits 0-2) y bit 3
 * "destruido", +2 k, +0xA el dano de batalla para ese jugador. Al final el
 * juego le borra el bit 3 a Parca de Espiritu (0x1596) y a Parca en la
 * Pesadilla. El Guerrero: su duenio no recibe dano de batalla y, una vez por
 * turno, no se destruye (la clave del ataque que lo salvo, para las llamadas
 * repetidas del mismo ataque; se limpia en el paso 0 de sub_0009EC80). */
/* ---- isaac (Isaac, Caballero Divino) -------------------------------------- */
/* "Cualquier monstruo que esta carta destruya en batalla se retira del juego".
 * En el resultado de la batalla (sub_00080170, desde los pasos del combate
 * sub_0009B5B0 y sub_0009BA10, los dos de la tabla de pasos; la anotacion se
 * borra al declarar otro ataque, sub_0009A290), si un lado es Isaac y el otro queda marcado destruido
 * (bit 3), se anota. Cuando esa carta se destruye
 * (sub_000C8620, lo que usa la batalla para el atacado) o va del Campo al
 * Cementerio (sub_000C6960, el final de la batalla sub_0009D350), se destierra en su lugar con sub_000C69A0(p, z, 0), como
 * Ciclon. (sub_000C7C50 recibe una copia en 0x666700, no la casilla.) */
static void isaac_batalla(uint32_t b, uint32_t at)
{
    uint32_t p;
    if (s_k_isaac < 0 || (MEM16(b) & 1u))
        return;
    for (p = 0; p < 2u; ++p) {
        uint32_t e = b + 6u + 12u * p, z = MEM16(e) & 7u;
        uint32_t o = b + 6u + 12u * (p ^ 1u), zo = MEM16(o) & 7u, ko = MEM16(o + 2);
        if (z >= 5u || carta_en(p, z) != (uint32_t)s_k_isaac)
            continue;
        if (zo >= 5u || !(MEM16(o) & 8u) || carta_en(p ^ 1u, zo) != ko)
            continue;
        if (!s_isaac.activo || s_isaac.p != (p ^ 1u) || s_isaac.z != zo || s_isaac.k != ko)
            fprintf(stderr, "[CARTAS] Isaac (jugador %u, casilla %u) destruye en batalla la carta k %u "
                    "(jugador %u, casilla %u): se destierra\n", p, z, ko, p ^ 1u, zo);
        s_isaac.activo = 1;
        s_isaac.hecho = 0;
        s_isaac.p = p ^ 1u;
        s_isaac.z = zo;
        s_isaac.k = ko;
    }
    (void)at;
}

/* ---- caballero_real (Caballero Real) -------------------------------------- */
/* "Cuando esta carta destruye un monstruo como resultado de una batalla y lo
 * manda al Cementerio, aumenta tus Puntos de Vida en la DEF del monstruo
 * destruido". No hay carta del juego que lo haga: se arma con dos caminos que
 * ya funcionan. El de Isaac para saber a quien destruyo (el resultado de la
 * batalla, sub_0007F8D0: el otro lado con el bit 3) y el momento en que el
 * destruido va al Cementerio por batalla (sub_000C7C50, la que dispara los
 * "va al Cementerio" como Momonga; si lo destierra Isaac/Macrocosmos no pasa).
 * Los LP con sub_000C8DF0(jugador, n), como Escudo drenador / Medicina roja.
 * La DEF: la de la carta (u32 0x5FEF78, bits 0-8 x10), la que tiene en el
 * Cementerio. */
static void real_batalla(uint32_t b)
{
    uint32_t p;
    if (s_k_real < 0 || (MEM16(b) & 1u))
        return;
    for (p = 0; p < 2u; ++p) {
        uint32_t e = b + 6u + 12u * p, z = MEM16(e) & 7u;
        uint32_t o = b + 6u + 12u * (p ^ 1u), zo = MEM16(o) & 7u, ko = MEM16(o + 2);
        if (z >= 5u || carta_en(p, z) != (uint32_t)s_k_real)
            continue;
        if (zo >= 5u || !(MEM16(o) & 8u) || carta_en(p ^ 1u, zo) != ko)
            continue;
        if (!s_real[p ^ 1u].activo || s_real[p ^ 1u].k != ko)
            fprintf(stderr, "[CARTAS] Caballero Real (jugador %u, casilla %u) destruye en batalla la carta k %u "
                    "(jugador %u, casilla %u)\n", p, z, ko, p ^ 1u, zo);
        s_real[p ^ 1u].activo = 1;
        s_real[p ^ 1u].k = ko;
        s_real[p ^ 1u].yo = p;
    }
}

static void real_cementerio(uint32_t p, uint32_t pe)
{
    uint32_t k, def, yo;
    if (!s_real[p].activo || !pe || (MEM32(pe) & 0x7FFFu) != s_real[p].k)
        return;
    s_real[p].activo = 0;
    k = s_real[p].k;
    yo = s_real[p].yo;
    def = (MEM32(T_DATOS + 4u * k) & 511u) * 10u;
    fprintf(stderr, "[CARTAS] Caballero Real: la carta k %u (jugador %u) va al Cementerio; el jugador %u gana %u LP\n",
            k, p, yo, def);
    fflush(stderr);
    if (def) {
        uint32_t sa = g_eax, sb = g_ebx, sc = g_ecx, sd = g_edx, si = g_esi, di = g_edi, bp = g_ebp;
        PUSH32(g_esp, def);
        PUSH32(g_esp, yo);
        PUSH32(g_esp, 0x000AA429u);
        sub_000C8DF0();
        g_esp += 8;
        g_eax = sa; g_ebx = sb; g_ecx = sc; g_edx = sd; g_esi = si; g_edi = di; g_ebp = bp;
    }
}

/* ---- Isaac como Macrocosmos (2026-11-13 20:4x, leido en el codigo) --------
 * El juego ya trae "destruido -> desterrado en vez de al Cementerio" para
 * Macrocosmos (imagen 0x1332), preguntando sub_000D34B0(jugador, 0x1332):
 *   - orden 0x54 (sub_000CA130, la destruccion en batalla, la encola el combate
 *     en 0x9BDED): paso 0 arma la animacion de la carta a la pila; con
 *     Macrocosmos el destino es 0x1E (desterrados), si no 0x1C (Cementerio);
 *     paso 2 vacia la casilla.
 *   - sub_000C7C50 (fin de batalla, 0x9D772 / 0x9DCC4): con Macrocosmos salta a
 *     0xC80C2 y encola 0x5A (sub_000CA450 -> sub_000D4B70: lista de desterrados
 *     0x65E17C); si no, 0x59 (sub_000CA310 -> sub_000D4B10: Cementerio 0x65DC7C)
 *     y los disparos de "va al Cementerio".
 * Para la carta que Isaac destruye (anotada por isaac_batalla), solo dentro de
 * esas dos llamadas, sub_000D34B0(x, 0x1332) responde 1: el juego hace su
 * destierro con su animacion y su pila. */
static int s_isaac_macro;               /* sub_000D34B0(x, 0x1332) da 1 */
static int isaac_al_cementerio(uint32_t p, uint32_t z, uint32_t pe)
{
    (void)z;
    if (!s_isaac.activo || p != s_isaac.p || !pe || (MEM32(pe) & 0x7FFFu) != s_isaac.k)
        return 0;
    s_isaac.activo = 0;
    s_isaac.hecho = 1;
    fprintf(stderr, "[CARTAS] Isaac: fin de batalla de la carta k %u (jugador %u): a desterrados (como Macrocosmos)\n",
            s_isaac.k, p);
    fflush(stderr);
    s_isaac_macro = 1;
    sub_000C7C50_pad();
    s_isaac_macro = 0;
    return 1;
}

void sub_000D34B0_pad(void);
void sub_000D34B0(void)
{
    if (s_isaac_macro && MEM32(g_esp + 8) == 0x1332u) {
        g_eax = 1;
        g_esp += 4;                     /* ret */
        return;
    }
    sub_000D34B0_pad();
}

/* ---- 2026-11-20 Ganga de arrancada: the stolen monster's picture stayed ----
 * on the opponent's side. CAUSE (05:33 log, "6C430 desde (1 1 1 ..) hasta
 * (0 1 0 ..)" for a target in column 2): order 0x5F (sub_000CB340) builds
 * the target's column in EDX and keeps it across an 8-bit "imul cl"
 * (AX = AL * CL). The recompiler lifted it as the 32-bit one-operand imul
 * (EDX:EAX = EAX * ECX), which zeroed EDX: every stolen monster was sent to
 * column 0; with a card there the game refused the move (sub_0006C430 -1)
 * and the picture stayed, the data moved anyway. Only a target in zone 0
 * worked (05:18). FIX: xboxrecomp-apex tools/recomp/lifter.py
 * _narrow_muldiv (8/16-bit mul/imul/div/idiv on AX / DX:AX only) and the 54
 * 8-bit imul already in gen/ rewritten the same way. Seen working in the
 * user's game at 05:43 ("74BE0 j1 tipo 1 col 0 -> j0 tipo 1 col 1: movida"). */
void sub_000CA130_pad(void);
void sub_000CA130(void)
{
    uint32_t p = (MEM16(0x0065F948u) >> 15) & 1u, z = MEM16(0x0065F94Au);
    if ((MEM32(0x00660154u) & 0x7Fu) == 0u && s_isaac.activo && p == s_isaac.p && z == s_isaac.z
            && carta_en(p, z) == s_isaac.k) {
        fprintf(stderr, "[CARTAS] Isaac: la carta k %u (jugador %u, casilla %u) sale del Campo hacia desterrados (orden 0x54)\n",
                s_isaac.k, p, z);
        fflush(stderr);
        s_isaac_macro = 1;
        sub_000CA130_pad();
        s_isaac_macro = 0;
        return;
    }
    sub_000CA130_pad();
}

/* La carta anotada por Isaac ya salio del Campo: si esta en el Cementerio, pasa
 * a los desterrados. Listas del jugador (base p*0x13F8, entradas de 8 bytes,
 * leidas en sub_000C5AE0): Cementerio 0x65DC7C con cuenta u8 0x65D0AA,
 * desterrados 0x65DEFC con cuenta u8 0x65D0AB (80 cada una). 2026-11-13: la
 * batalla saca al atacado por sub_000C7C50 sin pasar por C6960/C8620/C6390 con
 * la casilla llena, asi que se corrige despues, sin importar el camino. Se
 * llama desde bls_segundo (comprobaciones de ataque de la fase de batalla) y en
 * cada paso del turno (sub_0009EC80). */
static int s_isaac_redibujar;           /* redibujar las pilas hasta el paso 0 del turno */
static void isaac_redibujar(void)
{
    uint32_t sa = g_eax, sb = g_ebx, sc = g_ecx, sd = g_edx, si = g_esi, di = g_edi, bp = g_ebp;
    PUSH32(g_esp, 0x000CAD86u);
    sub_00088970();
    g_eax = sa; g_ebx = sb; g_ecx = sc; g_edx = sd; g_esi = si; g_edi = di; g_ebp = bp;
}

static void isaac_cementerio(void)
{
    uint32_t p = s_isaac.p, base, n, m, i, e, e2;
    /* 19:5x, captura del usuario: el Cementerio mostraba arriba la carta ya
     * desterrada (la cuenta si bajo): la animacion de ida al Cementerio la
     * vuelve a pintar despues; se redibuja en cada llamada hasta el turno nuevo */
    if (s_isaac_redibujar)
        isaac_redibujar();
    if (!s_isaac.activo || carta_en(p, s_isaac.z) == s_isaac.k)
        return;                         /* nada anotado o sigue en el Campo */
    base = (p & 1u) * 0x13F8u;
    n = MEM8(0x0065D0AAu + base);
    for (i = n; i-- > 0;)
        if ((MEM32(0x0065DC7Cu + base + 8u * i) & 0x7FFFu) == s_isaac.k)
            break;
    if (i >= n)
        return;                         /* todavia no llego al Cementerio (se sigue mirando) */
    s_isaac.activo = 0;
    m = MEM8(0x0065D0ACu + base);
    if (m >= 80u)
        return;
    e = MEM32(0x0065DC7Cu + base + 8u * i);
    e2 = MEM32(0x0065DC80u + base + 8u * i);
    for (; i + 1u < n; ++i) {
        MEM32(0x0065DC7Cu + base + 8u * i) = MEM32(0x0065DC7Cu + base + 8u * (i + 1u));
        MEM32(0x0065DC80u + base + 8u * i) = MEM32(0x0065DC80u + base + 8u * (i + 1u));
    }
    MEM32(0x0065DC7Cu + base + 8u * (n - 1u)) = 0;
    MEM32(0x0065DC80u + base + 8u * (n - 1u)) = 0;
    MEM8(0x0065D0AAu + base) = (uint8_t)(n - 1u);
    /* a desterrados con la funcion del juego: sub_000D4B70(&carta) es la que usa
     * la orden de destierro 0x57 (sub_000CAD20 -> sub_000D3F60, la de Ciclon):
     * agrega a 0x65E17C (cuenta 0x65D0AC). (0x65DEFC/0x65D0AB NO son los
     * desterrados: ahi lo ponia la version de 19:24.) */
    {
        uint32_t sa = g_eax, sb = g_ebx, sc = g_ecx, sd = g_edx, si = g_esi, di = g_edi, bp = g_ebp, tmp;
        g_esp -= 8;
        tmp = g_esp;
        MEM32(tmp) = (e & 0xFFFF7FFFu) | ((p & 1u) << 15);   /* bit 15: el duenio (D4B70 lo lee) */
        MEM32(tmp + 4) = e2;
        PUSH32(g_esp, tmp);
        PUSH32(g_esp, 0x000D3F8Bu);
        sub_000D4B70();
        g_esp += 4 + 8;
        /* redibuja las pilas del tablero (lo que hacen las ordenes 0x5A y 0x57 al
         * terminar, sub_000CA450 / sub_000CAD20): si no, el Cementerio sigue
         * mostrando arriba la carta desterrada (captura del usuario, 19:4x) */
        g_eax = sa; g_ebx = sb; g_ecx = sc; g_edx = sd; g_esi = si; g_edi = di; g_ebp = bp;
    }
    m = MEM8(0x0065D0ACu + base);
    s_isaac.hecho = 1;
    s_isaac_redibujar = 1;
    isaac_redibujar();
    fprintf(stderr, "[CARTAS] Isaac: la carta k %u (jugador %u) pasa del Cementerio a desterrados (Cementerio %u, desterrados %u)\n",
            s_isaac.k, p, n - 1u, m);
    fflush(stderr);
}

/* sub_000C6390(p, z, 1): saca una carta del Campo (la usan el Cementerio
 * sub_000C6960, el destierro sub_000C69A0 y sub_000C7C50, que es por donde va
 * el atacado destruido: 0xC80BA/0xC80F5; 2026-11-13, registro del usuario: Isaac
 * anoto la carta pero no paso ni por C6960 ni por C8620). Si es la anotada por
 * Isaac y no la saca el propio destierro, se destierra (sub_000C69A0, que vuelve
 * a llamar aca). Desde 20:4x solo pasa: Isaac usa el camino de Macrocosmos. */
void sub_000C6390_pad(void);
void sub_000C6390(void)
{
    sub_000C6390_pad();
}

/* sub_000C6960(p, z, copia): del Campo al Cementerio (orden 0x5A; el final de la batalla) */
void sub_000C6960_pad(void);
void sub_000C6960(void)
{
    sub_000C6960_pad();
}

/* sub_000C8620(p, z, ?): destruir una carta del Campo (orden 0x5C...; la batalla
 * la llama para el atacado en sub_0009BA10 / sub_0009B5B0) */
void sub_000C8620_pad(void);
void sub_000C8620(void)
{
    sub_000C8620_pad();
}

void sub_0007F8D0_pad(void);
void sub_0007F8D0(void)
{
    uint32_t b = MEM32(g_esp + 8), at = MEM32(g_esp + 12) & 1u, p;
    sub_0007F8D0_pad();
    if (b == 0x006666E0u) {
        isaac_batalla(b, at);
        real_batalla(b);
    }
    if (s_k_fort < 0 || b != 0x006666E0u)
        return;
    for (p = 0; p < 2u; ++p) {
        uint32_t e = b + 6u + 12u * p, fl = MEM16(e), z = fl & 7u, clave;
        if (MEM16(e + 2) != (uint32_t)s_k_fort || z >= 5u || carta_en(p, z) != (uint32_t)s_k_fort)
            continue;
        if (p != at && (MEM16(b) & 1u))
            continue;                   /* ataque directo: no hay atacado */
        if (MEM16(e + 0xA)) {
            fprintf(stderr, "[CARTAS] Guerrero de la Fortaleza (jugador %u, casilla %u): sin dano de batalla (era %u)\n",
                    p, z, MEM16(e + 0xA));
            MEM16(e + 0xA) = 0;
        }
        if (!(fl & 8u))
            continue;
        clave = (MEM16(b) & 0xFFFEu) | 0x10000u;
        if (!s_fort_usado[p][z] || s_fort_usado[p][z] == clave) {
            s_fort_usado[p][z] = clave;
            MEM16(e) = (uint16_t)(fl & ~8u);
            fprintf(stderr, "[CARTAS] Guerrero de la Fortaleza (jugador %u, casilla %u): no se destruye en batalla\n",
                    p, z);
        } else
            fprintf(stderr, "[CARTAS] Guerrero de la Fortaleza (jugador %u, casilla %u): ya se salvo este turno\n",
                    p, z);
    }
}

/* ---- caius (Caius, el Monarca de las Sombras) ----------------------------- */
/* Usa el disparo y la entrada de Buscador de dragon (4506: resolver
 * sub_000AB0F0, casilla valida sub_000A2CB0, eleccion sub_000BB830; ctx como
 * el de Tifon: +2 bit 0 duenio, +4 bit 2 anulada, +0xA & 7 == 1 hay objetivo,
 * +0xC jugador y +0xD casilla del objetivo). Caius: cualquier carta del Campo;
 * la destierra con sub_000C69A0(p, z, 0) y, si era un monstruo de OSCURIDAD,
 * 1000 de dano al adversario con sub_000C8BC0. */
void sub_000AB0F0_pad(void);
void sub_000A2CB0_pad(void);
void sub_000BB830_pad(void);

void sub_000A2CB0(void)
{
    uint32_t ctx = MEM32(g_esp + 4), pz = MEM16(g_esp + 8);
    if (es_k(ctx, s_k_caius)) {
        ret16(carta_en(pz & 0xFFu, pz >> 8) != 0);
        return;
    }
    sub_000A2CB0_pad();
}

static uint32_t caius_anotar(uint32_t ctx, uint32_t p, uint32_t z)
{
    PUSH32(g_esp, z);
    PUSH32(g_esp, p);
    PUSH32(g_esp, ctx);
    PUSH32(g_esp, 0x000BD482u);
    sub_000BAA20();
    g_esp += 12;
    return (g_eax & 0xFFFFu) != 0;
}

/* la eleccion: la CPU (jugador 1) la primera carta del adversario (monstruos
 * primero), si no tiene ninguna una suya que no sea Caius, si no Caius; el
 * jugador, como Golpe de fenix pero con las casillas de los dos (mascara
 * 0xFE de cada jugador) y el cursor en el campo del adversario si tiene cartas */
void sub_000BB830(void)
{
    uint32_t ctx = MEM32(g_esp + 4);
    uint32_t sb, si, di, bp, yo, op, z, r = 0;
    if (!es_k(ctx, s_k_caius)) {
        sub_000BB830_pad();
        return;
    }
    sb = g_ebx; si = g_esi; di = g_edi; bp = g_ebp;
    yo = MEM8(ctx + 2) & 1u;
    op = yo ^ 1u;
    if (yo) {
        MEM8(ctx + 0xA) &= 0xF8u;
        for (z = 0; z <= 10u && !r; ++z)
            if (carta_en(op, z))
                r = caius_anotar(ctx, op, z);
        for (z = 0; z <= 10u && !r; ++z)
            if (carta_en(yo, z) && carta_en(yo, z) != (uint32_t)s_k_caius)
                r = caius_anotar(ctx, yo, z);
        for (z = 0; z < 5u && !r; ++z)
            if (carta_en(yo, z))
                r = caius_anotar(ctx, yo, z);
        fprintf(stderr, "[CARTAS] Caius (CPU): objetivo jugador %u casilla %u\n", MEM8(ctx + 0xC) & 1u, MEM8(ctx + 0xD));
    } else if (!MEM8(0x0066BFC1u)) {
        uint32_t lado = op, zona = 5u, cas = 0, hay = 0;
        for (z = 0; z <= 10u; ++z)
            hay |= carta_en(op, z) != 0;
        if (!hay)
            lado = yo;
        for (z = 0; z < 5u; ++z)
            if (carta_en(lado, z)) {
                zona = 0;
                cas = z;
                break;
            }
        PUSH32(g_esp, s_txt_caius);
        PUSH32(g_esp, 0xBu);
        PUSH32(g_esp, 0x712u);
        PUSH32(g_esp, 0x206u);
        PUSH32(g_esp, 0x000BD569u);
        sub_00087E20();
        g_esp += 16;
        MEM8(ctx + 0xA) &= 0xF8u;
        PUSH32(g_esp, cas);
        PUSH32(g_esp, zona);
        PUSH32(g_esp, lado);
        PUSH32(g_esp, 0x000BD586u);
        sub_00086F50();
        g_esp += 12;
        MEM8(0x0066BFC1u) = (uint8_t)(MEM8(0x0066BFC1u) + 1u);
    } else if (MEM8(0x006589E6u) & 2u)
        MEM8(0x0066BFC1u) = 0;
    else {
        PUSH32(g_esp, 0xFEu | (0xFEu << 16));
        PUSH32(g_esp, 0x000BD50Au);
        sub_00092E20();
        g_esp += 4;
        if (g_eax & 0xFFFFu) {
            r = caius_anotar(ctx, MEM32(0x006633D4u), MEM32(0x006633D8u) + MEM32(0x006633DCu));
            if (!r) {
                PUSH32(g_esp, 0x10000004u);
                PUSH32(g_esp, 0x000BD545u);
                sub_0007E280();
                g_esp += 4;
            }
        }
    }
    g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
    ret16(r);
}

void sub_000AB0F0(void)
{
    uint32_t ctx = MEM32(g_esp + 4);
    uint32_t sb, si, di, bp, yo, p, z, c;
    if (!es_k(ctx, s_k_caius)) {
        sub_000AB0F0_pad();
        return;
    }
    sb = g_ebx; si = g_esi; di = g_edi; bp = g_ebp;
    yo = MEM8(ctx + 2) & 1u;
    p = MEM8(ctx + 0xC) & 1u;
    z = MEM8(ctx + 0xD);
    c = carta_en(p, z);
    if (MEM8(ctx + 4) & 4u)
        fprintf(stderr, "[CARTAS] Caius: anulado\n");
    else if ((MEM8(ctx + 0xA) & 7u) != 1u || !c)
        fprintf(stderr, "[CARTAS] Caius: sin objetivo (jugador %u, casilla %u)\n", p, z);
    else {
        int oscuro = z < 5u && ((MEM32(T_DATOS + 4u * c) >> 29) & 7u) == 2u;
        PUSH32(g_esp, 0);
        PUSH32(g_esp, z);
        PUSH32(g_esp, p);
        PUSH32(g_esp, 0x000B845Bu);
        sub_000C69A0();
        g_esp += 12;
        if (oscuro) {
            PUSH32(g_esp, 1000u);
            PUSH32(g_esp, yo ^ 1u);
            PUSH32(g_esp, 0x000AF75Cu);
            sub_000C8BC0();
            g_esp += 8;
        }
        fprintf(stderr, "[CARTAS] Caius (jugador %u): destierra la carta k %u (jugador %u, casilla %u)%s\n",
                yo, c, p, z, oscuro ? ", era de OSCURIDAD: 1000 de dano al adversario" : "");
    }
    g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
    ret16(0);
}

/* ---- abare_ushioni (2026-11-17) --------------------------------------------
 * ctx (el de Mago de tiempo): +0 k, +2 bit 0 el jugador, bits 4-9 la casilla,
 * +4 bit 2 anulada. Condicion (ctx, ?, u16 que tiene que ser 0). */
void sub_000A5010_pad(void);
void sub_000B2D10_pad(void);

void sub_000A5010(void)
{
    uint32_t ctx = MEM32(g_esp + 4), p, z;
    if (s_k_abare < 0 || !es_k(ctx, s_k_abare)) {
        sub_000A5010_pad();
        return;
    }
    p = MEM8(ctx + 2) & 1u;
    z = (MEM16(ctx + 2) >> 4) & 0x3Fu;
    ret16(!MEM16(g_esp + 0xC) && z < 5u && carta_en(p, z) == (uint32_t)s_k_abare
          && (MEM8(p * 0x13F8u + z * 0x9Cu + 0x0065D0D2u) & 2u) && !s_abare_usado[p][z]);
}

void sub_000B2D10(void)
{
    uint32_t ctx = MEM32(g_esp + 4), p, z, st;
    if (s_k_abare < 0 || !es_k(ctx, s_k_abare)) {
        sub_000B2D10_pad();
        return;
    }
    p = MEM8(ctx + 2) & 1u;
    z = (MEM16(ctx + 2) >> 4) & 0x3Fu;
    st = MEM32(0x0066BFBCu) & 0xFFu;
    if (st == 0x80u && z < 5u)
        s_abare_usado[p][z] = 1;
    if (st != 0x7Fu || (MEM8(ctx + 4) & 4u)) {
        sub_000B2D10_pad();             /* 0x80: la moneda; 0xA: fin (como Mago de tiempo) */
        return;
    }
    {
        uint32_t sb = g_ebx, si = g_esi, di = g_edi, bp = g_ebp;
        int acerto = MEM16(0x0065F930u) != 0;
        PUSH32(g_esp, 1000u);
        PUSH32(g_esp, acerto ? p ^ 1u : p);
        PUSH32(g_esp, 0x000B2F2Eu);
        sub_000C8BC0();
        g_esp += 8;
        fprintf(stderr, "[CARTAS] Abare Ushioni (jugador %u, casilla %u): %s, 1000 de dano %s\n",
                p, z, acerto ? "acerto" : "fallo", acerto ? "al adversario" : "a su duenio");
        g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
        ret16(0xAu);
    }
}

/* ---- acantilado_naturia (2026-11-17) ----------------------------------------
 * sub_000C5DA0: una carta deja el Campo por otra causa que la batalla y va al
 * Cementerio (registra los disparos de Sangan, Bruja del Black Forest...): el
 * Acantilado pasa por Sangan mientras corre. sub_000B0FF0: el resolver de Rata
 * gigante / Tomate de Mystic (ctx +0 k, +2 bit 0 jugador; estado 0x66BFBC). */
void sub_000C5DA0_pad(void);
void sub_000B0FF0_pad(void);

void sub_000C5DA0(void)
{
    uint16_t ia = 0, ib = 0;
    if (s_k_acant > 0) {
        ia = MEM16(T_IMAGEN + 2u * (uint32_t)s_k_acant);
        MEM16(T_IMAGEN + 2u * (uint32_t)s_k_acant) = (uint16_t)ID_SANGAN;
    }
    if (s_k_beren > 0) {
        ib = MEM16(T_IMAGEN + 2u * (uint32_t)s_k_beren);
        MEM16(T_IMAGEN + 2u * (uint32_t)s_k_beren) = (uint16_t)ID_SANGAN;
    }
    sub_000C5DA0_pad();
    if (s_k_beren > 0)
        MEM16(T_IMAGEN + 2u * (uint32_t)s_k_beren) = ib;
    if (s_k_acant > 0)
        MEM16(T_IMAGEN + 2u * (uint32_t)s_k_acant) = ia;
}

static void acant_lista(uint32_t p)
{
    uint32_t base = (p & 1u) * 0x13F8u, n = MEM8(0x0065D0A9u + base), i, m = 0;
    for (i = 0; i < n && m < 128u; ++i) {
        uint32_t e = MEM32(0x0065D9FCu + base + 8u * i), e2 = MEM32(0x0065DA00u + base + 8u * i);
        uint32_t c = e & 0x7FFFu, nivel;
        if (!c || c >= NCARTAS || !s_naturia[c] || !es_monstruo(c))
            continue;
        nivel = (MEM32(T_DATOS + 4u * c) >> 25) & 0xFu;
        if (nivel < 1u || nivel > 4u)
            continue;
        MEM32(0x0065855Cu + 8u * m) = e;
        MEM32(0x00658560u + 8u * m) = e2;
        MEM8(0x0065895Cu + m) = 2;      /* del Deck */
        ++m;
    }
    MEM16(0x006589DCu) = (uint16_t)m;
    g_eax = m;
    fprintf(stderr, "[CARTAS] Acantilado Naturia (jugador %u): %u Naturia de Nivel 4 o menor en el Deck\n", p, m);
}

void sub_000B0FF0(void)
{
    uint32_t ctx = MEM32(g_esp + 4), st;
    uint16_t ia;
    if (s_k_acant <= 0 || !es_k(ctx, s_k_acant)) {
        sub_000B0FF0_pad();
        return;
    }
    st = MEM32(0x0066BFBCu) & 0xFFu;
    ia = MEM16(T_IMAGEN + 2u * (uint32_t)s_k_acant);
    MEM16(T_IMAGEN + 2u * (uint32_t)s_k_acant) = (uint16_t)ID_RATA;
    s_acant_lista = 1;
    sub_000B0FF0_pad();
    s_acant_lista = 0;
    MEM16(T_IMAGEN + 2u * (uint32_t)s_k_acant) = ia;
    fprintf(stderr, "[CARTAS] Acantilado Naturia (jugador %u): resolver de Rata gigante, estado 0x%X -> 0x%X\n",
            MEM8(ctx + 2) & 1u, st, g_eax & 0xFFFFu);
}

/* ---- berenjena_naturia (2026-11-17) ------------------------------------------
 * sub_000BABA0 (la eleccion de Mago de la fe: ctx +2 bit 0 = la CPU elige con
 * sub_00089CB0, si no la lista en pantalla; paso u8 0x66BFC1) y
 * sub_000A9570 (su resolver), con la carta pasando por Mago de la fe. */
void sub_000BABA0_pad(void);
void sub_000A9570_pad(void);

static void beren_lista(uint32_t p)
{
    uint32_t base = (p & 1u) * 0x13F8u, n = MEM8(0x0065D0AAu + base), i, m = 0;
    for (i = 0; i < n && m < 128u; ++i) {
        uint32_t e = MEM32(0x0065DC7Cu + base + 8u * i), e2 = MEM32(0x0065DC80u + base + 8u * i);
        uint32_t c = e & 0x7FFFu;
        if (!c || c >= NCARTAS || !s_naturia[c] || !es_monstruo(c) || c == (uint32_t)s_k_beren)
            continue;
        MEM32(0x0065855Cu + 8u * m) = e;
        MEM32(0x00658560u + 8u * m) = e2;
        MEM8(0x0065895Cu + m) = 4;      /* del Cementerio */
        ++m;
    }
    MEM16(0x006589DCu) = (uint16_t)m;
    g_eax = m;
    fprintf(stderr, "[CARTAS] Berenjena Naturia (jugador %u): %u Naturia en el Cementerio\n", p, m);
}

void sub_000BABA0(void)
{
    uint32_t ctx = MEM32(g_esp + 4);
    uint16_t ib;
    if (s_k_beren <= 0 || !es_k(ctx, s_k_beren)) {
        sub_000BABA0_pad();
        return;
    }
    ib = MEM16(T_IMAGEN + 2u * (uint32_t)s_k_beren);
    MEM16(T_IMAGEN + 2u * (uint32_t)s_k_beren) = (uint16_t)ID_FE;
    s_beren_lista = 1;
    sub_000BABA0_pad();
    s_beren_lista = 0;
    MEM16(T_IMAGEN + 2u * (uint32_t)s_k_beren) = ib;
}

void sub_000A9570(void)
{
    uint32_t ctx = MEM32(g_esp + 4);
    uint16_t ib;
    if (s_k_beren <= 0 || !es_k(ctx, s_k_beren)) {
        sub_000A9570_pad();
        return;
    }
    fprintf(stderr, "[CARTAS] Berenjena Naturia (jugador %u): objetivo %s (k %u)\n", MEM8(ctx + 2) & 1u,
            (MEM8(ctx + 0xA) & 7u) == 2u ? "elegido" : "ninguno",
            (MEM16(ctx + 0xC) | ((uint32_t)MEM16(ctx + 0xE) << 16)) & 0x7FFFu);
    ib = MEM16(T_IMAGEN + 2u * (uint32_t)s_k_beren);
    MEM16(T_IMAGEN + 2u * (uint32_t)s_k_beren) = (uint16_t)ID_FE;
    sub_000A9570_pad();
    MEM16(T_IMAGEN + 2u * (uint32_t)s_k_beren) = ib;
}

/* ---- naturia_vanidosa (2026-11-17) -------------------------------------------- */
void sub_00082140_pad(void);
void sub_000A8500_pad(void);
void sub_000B71A0_pad(void);
void sub_000C7980(void);

/* el otro monstruo Naturia boca arriba de p (no la casilla z) con menos ATK, o -1 */
static int vani_pareja(uint32_t p, uint32_t z)
{
    uint32_t i, mejor = 0xFFFFFFFFu;
    int r = -1;
    for (i = 0; i < 5u; ++i) {
        uint32_t c = carta_en(p, i), atk;
        if (i == z || !c || c >= NCARTAS || !s_naturia[c] || !es_monstruo(c)
                || !(MEM8((p & 1u) * 0x13F8u + i * 0x9Cu + 0x0065D0D2u) & 2u))
            continue;
        atk = (MEM32(T_DATOS + 4u * c) >> 9) & 0x1FFu;
        if (atk < mejor) {
            mejor = atk;
            r = (int)i;
        }
    }
    return r;
}

/* la activacion (puntero a su carta: u16 +0 = k) es una Magica o una Trampa */
static int vani_magica_trampa(uint32_t act)
{
    uint32_t c, t;
    if (!act)
        return 0;
    c = MEM16(act) & 0x7FFFu;
    if (!c || c >= NCARTAS)
        return 0;
    t = (MEM32(T_DATOS + 4u * c) >> 20) & 0x1Fu;
    return t == 0x15u || t == 0x16u;
}

void sub_00082140(void)
{
    uint32_t act = MEM32(g_esp + 4), p = MEM32(g_esp + 8) & 1u, zona = MEM32(g_esp + 0xC), z = MEM32(g_esp + 0x10);
    if (s_k_vani > 0 && zona == 0u && z < 5u && carta_en(p, z) == (uint32_t)s_k_vani
            && (MEM8(p * 0x13F8u + z * 0x9Cu + 0x0065D0D2u) & 2u)) {
        int ok = vani_magica_trampa(act) && (MEM8(act + 2) & 1u) != p && vani_pareja(p, z) >= 0;
        fprintf(stderr, "[CARTAS] Naturia Vanidosa (jugador %u, casilla %u): %s responder a la carta k %u\n",
                p, z, ok ? "puede" : "no puede", act ? MEM16(act) & 0x7FFFu : 0u);
        g_eax = (g_eax & 0xFFFF0000u) | (ok ? 0x41u : 1u);
        g_esp += 4;
        return;
    }
    sub_00082140_pad();
}

void sub_000A8500(void)
{
    uint32_t ctx = MEM32(g_esp + 4), act = MEM32(g_esp + 8), p, z;
    if (s_k_vani <= 0 || !es_k(ctx, s_k_vani)) {
        sub_000A8500_pad();
        return;
    }
    p = MEM8(ctx + 2) & 1u;
    z = (MEM16(ctx + 2) >> 4) & 0x3Fu;
    ret16(!MEM16(g_esp + 0xC) && vani_magica_trampa(act) && vani_pareja(p, z) >= 0);
}

void sub_000B71A0(void)
{
    uint32_t ctx = MEM32(g_esp + 4), act = MEM32(g_esp + 8), p, z;
    uint32_t sb, si, di, bp;
    int z2;
    if (s_k_vani <= 0 || !es_k(ctx, s_k_vani)) {
        sub_000B71A0_pad();
        return;
    }
    sb = g_ebx; si = g_esi; di = g_edi; bp = g_ebp;
    p = MEM8(ctx + 2) & 1u;
    z = (MEM16(ctx + 2) >> 4) & 0x3Fu;
    z2 = vani_pareja(p, z);
    if (z2 >= 0) {
        PUSH32(g_esp, (uint32_t)z2);
        PUSH32(g_esp, p);
        PUSH32(g_esp, 0x000B71E1u);
        sub_000C7980();
        g_esp += 8;
    }
    if (z < 5u && carta_en(p, z) == (uint32_t)s_k_vani) {
        PUSH32(g_esp, z);
        PUSH32(g_esp, p);
        PUSH32(g_esp, 0x000B71E1u);
        sub_000C7980();
        g_esp += 8;
    }
    if (z2 >= 0 && vani_magica_trampa(act)) {
        PUSH32(g_esp, 0);
        PUSH32(g_esp, 0);
        PUSH32(g_esp, 1);
        PUSH32(g_esp, ((MEM16(ctx + 2) << 15) & 0xFFFFu) | 0x88u);
        PUSH32(g_esp, 0x000B721Bu);
        sub_00081150();
        g_esp += 16;
    }
    fprintf(stderr, "[CARTAS] Naturia Vanidosa (jugador %u): sacrifica la casilla %u y la %d; %s la carta k %u\n",
            p, z, z2, z2 >= 0 && vani_magica_trampa(act) ? "niega y destruye" : "no niega",
            act ? MEM16(act) & 0x7FFFu : 0u);
    g_ebx = sb; g_esi = si; g_edi = di; g_ebp = bp;
    ret16(0);
}

/* ---- chinche_naturia (2026-11-17) --------------------------------------------
 * p el que ataca, at = u32 0x6666E0 (bit 0 directo, bits 13-15 la casilla
 * atacada del otro jugador). Devuelve 1 si la Chinche actuo. */
static int chinche_ataque(uint32_t p, uint32_t at)
{
    uint32_t d = (p ^ 1u) & 1u, z = (at >> 13) & 7u, c, zc;
    uint32_t sa, sb, sc, sd, si, di, bp;
    if (z >= 5u)
        return 0;
    c = carta_en(d, z);
    if (!c || c >= NCARTAS || !s_naturia[c] || !es_monstruo(c)
            || !(MEM8(d * 0x13F8u + z * 0x9Cu + 0x0065D0D2u) & 2u))
        return 0;
    for (zc = 0; zc < 5u; ++zc)
        if (carta_en(d, zc) == (uint32_t)s_k_chinche && (MEM8(d * 0x13F8u + zc * 0x9Cu + 0x0065D0D2u) & 2u))
            break;
    if (zc >= 5u)
        return 0;
    sa = g_eax; sb = g_ebx; sc = g_ecx; sd = g_edx; si = g_esi; di = g_edi; bp = g_ebp;
    PUSH32(g_esp, zc);
    PUSH32(g_esp, d);
    PUSH32(g_esp, 0x000B71E1u);
    sub_000C7980();
    g_esp += 8;
    PUSH32(g_esp, 0);
    PUSH32(g_esp, 0);
    PUSH32(g_esp, 0);
    PUSH32(g_esp, 0x1Fu | (d << 15));
    PUSH32(g_esp, 0x000AD211u);
    sub_00081150();
    g_esp += 16;
    fprintf(stderr, "[CARTAS] Chinche Hedionda Naturia (jugador %u, casilla %u): atacan a la carta k %u (casilla %u); "
            "va al Cementerio, ataque negado y fin de la Battle Phase\n", d, zc, c, z);
    g_eax = sa; g_ebx = sb; g_ecx = sc; g_edx = sd; g_esi = si; g_edi = di; g_ebp = bp;
    return 1;
}

/* ---- cosmo_naturia (2026-11-17) ----------------------------------------------- */
void sub_000963C0(void);

/* el indice de la carta k en la mano de p, o -1 */
static int mano_indice(uint32_t p, uint32_t k)
{
    uint32_t n = MEM8(0x0065D0A8u + (p & 1u) * 0x13F8u), i;
    for (i = 0; i < n && i < 64u; ++i)
        if (mano_k(p, i) == k)
            return (int)i;
    return -1;
}

static int casilla_libre(uint32_t p)
{
    uint32_t z;
    for (z = 0; z < 5u; ++z)
        if (!carta_en(p, z))
            return (int)z;
    return -1;
}

/* despues de cada paso de la Llamada Normal (sub_00093FA0): en el estado 2
 * (una vez por llamada) registra el disparo de la Cosmoremolacha del otro */
static void cosmo_registrar(const char *que);
static void cosmo_llamada(void)
{
    static int hecho;
    uint32_t est = (MEM16(0x0066BA8Eu) >> 5) & 0x7Fu;
    if (s_k_cosmo <= 0)
        return;
    if (est != 2u) {
        hecho = 0;
        return;
    }
    if (hecho)
        return;
    hecho = 1;
    cosmo_registrar("Llamada Normal");
}

/* el estado de entrada 0 empieza otra llamada; "listo": la carta ya esta en
 * el Campo (una vez por llamada) */
static void cosmo_maquina(uint32_t est, int listo, const char *que)
{
    static int hecho;
    if (s_k_cosmo <= 0)
        return;
    if (est == 0u)
        hecho = 0;
    if (!listo || hecho)
        return;
    hecho = 1;
    cosmo_registrar(que);
}

static void cosmo_registrar(const char *que)
{
    uint32_t s, d;
    int z;
    s = MEM32(0x0066BA7Cu) & 1u;
    d = s ^ 1u;
    if (mano_indice(d, (uint32_t)s_k_cosmo) < 0)
        return;
    z = casilla_libre(d);
    if (z < 0) {
        fprintf(stderr, "[CARTAS] Cosmoremolacha Naturia (jugador %u): el adversario llamo, pero no hay casilla libre\n", d);
        return;
    }
    {
        uint32_t sa = g_eax, sb = g_ebx, sc = g_ecx, sd = g_edx, si = g_esi, di = g_edi, bp = g_ebp;
        PUSH32(g_esp, (((uint32_t)z | 0x840u | (d << 15)) << 16) | (uint32_t)s_k_cosmo);
        PUSH32(g_esp, 0x0009F174u);
        sub_00081EF0();
        g_esp += 4;
        g_eax = sa; g_ebx = sb; g_ecx = sc; g_edx = sd; g_esi = si; g_edi = di; g_ebp = bp;
    }
    fprintf(stderr, "[CARTAS] Cosmoremolacha Naturia (jugador %u): %s del adversario (carta k %u); disparo registrado\n",
            d, que, MEM32(0x0066BA80u) & 0x7FFFu);
    fflush(stderr);
}

/* el eslabon que se resuelve (u16 0x66BF98; entradas de 0x14 en 0x66BD04: +0
 * k, +2 bit 0 duenio): el resolver recibe una copia de la casilla del Campo
 * del eslabon (sub_000D1E70 -> 0x66BFB4), vacia para la Cosmoremolacha (esta
 * en la mano): se la reconoce por el eslabon. Devuelve el duenio o -1. */
static int cosmo_eslabon(void)
{
    uint32_t e = 0x0066BD04u + 0x14u * MEM16(0x0066BF98u);
    if ((MEM16(e) & 0x7FFFu) != (uint32_t)s_k_cosmo)
        return -1;
    return (int)(MEM8(e + 2) & 1u);
}

/* el resolver; estado u8 0x66BFBC, empieza en 0x80 */
static int cosmo_resolver(uint32_t p)
{
    uint32_t sb = g_ebx, sc = g_ecx, sd = g_edx, si = g_esi, di = g_edi, bp = g_ebp;
    uint32_t st = MEM8(0x0066BFBCu), ret = 0;
    fprintf(stderr, "[CARTAS] Cosmoremolacha Naturia (jugador %u): resolver, estado %02X\n", p, st);
    if (st == 0x80u) {
        int i = mano_indice(p, (uint32_t)s_k_cosmo);
        if (i < 0)
            fprintf(stderr, "[CARTAS] Cosmoremolacha Naturia (jugador %u): ya no esta en la mano\n", p);
        else if (casilla_libre(p) < 0)
            fprintf(stderr, "[CARTAS] Cosmoremolacha Naturia (jugador %u): sin casilla libre\n", p);
        else {
            uint32_t e = 0x0065D77Cu + p * 0x13F8u + 8u * (uint32_t)i;
            MEM32(0x006633DCu) = (uint32_t)i;
            PUSH32(g_esp, 0);
            PUSH32(g_esp, MEM16(e + 2));
            PUSH32(g_esp, MEM16(e));
            PUSH32(g_esp, (p << 15) | 0x8Eu);
            PUSH32(g_esp, 0x000B8C41u);
            sub_00081150();
            g_esp += 16;
            PUSH32(g_esp, 0);
            PUSH32(g_esp, 1);
            PUSH32(g_esp, e);
            PUSH32(g_esp, p);
            PUSH32(g_esp, 0x000B8C54u);
            sub_000963C0();
            g_esp += 16;
            fprintf(stderr, "[CARTAS] Cosmoremolacha Naturia (jugador %u): Llamada Especial desde la mano (carta %d)\n", p, i);
            ret = 0x7Eu;
        }
    }
    fflush(stderr);
    g_ebx = sb; g_ecx = sc; g_edx = sd; g_esi = si; g_edi = di; g_ebp = bp;
    g_eax = (g_eax & 0xFFFF0000u) | ret;
    g_esp += 4;                         /* ret */
    return 1;
}

/* ---- 2026-11-17 22:xx: arreglos tras la prueba del usuario --------------------
 * Chinche: la declaracion (sub_0009A290) no corre cuando ataca la CPU (registro:
 * atacada la Chinche boca arriba, ninguna linea). Ahora en el paso 1 del combate
 * (sub_0009A540, rayo.c), el que sigue a la declaracion de cualquier ataque, una
 * vez por ataque; ataca el jugador del turno ((0x65F8DA >> 1) & 1). */
static uint32_t s_paso_hecho;
/* rayo.c, despues del paso 1: si el paso termino (devolvio != 0: el despacho
 * sub_0009E5C0 pasa al 2) o cambio el estado, el proximo ataque se revisa de
 * nuevo. Antes se reiniciaba solo con el estado != 1, pero el paso 1 corre solo
 * en el estado 1: tras el primer ataque de la partida no se revisaba mas. */
void ygo_cartas_paso_ataque_fin(void)
{
    if ((g_eax & 0xFFFFu) || MEM8(0x0065F8DEu) != 1)
        s_paso_hecho = 0;
}

void ygo_cartas_paso_ataque(void)
{
    uint32_t at = MEM32(0x006666E0u), p = (MEM8(0x0065F8DAu) >> 1) & 1u;
    if (MEM8(0x0065F8DEu) != 1) {
        s_paso_hecho = 0;
        return;
    }
    if (s_paso_hecho || s_k_chinche <= 0)
        return;
    s_paso_hecho = 1;
    if (at & 1u)
        return;
    {
        uint32_t d = p ^ 1u, z = (at >> 13) & 7u, c = z < 5u ? carta_en(d, z) : 0u;
        if (c && c < NCARTAS && s_naturia[c]) {
            fprintf(stderr, "[CARTAS] ataque del jugador %u a la casilla %u del %u: Naturia k %u\n", p, z, d, c);
            if (!chinche_ataque(p, at))
                fprintf(stderr, "[CARTAS] Chinche Hedionda Naturia: no actua (no esta boca arriba en el Campo del %u, o el atacado esta boca abajo)\n", d);
            fflush(stderr);
        }
    }
}

/* Acantilado y Berenjena al ser sacrificados: sub_000C7980(p, z) (el sacrificio:
 * Invocacion por Sacrificio, Maryokutai...) compara la imagen con Sangan (0xFD6)
 * y registra su disparo; la carta pasa por Sangan mientras corre. */
void sub_000C7980_pad(void);
void sub_000C7980(void)
{
    uint32_t pp = MEM32(g_esp + 4) & 1u, z = MEM32(g_esp + 8), c = z < 5u ? carta_en(pp, z) : 0u;
    uint16_t ia;
    if (!como_sangan(c)) {
        sub_000C7980_pad();
        return;
    }
    fprintf(stderr, "[CARTAS] %s sacrificada (jugador %u, casilla %u): pasa por Sangan\n",
            c == (uint32_t)s_k_acant ? "Acantilado Naturia" : "Berenjena Naturia", pp, z);
    fflush(stderr);
    ia = MEM16(T_IMAGEN + 2u * c);
    MEM16(T_IMAGEN + 2u * c) = (uint16_t)ID_SANGAN;
    sub_000C7980_pad();
    MEM16(T_IMAGEN + 2u * c) = ia;
}

/* Vanidosa en cualquier turno: sub_000A4EC0(activacion, p) decide si p puede
 * encadenar (y si se le ofrece). Revisa las Magicas/Trampas (sub_000A4C60), en
 * su turno las Rapidas de la mano y SOLO fuera de su turno y contra una Magica
 * (0x16) los monstruos: Maryokutai (0x148C) boca arriba. La Vanidosa responde
 * en los dos turnos a Magicas y Trampas del adversario. */
void sub_000A4EC0_pad(void);
int ygo_auto_on(void);                  /* auto.c: AUTO on, the player is never asked */
void sub_000A4EC0(void)
{
    uint32_t act = MEM32(g_esp + 4), p = MEM32(g_esp + 8) & 1u, z;
    if (p == 0u && ygo_auto_on()) {
        g_eax &= 0xFFFF0000u;           /* 0: the player does not chain, no box */
        g_esp += 4;
        return;
    }
    if (s_k_vani > 0 && act && vani_magica_trampa(act) && (MEM8(act + 2) & 1u) != p) {
        for (z = 0; z < 5u; ++z) {
            if (carta_en(p, z) != (uint32_t)s_k_vani || !(MEM8(p * 0x13F8u + z * 0x9Cu + 0x0065D0D2u) & 2u))
                continue;
            if (vani_pareja(p, z) < 0) {
                fprintf(stderr, "[CARTAS] Naturia Vanidosa (jugador %u): falta otro Naturia boca arriba\n", p);
                break;
            }
            fprintf(stderr, "[CARTAS] Naturia Vanidosa (jugador %u, casilla %u): puede encadenar a la carta k %u\n",
                    p, z, MEM16(act) & 0x7FFFu);
            fflush(stderr);
            g_eax = (g_eax & 0xFFFF0000u) | 1u;
            g_esp += 4;
            return;
        }
    }
    sub_000A4EC0_pad();
}

/* ---- 2026-11-17 22:4x: copias que aparecen en el cofre (el usuario: "tenia 1
 * en el mazo y paso a tener 2") -------------------------------------------------
 * Diagnostico: cada cambio del cofre (u8 0x651E70 + 0xCC1 + k) se anota con la
 * hora, y las dos funciones del juego que le suman copias: sub_000D9420 (repasa
 * las 0x473 cartas y suma 1 por cada carta del mazo 0x66609A) y sub_000D9720. */
void sub_000D9420_pad(void);
void sub_000D9720_pad(void);
void sub_000D9420(void)
{
    fprintf(stderr, "[COFRE] sub_000D9420 (vuelve a contar el cofre con el mazo: %u cartas)\n", MEM16(0x0066649Au));
    fflush(stderr);
    sub_000D9420_pad();
}
void sub_000D9720(void)
{
    /* 2026-11-25: the god picked on the "Cual quieres?" screen. The only
     * caller, sub_00101A00 (edi = that screen), takes the group
     * MEM32(MEM32(screen + 0x10) + 0x74) (0 = Obelisco, confirmed by the user) */
    {
        extern int ygo_ram_ok(uint32_t a);
        uint32_t o = ygo_ram_ok(g_edi) ? MEM32(g_edi + 0x10u) : 0u;
        uint32_t dios = ygo_ram_ok(o) ? MEM32(o + 0x74u) : 99u;
        fprintf(stderr, "[COFRE] dios elegido: grupo %u (ranura %u de [PREMIOS])\n", dios, dios);
        {
            extern void ygo_premios_dioses_hecho(uint32_t scr);
            ygo_premios_dioses_hecho(g_edi);    /* premios.c: the gods' sign goes away */
        }
    }
    fprintf(stderr, "[COFRE] sub_000D9720 (suma copias al cofre)\n");
    fflush(stderr);
    sub_000D9720_pad();
    {
        extern void ygo_guardado_premio(void);
        ygo_guardado_premio();          /* guardado.c: saved with each prize */
    }
}

/* llamada 10 veces por segundo (premios_extra.c, con ygo_cartas_limites) */
void ygo_cofre_vigilar(void)
{
    static uint8_t ant[NCARTAS];
    static int listo;
    uint32_t k, cambios = 0;
    if (!listo) {
        for (k = 1; k < NCARTAS; ++k)
            ant[k] = (uint8_t)MEM8(0x00651E70u + 0xCC1u + k);
        listo = 1;
        return;
    }
    for (k = 1; k < NCARTAS; ++k) {
        uint8_t c = (uint8_t)MEM8(0x00651E70u + 0xCC1u + k);
        if (c == ant[k])
            continue;
        if (++cambios <= 12u) {
            SYSTEMTIME t;
            GetLocalTime(&t);
            fprintf(stderr, "[COFRE] %02u:%02u:%02u k %u (imagen %u%s): %u -> %u copias\n", t.wHour, t.wMinute,
                    t.wSecond, k, MEM16(T_IMAGEN + 2u * k), s_naturia[k] ? ", Naturia" : "", ant[k], c);
        }
        ant[k] = c;
    }
    if (cambios > 12u)
        fprintf(stderr, "[COFRE] ... %u cambios en total (cofre cargado o rearmado)\n", cambios);
    if (cambios)
        fflush(stderr);
    /* 2026-11-20: the Ganga de arrancada diagnostics ([CAMPO], memory search,
     * forced Ganga, [MOVER]) are gone: the cause was the recompiler's 8-bit
     * imul (see the note before sub_000CA130) */
}

/* ---- 2026-11-17 23:5x: Berenjena Naturia se disparaba al ser volteada --------
 * Su efecto es el de Mago de la fe (alias), que es de VOLTEO: la Llamada de
 * Volteo (sub_00094AF0, con los alias puestos) registraba su disparo (registro:
 * eslabon 0098/52/28 sin "va al Cementerio"). El disparo propio (al ir del Campo
 * al Cementerio) se registra con la carta pasando por Sangan (sub_000C7C50,
 * sub_000C5DA0, sub_000C7980). sub_00081EF0(arg: (casilla|0x840|duenio<<15)<<16
 * | k) registra todo disparo: el de la Berenjena solo pasa si su imagen es Sangan. */
void sub_00081EF0_pad(void);
void sub_00081EF0(void)
{
    uint32_t a = MEM32(g_esp + 4);
    if (s_k_beren > 0 && (a & 0x7FFFu) == (uint32_t)s_k_beren
            && MEM16(T_IMAGEN + 2u * (uint32_t)s_k_beren) != (uint16_t)ID_SANGAN) {
        fprintf(stderr, "[CARTAS] Berenjena Naturia: disparo de Volteo descartado (solo al ir del Campo al Cementerio)\n");
        fflush(stderr);
        g_eax = 0;
        g_esp += 4;
        return;
    }
    sub_00081EF0_pad();
}

/* ---- 2026-11-18 00:1x: Vanidosa en el turno propio ---------------------------
 * Registro 00:00: "puede encadenar", Si en la caja, y en la eleccion del Campo
 * (sub_00092E20 desde sub_00082280, vuelta 0x824A9) la casilla de la Vanidosa
 * "not a choice (mask 0F)". La mascara la pone el juego: 0x0F en el turno del
 * jugador 0 (mano, Magicas/Trampas), 0xEE en el del 1. sub_00092620 (casilla
 * valida): monstruos = bits 0xF0 << (p*16): 0x20 boca arriba, 0x40/0x80 segun
 * +0xA bit 0. Con la Vanidosa en condiciones se suma 0xE0 (lo que el juego da
 * fuera de turno); la casilla elegida la juzga despues sub_00082140. */
uint32_t ygo_cartas_mascara_cadena(uint32_t mask)
{
    uint32_t act = 0x0066BD04u + 0x14u * MEM16(0x0066BF98u), z;
    if (s_k_vani <= 0 || !vani_magica_trampa(act) || (MEM8(act + 2) & 1u) == 0u)
        return mask;
    for (z = 0; z < 5u; ++z)
        if (carta_en(0, z) == (uint32_t)s_k_vani && (MEM8(z * 0x9Cu + 0x0065D0D2u) & 2u) && vani_pareja(0, z) >= 0) {
            fprintf(stderr, "[CARTAS] Naturia Vanidosa (casilla %u): eleccion de la cadena, mascara %02X -> %02X\n",
                    z, mask, mask | 0xE0u);
            fflush(stderr);
            return mask | 0xE0u;
        }
    return mask;
}
