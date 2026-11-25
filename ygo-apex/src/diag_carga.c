/* diag_carga.c — 2026-11-21 (the user: "revisa si va lento algo, sobre todo
 * cuando se cierra la puerta luego de un duelo de torneo"). The 02:00 log:
 * the loads between screens take 150-557 ms in one frame with the disk at
 * 3-24 ms, texture uploads at 1-50 ms: the rest is the game's own code.
 * The loads read PNGs (cf_anim, texdb) through Alchemy's PNG loader
 * (sub_0017AF80 / sub_0017B1A0), libpng 1.0.8 (png_read_row sub_0028F180)
 * and zlib 1.1.3 (inflate sub_00297940), all recompiled. This measures how
 * much of a load each takes, before changing anything:
 *   [CARGA] png N: X ms (filas Y ms, zlib Z ms)
 * Diagnostic only; nothing changes in what the game does. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

void sub_0017AF80_pad(void);
void sub_0017B1A0_pad(void);
void sub_0028F180_pad(void);

static LARGE_INTEGER s_f;
static int s_dentro;                    /* inside a PNG load (nested entries) */
static int64_t s_filas, s_zlib;         /* ticks inside the current load */
static int64_t s_filtro, s_transf;      /* png_nativo.c: the row filter, the transformations */
static unsigned s_n;

