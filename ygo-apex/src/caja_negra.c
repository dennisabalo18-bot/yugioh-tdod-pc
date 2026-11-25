/* Caja negra del duelo (2026-11-24; el usuario: "no podemos hacer que el
 * registro de verdad registre las cosas porque cada vez que pasa algo no
 * sirve para nada el registro", después del duelo trabado tras la moneda de
 * Jirai Gumo en una PC con GT 1030).
 *
 * Qué hace:
 * - En cada lectura del mando (mando_directo.c, hilo del juego, hasta 250 por
 *   segundo) compara unos pocos valores del duelo con los de la vez anterior
 *   y, si alguno cambió, guarda una línea corta EN MEMORIA (un anillo de
 *   CAJA_N líneas; lo viejo se pisa). No escribe en el disco.
 * - Los otros módulos le pasan notas (ygo_caja_nota: el clic del mouse, la
 *   moneda...), que van al mismo anillo.
 * - Vuelca el anillo al registro solo cuando hace falta:
 *   * el duelo lleva CAJA_TRABA_MS sin que cambie nada del duelo y en ese
 *     tiempo hubo teclas o clics (alguien quiso jugar y el juego no avanzó):
 *     "[CAJA] DUELO TRABADO";
 *   * el juego dejó de leer el mando CAJA_MUDO_MS en pleno duelo (el hilo del
 *     juego quedó atrapado): lo ve un hilo aparte que mira una vez por
 *     segundo;
 *   * se cierra la ventana en pleno duelo, error del juego (fatal.c) o falla
 *     (main.c).
 *   Cada volcado termina con "estado ahora", todos los valores, y qué espera.
 * - Costo: unas 20 lecturas de memoria y comparaciones por lectura del mando;
 *   una línea en memoria solo cuando algo cambia. Nada por cuadro en el disco.
 *
 * Los valores (direcciones del juego; ESTADO.md y los .c que los usan):
 *   escena   u32 0x657148  6 el duelo, 7 el resultado (auto.c, danio.c)
 *   accion   u16 0x65F948  la acción en curso, bit 15 = el rival (raton.c)
 *   pedido   u32 0x65F91C  el pedido al jugador (sub_00084D00: jugador, modo)
 *   ped_arg  u16 0x65F920  su argumento
 *   paso     u8  0x65F92E  el paso del pedido (la moneda: 1 espera la cara)
 *   result   u16 0x65F930  lo elegido / la moneda acertada
 *   cara     u16 0x660B8C  la cara de la moneda
 *   atq      u8  0x65F8DE  el paso del ataque
 *   atq_est  u8  0x65F8DF  su estado (Jirai Gumo: 0xC8 tras la moneda)
 *   batalla  u16 0x6666E0  atacante (bits 10-12 casilla) y banderas
 *   llamada  u16 0x66BA8E  el despacho de las llamadas (tipo, estado)
 *   carta    u16 0x66BA80  la carta de esa llamada (k)
 *   cadena   u16 0x66BF9A  eslabones (cadena.c)
 *   fin      u32 0x65F894  bit 0x10 el duelo terminó (estadisticas.c)
 *   lp0/lp1  u32 0x65D0A4 / +0x13F8
 *   teclas   u16 0x6589E6  lo que el juego leyó del mando este cuadro
 */
#include <windows.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "recomp/gen/recomp_types.h"
typedef void (*recomp_exit_hook_fn)(const char *why);   /* platform/recomp_exit_hook.h */
int recomp_exit_hook_add(recomp_exit_hook_fn fn);

#define CAJA_N        2048u      /* líneas guardadas */
#define CAJA_LARGO    120        /* cada una */
#define CAJA_VUELCA   400u       /* las últimas que van al registro */
#define CAJA_TRABA_MS 20000u     /* sin cambios en el duelo, con teclas */
#define CAJA_MUDO_MS  8000u      /* el juego sin leer el mando */

typedef struct { const char *nombre; uint32_t dir; unsigned tam; int avanza; } valor;

