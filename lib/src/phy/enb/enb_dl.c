/**
 * Copyright 2013-2021 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include "srsran/phy/enb/enb_dl.h"

#include "srsran/srsran.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CURRENT_FFTSIZE srsran_symbol_sz(q->cell.nof_prb)
#define CURRENT_SFLEN_RE SRSRAN_NOF_RE(q->cell)

static float enb_dl_get_norm_factor(uint32_t nof_prb)
{
  return 0.05f / sqrtf(nof_prb);
}

int srsran_enb_dl_init(srsran_enb_dl_t* q, cf_t* out_buffer[SRSRAN_MAX_PORTS], uint32_t max_prb)
{
  int ret = SRSRAN_ERROR_INVALID_INPUTS;

  if (q != NULL) {
    ret = SRSRAN_ERROR;

    bzero(q, sizeof(srsran_enb_dl_t));

    /* sf_symbols must fit the widest resource grid across all MBSFN SCS.
     * 0.37 kHz (SL2/SL4) uses 486 sc/PRB with 1 symbol/subframe = 486*prb RE.
     * For 75 PRBs that is 36450, exceeding the standard 15 kHz EXT-CP grid.
     * 0.37 kHz is capped at 75 PRBs by srsran_symbol_sz_scs(). */
    {
      uint32_t sl4_prb  = (max_prb <= 75u) ? max_prb : 75u;
      uint32_t sl4_re   = SRSRAN_NRE_SCS_370HZ * sl4_prb;
      uint32_t std_re   = SRSRAN_SF_LEN_RE(max_prb, SRSRAN_CP_EXT);
      uint32_t sf_alloc = (sl4_re > std_re) ? sl4_re : std_re;
      for (int i = 0; i < SRSRAN_MAX_PORTS; i++) {
        q->sf_symbols[i] = srsran_vec_cf_malloc(sf_alloc);
        if (!q->sf_symbols[i]) {
          perror("malloc");
          goto clean_exit;
        }
      }
    }
    for (int i = 0; i < SRSRAN_MAX_PORTS; i++) {
      q->out_buffer[i] = out_buffer[i];
    }

    /* cas_buffer: ifft[]'s own permanently-narrow output (see enb_dl.h doc comment).
     * Sized generously at max_prb like sf_symbols[], never resized. */
    for (int i = 0; i < SRSRAN_MAX_PORTS; i++) {
      q->cas_buffer[i] = srsran_vec_cf_malloc(SRSRAN_SF_LEN_PRB(max_prb));
      if (!q->cas_buffer[i]) {
        perror("malloc");
        goto clean_exit;
      }
    }

    srsran_ofdm_cfg_t ofdm_cfg = {};
    ofdm_cfg.nof_prb           = max_prb;
    ofdm_cfg.cp                = SRSRAN_CP_EXT;
    ofdm_cfg.normalize         = false;
    ofdm_cfg.in_buffer  = q->sf_symbols[0];
    ofdm_cfg.out_buffer = out_buffer[0];
    ofdm_cfg.sf_type    = SRSRAN_SF_MBSFN;
    ofdm_cfg.subcarrier_spacing    = SRSRAN_SCS_1KHZ25;
    /* srsran_dft_replan cannot grow past init_size, so pre-size the DFT plan for
     * the largest MBSFN symbol among all supported SCS. 0.37 kHz (SL2/SL4) is the
     * widest: 82944 samples for 75 PRBs vs 18432 for 1.25 kHz. Cap the 0.37 kHz
     * PRB count at 75 (the max supported by srsran_symbol_sz_scs for that SCS). */
    {
      uint32_t sl4_prb  = (max_prb <= 75u) ? max_prb : 75u;
      int      sz_1k25  = srsran_symbol_sz_scs(max_prb, SRSRAN_SCS_1KHZ25);
      int      sz_370   = srsran_symbol_sz_scs(sl4_prb, SRSRAN_SCS_370HZ_SL4);
      ofdm_cfg.symbol_sz = (sz_370 > sz_1k25 && sz_370 > 0) ? (uint32_t)sz_370 : (uint32_t)sz_1k25;
    }
    if (srsran_ofdm_tx_init_cfg(&q->ifft_mbsfn, &ofdm_cfg)) {
      ERROR("Error initiating FFT");
      goto clean_exit;
    }

    if (srsran_pbch_init(&q->pbch)) {
      ERROR("Error creating PBCH object");
      goto clean_exit;
    }
    if (srsran_pcfich_init(&q->pcfich, 0)) {
      ERROR("Error creating PCFICH object");
      goto clean_exit;
    }
    /* [kku]
    if (srsran_phich_init(&q->phich, 0)) {
      ERROR("Error creating PHICH object");
      goto clean_exit;
    }
    */
    int mbsfn_area_id = 1;

    if (srsran_pmch_init(&q->pmch, max_prb, 1)) {
      ERROR("Error creating PMCH object");
    }
    srsran_pmch_set_area_id(&q->pmch, mbsfn_area_id);

    if (srsran_pdcch_init_enb(&q->pdcch, max_prb)) {
      ERROR("Error creating PDCCH object");
      goto clean_exit;
    }

    if (srsran_pdsch_init_enb(&q->pdsch, max_prb)) {
      ERROR("Error creating PDSCH object");
      goto clean_exit;
    }

    if (srsran_refsignal_cs_init(&q->csr_signal, max_prb)) {
      ERROR("Error initializing CSR signal (%d)", ret);
      goto clean_exit;
    }

    /* Pre-allocate for the SCS with the highest pilot density (0.37 kHz SL2: 81/RB). */
    if (srsran_refsignal_mbsfn_init(&q->mbsfnr_signal, max_prb, SRSRAN_SCS_370HZ_SL2)) {
      ERROR("Error initializing CSR signal (%d)", ret);
      goto clean_exit;
    }
    ret = SRSRAN_SUCCESS;

  } else {
    ERROR("Invalid parameters");
  }

clean_exit:
  if (ret == SRSRAN_ERROR) {
    srsran_enb_dl_free(q);
  }
  return ret;
}