static int64_t ahora(void)
{
    LARGE_INTEGER t;
    if (!s_f.QuadPart)
        QueryPerformanceFrequency(&s_f);
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

static double ms(int64_t t)
{
    return (double)t * 1000.0 / (double)s_f.QuadPart;
}

/* 2026-11-21 02:2x: the 02:15 log (the whole tournament): the PNGs are only
 * ~1/3 of each load (the door: 155 of 479 ms). The rest, sampled: a thread
 * looks at where the game's thread is ~1000 times a second; when a frame
 * takes 100 ms or more (no present in between), the functions it was in
 * are logged, the most seen first:
 *   [CARGA] cuadro de N ms (M muestras): sub_X 31%, sub_Y 12%, ...
 * The game's thread is the one that loads the first PNG. */
#include <dbghelp.h>
uint64_t nv2a_d3d11_presents(void);     /* xboxrecomp-apex nv2a_pb_d3d11.c */

/* 02:3x (the user chose "Ver las esperas"): in the boot and menu hitches the
 * loading thread waited 40-70 % of the time. Now every thread of the
 * process is sampled. When a sample is a wait (pc outside the exe: ntdll),
 * its stack is read upward for the first two exe addresses (the kit's wait
 * and who called it). Per slow frame, one line for the loading thread and
 * one for each thread that worked in it:
 *   [CARGA] cuadro N ms, hilo T (carga): M muestras, espera E% (kitfn<-quien 30%, ...), trabajo: sub_X 12%, ...
 * The thread ids match the [THREAD] lines of the log. */
#include <tlhelp32.h>
#include "recomp/gen/recomp_types.h"

/* 02:4x: the waits themselves, from the kit (g_xbox_wait_hook, kernel.h):
 * each one of 0.2 ms or more, summed per thread, kind, object and the guest
 * code that asked (the first three code words on the guest stack). Per slow
 * frame the ten longest:
 *   [CARGA] espera cuadro N ms: hilo T candado 00631A64 120.5 ms (14) desde 0001CA72<00016D12<00423979 */
typedef void (*xbox_wait_hook_fn)(int kind, uint32_t obj, int64_t t0, int64_t t1);
extern xbox_wait_hook_fn g_xbox_wait_hook;
extern uint32_t g_xbox_code_lo, g_xbox_code_hi;
#define ESPERAS 256
typedef struct { DWORD tid; int kind, n; uint32_t obj, a, b, c; int64_t t; } espera;
static espera s_es[ESPERAS];
static int s_nes, s_es_perdidas;
static SRWLOCK s_es_cand = SRWLOCK_INIT;
/* 02:5x: the 02:50 log had ~200 "cuadro 55 ms, 1 muestras" lines with no
 * [HITCH] for them: a round over every thread of the process (the driver's,
 * the audio's) took ~55 ms, so the sampler measured itself. Only the guest's
 * threads now: those that wait through the kit (this hook) and the loader. */
#define GUEST 16
static DWORD s_gt[GUEST];
static int s_ngt;

static void al_esperar(int kind, uint32_t obj, int64_t t0, int64_t t1)
{
    uint32_t sp = g_esp, w[3] = {0, 0, 0};
    DWORD tid = GetCurrentThreadId();
    int i, k = 0;
    if (!s_f.QuadPart || (t1 - t0) * 5000 < s_f.QuadPart)
        return;
    for (i = 0; i < 160 && k < 3 && sp; i++) {
        uint32_t v = MEM32(sp + (uint32_t)i * 4u);
        if (v >= g_xbox_code_lo && v < g_xbox_code_hi)
            w[k++] = v;
    }
    AcquireSRWLockExclusive(&s_es_cand);
    for (i = 0; i < s_ngt && s_gt[i] != tid; i++)
        ;
    if (i == s_ngt && s_ngt < GUEST)
        s_gt[s_ngt++] = tid;
    for (i = 0; i < s_nes; i++)
        if (s_es[i].tid == tid && s_es[i].kind == kind && s_es[i].obj == obj
                && s_es[i].a == w[0] && s_es[i].b == w[1])
            break;
    if (i == s_nes) {
        if (s_nes == ESPERAS)
            i = -1;
        else {
            espera *x = &s_es[s_nes++];
            memset(x, 0, sizeof *x);
            x->tid = tid;
            x->kind = kind;
            x->obj = obj;
            x->a = w[0];
            x->b = w[1];
            x->c = w[2];
        }
    }
    if (i >= 0) {
        s_es[i].t += t1 - t0;
        s_es[i].n++;
    } else
        s_es_perdidas++;
    ReleaseSRWLockExclusive(&s_es_cand);
}

extern volatile LONG g_xbox_spin_hits, g_xbox_spin_misses;   /* kernel_sync.c */

static void esperas(double dur, int informar_ya)
{
    static const char *k_tipo[] = {"objeto", "objetos", "candado", "pausa"};
    static LONG h0, m0;
    LONG h = g_xbox_spin_hits, m = g_xbox_spin_misses;
    int i, j, nmax;
    AcquireSRWLockExclusive(&s_es_cand);
    if (informar_ya && (h != h0 || m != m0))
        fprintf(stderr, "[CARGA] espera cuadro %.0f ms: giro: %ld respondidas despiertas, %ld que igual durmieron\n",
                dur, (long)(h - h0), (long)(m - m0));
    h0 = h;
    m0 = m;
    if (informar_ya) {
        for (i = 0; i < s_nes; i++)
            for (j = i + 1; j < s_nes; j++)
                if (s_es[j].t > s_es[i].t) {
                    espera t = s_es[i];
                    s_es[i] = s_es[j];
                    s_es[j] = t;
                }
        nmax = s_nes < 10 ? s_nes : 10;
        for (i = 0; i < nmax; i++)
            fprintf(stderr, "[CARGA] espera cuadro %.0f ms: hilo %lu %s %08X %.1f ms (%d) desde %08X<%08X<%08X\n",
                    dur, (unsigned long)s_es[i].tid, k_tipo[s_es[i].kind & 3], s_es[i].obj,
                    ms(s_es[i].t), s_es[i].n, s_es[i].a, s_es[i].b, s_es[i].c);
        if (s_es_perdidas)
            fprintf(stderr, "[CARGA] espera cuadro %.0f ms: %d sin lugar en la tabla\n", dur, s_es_perdidas);
    }
    s_nes = 0;
    s_es_perdidas = 0;
    ReleaseSRWLockExclusive(&s_es_cand);
}
#define HILOS 64
#define MUESTRAS 16384
#define PILA 6
typedef struct { uint8_t h; DWORD64 pc, pila[PILA]; } muestra;
static HANDLE s_hilo;
static DWORD s_tid_carga;
static struct { DWORD tid; HANDLE h; } s_h[HILOS];
static int s_nh;
static muestra s_m[MUESTRAS];
static DWORD64 s_ini, s_fin;            /* the exe's image */

static int en_exe(DWORD64 a) { return a >= s_ini && a < s_fin; }

static void hilos(void)
{
    HANDLE sn = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 te;
    DWORD pid = GetCurrentProcessId(), yo = GetCurrentThreadId();
    BOOL ok;
    int i;
    if (sn == INVALID_HANDLE_VALUE)
        return;
    te.dwSize = sizeof te;
    for (ok = Thread32First(sn, &te); ok; ok = Thread32Next(sn, &te)) {
        HANDLE h;
        if (te.th32OwnerProcessID != pid || te.th32ThreadID == yo)
            continue;
        if (te.th32ThreadID != s_tid_carga) {
            int j, es = 0;
            AcquireSRWLockShared(&s_es_cand);
            for (j = 0; j < s_ngt; j++)
                es |= s_gt[j] == te.th32ThreadID;
            ReleaseSRWLockShared(&s_es_cand);
            if (!es)
                continue;
        }
        for (i = 0; i < s_nh && s_h[i].tid != te.th32ThreadID; i++)
            ;
        if (i < s_nh || s_nh == HILOS)
            continue;
        h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, te.th32ThreadID);
        if (!h)
            continue;
        s_h[s_nh].tid = te.th32ThreadID;
        s_h[s_nh].h = h;
        s_nh++;
    }
    CloseHandle(sn);
}

