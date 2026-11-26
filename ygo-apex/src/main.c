/**
 * Yu-Gi-Oh! The Dawn of Destiny - Recompiled Game Entry Point
 *
 * This is the executable that hosts the recompiled game code, on Windows and
 * on POSIX hosts (macOS). One boot sequence, host_main, is shared; the parts
 * that differ by host (the crash handler, the fatal-error dialog, the window,
 * the entry points) sit under #if defined(_WIN32) / #else.
 * It performs the following initialization sequence:
 *
 * 1. Install the crash handler for diagnostics (VEH on Windows,
 *    SIGBUS/SIGSEGV elsewhere)
 * 2. Load the original XBE file from disk
 * 3. Initialize the Xbox memory layout (map data sections to original VAs)
 * 4. Initialize the Xbox kernel replacement layer
 * 5. Set up game file paths for I/O redirection
 * 6. Initialize the kernel bridge (thunk table in Xbox memory)
 * 7. Initialize the stack pointer
 * 8. Call the game's original entry point (recompiled)
 *
 * Customize this file for your game:
 *   - Set YOUR_GAME_ENTRY_POINT to the XBE entry point address
 *   - Set YOUR_GAME_XBE_PATH to where the XBE file lives
 *   - Set YOUR_GAME_DIR to the game data directory
 *   - Add any CRT global pre-initialization your game needs
 *   - Customize the crash report for game-specific crash diagnosis
 *
 * XBE Details (fill in from xbe_parser output):
 *   Title:       Yu-Gi-Oh! The Dawn of Destiny (PAL, XDK 5849, ID 0x4B4E001E, entry 0x0012405D)
 *   Title ID:    0x00000000
 *   Base addr:   0x00010000
 *   Entry point: 0x00000000
 *   Code size:   ~??? KB (.text)
 *   Sections:    ?? (list them)
 *   Kernel imports: ??
 */

#include "recomp_env.h"

#if defined(_WIN32)
#include <windows.h>
#include <dbghelp.h>
#else
/* <signal.h> already provides ucontext_t and the Darwin mcontext; including
 * <ucontext.h> directly is an #error on macOS without _XOPEN_SOURCE. The NT
 * types used below (BOOL, LONG, CALLBACK...) come from the toolkit's kernel.h
 * through <xbox/xboxrecomp.h>. */
#include <signal.h>
#include <dlfcn.h>
#include <unistd.h>
#include <pthread.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

/* xboxrecomp runtime headers */
#include <xbox/xboxrecomp.h>
#include "video/video_player.h"   /* xbox_HostWindowSetTitle, xbox_HostWindowMain */
#include "usb/ohci.h"              /* xbox_OhciInit: the pad's USB controller */
#include "input/keyboard.h"        /* xbox_InputBind: mouse buttons onto the pad */
#include "input/mouse.h"
#include "input/host_pad.h"     /* xbox_InputLastDevice */
#include "input/input_map.h"
#include "input/input_names.h"
#include "kernel/nv2a_backend_common.h"   /* nv2a_host_opts_set: render scale, aniso */
#include "kernel/nv2a_tex_pack.h"         /* the texture pack, without the enhance layer */
#include "d3d/host_menu.h"                /* xbox_MenuAdd: the pack's F2 rows */

static void pack_menu_init(void);  /* the F2 row "Texturas mejoradas" */
static void fast_mode_from_ini(void);  /* "Modo experimental", before the walker starts */
static void fast_menu_init(void);      /* its F2 row */
#ifdef RECOMP_ENV_HAVE_ENHANCE_KEYS   /* the toolkit's opt-in enhancements layer */
#include "enhance.h"
#include "enhance_cfg.h"
#endif

/*
 * If xboxrecomp.h is not an umbrella header in your setup, include
 * the individual headers directly:
 *
 * #include "kernel.h"
 * #include "xbox_memory_layout.h"
 * #include "d3d8_xbox.h"
 * #include "dsound_xbox.h"
 * #include "xinput_xbox.h"
 */

/* ── Global register state (defined in xbox_memory_layout.c) ── */

/* RECOMP_TLS is not optional here. The runtime defines these thread-local, and
 * a plain `extern` referencing a __declspec(thread) variable does not resolve
 * to the calling thread's copy -- it resolves to the image's TLS template. The
 * host side then writes g_esp somewhere the generated code never reads, so the
 * guest starts with every register at zero and faults immediately, having
 * apparently ignored the setup that visibly ran. */
extern RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp;
extern RECOMP_TLS uint32_t g_ebx, g_esi, g_edi;
extern RECOMP_TLS uint32_t g_seh_ebp;
/* x87 and SSE state. Global for the same reason the volatile GPRs are: one
 * guest routine can lift to several C functions, so a value written in one
 * body is read in the next. Defined in xbox_memory_layout.c like the rest of
 * the register file. */
extern RECOMP_TLS double g_fp_stack[8];
extern RECOMP_TLS int g_fp_top;
extern RECOMP_TLS uint16_t g_fp_control_word;
extern RECOMP_TLS uint32_t g_mxcsr;
extern RECOMP_TLS int g_fp_cmp;
extern RECOMP_TLS RecompXmm g_xmm0, g_xmm1, g_xmm2, g_xmm3;
extern RECOMP_TLS RecompXmm g_xmm4, g_xmm5, g_xmm6, g_xmm7;
extern ptrdiff_t g_xbox_mem_offset;

/* ── XBE Constants ─────────────────────────────────────────── */

/*
 * TODO: Set these from your xbe_parser output.
 * Run: py -3 -m tools.xbe_parser game/default.xbe
 *
 * YOUR_GAME_XBE_PATH and YOUR_GAME_DIR are the defaults for a developer run
 * from the project root. An installed game's launcher sets RECOMP_GAME_FILES
 * (the game files) and RECOMP_HDD_DIR (the emulated hard disk) instead, and
 * those win (host_main, step 2).
 */
#define YOUR_GAME_ENTRY_POINT   0x0012405D  /* XBE entry point VA */
/* Relative to ygo-apex (run.bat's working directory): ..\xbe holds the XBE
 * the code was generated from, ..\..\Yu-Gi-Oh TDOD the game folder. */
#define YOUR_GAME_XBE_PATH      "..\\xbe\\default.xbe"
#define YOUR_GAME_DIR            "..\\..\\Yu-Gi-Oh TDOD"

/* ── Forward declarations ──────────────────────────────────── */

static BOOL load_xbe(const char *path, void **out_data, size_t *out_size);

/* Recompiled entry point (generated by recomp pipeline) */
extern void xbe_entry_point(void);

/* Flat dispatch table builder (generated, recomp_dispatch.c): declared here
 * because main.c does not include recomp_types.h, and Clang rejects the
 * implicit declaration MSVC only warned about. */
extern int recomp_dispatch_init(void);

/* The emulated APU.
 *
 * Two entry points that had no callers anywhere in the tree. apu_mmio_hook.c
 * documents apu_hook_handle_mmio as "called from VEH in main.c" and no main.c
 * called it; apu_decode_and_handle's first line is `if (!g_apu_state) return
 * false` and nothing assigned g_apu_state. So with RECOMP_AC97_READY set the
 * APU's registers were unmapped to be trapped, and every trap was then
 * declined and surfaced as an access violation on the first register
 * DirectSound touched. The crash handlers below now reach the APU model
 * through the toolkit's xbox_VehMmioFault (Windows) and xbox_PosixMmioFault
 * (POSIX).
 *
 * Declared rather than included so this does not depend on src/apu being on
 * the include path. */
typedef struct MCPXAPUState MCPXAPUState;
extern MCPXAPUState *mcpx_apu_init_standalone(uint8_t *ram_ptr);
extern MCPXAPUState *g_apu_state;

/* ── Crash reporting (shared by every host) ────────────────── */

/* Fault access kinds handed to report_crash. */
#define FAULT_READ     0
#define FAULT_WRITE    1
#define FAULT_UNKNOWN (-1)

/* Label for the host program counter in the [CRASH] line: the register's own
 * name, so the number can be pasted straight into a debugger. */
#if defined(__aarch64__) || defined(_M_ARM64)
#define HOST_PC_LABEL "PC"
#else
#define HOST_PC_LABEL "RIP"
#endif

/* All crash output goes through here.
 *
 * On POSIX this runs inside a signal handler, possibly while the faulting
 * thread holds stdio's lock, so it formats into a stack buffer and write()s
 * instead of going through fprintf. On Windows the VEH has always used
 * fprintf(stderr), and still does. */
static void crash_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
#if defined(_WIN32)
    vfprintf(stderr, fmt, ap);
#else
    {
        char buf[512];
        int n = vsnprintf(buf, sizeof(buf), fmt, ap);
        if (n > 0) {
            size_t len = (size_t)n < sizeof(buf) ? (size_t)n : sizeof(buf) - 1;
            ssize_t r = write(STDERR_FILENO, buf, len);
            (void)r;
        }
    }
#endif
    va_end(ap);
}

/* Name the host symbol containing pc, as "name+0xoff". Returns false, with
 * "<unknown>" in out, when there is none.
 *
 * Windows: dbghelp against the PDB (SymInitialize runs at install time).
 * Elsewhere: dladdr, which on macOS reads the executable's own symbol table --
 * static functions included, no -export_dynamic needed -- as long as the
 * binary is not stripped. dli_sname has no leading underscore. */
static bool host_symbol_for(void *pc, char *out, size_t n)
{
#if defined(_WIN32)
    /* SYMBOL_INFO is variable-length: the name is written past the struct, so
     * it must be over-allocated with MaxNameLen set to the slack. */
    char buf[sizeof(SYMBOL_INFO) + 256];
    SYMBOL_INFO *sym = (SYMBOL_INFO *)buf;
    DWORD64 disp = 0;

    memset(buf, 0, sizeof(buf));
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen = 255;
    if (SymFromAddr(GetCurrentProcess(), (DWORD64)(uintptr_t)pc, &disp, sym)) {
        snprintf(out, n, "%s+0x%llX", sym->Name, (unsigned long long)disp);
        return true;
    }
#else
    Dl_info info;

    if (dladdr(pc, &info) && info.dli_sname && info.dli_saddr) {
        snprintf(out, n, "%s+0x%llX", info.dli_sname,
                 (unsigned long long)((uintptr_t)pc - (uintptr_t)info.dli_saddr));
        return true;
    }
#endif
    snprintf(out, n, "<unknown>");
    return false;
}

/* Name the guest function a fault happened in, and recover the call chain.
 *
 * Recompiled code faults as ordinary native code, so the fault carries a host
 * PC and nothing else -- there is no guest program counter to report, and the
 * host address changes every build. Two things recover the guest view:
 *
 *   - every generated function is a real symbol in the image (sub_005A03C0
 *     and so on), so the symbol table (the linker's PDB on Windows) already
 *     maps host address back to guest function. host_symbol_for turns an
 *     anonymous PC into that name.
 *
 *   - every lifted call pushes its guest return address onto the guest stack
 *     before jumping, so the stack still holds the chain. Scanning up from esp
 *     for values inside the code sections recovers it.
 *
 * ponytail: the stack scan is a scan, not a frame walk -- these are FPO frames
 * with no reliable ebp chain, so there is nothing to walk. It over-reports,
 * since addresses from returned-from calls linger below esp, but naming the
 * guest function is the whole question at a fault.
 *
 * Requires an unstripped executable: on Windows, linking dbghelp and keeping
 * the .pdb beside the .exe.
 */
static void print_guest_context(void *pc)
{
    char name[300];

    if (host_symbol_for(pc, name, sizeof(name)))
        crash_printf("  in %s\n", name);

    if (g_xbox_mem_offset && g_esp) {
        const uint32_t *sp =
            (const uint32_t *)((uintptr_t)g_xbox_mem_offset + g_esp);
        int shown = 0, i;
        crash_printf("  guest stack (return addresses, innermost first):\n");
        for (i = 0; i < 256 && shown < 24; i++) {
            uint32_t v = sp[i];
            if (v > g_xbox_code_lo && v < g_xbox_code_hi) {
                crash_printf("    [esp+%-4d] 0x%08X\n", i * 4, v);
                shown++;
            }
        }
        /* When the frame is gone, say so, and fall back on what is left.
         *
         * A fault inside a block copy leaves no return address in range: the
         * copy is a host memmove and its caller's frame is further up than
         * this reaches. Worse, a copy that ran past its buffer overwrites the
         * stack itself, so the list comes out empty exactly when it is most
         * needed -- and an empty list under a promising heading reads like
         * "nothing to see" rather than "the evidence is destroyed". */
        if (!shown)
            crash_printf("    (no return addresses in range)\n");

        /* Everything below only pays for itself when the scan came back
         * empty. A crash where the chain was recovered already says what
         * happened, and burying that under another twenty lines makes the
         * common report worse to read. */
        if (!shown) {
            uint32_t k;

            /* The indirect-call history, which the stack cannot overwrite.
             * recomp_manual.c prints this too, but only from
             * recomp_icall_fail_log -- a page fault inside a block copy
             * never reaches that path, and this is the same information by
             * the route the fault actually took.
             *
             * Declared here rather than included: main.c does not pull in
             * recomp_types.h, and recomp_manual.c spells it the same way. */
            extern volatile uint32_t g_icall_trace[16];
            extern volatile uint32_t g_icall_trace_idx;

            crash_printf("  recent ICALL targets:");
            for (k = 0; k < 16; k++)
                crash_printf(" %08X",
                             g_icall_trace[(g_icall_trace_idx + k) & 15]);
            crash_printf("\n");

            /* The frame unfiltered. The pointers and lengths handed to
             * whatever faulted live here and look nothing like code, so the
             * filter above drops exactly what is wanted. On the crash this
             * was written for, one of these words was the end of the mapped
             * RAM mirror -- which is what identified the fault as a copy
             * running off its buffer rather than a stray pointer. */
            for (i = 0; i < 20; i++)
                crash_printf("    raw[esp+%-4d] 0x%08X\n", i * 4, sp[i]);
        }
    }
}

/*
 * The host-independent part of a crash report: what faulted, the guest
 * registers, and the guest function and call chain. Each host's handler
 * decodes its own fault record, calls this, then appends its native stack.
 * `what` is "Access violation" on Windows and the signal's name elsewhere.
 *
 * This is your primary debugging tool during bring-up. Customize it for your
 * game:
 *   - Add game-specific address checks (GPU register probes, etc.)
 *   - Add dumps of game-specific globals (heap handles, state flags)
 *   - Add SEH simulation if your game uses __try/__except
 */
static void report_crash(const char *what, void *host_pc, uintptr_t fault_addr,
                         int access)
{
    uintptr_t va = fault_addr - (uintptr_t)g_xbox_mem_offset;

    crash_printf("[CRASH] %s at " HOST_PC_LABEL "=0x%llX, fault addr=0x%llX (%s)\n",
        what,
        (unsigned long long)(uintptr_t)host_pc,
        (unsigned long long)fault_addr,
        access == FAULT_WRITE ? "write"
        : access == FAULT_READ ? "read" : "unknown");
    crash_printf("  Xbox regs: eax=0x%08X ecx=0x%08X edx=0x%08X esp=0x%08X\n",
        g_eax, g_ecx, g_edx, g_esp);
    crash_printf("  Xbox regs: ebx=0x%08X esi=0x%08X edi=0x%08X\n",
        g_ebx, g_esi, g_edi);
#if !defined(_WIN32)
    /* A signal can come from a host pointer (a toolkit bug, a NULL) as well
     * as from guest memory; a "guest VA" for that would be noise. */
    if (va > 0xFFFFFFFFu)
        crash_printf("  Xbox VA of fault: (host address, outside guest memory)\n");
    else
#endif
    crash_printf("  Xbox VA of fault: 0x%08X\n", (uint32_t)va);
    print_guest_context(host_pc);

    /*
     * TODO: Add game-specific diagnostics here. Examples:
     *
     * Dump CRT heap handle:
     *   uint32_t heap = *(uint32_t *)((uint8_t *)g_xbox_mem_offset + HEAP_HANDLE_VA);
     *   crash_printf("  CRT heap handle: 0x%08X\n", heap);
     *
     * Dump game state:
     *   uint32_t state = *(uint32_t *)((uint8_t *)g_xbox_mem_offset + GAME_STATE_VA);
     *   crash_printf("  Game state: %u\n", state);
     */
}

#if defined(_WIN32)

/* ── Windows: VEH crash handler ────────────────────────────── */

/*
 * Vectored Exception Handler for crash diagnostics.
 *
 * When the recompiled game hits an access violation, this handler prints
 * the faulting address, all Xbox register values, and a native stack trace.
 */