void srsran_enb_dl_free(srsran_enb_dl_t* q)
{
  if (q) {
    for (int i = 0; i < SRSRAN_MAX_PORTS; i++) {
      srsran_ofdm_tx_free(&q->ifft[i]);
    }
    srsran_ofdm_tx_free(&q->ifft_mbsfn);
    srsran_regs_free(&q->regs);
    srsran_pbch_free(&q->pbch);
    srsran_pcfich_free(&q->pcfich);
    srsran_phich_free(&q->phich);
    srsran_pdcch_free(&q->pdcch);
    srsran_pdsch_free(&q->pdsch);
    srsran_pmch_free(&q->pmch);
    srsran_refsignal_free(&q->csr_signal);
    srsran_refsignal_free(&q->mbsfnr_signal);
    for (int i = 0; i < SRSRAN_MAX_PORTS; i++) {
      if (q->sf_symbols[i]) {
        free(q->sf_symbols[i]);
      }
      if (q->cas_buffer[i]) {
        free(q->cas_buffer[i]);
      }
      srsran_resampler_fft_free(&q->cas_upsampler[i]);
    }
    bzero(q, sizeof(srsran_enb_dl_t));
  }
}

int srsran_enb_dl_set_cell(srsran_enb_dl_t* q, srsran_cell_t cell)
{
  int ret = SRSRAN_ERROR_INVALID_INPUTS;

  if (q != NULL && srsran_cell_isvalid(&cell)) {
    if (q->cell.id != cell.id || q->cell.nof_prb == 0) {
      if (q->cell.nof_prb != 0) {
        srsran_regs_free(&q->regs);
      }
      q->cell = cell;
      srsran_ofdm_cfg_t ofdm_cfg = {};
      ofdm_cfg.nof_prb           = q->cell.nof_prb;
      ofdm_cfg.cp                = cell.cp;
      ofdm_cfg.normalize         = false;
      for (int i = 0; i < SRSRAN_MAX_PORTS; i++) {
        ofdm_cfg.in_buffer  = q->sf_symbols[i];
        /* ifft[i] writes into its own permanently-narrow cas_buffer[i], not the
         * shared (possibly wider) out_buffer[i] - see enb_dl.h doc comment and
         * srsran_enb_dl_gen_signal()'s upsample step below. This must be set
         * here, at this first/fresh init call: once q->max_prb > 0,
         * ofdm_init_mbsfn_() only updates cp/nof_prb/symbol_sz/subcarrier_spacing
         * on a later resize, never in_buffer/out_buffer. */
        ofdm_cfg.out_buffer = q->cas_buffer[i];
        ofdm_cfg.sf_type    = SRSRAN_SF_NORM;
        if (srsran_ofdm_tx_init_cfg(&q->ifft[i], &ofdm_cfg)) {
          ERROR("Error initiating FFT (%d)", i);
          return SRSRAN_ERROR;
        }
      }
      if (srsran_regs_init_opts(&q->regs, q->cell, q->cell.mbms_dedicated ? 0 : 1, false)) {
        ERROR("Error resizing REGs");
        return SRSRAN_ERROR;
      }
      /* CAS/PBCH/PSS/SSS ifft[] stays permanently at the carrier's own native,
       * narrow symbol_sz - exactly like a standard, non-FeMBMS LTE cell,
       * regardless of mbsfn_prb. An earlier approach widened ifft[]'s own
       * symbol_sz directly to carry a wideband pmch_bandwidth on the same
       * transform as CAS; abandoned after a full day of unresolved,
       * live-process-only corruption (see SIB13_MBSFN_TEST_RESULTS.md). CAS's
       * narrow output is instead bridged up to the wire's (possibly wider)
       * rate by cas_upsampler[] below, in srsran_enb_dl_gen_signal(). */
      for (int i = 0; i < q->cell.nof_ports; i++) {
        if (srsran_ofdm_tx_set_prb(&q->ifft[i], q->cell.cp, q->cell.nof_prb)) {
          ERROR("Error re-planning iFFT (%d)", i);
          return SRSRAN_ERROR;
        }
      }
      /* (Re)compute the CAS <-> wire sample-rate ratio and (re)init the
       * upsampler bridge. Sample-RATE ratio, not symbol-size ratio: FeMBMS's
       * reduced-SCS numerologies are defined to preserve the same overall
       * sample rate for a given PRB count (verified against
       * srsran_sampling_freq_hz_scs()'s own reduction to the plain 15kHz
       * table for every non-370Hz SCS), so srsran_sampling_freq_hz() - not
       * the _scs variant - is the correct, load-bearing formula here. Gives
       * a clean integer ratio of 2 for this project's tested PMCH widths
       * (30-40 PRB against a 25 PRB carrier), generalizing to 3/4 for wider
       * configs. 370Hz SCS is out of scope, same as the widening code this
       * replaces. */
      {
        uint32_t wide_prb  = SRSRAN_MAX(q->cell.nof_prb, q->cell.mbsfn_prb);
        int      wide_hz   = srsran_sampling_freq_hz(wide_prb);
        int      narrow_hz = srsran_sampling_freq_hz(q->cell.nof_prb);
        if (wide_hz <= 0 || narrow_hz <= 0 || (wide_hz % narrow_hz) != 0) {
          ERROR("CAS<->PMCH sample-rate ratio not integer (wide=%d narrow=%d Hz)", wide_hz, narrow_hz);
          return SRSRAN_ERROR;
        }
        uint32_t ratio = (uint32_t)(wide_hz / narrow_hz);
        for (int i = 0; i < q->cell.nof_ports; i++) {
          /* Return value intentionally unchecked: ratio==1 (baseline, no PMCH
           * widening) returns SRSRAN_ERROR_OUT_OF_BOUNDS by this function's own
           * design (resampler.c), which is expected/benign here - mirrors
           * radio.cc's own identical, unchecked usage of this same call. */
          srsran_resampler_fft_init(&q->cas_upsampler[i], SRSRAN_RESAMPLER_MODE_INTERPOLATE, ratio);
        }
      }

      /* PMCH (cell.mbsfn_prb, via pmch-Bandwidth-r17) can be wider than the
       * carrier itself for FeMBMS extended coverage -- the MBSFN grid's
       * width/stride must cover whichever is wider, mirroring the
       * SRSRAN_SCS_IS_370HZ branch in srsran_enb_dl_set_mbsfn_subcarrier_spacing()
       * below, which already does this for that numerology. */
      if (srsran_ofdm_tx_set_prb_scs(
              &q->ifft_mbsfn, SRSRAN_CP_EXT, SRSRAN_MAX(q->cell.nof_prb, q->cell.mbsfn_prb), SRSRAN_SCS_1KHZ25)) {
        ERROR("Error re-planning ifft_mbsfn");
        return SRSRAN_ERROR;
      }

      srsran_ofdm_set_non_mbsfn_region(&q->ifft_mbsfn, 1);

      if (srsran_pbch_set_cell(&q->pbch, q->cell)) {
        ERROR("Error creating PBCH object");
        return SRSRAN_ERROR;
      }
      if (srsran_pcfich_set_cell(&q->pcfich, &q->regs, q->cell)) {
        ERROR("Error creating PCFICH object");
        return SRSRAN_ERROR;
      }
      /* [kku]
      if (srsran_phich_set_cell(&q->phich, &q->regs, q->cell)) {
        ERROR("Error creating PHICH object");
        return SRSRAN_ERROR;
      }
      */

      if (srsran_pdcch_set_cell(&q->pdcch, &q->regs, q->cell)) {
        ERROR("Error creating PDCCH object");
        return SRSRAN_ERROR;
      }

      if (srsran_pdsch_set_cell(&q->pdsch, q->cell)) {
        ERROR("Error creating PDSCH object");
        return SRSRAN_ERROR;
      }

      if (srsran_pmch_set_cell(&q->pmch, q->cell)) {
        ERROR("Error creating PMCH object");
        return SRSRAN_ERROR;
      }

      if (srsran_refsignal_cs_set_cell(&q->csr_signal, q->cell)) {
        ERROR("Error initializing CSR signal (%d)", ret);
        return SRSRAN_ERROR;
      }
      int mbsfn_area_id = 1;
      srsran_scs_t mbsfn_scs = (q->subcarrier_spacing != SRSRAN_SCS_15KHZ) ? q->subcarrier_spacing : SRSRAN_SCS_1KHZ25;
      if (srsran_refsignal_mbsfn_set_cell(&q->mbsfnr_signal, q->cell, mbsfn_area_id, mbsfn_scs)) {
        ERROR("Error initializing MBSFNR signal (%d)", ret);
        return SRSRAN_ERROR;
      }
      /* Generate PSS/SSS signals */
      srsran_pss_generate(q->pss_signal, cell.id % 3);
      srsran_sss_generate(q->sss_signal0, q->sss_signal5, cell.id);

      // Calculate common DCI locations
      for (int32_t cfi = 1; cfi <= 3; cfi++) {
        q->nof_common_locations[SRSRAN_CFI_IDX(cfi)] = srsran_pdcch_common_locations(
            &q->pdcch, q->common_locations[SRSRAN_CFI_IDX(cfi)], SRSRAN_MAX_CANDIDATES_COM, cfi);
      }
    }
    ret = SRSRAN_SUCCESS;

  } else {
    ERROR("Invalid cell properties: Id=%d, Ports=%d, PRBs=%d", cell.id, cell.nof_ports, cell.nof_prb);
  }
  return ret;
}