static const valor k_val[] = {
    { "escena",  0x00657148u, 4, 1 },
    { "accion",  0x0065F948u, 2, 1 },
    { "pedido",  0x0065F91Cu, 4, 1 },
    { "ped_arg", 0x0065F920u, 2, 1 },
    { "paso",    0x0065F92Eu, 1, 1 },
    { "result",  0x0065F930u, 2, 1 },
    { "cara",    0x00660B8Cu, 2, 0 },
    { "atq",     0x0065F8DEu, 1, 1 },
    { "atq_est", 0x0065F8DFu, 1, 1 },
    { "batalla", 0x006666E0u, 2, 1 },
    { "llamada", 0x0066BA8Eu, 2, 1 },
    { "carta",   0x0066BA80u, 2, 1 },
    { "cadena",  0x0066BF9Au, 2, 1 },
    { "fin",     0x0065F894u, 4, 1 },
    { "lp0",     0x0065D0A4u, 4, 1 },
    { "lp1",     0x0065E49Cu, 4, 1 },
    { "teclas",  0x006589E6u, 2, 0 },
};
#define NVAL (sizeof k_val / sizeof k_val[0])

static char s_anillo[CAJA_N][CAJA_LARGO];
static volatile LONG s_n;               /* líneas escritas desde el arranque */
static CRITICAL_SECTION s_cs;
static volatile LONG s_lista;
static uint32_t s_ant[NVAL];
static int s_hay_ant;
static ULONGLONG s_t0;
static volatile ULONGLONG s_ult_mando;  /* la última lectura del mando */
static volatile ULONGLONG s_ult_avance; /* el último cambio del duelo */
static volatile LONG s_en_duelo;
static volatile LONG s_toques;          /* teclas o clics desde el último avance */
static volatile LONG s_volcado;         /* esta traba ya se volcó */
static unsigned s_botones_ant;

static uint32_t leer(const valor *v)
{
    return v->tam == 1 ? MEM8(v->dir) : v->tam == 2 ? MEM16(v->dir) : MEM32(v->dir);
}

static void preparar(void)
{
    if (InterlockedCompareExchange(&s_lista, 1, 0) == 0) {
        InitializeCriticalSection(&s_cs);
        s_t0 = GetTickCount64();
        InterlockedExchange(&s_lista, 2);
    }
    while (s_lista != 2)
        Sleep(0);
}

static void poner(const char *texto)
{
    LONG i;
    double t = (double)(GetTickCount64() - s_t0) / 1000.0;
    preparar();
    EnterCriticalSection(&s_cs);
    i = s_n++;
    snprintf(s_anillo[(unsigned)i % CAJA_N], CAJA_LARGO, "%9.3f %s", t, texto);
    LeaveCriticalSection(&s_cs);
}

void ygo_caja_nota(const char *fmt, ...)
{
    char b[CAJA_LARGO];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    poner(b);
}

/* un clic o una tecla del jugador (raton.c, el mando) */
void ygo_caja_toque(void)
{
    InterlockedIncrement(&s_toques);
}

static void estado_ahora(char *b, size_t n)
{
    size_t o = 0;
    unsigned i;
    for (i = 0; i < NVAL && o + 24 < n; ++i)
        o += (size_t)snprintf(b + o, n - o, "%s %X  ", k_val[i].nombre, leer(&k_val[i]));
}