static LONG CALLBACK veh_handler(PEXCEPTION_POINTERS ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        uintptr_t fault_addr = ep->ExceptionRecord->ExceptionInformation[1];

        /*
         * GPU register probe at 0xFD000000 range.
         * Some games probe NV2A registers directly. On real hardware this
         * returns GPU state; here we just skip the instruction.
         * TODO: Implement mini x86-64 decoder for instruction skipping,
         * or connect to the xbox_nv2a library for proper handling.
         */
        if (fault_addr >= 0xFD000000 && fault_addr < 0xFE000000) {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        /* Trapped hardware registers (APU, USB OHCI) -> their models.
         *
         * The other half of RECOMP_AC97_READY: reporting the codec ready is
         * what lets DirectSoundCreate past its first gate, and from there it
         * drives the APU directly, so the registers have to fault to be seen.
         * "Handled" means the access was decoded and the instruction stepped
         * over, so execution resumes instead of unwinding. */
        if (xbox_VehMmioFault(ep))
            return EXCEPTION_CONTINUE_EXECUTION;

        report_crash("Access violation", (void *)ep->ContextRecord->Rip,
                     fault_addr,
                     ep->ExceptionRecord->ExceptionInformation[0]
                         ? FAULT_WRITE : FAULT_READ);

        /* Print native stack return addresses for debugging */
        {
            uintptr_t *sp = (uintptr_t *)ep->ContextRecord->Rsp;
            fprintf(stderr, "  Native stack (first 8 return addrs):\n");
            for (int i = 0; i < 64 && sp[i]; i++) {
                if (sp[i] >= 0x140000000ULL && sp[i] < 0x150000000ULL) {
                    fprintf(stderr, "    [%d] 0x%llX\n", i, (unsigned long long)sp[i]);
                }
            }
        }
        fflush(stderr);
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

/* ── RDR_ZWATCH: who reads the depth buffer? ─────────────────────────────
 *
 * Diagnostic for the aiming bug (shots land at the top right, not under the
 * crosshair). The D3D11 backend keeps colour and depth on the host GPU and
 * never writes them back to guest memory, so a title that reads its depth
 * buffer under the crosshair would read zeroes. RDR_ZWATCH=<va>,<bytes>
 * (default the 640x480 zeta at 0x80B14000) arms PAGE_GUARD on that range
 * every 50 ms and logs each distinct host instruction that touches it, with
 * the generated function it is in and the guest return addresses on the
 * stack. Silent when nothing reads it. */
extern RECOMP_TLS uint32_t g_esp;
static uint8_t *s_zw_base;
static size_t   s_zw_size;

static LONG CALLBACK zwatch_veh(PEXCEPTION_POINTERS ep)
{
    enum { SITES = 32 };
    static uintptr_t seen[SITES];
    static int nseen;
    const EXCEPTION_RECORD *er = ep->ExceptionRecord;
    uintptr_t addr, rip;
    int i;

    if (er->ExceptionCode != STATUS_GUARD_PAGE_VIOLATION || er->NumberParameters < 2)
        return EXCEPTION_CONTINUE_SEARCH;
    addr = (uintptr_t)er->ExceptionInformation[1];
    if (addr < (uintptr_t)s_zw_base || addr >= (uintptr_t)s_zw_base + s_zw_size)
        return EXCEPTION_CONTINUE_SEARCH;
    rip = (uintptr_t)ep->ContextRecord->Rip;
    for (i = 0; i < nseen && seen[i] != rip; i++)
        ;
    if (i == nseen && nseen < SITES) {
        char buf[sizeof(SYMBOL_INFO) + 128];
        SYMBOL_INFO *si = (SYMBOL_INFO *)buf;
        DWORD64 disp = 0;
        uint32_t esp = g_esp, k, shown = 0;

        seen[nseen++] = rip;
        memset(buf, 0, sizeof buf);
        si->SizeOfStruct = sizeof(SYMBOL_INFO);
        si->MaxNameLen = 127;
        fprintf(stderr, "[ZWATCH] %s guest 0x%08X (+0x%X into the buffer) from %s+0x%llX\n",
                er->ExceptionInformation[0] ? "write" : "read",
                (unsigned)(addr - (uintptr_t)g_xbox_mem_offset),
                (unsigned)(addr - (uintptr_t)s_zw_base),
                SymFromAddr(GetCurrentProcess(), rip, &disp, si) ? si->Name : "?",
                (unsigned long long)disp);
        for (k = 0; k < 256 && shown < 8; k++) {
            uint32_t v = *(volatile uint32_t *)((uint8_t *)g_xbox_mem_offset + esp + 4u * k);
            if (v >= 0x00011000u && v < 0x00338620u) {
                fprintf(stderr, "[ZWATCH]   [esp+%03X] 0x%08X\n", 4u * k, v);
                shown++;
            }
        }
        fflush(stderr);
    }
    return EXCEPTION_CONTINUE_EXECUTION;   /* the guard bit is already cleared */
}

static DWORD WINAPI zwatch_thread(LPVOID unused)
{
    (void)unused;
    for (;;) {
        DWORD old;
        VirtualProtect(s_zw_base, s_zw_size, PAGE_READWRITE | PAGE_GUARD, &old);
        Sleep(50);
    }
}

static void zwatch_start(void)
{
    const char *e = getenv("RDR_ZWATCH");
    uint32_t va = 0x80B14000u, bytes = 640u * 480u * 4u;
    HANDLE th;

    if (!e)
        return;
    if (*e && *e != '1')
        sscanf(e, "%x,%u", &va, &bytes);
    s_zw_base = (uint8_t *)g_xbox_mem_offset + va;
    s_zw_size = bytes;
    AddVectoredExceptionHandler(1, zwatch_veh);
    th = CreateThread(NULL, 0, zwatch_thread, NULL, 0, NULL);
    if (th)
        CloseHandle(th);
    fprintf(stderr, "[ZWATCH] guarding guest 0x%08X, %u bytes\n", va, bytes);
}

/* ── Mouse and keys ──────────────────────────────────────────────────── */

/* rdr-port: the keys the player gave each Xbox button in Configuración.exe
 * ("Asignar teclas del mando"), in controles.ini next to the exe:
 *     a=k32,k90        k<VK> a key, m<n> a mouse input (1 left, 2 right,
 *     rt=m1            3 middle, 4/5 side buttons, 6/7 wheel up/down; mw the
 *                      wheel both ways). One key a button since 2026-11-06.
 * A control listed there loses the defaults above and gets exactly those
 * (an empty value leaves it unbound); one not listed keeps them. The names
 * are input_map.h's InputControl order. */
static void rdr_controls_load(void)
{
    static const char *const names[INPUT_CONTROL_COUNT] = {
        "dpad_up", "dpad_down", "dpad_left", "dpad_right", "start", "back", "lthumb", "rthumb",
        "a", "b", "x", "y", "black", "white", "lt", "rt",
        "ls_up", "ls_down", "ls_left", "ls_right", "rs_up", "rs_down", "rs_left", "rs_right",
    };
    char line[256];
    int n = 0;
    FILE *f = fopen("controles.ini", "r");
    if (!f)
        return;
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '='), *tok, *save = NULL;
        int c;
        if (line[0] == '#' || !eq)
            continue;
        *eq = 0;
        eq[1 + strcspn(eq + 1, "\r\n")] = 0;
        for (c = 0; c < INPUT_CONTROL_COUNT && strcmp(names[c], line); c++)
            ;
        if (c == INPUT_CONTROL_COUNT)
            continue;
        xbox_InputBindingsClear(c);
        for (tok = strtok_s(eq + 1, ", ", &save); tok; tok = strtok_s(NULL, ", ", &save)) {
            int code = atoi(tok + 1);
            if (!strcmp(tok, "mw")) {               /* the wheel, both ways */
                xbox_InputBind(c, XBOX_MOUSE_WHEEL_UP, 1);
                xbox_InputBind(c, XBOX_MOUSE_WHEEL_DOWN, 1);
            } else if (tok[0] == 'k' || tok[0] == 'm')
                xbox_InputBind(c, code, tok[0] == 'm');
        }
        n++;
    }
    fclose(f);
    fprintf(stderr, "[RDR] controls: %d buttons from controles.ini\n", n);
}

/* ygo-port: the mouse for a card game (ratón nivel 1). Motion moves the
 * game's cursor a square at a time (mando_directo.c steps the d-pad from the
 * raw counts, so no stick gets any), the left button confirms (A), the right
 * one cancels (B), the middle one is Y and the wheel goes up and down the
 * lists (d-pad). Space is also A and Esc also B, as in a PC card game; Enter
 * stays START and Tab is BACK. controles.ini still overrides any button.
 * YGO_MOUSE_MODE = puntero | pasos | clics (raton.c); YGO_MOUSE_STEP (counts a square,
 * default 70) is the sensitivity. Both are F2 rows too. */
void ygo_mouse_mode_set(int mode);  /* raton.c: 0 game cursor, 1 pointer, 2 clicks */
int  ygo_mouse_mode_get(void);
void ygo_mouse_step_set(int counts);
void ygo_cursor_scan_start(void);   /* buscar_cursor.c */

static void rdr_mouse_init(void)
{
    xbox_mouse_opts o;
    const char *e;

    if (!xbox_InputMouseOn())
        return;
    xbox_InputMouseGetOpts(&o);
    o.look = XBOX_MOUSE_LOOK_NONE;
    xbox_InputMouseOpts(&o);
    xbox_InputMouseDirect(1);           /* raw counts to mando_directo.c */
    if ((e = getenv("YGO_MOUSE_MODE")) && *e)
        ygo_mouse_mode_set(!_stricmp(e, "juego") ? 1 : !_stricmp(e, "clics") ? 2 : 0);
    else
        ygo_mouse_mode_set(0);
    if ((e = getenv("YGO_MOUSE_STEP")) && *e)
        ygo_mouse_step_set(atoi(e));

    xbox_InputBindingsStage();          /* start from the compiled-in key map */
    xbox_InputBind(INPUT_A, XBOX_MOUSE_LEFT, 1);
    xbox_InputBind(INPUT_B, XBOX_MOUSE_RIGHT, 1);
    xbox_InputBind(INPUT_Y, XBOX_MOUSE_MIDDLE, 1);
    xbox_InputBind(INPUT_DPAD_UP, XBOX_MOUSE_WHEEL_UP, 1);
    xbox_InputBind(INPUT_DPAD_DOWN, XBOX_MOUSE_WHEEL_DOWN, 1);
    xbox_InputBind(INPUT_A, 0x20 /* VK_SPACE */, 0);
    xbox_InputBind(INPUT_B, 0x1B /* VK_ESCAPE */, 0);
    xbox_InputBind(INPUT_BACK, 0x09 /* VK_TAB */, 0);
    rdr_controls_load();
    xbox_InputBindingsCommit();
    fprintf(stderr, "[YGO] mouse: mode %d (0 game cursor, 1 pointer), LMB A, RMB B, MMB Y, "
                    "wheel up/down; Space A, Esc B, Tab back\n", ygo_mouse_mode_get());
}

/* ── Graphics: the GPU backend's host options without the enhance layer ──
 *
 * The D3D11 backend honours nv2a_host_opts (internal resolution, anisotropic
 * filtering, present filter, aspect, fullscreen), which only the opt-in
 * enhancements layer fills. Fill them here before the guest starts, from
 * env.bat:
 *   RDR_RENDER_SCALE  1..4   internal resolution factor (default 2: 1280x960)
 *   RDR_ANISO         0..16  anisotropic filtering (default 16)
 *   RDR_FILTER        nearest | linear | integer (default linear)
 *   RDR_ASPECT        4:3 | 16:9 | 16:10 | 21:9 (default 4:3, the stock frame)
 *   RDR_FULLSCREEN    1 starts borderless fullscreen (default 0)
 * snap_2d keeps 2D batches (the HUD, text) on the guest pixel grid above
 * scale 1, so they do not pick up seams from the larger target. */
static void rdr_graphics_init(void)
{
    struct nv2a_host_opts o;
    const char *e;
    unsigned aw = 0, ah = 0;

    memset(&o, 0, sizeof o);
    o.render_scale = 2;
    o.aniso = 16;
    /* Sharp scaling and no pixel-aligned HUD, fixed: their F2 rows are gone
     * (the user's choice, 2026-11-02). Pause on focus loss stays on. */
    o.present_filter = NV2A_PRESENT_NEAREST;
    /* ygo-port: off by default since 2026-11-08 (afternoon). snap_2d samples
     * every flat 2D batch at the 1x frame's points (nv2a_snap_uv2d: uv moved
     * to each N x N block's centre), so the cards, menus, text and board, most
     * of this game, looked exactly 480p at any internal resolution: "no sube
     * la resolución de verdad". It was on for the dark line down the middle of
     * the dialogue box (two mirrored halves whose meeting column samples past
     * the texture edge above scale 1). Off, those lines came back (12:52:
     * the yes/no box, the name entry, the speech bubble). So the default is
     * mode 2: host-rate sampling with only a half-texel clamp at the
     * texture's edge (nv2a_uv_snap, nv2a_edge). F2 "Imagen 2D" or
     * YGO_SNAP_2D=0/1/2 pick off / the 1x grid / mode 2. */
    o.snap_2d = (e = getenv("YGO_SNAP_2D")) && *e >= '0' && *e <= '2' ? *e - '0' : 2;
    if ((e = getenv("RDR_RENDER_SCALE")) && *e)
        o.render_scale = (unsigned)atoi(e);
    if (o.render_scale < 1) o.render_scale = 1;
    if (o.render_scale > 4) o.render_scale = 4;
    if ((e = getenv("RDR_ANISO")) && *e)
        o.aniso = (unsigned)atoi(e);
    if (o.aniso > 16) o.aniso = 16;
    if ((e = getenv("RDR_ASPECT")) && sscanf(e, "%u:%u", &aw, &ah) == 2
            && aw && ah && !(aw * 3 == ah * 4)) {
        o.aspect_w = aw;
        o.aspect_h = ah;
    }
    if ((e = getenv("RDR_FULLSCREEN")) && *e == '1')
        o.fullscreen = 1;
    /* ygo-port: the game always starts in a window (the user's choice,
     * 2026-11-08); F11 / Alt+Enter or F2 switch to fullscreen while playing.
     * F2's saved "Pantalla completa" is applied at the first flip, before the
     * window exists, so it never took effect at start, which is what the user
     * wants. */
    nv2a_host_opts_set(&o);
    fprintf(stderr, "[RDR] graphics: scale %ux (%ux%u), aniso %u, filter %s, aspect %s%s\n",
            o.render_scale, 640u * o.render_scale, 480u * o.render_scale, o.aniso,
            o.present_filter == NV2A_PRESENT_NEAREST ? "nearest"
            : o.present_filter == NV2A_PRESENT_INTEGER ? "integer" : "linear",
            o.aspect_w ? e : "4:3", o.fullscreen ? ", fullscreen" : "");
}

/* ── RDR_SCAN: find a counter that steps down by one ────────────────────
 *
 * RDR_SCAN=t1,t2,t3 (seconds since start) snapshots guest RAM three times
 * and prints every address whose value went down by exactly one at each
 * step, as int32, int16 or float. With a shot between snapshots that is the
 * ammo counter, whose writer is the firing code. Reads go through
 * ReadProcessMemory so unmapped guest pages are skipped, not faulted on. */
typedef struct { uint32_t va, len; } ScanRange;
static const ScanRange s_scan_ranges[] = {
    { 0x0040D8A0u, 0x0081CAE0u - 0x0040D8A0u },   /* .data and BSS */
    { 0x010C0000u, 0x04000000u - 0x010C0000u },   /* runtime heap */
    { 0x80000000u, 0x04000000u },                 /* contiguous window */
};

static uint8_t *scan_snapshot(void)
{
    size_t total = 0, off = 0, r;
    uint8_t *buf;
    for (r = 0; r < sizeof s_scan_ranges / sizeof s_scan_ranges[0]; r++)
        total += s_scan_ranges[r].len;
    buf = (uint8_t *)calloc(1, total);
    if (!buf)
        return NULL;
    for (r = 0; r < sizeof s_scan_ranges / sizeof s_scan_ranges[0]; r++) {
        uint32_t k;
        for (k = 0; k < s_scan_ranges[r].len; k += 0x10000u) {
            SIZE_T got = 0;
            uint32_t n = s_scan_ranges[r].len - k < 0x10000u ? s_scan_ranges[r].len - k : 0x10000u;
            ReadProcessMemory(GetCurrentProcess(),
                              (uint8_t *)g_xbox_mem_offset + s_scan_ranges[r].va + k,
                              buf + off + k, n, &got);
        }
        off += s_scan_ranges[r].len;
    }
    return buf;
}