int srsran_enb_dl_set_mbsfn_area_id(srsran_enb_dl_t* q, uint16_t mbsfn_area_id)
{
  if (q == NULL) {
    return SRSRAN_ERROR_INVALID_INPUTS;
  }
  srsran_pmch_set_area_id(&q->pmch, mbsfn_area_id);
  if (q->cell.nof_prb > 0) {
    if (srsran_refsignal_mbsfn_set_cell(&q->mbsfnr_signal, q->cell, mbsfn_area_id, q->subcarrier_spacing)) {
      ERROR("Error updating MBSFN RS signal for area_id %d\n", mbsfn_area_id);
      return SRSRAN_ERROR;
    }
  }
  return SRSRAN_SUCCESS;
}

int srsran_enb_dl_set_mbsfn_subcarrier_spacing(srsran_enb_dl_t* q, srsran_scs_t subcarrier_spacing)
{
  int ret = SRSRAN_ERROR_INVALID_INPUTS;
  if (q != NULL) {
    ret = SRSRAN_ERROR;
    /* PMCH (cell.mbsfn_prb, via pmch-Bandwidth-r17) can be wider than the carrier
     * itself for FeMBMS extended coverage, for ANY SCS - not just 0.37 kHz. Using
     * nof_prb alone here (this function's original behavior, predating the
     * wideband-PMCH-extension feature) silently re-narrowed ifft_mbsfn back down
     * from srsran_enb_dl_set_cell()'s own correct SRSRAN_MAX(nof_prb, mbsfn_prb)
     * sizing, the moment this function ran - which it always does, lazily, on
     * the first actual MBSFN subframe (see cc_worker.cc). This was a real,
     * pre-existing bug, confirmed live: ifft_mbsfn ended up permanently sized
     * for the narrow carrier (nof_prb) instead of the wider PMCH allocation,
     * for the entire remaining process lifetime once this function's very
     * first (lazy) call landed - independent of, and unaffected by, the
     * CAS/PMCH FFT-decoupling redesign elsewhere in this file (this function
     * only ever touches ifft_mbsfn). See SIB13_MBSFN_TEST_RESULTS.md.
     * For 0.37 kHz SCS the symbol size scales with 0.37 kHz PRBs (NscRB=486), not 15 kHz
     * PRBs.  srsran_symbol_sz_scs() only accepts up to 75 such PRBs; using the wide PRB
     * count directly (e.g. 100 for 20 MHz) returns an error and aborts the IFFT init, so
     * that branch keeps its own additional 75-PRB safety cap. */
    uint32_t ofdm_prb = SRSRAN_MAX(q->cell.nof_prb, q->cell.mbsfn_prb);
    if (SRSRAN_SCS_IS_370HZ(subcarrier_spacing)) {
      ofdm_prb = (ofdm_prb <= 75u) ? ofdm_prb : 75u;
    }
    if (srsran_ofdm_tx_set_prb_scs(&q->ifft_mbsfn, SRSRAN_CP_EXT, ofdm_prb, subcarrier_spacing)) {
      ERROR("Error setting MBSFN subcarrier spacing\n");
      return ret;
    }
    q->subcarrier_spacing = subcarrier_spacing;
    if (q->cell.nof_prb > 0) {
      /* Re-generate MBSFN RS pilots for the new SCS, preserving the current area_id. */
      if (srsran_refsignal_mbsfn_set_cell(&q->mbsfnr_signal, q->cell, q->mbsfnr_signal.mbsfn_area_id, subcarrier_spacing)) {
        ERROR("Error updating MBSFN RS signal for new SCS\n");
        return SRSRAN_ERROR;
      }
    }
    ret = SRSRAN_SUCCESS;
  }
  return ret;
}

