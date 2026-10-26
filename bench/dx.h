/* SPDX-License-Identifier: MPL-2.0 */
/* A 6-operator DX7-style voice for the CPU benchmark: pure C, no platform, so the same code runs in the host test
 * (tests/dx_test.c) and on the FM-1 (dx.c is an -O2 unit).
 *
 * Three renderers, the same voice and the same sound (the host test checks that):
 *   dx_render_i  integer: 32-bit phase, int16 sine table (1024), Q15 x Q15 -> int32 only (no 64-bit products: the
 *                chip bug of felucca-bsp docs/CPU.md sits beside 16-bit loads)
 *   dx_render_f  float: the same, with a float table
 *   dx_render_s  float without a table: fm_sin_turns() (a polynomial)
 * What a real engine adds and this leaves out (so the numbers are a bit LOW): the envelope stages (rates, levels,
 * keyboard scaling: block-rate work), the feedback of operator 6, the output filter, the mix to stereo.
 * Algorithms: voice v uses ALG[v % 3]; 3 stacks of 2 (DX7 #5), 2 stacks (2-op beside 4-op, #4-like), 6 carriers (#32). */
#pragma once
#include <stdint.h>

#define DX_OPS 6
#define DX_MAXV 24
#define DX_TBL 1024                     /* sine table entries (10-bit phase) */

typedef struct {
    uint32_t ph[DX_OPS], inc[DX_OPS];   /* phase and its step, 2^32 = one cycle */
    int32_t lvl[DX_OPS], dl[DX_OPS];    /* level (Q23) and its slope per sample: the envelope's segment */
    int32_t mi[DX_OPS];                 /* modulation depth: phase offset per unit of Q15 modulator */
    int32_t out[DX_OPS + 1];            /* last outputs (Q15); out[DX_OPS] stays 0: "no modulator" */
    int32_t cm[DX_OPS];                 /* -1: a carrier (in the mix), 0: a modulator */
    uint8_t sa[DX_OPS], sb[DX_OPS];     /* the modulators of each operator: indexes into out[] */
    uint32_t nops;                      /* operators in use, highest first */
    float fph[DX_OPS], finc[DX_OPS], flvl[DX_OPS], fdl[DX_OPS], fmi[DX_OPS], fcm[DX_OPS], fout[DX_OPS + 1];
} dx_voice_t;

void dx_init(void);                     /* the sine tables */
/* voice number vi of the chord (vi % 3 picks the algorithm), nops operators (1..6), note */
void dx_voice_set(dx_voice_t *v, uint32_t vi, uint32_t nops, uint32_t note);
void dx_render_i(dx_voice_t *v, uint32_t nv, int32_t *out, uint32_t n);   /* adds n samples (Q15, 6 carriers max) */
void dx_render_f(dx_voice_t *v, uint32_t nv, float *out, uint32_t n);
void dx_render_s(dx_voice_t *v, uint32_t nv, float *out, uint32_t n);