static DWORD WINAPI scan_thread(LPVOID arg)
{
    unsigned t[3] = { 0, 0, 0 }, i;
    uint8_t *snap[3];
    DWORD start = GetTickCount();
    int found = 0;
    size_t off = 0, r;

    sscanf((const char *)arg, "%u,%u,%u", &t[0], &t[1], &t[2]);
    for (i = 0; i < 3; i++) {
        while (GetTickCount() - start < t[i] * 1000u)
            Sleep(20);
        snap[i] = scan_snapshot();
        fprintf(stderr, "[SCAN] snapshot %u at %us\n", i, t[i]);
        if (!snap[i])
            return 0;
    }
    for (r = 0; r < sizeof s_scan_ranges / sizeof s_scan_ranges[0]; r++) {
        uint32_t k;
        for (k = 0; k + 4 <= s_scan_ranges[r].len; k += 2) {
            const uint8_t *a = snap[0] + off + k, *b = snap[1] + off + k, *c = snap[2] + off + k;
            int32_t ia, ib, ic; int16_t sa, sb, sc; float fa, fb, fc;
            const char *kind = NULL;
            memcpy(&ia, a, 4); memcpy(&ib, b, 4); memcpy(&ic, c, 4);
            memcpy(&sa, a, 2); memcpy(&sb, b, 2); memcpy(&sc, c, 2);
            memcpy(&fa, a, 4); memcpy(&fb, b, 4); memcpy(&fc, c, 4);
            if (!(k & 3) && ia > 1 && ia < 100000 && ib == ia - 1 && ic == ib - 1)
                kind = "int32";
            else if (sa > 1 && sb == sa - 1 && sc == sb - 1)
                kind = "int16";
            else if (!(k & 3) && fa > 1.5f && fa < 100000.f && fb == fa - 1.f && fc == fb - 1.f)
                kind = "float";
            if (kind && found++ < 60)
                fprintf(stderr, "[SCAN] 0x%08X %s %d -> %d -> %d\n",
                        s_scan_ranges[r].va + k, kind,
                        kind[0] == 'f' ? (int)fa : kind[3] == '1' ? sa : ia,
                        kind[0] == 'f' ? (int)fb : kind[3] == '1' ? sb : ib,
                        kind[0] == 'f' ? (int)fc : kind[3] == '1' ? sc : ic);
        }
        off += s_scan_ranges[r].len;
    }
    fprintf(stderr, "[SCAN] %d candidate(s)\n", found);
    fflush(stderr);
    return 0;
}

static void scan_start(void)
{
    const char *e = getenv("RDR_SCAN");
    HANDLE th;
    if (!e || !*e)
        return;
    th = CreateThread(NULL, 0, scan_thread, (LPVOID)e, 0, NULL);
    if (th)
        CloseHandle(th);
}

/* rdr-port: a fault nobody handled. Windows froze every thread while it
 * built its report, so the game looked alive but ignored every key, and the
 * player could not tell. Now: a line in the log, a notice, and out. Only the
 * truly unhandled ones reach here (the VEH above sees first-chance faults,
 * some of which a DLL handles itself). */
void ygo_caja_volcar(const char *por);   /* caja_negra.c */
static LONG WINAPI unhandled_filter(PEXCEPTION_POINTERS ep)
{
    wchar_t msg[256];
    fprintf(stderr, "[CRASH] unhandled 0x%08lX: closing the game\n",
            (unsigned long)ep->ExceptionRecord->ExceptionCode);
    fflush(stderr);
    ygo_caja_volcar("falla del juego");
    {
        extern void ygo_marcar_error(const char *motivo);   /* perfil_dir.c: Configuración's report */
        char m[64];
        snprintf(m, sizeof m, "falla 0x%08lX", (unsigned long)ep->ExceptionRecord->ExceptionCode);
        ygo_marcar_error(m);
    }
    _snwprintf(msg, 256,
               L"El juego tuvo un error y se va a cerrar (código 0x%08lX).\n\n"
               L"Abrí Configuración y tocá \"Informe de error\" para armar el archivo para mandar.",
               (unsigned long)ep->ExceptionRecord->ExceptionCode);
    msg[255] = 0;
    ClipCursor(NULL);
    MessageBoxW(NULL, msg, L"Yu-Gi-Oh! The Dawn of Destiny", MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND);
    TerminateProcess(GetCurrentProcess(), 1);
    return EXCEPTION_EXECUTE_HANDLER;
}

static void host_install_crash_handler(void)
{
    SetUnhandledExceptionFilter(unhandled_filter);
    /* Load symbols up front rather than from inside the handler: at fault
     * time the process is already in a bad way, and SymInitialize
     * allocates. Failure is not fatal -- the handler prints no name. */
    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
    SymInitialize(GetCurrentProcess(), NULL, TRUE);
    /* First handler in the chain */
    AddVectoredExceptionHandler(1, veh_handler);
}

#else /* !_WIN32 */

/* ── POSIX: SIGBUS/SIGSEGV crash handler ───────────────────── */

/* Guest memory is reserved PROT_NONE, and on macOS touching a mapped but
 * inaccessible page is SIGBUS; only a truly unmapped address is SIGSEGV. So
 * the common guest fault arrives as SIGBUS, and both are handled. Nothing in
 * the toolkit installs signal handlers, so nothing else competes for these.
 *
 * Known gaps, accepted for a bring-up tool: vsnprintf and dladdr are not
 * formally async-signal-safe (dladdr takes dyld's lock), and Darwin allocates
 * __thread storage lazily, so reading the guest registers on a thread that
 * never touched them can malloc. The Windows VEH has the same exposure with
 * fprintf. */

/* PC of the faulting instruction, from the signal context. */
static void *host_fault_pc(void *ucv)
{
    ucontext_t *uc = (ucontext_t *)ucv;
#if defined(__APPLE__) && defined(__aarch64__)
    return (void *)(uintptr_t)uc->uc_mcontext->__ss.__pc;
#elif defined(__APPLE__) && defined(__x86_64__)
    return (void *)(uintptr_t)uc->uc_mcontext->__ss.__rip;
#elif defined(__linux__) && defined(__aarch64__)
    return (void *)(uintptr_t)uc->uc_mcontext.pc;
#elif defined(__linux__) && defined(__x86_64__)
    return (void *)(uintptr_t)uc->uc_mcontext.gregs[REG_RIP];
#else
    (void)uc;
    return NULL;
#endif
}

/* Read or write, when the host can tell.
 *
 * arm64 Darwin: the ESR's WnR bit (6) means "write", but only for a data
 * abort (exception class 0x24 from a lower EL, 0x25 from the same EL). For any
 * other class the bit means something else, so say unknown. */
static int host_fault_access(void *ucv)
{
#if defined(__APPLE__) && defined(__aarch64__)
    ucontext_t *uc = (ucontext_t *)ucv;
    uint32_t esr = uc->uc_mcontext->__es.__esr;
    uint32_t ec = esr >> 26;

    if (ec == 0x24 || ec == 0x25)
        return (esr >> 6) & 1 ? FAULT_WRITE : FAULT_READ;
    return FAULT_UNKNOWN;
#else
    (void)ucv;
    return FAULT_UNKNOWN;
#endif
}

/* The native call chain, innermost first, by walking frame records.
 *
 * Apple's arm64 ABI keeps a frame pointer, so starting from the signal context
 * this is exact and async-signal-safe (bar dladdr) -- unlike backtrace(),
 * which drops the faulting leaf frame. Frame 0 is the faulting PC; frame 1 is
 * lr, which is the caller even when the leaf never built a frame of its own;
 * then each frame record [fp] = caller's fp, [fp+8] = return address. */
static void print_native_stack(void *ucv)
{
#if defined(__APPLE__) && defined(__aarch64__)
    ucontext_t *uc = (ucontext_t *)ucv;
    uintptr_t pc = (uintptr_t)uc->uc_mcontext->__ss.__pc;
    uintptr_t lr = (uintptr_t)uc->uc_mcontext->__ss.__lr;
    uintptr_t fp = (uintptr_t)uc->uc_mcontext->__ss.__fp;
    char name[300];
    int n = 0, first = 1;

    crash_printf("  Native stack (innermost first):\n");
    host_symbol_for((void *)pc, name, sizeof(name));
    crash_printf("    [%d] 0x%llX in %s\n", n++, (unsigned long long)pc, name);
    host_symbol_for((void *)lr, name, sizeof(name));
    crash_printf("    [%d] 0x%llX in %s\n", n++, (unsigned long long)lr, name);

    while (n < 16 && fp && (fp & 7) == 0) {
        uintptr_t next = ((const uintptr_t *)fp)[0];
        uintptr_t ret = ((const uintptr_t *)fp)[1];

        /* A leaf that did build a frame stored lr in it: skip that one
         * repeat only -- recursive frames legitimately share a return address. */
        if (!(first && ret == lr) && ret) {
            host_symbol_for((void *)ret, name, sizeof(name));
            crash_printf("    [%d] 0x%llX in %s\n", n++,
                         (unsigned long long)ret, name);
        }
        first = 0;
        if (next <= fp)     /* the stack grows down: callers' frames are higher */
            break;
        fp = next;
    }
#else
    (void)ucv;
    crash_printf("  Native stack: not available on this host\n");
#endif
}

/* Re-entry guard: a fault while writing the report, or a second thread
 * faulting meanwhile, must end the process, not report again. SA_NODEFER lets
 * a same-signal fault inside the handler get here instead of being
 * force-killed by the kernel with nothing said. */
static volatile sig_atomic_t g_in_crash_handler;

static void restore_default(int sig)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}

/* A signal sent with kill, raise or pthread_kill has no faulting instruction
 * to re-execute: returning would let the process run on. Deliver it again,
 * now with the default disposition. raise is async-signal-safe. */
static void die_if_sent(int sig, const siginfo_t *si)
{
    if (si->si_code == SI_USER || si->si_code == SI_QUEUE
#ifdef SI_TKILL
        || si->si_code == SI_TKILL
#endif
        )
        raise(sig);
}

static void posix_crash_handler(int sig, siginfo_t *si, void *ucv)
{
    /* Trapped hardware registers -> their models, through the toolkit's
     * dispatch: the APU, the AC'97 bus-master page (RECOMP_AC97_READY) and the
     * USB OHCI controllers. A serviced access returns here with pc advanced;
     * the kernel resumes the thread after the instruction. Hosts without a
     * decoder (x86-64 macOS) get 0 and fall through to the report. */
    if (xbox_PosixMmioFault(ucv, (uintptr_t)si->si_addr))
        return;

    if (__atomic_exchange_n(&g_in_crash_handler, 1, __ATOMIC_SEQ_CST)) {
        restore_default(sig);
        die_if_sent(sig, si);
        return;
    }

    report_crash(sig == SIGBUS ? "SIGBUS" : "SIGSEGV", host_fault_pc(ucv),
                 (uintptr_t)si->si_addr, host_fault_access(ucv));
    print_native_stack(ucv);

    /* Returning re-executes the faulting instruction with the default
     * disposition in place, so the process dies by the original signal: the
     * shell's exit status and any core dump say what happened. */
    restore_default(sig);
    die_if_sent(sig, si);
}

/* Alternate signal stack for the thread that installs the handler (the one
 * that runs the guest), so a stack overflow there still gets a report.
 * Guest-created threads have none (their start routine is toolkit code), so
 * an overflow on one of those dies unreported -- still by signal. 128 KB, the
 * Darwin SIGSTKSZ: snprintf plus a dladdr per frame does not fit in much
 * less. A literal, because SIGSTKSZ is not a compile-time constant on
 * glibc >= 2.34. */
static char g_crash_altstack[128 * 1024];

static void host_install_crash_handler(void)
{
    stack_t ss;
    struct sigaction sa;

    memset(&ss, 0, sizeof(ss));
    ss.ss_sp = g_crash_altstack;
    ss.ss_size = sizeof(g_crash_altstack);
    if (sigaltstack(&ss, NULL) != 0)
        fprintf(stderr, "[BOOT] sigaltstack failed; a guest-thread stack "
                        "overflow will die without a report\n");

    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = posix_crash_handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_NODEFER;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGSEGV, &sa, NULL);
}

#endif /* _WIN32 */

/* ── Startup failures ──────────────────────────────────────── */

/* A required startup step failed: say which, and what to do, on stderr on
 * every host -- and in a dialog on Windows, where a GUI-subsystem launch may
 * have no console to read. RECOMP_HEADLESS (no window) means no dialog
 * either: one would wait for a click nobody can give. */
static void host_report_fatal(const char *step, const char *detail)
{
    fprintf(stderr, "[FATAL] %s: %s\n", step, detail);
    fflush(stderr);
#if defined(_WIN32)
    if (!recomp_env_on(RENV_HEADLESS))
        MessageBoxA(NULL, detail, "Recomp", MB_ICONERROR);
#endif
}

/* rdr-port: the code was recompiled from one exact default.xbe, the PAL
 * (Europe) release, region "rest of the world", version 3. Any other build
 * (the NTSC one is 4,403,200 bytes) has its code and data elsewhere and dies
 * at the first indirect call, so say so and stop. RDR_XBE_ANY=1 skips the
 * check (development). FNV-1a 64 of the whole file. */
#define RDR_XBE_SIZE 7254016u                 /* Yu-Gi-Oh TDOD PAL */
#define RDR_XBE_FNV  0x700522D705460839ull

static int rdr_xbe_supported(const uint8_t *data, size_t size)
{
    const char *any = getenv("RDR_XBE_ANY");
    uint64_t h = 0xcbf29ce484222325ull;
    size_t i;
    if (any && *any == '1')
        return 1;
    if (size != RDR_XBE_SIZE)
        return 0;
    for (i = 0; i < size; i++)
        h = (h ^ data[i]) * 0x100000001b3ull;
    return h == RDR_XBE_FNV;
}

static void rdr_report_wrong_xbe(size_t size)
{
    char msg[1200];
    snprintf(msg, sizeof msg,
             "Tu copia de Yu-Gi-Oh! The Dawn of Destiny es otra versión del juego (default.xbe de %zu bytes).\n"
             "Este port solo funciona con la versión PAL (Europa) de Xbox: default.xbe de %u bytes.\n\n"
             "Your copy of Yu-Gi-Oh! The Dawn of Destiny is a different version of the game (default.xbe, %zu bytes).\n"
             "This port only works with the PAL (Europe) Xbox release: default.xbe, %u bytes.",
             size, RDR_XBE_SIZE, size, RDR_XBE_SIZE);
    fprintf(stderr, "[FATAL] load XBE: unsupported game version (%zu bytes)\n", size);
    fflush(stderr);
#if defined(_WIN32)
    if (!recomp_env_on(RENV_HEADLESS)) {
        wchar_t w[1200];
        if (MultiByteToWideChar(CP_UTF8, 0, msg, -1, w, (int)(sizeof w / sizeof w[0])))
            MessageBoxW(NULL, w, L"Yu-Gi-Oh! The Dawn of Destiny PC", MB_ICONERROR);
    }
#endif
}

/* ── Boot sequence (shared) ────────────────────────────────── */

/* RECOMP_STDIO_LOG and unbuffered stdio, from the entry points: before any
 * other output and before any thread, so every line of the boot reaches the
 * log. */
static void host_stdio_setup(void)
{
    /* RECOMP_STDIO_LOG=<path>: send stdout and stderr to a file (truncated).
     * Proton drops a game's stdio whatever its subsystem, so this is the only
     * way to see the boot log and crash reports there. The bench, the
     * Windows launcher and the SteamOS launcher all set it and read the
     * file. */
    int stdio_log_err = -1;   /* Windows only: did stderr reopen? */
    {
        const char *stdio_log = recomp_env(RENV_STDIO_LOG);
        if (stdio_log && *stdio_log) {
            /* Open it first: a failed freopen leaves the stream closed, and
             * the whole boot log would vanish without a word. */
            int opened = 0;
#if defined(_WIN32)
            /* A GUI-subsystem exe starts with no stderr descriptor
             * (_fileno(stderr) == -2), so there is nothing to _dup2 onto.
             * Reopen both streams instead, in append mode so that their
             * writes interleave rather than overwrite each other. */
            FILE *trunc = fopen(stdio_log, "w");
            if (trunc) {
                fclose(trunc);
                opened = 1;
                if (freopen(stdio_log, "a", stdout))
                    stdio_log_err = freopen(stdio_log, "a", stderr) != NULL;
            }
#else
            FILE *probe = fopen(stdio_log, "w");
            if (probe) {
                fclose(probe);
                opened = 1;
                if (freopen(stdio_log, "w", stdout))
                    dup2(fileno(stdout), fileno(stderr));
            }
#endif
            if (!opened)
                fprintf(stderr, "[BOOT] RECOMP_STDIO_LOG: cannot open %s; "
                        "output stays on the console\n", stdio_log);
        }
    }

    /* Unbuffered output for immediate visibility during debugging */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    if (stdio_log_err == 0)
        printf("[BOOT] RECOMP_STDIO_LOG: stderr NOT redirected\n");
    /* Deprecated names and bad keys found when the environment was read:
     * held until now, so they land in the log rather than a closed stderr. */
    recomp_env_flush_notes();
}

