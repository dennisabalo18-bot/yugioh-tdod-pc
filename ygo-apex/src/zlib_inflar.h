/* zlib_inflar.h — zlib 1.1.3's inflate() behaviour on miniz's mz_inflate,
 * shared by zlib_nativo.c and its offline check (scratchpad prueba_png.c).
 *
 * Two differences found by that check (2026-11-21, 151 PNGs of the game,
 * libpng's own calling pattern):
 *  - mz_inflate stops when its input is all taken, though output may still
 *    be there to give: called again while it makes progress.
 *  - it takes the input greedily (its bit buffer and its 32 KB window), so
 *    the last bytes of a stream can be all taken with the last row still
 *    inside it; libpng sees avail_in 0, looks for another IDAT and stops
 *    with "Not enough image data" (28 of 151). zlib takes input only as it
 *    needs it. So when the output is full and the input would show 0, one
 *    byte is kept back: shown as not taken, skipped at the next call, and
 *    given up (reported taken) when the stream ends or no output comes
 *    without it.
 * pend: the bytes kept back (0 or 1), per stream. */
#ifndef ZLIB_INFLAR_H
#define ZLIB_INFLAR_H

static int zinflar(mz_stream *s, unsigned *pend,
                   const unsigned char **next_in, unsigned *avail_in,
                   unsigned char **next_out, unsigned *avail_out)
{
    unsigned salto = *pend <= *avail_in ? *pend : *avail_in;
    unsigned in0 = *avail_in, out0 = *avail_out;
    int r;
    s->next_in = *next_in + salto;
    s->avail_in = *avail_in - salto;
    s->next_out = *next_out;
    s->avail_out = *avail_out;
    for (;;) {
        unsigned a = s->avail_in, b = s->avail_out;
        r = mz_inflate(s, MZ_SYNC_FLUSH);
        if (r != MZ_OK || !s->avail_out || (s->avail_in == a && s->avail_out == b))
            break;
    }
    *pend = 0;
    if (r == MZ_OK && !s->avail_in && !s->avail_out && s->next_in > *next_in) {
        /* the output is full and all the input taken: one byte kept back */
        s->next_in--;
        s->avail_in = 1;
        *pend = 1;
    }
    *next_in = s->next_in;
    *avail_in = s->avail_in;
    *next_out = s->next_out;
    *avail_out = s->avail_out;
    /* progress (the kept-back byte given up counts) is never an error */
    if (r == MZ_BUF_ERROR && (*avail_in != in0 || *avail_out != out0))
        r = MZ_OK;
    return r;
}

#endif