#ifdef resolve
void srsran_enb_dl_apply_power_allocation(srsran_enb_dl_t* q)
{
  uint32_t nof_symbols_slot = SRSRAN_CP_NSYMB(q->cell.cp);
  uint32_t nof_re_symbol    = SRSRAN_NRE * q->cell.nof_prb;

  if (q->rho_b != 0.0f && q->rho_b != 1.0f) {
    float scaling = q->rho_b;
    for (uint32_t i = 0; i < q->cell.nof_ports; i++) {
      for (uint32_t j = 0; j < 2; j++) {
        cf_t* ptr;
        ptr = q->sf_symbols[i] + nof_re_symbol * (j * nof_symbols_slot + 0);
        srsran_vec_sc_prod_cfc(ptr, scaling, ptr, nof_re_symbol);
        if (q->cell.cp == SRSRAN_CP_NORM) {
          ptr = q->sf_symbols[i] + nof_re_symbol * (j * nof_symbols_slot + 4);
          srsran_vec_sc_prod_cfc(ptr, scaling, ptr, nof_re_symbol);
        } else {
          ptr = q->sf_symbols[i] + nof_re_symbol * (j * nof_symbols_slot + 3);
          srsran_vec_sc_prod_cfc(ptr, scaling, ptr, nof_re_symbol);
        }
        if (q->cell.nof_ports == 4) {
          ptr = q->sf_symbols[i] + nof_re_symbol * (j * nof_symbols_slot + 1);
          srsran_vec_sc_prod_cfc(ptr, scaling, ptr, nof_re_symbol);
        }
      }
    }
  }
}

void srsran_enb_dl_prepare_power_allocation(srsran_enb_dl_t* q)
{
  uint32_t nof_symbols_slot = SRSRAN_CP_NSYMB(q->cell.cp);
  uint32_t nof_re_symbol    = SRSRAN_NRE * q->cell.nof_prb;

  if (q->rho_b != 0.0f && q->rho_b != 1.0f) {
    float scaling = 1.0f / q->rho_b;
    for (uint32_t i = 0; i < q->cell.nof_ports; i++) {
      for (uint32_t j = 0; j < 2; j++) {
        cf_t* ptr;
        ptr = q->sf_symbols[i] + nof_re_symbol * (j * nof_symbols_slot + 0);
        srsran_vec_sc_prod_cfc(ptr, scaling, ptr, nof_re_symbol);
        if (q->cell.cp == SRSRAN_CP_NORM) {
          ptr = q->sf_symbols[i] + nof_re_symbol * (j * nof_symbols_slot + 4);
          srsran_vec_sc_prod_cfc(ptr, scaling, ptr, nof_re_symbol);
        } else {
          ptr = q->sf_symbols[i] + nof_re_symbol * (j * nof_symbols_slot + 3);
          srsran_vec_sc_prod_cfc(ptr, scaling, ptr, nof_re_symbol);
        }
        if (q->cell.nof_ports == 4) {
          ptr = q->sf_symbols[i] + nof_re_symbol * (j * nof_symbols_slot + 1);
          srsran_vec_sc_prod_cfc(ptr, scaling, ptr, nof_re_symbol);
        }
      }
    }
  }
}

#endif

static void clear_sf(srsran_enb_dl_t* q)
{
  for (int i = 0; i < q->cell.nof_ports; i++) {
    srsran_vec_cf_zero(q->sf_symbols[i], CURRENT_SFLEN_RE);
  }
}

static void put_sync(srsran_enb_dl_t* q)
{
  uint32_t sf_idx = q->dl_sf.tti % 10;
  uint32_t sfn    = q->dl_sf.tti / 10;

  /* TS 36.211 §6.6.4.1: CAS frame period depends on carrier width.
   * nof_prb >= 25: nf mod 4 == 0.  6 < nof_prb < 25: nf mod 8 == 4. */
  bool is_cas_frame = (sf_idx == 0) && ((q->cell.nof_prb >= 25) ? (sfn % 4 == 0) : (sfn % 8 == 4));
  if (is_cas_frame) {
    if (q->cell.cas_muting) {
      /* TS 36.211 CR 0577: active CAS when sfn % (16*NCAS) < 4*KCAS.
       * Frames outside that window are muted CAS (MBSFN): suppress PSS/SSS. */
      if (sfn % (16u * (uint32_t)q->cell.n_cas) >= 4u * (uint32_t)q->cell.k_cas) {
        return;
      }
    }
    for (int p = 0; p < q->cell.nof_ports; p++) {
      srsran_pss_put_slot(q->pss_signal, q->sf_symbols[p], q->cell.nof_prb, q->cell.cp);
      srsran_sss_put_slot(sf_idx ? q->sss_signal5 : q->sss_signal0, q->sf_symbols[p], q->cell.nof_prb, q->cell.cp);
    }
  }
}

static void put_refs(srsran_enb_dl_t* q, srsran_dl_sf_cfg_t* dl_sf)
{
  srsran_scs_t scs = dl_sf->subcarrier_spacing;
  uint32_t     tti = q->dl_sf.tti;
  /* Pilot table index per SCS.
   * 0.37 kHz: 40 ms period; slot n_s = (tti%40 - 1)/3 (slots 0..12, 3 ms each).
   * All other SCS (including 2.5 kHz): 10 ms period → index = tti % 10. */
  uint32_t sf_idx;
  if (SRSRAN_SCS_IS_370HZ(scs)) {
    uint32_t pos40 = tti % 40u;
    sf_idx = (pos40 > 0u) ? (pos40 - 1u) / 3u : 0u;
  } else {
    sf_idx = tti % 10u;
  }
  if (q->dl_sf.sf_type == SRSRAN_SF_MBSFN) {
    srsran_refsignal_mbsfn_put_sf(
        q->cell, 0, q->csr_signal.pilots[0][tti % 10u], q->mbsfnr_signal.pilots[0][sf_idx], q->sf_symbols[0], scs, tti);

    /* DIAG (PMCH_RE_DUMP): dump the pilot values TX just wrote (the locally-generated
     * reference sequence, not yet touched by IFFT/wire/FFT), for direct comparison
     * against the RX-side pilotknown dump added in chest_dl.c. If TX's and RX's own
     * locally-generated sequences disagree here, no amount of over-the-air fidelity
     * would make the RX's pilot correlation succeed -- this checks that precondition
     * directly instead of assuming it. */
    if (getenv("PMCH_RE_DUMP") && (!getenv("PMCH_RE_DUMP_TTI") || (uint32_t)atoi(getenv("PMCH_RE_DUMP_TTI")) == tti) && scs != SRSRAN_SCS_15KHZ) {
      uint32_t act_prb = q->cell.mbsfn_prb ? q->cell.mbsfn_prb : q->cell.nof_prb;
      uint32_t n        = srsran_refsignal_mbsfn_rs_per_symbol(scs) * act_prb;
      char     fn[128];
      snprintf(fn, sizeof(fn), "/tmp/pmch_tx_pilotknown_tti%u.bin", tti);
      FILE* fp = fopen(fn, "wb");
      if (fp) {
        fwrite(q->mbsfnr_signal.pilots[0][sf_idx], sizeof(cf_t), n, fp);
        fclose(fp);
      }
      fprintf(stderr,
              "[PMCH_RE_DUMP] TX pilotknown tti=%u sf_idx=%u n=%u area_id=%u\n",
              tti,
              sf_idx,
              n,
              q->mbsfnr_signal.mbsfn_area_id);
    }
  } else {
    for (int p = 0; p < q->cell.nof_ports; p++) {
      srsran_refsignal_cs_put_sf(&q->csr_signal, &q->dl_sf, (uint32_t)p, q->sf_symbols[p]);
    }
  }
}