/* The boot and the guest. On Windows and under RECOMP_HEADLESS it runs on the
 * process main thread; on a windowed POSIX host on the toolkit's guest-main
 * pthread (xbox_HostWindowMain), so the crash handler's alternate stack and
 * the watchdog, both installed here, follow the guest onto that thread. */
static int host_main(void)
{
    void *xbe_data = NULL;
    size_t xbe_size = 0;

    printf("=== Yu-Gi-Oh! The Dawn of Destiny - Static Recompilation ===\n");
    printf("Loading XBE...\n");
    xbox_log_thread_role("guest-main", 0);

    /* Step 1: Crash diagnostics, before anything can fault */
    host_install_crash_handler();

    /* Step 2: Load XBE. RECOMP_GAME_FILES and RECOMP_HDD_DIR move the game
     * files and the emulated hard disk out of the working directory and the
     * toolkit's default save root: the CLI's launchers set both, so an
     * installed game keeps its saves outside the program files (and outside
     * any Wine prefix, which Proton may recreate). Unset, the defaults above
     * apply. */
    const char *game_dir_env = recomp_env(RENV_GAME_FILES);
    const char *hdd_dir = recomp_env(RENV_HDD_DIR);
    const char *game_dir = YOUR_GAME_DIR;
    char xbe_path[1024];
    if (hdd_dir && !*hdd_dir)
        hdd_dir = NULL;
    if (game_dir_env && *game_dir_env) {
        int n;
        game_dir = game_dir_env;
        n = snprintf(xbe_path, sizeof(xbe_path), "%s/default.xbe", game_dir);
        if (n < 0 || (size_t)n >= sizeof(xbe_path)) {
            /* A truncated path would open some other file, or none. */
            host_report_fatal("load XBE",
                              "the game files path (RECOMP_GAME_FILES) is too long");
            return 1;
        }
    } else {
        snprintf(xbe_path, sizeof(xbe_path), "%s", YOUR_GAME_XBE_PATH);
    }
    if (!load_xbe(xbe_path, &xbe_data, &xbe_size)) {
        char detail[1400];
        if (game_dir == game_dir_env)
            snprintf(detail, sizeof(detail),
                     "Failed to load %s (from RECOMP_GAME_FILES=%s).",
                     xbe_path, game_dir_env);
        else
            snprintf(detail, sizeof(detail),
                     "Failed to load default.xbe from %s.\n"
                     "Place the game files in " YOUR_GAME_DIR
                     ", or set RECOMP_GAME_FILES.", xbe_path);
        host_report_fatal("load XBE", detail);
        return 1;
    }
    printf("XBE loaded: %zu bytes\n", xbe_size);
    if (!rdr_xbe_supported(xbe_data, xbe_size)) {
        rdr_report_wrong_xbe(xbe_size);
        free(xbe_data);
        return 1;
    }

    /* rdr-port: "Modo experimental" (ygo_menu.ini) turns on run_ahead before
     * the memory layout starts the walker, which reads it once. */
    fast_mode_from_ini();

    /* Step 3: Initialize Xbox memory layout */
    printf("Initializing Xbox memory layout...\n");
    if (!xbox_MemoryLayoutInit(xbe_data, xbe_size)) {
        host_report_fatal("memory layout",
                          "Failed to initialize Xbox memory layout.\n"
                          "The required virtual address range may be unavailable.");
        free(xbe_data);
        return 1;
    }

    g_xbox_mem_offset = xbox_GetMemoryOffset();
    printf("Xbox memory mapped. Offset: 0x%llX\n", (unsigned long long)g_xbox_mem_offset);

    /* D3D's GPU completion fence. XDK D3D keeps a pointer to its device in a
     * global; the device holds the fence it last submitted at +0x2C and, at
     * +0x30, a pointer to the word the GPU writes as it completes work. The
     * toolkit drives PGRAPH 0x400B10, which D3D's wait-for-idle polls, from
     * that word. With RECOMP_PB_EXEC on the executor writes the word itself
     * at each semaphore release and this mirror steps aside after the first
     * one; it stays registered as the catch-up path when a walk stops short
     * of PUT, and as the only fence when the executor is off.
     *
     * The pointer is the game's: 0x2F1FB8 in BLiNX 2, 0x235528 in Spartan,
     * 0x35FB48 in Burnout 3 (XDK 5849; the same two offsets in all three).
     * `xbr new` fills the line in when it finds D3D's fence wait in the
     * dump's D3D section; otherwise find it as the CLI's docs/porting.md
     * ("The D3D fence") says and replace the example below. The runtime's
     * "NV2A fence mirror: ... now written by the executor; mirror off" line
     * confirms it. */
    /* Yu-Gi-Oh TDOD: the most-read global of its D3D section, as 0x3497F8 is
     * in Red Dead Revolver (the next one sits 0x4A0 below in both). */
    xbox_Nv2aMirrorFence(0x0038ECA8, 0x2C, 0x30);   /* Yu-Gi-Oh TDOD, XDK 5849 */

    /* The vblank interrupt, which the toolkit raises only on request. XDK
     * D3D connects the NV2A interrupt during device creation and paces Swap,
     * BlockUntilVerticalBlank and the title's vblank callback on its DPC;
     * without it the boot stops in D3D's first vblank wait. Nothing is
     * delivered until the title connects the ISR and enables the interrupt,
     * so a title that never asks for it only pays a 60 Hz timer. An explicit
     * RECOMP_VBLANK in the environment still wins (=0 turns it off). */
    if (!recomp_env(RENV_VBLANK))
        recomp_env_set(RENV_VBLANK, "1");

    /* D3D8 builds the KEVENT its vblank DPC sets inline, without
     * KeInitializeEvent. The toolkit gives such an event a host event only on
     * request (upstream's ke_guest_event); without it KeSetEvent hands the
     * guest address to the host as a handle, which fails silently on Win32
     * and faults on POSIX. The vblank above needs it, and it does nothing for
     * an event KeInitializeEvent made. An explicit RECOMP_TITLE_KEVENTS in
     * the environment still wins (=0 turns it off). */
    if (!recomp_env(RENV_TITLE_KEVENTS))
        recomp_env_set(RENV_TITLE_KEVENTS, "1");

    /* Bring up the emulated APU.
     *
     * Gated on the same variable that unmaps its registers, because the two
     * halves are useless apart: a trap with no model declines every access,
     * and a model nothing traps into never sees a register. The APU walks
     * Xbox physical RAM to find voice buffers, so it gets the guest RAM
     * base. The toolkit turns RECOMP_AC97_READY on by default only where a
     * fault decoder exists (x86-64 Windows, POSIX arm64). */
    if (recomp_env(RENV_AC97_READY)) {
        g_apu_state = mcpx_apu_init_standalone((uint8_t *)xbox_GetMemoryBase());
        fprintf(stderr, "[BOOT] emulated APU %s\n",
                g_apu_state ? "up" : "FAILED to initialise");
    }

    /* The pad. XAPI is statically linked into the title and reaches the
     * controller by programming the MCPX's USB OHCI registers itself
     * (look for 0xFED00000 in the generated code); nothing in that path goes
     * through a kernel export this runtime could answer. Without the
     * toolkit's controller model the registers read as zeroed RAM, XAPI
     * finds no root hub ports, and no pad -- host, keyboard or
     * RECOMP_PAD_SCRIPT -- ever reaches the title. With it, the host's pad
     * (SDL GameController on POSIX, XInput on Windows; the keyboard merges
     * onto port 0) is presented as an Xbox controller on port 1. Must follow
     * the memory mapping: the model traps the aperture by protecting its
     * pages. A title that overrides XInput itself instead drops these lines.
     * An explicit RECOMP_USB in the environment still wins (=0 turns it
     * off). */
    if (!recomp_env(RENV_USB))
        recomp_env_set(RENV_USB, "1");
    xbox_OhciInit();
    rdr_mouse_init();
    ygo_cursor_scan_start();          /* YGO_SCAN=1: F10 memory snapshots */
    /* ygo-port: no game-specific mouse look yet (RDR's mouselook.c hooked its
     * own camera functions). */
    rdr_graphics_init();
    if (nv2a_tex_pack_count())
        pack_menu_init();
    fast_menu_init();
    {
        extern void ygo_onda_start(void);
        ygo_onda_start();             /* onda.c: the click ring */
    }
    {
        extern void ygo_premios_start(void);
        ygo_premios_start();          /* premios.c: the opponent's rewards sign */
        {
            extern void ygo_premios_extra_start(void);
            ygo_premios_extra_start(); /* premios_extra.c: who gives each card, new card */
            {
                extern void ygo_perfil_start(void);
                ygo_perfil_start();     /* perfil.c: the main menu's Perfil */
                {
                    extern void ygo_buscador_start(void);
                    ygo_buscador_start();   /* buscador.c: the Biblioteca's search */
                    {
                        extern void ygo_auto_start(void);
                        ygo_auto_start();   /* auto.c: AUTO in the duel */
                    }
                }
            }
        }
    }
    {
        /* The F2 menu in the launcher's language (f2_idiomas.c). */
        extern void f2_idiomas_init(void);
        f2_idiomas_init();
    }
    /* The F2 menu's saved choices (d3d/host_menu.h) over env.bat's. */
    {
        extern void xbox_MenuLoad(const char *path);
        /* ygo-port: the settings in YGO_AJUSTES (env.bat: the port's
         * Ajustes\ folder, beside Partidas\), which a new version of the
         * port does not overwrite; Configuración.exe edits the same file.
         * The old ygo_menu.ini of the working folder is moved there once. */
        static char ini[MAX_PATH];
        const char *dir = getenv("YGO_AJUSTES");
        if (dir && *dir) {
            CreateDirectoryA(dir, NULL);
            snprintf(ini, sizeof ini, "%s\\ygo_menu.ini", dir);
            if (GetFileAttributesA(ini) == INVALID_FILE_ATTRIBUTES
                && GetFileAttributesA("ygo_menu.ini") != INVALID_FILE_ATTRIBUTES
                && CopyFileA("ygo_menu.ini", ini, TRUE))
                fprintf(stderr, "[MENU] ygo_menu.ini moved to %s\n", ini);
        } else
            snprintf(ini, sizeof ini, "ygo_menu.ini");
        xbox_MenuLoad(ini);
        /* ygo-port: pointer mode hides the game's own glove (the atlas copy
         * in mods_puntero\, tools/guante.py), so the Windows pointer drawn as
         * the glove is the only one. Read at the first file open. */
        if (ygo_mouse_mode_get() == 0) {
            char full[MAX_PATH];
            if (GetFullPathNameA("mods_puntero", MAX_PATH, full, NULL))
                _putenv_s("RECOMP_MODS_DIR", full);
        }
        /* ygo-port: the card mod's files (archivos\: the cards' images)
         * stand in for the disc's too, if "Cartas nuevas" is on (cartas.c). */
        {
            extern int ygo_cartas_get(void);
            const char *c = getenv("YGO_CARTAS"), *m = getenv("RECOMP_MODS_DIR");
            if (c && *c && !ygo_cartas_get()) {
                char all[MAX_PATH * 3];
                snprintf(all, sizeof all, "%s%s%s\\archivos", m && *m ? m : "",
                         m && *m ? ";" : "", c);
                _putenv_s("RECOMP_MODS_DIR", all);
            }
        }
        /* ygo-port: "Pantalla completa" as saved (F2 or Configuración.exe)
         * is how the game starts. The forced window start of 2026-11-08 is
         * gone (2026-11-10, the user: choosing fullscreen opened a window;
         * the saved settings must be honoured). */
    }

    /* Step 4: Initialize Xbox kernel */
    printf("Initializing Xbox kernel replacement...\n");
    xbox_kernel_init();

    /* Step 5: Set game directory for file I/O path translation. A NULL save
     * dir (RECOMP_HDD_DIR unset) puts saves and partition images in the
     * toolkit's default place (beside the game on Windows, $XDG_DATA_HOME or
     * ~/.local/share elsewhere). */
    {
        extern void xbox_path_init(const char *game_dir, const char *save_dir);
        xbox_path_init(game_dir, hdd_dir);
        { extern const char *g_ygo_game_dir; g_ygo_game_dir = game_dir; }   /* cartel.c: the portraits */
        {
            extern void ygo_juego_overlay(const char *game_dir);
            ygo_juego_overlay(game_dir);    /* perfil_dir.c: the port's files inside the game's folder */
        }
    }

    /* Step 6: Initialize kernel bridge (thunk table in Xbox memory) */
    printf("Initializing kernel bridge...\n");
    xbox_kernel_bridge_init();

    /* Step 7: Initialize stack */
    g_esp = XBOX_STACK_TOP;

    /*
     * TODO: Pre-initialize CRT globals if needed.
     *
     * Many Xbox games use the MSVC CRT. The CRT's __heap_init sets up a
     * heap descriptor at a game-specific address. You may need to
     * pre-initialize __active_heap to avoid small-block heap issues:
     *
     *   uint32_t *active_heap = (uint32_t *)((uint8_t *)g_xbox_mem_offset + ACTIVE_HEAP_VA);
     *   *active_heap = 1;  // 1 = system heap (HeapAlloc), avoids SBH init
     *
     * Find ACTIVE_HEAP_VA by searching the disassembly for __heap_init
     * or by looking for the CRT's __active_heap global in the data section.
     */

    printf("\n=== Initialization complete ===\n");
    printf("Entry point: 0x%08X\n", YOUR_GAME_ENTRY_POINT);
    printf("ESP: 0x%08X\n", g_esp);

    /* Build the flat dispatch table before the guest runs.
     *
     * Without this recomp_lookup falls back to a binary search over the
     * whole function table -- roughly log2(n) branches on *every*
     * indirect call, which for a 45,000-function C++ title is about 16
     * every time the game goes through a vtable. Half-Life 2 spent most
     * of its static initialisation inside recomp_lookup for exactly this
     * reason, and it read as a hang.
     *
     * Optional by design: if the allocation fails the search still works,
     * so a failure is worth one line and not a fatal error. */
    if (!recomp_dispatch_init())
        fprintf(stderr, "[BOOT] flat dispatch unavailable; "
                        "indirect calls will use the binary search\n");

    /* Arm the hang watchdog. Does nothing unless RECOMP_WATCHDOG_SECS is set,
     * and must be called from this thread -- the guest registers it samples are
     * thread-local, so it has to be handed the copies belonging to the thread
     * that runs guest code.
     *
     * Not optional boilerplate: without this call RECOMP_WATCHDOG_SECS is
     * silently inert, and the one diagnostic that tells a hang from slowness
     * does nothing while appearing to be set. */
    xbox_WatchdogStart();
    zwatch_start();
    scan_start();

    /* ygo-port: the game's own widescreen variables are not known yet. */

    {
        /* cartas.c: the card mod's data (names, texts, effects) over the
         * game's, before it starts */
        extern void ygo_cartas_init(void);
        ygo_cartas_init();
    }

    /* Step 8: Call the recompiled entry point */
    printf("\nStarting game...\n");
    fflush(stdout);

    xbe_entry_point();

    printf("\nGame returned. Cleaning up...\n");

    /* Cleanup */
    xbox_kernel_shutdown();
    xbox_MemoryLayoutShutdown();
    free(xbe_data);

    return 0;
}

/* ── XBE Loading ───────────────────────────────────────────── */

static BOOL load_xbe(const char *path, void **out_data, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open XBE: %s\n", path);
        return FALSE;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        return FALSE;
    }

    void *data = malloc((size_t)size);
    if (!data) {
        fclose(f);
        return FALSE;
    }

    if (fread(data, 1, (size_t)size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return FALSE;
    }

    fclose(f);
    *out_data = data;
    *out_size = (size_t)size;
    return TRUE;
}

/* ── Entry points ──────────────────────────────────────────── */

