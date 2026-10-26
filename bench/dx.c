/* SPDX-License-Identifier: MPL-2.0 */
#include "dx.h"
#include "fastmath.h"

static int16_t dx_tbl[DX_TBL];
static float dx_ftbl[DX_TBL];

void dx_init(void)
{
    uint32_t i;
    for (i = 0; i < DX_TBL; i++) {
        float s = fm_sin_turns((float)i * (1.0f / (float)DX_TBL));
        dx_ftbl[i] = s;
        dx_tbl[i] = (int16_t)(s * 32767.0f + (s < 0.0f ? -0.5f : 0.5f));
    }
}

/* the three algorithms, per operator (op 1..6 are index 0..5, processed 5 down to 0): its modulator (6 = none) and
 * whether it is a carrier */
static const uint8_t SA[3][DX_OPS] = {{1, 6, 3, 6, 5, 6},        /* A: 2>1, 4>3, 6>5: three stacks of two */
                                      {1, 6, 3, 4, 5, 6},        /* B: 2>1 and the chain 6>5>4>3 */
                                      {6, 6, 6, 6, 6, 6}};       /* C: six carriers (additive) */
static const uint8_t CAR[3][DX_OPS] = {{1, 0, 1, 0, 1, 0}, {1, 0, 1, 0, 0, 0}, {1, 1, 1, 1, 1, 1}};
static const float RATIO[DX_OPS] = {1.0f, 2.0f, 1.0f, 3.0f, 1.0f, 7.0f};    /* frequency ratios of op 1..6 */
static const float BETA[DX_OPS] = {1.5f, 1.8f, 1.2f, 1.0f, 1.4f, 0.9f};     /* modulation index (rad) when modulated */

void dx_voice_set(dx_voice_t *v, uint32_t vi, uint32_t nops, uint32_t note)
{
    const uint8_t *sa = SA[vi % 3u], *car = CAR[vi % 3u];
    float f0 = 440.0f * fm_exp2f((float)((int32_t)note - 69) / 12.0f);
    float ncar = 0.0f;
    uint32_t i;
    if (nops < 1u)
        nops = 1u;
    if (nops > DX_OPS)
        nops = DX_OPS;
    v->nops = nops;
    for (i = 0; i < nops; i++)
        ncar += car[i] ? 1.0f : 0.0f;
    for (i = 0; i < DX_OPS; i++) {
        float inc = f0 * RATIO[i] / 44100.0f;
        float lv = car[i] ? 0.9f / ncar : 1.0f;     /* carriers share the level; modulators full */
        uint8_t a = sa[i] >= nops ? (uint8_t)DX_OPS : sa[i];
        float beta = a == DX_OPS ? 0.0f : BETA[i];
        v->ph[i] = (uint32_t)(i * 0x1234567u + vi * 0x3456789u);
        v->inc[i] = (uint32_t)(inc * 4294967296.0f);
        v->lvl[i] = (int32_t)(lv * 8388608.0f);
        v->dl[i] = -(int32_t)(lv * 8388608.0f / (44100.0f * 2.0f));      /* a two second fall */
        v->mi[i] = (int32_t)(beta * (1.0f / FM_TWO_PI) * 131072.0f);     /* turns per Q15 unit, in 2^32 */
        v->cm[i] = (i < nops && car[i]) ? -1 : 0;
        v->sa[i] = a;
        v->sb[i] = (uint8_t)DX_OPS;
        v->out[i] = 0;
        v->fph[i] = (float)(v->ph[i] >> 8) * (1.0f / 16777216.0f);
        v->finc[i] = inc;
        v->flvl[i] = lv;
        v->fdl[i] = -lv / (44100.0f * 2.0f);
        v->fmi[i] = beta * (1.0f / FM_TWO_PI);
        v->fcm[i] = v->cm[i] ? 1.0f : 0.0f;
        v->fout[i] = 0.0f;
    }
    v->out[DX_OPS] = 0;
    v->fout[DX_OPS] = 0.0f;
}

void dx_render_i(dx_voice_t *v, uint32_t nv, int32_t *out, uint32_t n)
{
    uint32_t vi, s;
    for (vi = 0; vi < nv; vi++) {
        dx_voice_t *x = &v[vi];
        uint32_t nops = x->nops;
        for (s = 0; s < n; s++) {
            int32_t acc = 0;
            uint32_t i = nops;
            while (i-- > 0u) {
                int32_t m = x->out[x->sa[i]] + x->out[x->sb[i]];
                uint32_t p = x->ph[i] + x->inc[i];
                int32_t l = x->lvl[i], o;
                x->ph[i] = p;
                x->lvl[i] = l + x->dl[i];
                o = ((int32_t)dx_tbl[(p + (uint32_t)m * (uint32_t)x->mi[i]) >> 22] * (l >> 8)) >> 15;
                x->out[i] = o;
                acc += o & x->cm[i];
            }
            out[s] += acc >> 2;
        }
    }
}

void dx_render_f(dx_voice_t *v, uint32_t nv, float *out, uint32_t n)
{
    uint32_t vi, s;
    for (vi = 0; vi < nv; vi++) {
        dx_voice_t *x = &v[vi];
        uint32_t nops = x->nops;
        for (s = 0; s < n; s++) {
            float acc = 0.0f;
            uint32_t i = nops;
            while (i-- > 0u) {
                float m = x->fout[x->sa[i]] + x->fout[x->sb[i]];
                float p = x->fph[i] + x->finc[i], l = x->flvl[i], o;
                if (p >= 1.0f)
                    p -= 1.0f;
                x->fph[i] = p;
                x->flvl[i] = l + x->fdl[i];
                o = dx_ftbl[(uint32_t)(int32_t)((p + m * x->fmi[i] + 16.0f) * (float)DX_TBL) & (DX_TBL - 1u)] * l;
                x->fout[i] = o;
                acc += o * x->fcm[i];
            }
            out[s] += acc;
        }
    }
}

void dx_render_s(dx_voice_t *v, uint32_t nv, float *out, uint32_t n)
{
    uint32_t vi, s;
    for (vi = 0; vi < nv; vi++) {
        dx_voice_t *x = &v[vi];
        uint32_t nops = x->nops;
        for (s = 0; s < n; s++) {
            float acc = 0.0f;
            uint32_t i = nops;
            while (i-- > 0u) {
                float m = x->fout[x->sa[i]] + x->fout[x->sb[i]];
                float p = x->fph[i] + x->finc[i], l = x->flvl[i], o;
                if (p >= 1.0f)
                    p -= 1.0f;
                x->fph[i] = p;
                x->flvl[i] = l + x->fdl[i];
                o = fm_sin_turns(p + m * x->fmi[i]) * l;
                x->fout[i] = o;
                acc += o * x->fcm[i];
            }
            out[s] += acc;
        }
    }
}