static void put_mib(srsran_enb_dl_t* q)
{
  uint8_t bch_payload[SRSRAN_BCH_PAYLOAD_LEN];

  uint32_t sf_idx = q->dl_sf.tti % 10;
  uint32_t sfn    = q->dl_sf.tti / 10;

  /* PBCH is transmitted only in SF0 of a CAS frame (sf_idx == 0 + SFN period check).
   * The CAS-repetition requirement of TS 36.211 §6.6.4.1 is satisfied intra-SF0:
   * srsran_pbch_encode calls srsran_pbch_put_cas_rep internally, which copies the
   * PBCH symbols from slot 1 into additional symbols within the same SF0 resource
   * grid (dst_ns in {0,1} — both slots of SF0).  No cross-subframe PBCH copies are
   * produced or required by this implementation model; the sf_idx == 0 gate is
   * therefore correct and intentional. */
  bool is_cas_sf0 = (sf_idx == 0) && ((q->cell.nof_prb >= 25) ? (sfn % 4 == 0) : (sfn % 8 == 4));
  if (is_cas_sf0) {
    if (q->cell.cas_muting) {
      if (sfn % (16u * (uint32_t)q->cell.n_cas) >= 4u * (uint32_t)q->cell.k_cas) {
        return;
      }
    }
    if (q->cell.mbms_dedicated) {
      srsran_pbch_mib_mbms_pack(&q->cell, sfn, q->cell.additional_non_mbms_frames, bch_payload);
    } else {
      srsran_pbch_mib_pack(&q->cell, sfn, bch_payload);
    }
    /* BCH block index: 4 blocks cover the 4 active CAS frames per 16*NCAS period.
     * Active frames sit at sfn offsets 0,4,...,4*(KCAS-1) within that period, so
     * the index is (sfn%(16*NCAS))/4.  Without muting: (sfn%16)/4 = (sfn/4)%4. */
    uint32_t frame_idx = q->cell.cas_muting
                         ? (sfn % (16u * (uint32_t)q->cell.n_cas)) / 4u
                         : (sfn / 4u) % 4u;
    srsran_pbch_encode(&q->pbch, bch_payload, q->sf_symbols, frame_idx, sfn);
  }
}

static void put_pcfich(srsran_enb_dl_t* q)
{
  uint32_t sf_idx = q->dl_sf.tti % 10;
  uint32_t sfn    = q->dl_sf.tti / 10;
  /* PCFICH only in CAS subframes (sf=0 of a CAS SFN). CAS SFNs are sfn%4==0
   * for wide cells (nof_prb >= 25) and sfn%8==4 for narrow cells (nof_prb < 25).
   * With CAS muting, muted CAS frames are classified as MBSFN — encoding PCFICH
   * there would corrupt the MBSFN resource grid. */
  bool is_cas_sfn = (q->cell.nof_prb >= 25) ? (sfn % 4 == 0) : (sfn % 8 == 4);
  if (is_cas_sfn && sf_idx == 0 && q->dl_sf.sf_type != SRSRAN_SF_MBSFN) {
    srsran_pcfich_encode(&q->pcfich, &q->dl_sf, q->sf_symbols);
  }
}

void srsran_enb_dl_put_base(srsran_enb_dl_t* q, srsran_dl_sf_cfg_t* dl_sf)
{
  srsran_ofdm_set_non_mbsfn_region(&q->ifft_mbsfn, dl_sf->non_mbsfn_region);
  q->dl_sf = *dl_sf;
  clear_sf(q);
  put_sync(q);
  put_refs(q, dl_sf);
  put_mib(q);
  put_pcfich(q);
}

void srsran_enb_dl_put_phich(srsran_enb_dl_t* q, srsran_phich_grant_t* grant, bool ack)
{
  srsran_phich_resource_t resource;
  srsran_phich_calc(&q->phich, grant, &resource);
  srsran_phich_encode(&q->phich, &q->dl_sf, resource, ack, q->sf_symbols);
}

bool srsran_enb_dl_location_is_common_ncce(srsran_enb_dl_t* q, const srsran_dci_location_t* loc)
{
  if (SRSRAN_CFI_ISVALID(q->dl_sf.cfi)) {
    return srsran_location_find_location(
        q->common_locations[SRSRAN_CFI_IDX(q->dl_sf.cfi)], q->nof_common_locations[SRSRAN_CFI_IDX(q->dl_sf.cfi)], loc);
  } else {
    return false;
  }
}

int srsran_enb_dl_put_pdcch_dl(srsran_enb_dl_t* q, srsran_dci_cfg_t* dci_cfg, srsran_dci_dl_t* dci_dl)
{
  srsran_dci_msg_t dci_msg;
  ZERO_OBJECT(dci_msg);

  if (srsran_dci_msg_pack_pdsch(&q->cell, &q->dl_sf, dci_cfg, dci_dl, &dci_msg)) {
    ERROR("Error packing DL DCI");
  }
  if (srsran_pdcch_encode(&q->pdcch, &q->dl_sf, &dci_msg, q->sf_symbols)) {
    ERROR("Error encoding DL DCI message");
    return SRSRAN_ERROR;
  }

  return SRSRAN_SUCCESS;
}