/* The enhancements (render scale, present filter, fullscreen, pacing):
 * config.default.toml beside the executable, config/config.toml over it,
 * the environment over both, every key stock by default. The user root is
 * the working directory: a developer runs from the project root, as
 * YOUR_GAME_DIR already assumes, so their choices live in a gitignored
 * config/config.toml and no source path is baked into the binary; a
 * bundle's launcher names the file with RECOMP_CONFIG, so there the root
 * is never used. Before any thread, so the backends, the window and the
 * kernel read settled values. Without the layer (-DXBOXRECOMP_ENHANCE=OFF)
 * this is nothing.
 *
 * A game's own keys (enhance_cfg_bind_env, enhance_cfg_choice, ...) are read
 * between the two calls, so a file that sets one is not told the key is
 * unknown. */
/* F2 row for the texture pack: its images on or off, to compare in the game
 * at once (F6 toggles the same, nv2a_pb_d3d11.c). */
static const char *const k_pack_onoff[] = { "No", "Sí", NULL };
static int get_pack_art(void) { return nv2a_tex_pack_kind_enabled(XRTP_KIND_ART); }
static void set_pack_art(int i)
{
    nv2a_tex_pack_set_kind_enabled(XRTP_KIND_ART, i);
    nv2a_tex_pack_set_kind_enabled(XRTP_KIND_FONT, i);
}
/* The buttons drawn on screen: the pack's kind BUTTON entries in this style
 * (tools/botones_estilo.py), whatever "Texturas mejoradas" (F6) says. Saved
 * as botones in ygo_menu.ini. Automático (the default) follows the device
 * last used (xbox_InputLastDevice), checked by btn_auto_thread; a style
 * picked by hand stays, in this run and the next.
 * 2026-11-21: the F2 row's names and get/set removed (the row left F2 on
 * 11-10, it was Red Dead Revolver's); automatic, as before. */
static volatile int s_btn_choice;           /* 0 automatic, else style + 1 */
static DWORD WINAPI btn_auto_thread(LPVOID unused)
{
    (void)unused;
    for (;;) {
        int d = xbox_InputLastDevice();
        if (s_btn_choice == 0 && d >= 0 && d != nv2a_tex_pack_button_style())
            nv2a_tex_pack_set_button_style(d);
        Sleep(150);
    }
}
/* Subtitles are always on (movies.c); the row was taken out at the user's
 * request. */
static const xbox_menu_item k_pack_rows[] = {
    { NULL, "Texturas", NULL, NULL, NULL },
    { "pack_art", "Texturas mejoradas", k_pack_onoff, get_pack_art, set_pack_art },
    /* "Botones en pantalla" (botones) left out (2026-11-10, the user: it is
     * Red Dead Revolver's). The automatic style (btn_auto_thread) stays. */
};

/* "Modo experimental" (PC de gama baja): RECOMP_DEBUG=run_ahead=1. The game
 * thread waited at every D3D KickOff for the walker to finish the segment
 * before it (sub_0033EDE0, 22% of the game thread on an i5-3330); with it the
 * two overlap (13%, +5-8 fps there). The walker reads the key once, before
 * the F2 menu exists, so the ini line is read here first and the row only
 * records the choice for the next start. An explicit run_ahead in the
 * environment wins. */
/* 2026-11-21: the quality profile, the same as Configuración.exe's (the
 * user: "se puede poner en f2? cambiar en tiempo real? ... sincronizado con
 * configuracion"): the first row of Gráficos. Picking one sets its five rows
 * at once (k_perfil_val, the launcher's Perf table); changing one of those
 * by hand makes it Personalizado (get_perfil); Recomendado is this PC's
 * level (pc_detectar: the launcher's Detect rules). Saved as perfil= like
 * every row; read at boot for the enhanced textures' RAM (perfil_memoria). */
static const char *const k_perfil[] = { "Recomendado", "Bajo", "Medio", "Alto", "Ultra", "Personalizado", NULL };
static const char *const k_perfil_clave[5] = { "render_scale", "aniso", "fps_juego", "pack_art", "mascotas" };
static const char *const k_perfil_val[5][5] = {
    { NULL },
    { "x1 (640x480)",   "No",  "60",      "No", "No" },     /* Bajo */
    { "x2 (1280x960)",  "x4",  "60",      "Sí", "Sí" },     /* Medio */
    { "x3 (1920x1440)", "x8",  "120",     "Sí", "Sí" },     /* Alto */
    { "x4 (2560x1920)", "x16", "Monitor", "Sí", "Sí" },     /* Ultra */
};
static int s_perfil, s_pc_nivel = 2, s_perfil_aplicando;
static void pc_detectar(void);

/* the level a profile stands for: 1..4, 0 none (Personalizado) */
static int perfil_nivel(int p)
{
    pc_detectar();
    return p == 0 ? s_pc_nivel : p >= 1 && p <= 4 ? p : 0;
}

static int get_perfil(void)
{
    int n = perfil_nivel(s_perfil), k;
    if (n && !s_perfil_aplicando && !xbox_MenuLoading())
        for (k = 0; k < 5; k++) {
            const char *c = xbox_MenuChoice(k_perfil_clave[k]);
            if (c && strcmp(c, k_perfil_val[n][k])) {
                s_perfil = 5;           /* one changed by hand */
                break;
            }
        }
    return s_perfil;
}

static void set_perfil(int i)
{
    int n, k;
    s_perfil = i;
    n = perfil_nivel(i);
    if (!n || xbox_MenuLoading())
        return;                         /* the file's own rows follow */
    s_perfil_aplicando = 1;
    for (k = 0; k < 5; k++)
        xbox_MenuSetChoice(k_perfil_clave[k], k_perfil_val[n][k]);
    s_perfil_aplicando = 0;
    xbox_MenuSaveSoon();
}
static const xbox_menu_item k_perfil_rows[] = {
    { "perfil", "Perfil de calidad", k_perfil, get_perfil, set_perfil },
};
static const char *const k_fast_mode[] = { "No", "Sí (PC gama baja)", NULL };
static int s_fast_choice;
static int get_fast_mode(void) { return s_fast_choice; }
static void set_fast_mode(int i) { s_fast_choice = i; }
static const xbox_menu_item k_fast_rows[] = {
    { NULL, "Rendimiento", NULL, NULL, NULL },
    { "modo_rapido", "Modo experimental (al reiniciar)", k_fast_mode, get_fast_mode, set_fast_mode },
};

/* rdr-port: F2 "Ver controles": the controller with the keys picked in
 * Configuración.exe (controles.bmp, which it draws), beside the rows, as a
 * tutorial card. Not saved: each run starts with it hidden. */
void xbox_MenuPicture(const char *bmp_path);
static const char *const k_ctl_show[] = { "No", "Sí", NULL };
static int s_ctl_show;
static int get_ctl_show(void) { return s_ctl_show; }
static void set_ctl_show(int i)
{
    s_ctl_show = i;
    xbox_MenuPicture(i ? "controles.bmp" : NULL);
}
/* ygo-port: the mouse as the game's cursor (mando_directo.c). Saved in
 * ygo_menu.ini as raton_cursor and raton_sensibilidad. */
static const char *const k_mouse_mode[] = { "Puntero (estilo PC)", "Cursor del juego", "Solo clics", NULL };
static int get_mouse_mode(void) { return ygo_mouse_mode_get(); }
static void set_mouse_mode(int i) { ygo_mouse_mode_set(i); }
static const char *const k_mouse_sens[] = { "Baja", "Media", "Alta", NULL };
static const int k_mouse_step[] = { 180, 120, 70 };
static int s_mouse_sens = 1;
static int get_mouse_sens(void) { return s_mouse_sens; }
static void set_mouse_sens(int i) { s_mouse_sens = i; ygo_mouse_step_set(k_mouse_step[i]); }
/* 2026-11-23 (the user: OBS loses the glove in the menus; then "quiero que
 * todo salga en obs", for YouTube): the glove (raton.c) and the panels
 * (capas.c) drawn into the game's picture instead of as windows. */
void ygo_capas_set(int on);
void ygo_glove_in_frame_set(int on);
int ygo_glove_in_frame(void);
static const char *const k_glove_frame[] = { "No", "Sí", NULL };
static int get_glove_frame(void) { return ygo_glove_in_frame(); }
static void set_glove_frame(int i) { ygo_glove_in_frame_set(i); ygo_capas_set(i); }

/* ygo-port: render.snap_2d live (see rdr_graphics_init). */
/* Saved under a new key (imagen_2d): the old lineas_2d=Sí in ygo_menu.ini
 * would have kept the 480p sampling on for whoever had it. */
static const char *const k_snap[] = {
    "Alta resolución", "Original (480p)", "Sin corregir (puede mostrar líneas)", NULL
};
static const int k_snap_mode[] = { 2, 1, 0 };
static int get_snap(void)
{
    int m = nv2a_host_opts()->snap_2d;
    return m == 2 ? 0 : m == 1 ? 1 : 2;
}
static void set_snap(int i)
{
    struct nv2a_host_opts o = *nv2a_host_opts();
    o.snap_2d = k_snap_mode[i < 0 || i > 2 ? 0 : i];
    nv2a_host_opts_request(&o, NV2A_OPT_SNAP2D);
}
static const xbox_menu_item k_snap_rows[] = {
    { NULL, "Imagen", NULL, NULL, NULL },
    { "imagen_2d", "Imagen 2D (cartas, menús, textos)", k_snap, get_snap, set_snap },
};

/* ygo-port: fast card plays (raton.c: the game runs x4 from a confirmed
 * summon, set or activation until it waits for the player again). */
extern void ygo_fast_cards_set(int on);
extern int ygo_fast_cards_get(void);
static const char *const k_fast_cards[] = { "No", "Sí", NULL };
static int get_fast_cards(void) { return ygo_fast_cards_get(); }
static void set_fast_cards(int i) { ygo_fast_cards_set(i); }

static const xbox_menu_item k_ctl_rows[] = {
    { NULL, "Controles", NULL, NULL, NULL },
    { NULL, "Ver controles (teclado)", k_ctl_show, get_ctl_show, set_ctl_show },
    { "raton_modo", "Modo del ratón", k_mouse_mode, get_mouse_mode, set_mouse_mode },
    { "raton_sensibilidad", "Sensibilidad del cursor", k_mouse_sens, get_mouse_sens, set_mouse_sens },
    { "raton_guante_imagen", "Modo grabación (OBS)", k_glove_frame, get_glove_frame, set_glove_frame },
    { "cartas_rapidas", "Jugar cartas rápido (animaciones)", k_fast_cards, get_fast_cards, set_fast_cards },
};

/* rdr-port: F2 "Salir a Windows": closes the game as the window's X does
 * (WM_CLOSE to the game window: the mouse is let go and the exit hooks run).
 * Not saved; progress since the game's last save is not kept, as with X. */
/* One choice and no key: host_menu.c draws it as a button that Enter, Space,
 * → or a click press (set(0)). */
static const char *const k_quit[] = { "[ Enter ]", NULL };
static int get_quit(void) { return 0; }
static void set_quit(int i)
{
    HWND h;
    (void)i;
    h = FindWindowA("RecompNv2aD3D11", NULL);
    if (h)
        PostMessageA(h, WM_CLOSE, 0, 0);
    else
        ExitProcess(0);
}
/* 2026-11-25: the profile being played (perfil_dir.c, YGO_PERFIL_NOMBRE),
 * shown only: changing it needs the game restarted, so Configuración does it */
static char s_perfil_nom[160] = "?";
static const char *k_perfil_act[] = { s_perfil_nom, NULL };
static int get_perfil_act(void)
{
    static int hecho;
    if (!hecho) {
        const wchar_t *w = _wgetenv(L"YGO_PERFIL_NOMBRE");
        hecho = 1;
        if (w && *w)
            WideCharToMultiByte(CP_UTF8, 0, w, -1, s_perfil_nom, sizeof s_perfil_nom, NULL, NULL);
    }
    return 0;
}
static void set_perfil_act(int i) { (void)i; }
static const xbox_menu_item k_quit_rows[] = {
    { NULL, "Salir", NULL, NULL, NULL },
    { NULL, "Perfil (se cambia en Configuración)", k_perfil_act, get_perfil_act, set_perfil_act },
    { NULL, "Salir a Windows", k_quit, get_quit, set_quit },
};

/* voces.c: the voices: following the game's frame rate (x1.5 at ~90 fps,
 * x2 at ~120, same pitch), always normal, or none. The ini key is still
 * voces_velocidad (keeps what players saved). */
extern int ygo_voces_fps_get(void);
extern void ygo_voces_fps_set(int i);
static const char *const k_voces_fps[] = { "Sigue los FPS", "Normal", "No", NULL };
/* 2026-11-20 (the user: "guardar los Fps porque cada vez que lo abro lo pongo
 * en 120 Fps manualmente"): the frame rate (the kit's vblank rate, F8's
 * 60 / 90 / 120) as a saved row */
unsigned xbox_VblankHz(void);
void xbox_VblankHzSet(unsigned hz);
/* 2026-11-20: 144, 165 and 200 too (the user) */
/* "Monitor": the display's own refresh rate (with VSync the frames land on
 * its refreshes: 120 on a 144 / 165 Hz screen judders) */
static const char *const k_fps_juego[] = { "60", "90", "120", "144", "165", "200", "Monitor", NULL };
static const unsigned k_fps_hz[] = { 60u, 90u, 120u, 144u, 165u, 200u };
static int s_fps_monitor;
static unsigned fps_del_monitor(void)
{
    DEVMODEW dm;
    memset(&dm, 0, sizeof dm);
    dm.dmSize = sizeof dm;
    if (EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency >= 30u && dm.dmDisplayFrequency <= 240u)
        return dm.dmDisplayFrequency;
    return 60u;
}
static int fps_juego_get(void)
{
    unsigned h = xbox_VblankHz();
    int i = 5;
    if (s_fps_monitor && h == fps_del_monitor())
        return 6;
    while (i > 0 && h < k_fps_hz[i])
        i--;
    return i;
}
static void fps_juego_set(int i)
{
    s_fps_monitor = i == 6;
    xbox_VblankHzSet(i == 6 ? fps_del_monitor() : k_fps_hz[i >= 0 && i < 6 ? i : 0]);
    if (i == 6)
        fprintf(stderr, "[FPS] los del monitor: %u\n", xbox_VblankHz());
}
static const xbox_menu_item k_fps_rows[] = {
    { "fps_juego", "Cuadros por segundo (F8)", k_fps_juego, fps_juego_get, fps_juego_set },
};
static const xbox_menu_item k_voces_rows[] = {
    { NULL, "Voces", NULL, NULL, NULL },
    { "voces_velocidad", "Voces", k_voces_fps, ygo_voces_fps_get, ygo_voces_fps_set },
};

/* onda.c: a ring that spreads from the pointer at each left click. Under the
 * mouse's rows ("Controles"). */
extern int ygo_onda_get(void);
extern void ygo_onda_set(int i);
extern void ygo_onda_start(void);
static const char *const k_onda[] = { "Sí", "No", NULL };
static const xbox_menu_item k_onda_rows[] = {
    { "onda_clic", "Onda al hacer clic", k_onda, ygo_onda_get, ygo_onda_set },
};

/* premios.c: on the opponent select, a sign with the marked opponent's
 * rewards (cards, missing, fewer than 3). Sí by default. */
extern int ygo_premios_get(void);
extern void ygo_premios_set(int i);
static const char *const k_premios[] = { "Sí", "No", NULL };
static const xbox_menu_item k_premios_rows[] = {
    { "cartel_premios", "Cartel de premios del rival", k_premios, ygo_premios_get, ygo_premios_set },
};

/* premios_extra.c: who gives each card (Biblioteca), rewards without a 4th
 * copy, the new card notice. Each one optional. */
const char *g_ygo_game_dir;
extern int ygo_quien_get(void);
extern void ygo_quien_set(int i);
extern int ygo_sin4_get(void);
extern void ygo_sin4_set(int i);
extern int ygo_nueva_get(void);
extern void ygo_nueva_set(int i);
/* 2026-11-25 (the user: "que diga Ayudas ... todos los carteles de premios,
 * quien te los da, etc."): this row is every help sign: who gives each card
 * (with or without photos), the rewards sign, the Triple's and the gods'
 * (premios.c). No turns them all off. */
static const char *const k_quien[] = { "Sí, con fotos", "Sí, sin fotos", "No", NULL };
static const char *const k_sin4[] = { "Sí", "No", NULL };
/* auto.c: AUTO in the duel (the plate under the name; the game never asks the
 * player "chain?") */
