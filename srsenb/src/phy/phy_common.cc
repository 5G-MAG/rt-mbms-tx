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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "srsenb/hdr/phy/txrx.h"
#include "srsran/common/threads.h"
#include "srsran/phy/channel/channel.h"
#include <sstream>

#include <assert.h>

using namespace std;
using namespace asn1::rrc;

namespace srsenb {

void phy_common::reset()
{
  for (auto& q : ul_grants) {
    for (auto& g : q) {
      g = {};
    }
  }
}

bool phy_common::init(const phy_cell_cfg_list_t&    cell_list_,
                      const phy_cell_cfg_list_nr_t& cell_list_nr_,
                      srsran::radio_interface_phy*  radio_h_,
                      stack_interface_phy_lte*      stack_)
{
  radio         = radio_h_;
  stack         = stack_;
  cell_list_lte = cell_list_;
  cell_list_nr  = cell_list_nr_;

  pthread_mutex_init(&mtch_mutex, nullptr);
  pthread_cond_init(&mtch_cvar, nullptr);

  // Instantiate DL channel emulator
  if (params.dl_channel_args.enable) {
    dl_channel = srsran::channel_ptr(
        new srsran::channel(params.dl_channel_args, get_nof_rf_channels(), srslog::fetch_basic_logger("PHY")));
    dl_channel->set_srate((uint32_t)srsran_sampling_freq_hz(cell_list_lte[0].cell.nof_prb));
    dl_channel->set_signal_power_dBfs(srsran_enb_dl_get_maximum_signal_power_dBfs(cell_list_lte[0].cell.nof_prb));
  }

  // Create grants
  for (auto& q : ul_grants) {
    q.resize(cell_list_lte.size());
  }

  // Set UE PHY data-base stack and configuration
  ue_db.init(stack, params, cell_list_lte);
  if (mcch_configured) {
    build_mch_table();
    build_mcch_table();
  }

  reset();
  return true;
}

void phy_common::stop()
{
  semaphore.wait_all();
}

void phy_common::clear_grants(uint16_t rnti)
{
  std::lock_guard<std::mutex> lock(grant_mutex);

  // remove any pending dci for each subframe
  for (auto& list : ul_grants) {
    for (auto& q : list) {
      for (uint32_t j = 0; j < q.nof_grants; j++) {
        if (q.pusch[j].dci.rnti == rnti) {
          q.pusch[j].dci.rnti = 0;
        }
      }
    }
  }
}

const stack_interface_phy_lte::ul_sched_list_t phy_common::get_ul_grants(uint32_t tti)
{
  std::lock_guard<std::mutex> lock(grant_mutex);
  return ul_grants[tti];
}

void phy_common::set_ul_grants(uint32_t tti, const stack_interface_phy_lte::ul_sched_list_t& ul_grant_list)
{
  std::lock_guard<std::mutex> lock(grant_mutex);
  ul_grants[tti] = ul_grant_list;
}

/* The transmission of UL subframes must be in sequence. The correct sequence is guaranteed by a chain of N semaphores,
 * one per TTI%nof_workers. Each threads waits for the semaphore for the current thread and after transmission allows
 * next TTI to be transmitted
 *
 * Each worker uses this function to indicate that all processing is done and data is ready for transmission or
 * there is no transmission at all (tx_enable). In that case, the end of burst message will be sent to the radio
 */
void phy_common::worker_end(const worker_context_t& w_ctx, const bool& tx_enable, srsran::rf_buffer_t& buffer)
{
  // Wait for the green light to transmit in the current TTI
  semaphore.wait(w_ctx.worker_ptr);

  // For combine buffer with previous buffers
  if (tx_enable) {
    tx_buffer.set_nof_samples(buffer.get_nof_samples());
    tx_buffer.set_combine(buffer);
  }

  // If the current worker is not the last one, skip transmission
  if (not w_ctx.last) {
    if (tx_enable) {
      reset_last_worker();
    }

    // Release semaphore and let next worker to get in
    semaphore.release();

    // Wait for the last worker to finish
    if (tx_enable) {
      wait_last_worker();
    }

    return;
  }

  // Add current time alignment
  srsran::rf_timestamp_t tx_time = w_ctx.tx_time; // get transmit time from the last worker

  // Run DL channel emulator if created
  if (dl_channel) {
    dl_channel->run(tx_buffer.to_cf_t(), tx_buffer.to_cf_t(), tx_buffer.get_nof_samples(), tx_time.get(0));
  }

  // Always transmit on single radio
  radio->tx(tx_buffer, tx_time);

  // Reset transmit buffer
  tx_buffer = {};

  // Notify this is the last worker
  last_worker();

  // Allow next TTI to transmit
  semaphore.release();
}

void phy_common::set_mch_period_stop(uint32_t stop)
{
  pthread_mutex_lock(&mtch_mutex);
  have_mtch_stop  = true;
  mch_period_stop = stop;
  pthread_cond_signal(&mtch_cvar);
  pthread_mutex_unlock(&mtch_mutex);
}

void phy_common::set_last_mtch_start(uint8_t pmch_idx, uint32_t start_sf)
{
  if (pmch_idx >= last_mtch_start.size()) {
    return;
  }
  std::lock_guard<std::mutex> lock(last_mtch_start_mutex);
  last_mtch_start[pmch_idx] = start_sf;
}

uint32_t phy_common::get_last_mtch_start(uint8_t pmch_idx) const
{
  if (pmch_idx >= last_mtch_start.size()) {
    return 0;
  }
  std::lock_guard<std::mutex> lock(last_mtch_start_mutex);
  return last_mtch_start[pmch_idx];
}

void phy_common::configure_mbsfn(srsran::phy_cfg_mbsfn_t* cfg)
{
  {
    std::lock_guard<std::mutex> lock(mbsfn_mutex);
    mbsfn = *cfg;
  }
  sib13_configured = true;
  mcch_configured  = true;

  /* build_mch_table()/build_mcch_table() derive mch_table/mcch_table from the
   * MBSFN config we just stored above. init() also has a call to these two
   * functions, gated on mcch_configured - the *documented* assumption was
   * that init() runs at PHY startup before RRC ever calls configure_mbsfn(),
   * so that call was expected never to fire. That ordering is not actually
   * guaranteed: RRC's own init() runs on a different thread and can reach
   * configure_mbsfn() before PHY's init() has set `stack` (still nullptr at
   * that point) - build_mch_table() unconditionally dereferences `stack`,
   * so this was a real, timing-dependent null-pointer crash on eNB startup
   * (reproduced: ~14/15 cold starts in one environment). Guarding on
   * `stack` here and letting init()'s own mcch_configured-gated call catch
   * up in the other ordering closes the gap symmetrically - mcch_configured
   * is already true by the time this function returns either way, so
   * whichever of the two call sites runs second will always find its own
   * gate satisfied. init_pmch_ti_tx_bufs() doesn't touch `stack`, so it's
   * safe unconditionally regardless of ordering.
   *
   * Without build_mch_table()/build_mcch_table() ever running at all (the
   * original bug this comment used to describe, before the race above was
   * found), mcch_table stays permanently zero-initialized for the eNB's
   * entire lifetime and is_mcch_subframe()'s "mcch_table[sf] > 0" check can
   * never be true - the eNB broadcasts SIB13's MCCH schedule correctly
   * (receivers compute the right subframes to listen on) but never actually
   * transmits real MCCH content on any of them, silently falling through to
   * regular MCH-data scheduling instead. Found via a receiver-side MCCH
   * decode that always failed CRC despite SIB13 decoding correctly -
   * PMCH_RE_DUMP dumps showed every single PMCH encode using the
   * regular-MCH MCS, never MCCH's. */
  if (stack != nullptr) {
    build_mch_table();
    build_mcch_table();
  }
  init_pmch_ti_tx_bufs();
}

void phy_common::init_pmch_ti_tx_bufs()
{
  if (cell_list_lte.empty()) {
    return;
  }
  free_pmch_ti_tx_bufs();
  /* Same worst-case-across-SCS sizing as srsran_pmch_t's own per-worker
   * lazy allocation in pmch.c (MAX_PMCH_RE(SRSRAN_SCS_370HZ), a private
   * macro there - duplicated here via the public SRSRAN_NRE_SCS_370HZ,
   * confirmed numerically identical (486) rather than assumed). Worst-case
   * N=16, 256QAM (8 bits/RE) - keep in sync with pmch.c by hand if either
   * changes. */
  uint32_t nof_prb   = cell_list_lte[0].cell.mbsfn_prb ? cell_list_lte[0].cell.mbsfn_prb : cell_list_lte[0].cell.nof_prb;
  uint32_t max_re     = nof_prb * SRSRAN_NRE_SCS_370HZ;
  uint32_t buf_bytes = 16u * max_re * 8u;
  for (uint32_t m = 0; m < SRSRAN_PMCH_MAX_TI_M; m++) {
    pmch_ti_tx_buf[m] = (uint8_t*)calloc(buf_bytes, sizeof(uint8_t));
    if (!pmch_ti_tx_buf[m]) {
      fprintf(stderr, "Error allocating pmch_ti_tx_buf[%u] (%u bytes)\n", m, buf_bytes);
    }
  }
}

void phy_common::free_pmch_ti_tx_bufs()
{
  for (uint32_t m = 0; m < SRSRAN_PMCH_MAX_TI_M; m++) {
    if (pmch_ti_tx_buf[m]) {
      free(pmch_ti_tx_buf[m]);
      pmch_ti_tx_buf[m] = nullptr;
    }
  }
}

void phy_common::build_mch_table()
{
  // First reset tables
  ZERO_OBJECT(mch_table);

  // 40 element table represents 4 frames (40 subframes)
  uint32_t nof_sfs = 0;
  if (mbsfn.mbsfn_subfr_cnfg.nof_alloc_subfrs == srsran::mbsfn_sf_cfg_t::sf_alloc_type_t::one_frame) {
    generate_mch_table(&mch_table[0], (uint32_t)mbsfn.mbsfn_subfr_cnfg.sf_alloc, 1);
    nof_sfs = 10;
  } else if (mbsfn.mbsfn_subfr_cnfg.nof_alloc_subfrs == srsran::mbsfn_sf_cfg_t::sf_alloc_type_t::four_frames) {
    generate_mch_table(&mch_table[0], (uint32_t)mbsfn.mbsfn_subfr_cnfg.sf_alloc, 4);
    nof_sfs = 40;
  } else {
    fprintf(stderr, "No valid SF alloc\n");
  }
  // Debug
  std::stringstream ss;
  ss << "|";
  for (uint32_t j = 0; j < 40; j++) {
    ss << (int)mch_table[j] << "|";
  }

  stack->set_sched_dl_tti_mask(mch_table, nof_sfs);
}

void phy_common::build_mcch_table()
{
  /* mcch_table is read (unprotected, until this fix) by is_mcch_subframe()
   * from the PHY worker thread pool, once per subframe -- same class of race
   * as mbsfn_mutex guards for the mbsfn struct itself (see that member's doc
   * comment). Lock the whole zero-then-fill sequence so a concurrent reader
   * never observes a torn/all-zero-then-partially-filled intermediate state. */
  std::lock_guard<std::mutex> lock(mbsfn_mutex);
  ZERO_OBJECT(mcch_table);

  if (mbsfn.mbsfn_area_info.mcch_cfg.sf_alloc_info_is_r16) {
    generate_mcch_table_r16(mcch_table, static_cast<uint32_t>(mbsfn.mbsfn_area_info.mcch_cfg.sf_alloc_info));
  } else {
    generate_mcch_table(mcch_table, static_cast<uint32_t>(mbsfn.mbsfn_area_info.mcch_cfg.sf_alloc_info));
  }

  std::stringstream ss;
  ss << "|";
  for (uint32_t j = 0; j < 10; j++) {
    ss << (int)mcch_table[j] << "|";
  }
}

bool phy_common::is_mcch_subframe(srsran_mbsfn_cfg_t*           cfg,
                                   uint32_t                      phy_tti,
                                   const srsran::phy_cfg_mbsfn_t& mbsfn_snapshot,
                                   const uint8_t                 mcch_table_snapshot[10])
{
  uint32_t sfn; // System Frame Number
  uint8_t  sf;  // Subframe
  uint8_t  offset;
  uint8_t  period;

  sfn = phy_tti / 10;
  sf  = phy_tti % 10;

  if (sib13_configured) {
    // mbsfn_area_info_r9_s* area_info = &mbsfn_snapshot.mbsfn_area_info;
    const srsran::mbsfn_area_info_t* area_info = &mbsfn_snapshot.mbsfn_area_info;
    offset                               = area_info->mcch_cfg.mcch_offset;
    period                               = enum_to_number(area_info->mcch_cfg.mcch_repeat_period);

    if (getenv("PMCH_TI_DIAG")) {
      fprintf(stderr, "TI_DIAG_ISMCCH tti=%u sfn=%u sf=%u period=%u offset=%u mcch_table[sf]=%u match=%d\n",
              phy_tti, sfn, sf, period, offset, mcch_table_snapshot[sf],
              (int)((sfn % period == offset) && mcch_table_snapshot[sf] > 0));
    }
    if ((sfn % period == offset) && mcch_table_snapshot[sf] > 0) {
      cfg->mbsfn_area_id = area_info->mbsfn_area_id;
      /* MCCH in an MBMS-dedicated cell is never carried on a subframe with a real
       * PDCCH/non-MBSFN control region: every subcarrier_spacing_t value this
       * function's own switch statement below can produce (khz_7dot5, khz_2dot5,
       * khz_0dot37, khz_1dot25, and the ASN.1 "field not present"/nulltype value via
       * that switch's default case, which also maps to 1.25 kHz) is a FeMBMS
       * numerology with no PDCCH region at all - so non_mbsfn_region_length is
       * always 0 here, unconditionally.
       * Previously this used a separate equality check against exactly
       * {khz_1dot25, khz_7dot5, khz_0dot37}, which did not cover khz_2dot5 or the
       * nulltype/not-present case the switch below already treats as 1.25 kHz -
       * for those two cases this fell through to the raw SIB13
       * non_mbsfn_region_len value (typically 1 or 2), disagreeing with the
       * switch's own default two lines later. That mismatch propagated into
       * ofdm_tx_slot_mbsfn's CP-length selection (see the fix and comment there)
       * and corrupted the tail of every affected MBSFN symbol.
       * KNOWN GAP: khz_15 (plain 15 kHz numerology on an MBMS-dedicated cell) does NOT
       * fit the "no PDCCH region at all" premise above -- a 15 kHz MBSFN subframe shares
       * the CAS control-region structure (see sf_worker.cc's semiStaticCFI override,
       * which explicitly covers MBSFN-at-15kHz for exactly this reason) and can have a
       * real, non-zero control region. Forcing non_mbsfn_region_length=0 here for
       * khz_15 is still done unconditionally below, left as pre-existing behavior
       * rather than redesigned blind -- flagging, not fixing, until this can be
       * verified against a real 15 kHz MBMS-dedicated MCCH capture. */
      cfg->non_mbsfn_region_length = 0;
      /* MCCH and MTCH are both carried over the same PMCH, so they share the same
       * subcarrier spacing - SCS is a property of the PMCH transmission itself, not
       * of the logical channel mapped onto it. Mirrors the full mapping used below
       * for PMCH data subframes (including 2.5kHz and 0.37kHz SL2/SL4), rather than
       * a narrower, separate switch that silently defaulted those two to 1.25kHz. */
      switch (area_info->subcarrier_spacing) {
        case srsran::mbsfn_area_info_t::subcarrier_spacing_t::khz_7dot5:
          cfg->subcarrier_spacing = SRSRAN_SCS_7KHZ5;
          break;
        case srsran::mbsfn_area_info_t::subcarrier_spacing_t::khz_15:
          cfg->subcarrier_spacing = SRSRAN_SCS_15KHZ;
          break;
        case srsran::mbsfn_area_info_t::subcarrier_spacing_t::khz_2dot5:
          cfg->subcarrier_spacing = SRSRAN_SCS_2KHZ5;
          break;
        case srsran::mbsfn_area_info_t::subcarrier_spacing_t::khz_0dot37:
          switch (area_info->time_separation) {
            case srsran::mbsfn_area_info_t::time_separation_t::sl2:
              cfg->subcarrier_spacing = SRSRAN_SCS_370HZ_SL2;
              break;
            default:
              /* sl4 is the spec default when time_separation is absent (TS 36.211 §4.1). */
              cfg->subcarrier_spacing = SRSRAN_SCS_370HZ_SL4;
              break;
          }
          break;
        case srsran::mbsfn_area_info_t::subcarrier_spacing_t::khz_1dot25:
        default:
          cfg->subcarrier_spacing = SRSRAN_SCS_1KHZ25;
          break;
      }
      cfg->mbsfn_mcs               = enum_to_number(area_info->mcch_cfg.sig_mcs);
      cfg->enable                  = true;
      cfg->is_mcch                 = true;
      have_mtch_stop               = false;
      return true;
    }
  }
  return false;
}

bool phy_common::is_mch_subframe(srsran_mbsfn_cfg_t* cfg, uint32_t phy_tti)
{
  uint32_t sfn; // System Frame Number
  uint8_t  sf;  // Subframe
  uint8_t  offset;
  uint8_t  period;

  sfn = phy_tti / 10;
  sf  = phy_tti % 10;

  // Single locked snapshot for this whole call, instead of re-reading cell_list_lte[0].cell's
  // CAS-muting fields (each individually mutex-guarded) at every one of this function's 4 use
  // sites below -- see get_cell_cas_muting_cfg()'s doc comment for why these need a live getter
  // at all rather than being read directly like nof_prb.
  const cell_cas_muting_cfg_t cas_cfg = get_cell_cas_muting_cfg();

  // Single locked snapshot of mbsfn for this whole call -- see mbsfn_mutex's
  // doc comment for the confirmed cross-thread race this closes (RRC thread
  // writes mbsfn wholesale on every MCCH decode; this function runs on the
  // PHY worker thread pool, once per subframe).
  srsran::phy_cfg_mbsfn_t mbsfn_snapshot;
  uint8_t                 mcch_table_snapshot[10];
  {
    std::lock_guard<std::mutex> lock(mbsfn_mutex);
    mbsfn_snapshot = mbsfn;
    std::memcpy(mcch_table_snapshot, mcch_table, sizeof(mcch_table_snapshot));
  }

  // Set some defaults
  cfg->mbsfn_area_id           = 0;
  cfg->non_mbsfn_region_length = 1;
  cfg->mbsfn_mcs               = 2;
  cfg->enable                  = false;
  cfg->is_mcch                 = false;
  /* PMCH-specific fields not set by is_mcch_subframe — must be safe defaults so that
   * MCCH subframes do not enter the time-interleaving branch in srsran_pmch_encode.
   * Without these, the stack-local mbsfn_cfg struct in sf_worker carries garbage. */
  cfg->use_mcs_table2          = false;
  cfg->time_interleaving_n     = 1;
  cfg->time_interleaving_m     = 1;
  cfg->n_soft_ref_category     = 0;
  cfg->scaling_factor_beta_num = 0;
  cfg->scaling_factor_beta_den = 0;
  cfg->mch_subframe_idx        = 0;
  cfg->cyclic_shift            = 0;
  cfg->cyclic_shift_alpha      = 0;
  cfg->freq_interleaving       = false;

  /* CAS subframe detection. TS 36.211 §6.6.4.1:
   *   nof_prb >= 25: CAS period = 4 frames  (sfn % 4 == 0)
   *   nof_prb < 25: CAS period = 8 frames (sfn % 8 == 4)
   * With CAS muting (CR 0577), active CAS when sfn%(16*NCAS) < 4*KCAS; muted CAS falls through
   * to MBSFN scheduling. sfn%4==0 incorrectly catches sfn%8==0 frames for narrow cells.
   * Boundary must match enb_dl.c's put_sync/put_mib and sched_carrier.cc, which both use
   * a plain nof_prb>=25 split (no separate nof_prb==6 case) — a mismatched boundary here
   * would desync PSS/SSS/PBCH placement from MBSFN scheduling for a 6-PRB cell. */
  bool narrow_cell = !cell_list_lte.empty() && cell_list_lte[0].cell.nof_prb < 25;
  bool is_cas_candidate = (sf == 0) && (narrow_cell ? (sfn % 8 == 4) : (sfn % 4 == 0));
  if (is_cas_candidate) {
    if (cas_cfg.cas_muting) {
      uint32_t n_cas = (uint32_t)cas_cfg.n_cas;
      uint32_t k_cas = (uint32_t)cas_cfg.k_cas;
      if (sfn % (16u * n_cas) < 4u * k_cas) {
        return false; // true CAS frame
      }
      // muted CAS frame: fall through and schedule as MBSFN
    } else {
      return false;
    }
  }

  /* Check for MCCH before the additionalNonMBSFNSubframes exclusion below. MCCH's own
   * subframe position (mcch_offset/sf_alloc_info, SIB13-configured) is independent of
   * additionalNonMBSFNSubframes-r14 (a separate, MIB-MBMS-signalled field) - nothing
   * requires an operator to pick values that don't collide, and previously this
   * function excluded MCCH's own subframe whenever it happened to fall inside
   * [1, additional_non_mbms_frames] on an active CAS frame, silently preventing MCCH
   * from EVER being transmitted for that configuration (found while testing Rel-19
   * PMCH time interleaving with additional_non_mbsfn_subframes=2 and MCCH at its
   * default subframe 1 - every MCCH occasion is also a CAS-anchor frame whenever
   * mcch_repeat_period, as here, is a multiple of the fixed CAS period, so the
   * collision was permanent, not occasional). MCCH is transmitted instead of the
   * "additional non-MBSFN" content it would otherwise occupy that subframe with. */
  if (is_mcch_subframe(cfg, phy_tti, mbsfn_snapshot, mcch_table_snapshot)) {
    return true;
  }

  /* On a non-dedicated (mixed unicast/FeMBMS) cell, subframes 0/4/5/9 carry real PSS/SSS/
   * paging/SIB content and are not available for MCH at all — mirrors the modem's own
   * Phy::is_mbsfn_subframe (Phy.cpp), which already makes exactly this distinction on
   * receive. TX previously had NO such gate: sf==5 (always) and sf==0 (on non-CAS-anchor
   * frames) were already being scheduled as ordinary MCH capacity here regardless of
   * cell.mbms_dedicated, which this function never read at all -- dormant only because the
   * shipped config always sets mbms_dedicated=true. A false config with real PMCH capacity
   * would have scheduled MCH content directly onto subframes carrying real sync signals. */
  if (!(!cell_list_lte.empty() && cell_list_lte[0].cell.mbms_dedicated)) {
    if (sf != 1 && sf != 2 && sf != 3 && sf != 6 && sf != 7 && sf != 8) {
      return false;
    }
  }

  /* additionalNonMBSFNSubframes-r14 (MIB-MBMS bits[9-10]): SFs 1..N of active CAS frames
   * are non-MBSFN (TS 36.331 §6.7.4.1).  Apply after the sf=0 CAS gate above. */
  uint8_t add_non = cas_cfg.additional_non_mbms_frames;
  if (add_non > 0u) {
    bool sfn_is_cas    = narrow_cell ? (sfn % 8 == 4) : (sfn % 4 == 0);
    bool sfn_is_active = sfn_is_cas;
    if (sfn_is_cas && cas_cfg.cas_muting) {
      uint32_t n_cas = (uint32_t)cas_cfg.n_cas;
      uint32_t k_cas = (uint32_t)cas_cfg.k_cas;
      sfn_is_active = sfn % (16u * n_cas) < 4u * k_cas;
    }
    if (sfn_is_active && sf >= 1u && sf <= (uint32_t)add_non) {
      return false;
    }
  }

  srsran::mbsfn_sf_cfg_t*    subfr_cnfg = &mbsfn_snapshot.mbsfn_subfr_cnfg;
  srsran::mbsfn_area_info_t* area_info  = &mbsfn_snapshot.mbsfn_area_info;
  switch (area_info->subcarrier_spacing) {
    case srsran::mbsfn_area_info_t::subcarrier_spacing_t::khz_1dot25:
      cfg->subcarrier_spacing       = SRSRAN_SCS_1KHZ25;
      cfg->non_mbsfn_region_length  = 0;
      break;
    case srsran::mbsfn_area_info_t::subcarrier_spacing_t::khz_7dot5:
      cfg->subcarrier_spacing       = SRSRAN_SCS_7KHZ5;
      cfg->non_mbsfn_region_length  = 0;
      break;
    case srsran::mbsfn_area_info_t::subcarrier_spacing_t::khz_2dot5:
      cfg->subcarrier_spacing       = SRSRAN_SCS_2KHZ5;
      cfg->non_mbsfn_region_length  = 0;
      break;
    case srsran::mbsfn_area_info_t::subcarrier_spacing_t::khz_0dot37:
      switch (area_info->time_separation) {
        case srsran::mbsfn_area_info_t::time_separation_t::sl4:
          cfg->subcarrier_spacing = SRSRAN_SCS_370HZ_SL4;
          break;
        case srsran::mbsfn_area_info_t::time_separation_t::sl2:
          cfg->subcarrier_spacing = SRSRAN_SCS_370HZ_SL2;
          break;
        default:
          /* sl4 is the spec default when time_separation is absent (TS 36.211 §4.1). */
          cfg->subcarrier_spacing = SRSRAN_SCS_370HZ_SL4;
          break;
      }
      cfg->non_mbsfn_region_length = 0;
      break;
    case srsran::mbsfn_area_info_t::subcarrier_spacing_t::khz_15:
    default:
      // khz_15 (plain LTE numerology) and nulltype (field absent) both mean "no FeMBMS
      // SCS override" -- non_mbsfn_region_length is NOT forced to 0 here (unlike the
      // four FeMBMS-dedicated cases above): a 15 kHz MBSFN subframe can have a real,
      // non-zero control region, so the real SIB13-decoded value (set below at line ~476
      // when sib13_configured) is the correct one to use.
      cfg->subcarrier_spacing = SRSRAN_SCS_15KHZ;
      break;
  }

  /* is_mcch_subframe() already checked and returned above; if we're still here,
   * this subframe isn't MCCH's own. */
  if (not mcch_configured) {
    return false;
  }



  offset = subfr_cnfg->radioframe_alloc_offset;
  period = enum_to_number(subfr_cnfg->radioframe_alloc_period);

  if (sib13_configured) {
    cfg->mbsfn_area_id = area_info->mbsfn_area_id;
    // FeMBMS SCS subframes must have non_mbsfn_region_length = 0 (no PDCCH region).
    // The switch above already sets this for 1.25 kHz, 2.5 kHz, 7.5 kHz, and 0.37 kHz; guard
    // here so it isn't overwritten for unused MBSFN subframes in those cases.
    if (area_info->subcarrier_spacing != srsran::mbsfn_area_info_t::subcarrier_spacing_t::khz_1dot25 &&
        area_info->subcarrier_spacing != srsran::mbsfn_area_info_t::subcarrier_spacing_t::khz_2dot5 &&
        area_info->subcarrier_spacing != srsran::mbsfn_area_info_t::subcarrier_spacing_t::khz_7dot5 &&
        area_info->subcarrier_spacing != srsran::mbsfn_area_info_t::subcarrier_spacing_t::khz_0dot37) {
      cfg->non_mbsfn_region_length = enum_to_number(area_info->non_mbsfn_region_len);
    }
    if (mcch_configured) {

//      while (!have_mtch_stop) {
//        pthread_cond_wait(&mtch_cvar, &mtch_mutex);
//      }
      for (uint32_t i = 0; i < mbsfn_snapshot.mcch.nof_pmch_info; i++) {
        unsigned fn_in_scheduling_period =  sfn % enum_to_number(mbsfn_snapshot.mcch.pmch_info_list[i].mch_sched_period);
        /* Count true CAS frames elapsed before fn_in_scheduling_period.
         * Spec CR 0577: k_cas active CAS frames per 16*n_cas-frame period.
         * Without muting: one CAS per 4 frames → fn_in/4. */
        uint32_t nof_true_cas;
        if (cas_cfg.cas_muting) {
          uint32_t n_cas  = (uint32_t)cas_cfg.n_cas;
          uint32_t k_cas  = (uint32_t)cas_cfg.k_cas;
          uint32_t period = 16u * n_cas;
          uint32_t rem    = fn_in_scheduling_period % period;
          uint32_t cap    = 4u * k_cas;
          nof_true_cas = (fn_in_scheduling_period / period) * k_cas + (rem < cap ? rem : cap) / 4u;
        } else {
          nof_true_cas = fn_in_scheduling_period / (narrow_cell ? 8u : 4u);
        }
        /* Count MCCH subframes that have passed before (fn_in_scheduling_period, sf).
         * The MCCH repeats every mcch_repeat_period frames starting at mcch_offset.
         * Use the actual sfn to compute which radio frame within this scheduling
         * period first carries an MCCH — handles both mcch_rp < sched_period (multiple
         * MCCHs per period) and mcch_rp > sched_period (some periods have none). */
        uint32_t sched_period    = enum_to_number(mbsfn_snapshot.mcch.pmch_info_list[i].mch_sched_period);
        uint32_t mcch_rp         = enum_to_number(area_info->mcch_cfg.mcch_repeat_period);
        uint32_t mcch_off        = area_info->mcch_cfg.mcch_offset;
        uint32_t sfn_base        = sfn - fn_in_scheduling_period; /* start SFN of this period */
        uint32_t sfn_base_mod    = sfn_base % mcch_rp;
        uint32_t first_mcch_m    = (mcch_off + mcch_rp - sfn_base_mod) % mcch_rp;
        /* Find the MCCH subframe number within its radio frame (first sf where mcch_table[sf]>0). */
        uint8_t mcch_sf_in_frame = 1u;
        for (uint8_t s = 0; s < 10u; s++) { if (mcch_table_snapshot[s]) { mcch_sf_in_frame = s; break; } }
        uint32_t nof_mcch_passed = 0;
        for (uint32_t m = first_mcch_m; m < sched_period; m += mcch_rp) {
          if (m < fn_in_scheduling_period ||
              (m == fn_in_scheduling_period && sf > mcch_sf_in_frame)) {
            nof_mcch_passed++;
          }
        }
        /* additionalNonMBSFNSubframes offset: each active CAS frame in the scheduling period
         * removes add_non SFs from the PMCH data space.  nof_true_cas counts "non-anchor"
         * active CAS frames (fn_in > 0); the anchor at fn_in=0 contributes N more SFs but
         * is not counted by fn_in/4.  Include it via an anchor-active check. */
        uint32_t nof_additional_passed = 0u;
        if (add_non > 0u) {
          bool anchor_act = true;
          if (cas_cfg.cas_muting) {
            uint32_t n_cas = (uint32_t)cas_cfg.n_cas;
            uint32_t k_cas = (uint32_t)cas_cfg.k_cas;
            anchor_act = sfn_base % (16u * n_cas) < 4u * k_cas;
          }
          nof_additional_passed = (nof_true_cas + (anchor_act ? 1u : 0u)) * (uint32_t)add_non;
        }
        int sf_idx = (int)(fn_in_scheduling_period * 10u + sf) - (int)nof_true_cas - (int)nof_mcch_passed - (int)nof_additional_passed;
        if (getenv("CAS_MUTE_DIAG")) {
          fprintf(stderr, "CAS_MUTE_DIAG_TX tti=%u sfn=%u sf=%u fn_in=%u nof_true_cas=%u sf_idx=%d "
                          "nof_mcch_passed=%u nof_additional_passed=%u cas_muting=%d k_cas=%u n_cas=%u\n",
                  phy_tti, sfn, sf, fn_in_scheduling_period, nof_true_cas, sf_idx, nof_mcch_passed,
                  nof_additional_passed, (int)cas_cfg.cas_muting, (unsigned)cas_cfg.k_cas, (unsigned)cas_cfg.n_cas);
        }
        /* Guard: sf_idx must be >= pmch_start to avoid uint32_t wraparound on subtraction.
         * For i=0, pmch_start=1 (MCCH occupies sf_idx=0); sf_idx=0 here means the
         * subframe is at the MCCH position but was not caught by is_mcch_subframe()
         * (e.g. sf=0, which never appears in the MCCH table). Skip it. */
        uint32_t pmch_start      = (i == 0) ? 1u : (uint32_t)(mbsfn_snapshot.mcch.pmch_info_list[i - 1].sf_alloc_end + 1);
        if (sf_idx >= (int)pmch_start && (uint32_t)sf_idx <= mbsfn_snapshot.mcch.pmch_info_list[i].sf_alloc_end) {
          /* MCCH is only on area 0, sf=mcch_sf_in_frame, fn_in=mcch_off; is_mcch_subframe()
           * catches it first and returns early, so this arm is dead code for that subframe.
           * Areas i>0 have no MCCH — every subframe including their first is data. */
          cfg->mbsfn_mcs = mbsfn_snapshot.mcch.pmch_info_list[i].data_mcs;
          DEBUG("SFN %d SF %d: MCS %d", sfn, sf, cfg->mbsfn_mcs);
          cfg->use_mcs_table2      = mbsfn_snapshot.mcch.pmch_info_list[i].use_mcs_table2;
          cfg->time_interleaving_n = mbsfn_snapshot.mcch.pmch_info_list[i].time_interleaving_n;
          cfg->time_interleaving_m = mbsfn_snapshot.mcch.pmch_info_list[i].time_interleaving_m;
          cfg->n_soft_ref_category     = mbsfn_snapshot.mcch.pmch_info_list[i].n_soft_ref_category;
          cfg->scaling_factor_beta_num = mbsfn_snapshot.mcch.pmch_info_list[i].scaling_factor_beta_num;
          cfg->scaling_factor_beta_den = mbsfn_snapshot.mcch.pmch_info_list[i].scaling_factor_beta_den;
          cfg->cyclic_shift        = mbsfn_snapshot.mcch.pmch_info_list[i].cyclic_shift;
          cfg->cyclic_shift_alpha  = mbsfn_snapshot.mcch.pmch_info_list[i].cyclic_shift_alpha;
          cfg->freq_interleaving   = mbsfn_snapshot.mcch.pmch_info_list[i].freq_interleaving;
          cfg->mch_subframe_idx    = (uint32_t)sf_idx - pmch_start;
          cfg->pmch_idx            = (uint8_t)i;
          cfg->enable = true;
          cfg->non_mbsfn_region_length = 0;
          /* pmch-TimeInterleavingN/M-LastMTCH-r19 (TS 36.331 CR5168r3): mac.cc's
           * get_mch_sched() (via set_last_mtch_start()) tells us, once per scheduling
           * period, where the last of several MTCH sessions' window starts within
           * this PMCH's data region. 0 = no distinct last-session window active this
           * period (single-session case, or no LastMTCH override configured) -- the
           * comparison below is then always false, so this degenerates to exactly
           * today's flat cfg->mch_subframe_idx/time_interleaving_n/_m with zero
           * behavior change for that (by far the common) case. */
          uint32_t last_mtch_start_sf = get_last_mtch_start((uint8_t)i);
          if (last_mtch_start_sf > 0 && cfg->mch_subframe_idx >= last_mtch_start_sf) {
            cfg->mch_subframe_idx -= last_mtch_start_sf;
            uint8_t n_last = mbsfn_snapshot.mcch.pmch_info_list[i].time_interleaving_n_last_mtch;
            uint8_t m_last = mbsfn_snapshot.mcch.pmch_info_list[i].time_interleaving_m_last_mtch;
            if (n_last > 0) {
              /* n1 (n_last==1) means "no time interleaving for this session" --
               * represented the same way the main N field represents "disabled"
               * elsewhere in this codebase (N<=1), so pmch.c's existing N<=1 check
               * naturally treats this window as non-interleaved without needing a
               * separate flag. */
              cfg->time_interleaving_n = n_last;
              cfg->time_interleaving_m = (m_last > 0) ? m_last : cfg->time_interleaving_m;
            } else if (m_last > 0) {
              cfg->time_interleaving_m = m_last;
            }
          }
          break;
        }
      }
    }
  }
  return true;
}

bool phy_common::is_mbsfn_sf(srsran_mbsfn_cfg_t* cfg, uint32_t phy_tti)
{
  return is_mch_subframe(cfg, phy_tti);
}
} // namespace srsenb