int srsran_enb_dl_put_pdcch_ul(srsran_enb_dl_t* q, srsran_dci_cfg_t* dci_cfg, srsran_dci_ul_t* dci_ul)
{
  srsran_dci_msg_t dci_msg;
  ZERO_OBJECT(dci_msg);

  if (srsran_dci_msg_pack_pusch(&q->cell, &q->dl_sf, dci_cfg, dci_ul, &dci_msg)) {
    ERROR("Error packing UL DCI");
  }
  if (srsran_pdcch_encode(&q->pdcch, &q->dl_sf, &dci_msg, q->sf_symbols)) {
    ERROR("Error encoding UL DCI message");
    return SRSRAN_ERROR;
  }

  return SRSRAN_SUCCESS;
}

int srsran_enb_dl_put_pdsch(srsran_enb_dl_t* q, srsran_pdsch_cfg_t* pdsch, uint8_t* data[SRSRAN_MAX_CODEWORDS])
{
  return srsran_pdsch_encode(&q->pdsch, &q->dl_sf, pdsch, data, q->sf_symbols);
}

int srsran_enb_dl_put_pmch(srsran_enb_dl_t* q,
                            srsran_pmch_cfg_t* pmch_cfg,
                            uint8_t*            data,
                            uint8_t**           shared_ti_tx_buf)
{
  srsran_pmch_set_area_id(&q->pmch, pmch_cfg->area_id);
  return srsran_pmch_encode(&q->pmch, &q->dl_sf, pmch_cfg, data, q->sf_symbols, shared_ti_tx_buf);
}

void srsran_enb_dl_gen_signal(srsran_enb_dl_t* q)
{
  // TODO: PAPR control
  float norm_factor = enb_dl_get_norm_factor(q->cell.nof_prb);

  if (q->dl_sf.sf_type == SRSRAN_SF_MBSFN) {
    srsran_ofdm_tx_sf(&q->ifft_mbsfn);
    /* q->ifft_mbsfn.mbsfn_sf_len, not SRSRAN_SF_LEN_PRB(q->cell.nof_prb): the latter is
     * a fixed narrow-carrier length that (a) never reflects ifft_mbsfn's real,
     * possibly-wider output once mbsfn_prb > nof_prb, and (b) is wrong even at equal
     * width for any FeMBMS reduced SCS - see mbsfn_sf_len's doc comment in ofdm.h. */
    srsran_vec_sc_prod_cfc(q->ifft_mbsfn.cfg.out_buffer,
                           norm_factor / 2,
                           q->ifft_mbsfn.cfg.out_buffer,
                           q->ifft_mbsfn.mbsfn_sf_len);
    /* PMCH_RE_DUMP: dump the actual post-IFFT, post-normalization time-domain
     * samples for this subframe -- the exact content that srsran_enb_dl_gen_signal
     * hands off (via the zero-copy out_buffer wiring set up in srsran_enb_dl_init)
     * to whatever transmits it (ZMQ RF driver in the loopback test). Compared
     * directly against the raw captured IQ stream, with no IFFT reconstruction
     * or mirror-convention assumptions needed on the analysis side. */
    if (getenv("PMCH_RE_DUMP") &&
        (!getenv("PMCH_RE_DUMP_TTI") || (uint32_t)atoi(getenv("PMCH_RE_DUMP_TTI")) == q->dl_sf.tti)) {
      uint32_t sf_len = q->ifft_mbsfn.mbsfn_sf_len;
      char     fn[128];
      snprintf(fn, sizeof(fn), "/tmp/pmch_tx_postifft_tti%u.bin", q->dl_sf.tti);
      FILE* fpi = fopen(fn, "wb");
      if (fpi) {
        fwrite(q->ifft_mbsfn.cfg.out_buffer, sizeof(cf_t), sf_len, fpi);
        fclose(fpi);
      }
      fprintf(stderr, "[PMCH_RE_DUMP] TX postifft tti=%u sf_len=%u symbol_sz=%u\n",
              q->dl_sf.tti, sf_len, q->ifft_mbsfn.cfg.symbol_sz);
      /* DIAG: TX's own IFFT plan state, to compare against the equivalent RX-side
       * fft_mbsfn print in ue_dl.c -- if mirror/dc/forward differ between the two
       * sides for the same subcarrier_spacing, TX's copy_pre and RX's copy_post are
       * not true inverses of each other even though each is internally consistent. */
      fprintf(stderr,
              "[PMCH_RE_DUMP] DIAG ifft_mbsfn.fft_plan: size=%d init_size=%d mirror=%d dc=%d norm=%d forward=%d\n",
              q->ifft_mbsfn.fft_plan.size,
              q->ifft_mbsfn.fft_plan.init_size,
              (int)q->ifft_mbsfn.fft_plan.mirror,
              (int)q->ifft_mbsfn.fft_plan.dc,
              (int)q->ifft_mbsfn.fft_plan.norm,
              (int)q->ifft_mbsfn.fft_plan.forward);
    }
  } else {
    for (int i = 0; i < q->cell.nof_ports; i++) {
      srsran_ofdm_tx_sf(&q->ifft[i]);
      /* q->ifft[i].mbsfn_sf_len is now always the plain narrow sf_sz (ifft[i] no
       * longer widens - see srsran_enb_dl_set_cell()); normalize CAS's own narrow
       * output here, same as always. */
      srsran_vec_sc_prod_cfc(q->ifft[i].cfg.out_buffer,
                             norm_factor * 1.6,
                             q->ifft[i].cfg.out_buffer,
                             q->ifft[i].mbsfn_sf_len);
      /* Bridge the already-normalized narrow CAS signal up to the wire's rate,
       * into the shared out_buffer[i] that sf_worker/radio->tx() actually reads
       * (ifft[i].cfg.out_buffer is cas_buffer[i], NOT out_buffer[i], as of this
       * change - see enb_dl.h/srsran_enb_dl_set_cell()).
       *
       * REVERTED THE PER-OCCASION RESET (2026-07-19): resetting state before every
       * occasion forces srsran_resampler_fft_run()'s first internal iteration to run
       * against a phantom all-zero history (state_len=0) instead of a real signal
       * tail - this isn't a settling transient that decays within the occasion, it's
       * a structural, per-call discontinuity that also shifts the WHOLE occasion's
       * output by the filter's own srsran_resampler_fft_get_delay() (confirmed live:
       * this is why CAS_CE_DIAG's sync_error sat at a rock-stable ~7 samples, and why
       * fft_dump_check-style re-FFT of a captured post-decimation buffer showed a
       * clean, near-linear phase RAMP across CRS/PSS subcarriers rather than the
       * "chaotic, no smooth trend" signature the ORIGINAL (pre-redesign) architecture's
       * investigation reported - that original finding was independently found to be a
       * test-tool bug (hardcoded SRSRAN_CP_NORM against this cell's actual
       * extended_cp=true), not a real defect in the transmitted signal; the smaller,
       * genuinely-real defect underneath it is this uncompensated resampler delay).
       * NOT resetting lets cas_upsampler carry the previous CAS occasion's tail
       * forward instead of zero - not physically "correct" (that tail is ~40ms/1000
       * subframes stale), but deterministic and repeatable occasion-to-occasion
       * given cas_upsampler only ever runs on CAS occasions with an identical
       * mbsfn_sf_len every time, which is enough for the filter to settle into a
       * stable operating point rather than re-taking a cold-start hit every single
       * time. See SIB13_MBSFN_TEST_RESULTS.md. */
      srsran_resampler_fft_run(
          &q->cas_upsampler[i], q->ifft[i].cfg.out_buffer, q->out_buffer[i], q->ifft[i].mbsfn_sf_len);
    }
  }
}