extern int ygo_auto_get(void);
extern void ygo_auto_set(int i);
static const char *const k_auto[] = { "No", "Sí", NULL };
static const xbox_menu_item k_auto_rows[] = {
    { "auto_cadena", "AUTO en el duelo (no preguntar encadenar)", k_auto, ygo_auto_get, ygo_auto_set },
};
extern int ygo_guardado_get(void);
extern void ygo_guardado_set(int i);
/* 2026-11-20 (the user: "guardar despues de ganar pone que si siempre"): a
 * saved "No" is not taken */
static void guardado_siempre(int i) { (void)i; ygo_guardado_set(0); }
static const char *const k_guardado[] = { "Sí", "No", NULL };
static const xbox_menu_item k_guardado_rows[] = {
    { "guardar_ganar", "Guardar al ganar", k_guardado, ygo_guardado_get, guardado_siempre },
};
static const xbox_menu_item k_extra_rows[] = {
    { "ayudas", "Ayudas", k_quien, ygo_quien_get, ygo_quien_set },
    { "premios_sin_cuarta", "Premios sin cuarta copia", k_sin4, ygo_sin4_get, ygo_sin4_set },
    { "aviso_nueva", "Aviso de carta nueva", k_sin4, ygo_nueva_get, ygo_nueva_set },
};

/* raton.c: the opponent's turn faster (the guest clock x2 / x4 while the
 * opponent acts and nothing of the player's waits for keys). No by default. */
extern int ygo_rival_get(void);
extern void ygo_rival_set(int i);
static const char *const k_rival[] = { "No", "x2", "x4", NULL };
/* mascota.c: a 3D monster over each player's Extra Deck in the duel. */
extern int ygo_mascota_get(void);
extern void ygo_mascota_set(int i);
static const char *const k_mascota[] = { "Sí", "No", NULL };
/* the duel's entrance (mascota.c: the temple's camera and the mascots' rise) */
extern int ygo_entrada_get(void);
extern void ygo_entrada_set(int i);
static const char *const k_entrada[] = { "Completa", "Solo las mascotas", "Original", NULL };
/* carta_as.c (2026-11-19, idea 3): your mascot's card / the opponent's, summoned face up */
extern int ygo_as_get(void);
extern void ygo_as_set(int i);
static const char *const k_as[] = { "Siempre", "Primera vez por duelo", "No", NULL };
/* the player's mascot (each opponent has its own) */
extern int ygo_mascota_tuya_get(void);
extern void ygo_mascota_tuya_set(int i);
static const char *const k_mascota_tuya[] = {
    "Ojos Azules",
    "Kuriboh",
    "Mago Oscuro",
    "Mago Oscuro II",
    "Jinzo",
    "Cráneo Convocado",
    "Ojos Rojos",
    "Chica Maga Oscura",
    "Gaia el Caballero Feroz",
    "Guardián Celta",
    "Dama Arpía",
    "Mago del Tiempo",
    "Dragón Bebé",
    "Espadachín de la Llama",
    "Soldado del Brillo Negro",
    "Buster Blader",
    "Exodia",
    "Dragón Barril",
    "Rey Rex de Dos Cabezas",
    "Gran Polilla",
    "Pescador Legendario",
    "Restricción de Mil Ojos",
    "Necrofear Oscuro",
    "Bestia Enmascarada",
    "Máquina Tragamonedas",
    "Dragón Zombi",
    "Maha Vailo",
    "Golem del Milenio",
    "Dragón Definitivo de Ojos Azules",
    "Dragón Negro de Cráneo",
    "Mago del Caos Negro",
    "Dragón Meteoro Negro",
    "Valkyrion el Guerrero Magna",
    "Dragón Toon de Ojos Azules",
    "Gaia el Campeón Dragón",
    "Sabio Oscuro",
    "Dragón del Trueno de Dos Cabezas",
    "Dragón de Tres Cuernos",
    "Reina del Cosmos",
    "Zera el Mant",
    "Caballero de Cráneo",
    "Dragón Mascota de Harpie",
    "Guardián de la Puerta",
    "Renunciado",
    "Cráneo Convocado Toon",
    "Maldición del Dragón",
    "Mil Dragones",
    "Alfa el Guerrero Magnético",
    "Beta el Guerrero Magnético",
    "Gamma el Guerrero Magnético",
    "Dragón Meteoro",
    "Elfa Mística",
    "Bruja del Bosque Negro",
    "Tomate Místico",
    "Sangan",
    "Gardna del Gran Escudo",
    "Payaso Zombi",
    "Fantasma Ingenioso",
    "Mago de la Fe",
    "Légamo Humanoide",
    "Princesa de Fuego",
    "Hermanas Dama Arpía",
    "Sanga del Trueno",
    "Kazejin",
    "Suijin",
    "Dokurorider",
    "Bruja Oscura Dunames",
    "Guerrero de Pizarra",
    "Gusano Dragón Humanoide",
    "Asaltante Vorse",
    
    /* 2026-11-19: 30 more (mascota.c k_modelo 70..99) */
    "Fuerza de Ataque Goblin", "Elfas Gemelas", "Santa Juana", "Zoa", "Melchid la Bestia de Cuatro Caras", "Bickuribox", "Elfa Oscura", "Tortuga Cangrejo", "Espada de Garma", "Escarabajo Jabalina", "Tanque Laberinto", "Ballena Fortaleza", "Dragón Nocturno Serpiente", "Chakra", "Fiend Megacyber", "Bicho Devorador de Hombres", "Jarra Cibernética", "Parshath Caballero Aéreo", "Soldado Pingüino", "Jarra Cambiante", "Muka Muka", "Soldado Gigante de Piedra", "Muro Laberinto", "Ninja Armado", "Aprendiz de Bruja", "Gilasaurus", "Battleguard de Lava", "Seiyaryu", "Guerrero Pantera", "Jirai Gumo", NULL
};
static const xbox_menu_item k_mascota_rows[] = {
    { "mascotas", "Mascotas en el duelo", k_mascota, ygo_mascota_get, ygo_mascota_set },
    { "mascota_tuya", "Tu mascota", k_mascota_tuya, ygo_mascota_tuya_get, ygo_mascota_tuya_set },
    { "entrada_duelo", "Entrada del duelo", k_entrada, ygo_entrada_get, ygo_entrada_set },
    { "carta_as", "Carta As", k_as, ygo_as_get, ygo_as_set },
};

/* fundas.c: the card sleeve (idea 13): the art and its frame, apart */
extern int ygo_funda_get(void);
extern void ygo_funda_set(int i);
extern int ygo_funda_marco_get(void);
extern void ygo_funda_marco_set(int i);
static const char *const k_funda[] = {
                                       "Clásica", "Ojos Azules", "Kuriboh", "Mago Oscuro", "Mago Oscuro II", "Jinzo", "Cráneo Convocado", "Ojos Rojos", "Chica Maga Oscura", "Gaia el Caballero Feroz", "Guardián Celta", "Dama Arpía", "Mago del Tiempo", "Dragón Bebé", "Espadachín de la Llama", "Soldado del Brillo Negro", "Buster Blader", "Exodia", "Dragón Barril", "Rey Rex de Dos Cabezas", "Gran Polilla", "Pescador Legendario", "Restricción de Mil Ojos", "Necrofear Oscuro", "Bestia Enmascarada", "Máquina Tragamonedas", "Dragón Zombi", "Maha Vailo", "Golem del Milenio", "Dragón Definitivo de Ojos Azules", "Dragón Negro de Cráneo", "Mago del Caos Negro", "Dragón Meteoro Negro", "Valkyrion el Guerrero Magna", "Dragón Toon de Ojos Azules", "Gaia el Campeón Dragón", "Sabio Oscuro", "Dragón del Trueno de Dos Cabezas", "Dragón de Tres Cuernos", "Reina del Cosmos", "Zera el Mant", "Caballero de Cráneo", "Dragón Mascota de Harpie", "Guardián de la Puerta", "Renunciado", "Cráneo Convocado Toon", "Maldición del Dragón", "Mil Dragones", "Alfa el Guerrero Magnético", "Beta el Guerrero Magnético", "Gamma el Guerrero Magnético", "Dragón Meteoro", "Elfa Mística", "Bruja del Bosque Negro", "Tomate Místico", "Sangan", "Gardna del Gran Escudo", "Payaso Zombi", "Fantasma Ingenioso", "Mago de la Fe", "Légamo Humanoide", "Princesa de Fuego", "Hermanas Dama Arpía", "Sanga del Trueno", "Kazejin", "Suijin", "Dokurorider", "Bruja Oscura Dunames", "Guerrero de Pizarra", "Gusano Dragón Humanoide", "Asaltante Vorse", "Fuerza de Ataque Goblin", "Elfas Gemelas", "Santa Juana", "Zoa", "Melchid la Bestia de Cuatro Caras", "Bickuribox", "Elfa Oscura", "Tortuga Cangrejo", "Espada de Garma", "Escarabajo Jabalina", "Tanque Laberinto", "Ballena Fortaleza", "Dragón Nocturno Serpiente", "Chakra", "Fiend Megacyber", "Bicho Devorador de Hombres", "Jarra Cibernética", "Parshath Caballero Aéreo", "Soldado Pingüino", "Jarra Cambiante", "Muka Muka", "Soldado Gigante de Piedra", "Muro Laberinto", "Ninja Armado", "Aprendiz de Bruja", "Gilasaurus", "Battleguard de Lava", "Seiyaryu", "Guerrero Pantera", "Jirai Gumo", NULL };
/* fundas.c: the tapete (2026-11-16): the same arts as the sleeve, in k_arte's order */
extern int ygo_tapete_get(void);
extern void ygo_tapete_set(int i);
static const char *const k_tapete[] = {
                                       "Clásico", "Ojos Azules", "Kuriboh", "Mago Oscuro", "Mago Oscuro II", "Jinzo", "Cráneo Convocado", "Ojos Rojos", "Chica Maga Oscura", "Gaia el Caballero Feroz", "Guardián Celta", "Dama Arpía", "Mago del Tiempo", "Dragón Bebé", "Espadachín de la Llama", "Soldado del Brillo Negro", "Buster Blader", "Exodia", "Dragón Barril", "Rey Rex de Dos Cabezas", "Gran Polilla", "Pescador Legendario", "Restricción de Mil Ojos", "Necrofear Oscuro", "Bestia Enmascarada", "Máquina Tragamonedas", "Dragón Zombi", "Maha Vailo", "Golem del Milenio", "Dragón Definitivo de Ojos Azules", "Dragón Negro de Cráneo", "Mago del Caos Negro", "Dragón Meteoro Negro", "Valkyrion el Guerrero Magna", "Dragón Toon de Ojos Azules", "Gaia el Campeón Dragón", "Sabio Oscuro", "Dragón del Trueno de Dos Cabezas", "Dragón de Tres Cuernos", "Reina del Cosmos", "Zera el Mant", "Caballero de Cráneo", "Dragón Mascota de Harpie", "Guardián de la Puerta", "Renunciado", "Cráneo Convocado Toon", "Maldición del Dragón", "Mil Dragones", "Alfa el Guerrero Magnético", "Beta el Guerrero Magnético", "Gamma el Guerrero Magnético", "Dragón Meteoro", "Elfa Mística", "Bruja del Bosque Negro", "Tomate Místico", "Sangan", "Gardna del Gran Escudo", "Payaso Zombi", "Fantasma Ingenioso", "Mago de la Fe", "Légamo Humanoide", "Princesa de Fuego", "Hermanas Dama Arpía", "Sanga del Trueno", "Kazejin", "Suijin", "Dokurorider", "Bruja Oscura Dunames", "Guerrero de Pizarra", "Gusano Dragón Humanoide", "Asaltante Vorse", "Fuerza de Ataque Goblin", "Elfas Gemelas", "Santa Juana", "Zoa", "Melchid la Bestia de Cuatro Caras", "Bickuribox", "Elfa Oscura", "Tortuga Cangrejo", "Espada de Garma", "Escarabajo Jabalina", "Tanque Laberinto", "Ballena Fortaleza", "Dragón Nocturno Serpiente", "Chakra", "Fiend Megacyber", "Bicho Devorador de Hombres", "Jarra Cibernética", "Parshath Caballero Aéreo", "Soldado Pingüino", "Jarra Cambiante", "Muka Muka", "Soldado Gigante de Piedra", "Muro Laberinto", "Ninja Armado", "Aprendiz de Bruja", "Gilasaurus", "Battleguard de Lava", "Seiyaryu", "Guerrero Pantera", "Jirai Gumo", NULL };
static const char *const k_funda_marco[] = { "Dorado", "Plata", "Obsidiana", "Milenio", "Neón", NULL };
/* tapete_fx.c: the tapete's light (2026-11-20) */
extern int ygo_tapete_fx_get(void);
extern void ygo_tapete_fx_set(int i);
static const char *const k_tapete_fx[] = { "No", "Luz", "Pulso", "Brillo", NULL };
/* perfil.c: the names F2 shows, in the player's language */
const char *f2_traducir(const char *es);
static const char *nombre_de(const char *const *t, int i)
{
    int k;
    for (k = 0; t[k]; k++)
        if (k == i)
            return f2_traducir(t[k]);
    return "";
}
const char *ygo_perfil_mascota_nombre(int i) { return nombre_de(k_mascota_tuya, i); }
const char *ygo_perfil_funda_nombre(int i) { return nombre_de(k_funda, i); }
const char *ygo_perfil_tapete_nombre(int i) { return nombre_de(k_tapete, i); }

static const xbox_menu_item k_funda_rows[] = {
    { "funda", "Funda", k_funda, ygo_funda_get, ygo_funda_set },
    { "funda_marco", "Marco de la funda", k_funda_marco, ygo_funda_marco_get, ygo_funda_marco_set },
    { "tapete", "Tapete", k_tapete, ygo_tapete_get, ygo_tapete_set },
    { "tapete_efecto", "Efecto del tapete", k_tapete_fx, ygo_tapete_fx_get, ygo_tapete_fx_set },
};

static const xbox_menu_item k_rival_rows[] = {
    { "rival_rapido", "Turno del rival rápido", k_rival, ygo_rival_get, ygo_rival_set },
};

/* danio.c: the damage number by the life points */
extern int ygo_danio_get(void);
extern void ygo_danio_set(int i);
static const char *const k_danio[] = { "Sí", "No", NULL };
static const xbox_menu_item k_danio_rows[] = {
    { "numero_danio", "Número de daño", k_danio, ygo_danio_get, ygo_danio_set },
};

/* cartas.c: the card mod (optional): only with its folder. Applied at the
 * next start (the images are read from the disc's paths). */
extern int ygo_cartas_get(void);
extern void ygo_cartas_set(int i);
static const char *const k_cartas[] = { "Sí", "No", NULL };
static const xbox_menu_item k_cartas_rows[] = {
    { "cartas_nuevas", "Cartas agregadas (al reiniciar)", k_cartas, ygo_cartas_get, ygo_cartas_set },
};

