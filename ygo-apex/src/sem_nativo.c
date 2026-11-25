/* sem_nativo.c — 2026-11-21 (the user: "segui asi entonces mejora todo lo
 * que se pueda").
 *
 * The 03:36 log: in the door's one frame (444 ms) the main thread and the
 * loader worker did 134,452 hand-overs through Alchemy's POSIX semaphores
 * (a pthreads-win32-like layer in the XDK title, text.asm):
 *   sub_002F1C70 sem_init(s, pshared, value): CreateSemaphore(value, 0x7FFFFFF)
 *   sub_002F1CC0 sem_destroy(s): CloseHandle
 *   sub_002F1CF0 sem_trywait(s): WaitForSingleObject(0), EAGAIN on timeout
 *   sub_002F1D30 sem_wait(s): WaitForMultipleObjects(sem, cancel event)
 *   sub_002F1D70 sem_post(s): ReleaseSemaphore(1)
 * Each one two system calls (~3.3 us a hand-over, even with the 50 us spin
 * of kernel_sync.c catching nearly all awake); on the console a kernel call
 * is a plain call.
 *
 * Here the count lives in memory, beside the same kernel semaphore (now used
 * only to sleep): post adds 1 and releases the kernel object only when a
 * waiter sleeps on it; wait takes a unit if there is one, spinning up to
 * 50 us for it, and only then goes to sleep on the kernel object; trywait
 * takes one or fails with EAGAIN. A semaphore made before this saw it, or
 * not found, goes to the game's own functions. The kernel object is drained
 * at sem_init, so its count stays the sleepers' wake-ups only.
 * Ajustes\semaforos_originales.txt: the game's functions, all of them.
 * Logs: "[SEM] ..." every 30 s while used. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "recomp/gen/recomp_types.h"

void sub_002F1C70_pad(void);
void sub_002F1CC0_pad(void);
void sub_002F1CF0_pad(void);
void sub_002F1D30_pad(void);
void sub_002F1D70_pad(void);
void sub_00359AAF(void);                /* the CRT's _errno() */

#define TABLA 1024                      /* a power of 2 */
typedef struct { volatile LONG h; volatile LONG c; } sem;
static sem s_t[TABLA];
static SRWLOCK s_cand = SRWLOCK_INIT;
static int s_modo = -1;
static int64_t s_giro;                  /* QPC ticks */
static volatile LONG s_post_rapido, s_post_despierta, s_wait_rapido, s_wait_duerme, s_try;

static int modo(void)
{
    if (s_modo < 0) {
        char p[MAX_PATH];
        const char *dir = getenv("YGO_AJUSTES");
        LARGE_INTEGER f;
        snprintf(p, sizeof p, "%s%ssemaforos_originales.txt", dir && *dir ? dir : "", dir && *dir ? "\\" : "");
        QueryPerformanceFrequency(&f);
        s_giro = f.QuadPart / 20000;    /* 50 us */
        s_modo = GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES;
        fprintf(stderr, "[SEM] %s\n", s_modo ? "semaforos de Alchemy con la cuenta en memoria"
                                             : "semaforos del juego (semaforos_originales.txt)");
    }
    return s_modo;
}

static unsigned ranura(uint32_t h)
{
    return (h >> 2) * 2654435761u & (TABLA - 1);
}

static sem *buscar(uint32_t h)
{
    unsigned i, k;
    if (!h)
        return NULL;
    for (k = 0, i = ranura(h); k < TABLA; k++, i = (i + 1) & (TABLA - 1)) {
        LONG v = s_t[i].h;
        if ((uint32_t)v == h)
            return &s_t[i];
        if (!v)
            return NULL;
    }
    return NULL;
}

static int64_t ahora(void)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

static void informe(void)
{
    static int64_t ult;
    int64_t t = ahora();
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    if (t - ult < f.QuadPart * 30)
        return;
    ult = t;
    fprintf(stderr, "[SEM] post %ld sin llamada / %ld despertando; wait %ld sin dormir / %ld durmiendo; trywait %ld\n",
            (long)s_post_rapido, (long)s_post_despierta, (long)s_wait_rapido, (long)s_wait_duerme, (long)s_try);
}

/* 03:4x: the first build hung the boot. The kit gives the title tokens,
 * not host handles (registro: "token=0x48000004 handle=...04F4"), and the
 * post called ReleaseSemaphore on the token itself: it failed and the
 * sleeper never woke. Every touch of the kernel object now goes through the
 * game's own functions (which go through the kit): the drain at sem_init is
 * the game's sem_trywait, the wake-up in sem_post the game's sem_post, the
 * sleep in sem_wait the game's sem_wait. */

/* the game's sem_trywait(s), called from here (cdecl): 0 when it took one */
static uint32_t trywait_juego(uint32_t s)
{
    uint32_t esp0 = g_esp;
    g_esp -= 8;
    MEM32(g_esp + 4) = s;
    MEM32(g_esp) = 0x002F1D20u;         /* a return address inside sem_trywait */
    sub_002F1CF0_pad();
    g_esp = esp0;
    return g_eax;
}