bool srsran_enb_dl_gen_cqi_periodic(const srsran_cell_t*   cell,
                                    const srsran_dl_cfg_t* dl_cfg,
                                    uint32_t               tti,
                                    uint32_t               last_ri,
                                    srsran_cqi_cfg_t*      cqi_cfg)
{
  bool cqi_enabled = false;
  if (srsran_cqi_periodic_ri_send(&dl_cfg->cqi_report, tti, cell->frame_type)) {
    cqi_cfg->ri_len = srsran_ri_nof_bits(cell);
    cqi_enabled     = true;
  } else if (srsran_cqi_periodic_send(&dl_cfg->cqi_report, tti, cell->frame_type)) {
    if (dl_cfg->cqi_report.format_is_subband &&
        srsran_cqi_periodic_is_subband(&dl_cfg->cqi_report, tti, cell->nof_prb, cell->frame_type)) {
      // 36.213 table 7.2.2-1, periodic CQI supports UE-selected only
      cqi_cfg->type                 = SRSRAN_CQI_TYPE_SUBBAND_UE;
      cqi_cfg->L                    = srsran_cqi_hl_get_L(cell->nof_prb);
      cqi_cfg->subband_label_2_bits = cqi_cfg->L > 1;
    } else {
      cqi_cfg->type = SRSRAN_CQI_TYPE_WIDEBAND;
    }
    if (dl_cfg->tm == SRSRAN_TM4) {
      cqi_cfg->pmi_present     = true;
      cqi_cfg->rank_is_not_one = last_ri > 0;
    }
    cqi_enabled          = true;
    cqi_cfg->data_enable = cqi_enabled;
  }
  return cqi_enabled;
}

bool srsran_enb_dl_gen_cqi_aperiodic(const srsran_cell_t*   cell,
                                     const srsran_dl_cfg_t* dl_cfg,
                                     uint32_t               ri,
                                     srsran_cqi_cfg_t*      cqi_cfg)
{
  bool                           cqi_enabled    = false;
  const srsran_cqi_report_cfg_t* cqi_report_cfg = &dl_cfg->cqi_report;

  cqi_cfg->type = SRSRAN_CQI_TYPE_SUBBAND_HL;
  if (dl_cfg->tm == SRSRAN_TM3 || dl_cfg->tm == SRSRAN_TM4) {
    cqi_cfg->ri_len = srsran_ri_nof_bits(cell);
  }
  cqi_cfg->N                  = (cell->nof_prb > 7) ? srsran_cqi_hl_get_no_subbands(cell->nof_prb) : 0;
  cqi_cfg->four_antenna_ports = (cell->nof_ports == 4);
  cqi_cfg->pmi_present        = (cqi_report_cfg->pmi_idx != 0);
  cqi_cfg->rank_is_not_one    = ri > 0;
  cqi_cfg->data_enable        = true;

  return cqi_enabled;
}

void srsran_enb_dl_save_signal(srsran_enb_dl_t* q)
{
  char tmpstr[64];

  uint32_t tti = q->dl_sf.tti;

  snprintf(tmpstr, 64, "sf_symbols_%d", tti);
  srsran_vec_save_file(tmpstr, q->sf_symbols[0], SRSRAN_NOF_RE(q->cell) * sizeof(cf_t));

  /*
  int cb_len = q->pdsch_cfg.cb_segm[0].K1;
  for (int i=0;i<q->pdsch_cfg.cb_segm[0].C;i++) {
    snprintf(tmpstr,64,"output/rmout_%d_%d",i,tti);
    srsran_bit_unpack_vector(softbuffer->buffer_b[i], q->tmp, (3*cb_len+12));
    srsran_vec_save_file(tmpstr, q->tmp, (3*cb_len+12)*sizeof(uint8_t));
  }*/

  // printf("Saved files for tti=%d, sf=%d, cfi=%d, mcs=%d, tbs=%d, rv=%d, rnti=0x%x\n", tti, tti%10, cfi,
  //       q->dci.mcs[0].idx, q->dci.mcs[0].tbs, rv_idx, rnti);
}

void srsran_enb_dl_gen_ack(const srsran_cell_t*      cell,
                           const srsran_dl_sf_cfg_t* sf,
                           const srsran_pdsch_ack_t* ack_info,
                           srsran_uci_cfg_t*         uci_cfg)
{
  srsran_uci_data_t uci_data = {};

  // Copy UCI configuration
  uci_data.cfg = *uci_cfg;

  srsran_ue_dl_gen_ack(cell, sf, ack_info, &uci_data);

  // Copy back the result of uci configuration
  *uci_cfg = uci_data.cfg;
}

static void enb_dl_get_ack_fdd_all_spatial_bundling(const srsran_uci_value_t* uci_value,
                                                    srsran_pdsch_ack_t*       pdsch_ack,
                                                    uint32_t                  nof_tb)
{
  for (uint32_t cc_idx = 0; cc_idx < pdsch_ack->nof_cc; cc_idx++) {
    if (pdsch_ack->cc[cc_idx].m[0].present) {
      for (uint32_t tb = 0; tb < nof_tb; tb++) {
        // Check that TB was transmitted
        if (pdsch_ack->cc[cc_idx].m[0].value[tb] != 2) {
          pdsch_ack->cc[cc_idx].m[0].value[tb] = uci_value->ack.ack_value[cc_idx];
        }
      }
    }
  }
}

