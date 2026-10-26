/* SPDX-License-Identifier: MPL-2.0 */
/* The benchmark's voice on the host: the three renderers give the same sound, it is sane (no NaN, no clipping, no
 * DC, not silent), and it writes WAVs. cc -O2 -ffp-contract=off -std=c99 -Ibase/include -Ibench bench/tests/dx_test.c
 * bench/dx.c -lm -o dx_test && ./dx_test [outdir] */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dx.h"

#define SR 44100
#define SECS 2
#define NS (SR * SECS)

static void wav(const char *path, const float *x, uint32_t n)
{
    FILE *f = fopen(path, "wb");
    uint32_t i, sr = SR, br = SR * 2, ds = n * 2, rs = 36 + ds;
    uint16_t one = 1, bits = 16, bal = 2, fmt = 1;
    uint32_t sixteen = 16;
    if (!f)
        return;
    fwrite("RIFF", 1, 4, f), fwrite(&rs, 4, 1, f), fwrite("WAVEfmt ", 1, 8, f), fwrite(&sixteen, 4, 1, f);
    fwrite(&fmt, 2, 1, f), fwrite(&one, 2, 1, f), fwrite(&sr, 4, 1, f), fwrite(&br, 4, 1, f), fwrite(&bal, 2, 1, f);
    fwrite(&bits, 2, 1, f), fwrite("data", 1, 4, f), fwrite(&ds, 4, 1, f);
    for (i = 0; i < n; i++) {
        float v = x[i] * 0.5f;
        int16_t s = (int16_t)(fmaxf(-1.0f, fminf(1.0f, v)) * 32767.0f);
        fwrite(&s, 2, 1, f);
    }
    fclose(f);
}

static dx_voice_t V[DX_MAXV];
static const uint8_t CHORD[DX_MAXV] = {48, 55, 60, 64, 67, 72, 52, 59, 62, 65, 69, 74, 45, 57, 61, 66, 71, 76, 50, 56, 63, 68, 73, 78};

int main(int argc, char **argv)
{
    static float fi[NS], ff[NS], fs[NS];
    static int32_t ii[NS];
    const char *dir = argc > 1 ? argv[1] : ".";
    uint32_t nv, blk, s, i, bad = 0;
    char p[512];
    dx_init();
    for (nv = 1; nv <= 24; nv = nv == 1 ? 4 : nv == 4 ? 8 : nv + 4) {   /* 1, 4, 8, 12, 16, 20, 24 */
        for (blk = 64; blk <= 128; blk *= 2) {
            double e = 0, ref = 0, es = 0, dc = 0, peak = 0;
            memset(ii, 0, sizeof ii), memset(ff, 0, sizeof ff), memset(fs, 0, sizeof fs);
            for (i = 0; i < nv; i++)
                dx_voice_set(&V[i], i, 6, CHORD[i]);
            for (s = 0; s + blk <= NS; s += blk)
                dx_render_i(V, nv, ii + s, blk);
            for (i = 0; i < nv; i++)
                dx_voice_set(&V[i], i, 6, CHORD[i]);
            for (s = 0; s + blk <= NS; s += blk)
                dx_render_f(V, nv, ff + s, blk);
            for (i = 0; i < nv; i++)
                dx_voice_set(&V[i], i, 6, CHORD[i]);
            for (s = 0; s + blk <= NS; s += blk)
                dx_render_s(V, nv, fs + s, blk);
            for (s = 0; s < NS; s++) {
                double a = ii[s] / 8192.0, b = ff[s], c = fs[s];
                if (isnan(b) || isnan(c))
                    bad++;
                e += (a - b) * (a - b), es += (c - b) * (c - b), ref += b * b, dc += b;
                if (fabs(b) > peak)
                    peak = fabs(b);
            }
            printf("N=%2u blk=%3u  int-vs-float %6.1f dB   fsin-vs-table %6.1f dB   rms %.3f peak %.3f dc %.4f\n", nv, blk,
                   10 * log10(e / ref + 1e-30), 10 * log10(es / ref + 1e-30), sqrt(ref / NS), peak, dc / NS);
            if (nv == 4 && blk == 128) {
                for (s = 0; s < NS; s++)
                    fi[s] = (float)(ii[s] / 8192.0);
                snprintf(p, sizeof p, "%s/dx_int_4v.wav", dir), wav(p, fi, NS);
                snprintf(p, sizeof p, "%s/dx_float_4v.wav", dir), wav(p, ff, NS);
            }
            {   /* the first 4000 samples, before the float phases drift from the 32-bit ones: the same sound */
                double e4 = 0, r4 = 0;
                for (s = 0; s < 4000; s++) {
                    double a = ii[s] / 8192.0, b = ff[s];
                    e4 += (a - b) * (a - b), r4 += b * b;
                }
                if (10 * log10(e4 / r4 + 1e-30) > -55.0) {
                    printf("  first 4000 samples differ: %.1f dB\n", 10 * log10(e4 / r4));
                    bad++;
                }
            }
            if (10 * log10(e / ref + 1e-30) > -30.0 || 10 * log10(es / ref + 1e-30) > -45.0 || sqrt(ref / NS) < 0.02 ||
                peak > (double)nv || fabs(dc / NS) > 0.05)
                bad++;
        }
    }
    puts(bad ? "DX TEST FAILED" : "DX TEST OK");
    return bad != 0;
}
