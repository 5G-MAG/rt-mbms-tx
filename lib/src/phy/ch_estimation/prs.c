/*
 * Positioning reference signals (TS 36.211 V19.3.0 clause 6.10.4), transmit side.
 *
 * This file is part of rt-mbms-tx. It is distributed under the GNU Affero General Public License, as the rest of this library.
 */

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "srsran/phy/ch_estimation/prs.h"
#include "srsran/phy/common/phy_common.h"

#define PRS_NOF_SUBFRAMES_SFN 10240u /* subframes in 1024 radio frames */
#define PRS_MAX_DL_RB 110u           /* N_RB^max,DL of the sequence definition, clause 6.10.4.1 */
#define PRS_GOLD_NC 1600u            /* Nc of clause 7.2 */

typedef struct {
  uint32_t first, last, period;
} prs_row_t;

/* Table 6.10.4.3-1: Delta_PRS = I_PRS - first index of the row (the equations are on the rendered page, not in the text export). */
static const prs_row_t prs_table[] = {{0, 159, 160},
                                      {160, 479, 320},
                                      {480, 1119, 640},
                                      {1120, 2399, 1280},
                                      {2400, 2404, 5},
                                      {2405, 2414, 10},
                                      {2415, 2434, 20},
                                      {2435, 2474, 40},
                                      {2475, 2554, 80}};

int srsran_prs_period_offset(uint32_t config_index, uint32_t* period, uint32_t* offset)
{
  for (size_t i = 0; i < sizeof(prs_table) / sizeof(prs_table[0]); i++) {
    if (config_index >= prs_table[i].first && config_index <= prs_table[i].last) {
      if (period) {
        *period = prs_table[i].period;
      }
      if (offset) {
        *offset = config_index - prs_table[i].first;
      }
      return SRSRAN_SUCCESS;
    }
  }
  return SRSRAN_ERROR_INVALID_INPUTS;
}

static bool muting_bit(const srsran_prs_cfg_t* cfg, uint32_t index)
{
  return (cfg->muting[index / 8] >> (index % 8)) & 1u;
}

bool srsran_prs_active(const srsran_prs_cfg_t* cfg, uint32_t tti)
{
  uint32_t period = 0, offset = 0;
  if (cfg == NULL || !cfg->enabled || srsran_prs_period_offset(cfg->config_index, &period, &offset) != SRSRAN_SUCCESS) {
    return false;
  }
  uint32_t sf = tti % PRS_NOF_SUBFRAMES_SFN;
  for (uint32_t j = 0; j < cfg->nof_subframes; j++) {
    uint32_t start = (sf + PRS_NOF_SUBFRAMES_SFN - j) % PRS_NOF_SUBFRAMES_SFN;
    if ((start + PRS_NOF_SUBFRAMES_SFN - offset) % period != 0) {
      continue;
    }
    if (cfg->muting_len == 0) {
      return true;
    }
    /* Occasions are counted from SFN 0 within each cycle of 10240 subframes, so when T_REP x T_PRS exceeds 10240 only the first 10240 / T_PRS bits
     * are ever reached, which is the rule of TS 37.355 prs-MutingInfo for that case; T_PRS divides 10240 and T_REP is a power of two. */
    uint32_t occasion = ((start + PRS_NOF_SUBFRAMES_SFN - offset) % PRS_NOF_SUBFRAMES_SFN) / period;
    return muting_bit(cfg, occasion % cfg->muting_len);
  }
  return false;
}

/* Frame in which an MBMS-dedicated cell has a cell acquisition subframe (clause 6.6.4.1, as enb_dl.c already uses it). */
static bool cas_frame(const srsran_cell_t* cell, uint32_t n_f)
{
  return (cell->nof_prb >= 25) ? (n_f % 4 == 0) : (n_f % 8 == 4);
}