static void
enb_dl_get_ack_fdd_pcell_skip_drx(const srsran_uci_value_t* uci_value, srsran_pdsch_ack_t* pdsch_ack, uint32_t nof_tb)
{
  uint32_t ack_idx = 0;
  if (pdsch_ack->cc[0].m[0].present) {
    for (uint32_t tb = 0; tb < nof_tb; tb++) {
      // Check that TB was transmitted
      if (pdsch_ack->cc[0].m[0].value[tb] != 2) {
        if (uci_value->ack.valid) {
          pdsch_ack->cc[0].m[0].value[tb] = uci_value->ack.ack_value[ack_idx++];
        } else {
          pdsch_ack->cc[0].m[0].value[tb] = 0;
        }
      }
    }
  }
}

static void
enb_dl_get_ack_fdd_all_keep_drx(const srsran_uci_value_t* uci_value, srsran_pdsch_ack_t* pdsch_ack, uint32_t nof_tb)
{
  for (uint32_t cc_idx = 0; cc_idx < pdsch_ack->nof_cc; cc_idx++) {
    if (pdsch_ack->cc[cc_idx].m[0].present) {
      for (uint32_t tb = 0; tb < nof_tb; tb++) {
        // Check that TB was transmitted
        if (pdsch_ack->cc[cc_idx].m[0].value[tb] != 2) {
          if (uci_value->ack.valid) {
            pdsch_ack->cc[cc_idx].m[0].value[tb] = uci_value->ack.ack_value[cc_idx * nof_tb + tb];
          } else {
            pdsch_ack->cc[cc_idx].m[0].value[tb] = 0;
          }
        }
      }
    }
  }
}

static void
get_ack_fdd(const srsran_uci_cfg_t* uci_cfg, const srsran_uci_value_t* uci_value, srsran_pdsch_ack_t* pdsch_ack)
{
  // Number of transport blocks for the current Transmission Mode
  uint32_t nof_tb = 1;
  if (pdsch_ack->transmission_mode > SRSRAN_TM2) {
    nof_tb = SRSRAN_MAX_CODEWORDS;
  }

  // Count number of transmissions
  uint32_t tb_count     = 0; // All transmissions
  uint32_t tb_count_cc0 = 0; // Transmissions on PCell
  for (uint32_t cc_idx = 0; cc_idx < pdsch_ack->nof_cc; cc_idx++) {
    for (uint32_t tb = 0; tb < nof_tb; tb++) {
      if (pdsch_ack->cc[cc_idx].m[0].present && pdsch_ack->cc[cc_idx].m[0].value[tb] != 2) {
        tb_count++;
      }

      // Save primary cell number of TB
      if (cc_idx == 0) {
        tb_count_cc0 = tb_count;
      }
    }
  }

  // Does CSI report need to be transmitted?
  bool csi_report = uci_cfg->cqi.data_enable || uci_cfg->cqi.ri_len;

  switch (pdsch_ack->ack_nack_feedback_mode) {
    case SRSRAN_PUCCH_ACK_NACK_FEEDBACK_MODE_NORMAL:
      // Get ACK from PCell only, skipping DRX
      enb_dl_get_ack_fdd_pcell_skip_drx(uci_value, pdsch_ack, nof_tb);
      break;
    case SRSRAN_PUCCH_ACK_NACK_FEEDBACK_MODE_CS:
      if (pdsch_ack->nof_cc == 1) {
        enb_dl_get_ack_fdd_pcell_skip_drx(uci_value, pdsch_ack, nof_tb);
      } else if (pdsch_ack->is_pusch_available) {
        enb_dl_get_ack_fdd_all_keep_drx(uci_value, pdsch_ack, nof_tb);
      } else if (uci_value->scheduling_request) {
        // For FDD with PUCCH format 1b with channel selection, when both HARQ-ACK and SR are transmitted in the same
        // sub-frame a UE shall transmit the HARQ-ACK on its assigned HARQ-ACK PUCCH resource with channel selection as
        // defined in subclause 10.1.2.2.1 for a negative SR transmission and transmit one HARQ-ACK bit per serving cell
        // on its assigned SR PUCCH resource for a positive SR transmission according to the following:
        // − if only one transport block or a PDCCH indicating downlink SPS release is detected on a serving cell, the
        //   HARQ-ACK bit for the serving cell is the HARQ-ACK bit corresponding to the transport block or the PDCCH
        //   indicating downlink SPS release;
        // − if two transport blocks are received on a serving cell, the HARQ-ACK bit for the serving cell is generated
        //   by spatially bundling the HARQ-ACK bits corresponding to the transport blocks;
        // − if neither PDSCH transmission for which HARQ-ACK response shall be provided nor PDCCH indicating
        //   downlink SPS release is detected for a serving cell, the HARQ-ACK bit for the serving cell is set to NACK;
        enb_dl_get_ack_fdd_all_spatial_bundling(uci_value, pdsch_ack, nof_tb);
      } else if (csi_report) {
        enb_dl_get_ack_fdd_pcell_skip_drx(uci_value, pdsch_ack, nof_tb);
      } else {
        enb_dl_get_ack_fdd_all_keep_drx(uci_value, pdsch_ack, nof_tb);
      }
      break;
    case SRSRAN_PUCCH_ACK_NACK_FEEDBACK_MODE_PUCCH3:
      if (tb_count == tb_count_cc0) {
        enb_dl_get_ack_fdd_pcell_skip_drx(uci_value, pdsch_ack, nof_tb);
      } else {
        enb_dl_get_ack_fdd_all_keep_drx(uci_value, pdsch_ack, nof_tb);
      }
      break;
    case SRSRAN_PUCCH_ACK_NACK_FEEDBACK_MODE_ERROR:
    default:; // Do nothing
      break;
  }
}

void srsran_enb_dl_get_ack(const srsran_cell_t*      cell,
                           const srsran_uci_cfg_t*   uci_cfg,
                           const srsran_uci_value_t* uci_value,
                           srsran_pdsch_ack_t*       pdsch_ack)
{
  if (cell->frame_type == SRSRAN_FDD) {
    get_ack_fdd(uci_cfg, uci_value, pdsch_ack);
  } else {
    ERROR("Not implemented for TDD");
  }
}

float srsran_enb_dl_get_maximum_signal_power_dBfs(uint32_t nof_prb)
{
  return srsran_convert_amplitude_to_dB(enb_dl_get_norm_factor(nof_prb)) +
         srsran_convert_power_to_dB((float)nof_prb * SRSRAN_NRE) + 3.0f;
}