static void fast_menu_init(void)
{
    /* 2026-11-14, the user cleaned F2 up: "saca modo del raton, onda al hacer
     * click, cartel de premios, premios sin cuarta copia, aviso carta nueva,
     * modo experimental, volcar texturas, imagenes 2d". Those rows are
     * hidden, not dropped: still loaded from and saved to ygo_menu.ini (the
     * choices made stay in force, e.g. the PC-style pointer) and still in
     * Configuración.exe. Their section titles go with them when empty
     * (Imagen, Rendimiento). */
    xbox_MenuAddHidden(&k_snap_rows[1]);
    xbox_MenuAddFirst(&k_perfil_rows[0]);      /* the quality profile, Gráficos' first row */
    xbox_MenuAdd(&k_fps_rows[0]);       /* F8's rate, saved */
    xbox_MenuAdd(&k_ctl_rows[0]);
    /* k_ctl_rows[1], "Ver controles", left out: it shows controles.bmp,
     * which RDR's Configuración.exe drew; this port has none, so the row
     * showed nothing. */
    xbox_MenuAddHidden(&k_ctl_rows[2]);
    xbox_MenuAdd(&k_ctl_rows[3]);
    xbox_MenuAdd(&k_ctl_rows[4]);       /* 2026-11-23: Modo grabación (OBS) */
    xbox_MenuAddHidden(&k_onda_rows[0]);
    xbox_MenuAdd(&k_rival_rows[0]);
    xbox_MenuAddHidden(&k_premios_rows[0]);
    xbox_MenuAdd(&k_mascota_rows[0]);
    /* 2026-11-20 (the user: "oculta todas estas opciones"): mascot, entrance,
     * Carta As, sleeve, frame, tapete and its light out of F2 (the Perfil and
     * the duel's button set them); still loaded and saved */
    xbox_MenuAddHidden(&k_mascota_rows[1]);
    xbox_MenuAddHidden(&k_mascota_rows[2]);
    xbox_MenuAddHidden(&k_mascota_rows[3]);   /* carta_as.c: Carta As */
    xbox_MenuAddHidden(&k_funda_rows[0]);
    xbox_MenuAddHidden(&k_funda_rows[1]);
    xbox_MenuAddHidden(&k_funda_rows[2]);
    xbox_MenuAddHidden(&k_funda_rows[3]);   /* tapete_fx.c */
    xbox_MenuAddHidden(&k_danio_rows[0]);  /* 2026-11-20: hidden (the user), as in Configuración */
    xbox_MenuAdd(&k_extra_rows[0]);
    /* 2026-11-20: AUTO hidden (the duel's plate sets it, saved; No at first);
     * saving after a win always on */
    xbox_MenuAddHidden(&k_auto_rows[0]);
    xbox_MenuAddHidden(&k_guardado_rows[0]);
    xbox_MenuAddHidden(&k_extra_rows[1]);
    xbox_MenuAddHidden(&k_extra_rows[2]);
    /* k_ctl_rows[5], "Jugar cartas rápido", left out (2026-11-09: the
     * user wanted the speed-up gone with the rest of the Joey clicks) */
    xbox_MenuAddHidden(&k_fast_rows[1]);
    xbox_MenuHide("tex_dump");
    {
        /* The voices are optional (YGO_VOCES, modding\voces_latino): no
         * folder, no row. */
        const char *v = getenv("YGO_VOCES");
        if (v && *v && GetFileAttributesA(v) != INVALID_FILE_ATTRIBUTES) {
            xbox_MenuAdd(&k_voces_rows[0]);
            xbox_MenuAdd(&k_voces_rows[1]);
        }
    }
    {
        const char *c = getenv("YGO_CARTAS");
        if (c && *c)
            xbox_MenuAdd(&k_cartas_rows[0]);   /* 2026-11-26: shown again, "Cartas agregadas" (the user) */
    }
    xbox_MenuAdd(&k_quit_rows[0]);
    xbox_MenuAdd(&k_quit_rows[1]);
}

/* 2026-11-21 (the user: "podes agregar algo que detecte cuando el jugador
 * abra el juego que pc tiene y en base a eso tomar la mejor decision"): the
 * PC's cores and RAM, once. The video memory is the kit's (nv2a_pb_d3d11.c
 * sizes its texture pool from the adapter). */
static unsigned s_pc_logicos, s_pc_fisicos;
static unsigned long long s_pc_ram_mb, s_pc_vram_mb;

static void pc_detectar(void)
{
    static int hecho;
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION buf[256];
    DWORD len = sizeof buf, i;
    MEMORYSTATUSEX ms;
    if (hecho)
        return;
    hecho = 1;
    s_pc_logicos = (unsigned)GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (GetLogicalProcessorInformation(buf, &len))
        for (i = 0; i < len / sizeof buf[0]; i++)
            if (buf[i].Relationship == RelationProcessorCore)
                s_pc_fisicos++;
    if (!s_pc_fisicos)
        s_pc_fisicos = s_pc_logicos;
    ms.dwLength = sizeof ms;
    if (GlobalMemoryStatusEx(&ms))
        s_pc_ram_mb = ms.ullTotalPhys >> 20;
    {
        /* the card with the most memory of its own, as Configuración reads
         * it (the display class's HardwareInformation.qwMemorySize) */
        HKEY cls;
        if (!RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                           "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}",
                           0, KEY_READ, &cls)) {
            char sub[64];
            DWORD n;
            for (i = 0; n = sizeof sub, !RegEnumKeyExA(cls, i, sub, &n, NULL, NULL, NULL, NULL); i++) {
                HKEY k;
                unsigned long long q = 0;
                DWORD d = 0, sz;
                if (RegOpenKeyExA(cls, sub, 0, KEY_READ, &k))
                    continue;
                sz = sizeof q;
                if (RegQueryValueExA(k, "HardwareInformation.qwMemorySize", NULL, NULL, (BYTE *)&q, &sz) || !q) {
                    sz = sizeof d;
                    if (!RegQueryValueExA(k, "HardwareInformation.MemorySize", NULL, NULL, (BYTE *)&d, &sz))
                        q = d;
                }
                RegCloseKey(k);
                if ((q >> 20) > s_pc_vram_mb)
                    s_pc_vram_mb = q >> 20;
            }
            RegCloseKey(cls);
        }
    }
    /* the level, with Configuración's rules (Configuracion.cs Detect) */
    if (s_pc_logicos >= 8 && s_pc_ram_mb >= 15360 && s_pc_vram_mb >= 6144)
        s_pc_nivel = 4;
    else if (s_pc_logicos >= 6 && s_pc_ram_mb >= 11264 && s_pc_vram_mb >= 3584)
        s_pc_nivel = 3;
    else if (s_pc_logicos >= 4 && s_pc_ram_mb >= 5632 && s_pc_vram_mb >= 1536)
        s_pc_nivel = 2;
    else
        s_pc_nivel = 1;
}

static void fast_mode_from_ini(void)
{
    char line[256], path[MAX_PATH];
    /* the settings live in YGO_AJUSTES since 2026-11-10 (as xbox_MenuLoad) */
    const char *dir = getenv("YGO_AJUSTES");
    FILE *f;
    if (dir && *dir)
        snprintf(path, sizeof path, "%s\\ygo_menu.ini", dir);
    else
        snprintf(path, sizeof path, "ygo_menu.ini");
    f = fopen(path, "r");
    if (!f)
        f = fopen("ygo_menu.ini", "r");
    /* 2026-11-21 (the user: "lo quiero abrir el juego y que vaya a 165
     * clavado"; the 04:22 profile: the game's main thread 30 % in D3D's
     * KickOff, sub_00375B70, waiting for the walker): on by default now, so
     * the game and the kit's drawing overlap; "No" in F2 turns it off. */
    /* 2026-11-21 05:2x: the row is hidden in F2 and in Configuración (the
     * user, 11-14), so the PC decides: the game and the walker overlap when
     * there are 4 or more hardware threads for them and the title's other
     * threads; on 2-3 they would fight over the cores. The ini's line is
     * read no more. RECOMP_DEBUG=run_ahead=0/1 still overrides. */
    pc_detectar();
    s_fast_choice = s_pc_logicos >= 4;
    if (f)
        fclose(f);
    (void)line;
    if (s_fast_choice && !recomp_env(RENV_RUN_AHEAD))
        recomp_env_set(RENV_RUN_AHEAD, "1");
    /* 2026-11-24 (el registro de la PC con GT 1030, 4 hilos: 367 cortes de
     * sonido "[XA2] underrun" en el duelo; en la PC de 8+ hilos, 0): con 4
     * buffers de 512 la reserva es 43 ms (marca baja 21 ms) y el hilo del
     * APU, con prioridad normal, pierde turnos con el juego, el kit y la
     * carga de texturas. En 4 hilos o menos: 8 buffers (85 ms, marca baja
     * 43 ms) y prioridad alta (no la crítica: apu_xaudio2.h). Las variables
     * RECOMP_AUDIO_* puestas a mano siguen mandando. */
    if (s_pc_logicos <= 4) {
        if (!recomp_env(RENV_AUDIO_BUF_COUNT))
            recomp_env_set(RENV_AUDIO_BUF_COUNT, "8");
        if (!recomp_env(RENV_AUDIO_THREAD_PRIORITY))
            recomp_env_set(RENV_AUDIO_THREAD_PRIORITY, "highest");
    }
    fprintf(stderr, "[PC] %u nucleos (%u hilos), RAM %llu MB: giro antes de dormir %s; perfil %s,"
            " texturas mejoradas en memoria hasta %s MB\n",
            s_pc_fisicos, s_pc_logicos, s_pc_ram_mb, getenv("RECOMP_WAIT_SPIN_US") ? "si" : "no",
            k_perfil[s_perfil], getenv("RECOMP_TEXPACK_MB") ? getenv("RECOMP_TEXPACK_MB") : "1536");
    fprintf(stderr, "[rdr] modo experimental: %s\n",
            recomp_env(RENV_RUN_AHEAD) && strcmp(recomp_env(RENV_RUN_AHEAD), "0")
                ? "on (run_ahead)" : "off");
}

/* 2026-11-21: Configuración.exe's quality profile (k_perfil) and, from it,
 * the enhanced textures kept in RAM: before the pack loads (host_enhance_init) */
static void perfil_memoria(void)
{
    char line[256], path[MAX_PATH];
    const char *dir = getenv("YGO_AJUSTES");
    FILE *f;
    if (dir && *dir)
        snprintf(path, sizeof path, "%s\\ygo_menu.ini", dir);
    else
        snprintf(path, sizeof path, "ygo_menu.ini");
    f = fopen(path, "r");
    if (!f)
        f = fopen("ygo_menu.ini", "r");
    pc_detectar();
    s_perfil = 0;
    if (f) {
        while (fgets(line, sizeof line, f)) {
            int k;
            line[strcspn(line, "\r\n")] = 0;
            if (strncmp(line, "perfil=", 7))
                continue;
            for (k = 0; k_perfil[k]; k++)
                if (!strcmp(line + 7, k_perfil[k]))
                    s_perfil = k;
        }
        fclose(f);
    }
    if (!getenv("RECOMP_TEXPACK_MB")) {
        /* 2026-11-21: the enhanced textures kept in RAM (nv2a_tex_pack.c,
         * 1536 MB before): by the profile, or by the RAM for Recomendado and
         * Personalizado */
        static const unsigned por_perfil[] = { 0, 256, 768, 1536, 2048, 0 };
        unsigned mb = por_perfil[s_perfil];
        char v[16];
        if (!mb)
            mb = s_pc_ram_mb < 6000 ? 384 : s_pc_ram_mb < 12000 ? 768 : s_pc_ram_mb < 24000 ? 1536 : 2048;
        snprintf(v, sizeof v, "%u", mb);
        _putenv_s("RECOMP_TEXPACK_MB", v);
    }
}

static void pack_menu_init(void)
{
    size_t i;
    if (nv2a_tex_pack_button_style() < 0)
        nv2a_tex_pack_set_button_style(0);   /* keyboard until something is used */
    CreateThread(NULL, 0, btn_auto_thread, NULL, 0, NULL);
    for (i = 0; i < sizeof k_pack_rows / sizeof k_pack_rows[0]; i++)
        xbox_MenuAdd(&k_pack_rows[i]);
}

static void host_enhance_init(void)
{
    perfil_memoria();           /* the pack's RAM budget, before it loads */
#ifdef RECOMP_ENV_HAVE_ENHANCE_KEYS
    xbox_enhance_init(".", NULL);
    enhance_cfg_report_unused();
    fflush(stderr);
#else
    /* Without the layer nothing loaded RECOMP_TEXTURES_PACK (env.bat), so the
     * upscaled textures and fonts never reached the screen: load it here. */
    const char *pack = getenv("RECOMP_TEXTURES_PACK");
    /* the card mod's HD pack (kit: merged into the pack on load; with no
     * texture pack, loaded on its own). This build has no enhance layer, so
     * the kit's enhance.c never set it (2026-11-11: thumbnails blurry). */
    const char *extra = getenv("RECOMP_TEXTURES_PACK_EXTRA");
    char err[128];
    int n;
    nv2a_tex_pack_set_extra(extra);
    /* each language's pack (es, en, fr, de, it), read like the pack; this
     * build has no enhance layer, so it is set here too (2026-11-12: in
     * English the cards and menus were blurry, the packs never loaded) */
    nv2a_tex_pack_set_more(getenv("RECOMP_TEXTURES_PACK_MORE"));
    if ((!pack || !*pack) && extra && *extra)
        pack = extra;
    if (pack && *pack) {
        n = nv2a_tex_pack_load(pack, err, sizeof err);
        if (n < 0)
            fprintf(stderr, "[rdr] textures.pack %s: %s; no pack\n", pack, err);
        else
            fprintf(stderr, "[rdr] textures.pack %s: %d entries (%u font)\n", pack, n,
                    nv2a_tex_pack_kind_count(XRTP_KIND_FONT));
        fflush(stderr);
    }
#endif
}

#if defined(_WIN32)
/* rdr-port: the release layout, next to the exe:
 *     Game\         the player's own game folder (default.xbe and the .dat),
 *                   or one folder inside it holding them
 *     Mods-Extras\  videos\<name>.mp4, texturas\textures.pack, replaced files
 * When Game\ has the game, every setting env.bat would give is set here
 * (unless the environment already has it), so the exe runs on its own. */
static void set_default(const char *k, const char *v)
{
    if (!getenv(k))
        _putenv_s(k, v);
}

/* The first file called `name` (any case) in dir or its subfolders, up to
 * `depth` levels down, into out; 0 when none. Skips texture dump folders.
 * movies.c finds the videos with it too. */
int rdr_find_file(const char *dir, const char *name, char *out, size_t n, int depth)
{
    WIN32_FIND_DATAA fd;
    HANDLE h;
    char p[MAX_PATH * 2];
    DWORD a;
    int found = 0;

    snprintf(p, sizeof p, "%s\\%s", dir, name);
    a = GetFileAttributesA(p);
    if (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY)) {
        snprintf(out, n, "%s", p);
        return 1;
    }
    if (depth <= 0)
        return 0;
    snprintf(p, sizeof p, "%s\\*", dir);
    h = FindFirstFileA(p, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != '.'
                && _stricmp(fd.cFileName, "dump") != 0) {
            snprintf(p, sizeof p, "%s\\%s", dir, fd.cFileName);
            found = rdr_find_file(p, name, out, n, depth - 1);
        }
    } while (!found && FindNextFileA(h, &fd));
    FindClose(h);
    return found;
}