int srsran_prs_cfg_validate(const srsran_prs_cfg_t* cfg, const srsran_cell_t* cell, char* why, uint32_t why_len)
{
#define PRS_FAIL(...)                                                                                                  \
  do {                                                                                                                 \
    if (why && why_len) {                                                                                              \
      snprintf(why, why_len, __VA_ARGS__);                                                                             \
    }                                                                                                                  \
    return SRSRAN_ERROR_INVALID_INPUTS;                                                                                \
  } while (0)

  if (cfg == NULL || cell == NULL) {
    PRS_FAIL("no configuration");
  }
  if (!cfg->enabled) {
    return SRSRAN_SUCCESS;
  }
  if (cell->frame_type != SRSRAN_FDD) {
    PRS_FAIL("PRS is implemented for FDD cells only");
  }
  if (cfg->id > 4095 && cfg->id != SRSRAN_PRS_ID_CELL) {
    PRS_FAIL("PRS ID %u is outside 0..4095", cfg->id);
  }
  uint32_t period = 0, offset = 0;
  if (srsran_prs_period_offset(cfg->config_index, &period, &offset) != SRSRAN_SUCCESS) {
    PRS_FAIL("PRS configuration index %u is reserved or out of range (Table 6.10.4.3-1)", cfg->config_index);
  }
  if (cfg->nof_subframes != 1 && cfg->nof_subframes != 2 && cfg->nof_subframes != 4 && cfg->nof_subframes != 6) {
    PRS_FAIL("N_PRS %u is not 1, 2, 4 or 6", cfg->nof_subframes);
  }
  if (cfg->nof_prb != 6 && cfg->nof_prb != 15 && cfg->nof_prb != 25 && cfg->nof_prb != 50 && cfg->nof_prb != 75 &&
      cfg->nof_prb != 100) {
    PRS_FAIL("PRS bandwidth %u PRB is not 6, 15, 25, 50, 75 or 100", cfg->nof_prb);
  }
  if (cfg->nof_prb > cell->nof_prb) {
    PRS_FAIL("PRS bandwidth %u PRB exceeds the cell bandwidth of %u PRB", cfg->nof_prb, cell->nof_prb);
  }
  if (cfg->muting_len != 0) {
    uint32_t n = cfg->muting_len;
    if (n < 2 || n > 1024 || (n & (n - 1)) != 0) {
      PRS_FAIL("the PRS muting sequence has %u bits, not 2, 4, ..., 1024", n);
    }
  }
  if (cell->mbms_dedicated) {
    srsran_prs_cfg_t unmuted = *cfg;
    unmuted.muting_len       = 0;
    for (uint32_t tti = 0; tti < PRS_NOF_SUBFRAMES_SFN; tti++) {
      if (srsran_prs_active(&unmuted, tti) && !(tti % 10 == 0 && cas_frame(cell, tti / 10))) {
        PRS_FAIL("PRS would be sent in subframe %u of frame %u; PRS is defined for 15 kHz only and the cell acquisition "
                 "subframes are the only 15 kHz subframes of an MBMS-dedicated cell (use N_PRS 1 and a configuration "
                 "whose occasions are subframe 0 of the cell acquisition frames, e.g. index 2435 at 25 PRB)",
                 tti % 10,
                 tti / 10);
      }
    }
  }
  return SRSRAN_SUCCESS;
#undef PRS_FAIL
}

void srsran_prs_sync_pbch_mask(const srsran_cell_t* cell, uint32_t n_f, uint32_t n_s, bool* mask)
{
  uint32_t nsym   = SRSRAN_CP_NSYMB(cell->cp);
  uint32_t width  = SRSRAN_NRE * cell->nof_prb;
  uint32_t centre = 6 * cell->nof_prb;
  memset(mask, 0, sizeof(bool) * width * nsym);

  bool present;
  bool sync_slot;
  if (cell->mbms_dedicated) {
    /* PSS and SSS in slot 0 and the PBCH core set in slot 1, of the frames with n_f mod 4 = 0 whatever the bandwidth (clauses 6.11.1.2, 6.6.4); the
     * frame set of the cell acquisition subframes (cas_frame) is that of the PBCH repetition, clause 6.6.4.1, and is not the same below 25 PRB */
    present   = (n_f % 4 == 0);
    sync_slot = (n_s == 0);
  } else {
    /* PSS and SSS in slots 0 and 10, the PBCH core set in slot 1, in every frame */
    present   = true;
    sync_slot = (n_s == 0 || n_s == 10);
  }
  if (!present) {
    return;
  }
  if (sync_slot) {
    for (uint32_t k = centre - 31; k < centre + 31; k++) {
      mask[(nsym - 1) * width + k] = true; /* PSS: last OFDM symbol */
      mask[(nsym - 2) * width + k] = true; /* SSS: the one before it */
    }
  }
  if (n_s == 1) {
    for (uint32_t l = 0; l < 4; l++) {
      for (uint32_t k = centre - 36; k < centre + 36; k++) {
        mask[l * width + k] = true; /* PBCH core set */
      }
    }
  }
}