/* 03:0x (the user: "anota todo para que en un solo duelo sepamos todo"):
 * each sample keeps its real call chain, unwound with the x64 unwind data
 * (RtlLookupFunctionEntry / RtlVirtualUnwind, no stack guessing): the first
 * PILA frames inside the exe. A leaf in a helper (sub_00375B70 was 20 %)
 * then says who called it, and a wait in ntdll which kit function and
 * which game code waited. Per slow frame and thread: the leaf functions,
 * and the most seen chains:
 *   [CARGA] cuadro N ms, hilo T: M muestras, espera E%, trabajo: sub_X 12%, ...
 *   [CARGA]   pila 31%: ZwWaitForAlertByThreadId<-kitfn<-sub_A<-sub_B<-sub_C
 * Only the guest's threads are sampled (a round over all of them stays
 * short); nothing allocates or locks while one is suspended (the unwind
 * reads the image's tables and the stack). */
static int ronda(int n)
{
    CONTEXT c;
    int i;
    for (i = 0; i < s_nh && n < MUESTRAS; i++) {
        muestra *m = &s_m[n];
        int f, k = 0;
        if (SuspendThread(s_h[i].h) == (DWORD)-1)
            continue;
        memset(&c, 0, sizeof c);
        c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
        if (GetThreadContext(s_h[i].h, &c)) {
            m->h = (uint8_t)i;
            m->pc = c.Rip;
            memset(m->pila, 0, sizeof m->pila);
            __try {                     /* a bad frame ends the chain, never the game */
            for (f = 0; f < 24 && k < PILA && c.Rip; f++) {
                DWORD64 img = 0, est = 0;
                PVOID hd = NULL;
                PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(c.Rip, &img, NULL);
                if (f > 0 && en_exe(c.Rip))
                    m->pila[k++] = c.Rip;
                if (!fn) {                  /* a leaf: its return address on top */
                    if (!c.Rsp)
                        break;
                    c.Rip = *(DWORD64 *)(uintptr_t)c.Rsp;
                    c.Rsp += 8;
                } else
                    RtlVirtualUnwind(UNW_FLAG_NHANDLER, img, c.Rip, fn, &c, &hd, &est, NULL);
            }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
            n++;
        }
        ResumeThread(s_h[i].h);
    }
    return n;
}

static const char *nombre(DWORD64 a, DWORD64 *ini)
{
    static char buf[sizeof(SYMBOL_INFO) + 128];
    SYMBOL_INFO *si = (SYMBOL_INFO *)buf;
    DWORD64 disp = 0;
    memset(buf, 0, sizeof buf);
    si->SizeOfStruct = sizeof(SYMBOL_INFO);
    si->MaxNameLen = 127;
    *ini = 0;
    if (!a || !SymFromAddr(GetCurrentProcess(), a, &disp, si))
        return "?";
    *ini = si->Address;
    return si->Name;
}

typedef struct { DWORD64 a, b; int c; char nom[200]; } cuenta;

static void sumar(cuenta *f, int *nf, int max, DWORD64 a, DWORD64 b, const char *nom)
{
    int j;
    for (j = 0; j < *nf && (f[j].a != a || f[j].b != b); j++)
        ;
    if (j == *nf) {
        if (*nf == max)
            return;
        f[j].a = a;
        f[j].b = b;
        f[j].c = 0;
        snprintf(f[j].nom, sizeof f[j].nom, "%s", nom);
        (*nf)++;
    }
    f[j].c++;
}

static void ordenar(cuenta *f, int nf)
{
    int i, j;
    for (i = 0; i < nf; i++)
        for (j = i + 1; j < nf; j++)
            if (f[j].c > f[i].c) {
                cuenta t = f[i];
                f[i] = f[j];
                f[j] = t;
            }
}

static void informar(int n, double dur)
{
    static cuenta ft[64], fp[64];
    char linea[1600];
    uint32_t estado = MEM32(0x00657148u);   /* the duel's state (danio.c: 6/7 in play) */
    int t, i;
    for (t = 0; t < s_nh; t++) {
        int tot = 0, esp = 0, ntr = 0, npi = 0, len;
        for (i = 0; i < n; i++) {
            const muestra *m = &s_m[i];
            DWORD64 a, h, clave;
            char k[200];
            size_t l;
            int j;
            if (m->h != t)
                continue;
            tot++;
            if (en_exe(m->pc)) {
                const char *nm = nombre(m->pc, &a);
                sumar(ft, &ntr, 64, a, 0, nm);
            } else
                esp++;
            /* the chain: the leaf, then up to 4 frames of the exe */
            snprintf(k, sizeof k, "%s", nombre(m->pc, &a));
            clave = a;
            for (j = 0; j < 4 && j < PILA && m->pila[j]; j++) {
                const char *nm = nombre(m->pila[j], &h);
                l = strlen(k);
                snprintf(k + l, sizeof k - l, "<-%s", nm);
                clave = clave * 1000003u ^ h;
            }
            sumar(fp, &npi, 64, clave, 0, k);
        }
        if (!tot)
            continue;
        /* the loading thread always; another only if it worked in the frame */
        if (s_h[t].tid != s_tid_carga && (tot - esp < 10 || (tot - esp) * 10 < tot))
            continue;
        ordenar(ft, ntr);
        ordenar(fp, npi);
        len = snprintf(linea, sizeof linea, "[CARGA] cuadro %.0f ms, hilo %lu%s, estado %u: %d muestras, espera %d%%, trabajo:",
                       dur, (unsigned long)s_h[t].tid, s_h[t].tid == s_tid_carga ? " (principal)" : "",
                       estado, tot, esp * 100 / tot);
        for (i = 0; i < ntr && i < 8 && len < (int)sizeof linea - 120; i++)
            len += snprintf(linea + len, sizeof linea - (size_t)len, " %s %d%%,",
                            ft[i].nom, ft[i].c * 100 / tot);
        fprintf(stderr, "%s\n", linea);
        for (i = 0; i < npi && i < 6; i++)
            if (fp[i].c * 20 >= tot)    /* 5 % or more */
                fprintf(stderr, "[CARGA]   pila %d%%: %s\n", fp[i].c * 100 / tot, fp[i].nom);
    }
}

static DWORD WINAPI muestreo(LPVOID arg)
{
    uint64_t p0 = nv2a_d3d11_presents();
    int64_t t0 = ahora(), tr = 0;
    int n = 0;
    (void)arg;
    {
        HMODULE ex = GetModuleHandleA(NULL);
        IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((char *)ex + ((IMAGE_DOS_HEADER *)ex)->e_lfanew);
        s_ini = (DWORD64)(uintptr_t)ex;
        s_fin = s_ini + nt->OptionalHeader.SizeOfImage;
    }
    hilos();
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    for (;;) {
        uint64_t p;
        Sleep(1);
        p = nv2a_d3d11_presents();
        if (p != p0) {
            double dur = ms(ahora() - t0);
            int lento = dur >= 20.0 && dur < 5000.0 && n > 0;   /* also the duel's small hitches (25-50 ms) */
            if (lento)
                informar(n, dur);
            esperas(dur, lento);
            n = 0;
            p0 = p;
            t0 = ahora();
            if (ms(t0 - tr) >= 500.0) {   /* new threads, between frames */
                hilos();
                tr = t0;
            }
        }
        n = ronda(n);
    }
    return 0;
}

static void png(void (*f)(void))
{
    int64_t t0;
    /* 2026-11-21 03:5x: the sampler and the wait hook did their job (the
     * fixes are in and proven); they suspend the guest's threads ~1000 times
     * a second, so they only run with Ajustes\diagnostico_cargas.txt. The
     * PNG lines stay (a clock read per load). */
    static int s_diag = -1;
    if (s_diag < 0) {
        char p[MAX_PATH];
        const char *dir = getenv("YGO_AJUSTES");
        snprintf(p, sizeof p, "%s%sdiagnostico_cargas.txt", dir && *dir ? dir : "", dir && *dir ? "\\" : "");
        s_diag = GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES;
        if (s_diag)
            fprintf(stderr, "[CARGA] diagnostico encendido (%s)\n", p);
    }
    if (s_diag && !s_hilo && DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(),
                                   &s_hilo, THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, 0)) {
        s_tid_carga = GetCurrentThreadId();
        ahora();                        /* s_f before the hook reads it */
        g_xbox_wait_hook = al_esperar;
        HANDLE h = CreateThread(NULL, 0, muestreo, NULL, 0, NULL);
        if (h)
            CloseHandle(h);
    }
    if (s_dentro) {
        f();
        return;
    }
    s_dentro = 1;
    s_filas = s_zlib = s_filtro = s_transf = 0;
    t0 = ahora();
    f();
    t0 = ahora() - t0;
    s_dentro = 0;
    s_n++;
    if (ms(t0) >= 2.0)
        fprintf(stderr, "[CARGA] png %u: %.1f ms (filas %.1f ms, zlib %.1f ms, filtro %.1f ms,"
                " transformaciones %.1f ms)\n",
                s_n, ms(t0), ms(s_filas), ms(s_zlib), ms(s_filtro), ms(s_transf));
}