/* el anillo al registro: las últimas CAJA_VUELCA líneas y el estado */
static void volcar(const char *por)
{
    static char estado[600];
    LONG n, desde, i;
    int con_cs;
    preparar();
    /* desde una falla el hilo que cayó puede tener la sección: no esperar */
    con_cs = TryEnterCriticalSection(&s_cs);
    n = s_n;
    desde = n > (LONG)CAJA_VUELCA ? n - (LONG)CAJA_VUELCA : 0;
    if (n - desde > (LONG)CAJA_N)
        desde = n - (LONG)CAJA_N;
    fprintf(stderr, "[CAJA] ===== %s =====\n", por);
    fprintf(stderr, "[CAJA] lo último del duelo (%ld cambios guardados, segundos desde el arranque):\n",
            (long)(n - desde));
    for (i = desde; i < n; ++i)
        fprintf(stderr, "[CAJA] %s\n", s_anillo[(unsigned)i % CAJA_N]);
    estado_ahora(estado, sizeof estado);
    fprintf(stderr, "[CAJA] estado ahora: %s\n", estado);
    fprintf(stderr, "[CAJA] sin cambios en el duelo hace %llu ms; última lectura del mando hace %llu ms; "
            "teclas o clics desde el último cambio: %ld\n",
            (unsigned long long)(GetTickCount64() - s_ult_avance),
            (unsigned long long)(GetTickCount64() - s_ult_mando), (long)s_toques);
    fprintf(stderr, "[CAJA] ===== fin =====\n");
    fflush(stderr);
    if (con_cs)
        LeaveCriticalSection(&s_cs);
}

void ygo_caja_volcar(const char *por)
{
    if (s_lista == 2 && s_n > 0)
        volcar(por);
}

static void al_salir(const char *why)
{
    if (s_en_duelo)
        ygo_caja_volcar(why && *why ? "se cerró el juego en pleno duelo" : "salida");
}

/* hilo aparte, una vez por segundo: el juego dejó de leer el mando */
static DWORD WINAPI vigia(LPVOID p)
{
    (void)p;
    for (;;) {
        Sleep(1000);
        if (s_en_duelo && !s_volcado && GetTickCount64() - s_ult_mando > CAJA_MUDO_MS) {
            InterlockedExchange(&s_volcado, 1);
            volcar("EL JUEGO DEJÓ DE RESPONDER EN PLENO DUELO (no lee el mando)");
        }
    }
    return 0;
}

/* cada lectura del mando (mando_directo.c), con los botones que van al juego */
void ygo_caja_tick(unsigned botones)
{
    static int arrancado;
    ULONGLONG t = GetTickCount64();
    uint32_t v[NVAL];
    unsigned i;
    int cambio = 0, avance = 0, duelo;
    char b[CAJA_LARGO];
    size_t o = 0;

    if (!arrancado) {
        arrancado = 1;
        preparar();
        recomp_exit_hook_add(al_salir);
        CloseHandle(CreateThread(NULL, 0, vigia, NULL, 0, NULL));
    }
    s_ult_mando = t;
    if ((botones & ~s_botones_ant) != 0u)
        InterlockedIncrement(&s_toques);
    s_botones_ant = botones;

    duelo = MEM32(0x00656F88u) != 0u && MEM32(0x00657148u) == 6u;
    if (!duelo && !s_en_duelo)
        return;
    for (i = 0; i < NVAL; ++i)
        v[i] = leer(&k_val[i]);
    if (!s_hay_ant || duelo != s_en_duelo) {
        memcpy(s_ant, v, sizeof v);
        s_hay_ant = 1;
        s_ult_avance = t;
        s_toques = 0;
        s_volcado = 0;
        InterlockedExchange(&s_en_duelo, duelo);
        ygo_caja_nota(duelo ? "--- empieza el duelo ---" : "--- termina el duelo (escena %X) ---", v[0]);
        return;
    }
    for (i = 0; i < NVAL; ++i) {
        if (v[i] == s_ant[i])
            continue;
        if (o + 24 < sizeof b)
            o += (size_t)snprintf(b + o, sizeof b - o, "%s %X>%X ", k_val[i].nombre, s_ant[i], v[i]);
        cambio = 1;
        if (k_val[i].avanza)
            avance = 1;
    }
    if (cambio) {
        memcpy(s_ant, v, sizeof v);
        poner(b);
    }
    if (avance) {
        s_ult_avance = t;
        s_toques = 0;
        s_volcado = 0;
    } else if (!s_volcado && s_toques >= 3 && t - s_ult_avance > CAJA_TRABA_MS) {
        InterlockedExchange(&s_volcado, 1);
        volcar("POSIBLE DUELO TRABADO: el duelo no avanza aunque hay teclas o clics");
    }
}