/* Gold sequence of clause 7.2: c(n) = x1(n + Nc) + x2(n + Nc), x1(0) = 1, x2 initialised with c_init. */
static void gold_bits(uint32_t c_init, uint8_t* out, uint32_t len)
{
  uint32_t x1 = 1, x2 = c_init;
  for (uint32_t n = 0; n < PRS_GOLD_NC + len; n++) {
    if (n >= PRS_GOLD_NC) {
      out[n - PRS_GOLD_NC] = (uint8_t)((x1 ^ x2) & 1u);
    }
    uint32_t n1 = ((x1 >> 3) ^ x1) & 1u;
    uint32_t n2 = ((x2 >> 3) ^ (x2 >> 2) ^ (x2 >> 1) ^ x2) & 1u;
    x1          = (x1 >> 1) | (n1 << 30);
    x2          = (x2 >> 1) | (n2 << 30);
  }
}

static uint32_t prs_c_init(uint32_t n_id_prs, uint32_t n_s, uint32_t l, bool normal_cp)
{
  uint32_t group = n_id_prs / 512, rest = n_id_prs % 512;
  return (1u << 28) * group + (1u << 10) * (7 * (n_s + 1) + l + 1) * (2 * rest + 1) + 2 * rest + (normal_cp ? 1u : 0u);
}

/* Finds the elements of one slot into out[*n ...]; returns SRSRAN_SUCCESS or an error when `max` is exceeded. */
static int collect_slot(const srsran_prs_cfg_t* cfg,
                        const srsran_cell_t*    cell,
                        uint32_t                n_f,
                        uint32_t                n_s,
                        const cf_t*             sf_symbols,
                        srsran_prs_re_t*        out,
                        uint32_t*               n,
                        uint32_t                max)
{
  const bool normal_cp = SRSRAN_CP_ISNORM(cell->cp);
  const uint32_t nsym  = SRSRAN_CP_NSYMB(cell->cp);
  const uint32_t width = SRSRAN_NRE * cell->nof_prb;
  const bool four      = (cell->nof_ports == 4);

  /* symbols of the slot that carry PRS, clause 6.10.4.2 */
  uint32_t symbols[5];
  uint32_t nof_symbols = 0;
  if (normal_cp) {
    if (n_s % 2 == 0) {
      symbols[nof_symbols++] = 3, symbols[nof_symbols++] = 5, symbols[nof_symbols++] = 6;
    } else {
      if (!four) {
        symbols[nof_symbols++] = 1;
      }
      symbols[nof_symbols++] = 2, symbols[nof_symbols++] = 3, symbols[nof_symbols++] = 5, symbols[nof_symbols++] = 6;
    }
  } else {
    if (n_s % 2 == 0) {
      symbols[nof_symbols++] = 4, symbols[nof_symbols++] = 5;
    } else {
      if (!four) {
        symbols[nof_symbols++] = 1;
      }
      symbols[nof_symbols++] = 2, symbols[nof_symbols++] = 4, symbols[nof_symbols++] = 5;
    }
  }

  bool mask[SRSRAN_NRE * SRSRAN_MAX_PRB * SRSRAN_CP_NORM_NSYMB];
  srsran_prs_sync_pbch_mask(cell, n_f, n_s, mask);

  const uint32_t id      = (cfg->id == SRSRAN_PRS_ID_CELL) ? cell->id : cfg->id;
  const uint32_t v_shift = id % 6;
  const uint32_t base    = normal_cp ? 6 : 5;
  uint8_t        c[4 * PRS_MAX_DL_RB];

  for (uint32_t s = 0; s < nof_symbols; s++) {
    uint32_t l = symbols[s];
    gold_bits(prs_c_init(id, n_s, l, normal_cp), c, 4 * PRS_MAX_DL_RB);
    for (uint32_t m = 0; m < 2 * cfg->nof_prb; m++) {
      uint32_t k   = 6 * (m + cell->nof_prb - cfg->nof_prb) + ((base - l + v_shift) % 6);
      uint32_t idx = m + PRS_MAX_DL_RB - cfg->nof_prb;
      if (mask[l * width + k]) {
        continue;
      }
      uint32_t re_index = SRSRAN_RE_IDX(cell->nof_prb, l + nsym * (n_s % 2), k);
      if (sf_symbols[re_index] != 0) {
        continue; /* already holds a signal (cell-specific reference signal, repeated PBCH): not taken */
      }
      if (*n >= max) {
        return SRSRAN_ERROR;
      }
      out[*n].index = re_index;
      out[*n].value = ((1.0f - 2.0f * (float)c[2 * idx]) + _Complex_I * (1.0f - 2.0f * (float)c[2 * idx + 1])) * 0.70710678118654752f;
      (*n)++;
    }
  }
  return SRSRAN_SUCCESS;
}