void sub_0017AF80(void) { png(sub_0017AF80_pad); }
void sub_0017B1A0(void) { png(sub_0017B1A0_pad); }

static int s_en_fila;
void sub_0028F180(void)
{
    int64_t t0;
    if (s_en_fila) {
        sub_0028F180_pad();
        return;
    }
    s_en_fila = 1;
    t0 = ahora();
    sub_0028F180_pad();
    s_filas += ahora() - t0;
    s_en_fila = 0;
}

/* zlib_nativo.c: the time of each inflate (now native) */
void diag_carga_zlib(int64_t ticks)
{
    s_zlib += ticks;
}

/* png_nativo.c: 0 the row filter, 1 the row's transformations */
void diag_carga_png(int que, int64_t ticks)
{
    if (que)
        s_transf += ticks;
    else
        s_filtro += ticks;
}

/* 2026-11-21 (the 04:59 log: the 3D battle's frames 10-16 ms with the kit
 * drawing 4 ms of them): the game's D3D KickOff waiting for the walker
 * (sub_00375B70, 30 % of the main thread in the 04:22 profile), summed per
 * frame for the kit's [LENTO] line ("espera al kit"): high there, the
 * frame waits on the kit; low, on the game's own work. */
extern void sub_00375B70_pad(void);
extern volatile float g_nv2a_espera_ms;
void sub_00375B70(void)
{
    LARGE_INTEGER a, b, f;
    QueryPerformanceCounter(&a);
    sub_00375B70_pad();
    QueryPerformanceCounter(&b);
    QueryPerformanceFrequency(&f);
    g_nv2a_espera_ms += (float)((double)(b.QuadPart - a.QuadPart) * 1000.0 / (double)f.QuadPart);
}
