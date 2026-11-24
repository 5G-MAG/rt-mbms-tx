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

#include "srsenb/hdr/stack/mac/sched_carrier.h"
#include "srsenb/hdr/stack/mac/sched_helpers.h"
#include "srsenb/hdr/stack/mac/schedulers/sched_time_pf.h"
#include "srsenb/hdr/stack/mac/schedulers/sched_time_rr.h"
#include "srsran/common/standard_streams.h"
#include "srsran/common/string_helpers.h"
#include "srsran/interfaces/enb_rrc_interfaces.h"

namespace srsenb {

using srsran::tti_point;

/*******************************************************
 *        Broadcast (SIB+Paging) scheduling
 *******************************************************/

bc_sched::bc_sched(const sched_cell_params_t& cfg_, srsenb::rrc_interface_mac* rrc_) :
  cc_cfg(&cfg_), rrc(rrc_), logger(srslog::fetch_basic_logger("MAC"))
{}

void bc_sched::dl_sched(sf_sched* tti_sched)
{
  current_tti = tti_sched->get_tti_tx_dl();
  /* TS 36.213 v19.4.0 9.1.1: on an MBMS-dedicated cell, a UE monitors a
   * common search space for MBMS reception at aggregation level L=16 with a
   * single PDCCH candidate -- both the level and the "single candidate" part
   * are spec-mandated, not implementation choices. AL16 needs 16 contiguous
   * CCEs (1<<4), which a narrow cell only has at a high enough CFI -- e.g. a
   * 25 PRB cell provides just 4/13/21 CCEs at CFI=1/2/3 respectively
   * (nof_cce_table), so AL16 is only actually usable at CFI=3. Blindly
   * requesting it regardless of the cell's *effective* CFI (the
   * semiStaticCFI-MBMS-r16 override when configured, since that's what's
   * really transmitted on CAS subframes -- see semi_static_cfi's own
   * assignment in enb_cfg_parser.cc for why; otherwise the scheduler's own
   * max_nof_ctrl_symbols ceiling) makes bc_sched::alloc_sibs unconditionally
   * fail with no_cch_space forever whenever that CFI doesn't provide enough
   * CCEs -- SIB1/paging never gets broadcast at all, silently. Pick the
   * highest aggregation level that actually fits instead; alloc_sibs()'s own
   * round-robin (see below) is what enforces the "single candidate" part by
   * giving every concurrently-pending SI message a fair turn at it over
   * successive CAS occasions, rather than lowering the aggregation level. */
  if (cc_cfg->cfg.cell.mbms_dedicated) {
    uint32_t effective_cfi = (cc_cfg->cfg.cell.semi_static_cfi != 0) ? cc_cfg->cfg.cell.semi_static_cfi
                                                                      : cc_cfg->sched_cfg->max_nof_ctrl_symbols;
    uint32_t ncce_avail = cc_cfg->nof_cce_table[effective_cfi - 1];
    if (ncce_avail >= 16) {
      bc_aggr_level = 4;
    } else if (ncce_avail >= 8) {
      bc_aggr_level = 3;
    } else if (ncce_avail >= 4) {
      bc_aggr_level = 2;
    } else if (ncce_avail >= 2) {
      bc_aggr_level = 1;
    } else {
      bc_aggr_level = 0;
    }
  } else {
    bc_aggr_level = 2;
  }

  /* Activate/deactivate SI windows */
  update_si_windows(tti_sched);

  /* Allocate DCIs and RBGs for each SIB */
  alloc_sibs(tti_sched);

  /* Allocate Paging */
  // NOTE: It blocks
  alloc_paging(tti_sched);
}

void bc_sched::update_si_windows(sf_sched* tti_sched)
{
  tti_point tti_tx_dl      = tti_sched->get_tti_tx_dl();
  uint32_t  current_sf_idx = tti_sched->get_tti_tx_dl().sf_idx();
  uint32_t  current_sfn    = tti_sched->get_tti_tx_dl().sfn();

  for (uint32_t i = 0; i < pending_sibs.size(); ++i) {
    // There is SIB data
    if (cc_cfg->cfg.sibs[i].len == 0) {
      continue;
    }

    if (not pending_sibs[i].is_in_window) {
      uint32_t sf = 0;
      uint32_t x  = 0;
      if (i > 0) {
        x  = (i - 1) * cc_cfg->cfg.si_window_ms;
        sf = x % 10;
      }
      if ((current_sfn % (cc_cfg->cfg.sibs[i].period_rf)) == x / 10 && current_sf_idx == sf) {
        pending_sibs[i].is_in_window = true;
        pending_sibs[i].window_start = tti_tx_dl;
        pending_sibs[i].n_tx         = 0;
      }
    } else {
      if (i > 0) {
        if (pending_sibs[i].window_start + cc_cfg->cfg.si_window_ms < tti_tx_dl) {
          // the si window has passed
          pending_sibs[i] = {};
        }
      } else {
        // SIB1 is always in window
        if (pending_sibs[0].n_tx == 4) {
          pending_sibs[0].n_tx = 0;
        }
      }
    }
  }
}

void bc_sched::alloc_sibs(sf_sched* tti_sched)
{
  uint32_t current_sf_idx = tti_sched->get_tti_tx_dl().sf_idx();
  uint32_t current_sfn    = tti_sched->get_tti_tx_dl().sfn();

  /* MBMS-dedicated cells only have a CAS (cell-acquisition subframe) occasion
   * in SF0 every 40ms (nof_prb>=25: sfn%4==0; narrower cells sfn%8==4) -- SIBs
   * are never delivered on any other subframe, unlike a normal cell's greedy
   * within-window scheduling below. additionalNonMBSFNSubframes is a distinct
   * mechanism (used for e.g. MCCH change notifications, TS 36.331's own
   * notificationSF-Index-r9 description) and is *not* an extra pool of SI
   * message capacity, despite alloc_sibs() previously treating it as one. */
  bool mbms_sf_active = true;
  if (cc_cfg->cfg.cell.mbms_dedicated) {
    uint32_t nof_prb   = cc_cfg->cfg.cell.nof_prb;
    bool is_cas_sfn    = (nof_prb >= 25u) ? (current_sfn % 4u == 0u) : (current_sfn % 8u == 4u);
    bool sfn_is_active = is_cas_sfn;
    if (is_cas_sfn && cc_cfg->cfg.cell.cas_muting) {
      uint32_t n_cas = (uint32_t)cc_cfg->cfg.cell.n_cas;
      uint32_t k_cas = (uint32_t)cc_cfg->cfg.cell.k_cas;
      sfn_is_active  = current_sfn % (16u * n_cas) < 4u * k_cas;
    }
    mbms_sf_active = sfn_is_active && current_sf_idx == 0;
  }
  if (!mbms_sf_active) {
    return;
  }

  /* TS 36.213 9.1.1: the MBMS common search space provides a *single* PDCCH
   * candidate at AL16 in that one CAS occasion -- at most one SI message's
   * DCI can ever fit there. A SIB with an always-open window (e.g. SIB2)
   * would otherwise win that one candidate every single occasion in the
   * fixed sib_idx-ascending order below, starving every other concurrently-
   * pending SI message (including a dynamically-activated SIB12 CMAS/PWS
   * alert) via no_sch_space forever, since the retry loop below only retries
   * on coderate, not on CCE exhaustion. Round-robin which sib_idx gets first
   * attempt each occasion instead, so every pending SI message eventually
   * gets a turn at that one candidate. For non-MBMS-dedicated cells this is
   * a no-op in practice: they have more than one CCE opportunity per
   * subframe, so the first successful nrbgs attempt below is not the only
   * chance remaining sib_idx values get in the same subframe. */
  for (uint32_t attempt = 0; attempt < pending_sibs.size(); attempt++) {
    uint32_t     sib_idx     = (next_sib_priority_idx + attempt) % pending_sibs.size();
    sched_sib_t& pending_sib = pending_sibs[sib_idx];
    // Check if SIB is configured and within window
    if (cc_cfg->cfg.sibs[sib_idx].len == 0 or not pending_sib.is_in_window or pending_sib.n_tx >= 4) {
      continue;
    }

    // Attempt PDSCH grants with increasing number of RBGs
    alloc_result ret = alloc_result::invalid_coderate;
    for (uint32_t nrbgs = 1; nrbgs < cc_cfg->nof_rbgs and ret == alloc_result::invalid_coderate; ++nrbgs) {
      rbg_interval rbg_interv = find_empty_rbg_interval(nrbgs, tti_sched->get_dl_mask());
      if (rbg_interv.length() != nrbgs) {
        ret = alloc_result::no_sch_space;
        break;
      }
      ret = tti_sched->alloc_sib(bc_aggr_level, sib_idx, pending_sibs[sib_idx].n_tx, rbg_interv);
      if (ret == alloc_result::success) {
        // SIB scheduled successfully
        pending_sibs[sib_idx].n_tx++;
      }
    }
    if (ret != alloc_result::success) {
      logger.warning("SCHED: Could not allocate SI message, idx=%d, len=%d. Cause: %s",
                     sib_idx,
                     cc_cfg->cfg.sibs[sib_idx].len,
                     to_string(ret));
      continue;
    }
    // Won this occasion's one candidate -- give the next SI message first
    // priority next occasion, and stop (nothing else can fit here anyway).
    next_sib_priority_idx = (sib_idx + 1) % pending_sibs.size();
    return;
  }
  // Nobody was eligible/fit this occasion -- still rotate, so a SIB that
  // repeatedly fails (e.g. coderate) doesn't also block others from ever
  // getting first attempt.
  next_sib_priority_idx = (next_sib_priority_idx + 1) % pending_sibs.size();
}

void bc_sched::alloc_paging(sf_sched* tti_sched)
{
  uint32_t paging_payload = 0;

  // Check if pending Paging message
  if (not rrc->is_paging_opportunity(tti_sched->get_tti_tx_dl().to_uint(), &paging_payload) or paging_payload == 0) {
    return;
  }

  alloc_result ret = alloc_result::invalid_coderate;
  for (uint32_t nrbgs = 1; nrbgs < cc_cfg->nof_rbgs and ret == alloc_result::invalid_coderate; ++nrbgs) {
    rbg_interval rbg_interv = find_empty_rbg_interval(nrbgs, tti_sched->get_dl_mask());
    if (rbg_interv.length() != nrbgs) {
      ret = alloc_result::no_sch_space;
      break;
    }

    ret = tti_sched->alloc_paging(bc_aggr_level, paging_payload, rbg_interv);
  }

  if (ret != alloc_result::success) {
    logger.warning("SCHED: Could not allocate Paging with payload length=%d, cause=%s", paging_payload, to_string(ret));
  }
}

void bc_sched::reset()
{
  for (auto& sib : pending_sibs) {
    sib = {};
  }
}

/*******************************************************
 *                 RAR scheduling
 *******************************************************/

ra_sched::ra_sched(const sched_cell_params_t& cfg_, sched_ue_list& ue_db_) :
  cc_cfg(&cfg_), logger(srslog::fetch_basic_logger("MAC")), ue_db(&ue_db_)
{}

alloc_result ra_sched::allocate_pending_rar(sf_sched* tti_sched, const pending_rar_t& rar, uint32_t& nof_grants_alloc)
{
  alloc_result ret = alloc_result::other_cause;
  for (nof_grants_alloc = rar.msg3_grant.size(); nof_grants_alloc > 0; nof_grants_alloc--) {
    ret = alloc_result::invalid_coderate;
    for (uint32_t nrbg = 1; nrbg < cc_cfg->nof_rbgs and ret == alloc_result::invalid_coderate; ++nrbg) {
      rbg_interval rbg_interv = find_empty_rbg_interval(nrbg, tti_sched->get_dl_mask());
      if (rbg_interv.length() == nrbg) {
        ret = tti_sched->alloc_rar(rar_aggr_level, rar, rbg_interv, nof_grants_alloc);
      } else {
        ret = alloc_result::no_sch_space;
      }
    }

    // If allocation was not successful because there were not enough RBGs, try allocating fewer Msg3 grants
    if (ret != alloc_result::invalid_coderate and ret != alloc_result::no_sch_space) {
      break;
    }
  }
  if (ret != alloc_result::success) {
    logger.info("SCHED: RAR allocation for L=%d was postponed. Cause=%s", rar_aggr_level, to_string(ret));
  }
  return ret;
}

// Schedules RAR
// On every call to this function, we schedule the oldest RAR which is still within the window. If outside the window we
// discard it.
void ra_sched::dl_sched(sf_sched* tti_sched)
{
  tti_point tti_tx_dl = tti_sched->get_tti_tx_dl();
  rar_aggr_level      = 2;

  for (auto it = pending_rars.begin(); it != pending_rars.end();) {
    auto& rar = *it;

    // In case of RAR outside RAR window:
    // - if window has passed, discard RAR
    // - if window hasn't started, stop loop, as RARs are ordered by TTI
    srsran::tti_interval rar_window{rar.prach_tti + PRACH_RAR_OFFSET,
                                    rar.prach_tti + PRACH_RAR_OFFSET + cc_cfg->cfg.prach_rar_window};
    if (not rar_window.contains(tti_tx_dl)) {
      if (tti_tx_dl >= rar_window.stop()) {
        fmt::memory_buffer str_buffer;
        fmt::format_to(str_buffer,
                       "SCHED: Could not transmit RAR within the window (RA={}, Window={}, RAR={}",
                       rar.prach_tti,
                       rar_window,
                       tti_tx_dl);
        srsran::console("%s\n", srsran::to_c_str(str_buffer));
        logger.warning("%s", srsran::to_c_str(str_buffer));
        it = pending_rars.erase(it);
        continue;
      }
      return;
    }

    // Try to schedule DCI + RBGs for RAR Grant
    uint32_t     nof_rar_allocs = 0;
    alloc_result ret            = allocate_pending_rar(tti_sched, rar, nof_rar_allocs);

    if (ret == alloc_result::success) {
      // If RAR allocation was successful:
      // - in case all Msg3 grants were allocated, remove pending RAR, and continue with following RAR
      // - otherwise, erase only Msg3 grants that were allocated, and stop iteration

      if (nof_rar_allocs == rar.msg3_grant.size()) {
        it = pending_rars.erase(it);
      } else {
        std::copy(rar.msg3_grant.begin() + nof_rar_allocs, rar.msg3_grant.end(), rar.msg3_grant.begin());
        rar.msg3_grant.resize(rar.msg3_grant.size() - nof_rar_allocs);
        break;
      }
    } else {
      // If RAR allocation was not successful:
      // - in case of unavailable PDCCH space, try next pending RAR allocation
      // - otherwise, stop iteration
      if (ret != alloc_result::no_cch_space) {
        break;
      }
      ++it;
    }
  }
}

int ra_sched::dl_rach_info(dl_sched_rar_info_t rar_info)
{
  logger.info("SCHED: New PRACH tti=%d, preamble=%d, temp_crnti=0x%x, ta_cmd=%d, msg3_size=%d",
              rar_info.prach_tti,
              rar_info.preamble_idx,
              rar_info.temp_crnti,
              rar_info.ta_cmd,
              rar_info.msg3_size);
  // RA-RNTI = 1 + t_id + f_id
  // t_id = index of first subframe specified by PRACH (0<=t_id<10)
  // f_id = index of the PRACH within subframe, in ascending order of freq domain (0<=f_id<6) (for FDD, f_id=0)
  uint16_t ra_rnti = 1 + (uint16_t)(rar_info.prach_tti % 10u);

  // find pending rar with same RA-RNTI
  for (pending_rar_t& r : pending_rars) {
    if (r.prach_tti.to_uint() == rar_info.prach_tti and ra_rnti == r.ra_rnti) {
      if (r.msg3_grant.size() >= sched_interface::MAX_RAR_LIST) {
        logger.warning("PRACH ignored, as the the maximum number of RAR grants per tti has been reached");
        return SRSRAN_ERROR;
      }
      r.msg3_grant.push_back(rar_info);
      return SRSRAN_SUCCESS;
    }
  }

  // create new RAR
  pending_rar_t p;
  p.ra_rnti   = ra_rnti;
  p.prach_tti = tti_point{rar_info.prach_tti};
  p.msg3_grant.push_back(rar_info);
  pending_rars.push_back(p);

  return SRSRAN_SUCCESS;
}

//! Schedule Msg3 grants in UL based on allocated RARs
void ra_sched::ul_sched(sf_sched* sf_dl_sched, sf_sched* sf_msg3_sched)
{
  srsran::const_span<sf_sched::rar_alloc_t> alloc_rars = sf_dl_sched->get_allocated_rars();

  for (const auto& rar : alloc_rars) {
    for (const auto& msg3grant : rar.rar_grant.msg3_grant) {
      uint16_t crnti   = msg3grant.data.temp_crnti;
      auto     user_it = ue_db->find(crnti);
      if (user_it != ue_db->end() and
          sf_msg3_sched->alloc_msg3(user_it->second.get(), msg3grant) == alloc_result::success) {
        logger.debug("SCHED: Queueing Msg3 for rnti=0x%x at tti=%d", crnti, sf_msg3_sched->get_tti_tx_ul().to_uint());
      } else {
        logger.error(
            "SCHED: Failed to allocate Msg3 for rnti=0x%x at tti=%d", crnti, sf_msg3_sched->get_tti_tx_ul().to_uint());
      }
    }
  }
}

void ra_sched::reset()
{
  pending_rars.clear();
}

/*******************************************************
 *                 Carrier scheduling
 *******************************************************/

sched::carrier_sched::carrier_sched(rrc_interface_mac*       rrc_,
                                    sched_ue_list*           ue_db_,
                                    uint32_t                 enb_cc_idx_,
                                    sched_result_ringbuffer* sched_results_) :
  rrc(rrc_),
  ue_db(ue_db_),
  logger(srslog::fetch_basic_logger("MAC")),
  enb_cc_idx(enb_cc_idx_),
  prev_sched_results(sched_results_)
{
  sf_dl_mask.resize(1, 0);
}

sched::carrier_sched::~carrier_sched() = default;

void sched::carrier_sched::reset()
{
  ra_sched_ptr.reset();
  bc_sched_ptr.reset();
}

void sched::carrier_sched::carrier_cfg(const sched_cell_params_t& cell_params_)
{
  // carrier_sched is now fully set
  cc_cfg = &cell_params_;

  // init Broadcast/RA schedulers
  bc_sched_ptr.reset(new bc_sched{*cc_cfg, rrc});
  ra_sched_ptr.reset(new ra_sched{*cc_cfg, *ue_db});

  // Setup data scheduling algorithms
  if (cell_params_.sched_cfg->sched_policy == "time_rr") {
    sched_algo.reset(new sched_time_rr{*cc_cfg, *cell_params_.sched_cfg});
    logger.info("Using time-domain RR scheduling policy for cc=%d", cc_cfg->enb_cc_idx);
  } else {
    sched_algo.reset(new sched_time_pf{*cc_cfg, *cell_params_.sched_cfg});
    logger.info("Using time-domain PF scheduling policy for cc=%d", cc_cfg->enb_cc_idx);
  }

  // Initiate the tti_scheduler for each TTI
  for (sf_sched& tti_sched : sf_scheds) {
    tti_sched.init(*cc_cfg);
  }
}

void sched::carrier_sched::set_dl_tti_mask(uint8_t* tti_mask, uint32_t nof_sfs)
{
  sf_dl_mask.assign(tti_mask, tti_mask + nof_sfs);
}

const cc_sched_result& sched::carrier_sched::generate_tti_result(tti_point tti_rx)
{
  sf_sched*        tti_sched = get_sf_sched(tti_rx);
  sf_sched_result* sf_result = prev_sched_results->get_sf(tti_rx);
  cc_sched_result* cc_result = sf_result->get_cc(enb_cc_idx);

  bool dl_active = sf_dl_mask[tti_sched->get_tti_tx_dl().to_uint() % sf_dl_mask.size()] == 0;

  /* Refresh UE internal buffers and subframe vars */
  /*for (auto& user : *ue_db) {
    user.second->new_subframe(tti_rx, enb_cc_idx);
  }*/

  /* Schedule PHICH */
//  for (auto& ue_pair : *ue_db) {
//    if (tti_sched->alloc_phich(ue_pair.second.get()) == alloc_result::no_grant_space) {
//      break;
//    }
//  }

  /* Schedule DL control data */
  if (dl_active) {
    /* Schedule Broadcast data (SIB and paging) */
    bc_sched_ptr->dl_sched(tti_sched);

    /* Schedule RAR */
//    ra_sched_ptr->dl_sched(tti_sched);

    /* Schedule Msg3 */
//    sf_sched* sf_msg3_sched = get_sf_sched(tti_rx + MSG3_DELAY_MS);
//    ra_sched_ptr->ul_sched(tti_sched, sf_msg3_sched);
  }

  /* Prioritize PDCCH scheduling for DL and UL data in a RoundRobin fashion */
  //if ((tti_rx.to_uint() % 2) == 0) {
    //alloc_ul_users(tti_sched);
  //}

  /* Schedule DL user data */
  //alloc_dl_users(tti_sched);

  //if ((tti_rx.to_uint() % 2) == 1) {
    //alloc_ul_users(tti_sched);
  //}

  /* Select the winner DCI allocation combination, store all the scheduling results */
  tti_sched->generate_sched_results(*ue_db);

  /* Reset ue harq pending ack state, clean-up blocked pids */
  /*for (auto& user : *ue_db) {
    user.second->finish_tti(tti_rx, enb_cc_idx);
  }*/

  log_dl_cc_results(logger, enb_cc_idx, cc_result->dl_sched_result);
  log_phich_cc_results(logger, enb_cc_idx, cc_result->ul_sched_result);

  return *cc_result;
}

void sched::carrier_sched::alloc_dl_users(sf_sched* tti_result)
{
  if (sf_dl_mask[tti_result->get_tti_tx_dl().to_uint() % sf_dl_mask.size()] != 0) {
    return;
  }

  // NOTE: In case of 6 PRBs, do not transmit if there is going to be a PRACH in the UL to avoid collisions
  if (cc_cfg->nof_prb() == 6) {
    tti_point tti_rx_ack = to_tx_dl_ack(tti_result->get_tti_rx());
    if (srsran_prach_in_window_config_fdd(cc_cfg->cfg.prach_config, tti_rx_ack.to_uint(), -1)) {
      tti_result->reserve_dl_rbgs(0, cc_cfg->nof_rbgs);
    }
  }

  // call DL scheduler metric to fill RB grid
  sched_algo->sched_dl_users(*ue_db, tti_result);
}

int sched::carrier_sched::alloc_ul_users(sf_sched* tti_sched)
{
  /* Call scheduler for UL data */
  sched_algo->sched_ul_users(*ue_db, tti_sched);

  return SRSRAN_SUCCESS;
}

sf_sched* sched::carrier_sched::get_sf_sched(tti_point tti_rx)
{
  sf_sched* ret = &sf_scheds[tti_rx.to_uint()];
  if (ret->get_tti_rx() != tti_rx) {
    if (not prev_sched_results->has_sf(tti_rx)) {
      // Reset if tti_rx has not been yet set in the sched results
      prev_sched_results->new_tti(tti_rx);
    }
    sf_sched_result* sf_res = prev_sched_results->get_sf(tti_rx);
    // start new TTI for the given CC.
    ret->new_tti(tti_rx, sf_res);
  }
  return ret;
}

const sf_sched_result* sched::carrier_sched::get_sf_result(tti_point tti_rx) const
{
  return prev_sched_results->get_sf(tti_rx);
}

int sched::carrier_sched::dl_rach_info(dl_sched_rar_info_t rar_info)
{
  return ra_sched_ptr->dl_rach_info(rar_info);
}

} // namespace srsenb
