/*
 * ygo-port bring-up: the engine's allocations (Alchemy memory manager).
 *
 * Every engine allocation goes through the manager in [0x670D1C]
 * (vtable 0x46AE80): +0xA4 alloc(size) -> +0x144 sub_001AF320, thiscall
 * (old_ptr, size, align), ret 0xC. The boot hung because an allocation came
 * back NULL (the string table's singleton, [0x670D08], stayed 0 and every
 * use after it went through a null pointer). This logs the first calls and
 * every NULL one, with the manager's fields, to see why.
 *
 * YGO_MEM_TRACE=0 turns it off. recomp_0053.c's definition is renamed _pad
 * (tools/rename_pads.py).
 */
#include <stdio.h>
#include <stdlib.h>
#include "recomp/gen/recomp_types.h"

void sub_001AF320_pad(void);
void sub_0019B400_pad(void);
void sub_00121449_pad(void);

static int trace_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("YGO_MEM_TRACE");
        on = !(e && *e == '0');
    }
    return on;
}

/* The heap's morecore (sbrk over VirtualAlloc; vtable 0x466DE8 +0x6C),
 * thiscall (increment), ret 4. */
void sub_0019B400(void)
{
    static unsigned long n;
    uint32_t inc = MEM32(g_esp + 4), from = MEM32(g_esp);
    sub_0019B400_pad();
    if (trace_on() && ++n <= 30) {
        fprintf(stderr, "[MEM] morecore(%d) from %08X -> %08X\n", (int)inc, from, g_eax);
        fflush(stderr);
    }
}

/* XAPI VirtualAlloc(addr, size, type, protect), stdcall, ret 0x10. */
void sub_00121449(void)
{
    static unsigned long n;
    uint32_t addr = MEM32(g_esp + 4), size = MEM32(g_esp + 8);
    uint32_t type = MEM32(g_esp + 12), prot = MEM32(g_esp + 16), from = MEM32(g_esp);
    sub_00121449_pad();
    if (trace_on() && ++n <= 40) {
        fprintf(stderr, "[MEM] VirtualAlloc(%08X, 0x%X, type %X, prot %X) from %08X -> %08X\n",
                addr, size, type, prot, from, g_eax);
        fflush(stderr);
    }
}

void sub_001AF320(void)
{
    static int on = -1;
    static unsigned long n, nulls;
    uint32_t mgr = g_ecx, old = MEM32(g_esp + 4), size = MEM32(g_esp + 8);
    uint32_t align = MEM32(g_esp + 12), from = MEM32(g_esp);

    if (on < 0) {
        const char *e = getenv("YGO_MEM_TRACE");
        on = !(e && *e == '0');
    }
    sub_001AF320_pad();
    if (!on)
        return;
    n++;
    if (n <= 40 || (!g_eax && nulls < 20)) {
        if (!g_eax)
            nulls++;
        fprintf(stderr, "[MEM] #%lu alloc mgr=%08X old=%08X size=0x%X align=0x%X from %08X -> %08X%s\n",
                n, mgr, old, size, align & 0xFFFF, from, g_eax, g_eax ? "" : "  <== NULL");
        if (!g_eax && nulls == 1) {
            int k;
            fprintf(stderr, "  manager fields:");
            for (k = 0; k < 0x78; k += 4)
                fprintf(stderr, "%s+%02X=%08X", k % 32 ? " " : "\n   ", k, MEM32(mgr + k));
            fprintf(stderr, "\n");
        }
        fflush(stderr);
    }
}