int srsran_prs_put_slot(const srsran_prs_cfg_t* cfg,
                        const srsran_cell_t*    cell,
                        uint32_t                n_f,
                        uint32_t                n_s,
                        cf_t*                   sf_symbols)
{
  if (cfg == NULL || cell == NULL || sf_symbols == NULL || !cfg->enabled || cfg->nof_prb > cell->nof_prb) {
    return SRSRAN_ERROR_INVALID_INPUTS;
  }
  srsran_prs_re_t res[SRSRAN_PRS_MAX_RES / 2];
  uint32_t        n = 0;
  if (collect_slot(cfg, cell, n_f, n_s, sf_symbols, res, &n, SRSRAN_PRS_MAX_RES / 2) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  for (uint32_t i = 0; i < n; i++) {
    sf_symbols[res[i].index] = res[i].value;
  }
  return (int)n;
}

/* True if PRS may be sent in the subframe: active, and for an MBMS-dedicated cell only in an active cell acquisition subframe (the only 15 kHz ones). */
static bool subframe_carries_prs(const srsran_prs_cfg_t* cfg, const srsran_cell_t* cell, uint32_t tti)
{
  if (!srsran_prs_active(cfg, tti)) {
    return false;
  }
  uint32_t sf_idx = tti % 10;
  uint32_t n_f    = (tti / 10) % 1024;
  if (cell->mbms_dedicated) {
    if (sf_idx != 0 || !cas_frame(cell, n_f)) {
      return false;
    }
    if (cell->cas_muting && n_f % (16u * (uint32_t)cell->n_cas) >= 4u * (uint32_t)cell->k_cas) {
      return false; /* a muted cell acquisition subframe is MBSFN */
    }
  }
  return true;
}

int srsran_prs_collect_sf(const srsran_prs_cfg_t* cfg,
                          const srsran_cell_t*    cell,
                          uint32_t                tti,
                          const cf_t*             sf_symbols,
                          srsran_prs_re_t*        out,
                          uint32_t                max)
{
  if (cfg == NULL || cell == NULL || sf_symbols == NULL || out == NULL) {
    return SRSRAN_ERROR_INVALID_INPUTS;
  }
  if (!cfg->enabled || cfg->nof_prb > cell->nof_prb || !subframe_carries_prs(cfg, cell, tti)) {
    return 0;
  }
  uint32_t n = 0;
  for (uint32_t slot = 0; slot < 2; slot++) {
    if (collect_slot(cfg, cell, (tti / 10) % 1024, 2 * (tti % 10) + slot, sf_symbols, out, &n, max) != SRSRAN_SUCCESS) {
      return SRSRAN_ERROR;
    }
  }
  return (int)n;
}

int srsran_prs_put_sf(const srsran_prs_cfg_t* cfg, const srsran_cell_t* cell, uint32_t tti, cf_t* sf_symbols)
{
  srsran_prs_re_t res[SRSRAN_PRS_MAX_RES];
  int             n = srsran_prs_collect_sf(cfg, cell, tti, sf_symbols, res, SRSRAN_PRS_MAX_RES);
  for (int i = 0; i < n; i++) {
    sf_symbols[res[i].index] = res[i].value;
  }
  return n;
}
