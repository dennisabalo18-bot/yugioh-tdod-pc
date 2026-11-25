/*
 * buscar_cursor.c - find the game's cursor variables with the player's help
 * (bring-up tool for the Joey-style mouse; YGO_SCAN=1 turns it on).
 *
 * F10 in the game window takes a memory snapshot, with a beep. The player
 * moves the game's cursor one square the same way between snapshots (e.g.
 * right, right, right through the hand) and presses F10 after each move.
 * From the second snapshot on, only the values that changed by the same
 * small step every time remain: the cursor's index (byte, int16 or int32)
 * and things that follow it. After every snapshot from the third the
 * survivors are written to stderr as "[SCAN] ...". F10 with Shift starts a
 * new search.
 *
 * Looked at: .data and BSS, the low heap and the extended VMA's committed
 * pages (the engine's heap lives there in this game).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

extern ptrdiff_t g_xbox_mem_offset;
int guest_vmem_committed(uint32_t va, uint32_t bytes);

typedef struct { uint32_t va, len; int ext; } Range;
static const Range k_ranges[] = {
    { 0x005FB9E0u, 0x0081D000u - 0x005FB9E0u, 0 },   /* .data and BSS */
    { 0x00FD0000u, 0x04000000u - 0x00FD0000u, 0 },   /* low heap */
    { 0x74000000u, 0x7FFE0000u - 0x74000000u, 1 },   /* extended VMA */
};
#define N_RANGES (sizeof k_ranges / sizeof k_ranges[0])

typedef struct { uint32_t va; int32_t last, delta; uint8_t size; } Cand;

static uint8_t *s_first;           /* the first snapshot, all ranges */
static size_t   s_first_len;
static Cand    *s_cand;
static size_t   s_ncand, s_cap;
static int      s_snaps;

static int read_guest(uint32_t va, void *out, uint32_t n)
{
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), (uint8_t *)g_xbox_mem_offset + va,
                             out, n, &got) && got == n;
}

static int32_t value(const uint8_t *p, int size)
{
    if (size == 1) return *p;
    if (size == 2) { int16_t v; memcpy(&v, p, 2); return v; }
    { int32_t v; memcpy(&v, p, 4); return v; }
}

static void take_first(void)
{
    size_t off = 0, r;
    s_first_len = 0;
    for (r = 0; r < N_RANGES; r++)
        s_first_len += k_ranges[r].len;
    free(s_first);
    s_first = (uint8_t *)calloc(1, s_first_len);
    if (!s_first) {
        fprintf(stderr, "[SCAN] out of memory for the first snapshot\n");
        return;
    }
    for (r = 0; r < N_RANGES; r++) {
        uint32_t k;
        for (k = 0; k < k_ranges[r].len; k += 0x1000u) {
            uint32_t va = k_ranges[r].va + k;
            if (k_ranges[r].ext && !guest_vmem_committed(va, 0x1000u))
                continue;
            read_guest(va, s_first + off + k, 0x1000u);
        }
        off += k_ranges[r].len;
    }
}

static void add(uint32_t va, int size, int32_t last, int32_t delta)
{
    if (s_ncand == s_cap) {
        size_t cap = s_cap ? s_cap * 2 : 1 << 20;
        Cand *c = (Cand *)realloc(s_cand, cap * sizeof *c);
        if (!c) return;
        s_cand = c;
        s_cap = cap;
    }
    s_cand[s_ncand].va = va;
    s_cand[s_ncand].size = (uint8_t)size;
    s_cand[s_ncand].last = last;
    s_cand[s_ncand].delta = delta;
    s_ncand++;
}

/* Second snapshot: every byte, int16 and int32 that moved by 1..8 either
 * way since the first. */
static void take_second(void)
{
    static uint8_t page[0x1000 + 4];
    size_t off = 0, r;
    s_ncand = 0;
    for (r = 0; r < N_RANGES; r++) {
        uint32_t k;
        for (k = 0; k < k_ranges[r].len; k += 0x1000u) {
            uint32_t va = k_ranges[r].va + k, i;
            const uint8_t *old = s_first + off + k;
            if (k_ranges[r].ext && !guest_vmem_committed(va, 0x1000u))
                continue;
            if (!memcmp(old, (uint8_t *)g_xbox_mem_offset + va, 0x1000u))
                continue;
            if (!read_guest(va, page, 0x1000u))
                continue;
            for (i = 0; i < 0x1000u; i++) {
                int size;
                if (old[i] == page[i])
                    continue;
                for (size = 1; size <= 4; size *= 2) {
                    int32_t a, b, d;
                    if (i + size > 0x1000u || (i & (size - 1)))
                        continue;
                    a = value(old + i, size);
                    b = value(page + i, size);
                    d = b - a;
                    if (d && d >= -8 && d <= 8)
                        add(va + i, size, b, d);
                }
            }
        }
        off += k_ranges[r].len;
    }
    free(s_first);
    s_first = NULL;
}