static void errno_juego(uint32_t valor)
{
    g_esp -= 4;
    MEM32(g_esp) = 0x002F1D20u;         /* a return address inside sem_trywait */
    sub_00359AAF();
    MEM32(g_eax) = valor;
}

/* int sem_init(sem_t *s, int pshared, unsigned value), cdecl */
void sub_002F1C70(void)
{
    uint32_t s = MEM32(g_esp + 4), valor = MEM32(g_esp + 12);
    if (!modo()) {
        sub_002F1C70_pad();
        return;
    }
    sub_002F1C70_pad();                 /* the kernel object, *s = its handle */
    if (g_eax == 0 && s) {
        uint32_t h = MEM32(s), ret = g_eax;
        unsigned i, k, n = 0;
        AcquireSRWLockExclusive(&s_cand);
        for (k = 0, i = ranura(h); k < TABLA; k++, i = (i + 1) & (TABLA - 1))
            if (!s_t[i].h || s_t[i].h == -1 || (uint32_t)s_t[i].h == h)
                break;
        if (k < TABLA) {
            /* the kernel count to 0 (the game's trywait): it only wakes sleepers now */
            while (n < valor && trywait_juego(s) == 0)
                n++;
            s_t[i].c = (LONG)n;
            InterlockedExchange(&s_t[i].h, (LONG)h);
        } else
            fprintf(stderr, "[SEM] tabla llena: %08X queda con las funciones del juego\n", h);
        ReleaseSRWLockExclusive(&s_cand);
        g_eax = ret;
    }
}

/* int sem_destroy(sem_t *s) */
void sub_002F1CC0(void)
{
    uint32_t s = MEM32(g_esp + 4);
    if (modo() && s) {
        sem *e;
        AcquireSRWLockExclusive(&s_cand);
        e = buscar(MEM32(s));
        if (e)
            InterlockedExchange(&e->h, -1);   /* a tombstone: the probe chain stays */
        ReleaseSRWLockExclusive(&s_cand);
    }
    sub_002F1CC0_pad();
}

/* int sem_post(sem_t *s) */
void sub_002F1D70(void)
{
    uint32_t s = MEM32(g_esp + 4);
    sem *e = modo() && s ? buscar(MEM32(s)) : NULL;
    if (!e) {
        sub_002F1D70_pad();
        return;
    }
    if (InterlockedIncrement(&e->c) <= 0) {
        /* one sleeps: the game's sem_post releases the kernel object for it */
        InterlockedIncrement(&s_post_despierta);
        sub_002F1D70_pad();
        return;
    }
    InterlockedIncrement(&s_post_rapido);
    g_eax = 0;
    g_esp += 4;
    informe();
}

/* int sem_wait(sem_t *s) */
void sub_002F1D30(void)
{
    uint32_t s = MEM32(g_esp + 4);
    sem *e = modo() && s ? buscar(MEM32(s)) : NULL;
    int64_t t0;
    if (!e) {
        sub_002F1D30_pad();
        return;
    }
    t0 = ahora();
    for (;;) {
        LONG v = e->c;
        if (v > 0 && InterlockedCompareExchange(&e->c, v - 1, v) == v) {
            InterlockedIncrement(&s_wait_rapido);
            g_eax = 0;
            g_esp += 4;
            return;
        }
        if (v <= 0 && ahora() - t0 >= s_giro)
            break;
        YieldProcessor();
    }
    if (InterlockedDecrement(&e->c) < 0) {
        /* none: sleep until a post releases the kernel object. The game's
         * sem_wait does that sleep: the same kernel semaphore, plus the
         * thread's cancel event (sub_002F2120 sets it; it ends the thread) */
        InterlockedIncrement(&s_wait_duerme);
        sub_002F1D30_pad();
        if (g_eax)                      /* an error: the unit was not taken */
            InterlockedIncrement(&e->c);
        return;
    }
    InterlockedIncrement(&s_wait_rapido);
    g_eax = 0;
    g_esp += 4;
}

/* int sem_trywait(sem_t *s) */
void sub_002F1CF0(void)
{
    uint32_t s = MEM32(g_esp + 4);
    sem *e = modo() && s ? buscar(MEM32(s)) : NULL;
    LONG v;
    if (!e) {
        sub_002F1CF0_pad();
        return;
    }
    InterlockedIncrement(&s_try);
    for (;;) {
        v = e->c;
        if (v <= 0)
            break;
        if (InterlockedCompareExchange(&e->c, v - 1, v) == v) {
            g_eax = 0;
            g_esp += 4;
            return;
        }
    }
    {
        uint32_t ret_esp = g_esp;
        errno_juego(0x0B);              /* EAGAIN */
        g_esp = ret_esp;
    }
    g_eax = 0xFFFFFFFFu;
    g_esp += 4;
}