static void portable_defaults(void)
{
    static const char *const env[][2] = {
        { "RECOMP_AC97_READY", "1" }, { "RECOMP_PB_EXEC", "1" },
        { "RECOMP_PB_BACKEND", "d3d11" }, { "RECOMP_VBLANK", "paced" },
        { "RECOMP_KEYBOARD", "1" }, { "RECOMP_MOUSE", "1" },
        /* ygo-port: the same switches as ygo-apex\env.bat (2026-11-10, the
         * player's folder): the pointer mouse, and RECOMP_EXT_VMA, without
         * which the Alchemy heap gets no memory and the game never starts */
        { "YGO_MOUSE_MODE", "puntero" }, { "YGO_MOUSE_STEP", "120" },
        { "RECOMP_EXT_VMA", "1" },
        /* 2026-11-18: "el disco esta rayado o sucio" tras ~25 min y varios duelos:
         * xbox_HeapAlloc out of memory (16-20 MB pedidos, 49.3/50.5 MB usados) con
         * solo 18 MB vivos y 17 MB de sobra en bloques reusados: sin reclaim un
         * bloque liberado se entrega entero al siguiente pedido, por chico que sea */
        { "RECOMP_HEAP_RECLAIM", "1" },
        { "RECOMP_CONTIG_AVOID_IMAGE", "1" }, { "RDR_RENDER_SCALE", "2" },
        { "RDR_ANISO", "16" }, { "RDR_ASPECT", "4:3" }, { "RECOMP_WIDESCREEN", "0" },
        { "RECOMP_LANGUAGE", "es" }, { "RECOMP_TEXTURES_DUMP", "0" },
        { "RDR_FULLSCREEN", "0" }, { "RECOMP_D3D11_SKIP_RT", "1" },
    };
    char root[MAX_PATH], game[MAX_PATH + 16], p[MAX_PATH + 64];
    char *slash;
    size_t i;

    if (!GetModuleFileNameA(NULL, root, sizeof root) || !(slash = strrchr(root, '\\')))
        return;
    *slash = 0;
    snprintf(game, sizeof game, "%s\\Game", root);
    /* the game anywhere inside Game\ (pasted whole, or in folders of its own) */
    if (!rdr_find_file(game, "default.xbe", p, sizeof p, 4))
        return;
    *strrchr(p, '\\') = 0;
    snprintf(game, sizeof game, "%s", p);
    SetCurrentDirectoryA(root);             /* ygo_menu.ini, logs */
    /* The saves (UDATA/TDATA) in the port's own folder, not in
     * %LOCALAPPDATA%\xboxrecomp (2026-11-08, the user's request). */
    /* 2026-11-25: Partidas\<profile> (perfil_dir.c, below), not Partidas */
    /* ygo-port: F2's and Configuración.exe's settings, kept by an update */
    snprintf(p, sizeof p, "%s\\Ajustes", root);
    set_default("YGO_AJUSTES", p);
    /* 2026-11-22 (the user: "todo lo que sea datos o vaya de la mano con un
     * perfil, cartas ganadas, historial duelo ... vaya en la carpeta
     * Partidas"; the folder copied to 2 more PCs without Partidas got new
     * saves but the user's history and rivals): the profile's own files
     * (perfil.ini, duelos.txt, cartas_nuevas.txt, prestados.txt,
     * premios_registro.txt, respaldos\) in Partidas\Perfil, beside the
     * saves. Ajustes\ keeps only settings. */
    /* 2026-11-25 (the user: a folder per profile, and in it one per game
     * slot): perfil_dir.c sets RECOMP_SAVE_DIR and YGO_PERFIL to
     * Partidas\<active profile> and moves the old Partidas\Perfil there */
    {
        extern void ygo_perfiles_iniciar(const char *root);
        ygo_perfiles_iniciar(root);
    }
    /* 2026-11-21: RECOMP_GUEST_CPUS (the title's threads on one core) was
     * tried for the door's semaphore hand-overs and taken out: the 03:25
     * log had the door 462 ms (495 before) but the duel worse, 22 hitches of
     * 30-73 ms (6 of 25-43 before): the monster and effect .igb loaded at
     * each attack by the loader worker, parallel on another core until then,
     * took the main thread's core. Every core, as before.
     * Instead, the same hand-overs without losing the cores: a wait spins
     * 50 us before it blocks (kernel_sync.c, RECOMP_WAIT_SPIN_US), so the
     * answer of a hand-over is caught awake. Ajustes\sin_giro.txt: none. */
    {
        char f[MAX_PATH + 96];
        snprintf(f, sizeof f, "%s\\sin_giro.txt", p);
        /* 2026-11-21 05:2x: only with 6 or more hardware threads: spinning
         * keeps a core busy, and on a small PC that core is the game's */
        pc_detectar();
        if (GetFileAttributesA(f) == INVALID_FILE_ATTRIBUTES && s_pc_logicos >= 6)
            set_default("RECOMP_WAIT_SPIN_US", "50");
        /* 2026-11-21 04:2x (the user: "que hacemos para mejorar lo del
         * dibujo?"): Ajustes\perfil_dibujo.txt turns on the toolkit's
         * whole-process sampler (host_prof.c, 1000 Hz, every thread, the
         * kit's walker that draws included), written at exit to
         * perfil_dibujo_salida.txt beside the exe, to see where the kit's
         * 3-5 ms of drawing a frame go. Off without the file (it costs). */
        snprintf(f, sizeof f, "%s\\perfil_dibujo.txt", p);
        if (GetFileAttributesA(f) != INVALID_FILE_ATTRIBUTES) {
            char v[MAX_PATH + 96];
            snprintf(v, sizeof v, "prof=1000,prof_out=%s\\perfil_dibujo_salida.txt", root);
            set_default("RECOMP_TRACE", v);
        }
    }
    /* rdr-port: the package ships ygo_menu.default.ini, never ygo_menu.ini,
     * so unpacking an update over a folder keeps the player's settings; the
     * first run starts from the defaults. */
    if (GetFileAttributesA("ygo_menu.ini") == INVALID_FILE_ATTRIBUTES)
        CopyFileA("ygo_menu.default.ini", "ygo_menu.ini", TRUE);
    snprintf(p, sizeof p, "%s\\registro.txt", root);    /* the run's log, for support */
    {
        /* 2026-11-19: the last run's log kept (after a crash the player opens
         * the game again before sending it, and it was overwritten) */
        char ant[MAX_PATH + 32];
        snprintf(ant, sizeof ant, "%s\\registro_anterior.txt", root);
        CopyFileA(p, ant, FALSE);
    }
    set_default("RECOMP_STDIO_LOG", p);
    /* 2026-11-21: the texture pack's miss list (RECOMP_TRACE=tex_pack) no
     * longer on by default: every 600 frames it wrote ~600 lines from the
     * walker, and the game waited 8-10 ms for it (the 05:05 log's [LENTO]
     * 8401, 10801, 13201, 14401, 15001: "juego espera al kit" 8-10 ms with
     * the kit drawing 2.5) */
    /* 05:3x (the user: "no rompio nada? se puede jugar por horas"): a [MEM]
     * line every minute (RAM and video memory), so any long session's log
     * shows whether memory grows */
    set_default("RECOMP_TRACE", "mem=60");
    set_default("RECOMP_GAME_FILES", game);
    for (i = 0; i < sizeof env / sizeof env[0]; i++)
        set_default(env[i][0], env[i][1]);
    {   /* 2026-11-25: zips unpacked into a folder named after them (mods_carpeta.c) */
        extern void ygo_mods_acomodar(const char *root);
        ygo_mods_acomodar(root);
    }
    snprintf(p, sizeof p, "%s\\Mods-Extras", root);
    CreateDirectoryA(p, NULL);
    set_default("RECOMP_MODS_DIR", p);
    snprintf(p, sizeof p, "%s\\Mods-Extras\\texturas\\dump", root);
    set_default("RECOMP_TEXTURES_DUMP_DIR", p);
    {
        char mods[MAX_PATH + 16], pack[MAX_PATH * 2];
        char mas_l[MAX_PATH * 5] = "";      /* RECOMP_TEXTURES_PACK_MORE, set at the end */
        const char *lang_l = "es";          /* the game language's code */
        snprintf(mods, sizeof mods, "%s\\Mods-Extras", root);
        /* the BC3 pack under either name, anywhere in Mods-Extras\ */
        if (rdr_find_file(mods, "textures_bc3.pack", pack, sizeof pack, 4)
                || rdr_find_file(mods, "textures.pack", pack, sizeof pack, 4))
            set_default("RECOMP_TEXTURES_PACK", pack);
        /* ygo-port: the game language's textures (cards, menus and card
         * thumbnails with its own text: tools/idiomas_pack.py,
         * tools/separar_espanol.py), textures_<xx>.pack anywhere in
         * Mods-Extras\ (kit: RECOMP_TEXTURES_PACK_MORE, read like the pack).
         * Only that language's: the thumbnail sheets are one file for every
         * language (the same hash), so each pack has its own. The language is
         * the F2 one ("language=" in YGO_AJUSTES\ygo_menu.ini, saved by
         * xbox_MenuLoad's menu, which loads after this), else RECOMP_LANGUAGE. */
        {
            static const struct { const char *ini, *code; } k_idiomas[] = {
                { "Engl", "en" }, { "Espa", "es" }, { "Fran", "fr" }, { "Deut", "de" }, { "Ital", "it" } };
            const char *ajustes = getenv("YGO_AJUSTES"), *code = NULL, *env = getenv("RECOMP_LANGUAGE");
            char ini[MAX_PATH], line[256], nombre[32], uno[MAX_PATH * 2];
            unsigned li;
            FILE *fi;
            if (ajustes && *ajustes) {
                snprintf(ini, sizeof ini, "%s\\ygo_menu.ini", ajustes);
                if ((fi = fopen(ini, "r")) != NULL) {
                    while (!code && fgets(line, sizeof line, fi))
                        if (!strncmp(line, "language=", 9))
                            for (li = 0; li < sizeof k_idiomas / sizeof k_idiomas[0]; li++)
                                if (!strncmp(line + 9, k_idiomas[li].ini, 4))
                                    code = k_idiomas[li].code;
                    fclose(fi);
                }
            }
            if (!code && env)
                for (li = 0; li < sizeof k_idiomas / sizeof k_idiomas[0]; li++)
                    if (!_strnicmp(env, k_idiomas[li].code, 2))
                        code = k_idiomas[li].code;
            if (!code)
                code = "es";
            lang_l = code;
            snprintf(nombre, sizeof nombre, "textures_%s.pack", code);
            if (rdr_find_file(mods, nombre, uno, sizeof uno, 4))
                snprintf(mas_l, sizeof mas_l, "%s", uno);
        }
        /* ygo-port: the Latin American voices (optional): the folder with
         * textos.txt (tools/voces_mod.py) anywhere in Mods-Extras\ */
        if (rdr_find_file(mods, "textos.txt", pack, sizeof pack, 4)) {
            *strrchr(pack, '\\') = 0;
            set_default("YGO_VOCES", pack);
        }
        /* ygo-port: the card mod (optional, tools/cartas_mod.py): the folder
         * with cartas.txt anywhere in Mods-Extras\ (cartas.c); its HD pack
         * goes with the texture pack (kit: RECOMP_TEXTURES_PACK_EXTRA). */
        if (rdr_find_file(mods, "cartas.txt", pack, sizeof pack, 4)) {
            *strrchr(pack, '\\') = 0;
            set_default("YGO_CARTAS", pack);
            {
                /* its card thumbnail sheets in the game language
                 * (cartas_<xx>.pack: one sheet for every language, the same
                 * hash), after the language's textures so they win */
                char lp[MAX_PATH * 2 + 32];
                snprintf(lp, sizeof lp, "%s\\cartas_%s.pack", pack, lang_l);
                if (GetFileAttributesA(lp) != INVALID_FILE_ATTRIBUTES) {
                    if (*mas_l)
                        strncat(mas_l, ";", sizeof mas_l - strlen(mas_l) - 1);
                    strncat(mas_l, lp, sizeof mas_l - strlen(mas_l) - 1);
                }
            }
            strncat(pack, "\\cartas.pack", sizeof pack - strlen(pack) - 1);
            if (GetFileAttributesA(pack) != INVALID_FILE_ATTRIBUTES)
                set_default("RECOMP_TEXTURES_PACK_EXTRA", pack);
        }
        if (*mas_l)
            set_default("RECOMP_TEXTURES_PACK_MORE", mas_l);
    }
}

/* ygo-port: started on its own (Yu-Gi-Oh.exe in the port's folder, or from
 * Configuración.exe), not by run.bat: env.bat's switches are read from the
 * file itself, as cmd would set them, so there is one list of them. Its
 * "set K=V", "set "K=V"" and "for %%I in ("dir") do set "K=%%~fIrest""
 * lines; %~dp0 is env.bat's folder, %NAME% a variable already set. The
 * working folder becomes env.bat's (ygo-apex, as run.bat's cd), and the log
 * goes to its runs\last_err.txt, as run.bat redirects it. A variable the
 * environment already has wins (set_default). No launcher program in
 * between: a small exe starting the game hidden was taken for malware. */
static void env_bat_expand(const char *in, const char *dp0, char *out, size_t n)
{
    size_t o = 0;
    while (*in && o + 1 < n) {
        if (!strncmp(in, "%~dp0", 5)) {
            o += (size_t)snprintf(out + o, n - o, "%s", dp0);
            in += 5;
        } else if (*in == '%' && strchr(in + 1, '%') && in[1] != '%') {
            const char *e = strchr(in + 1, '%');
            char name[128];
            const char *v;
            size_t l = (size_t)(e - in - 1);
            if (l >= sizeof name)
                l = sizeof name - 1;
            memcpy(name, in + 1, l);
            name[l] = 0;
            v = getenv(name);
            o += (size_t)snprintf(out + o, n - o, "%s", v ? v : "");
            in = e + 1;
        } else
            out[o++] = *in++;
        if (o >= n)
            o = n - 1;
    }
    out[o] = 0;
}

static void env_bat_defaults(void)
{
    char exe[MAX_PATH], bat[MAX_PATH + 64], dp0[MAX_PATH + 64], line[1024], val[2048];
    static const char *const where[] = { "env.bat", "ygo-apex\\env.bat", "..\\..\\env.bat" };
    char *slash;
    FILE *f = NULL;
    size_t i;

    if (getenv("RECOMP_PB_BACKEND"))           /* run.bat already set them */
        return;
    if (!GetModuleFileNameA(NULL, exe, sizeof exe) || !(slash = strrchr(exe, '\\')))
        return;
    *slash = 0;
    for (i = 0; i < sizeof where / sizeof where[0] && !f; i++) {
        char p[MAX_PATH + 64];
        snprintf(p, sizeof p, "%s\\%s", exe, where[i]);
        if (GetFullPathNameA(p, sizeof bat, bat, NULL) && (f = fopen(bat, "r")) != NULL)
            break;
    }
    if (!f)
        return;
    snprintf(dp0, sizeof dp0, "%s", bat);
    *(strrchr(dp0, '\\') + 1) = 0;             /* with its backslash, as %~dp0 */
    while (fgets(line, sizeof line, f)) {
        char *s = line, *k, *v, *eq;
        size_t l = strlen(s);
        while (l && (s[l - 1] == '\n' || s[l - 1] == '\r' || s[l - 1] == ' '))
            s[--l] = 0;
        while (*s == ' ' || *s == '\t')
            s++;
        if (!_strnicmp(s, "for %%", 6)) {
            /* for %%I in ("dir") do set "K=%%~fIrest" */
            char dir[MAX_PATH], full[MAX_PATH], *a = strchr(s, '"'), *b, *st;
            if (!a || !(b = strchr(a + 1, '"')) || !(st = strstr(b, "set \"")))
                continue;
            *b = 0;
            env_bat_expand(a + 1, dp0, dir, sizeof dir);
            k = st + 5;
            if (!(eq = strchr(k, '=')) || !(v = strstr(eq, "%%~f")))
                continue;
            *eq = 0;
            v += 5;                            /* past %%~fI */
            if ((l = strlen(v)) && v[l - 1] == '"')
                v[l - 1] = 0;
            if (!GetFullPathNameA(dir, sizeof full, full, NULL))
                continue;
            l = strlen(full);
            if (l && full[l - 1] == '\\')
                full[l - 1] = 0;
            env_bat_expand(v, dp0, line, sizeof line);   /* line is done with */
            snprintf(val, sizeof val, "%s%s", full, line);
            set_default(k, val);
            continue;
        }
        if (_strnicmp(s, "set ", 4))
            continue;
        k = s + 4;
        while (*k == ' ')
            k++;
        if (*k == '"') {
            k++;
            if ((l = strlen(k)) && k[l - 1] == '"')
                k[l - 1] = 0;
        }
        if (!(eq = strchr(k, '=')) || eq == k)
            continue;
        *eq = 0;
        env_bat_expand(eq + 1, dp0, val, sizeof val);
        set_default(k, val);
    }
    fclose(f);
    SetCurrentDirectoryA(dp0);                 /* ygo-apex, as run.bat's cd */
    snprintf(val, sizeof val, "%sruns", dp0);
    CreateDirectoryA(val, NULL);
    snprintf(val, sizeof val, "%sruns\\last_err.txt", dp0);
    set_default("RECOMP_STDIO_LOG", val);
}

/* The Windows build links as a GUI-subsystem program, which starts here. */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;
    env_bat_defaults();
    portable_defaults();
    recomp_env_init();   /* the environment, read once, before any thread */
    host_stdio_setup();
    host_enhance_init();
    xbox_log_thread_role("process-main", 0);
    xbox_HostWindowSetTitle("Yu-Gi-Oh! The Dawn of Destiny");
    return host_main();
}
#endif

/* Console entry point: every non-Windows host, and console builds on Windows
 * (for debugging -- lets you see printf output).
 *
 * POSIX: the process main thread belongs to the window. AppKit only runs its
 * event loop there, so xbox_HostWindowMain (toolkit fb_present_sdl.c) opens
 * an SDL window here and runs host_main -- the whole boot and the guest --
 * on a "guest-main" pthread. RECOMP_HEADLESS=1 keeps the guest on this
 * thread with no window. */
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    recomp_env_init();   /* the environment, read once, before any thread */
    host_stdio_setup();
    host_enhance_init();
    /* The initial thread, named first: the toolkit leaves its OS name alone
     * (renaming it breaks pgrep/ps). host_main then logs guest-main, on this
     * thread when headless or on Windows, on its own pthread when windowed. */
    xbox_log_thread_role("process-main", 0);
    /* The window's title bar; without it, the certificate's name. */
    xbox_HostWindowSetTitle("Yu-Gi-Oh! The Dawn of Destiny");
#if defined(_WIN32)
    return host_main();
#else
#if defined(__APPLE__)
    /* Metal is the Mac's backend unless RECOMP_PB_BACKEND says otherwise: the
     * CPU rasteriser runs a title at a few frames a second there.
     * RECOMP_PB_BACKEND=cpu still asks for it. Set before the window reads
     * it, since a Metal window is a CAMetalLayer one. */
    {
        const char *be = recomp_env(RENV_PB_BACKEND);
        if (!be || !*be)
            recomp_env_set(RENV_PB_BACKEND, "metal");
    }
#endif
    return xbox_HostWindowMain(host_main);
#endif
}
