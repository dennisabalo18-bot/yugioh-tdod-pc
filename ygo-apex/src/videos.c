/*
 * ygo-port: the game's movie player.
 *
 * sub_0002C690(path, flag), cdecl, plays an .xmv to the end on the calling
 * thread (XMVDecoder from sub_0039DAED, a decode thread at 0x0002C670) and
 * returns al = 1, or al = 0 when the file cannot be opened. Its only caller
 * (0x001128C1, the opening movie state) ignores the result and moves on.
 *
 * Under the recompiler the XMV decode never delivers a frame: the game stops
 * presenting and waits there forever (yugiX_op.xmv, first boot). Until the
 * MP4 player is in (guide section 7), the movie is skipped the way a missing
 * file is. YGO_XMV=1 runs the game's own player, for testing it.
 *
 * recomp's definition is renamed _pad (tools/rename_pads.py).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "recomp/gen/recomp_types.h"

void sub_0002C690_pad(void);

void sub_0002C690(void)
{
    static int own = -1;
    uint32_t p = MEM32(g_esp + 4);
    char path[128];
    int i;

    for (i = 0; i < 127 && MEM8(p + i); ++i)
        path[i] = (char)MEM8(p + i);
    path[i] = 0;

    if (own < 0) {
        const char *e = getenv("YGO_XMV");
        own = e && e[0] == '1';
    }
    if (own) {
        fprintf(stderr, "[VIDEO] %s: the game's XMV player\n", path);
        sub_0002C690_pad();
        return;
    }
    fprintf(stderr, "[VIDEO] %s: skipped (YGO_XMV=1 plays it)\n", path);
    fflush(stderr);
    g_eax = 0;          /* al = 0: what a file that cannot be opened returns */
    g_esp += 4;         /* cdecl ret */
}