static void take_next(void)
{
    size_t i, kept = 0;
    for (i = 0; i < s_ncand; i++) {
        uint8_t b[4];
        int32_t v;
        if (!read_guest(s_cand[i].va, b, s_cand[i].size))
            continue;
        v = value(b, s_cand[i].size);
        if (v - s_cand[i].last == s_cand[i].delta) {
            s_cand[kept] = s_cand[i];
            s_cand[kept].last = v;
            kept++;
        }
    }
    s_ncand = kept;
}

static void snapshot(int restart)
{
    if (restart || !s_snaps) {
        s_snaps = 0;
        s_ncand = 0;
        take_first();
        fprintf(stderr, "[SCAN] snapshot 1 (new search)\n");
    } else if (s_snaps == 1) {
        take_second();
        fprintf(stderr, "[SCAN] snapshot 2: %zu values moved by 1..8\n", s_ncand);
    } else {
        size_t i;
        take_next();
        fprintf(stderr, "[SCAN] snapshot %d: %zu left\n", s_snaps + 1, s_ncand);
        for (i = 0; i < s_ncand && i < 80; i++)
            fprintf(stderr, "[SCAN]   0x%08X %s now %d (step %+d)\n", s_cand[i].va,
                    s_cand[i].size == 1 ? "byte " : s_cand[i].size == 2 ? "int16" : "int32",
                    s_cand[i].last, s_cand[i].delta);
    }
    s_snaps++;
    fflush(stderr);
    MessageBeep(MB_OK);
}

static DWORD WINAPI scan_keys(LPVOID unused)
{
    int was = 0;
    (void)unused;
    for (;;) {
        int now = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
        if (now && !was)
            snapshot((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
        was = now;
        Sleep(15);
    }
}

/* The duel cursor, as sub_00086F50(player, zone, square) stores it:
 * [0x6633D4] player, [0x6633D8] zone (0 monsters, 5 spells/traps, ...),
 * [0x6633DC] square (found with F10, 2026-11-08). Each new position held
 * 0.4 s is pictured once (fotos_cursor\p<player>_z<zone>_i<square>.bmp),
 * so the squares' places on screen can be measured for the mouse. */
void nv2a_d3d11_request_dump(const char *path);

static DWORD WINAPI cursor_photos(LPVOID unused)
{
    uint32_t last[3] = { ~0u, ~0u, ~0u }, now[3];
    DWORD since = 0;
    int shot = 1;
    (void)unused;
    CreateDirectoryA("fotos_cursor", NULL);
    for (;;) {
        Sleep(50);
        if (!read_guest(0x006633D4u, now, sizeof now))
            continue;
        if (memcmp(now, last, sizeof now)) {
            memcpy(last, now, sizeof now);
            since = GetTickCount();
            shot = 0;
            fprintf(stderr, "[CURSOR] player %u zone %u square %u\n", now[0], now[1], now[2]);
            fflush(stderr);
        } else if (!shot && GetTickCount() - since > 400) {
            char path[MAX_PATH];
            snprintf(path, sizeof path, "fotos_cursor\\p%u_z%u_i%u.bmp", now[0], now[1], now[2]);
            if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
                nv2a_d3d11_request_dump(path);
            shot = 1;
        }
    }
}

void ygo_cursor_scan_start(void)
{
    const char *e = getenv("YGO_SCAN");
    HANDLE th;
    if (!e || *e != '1')
        return;
    th = CreateThread(NULL, 0, scan_keys, NULL, 0, NULL);
    if (th)
        CloseHandle(th);
    th = CreateThread(NULL, 0, cursor_photos, NULL, 0, NULL);
    if (th)
        CloseHandle(th);
    fprintf(stderr, "[SCAN] F10: memory snapshot (Shift+F10: new search)\n");
}
