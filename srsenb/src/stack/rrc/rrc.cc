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

#include "srsenb/hdr/stack/rrc/rrc.h"
#include <cstdio>
#include "srsenb/hdr/stack/mac/sched_interface.h"
#include "srsenb/hdr/stack/rrc/rrc_cell_cfg.h"
#include "srsenb/hdr/stack/rrc/rrc_endc.h"
#include "srsenb/hdr/stack/rrc/rrc_mobility.h"
#include "srsenb/hdr/stack/rrc/rrc_paging.h"
#include "srsenb/hdr/stack/s1ap/s1ap.h"
#include "srsran/asn1/asn1_utils.h"
#include "srsran/asn1/rrc_utils.h"
#include "srsran/common/bcd_helpers.h"
#include "srsran/common/enb_events.h"
#include "srsran/common/rwlock_guard.h"
#include "srsran/common/standard_streams.h"
#include "srsran/common/string_helpers.h"
#include "srsran/interfaces/enb_mac_interfaces.h"
#include "srsran/interfaces/enb_pdcp_interfaces.h"
#include "srsran/interfaces/enb_rlc_interfaces.h"
#include "srsran/phy/phch/ra_dl.h"

using srsran::byte_buffer_t;

using namespace asn1::rrc;

// Forward-declare parse_sib12 to avoid pulling libconfig into this TU
namespace srsenb { namespace sib_sections {
int parse_sib12(const std::string& filename, asn1::rrc::sib_type12_r9_s* data);
} } // namespace sib_sections, srsenb

namespace srsenb {

/* Largest PMCH mcs_idx <= requested_mcs whose transport block actually fits in the
 * MBSFN subframe's data REs. srsran_pmch_fill_ra_mcs() picks modulation/TBS purely
 * from TS 36.213 Table 7.1.7.1-1 (mirroring the classic PDSCH table, per spec clause
 * 11.1) - it has no notion of the *available* RE budget, so nothing stops an operator
 * from configuring an MCS whose TBS+24 exceeds Qm*nof_re, i.e. a code rate above 1:
 * a transport block that cannot decode even in principle (reproduced directly via
 * pmch_test at mcs 26-28/100 PRB - every codeblock hits max iterations and CRC=KO,
 * not a subtle bug). Guard against silently configuring that here, using the
 * worst-case (fewest available REs) standard 15kHz MBSFN budget: cfi=2, extended CP.
 * A real non_mbsfn_region_length=1 config has strictly more REs than assumed here, so
 * this can only be conservative, never let an infeasible MCS through.
 * Only meaningful for the standard 15kHz path - FeMBMS-dedicated low-SCS carriers
 * have no control region and a very different (much larger) RE budget; this does not
 * attempt to model those and must not be called for them.
 *
 * KNOWN LIMITATION (found July 2026, not fixed): this only guarantees code rate <= 1,
 * i.e. mathematically decodable in principle - not that decode is actually reliable.
 * A real ZMQ-loopback test at nof_prb=25 with mbms_mcs=28 (clamped by this function to
 * 26, rate <=1) measured ~99.9% MTCH CRC failure over a real, non-identity channel; the
 * same test at mcs=2 decoded cleanly (0 CRC errors, all packets delivered). So a
 * configuration that passes this check can still be far too marginal to use in
 * practice. No safety margin is applied here - deliberately: the one data point above
 * establishes that the problem exists, but not how much margin would actually fix it,
 * and any specific number would be an invented guess dressed up as a numeric fix, not
 * something derived from spec or from calibrated link-budget/turbo-code analysis. If
 * this is worth hardening, it needs real characterization (e.g. turbo-code BLER-vs-code
 * rate curves at target SNR) before picking a number - not a guess. */
static uint16_t clamp_pmch_mcs_to_feasible(uint16_t requested_mcs, uint32_t nof_prb, bool use_mcs_table2)
{
  static const uint32_t worst_case_re_per_prb = 102; // cfi=2, EXT CP, both slots, MBSFN-RS excluded
  const uint32_t        nof_re                = nof_prb * worst_case_re_per_prb;
  for (uint16_t mcs = requested_mcs; mcs > 0; mcs--) {
    srsran_ra_tb_t tb = {};
    tb.mcs_idx        = mcs;
    if (srsran_pmch_fill_ra_mcs(&tb, nof_prb, use_mcs_table2, SRSRAN_SCS_15KHZ) < 0) {
      continue;
    }
    uint32_t qm = srsran_mod_bits_x_symbol(tb.mod);
    if ((uint32_t)tb.tbs + 24 <= qm * nof_re) {
      return mcs;
    }
  }
  return 0;
}

rrc::rrc(srsran::task_sched_handle task_sched_, enb_bearer_manager& manager_) :
  logger(srslog::fetch_basic_logger("RRC")), bearer_manager(manager_), task_sched(task_sched_), rx_pdu_queue(128)
{
  pthread_rwlock_init(&cell_common_list_rwlock, nullptr);
}

rrc::~rrc()
{
  pthread_rwlock_destroy(&cell_common_list_rwlock);
}

int32_t rrc::init(const rrc_cfg_t&       cfg_,
                  phy_interface_rrc_lte* phy_,
                  mac_interface_rrc*     mac_,
                  rlc_interface_rrc*     rlc_,
                  pdcp_interface_rrc*    pdcp_,
                  s1ap_interface_rrc*    s1ap_,
                  gtpu_interface_rrc*    gtpu_)
{
  return init(cfg_, phy_, mac_, rlc_, pdcp_, s1ap_, gtpu_, nullptr);
}

int32_t rrc::init(const rrc_cfg_t&       cfg_,
                  phy_interface_rrc_lte* phy_,
                  mac_interface_rrc*     mac_,
                  rlc_interface_rrc*     rlc_,
                  pdcp_interface_rrc*    pdcp_,
                  s1ap_interface_rrc*    s1ap_,
                  gtpu_interface_rrc*    gtpu_,
                  rrc_nr_interface_rrc*  rrc_nr_)
{
  phy    = phy_;
  mac    = mac_;
  rlc    = rlc_;
  pdcp   = pdcp_;
  gtpu   = gtpu_;
  s1ap   = s1ap_;
  rrc_nr = rrc_nr_;

  cfg = cfg_;

  if (cfg.sibs[12].type() == asn1::rrc::sys_info_r8_ies_s::sib_type_and_info_item_c_::types::sib13_v920 &&
      cfg.enable_mbsfn) {
    configure_mbsfn_sibs();
  }

  cell_res_list.reset(new freq_res_common_list{cfg});

  // Loads the PRACH root sequence
  cfg.sibs[1].sib2().rr_cfg_common.prach_cfg.root_seq_idx = cfg.cell_list[0].root_seq_idx;

  if (cfg.num_nr_cells > 0) {
    cfg.sibs[1].sib2().ext = true;
    cfg.sibs[1].sib2().plmn_info_list_r15.set_present();
    cfg.sibs[1].sib2().plmn_info_list_r15.get()->resize(1);
    auto& plmn                       = cfg.sibs[1].sib2().plmn_info_list_r15.get()->back();
    plmn.upper_layer_ind_r15_present = true;
  }

  if (generate_sibs() != SRSRAN_SUCCESS) {
    logger.error("Couldn't generate SIBs.");
    return false;
  }
  config_mac();

  // Check valid inactivity timeout config
  uint32_t t310 = cfg.sibs[1].sib2().ue_timers_and_consts.t310.to_number();
  uint32_t t311 = cfg.sibs[1].sib2().ue_timers_and_consts.t311.to_number();
  uint32_t n310 = cfg.sibs[1].sib2().ue_timers_and_consts.n310.to_number();
  logger.info("T310 %d, T311 %d, N310 %d", t310, t311, n310);
  if (cfg.inactivity_timeout_ms < t310 + t311 + n310) {
    srsran::console("\nWarning: Inactivity timeout is smaller than the sum of t310, t311 and n310.\n"
                    "This may break the UE's re-establishment procedure.\n");
    logger.warning("Inactivity timeout is smaller than the sum of t310, t311 and n310. This may break the UE's "
                   "re-establishment procedure.");
  }
  logger.info("Inactivity timeout: %d ms", cfg.inactivity_timeout_ms);
  logger.info("Max consecutive MAC KOs: %d", cfg.max_mac_dl_kos);

  pending_paging.reset(new paging_manager(cfg.sibs[1].sib2().rr_cfg_common.pcch_cfg.default_paging_cycle.to_number(),
                                          cfg.sibs[1].sib2().rr_cfg_common.pcch_cfg.nb.to_number()));

  running = true;

  if (logger.debug.enabled()) {
    asn1::json_writer js{};
    cfg.srb1_cfg.rlc_cfg.to_json(js);
    logger.debug("SRB1 configuration: %s", js.to_string().c_str());
    js = {};
    cfg.srb2_cfg.rlc_cfg.to_json(js);
    logger.debug("SRB2 configuration: %s", js.to_string().c_str());
  }
  return SRSRAN_SUCCESS;
}

void rrc::stop()
{
  if (running) {
    running   = false;
    rrc_pdu p = {0, LCID_EXIT, false, nullptr};
    rx_pdu_queue.push_blocking(std::move(p));
  }
  users.clear();
}

/*******************************************************************************
  Public functions
*******************************************************************************/

void rrc::get_metrics(rrc_metrics_t& m)
{
  if (running) {
    m.ues.resize(users.size());
    size_t count = 0;
    for (auto& ue : users) {
      ue.second->get_metrics(m.ues[count++]);
    }
  }
}

/*******************************************************************************
  MAC interface

  Those functions that shall be called from a phch_worker should push the command
  to the queue and process later
*******************************************************************************/

uint8_t* rrc::read_pdu_bcch_dlsch(const uint8_t cc_idx, const uint32_t sib_index)
{
  srsran::rwlock_read_guard lock(cell_common_list_rwlock);
  if (sib_index < ASN1_RRC_MAX_SIB && cc_idx < cell_common_list->nof_cells()) {
    return cell_common_list->get_cc_idx(cc_idx)->sib_buffer.at(sib_index)->msg;
  }
  return nullptr;
}

void rrc::set_radiolink_dl_state(uint16_t rnti, bool crc_res)
{
  // embed parameters in arg value
  rrc_pdu p = {rnti, LCID_RADLINK_DL, crc_res, nullptr};

  if (not rx_pdu_queue.try_push(std::move(p))) {
    logger.error("Failed to push radio link DL state");
  }
}

void rrc::set_radiolink_ul_state(uint16_t rnti, bool crc_res)
{
  // embed parameters in arg value
  rrc_pdu p = {rnti, LCID_RADLINK_UL, crc_res, nullptr};

  if (not rx_pdu_queue.try_push(std::move(p))) {
    logger.error("Failed to push radio link UL state");
  }
}

void rrc::set_activity_user(uint16_t rnti)
{
  rrc_pdu p = {rnti, LCID_ACT_USER, false, nullptr};

  if (not rx_pdu_queue.try_push(std::move(p))) {
    logger.error("Failed to push UE activity command to RRC queue");
  }
}

void rrc::rem_user_thread(uint16_t rnti)
{
  rrc_pdu p = {rnti, LCID_REM_USER, false, nullptr};
  if (not rx_pdu_queue.try_push(std::move(p))) {
    logger.error("Failed to push UE remove command to RRC queue");
  }
}

uint32_t rrc::get_nof_users()
{
  return users.size();
}

void rrc::max_retx_attempted(uint16_t rnti)
{
  rrc_pdu p = {rnti, LCID_RLC_RTX, false, nullptr};
  if (not rx_pdu_queue.try_push(std::move(p))) {
    logger.error("Failed to push max Retx event to RRC queue");
  }
}

void rrc::protocol_failure(uint16_t rnti)
{
  rrc_pdu p = {rnti, LCID_PROT_FAIL, false, nullptr};
  if (not rx_pdu_queue.try_push(std::move(p))) {
    logger.error("Failed to push protocol failure to RRC queue");
  }
}

// This function is called from PRACH worker (can wait)
int rrc::add_user(uint16_t rnti, const sched_interface::ue_cfg_t& sched_ue_cfg)
{
  auto user_it = users.find(rnti);
  if (user_it == users.end()) {
    if (rnti != SRSRAN_MRNTI) {
      // only non-eMBMS RNTIs are present in user map
      unique_rnti_ptr<ue> u = make_rnti_obj<ue>(rnti, this, rnti, sched_ue_cfg);
      if (u->init() != SRSRAN_SUCCESS) {
        logger.error("Adding user rnti=0x%x - Failed to allocate user resources", rnti);
        return SRSRAN_ERROR;
      }
      users.insert(std::make_pair(rnti, std::move(u)));
    }
    rlc->add_user(rnti);
    pdcp->add_user(rnti);
    logger.info("Added new user rnti=0x%x", rnti);
    if (rnti == SRSRAN_MRNTI) {
      for (auto& mbms_item : mcch.msg.c1().mbsfn_area_cfg_r9().pmch_info_list_r9[0].mbms_session_info_list_r9) {
        uint32_t lcid    = mbms_item.lc_ch_id_r9;
        uint32_t addr_in = 0;
        // adding UE object to MAC for MRNTI without scheduling configuration (broadcast not part of regular scheduling)
        rlc->add_bearer_mrb(SRSRAN_MRNTI, lcid);
        /* eps_bearer_id must be distinct per MBMS session (mirrors lcid, same as the
         * gtpu->add_bearer() call two lines below) -- previously hardcoded to 1 for
         * every session, so gtpu_pdcp_adapter::write_sdu()'s bearers->get_radio_bearer(
         * MRNTI, eps_bearer_id) lookup collided across sessions: the last-registered
         * session's lcid silently overwrote every earlier one's mapping under the same
         * key, and M1-U content for any session but the last was dropped with "Can't
         * deliver SDU for EPS bearer N" -- found while wiring up per-session M1-U TEID
         * demux, latent since multi-session MBMS support was added, undetected until
         * now because every real test only ever exercised a single session. */
        bearer_manager.add_eps_bearer(SRSRAN_MRNTI, static_cast<uint8_t>(lcid), srsran::srsran_rat_t::lte, lcid);
        pdcp->add_bearer(SRSRAN_MRNTI, lcid, srsran::make_drb_pdcp_config_t(1, false));
        gtpu->add_bearer(SRSRAN_MRNTI, lcid, 1, 1, addr_in);
      }
    }
  } else {
    logger.error("Adding user rnti=0x%x (already exists)", rnti);
  }
  return SRSRAN_SUCCESS;
}

/* Function called by MAC after the reception of a C-RNTI CE indicating that the UE still has a
 * valid RNTI.
 */
void rrc::upd_user(uint16_t new_rnti, uint16_t old_rnti)
{
  // Remove new_rnti
  auto new_ue_it = users.find(new_rnti);
  if (new_ue_it != users.end()) {
    new_ue_it->second->deactivate_bearers();
    rem_user_thread(new_rnti);
  }

  // Send Reconfiguration to old_rnti if is RRC_CONNECT or RRC Release if already released here
  auto old_it = users.find(old_rnti);
  if (old_it == users.end()) {
    logger.info("rnti=0x%x received MAC CRNTI CE: 0x%x, but old context is unavailable", new_rnti, old_rnti);
    return;
  }
  ue* ue_ptr = old_it->second.get();

  if (ue_ptr->mobility_handler->is_ho_running()) {
    ue_ptr->mobility_handler->trigger(ue::rrc_mobility::user_crnti_upd_ev{old_rnti, new_rnti});
  } else {
    logger.info("Resuming rnti=0x%x RRC connection due to received C-RNTI CE from rnti=0x%x.", old_rnti, new_rnti);
    if (ue_ptr->is_connected()) {
      // Send a new RRC Reconfiguration to overlay previous
      old_it->second->send_connection_reconf();
    }
  }

  // Log event.
  event_logger::get().log_connection_resume(
      ue_ptr->get_cell_list().get_ue_cc_idx(UE_PCELL_CC_IDX)->cell_common->enb_cc_idx, old_rnti, new_rnti);
}

// Note: this method is not part of UE methods, because the UE context may not exist anymore when reject is sent
void rrc::send_rrc_connection_reject(uint16_t rnti)
{
  dl_ccch_msg_s dl_ccch_msg;
  dl_ccch_msg.msg.set_c1().set_rrc_conn_reject().crit_exts.set_c1().set_rrc_conn_reject_r8().wait_time = 10;

  // Allocate a new PDU buffer, pack the message and send to PDCP
  srsran::unique_byte_buffer_t pdu = srsran::make_byte_buffer();
  if (pdu == nullptr) {
    logger.error("Allocating pdu");
    return;
  }
  asn1::bit_ref bref(pdu->msg, pdu->get_tailroom());
  if (dl_ccch_msg.pack(bref) != asn1::SRSASN_SUCCESS) {
    logger.error(pdu->msg, bref.distance_bytes(), "Failed to pack DL-CCCH-Msg:");
    return;
  }
  pdu->N_bytes = bref.distance_bytes();

  log_rrc_message(Tx, rnti, srb_to_lcid(lte_srb::srb0), *pdu, dl_ccch_msg, dl_ccch_msg.msg.c1().type().to_string());

  rlc->write_sdu(rnti, srb_to_lcid(lte_srb::srb0), std::move(pdu));
}

/*******************************************************************************
  PDCP interface
*******************************************************************************/
void rrc::write_pdu(uint16_t rnti, uint32_t lcid, srsran::unique_byte_buffer_t pdu)
{
  rrc_pdu p = {rnti, lcid, false, std::move(pdu)};
  if (not rx_pdu_queue.try_push(std::move(p))) {
    logger.error("Failed to push Release command to RRC queue");
  }
}

void rrc::notify_pdcp_integrity_error(uint16_t rnti, uint32_t lcid)
{
  logger.warning("Received integrity protection failure indication, rnti=0x%x, lcid=%u", rnti, lcid);
  s1ap->user_release(rnti, asn1::s1ap::cause_radio_network_opts::unspecified);
}

/*******************************************************************************
  S1AP interface
*******************************************************************************/
void rrc::write_dl_info(uint16_t rnti, srsran::unique_byte_buffer_t sdu)
{
  dl_dcch_msg_s dl_dcch_msg;
  dl_dcch_msg.msg.set_c1();
  dl_dcch_msg_type_c::c1_c_* msg_c1 = &dl_dcch_msg.msg.c1();

  auto user_it = users.find(rnti);
  if (user_it != users.end()) {
    dl_info_transfer_r8_ies_s* dl_info_r8 =
        &msg_c1->set_dl_info_transfer().crit_exts.set_c1().set_dl_info_transfer_r8();
    //    msg_c1->dl_info_transfer().rrc_transaction_id = ;
    dl_info_r8->non_crit_ext_present = false;
    dl_info_r8->ded_info_type.set_ded_info_nas();
    dl_info_r8->ded_info_type.ded_info_nas().resize(sdu->N_bytes);
    memcpy(msg_c1->dl_info_transfer().crit_exts.c1().dl_info_transfer_r8().ded_info_type.ded_info_nas().data(),
           sdu->msg,
           sdu->N_bytes);

    sdu->clear();

    user_it->second->send_dl_dcch(&dl_dcch_msg, std::move(sdu));
  } else {
    logger.error("Rx SDU for unknown rnti=0x%x", rnti);
  }
}

void rrc::release_ue(uint16_t rnti)
{
  rrc_pdu p = {rnti, LCID_REL_USER, false, nullptr};
  if (not rx_pdu_queue.try_push(std::move(p))) {
    logger.error("Failed to push Release command to RRC queue");
  }
}

bool rrc::setup_ue_ctxt(uint16_t rnti, const asn1::s1ap::init_context_setup_request_s& msg)
{
  logger.info("Adding initial context for 0x%x", rnti);
  auto user_it = users.find(rnti);
  if (user_it == users.end()) {
    logger.warning("Unrecognised rnti: 0x%x", rnti);
    return false;
  }

  user_it->second->handle_ue_init_ctxt_setup_req(msg);
  return true;
}

bool rrc::modify_ue_ctxt(uint16_t rnti, const asn1::s1ap::ue_context_mod_request_s& msg)
{
  logger.info("Modifying context for 0x%x", rnti);
  auto user_it = users.find(rnti);

  if (user_it == users.end()) {
    logger.warning("Unrecognised rnti: 0x%x", rnti);
    return false;
  }

  return user_it->second->handle_ue_ctxt_mod_req(msg);
}

bool rrc::release_erabs(uint32_t rnti)
{
  logger.info("Releasing E-RABs for 0x%x", rnti);
  auto user_it = users.find(rnti);

  if (user_it == users.end()) {
    logger.warning("Unrecognised rnti: 0x%x", rnti);
    return false;
  }

  bool ret = user_it->second->release_erabs();
  return ret;
}

int rrc::release_erab(uint16_t rnti, uint16_t erab_id)
{
  logger.info("Releasing E-RAB id=%d for 0x%x", erab_id, rnti);
  auto user_it = users.find(rnti);

  if (user_it == users.end()) {
    logger.warning("Unrecognised rnti: 0x%x", rnti);
    return SRSRAN_ERROR;
  }

  return user_it->second->release_erab(erab_id);
}

int rrc::notify_ue_erab_updates(uint16_t rnti, srsran::const_byte_span nas_pdu)
{
  auto user_it = users.find(rnti);
  if (user_it == users.end()) {
    logger.warning("Unrecognised rnti: 0x%x", rnti);
    return SRSRAN_ERROR;
  }
  user_it->second->send_connection_reconf(nullptr, false, nas_pdu);
  return SRSRAN_SUCCESS;
}

bool rrc::has_erab(uint16_t rnti, uint32_t erab_id) const
{
  auto user_it = users.find(rnti);
  if (user_it == users.end()) {
    logger.warning("Unrecognised rnti: 0x%x", rnti);
    return false;
  }
  return user_it->second->has_erab(erab_id);
}

int rrc::get_erab_addr_in(uint16_t rnti, uint16_t erab_id, transp_addr_t& addr_in, uint32_t& teid_in) const
{
  auto user_it = users.find(rnti);
  if (user_it == users.end()) {
    logger.warning("Unrecognised rnti: 0x%x", rnti);
    return SRSRAN_ERROR;
  }
  return user_it->second->get_erab_addr_in(erab_id, addr_in, teid_in);
}

void rrc::set_aggregate_max_bitrate(uint16_t rnti, const asn1::s1ap::ue_aggregate_maximum_bitrate_s& bitrate)
{
  auto user_it = users.find(rnti);
  if (user_it == users.end()) {
    logger.warning("Unrecognised rnti: 0x%x", rnti);
    return;
  }
  user_it->second->set_bitrates(bitrate);
}

int rrc::setup_erab(uint16_t                                           rnti,
                    uint16_t                                           erab_id,
                    const asn1::s1ap::erab_level_qos_params_s&         qos_params,
                    srsran::const_span<uint8_t>                        nas_pdu,
                    const asn1::bounded_bitstring<1, 160, true, true>& addr,
                    uint32_t                                           gtpu_teid_out,
                    asn1::s1ap::cause_c&                               cause)
{
  logger.info("Setting up erab id=%d for 0x%x", erab_id, rnti);
  auto user_it = users.find(rnti);
  if (user_it == users.end()) {
    logger.warning("Unrecognised rnti: 0x%x", rnti);
    cause.set_radio_network().value = asn1::s1ap::cause_radio_network_opts::unknown_erab_id;
    return SRSRAN_ERROR;
  }
  return user_it->second->setup_erab(erab_id, qos_params, nas_pdu, addr, gtpu_teid_out, cause);
}

int rrc::modify_erab(uint16_t                                   rnti,
                     uint16_t                                   erab_id,
                     const asn1::s1ap::erab_level_qos_params_s& qos_params,
                     srsran::const_span<uint8_t>                nas_pdu,
                     asn1::s1ap::cause_c&                       cause)
{
  logger.info("Modifying E-RAB for 0x%x. E-RAB Id %d", rnti, erab_id);
  auto user_it = users.find(rnti);
  if (user_it == users.end()) {
    logger.warning("Unrecognised rnti: 0x%x", rnti);
    cause.set_radio_network().value = asn1::s1ap::cause_radio_network_opts::unknown_erab_id;
    return SRSRAN_ERROR;
  }

  return user_it->second->modify_erab(erab_id, qos_params, nas_pdu, cause);
}

/*******************************************************************************
  Paging functions
  These functions use a different mutex because access different shared variables
  than user map
*******************************************************************************/

void rrc::add_paging_id(uint32_t ueid, const asn1::s1ap::ue_paging_id_c& ue_paging_id)
{
  if (ue_paging_id.type().value == asn1::s1ap::ue_paging_id_c::types_opts::imsi) {
    pending_paging->add_imsi_paging(ueid, ue_paging_id.imsi());
  } else {
    pending_paging->add_tmsi_paging(ueid, ue_paging_id.s_tmsi().mmec[0], ue_paging_id.s_tmsi().m_tmsi);
  }
}

bool rrc::is_paging_opportunity(uint32_t tti, uint32_t* payload_len)
{
  if (etws_paging_active_.load() && etws_paging_count_.load() > 0) {
    *payload_len = 4; // minimal PCCH PDU with etws_ind
    return true;
  }
  *payload_len = pending_paging->pending_pcch_bytes(tti_point(tti));
  return *payload_len > 0;
}

void rrc::read_pdu_pcch(uint32_t tti_tx_dl, uint8_t* payload, uint32_t buffer_size)
{
  if (etws_paging_active_.load() && etws_paging_count_.load() > 0) {
    etws_paging_count_.fetch_sub(1);
    pcch_msg_s pcch;
    paging_s&  pg       = pcch.msg.set_c1().paging();
    pg.etws_ind_present = true;
    srsran::byte_buffer_t buf;
    asn1::bit_ref         bref(buf.msg, buf.get_tailroom());
    if (pcch.pack(bref) == asn1::SRSASN_SUCCESS) {
      buf.N_bytes = bref.distance_bytes();
      if (buf.N_bytes <= buffer_size) {
        std::copy(buf.msg, buf.msg + buf.N_bytes, payload);
        logger.warning("ETWS paging broadcast, %u occasions remaining",
                       (unsigned)etws_paging_count_.load());
      }
    }
    return;
  }

  auto read_func = [this, payload, buffer_size](srsran::const_byte_span pdu, const pcch_msg_s& msg, bool first_tx) {
    // copy PCCH pdu to buffer
    if (pdu.size() > buffer_size) {
      logger.warning("byte buffer with size=%zd is too small to fit pcch msg with size=%zd", buffer_size, pdu.size());
      return false;
    }
    std::copy(pdu.begin(), pdu.end(), payload);

    if (first_tx) {
      logger.info("Assembling PCCH payload with %d UE identities, payload_len=%d bytes",
                  msg.msg.c1().paging().paging_record_list.size(),
                  pdu.size());
      log_broadcast_rrc_message(SRSRAN_PRNTI, pdu, msg, msg.msg.c1().type().to_string());
    }
    return true;
  };

  pending_paging->read_pdu_pcch(tti_point(tti_tx_dl), read_func);
}

/*******************************************************************************
  Handover functions
*******************************************************************************/

void rrc::ho_preparation_complete(uint16_t                     rnti,
                                  ho_prep_result               result,
                                  const asn1::s1ap::ho_cmd_s&  msg,
                                  srsran::unique_byte_buffer_t rrc_container)
{
  users.at(rnti)->mobility_handler->handle_ho_preparation_complete(result, msg, std::move(rrc_container));
}

void rrc::set_erab_status(uint16_t rnti, const asn1::s1ap::bearers_subject_to_status_transfer_list_l& erabs)
{
  auto ue_it = users.find(rnti);
  if (ue_it == users.end()) {
    logger.warning("rnti=0x%x does not exist", rnti);
    return;
  }
  ue_it->second->mobility_handler->trigger(erabs);
}

/*******************************************************************************
  EN-DC/NSA helper functions
*******************************************************************************/

void rrc::sgnb_addition_ack(uint16_t eutra_rnti, sgnb_addition_ack_params_t params)
{
  logger.info("Received SgNB addition acknowledgement for rnti=0x%x", eutra_rnti);
  auto ue_it = users.find(eutra_rnti);
  if (ue_it == users.end()) {
    logger.warning("rnti=0x%x does not exist", eutra_rnti);
    return;
  }
  ue_it->second->endc_handler->trigger(ue::rrc_endc::sgnb_add_req_ack_ev{params});

  // trigger RRC Reconfiguration to send NR config to UE
  ue_it->second->send_connection_reconf();
}

void rrc::sgnb_addition_reject(uint16_t eutra_rnti)
{
  logger.error("Received SgNB addition reject for rnti=%d", eutra_rnti);
  auto ue_it = users.find(eutra_rnti);
  if (ue_it == users.end()) {
    logger.warning("rnti=0x%x does not exist", eutra_rnti);
    return;
  }
  ue_it->second->endc_handler->trigger(ue::rrc_endc::sgnb_add_req_reject_ev{});
}

void rrc::sgnb_addition_complete(uint16_t eutra_rnti, uint16_t nr_rnti)
{
  logger.info("User rnti=0x%x successfully enabled EN-DC", eutra_rnti);
  auto ue_it = users.find(eutra_rnti);
  if (ue_it == users.end()) {
    logger.warning("rnti=0x%x does not exist", eutra_rnti);
    return;
  }
  ue_it->second->endc_handler->trigger(ue::rrc_endc::sgnb_add_complete_ev{nr_rnti});
}

void rrc::sgnb_inactivity_timeout(uint16_t eutra_rnti)
{
  logger.info("Received NR inactivity timeout for rnti=0x%x - releasing UE", eutra_rnti);
  auto ue_it = users.find(eutra_rnti);
  if (ue_it == users.end()) {
    logger.warning("rnti=0x%x does not exist", eutra_rnti);
    return;
  }
  s1ap->user_release(eutra_rnti, asn1::s1ap::cause_radio_network_opts::user_inactivity);
}

void rrc::sgnb_release_ack(uint16_t eutra_rnti)
{
  auto ue_it = users.find(eutra_rnti);
  if (ue_it != users.end()) {
    logger.info("Received SgNB release acknowledgement for rnti=0x%x", eutra_rnti);
    ue_it->second->endc_handler->trigger(ue::rrc_endc::sgnb_rel_req_ack_ev{});
  } else {
    // The EUTRA does not need to wait for Release Ack in case it wants to destroy the EUTRA UE
    logger.info("Received SgNB release acknowledgement for already released rnti=0x%x", eutra_rnti);
  }
}

/*******************************************************************************
  Private functions
  All private functions are not mutexed and must be called from a mutexed environment
  from either a public function or the internal thread
*******************************************************************************/

void rrc::parse_ul_ccch(ue& ue, srsran::unique_byte_buffer_t pdu)
{
  srsran_assert(pdu != nullptr, "parse_ul_ccch called for empty message");

  ul_ccch_msg_s  ul_ccch_msg;
  asn1::cbit_ref bref(pdu->msg, pdu->N_bytes);
  if (ul_ccch_msg.unpack(bref) != asn1::SRSASN_SUCCESS or
      ul_ccch_msg.msg.type().value != ul_ccch_msg_type_c::types_opts::c1) {
    log_rx_pdu_fail(ue.rnti, srb_to_lcid(lte_srb::srb0), *pdu, "Failed to unpack UL-CCCH message");
    return;
  }

  // Log Rx message
  log_rrc_message(
      Rx, ue.rnti, srsran::srb_to_lcid(lte_srb::srb0), *pdu, ul_ccch_msg, ul_ccch_msg.msg.c1().type().to_string());

  switch (ul_ccch_msg.msg.c1().type().value) {
    case ul_ccch_msg_type_c::c1_c_::types::rrc_conn_request:
      ue.save_ul_message(std::move(pdu));
      ue.handle_rrc_con_req(&ul_ccch_msg.msg.c1().rrc_conn_request());
      break;
    case ul_ccch_msg_type_c::c1_c_::types::rrc_conn_reest_request:
      ue.save_ul_message(std::move(pdu));
      ue.handle_rrc_con_reest_req(&ul_ccch_msg.msg.c1().rrc_conn_reest_request());
      break;
    default:
      logger.error("Processing UL-CCCH for rnti=0x%x - Unsupported message type %s",
                   ul_ccch_msg.msg.c1().type().to_string());
      break;
  }
}

///< User mutex must be hold by caller
void rrc::parse_ul_dcch(ue& ue, uint32_t lcid, srsran::unique_byte_buffer_t pdu)
{
  srsran_assert(pdu != nullptr, "parse_ul_dcch called for empty message");

  ue.parse_ul_dcch(lcid, std::move(pdu));
}

///< User mutex must be hold by caller
void rrc::process_release_complete(uint16_t rnti)
{
  logger.info("Received Release Complete rnti=0x%x", rnti);
  auto user_it = users.find(rnti);
  if (user_it == users.end()) {
    logger.error("Received ReleaseComplete for unknown rnti=0x%x", rnti);
    return;
  }
  ue* u = user_it->second.get();

  if (u->is_idle() or u->mobility_handler->is_ho_running()) {
    rem_user_thread(rnti);
  } else if (not u->is_idle()) {
    rlc->clear_buffer(rnti);
    user_it->second->send_connection_release();
    // delay user deletion for ~50 TTI (until RRC release is sent)
    task_sched.defer_callback(50, [this, rnti]() { rem_user_thread(rnti); });
  }
}

void rrc::rem_user(uint16_t rnti)
{
  auto user_it = users.find(rnti);
  if (user_it != users.end()) {
    // First remove MAC and GTPU to stop processing DL/UL traffic for this user
    mac->ue_rem(rnti); // MAC handles PHY
    gtpu->rem_user(rnti);

    // Now remove RLC and PDCP
    bearer_manager.rem_user(rnti);
    rlc->rem_user(rnti);
    pdcp->rem_user(rnti);

    users.erase(rnti);

    srsran::console("Disconnecting rnti=0x%x.\n", rnti);
    logger.info("Removed user rnti=0x%x", rnti);
  } else {
    logger.error("Removing user rnti=0x%x (does not exist)", rnti);
  }
}

void rrc::fill_sib_lens(uint32_t ccidx, sched_interface::cell_cfg_sib_t (&sibs)[sched_interface::MAX_SIBS]) const
{
  srsran::rwlock_read_guard lock(cell_common_list_rwlock);
  for (auto& s : sibs) {
    s = {};
  }
  for (uint32_t i = 0; i < nof_si_messages; i++) {
    sibs[i].len = cell_common_list->get_cc_idx(ccidx)->sib_buffer.at(i)->N_bytes;
    if (i == 0) {
      sibs[i].period_rf = 8; // SIB1 is always 8 rf
    } else {
      sibs[i].period_rf = cfg.sib1.sched_info_list_mbms_r14[i - 1].si_periodicity_r14.to_number();
    }
  }
}

/// Re-pushes just the per-cell SIB length/periodicity table to MAC, e.g. after a post-init
/// generate_sibs() rebuild (cas_muting toggle, SIB12 activation) changes a packed SIB length.
/// Cheap and safe to call often, unlike config_mac()/mac::cell_cfg(), which also recreates
/// the broadcast/RA schedulers and would disrupt already-connected UEs.
void rrc::update_mac_sib_cfg()
{
  for (uint32_t ccidx = 0; ccidx < cfg.cell_list.size(); ++ccidx) {
    sched_interface::cell_cfg_sib_t sibs[sched_interface::MAX_SIBS];
    fill_sib_lens(ccidx, sibs);
    mac->set_sib_lens(ccidx, sibs);
  }
}

void rrc::config_mac()
{
  using sched_cell_t = sched_interface::cell_cfg_t;

  // Fill MAC scheduler configuration for SIBs
  std::vector<sched_cell_t> sched_cfg;
  sched_cfg.resize(cfg.cell_list.size());

  for (uint32_t ccidx = 0; ccidx < cfg.cell_list.size(); ++ccidx) {
    sched_interface::cell_cfg_t& item = sched_cfg[ccidx];

    // set sib/prach cfg
    fill_sib_lens(ccidx, item.sibs);
    item.prach_config        = cfg.sibs[1].sib2().rr_cfg_common.prach_cfg.prach_cfg_info.prach_cfg_idx;
    item.prach_nof_preambles = cfg.sibs[1].sib2().rr_cfg_common.rach_cfg_common.preamb_info.nof_ra_preambs.to_number();
    item.si_window_ms        = cfg.sib1.si_win_len_r14.to_number();
    item.prach_rar_window =
        cfg.sibs[1].sib2().rr_cfg_common.rach_cfg_common.ra_supervision_info.ra_resp_win_size.to_number();
    item.prach_freq_offset    = cfg.sibs[1].sib2().rr_cfg_common.prach_cfg.prach_cfg_info.prach_freq_offset;
    item.maxharq_msg3tx       = cfg.sibs[1].sib2().rr_cfg_common.rach_cfg_common.max_harq_msg3_tx;
    item.enable_64qam         = cfg.sibs[1].sib2().rr_cfg_common.pusch_cfg_common.pusch_cfg_basic.enable64_qam;
    item.target_pucch_ul_sinr = cfg.cell_list[ccidx].target_pucch_sinr_db;
    item.target_pusch_ul_sinr = cfg.cell_list[ccidx].target_pusch_sinr_db;
    item.enable_phr_handling  = cfg.cell_list[ccidx].enable_phr_handling;
    item.min_phr_thres        = cfg.cell_list[ccidx].min_phr_thres;
    item.delta_pucch_shift    = cfg.sibs[1].sib2().rr_cfg_common.pucch_cfg_common.delta_pucch_shift.to_number();
    item.ncs_an               = cfg.sibs[1].sib2().rr_cfg_common.pucch_cfg_common.ncs_an;
    item.n1pucch_an           = cfg.sibs[1].sib2().rr_cfg_common.pucch_cfg_common.n1_pucch_an;
    item.nrb_cqi              = cfg.sibs[1].sib2().rr_cfg_common.pucch_cfg_common.nrb_cqi;

    item.nrb_pucch = SRSRAN_MAX(cfg.sr_cfg.nof_prb, item.nrb_cqi);
    logger.info("Allocating %d PRBs for PUCCH", item.nrb_pucch);

    // Copy base cell configuration
    item.cell    = cfg.cell;
    item.cell.id = cfg.cell_list[ccidx].pci;

    // copy secondary cell list info
    sched_cfg[ccidx].scell_list.reserve(cfg.cell_list[ccidx].scell_list.size());
    for (uint32_t scidx = 0; scidx < cfg.cell_list[ccidx].scell_list.size(); ++scidx) {
      const auto& scellitem = cfg.cell_list[ccidx].scell_list[scidx];
      // search enb_cc_idx specific to cell_id
      auto it = std::find_if(cfg.cell_list.begin(), cfg.cell_list.end(), [&scellitem](const cell_cfg_t& e) {
        return e.cell_id == scellitem.cell_id;
      });
      if (it == cfg.cell_list.end()) {
        logger.warning("Secondary cell 0x%x not configured", scellitem.cell_id);
        continue;
      }
      sched_interface::cell_cfg_t::scell_cfg_t scellcfg;
      scellcfg.enb_cc_idx               = it - cfg.cell_list.begin();
      scellcfg.ul_allowed               = scellitem.ul_allowed;
      scellcfg.cross_carrier_scheduling = scellitem.cross_carrier_sched;
      sched_cfg[ccidx].scell_list.push_back(scellcfg);
    }
  }

  // Configure MAC scheduler
  mac->cell_cfg(sched_cfg);
}

/* This methods packs the SIBs for each component carrier and stores them
 * inside the sib_buffer, a vector of SIBs for each CC.
 *
 * Before packing the message, it patches the cell specific params of
 * the SIB, including the cellId and the PRACH config index.
 *
 * The number of generates SIB messages is stored in the class member nof_si_messages
 *
 * @return SRSRAN_SUCCESS on success, SRSRAN_ERROR on failure
 */
uint32_t rrc::generate_sibs()
{
  // nof_messages: SIB1-MBMS plus one SI message per sched_info entry
  uint32_t                    nof_messages = 1 + cfg.sib1.sched_info_list_mbms_r14.size();
  sched_info_list_mbms_r14_l& sched_info   = cfg.sib1.sched_info_list_mbms_r14;

  // Build the new cell ctxt list off to the side (not the live cell_common_list member) so
  // concurrent PHY-worker reads of the old list (via read_pdu_bcch_dlsch()) keep seeing a fully
  // populated, consistent list until the new one is completely built and ready to publish.
  std::shared_ptr<enb_cell_common_list> new_cell_list(new enb_cell_common_list{cfg});

  // generate and pack into SIB buffers
  for (uint32_t cc_idx = 0; cc_idx < cfg.cell_list.size(); cc_idx++) {
    enb_cell_common* cell_ctxt = new_cell_list->get_cc_idx(cc_idx);
    // msg is array of SI messages, each SI message msg[i] may contain multiple SIBs
    // all SIBs in a SI message msg[i] share the same periodicity
    asn1::dyn_array<bcch_dl_sch_msg_mbms_s> msg(nof_messages);

    // Copy SIB1 to first SI message
    msg[0].msg.set_c1().set_sib_type1_mbms_r14() = cell_ctxt->sib1;

    // Copy rest of SIBs
    for (uint32_t sched_info_elem = 0; sched_info_elem < nof_messages - 1; sched_info_elem++) {
      uint32_t msg_index = sched_info_elem + 1; // first msg is SIB1, therefore start with second

      msg[msg_index].msg.set_c1().set_sys_info_mbms_r14().crit_exts.set_sys_info_r8();
      sys_info_r8_ies_s::sib_type_and_info_l_& sib_list =
          msg[msg_index].msg.c1().sys_info_mbms_r14().crit_exts.sys_info_r8().sib_type_and_info;

      // SIB2 always in second SI message
      if (msg_index == 1) {
        sib_info_item_c sibitem;
        sibitem.set_sib2() = cell_ctxt->sib2;
        sib_list.push_back(sibitem);
      }

      // Add other SIBs: sib_type_mbms_r14_e::to_number() returns actual SIB number (10,11,12,13...)
      for (uint32_t j = 0; j < sched_info[sched_info_elem].sib_map_info_r14.size(); j++) {
        uint32_t sib_idx = sched_info[sched_info_elem].sib_map_info_r14[j].to_number() - 1;
        if (sib_idx < ASN1_RRC_MAX_SIB) {
          sib_list.push_back(cfg.sibs[sib_idx]);
        }
      }
    }

    // Pack payload for all messages
    for (uint32_t msg_index = 0; msg_index < nof_messages; msg_index++) {
      srsran::unique_byte_buffer_t sib_buffer = srsran::make_byte_buffer();
      if (sib_buffer == nullptr) {
        logger.error("Couldn't allocate PDU in %s().", __FUNCTION__);
        return SRSRAN_ERROR;
      }
      asn1::bit_ref bref(sib_buffer->msg, sib_buffer->get_tailroom());
      if (msg[msg_index].pack(bref) != asn1::SRSASN_SUCCESS) {
        logger.error("Failed to pack SIB message %d", msg_index);
        return SRSRAN_ERROR;
      }
      sib_buffer->N_bytes = bref.distance_bytes();
      cell_ctxt->sib_buffer.push_back(std::move(sib_buffer));

      // Log SIBs in JSON format
      fmt::memory_buffer membuf;
      const char*        msg_str = msg[msg_index].msg.c1().type().to_string();
      if (msg[msg_index].msg.c1().type().value != asn1::rrc::bcch_dl_sch_msg_type_mbms_r14_c::c1_c_::types_opts::sib_type1_mbms_r14) {
        msg_str = msg[msg_index].msg.c1().sys_info_mbms_r14().crit_exts.type().to_string();
      }
      fmt::format_to(membuf, "{}, cc={}, idx={}", msg_str, cc_idx, msg_index);
      log_broadcast_rrc_message(SRSRAN_SIRNTI_MBMS_DEDICATED, *cell_ctxt->sib_buffer.back(), msg[msg_index], srsran::to_c_str(membuf));
    }

    if (cfg.sibs[6].type() == asn1::rrc::sys_info_r8_ies_s::sib_type_and_info_item_c_::types::sib7) {
      sib7 = cfg.sibs[6].sib7();
    }
  }

  // Publish the fully-built list under the write lock. A mid-build failure above returns before
  // reaching here, leaving the live cell_common_list (and thus the last-known-good SIBs) untouched.
  {
    srsran::rwlock_write_guard lock(cell_common_list_rwlock);
    cell_common_list = std::move(new_cell_list);
  }
  nof_si_messages = nof_messages;

  return SRSRAN_SUCCESS;
}

void rrc::reconfigure_embms(uint8_t            pmch_bandwidth,
                            uint16_t           mcs,
                            uint8_t            time_interleaving_n,
                            uint8_t            time_interleaving_m,
                            uint8_t            time_interleaving_n_last_mtch,
                            uint8_t            time_interleaving_m_last_mtch,
                            uint8_t            cyclic_shift_alpha,
                            bool               freq_interleaving,
                            bool               use_mcs_table2,
                            bool               cas_muting,
                            uint8_t            k_cas,
                            uint8_t            n_cas,
                            uint8_t            mch_sched_period_rf,
                            uint8_t            nof_mbms_sessions,
                            bool               time_separation_sl2,
                            const std::string& subcarrier_spacing)
{
  /* reload_embms_config()'s SIGHUP path re-parses embms.* from the config file
   * directly into this call, bypassing the startup-only range checks in
   * enb_cfg_parser.cc's set_derived_args()/parse_cell_cfg() — re-apply the
   * same legal-value checks here so a bad live edit can't reach OTA SIB13/MCCH
   * signaling or PHY buffer sizing unvalidated. */
  if (pmch_bandwidth != 0 && pmch_bandwidth != 25 && pmch_bandwidth != 30 && pmch_bandwidth != 35 &&
      pmch_bandwidth != 40) {
    logger.warning("reconfigure_embms: pmch_bandwidth=%u is not valid (must be 0, 25, 30, 35, or 40) — setting to 0",
                   pmch_bandwidth);
    pmch_bandwidth = 0;
  }
  if (pmch_bandwidth > cfg.cell.nof_prb) {
    logger.warning("reconfigure_embms: pmch_bandwidth=%u exceeds cell nof_prb=%u — clamping to %u",
                   pmch_bandwidth, cfg.cell.nof_prb, cfg.cell.nof_prb);
    pmch_bandwidth = (uint8_t)cfg.cell.nof_prb;
  }
  if (time_interleaving_n > 1) {
    static const uint8_t valid_n[] = {2, 4, 8, 16};
    bool                 n_ok      = false;
    for (uint8_t v : valid_n) {
      if (time_interleaving_n == v) {
        n_ok = true;
        break;
      }
    }
    if (!n_ok) {
      logger.warning("reconfigure_embms: time_interleaving_n=%u is not valid (must be 2, 4, 8, or 16) — disabling "
                     "time interleaving",
                     time_interleaving_n);
      time_interleaving_n = 0;
      time_interleaving_m = 0;
    } else {
      /* TS 36.300 §15.3.3: "MTCH and MCCH can be multiplexed on the same MCH (if
       * time interleaving is not configured)" - i.e. a time-interleaved MCH must
       * not also carry MCCH. This function always configures pmch_info_list[0]
       * (see below), which is always the PMCH that immediately follows MCCH
       * (phy_common.cc: pmch_start = (i==0) ? 1 : ...), so enabling time
       * interleaving here always creates that conflict. */
      logger.warning("reconfigure_embms: time_interleaving_n=%u enables time interleaving on pmch_info_list[0], "
                     "which also carries MCCH — TS 36.300 §15.3.3 requires a time-interleaved MCH not carry MCCH; "
                     "a spec-compliant UE may not correctly receive this configuration",
                     time_interleaving_n);
    }
  }
  if (time_interleaving_n > 1 && time_interleaving_m > 0) {
    static const uint8_t valid_m[] = {4, 8, 16, 32};
    bool                 m_ok      = false;
    for (uint8_t v : valid_m) {
      if (time_interleaving_m == v) {
        m_ok = true;
        break;
      }
    }
    if (!m_ok) {
      uint8_t fallback_m = (time_interleaving_n < 4) ? 4u : time_interleaving_n;
      logger.warning("reconfigure_embms: time_interleaving_m=%u is not valid (must be 4, 8, 16, or 32) — setting to %u",
                     time_interleaving_m, fallback_m);
      time_interleaving_m = fallback_m;
    }
  }
  if (time_interleaving_n > 1 && time_interleaving_m < time_interleaving_n) {
    logger.warning("reconfigure_embms: time_interleaving_m (%d) < time_interleaving_n (%d), clamping M to N",
                   time_interleaving_m, time_interleaving_n);
    time_interleaving_m = time_interleaving_n;
  }
  /* pmch-TimeInterleavingN/M-LastMTCH-r19 (TS 36.331 CR5168r3): lets the last of
   * nof_mbms_sessions MTCH sessions on a time-interleaved MCH use different (or,
   * via N-last=1/n1, no) time interleaving. N-last accepts 1 in addition to the
   * main field's {2,4,8,16} — 1 is the one value the main N field cannot express
   * (there, 0/1 both mean "disabled"; here 1 has its own distinct meaning: "this
   * specific session opts out while the others stay interleaved"). */
  if (time_interleaving_n_last_mtch > 0) {
    static const uint8_t valid_n_last[] = {1, 2, 4, 8, 16};
    bool                 n_last_ok      = false;
    for (uint8_t v : valid_n_last) {
      if (time_interleaving_n_last_mtch == v) {
        n_last_ok = true;
        break;
      }
    }
    if (!n_last_ok) {
      logger.warning("reconfigure_embms: time_interleaving_n_last_mtch=%u is not valid (must be 1, 2, 4, 8, or 16) "
                     "— disabling LastMTCH time interleaving override",
                     time_interleaving_n_last_mtch);
      time_interleaving_n_last_mtch = 0;
      time_interleaving_m_last_mtch = 0;
    } else if (time_interleaving_n <= 1) {
      logger.warning("reconfigure_embms: time_interleaving_n_last_mtch=%u set but main time_interleaving_n=%u "
                     "(time interleaving disabled) — LastMTCH override has nothing to override; ignoring",
                     time_interleaving_n_last_mtch, time_interleaving_n);
      time_interleaving_n_last_mtch = 0;
      time_interleaving_m_last_mtch = 0;
    } else if (nof_mbms_sessions <= 1) {
      logger.warning("reconfigure_embms: time_interleaving_n_last_mtch=%u set but nof_mbms_sessions=%u — LastMTCH "
                     "only makes sense with 2+ sessions on this PMCH; ignoring",
                     time_interleaving_n_last_mtch, nof_mbms_sessions);
      time_interleaving_n_last_mtch = 0;
      time_interleaving_m_last_mtch = 0;
    }
  }
  if (time_interleaving_n_last_mtch > 1 && time_interleaving_m_last_mtch > 0) {
    static const uint8_t valid_m_last[] = {4, 8, 16, 32};
    bool                 m_last_ok      = false;
    for (uint8_t v : valid_m_last) {
      if (time_interleaving_m_last_mtch == v) {
        m_last_ok = true;
        break;
      }
    }
    if (!m_last_ok) {
      logger.warning("reconfigure_embms: time_interleaving_m_last_mtch=%u is not valid (must be 4, 8, 16, or 32) "
                     "— falling back to the main time_interleaving_m=%u for the last MTCH",
                     time_interleaving_m_last_mtch, time_interleaving_m);
      time_interleaving_m_last_mtch = 0;
    }
  }
  if (time_interleaving_n > 1 && nof_mbms_sessions > 1 && time_interleaving_n_last_mtch == 0) {
    /* TS 36.300 CR1428 (the CSA/MSA paragraph): "the eNB applies MAC
     * multiplexing of different MTCHs and optionally MCCH to be
     * transmitted on this MCH, if time interleaving is not configured" -
     * i.e. a time-interleaved MCH should carry at most one MTCH session.
     * TS 36.331 CR5168r3 (pmch-TimeInterleaving{N,M}-LastMTCH-r19, the later,
     * field-defining CR) presupposes the opposite -- multiple MTCH sessions on
     * one time-interleaved MCH, with the last one individually opted out via
     * this override. Narrowed to only warn when that override is NOT set: once
     * it is, multi-session + time interleaving is the spec-blessed pattern
     * CR5168r3 exists for, not a misconfiguration. */
    logger.warning("reconfigure_embms: time_interleaving_n=%u enables time interleaving with nof_mbms_sessions=%u "
                   "sessions configured and no time_interleaving_n_last_mtch override — TS 36.300 CR1428 expects a "
                   "time-interleaved MCH to carry at most one MTCH session; a spec-compliant UE may not correctly "
                   "receive this configuration. Set time_interleaving_n_last_mtch (TS 36.331 CR5168r3) to legitimize "
                   "multiple sessions.",
                   time_interleaving_n, nof_mbms_sessions);
  }
  if (mch_sched_period_rf > 0) {
    static const uint8_t valid_sp[] = {4, 8, 16, 32, 64};
    bool                 sp_ok      = false;
    for (uint8_t v : valid_sp) {
      if (mch_sched_period_rf == v) {
        sp_ok = true;
        break;
      }
    }
    if (!sp_ok) {
      logger.warning("reconfigure_embms: mch_sched_period_rf=%u is not valid (must be 4, 8, 16, 32, or 64) — ignoring",
                     mch_sched_period_rf);
      mch_sched_period_rf = 0; // 0 means "leave cfg.mch_sched_period_rf unchanged" below
    }
  }
  if (cas_muting) {
    static const uint8_t valid_n_cas[] = {2, 4, 8, 16};
    bool                 n_cas_ok      = false;
    for (uint8_t v : valid_n_cas) {
      if (n_cas == v) {
        n_cas_ok = true;
        break;
      }
    }
    if (!n_cas_ok) {
      logger.warning("reconfigure_embms: n_cas=%u is not valid (must be 2, 4, 8, or 16) — disabling cas_muting",
                     n_cas);
      cas_muting = false;
    } else if (k_cas < 4 || k_cas > 63) {
      logger.warning("reconfigure_embms: k_cas=%u is not valid (must be 4..63) — disabling cas_muting", k_cas);
      cas_muting = false;
    }
  }
  cfg.pmch_bandwidth            = pmch_bandwidth;
  cfg.mbms_mcs                  = mcs;
  cfg.pmch_time_interleaving_n  = time_interleaving_n;
  cfg.pmch_time_interleaving_m  = time_interleaving_m;
  cfg.pmch_time_interleaving_n_last_mtch = time_interleaving_n_last_mtch;
  cfg.pmch_time_interleaving_m_last_mtch = time_interleaving_m_last_mtch;
  cfg.pmch_cyclic_shift_alpha   = cyclic_shift_alpha;
  cfg.pmch_freq_interleaving    = freq_interleaving;
  cfg.pmch_use_mcs_table2       = use_mcs_table2;
  cfg.cell.cas_muting = cas_muting;
  cfg.cell.k_cas      = k_cas;
  cfg.cell.n_cas      = n_cas;
  if (mch_sched_period_rf > 0) {
    cfg.mch_sched_period_rf = mch_sched_period_rf;
  }
  if (nof_mbms_sessions > 0 && nof_mbms_sessions <= 8u) {
    cfg.nof_mbms_sessions = nof_mbms_sessions;
  }
  cfg.pmch_time_separation_sl2 = time_separation_sl2;
  cfg.pmch_subcarrier_spacing  = subcarrier_spacing;
  configure_mbsfn_sibs();
}

void rrc::reload_sib12(bool activate)
{
  if (activate) {
    if (cfg.sib12_alert_file.empty()) {
      logger.error("reload_sib12: sib12_alert_file not configured");
      return;
    }
    sib_type12_r9_s sib12_data = {};
    if (sib_sections::parse_sib12(cfg.sib12_alert_file, &sib12_data) != SRSRAN_SUCCESS) {
      logger.error("reload_sib12: failed to parse %s", cfg.sib12_alert_file.c_str());
      return;
    }
    install_sib12(sib12_data);
  } else {
    clear_sib12();
  }
}

void rrc::install_sib12(const asn1::rrc::sib_type12_r9_s& sib12_data)
{
  cfg.sibs[11].set_sib12_v920() = sib12_data;

  // Add SIB12 to sched_info if not already present
  bool found = false;
  for (uint32_t i = 0; !found && i < cfg.sib1.sched_info_list_mbms_r14.size(); i++) {
    for (uint32_t j = 0; j < cfg.sib1.sched_info_list_mbms_r14[i].sib_map_info_r14.size(); j++) {
      if (cfg.sib1.sched_info_list_mbms_r14[i].sib_map_info_r14[j].value ==
          sib_type_mbms_r14_opts::sib_type12_v920) {
        found = true;
        break;
      }
    }
  }
  if (!found) {
    sched_info_mbms_r14_s new_si;
    new_si.si_periodicity_r14.value = sched_info_mbms_r14_s::si_periodicity_r14_opts::rf16;
    sib_type_mbms_r14_e sib12_enum;
    sib12_enum.value = sib_type_mbms_r14_opts::sib_type12_v920;
    new_si.sib_map_info_r14.push_back(sib12_enum);
    cfg.sib1.sched_info_list_mbms_r14.push_back(new_si);
    sib12_sched_added_ = true;
  }

  cfg.sib1.sys_info_value_tag_r14 = (cfg.sib1.sys_info_value_tag_r14 + 1) & 0x1F;
  if (!cfg.sib_tag_state_file.empty()) {
    FILE* f = fopen(cfg.sib_tag_state_file.c_str(), "w");
    if (f) { fprintf(f, "%u\n", (unsigned)cfg.sib1.sys_info_value_tag_r14); fclose(f); }
  }
  generate_sibs();
  update_mac_sib_cfg();
  etws_paging_active_.store(true);
  etws_paging_count_.store(64);
  logger.warning("SIB12 emergency alert activated — 64 ETWS paging broadcasts scheduled");
}

void rrc::clear_sib12()
{
  etws_paging_active_.store(false);
  etws_paging_count_.store(0);

  if (sib12_sched_added_) {
    sched_info_list_mbms_r14_l new_list;
    for (uint32_t i = 0; i < cfg.sib1.sched_info_list_mbms_r14.size(); i++) {
      const sched_info_mbms_r14_s& si = cfg.sib1.sched_info_list_mbms_r14[i];
      bool has_sib12 = false;
      for (uint32_t j = 0; j < si.sib_map_info_r14.size(); j++) {
        if (si.sib_map_info_r14[j].value == sib_type_mbms_r14_opts::sib_type12_v920) {
          has_sib12 = true;
          break;
        }
      }
      if (!has_sib12) {
        new_list.push_back(si);
      }
    }
    cfg.sib1.sched_info_list_mbms_r14 = new_list;
    sib12_sched_added_                 = false;
  }

  cfg.sib1.sys_info_value_tag_r14 = (cfg.sib1.sys_info_value_tag_r14 + 1) & 0x1F;
  if (!cfg.sib_tag_state_file.empty()) {
    FILE* f = fopen(cfg.sib_tag_state_file.c_str(), "w");
    if (f) { fprintf(f, "%u\n", (unsigned)cfg.sib1.sys_info_value_tag_r14); fclose(f); }
  }
  generate_sibs();
  update_mac_sib_cfg();
  logger.warning("SIB12 emergency alert cancelled");
}

void rrc::write_replace_warning(const asn1::s1ap::write_replace_warning_request_ies_container& ies)
{
  // Single-segment only for now (see enb_rrc_interfaces.h's write_replace_warning() comment) --
  // sib_type12_r9_s's warning_msg_segment_r9 is a dyn_octstring with no built-in size cap, but a
  // real cell has a finite SI transport block size. Splitting a long Warning-Message-Contents
  // across multiple SIB12 instances (warning_msg_segment_type_r9/_num_r9) is a defined follow-up,
  // not implemented here -- flag it loudly instead of silently truncating.
  static const uint32_t SINGLE_SEGMENT_WARN_THRESHOLD = 1000;
  if (ies.warning_msg_contents.value.size() > SINGLE_SEGMENT_WARN_THRESHOLD) {
    logger.warning(
        "write_replace_warning: Warning-Message-Contents is %d bytes; multi-segment SIB12 chunking is not "
        "implemented, sending as a single (possibly oversized for some cells) segment",
        (int)ies.warning_msg_contents.value.size());
  }

  asn1::rrc::sib_type12_r9_s sib12_data = {};
  sib12_data.msg_id_r9.from_number(ies.msg_id.value.to_number());
  sib12_data.serial_num_r9.from_number(ies.serial_num.value.to_number());
  sib12_data.warning_msg_segment_type_r9.value = asn1::rrc::sib_type12_r9_s::warning_msg_segment_type_r9_opts::last_segment;
  sib12_data.warning_msg_segment_num_r9        = 0;

  if (ies.data_coding_scheme_present) {
    sib12_data.data_coding_scheme_r9[0]      = (uint8_t)ies.data_coding_scheme.value.to_number();
    sib12_data.data_coding_scheme_r9_present = true;
  }

  if (ies.warning_msg_contents_present) {
    sib12_data.warning_msg_segment_r9.resize(ies.warning_msg_contents.value.size());
    memcpy(sib12_data.warning_msg_segment_r9.data(),
           ies.warning_msg_contents.value.data(),
           ies.warning_msg_contents.value.size());
  }

  if (ies.warning_area_coordinates_present) {
    // Opaque octet string, per ATIS-0700041 -- carried through untouched, never decoded here
    // (mirrors srsepc/src/mme/sbc.cc's own handling of the same IE on the CBC-facing side).
    sib12_data.warning_area_coordinates_segment_r15_present = true;
    sib12_data.warning_area_coordinates_segment_r15.resize(ies.warning_area_coordinates.value.size());
    memcpy(sib12_data.warning_area_coordinates_segment_r15.data(),
           ies.warning_area_coordinates.value.data(),
           ies.warning_area_coordinates.value.size());
  }

  install_sib12(sib12_data);
}

void rrc::kill_warning(const asn1::s1ap::kill_request_ies_container& ies)
{
  // This eNB only tracks a single active SIB12 slot (same model reload_sib12()/the file+SIGUSR1
  // path already uses) -- Kill-Request's own msg_id/serial_num aren't matched against a specific
  // still-active alert, it just clears whatever is currently broadcasting.
  clear_sib12();
}

void rrc::set_q_rx_lev_min(int8_t value)
{
  if (value < -70 || value > -22) {
    logger.warning("set_q_rx_lev_min: value=%d out of range (must be -70..-22 dBm) — clamping", value);
    value = (value < -70) ? -70 : -22;
  }
  cfg.sib1.q_rx_lev_min_r14 = value;

  // Bump the value tag so already-idle UEs notice SIB1 changed and re-read it (TS 36.331
  // §5.2.1.3), same as reload_sib12() above -- unlike reconfigure_embms()'s eMBMS-specific
  // parameters (which UEs only re-read at the next MCCH/MTCH access anyway),
  // q-RxLevMin-r14 is a cell-selection criterion idle UEs may otherwise never revisit.
  cfg.sib1.sys_info_value_tag_r14 = (cfg.sib1.sys_info_value_tag_r14 + 1) & 0x1F;
  if (!cfg.sib_tag_state_file.empty()) {
    FILE* f = fopen(cfg.sib_tag_state_file.c_str(), "w");
    if (f) { fprintf(f, "%u\n", (unsigned)cfg.sib1.sys_info_value_tag_r14); fclose(f); }
  }
  generate_sibs();
  update_mac_sib_cfg();
  logger.info("q-RxLevMin-r14 updated to %d dBm", value);
}

void rrc::mbms_session_start(const std::string&    tmgi_key,
                              const srsran::tmgi_t& tmgi,
                              uint8_t               session_id,
                              bool                  session_id_present)
{
  srsran::pmch_info_t::mbms_session_info_t info;
  info.tmgi               = tmgi;
  info.session_id_present = session_id_present;
  info.session_id         = session_id;
  mbms_sessions[tmgi_key] = info;
  logger.info("MBMS session start: TMGI key %s, total sessions now %zu", tmgi_key.c_str(), mbms_sessions.size());
  configure_mbsfn_sibs();
}

void rrc::mbms_session_stop(const std::string& tmgi_key)
{
  auto it = mbms_sessions.find(tmgi_key);
  if (it == mbms_sessions.end()) {
    logger.warning("MBMS session stop for unknown TMGI key %s", tmgi_key.c_str());
    return;
  }
  mbms_sessions.erase(it);
  logger.info("MBMS session stop: TMGI key %s, total sessions now %zu", tmgi_key.c_str(), mbms_sessions.size());
  configure_mbsfn_sibs();
}

void rrc::configure_mbsfn_sibs()
{
  // populate struct with sib2 values needed in PHY/MAC
  srsran::sib2_mbms_t sibs2;
  sibs2.mbsfn_sf_cfg_list_present = cfg.sibs[1].sib2().mbsfn_sf_cfg_list_present;
  sibs2.nof_mbsfn_sf_cfg          = cfg.sibs[1].sib2().mbsfn_sf_cfg_list.size();
  for (int i = 0; i < sibs2.nof_mbsfn_sf_cfg; i++) {
    const asn1::rrc::mbsfn_sf_cfg_s& src = cfg.sibs[1].sib2().mbsfn_sf_cfg_list[i];
    sibs2.mbsfn_sf_cfg_list[i].radioframe_alloc_offset = src.radioframe_alloc_offset;
    sibs2.mbsfn_sf_cfg_list[i].radioframe_alloc_period =
        (srsran::mbsfn_sf_cfg_t::alloc_period_t)src.radioframe_alloc_period.value;
    if (src.sf_alloc.type() == asn1::rrc::mbsfn_sf_cfg_s::sf_alloc_c_::types::four_frames) {
      sibs2.mbsfn_sf_cfg_list[i].nof_alloc_subfrs = srsran::mbsfn_sf_cfg_t::sf_alloc_type_t::four_frames;
      sibs2.mbsfn_sf_cfg_list[i].sf_alloc         = (uint32_t)src.sf_alloc.four_frames().to_number();
    } else {
      sibs2.mbsfn_sf_cfg_list[i].nof_alloc_subfrs = srsran::mbsfn_sf_cfg_t::sf_alloc_type_t::one_frame;
      sibs2.mbsfn_sf_cfg_list[i].sf_alloc         = (uint32_t)src.sf_alloc.one_frame().to_number();
    }
  }
  // populate struct with sib13 values needed for PHY/MAC
  srsran::sib13_t sibs13;
  sibs13.notif_cfg.notif_offset = cfg.sibs[12].sib13_v920().notif_cfg_r9.notif_offset_r9;
  sibs13.notif_cfg.notif_repeat_coeff =
      (srsran::mbms_notif_cfg_t::coeff_t)cfg.sibs[12].sib13_v920().notif_cfg_r9.notif_repeat_coeff_r9.value;
  sibs13.notif_cfg.notif_sf_idx = cfg.sibs[12].sib13_v920().notif_cfg_r9.notif_sf_idx_r9;
  sibs13.nof_mbsfn_area_info    = cfg.sibs[12].sib13_v920().mbsfn_area_info_list_r9.size();
  for (uint32_t i = 0; i < sibs13.nof_mbsfn_area_info; i++) {
    sibs13.mbsfn_area_info_list[i] = srsran::make_mbsfn_area_info(cfg.sibs[12].sib13_v920().mbsfn_area_info_list_r9[i]);
    /* Rel-17: pmch-Bandwidth-r17 overrides MBSFN PRB count */
    if (cfg.pmch_bandwidth > 0) {
      sibs13.mbsfn_area_info_list[i].pmch_bandwidth = cfg.pmch_bandwidth;
    }
    /*
    sibs13.mbsfn_area_info_list[i].mbsfn_area_id =
        cfg.sibs[12].sib13_v920().mbsfn_area_info_list_r9[i].mbsfn_area_id_r9;
    sibs13.mbsfn_area_info_list[i].notif_ind        = cfg.sibs[12].sib13_v920().mbsfn_area_info_list_r9[i].notif_ind_r9;
    sibs13.mbsfn_area_info_list[i].mcch_cfg.sig_mcs = (srsran::mbsfn_area_info_t::mcch_cfg_t::sig_mcs_t)cfg.sibs[12]
                                                          .sib13_v920()
                                                          .mbsfn_area_info_list_r9[i]
                                                          .mcch_cfg_r9.sig_mcs_r9.value;
    sibs13.mbsfn_area_info_list[i].mcch_cfg.sf_alloc_info =
        cfg.sibs[12].sib13_v920().mbsfn_area_info_list_r9[i].mcch_cfg_r9.sf_alloc_info_r9.to_number();
    sibs13.mbsfn_area_info_list[i].mcch_cfg.mcch_repeat_period = 
      from_mcch_repeat_period_r9(cfg.sibs[12]
            .sib13_v920()
            .mbsfn_area_info_list_r9[i]
            .mcch_cfg_r9.mcch_repeat_period_r9.value);
    sibs13.mbsfn_area_info_list[i].mcch_cfg.mcch_offset =
        cfg.sibs[12].sib13_v920().mbsfn_area_info_list_r9[i].mcch_cfg_r9.mcch_offset_r9;
    sibs13.mbsfn_area_info_list[i].mcch_cfg.mcch_mod_period = 
      from_mcch_mod_period_r9(cfg.sibs[12]
            .sib13_v920()
            .mbsfn_area_info_list_r9[i]
            .mcch_cfg_r9.mcch_mod_period_r9.value);
    sibs13.mbsfn_area_info_list[i].non_mbsfn_region_len = (srsran::mbsfn_area_info_t::region_len_t)cfg.sibs[12]
                                                              .sib13_v920()
                                                              .mbsfn_area_info_list_r9[i]
                                                              .non_mbsfn_region_len.value;
    sibs13.mbsfn_area_info_list[i].notif_ind = cfg.sibs[12].sib13_v920().mbsfn_area_info_list_r9[i].notif_ind_r9;
    */
  }

  /* Apply r16-only operator config to the internal sibs13 used by MAC get_mch_sched.
   * make_mbsfn_area_info(r9) cannot derive 0.37/2.5 kHz SCS or SL2 from r9 alone. */
  if (sibs13.nof_mbsfn_area_info > 0) {
    using SCS = srsran::mbsfn_area_info_t::subcarrier_spacing_t;
    using TS  = srsran::mbsfn_area_info_t::time_separation_t;
    if (!cfg.pmch_subcarrier_spacing.empty()) {
      if      (cfg.pmch_subcarrier_spacing == "khz0dot37") sibs13.mbsfn_area_info_list[0].subcarrier_spacing = SCS::khz_0dot37;
      else if (cfg.pmch_subcarrier_spacing == "khz2dot5")  sibs13.mbsfn_area_info_list[0].subcarrier_spacing = SCS::khz_2dot5;
      else if (cfg.pmch_subcarrier_spacing == "khz1dot25") sibs13.mbsfn_area_info_list[0].subcarrier_spacing = SCS::khz_1dot25;
      else if (cfg.pmch_subcarrier_spacing == "khz7dot5")  sibs13.mbsfn_area_info_list[0].subcarrier_spacing = SCS::khz_7dot5;
      else if (cfg.pmch_subcarrier_spacing == "khz15")     sibs13.mbsfn_area_info_list[0].subcarrier_spacing = SCS::khz_15;
    }
    if (cfg.pmch_time_separation_sl2) {
      sibs13.mbsfn_area_info_list[0].time_separation = TS::sl2;
    }
  }

  /* Serialize pmch-Bandwidth-r17 into OTA SIB13 as an r16 MBSFN area info extension.
   * mbsfn_area_info_r16_s carries the r17 bandwidth field (n30/n35/n40 = 30/35/40 PRBs).
   * Mirror the r9 MCCH config into the mandatory r16 fields so the extension is self-consistent.
   * Always clear first so a transition from bandwidth>0 → 0 removes the stale extension. */
  {
    auto& sib13 = cfg.sibs[12].sib13_v920();
    sib13.mbsfn_area_info_list_r16_present = false;
    sib13.mbsfn_area_info_list_r16.resize(0);
    sib13.ext = sib13.notif_cfg_v1430.is_present();
  }
  /* TS 36.331 §6.3.7 Rel-16: mbsfn_area_info_list_r16 must be present for all
   * MBMS-dedicated cells, not only those needing pmch_bandwidth_r17, SL2, or a
   * non-r9 SCS.  The SCS mapping below handles all four values correctly. */
  if (cfg.sibs[12].sib13_v920().mbsfn_area_info_list_r9.size() > 0) {
    using R16     = mbsfn_area_info_r16_s;
    using R16MCCH = R16::mcch_cfg_r16_s_;
    using R16SCS  = R16::subcarrier_spacing_mbms_r16_e_;
    using R16BW   = R16::pmch_bandwidth_r17_e_;
    using R16TS   = R16::time_separation_r16_e_;

    auto& sib13 = cfg.sibs[12].sib13_v920();
    sib13.ext   = true;
    const uint32_t n = sib13.mbsfn_area_info_list_r9.size();
    sib13.mbsfn_area_info_list_r16.resize(n);

    for (uint32_t i = 0; i < n; i++) {
      const auto& r9  = sib13.mbsfn_area_info_list_r9[i];
      auto&       r16 = sib13.mbsfn_area_info_list_r16[i];

      r16.mbsfn_area_id_r16 = r9.mbsfn_area_id_r9;
      r16.notif_ind_r16     = r9.notif_ind_r9;

      // Map r9 MCCH repeat period (rf32..rf256) to r16 (rf1..rf256)
      switch (r9.mcch_cfg_r9.mcch_repeat_period_r9.to_number()) {
        case 1:   r16.mcch_cfg_r16.mcch_repeat_period_r16 = R16MCCH::mcch_repeat_period_r16_opts::rf1;   break;
        case 2:   r16.mcch_cfg_r16.mcch_repeat_period_r16 = R16MCCH::mcch_repeat_period_r16_opts::rf2;   break;
        case 4:   r16.mcch_cfg_r16.mcch_repeat_period_r16 = R16MCCH::mcch_repeat_period_r16_opts::rf4;   break;
        case 8:   r16.mcch_cfg_r16.mcch_repeat_period_r16 = R16MCCH::mcch_repeat_period_r16_opts::rf8;   break;
        case 16:  r16.mcch_cfg_r16.mcch_repeat_period_r16 = R16MCCH::mcch_repeat_period_r16_opts::rf16;  break;
        case 32:  r16.mcch_cfg_r16.mcch_repeat_period_r16 = R16MCCH::mcch_repeat_period_r16_opts::rf32;  break;
        case 64:  r16.mcch_cfg_r16.mcch_repeat_period_r16 = R16MCCH::mcch_repeat_period_r16_opts::rf64;  break;
        case 128: r16.mcch_cfg_r16.mcch_repeat_period_r16 = R16MCCH::mcch_repeat_period_r16_opts::rf128; break;
        default:  r16.mcch_cfg_r16.mcch_repeat_period_r16 = R16MCCH::mcch_repeat_period_r16_opts::rf256; break;
      }

      // Map r9 MCCH mod period (rf512..rf1024) to r16 (rf1..rf1024)
      switch (r9.mcch_cfg_r9.mcch_mod_period_r9.to_number()) {
        case 512:  r16.mcch_cfg_r16.mcch_mod_period_r16 = R16MCCH::mcch_mod_period_r16_opts::rf512;  break;
        case 1024: r16.mcch_cfg_r16.mcch_mod_period_r16 = R16MCCH::mcch_mod_period_r16_opts::rf1024; break;
        default:   r16.mcch_cfg_r16.mcch_mod_period_r16 = R16MCCH::mcch_mod_period_r16_opts::rf512;  break;
      }

      r16.mcch_cfg_r16.mcch_offset_r16 = r9.mcch_cfg_r9.mcch_offset_r9;
      // R9 sf-AllocInfo is 6 bits (SF1,2,3,6,7,8 at bit positions 5..0 MSB-first).
      // R16 sf-AllocInfo is 9 bits (SF1..9 at bit positions 8..0 MSB-first).
      // Cannot use to_number()/from_number() directly — bit positions differ.
      {
        const uint32_t v9 = r9.mcch_cfg_r9.sf_alloc_info_r9.to_number(); // bits: SF1=32,SF2=16,SF3=8,SF6=4,SF7=2,SF8=1
        uint32_t v16 = 0;
        if (v9 & 32u) v16 |= 256u; // SF1 → r16 bit 8
        if (v9 & 16u) v16 |= 128u; // SF2 → r16 bit 7
        if (v9 &  8u) v16 |=  64u; // SF3 → r16 bit 6
        if (v9 &  4u) v16 |=   8u; // SF6 → r16 bit 3
        if (v9 &  2u) v16 |=   4u; // SF7 → r16 bit 2
        if (v9 &  1u) v16 |=   2u; // SF8 → r16 bit 1
        r16.mcch_cfg_r16.sf_alloc_info_r16.from_number(v16);
      }

      // sig_mcs: same enumeration order in r9 and r16 (n2/n7/n13/n19)
      r16.mcch_cfg_r16.sig_mcs_r16 = (R16MCCH::sig_mcs_r16_opts::options)r9.mcch_cfg_r9.sig_mcs_r9.value;

      // Map subcarrier spacing to r16 enum.
      // If the operator set an explicit override (required for 2.5/0.37 kHz which
      // have no r9 representation), use it; otherwise derive from the r9 field.
      // khz15 (plain LTE numerology, no FeMBMS override) is the spec default -- it must be
      // the fallback in every branch below, not khz_7dot5. Before this fix, every one of
      // these fallbacks (an unrecognized override string, a present-but-unhandled r9 value,
      // and -- the common case -- no override and no r9 field at all) unconditionally chose
      // khz_7dot5, so any plain-15kHz MBMS-dedicated cell mis-signalled itself as 7.5 kHz in
      // this r16 extension (which is mandatory, always populated, for every MBMS-dedicated
      // cell -- there is no "absent" state to fall back on instead).
      if (!cfg.pmch_subcarrier_spacing.empty()) {
        if      (cfg.pmch_subcarrier_spacing == "khz1dot25") r16.subcarrier_spacing_mbms_r16 = R16SCS::khz_1dot25;
        else if (cfg.pmch_subcarrier_spacing == "khz2dot5")  r16.subcarrier_spacing_mbms_r16 = R16SCS::khz_2dot5;
        else if (cfg.pmch_subcarrier_spacing == "khz7dot5")  r16.subcarrier_spacing_mbms_r16 = R16SCS::khz_7dot5;
        else if (cfg.pmch_subcarrier_spacing == "khz0dot37") r16.subcarrier_spacing_mbms_r16 = R16SCS::khz0dot37;
        else if (cfg.pmch_subcarrier_spacing == "khz15")     r16.subcarrier_spacing_mbms_r16 = R16SCS::khz15;
        else                                                  r16.subcarrier_spacing_mbms_r16 = R16SCS::khz15;
      } else if (r9.subcarrier_spacing_mbms_r14_present) {
        // No khz15 case: subcarrierSpacingMBMS-r14 only has {khz7dot5, khz1dot25} as
        // real values -- 15 kHz at the r9/r14 level is represented by this field being
        // absent (the branch below), so it can't reach this switch at all.
        switch (r9.subcarrier_spacing_mbms_r14.value) {
          case mbsfn_area_info_r9_s::subcarrier_spacing_mbms_r14_e_::khz7dot5:
            r16.subcarrier_spacing_mbms_r16 = R16SCS::khz_7dot5;  break;
          case mbsfn_area_info_r9_s::subcarrier_spacing_mbms_r14_e_::khz1dot25:
            r16.subcarrier_spacing_mbms_r16 = R16SCS::khz_1dot25; break;
          default:
            r16.subcarrier_spacing_mbms_r16 = R16SCS::khz15;      break;
        }
      } else {
        r16.subcarrier_spacing_mbms_r16 = R16SCS::khz15;
      }

      /* timeSeparation-r16: SL2 or SL4 for 0.37 kHz cells (TS 36.211 §4.1).
       * Absent = SL4 (default). Only set when operator configures SL2. */
      if (cfg.pmch_time_separation_sl2) {
        r16.time_separation_r16_present = true;
        r16.time_separation_r16         = R16TS::s12;
      } else {
        r16.time_separation_r16_present = false;
      }

      // pmch-Bandwidth-r17: 30/35/40 PRBs for 6/7/8 MHz FeMBMS channels
      switch (cfg.pmch_bandwidth) {
        case 30: r16.pmch_bandwidth_r17 = R16BW::n30; break;
        case 35: r16.pmch_bandwidth_r17 = R16BW::n35; break;
        case 40: r16.pmch_bandwidth_r17 = R16BW::n40; break;
        default: r16.pmch_bandwidth_r17_present = false; continue;
      }
      r16.pmch_bandwidth_r17_present = true;
    }
    sib13.mbsfn_area_info_list_r16_present = true;
    logger.info("SIB13: added mbsfn_area_info_list_r16: pmch_bandwidth_r17=%d PRBs, time_separation=%s, scs_override=%s",
                cfg.pmch_bandwidth,
                cfg.pmch_time_separation_sl2 ? "SL2" : "SL4(default)",
                cfg.pmch_subcarrier_spacing.empty() ? "(from-r9)" : cfg.pmch_subcarrier_spacing.c_str());
  }

  /* MBMS-ROM-Info-r16 (TS 36.331 §6.3.7 Rel-16): advertise channel params for ROM receivers.
   * Derive EARFCN and BW from cell config; SCS from pmch_subcarrier_spacing if set. */
  {
    using ROM    = asn1::rrc::mbms_rom_info_r16_s;
    using ROMSCS = ROM::subcarrier_spacing_r16_e_;
    using ROMBW  = ROM::bw_r16_e_;
    auto& sib13  = cfg.sibs[12].sib13_v920();
    sib13.mbms_rom_info_list_r16_present = false;
    sib13.mbms_rom_info_list_r16.resize(0);

    if (sib13.mbsfn_area_info_list_r9.size() > 0 && !cfg.cell_list.empty()) {
      ROM ri{};
      ri.rom_freq_r16 = cfg.cell_list[0].dl_earfcn;

      /* Map nof_prb → BW enum (standard values only) */
      switch (cfg.cell.nof_prb) {
        case 6:   ri.bw_r16 = ROMBW::n6;   break;
        case 15:  ri.bw_r16 = ROMBW::n15;  break;
        case 25:  ri.bw_r16 = ROMBW::n25;  break;
        case 50:  ri.bw_r16 = ROMBW::n50;  break;
        case 75:  ri.bw_r16 = ROMBW::n75;  break;
        case 100: ri.bw_r16 = ROMBW::n100; break;
        default:  ri.bw_r16 = ROMBW::n25;  break;
      }

      /* Map pmch SCS to ROM SCS enum when set (khz15 is implicit default, omit to save bits) */
      if (!cfg.pmch_subcarrier_spacing.empty() && cfg.pmch_subcarrier_spacing != "khz15") {
        ri.subcarrier_spacing_r16_present = true;
        if      (cfg.pmch_subcarrier_spacing == "khz7dot5")  ri.subcarrier_spacing_r16 = ROMSCS::khz7dot5;
        else if (cfg.pmch_subcarrier_spacing == "khz1dot25") ri.subcarrier_spacing_r16 = ROMSCS::khz1dot25;
        else                                                  ri.subcarrier_spacing_r16 = ROMSCS::khz15;
      }

      sib13.ext = true;
      sib13.mbms_rom_info_list_r16.push_back(ri);
      sib13.mbms_rom_info_list_r16_present = true;
      /* Mirror to SIB1-MBMS embedded copy */
      cfg.sib1.sib_type13_r14.ext = true;
      cfg.sib1.sib_type13_r14.mbms_rom_info_list_r16.push_back(ri);
      cfg.sib1.sib_type13_r14.mbms_rom_info_list_r16_present = true;
      logger.info("SIB13: MBMS-ROM-Info-r16: EARFCN=%u BW=%uPRB%s", ri.rom_freq_r16, cfg.cell.nof_prb,
                  ri.subcarrier_spacing_r16_present
                      ? std::string(" SCS=") + ri.subcarrier_spacing_r16.to_string()
                      : "");
    }
  }

  srsran::mcch_msg_t mcch_t;
  /* sf_alloc_end = sched_period_rf * 10 - nof_cas*(1 + add_non) - 1 (MCCH).
   * Each active CAS frame removes 1 SF (SF0) plus add_non additional non-MBSFN SFs (SF1..N).
   * With CAS muting, fewer active CAS frames leave more MBSFN data subframes. */
  uint32_t sched_period_rf = cfg.mch_sched_period_rf ? cfg.mch_sched_period_rf : 64u;
  /* CAS candidate period is 4 frames for wide cells (nof_prb>=25) but 8 frames
   * for narrow cells (6<nof_prb<25, TS 36.211 §6.6.4.1) — see phy_common.cc. */
  bool     narrow_cell = cfg.cell.nof_prb > 6 && cfg.cell.nof_prb < 25;
  uint32_t nof_cas;
  if (cfg.cell.cas_muting) {
    uint32_t n_cas  = (uint32_t)cfg.cell.n_cas;
    uint32_t k_cas  = (uint32_t)cfg.cell.k_cas;
    uint32_t period = 16u * n_cas;
    uint32_t rem    = sched_period_rf % period;
    uint32_t cap    = 4u * k_cas;
    nof_cas = (sched_period_rf / period) * k_cas + (rem < cap ? rem : cap) / 4u;
  } else {
    nof_cas = sched_period_rf / (narrow_cell ? 8u : 4u);
  }
  using CAP = srsran::mcch_msg_t::common_sf_alloc_period_t;
  srsran::mcch_msg_t::common_sf_alloc_period_t cap;
  switch (sched_period_rf) {
    case 4:   cap = CAP::rf4;   break;
    case 8:   cap = CAP::rf8;   break;
    case 16:  cap = CAP::rf16;  break;
    case 32:  cap = CAP::rf32;  break;
    default:  cap = CAP::rf64;  break;
  }
  mcch_t.common_sf_alloc_period = cap;
  mcch_t.nof_common_sf_alloc            = 1;
  srsran::mbsfn_sf_cfg_t& sf_alloc_item  = mcch_t.common_sf_alloc[0];
  sf_alloc_item.radioframe_alloc_offset  = 0;
  sf_alloc_item.radioframe_alloc_period  = srsran::mbsfn_sf_cfg_t::alloc_period_t::n1;
  sf_alloc_item.nof_alloc_subfrs         = srsran::mbsfn_sf_cfg_t::sf_alloc_type_t::one_frame;
  sf_alloc_item.sf_alloc                 = 63;
  mcch_t.nof_pmch_info                  = 1;
  srsran::pmch_info_t* pmch_item        = &mcch_t.pmch_info_list[0];

  if (mbms_sessions.empty()) {
    uint32_t nof_sessions             = cfg.nof_mbms_sessions ? cfg.nof_mbms_sessions : 1u;
    pmch_item->nof_mbms_session_info = nof_sessions;
    for (uint32_t s = 0; s < nof_sessions && s < 8u; s++) {
      pmch_item->mbms_session_info_list[s].lc_ch_id = (uint8_t)(s + 1);
    }
  } else {
    // Real, M3AP-driven session state (see mbms_session_start()) instead of the static/fabricated fallback above.
    // (Comparing against a plain uint32_t copy, not a reference to max_session_per_pmch directly: the latter
    // is an in-class-initialized static const with no out-of-class definition, so std::min binding a const
    // reference to it is an ODR-use that fails to link.)
    uint32_t max_sessions  = srsran::pmch_info_t::max_session_per_pmch;
    uint32_t nof_sessions  = std::min<uint32_t>((uint32_t)mbms_sessions.size(), max_sessions);
    pmch_item->nof_mbms_session_info = nof_sessions;
    uint32_t s                       = 0;
    for (auto& kv : mbms_sessions) {
      if (s >= nof_sessions) {
        break;
      }
      pmch_item->mbms_session_info_list[s]            = kv.second;
      pmch_item->mbms_session_info_list[s].lc_ch_id   = (uint8_t)(s + 1);
      s++;
    }
  }
  uint16_t mbms_mcs = cfg.mbms_mcs;
  if (mbms_mcs > 26) {
    mbms_mcs = 26; // ETSI TS 103 720 Table 11.3.1-1/-2 (Physical layer capacity for
                   // LTE-based 5G Broadcast, all supported numerologies, full
                   // QPSK/16QAM/64QAM/256QAM range): both tables list MCS 0-26 only,
                   // never 27/28, across both the published V1.2.1 and the current
                   // draft. MCS 27/28 exist in the generic TS 36.213 clause 11.1 PMCH
                   // MCS tables (Table 7.1.7.1-1/1A, Table 11.1-1/11.1-2) but are
                   // outside the range this 5G Broadcast profile actually defines/uses.
    logger.warning("PMCH data MCS too high, setting it to 26");
  }
  if (cfg.pmch_subcarrier_spacing.empty() || cfg.pmch_subcarrier_spacing == "khz15") {
    uint32_t nof_prb_pmch = cfg.cell.mbsfn_prb ? cfg.cell.mbsfn_prb : cfg.cell.nof_prb;
    uint16_t feasible_mcs = clamp_pmch_mcs_to_feasible(mbms_mcs, nof_prb_pmch, cfg.pmch_use_mcs_table2);
    if (feasible_mcs < mbms_mcs) {
      logger.error("Configured PMCH MCS=%d is infeasible for %d PRB (code rate > 1, every "
                    "subframe would fail CRC); clamping to MCS=%d",
                    mbms_mcs, nof_prb_pmch, feasible_mcs);
      mbms_mcs = feasible_mcs;
    }
  }
  logger.debug("PMCH data MCS=%d", mbms_mcs);
  pmch_item->data_mcs             = mbms_mcs;
  uint32_t add_non = (uint32_t)cfg.cell.additional_non_mbms_frames;
  pmch_item->sf_alloc_end = (uint32_t)(sched_period_rf * 10u - nof_cas * (1u + add_non) - 1u);
  /* TS 36.211 §6.5.3: M_TimePMCH must divide the PMCH data subframe count (sf_alloc_end,
   * since pmch_start=1 for pmch[0]).  Clamp M down to the largest divisor <= configured M. */
  if (cfg.pmch_time_interleaving_n > 1 && cfg.pmch_time_interleaving_m > 1) {
    uint32_t data_sfs = pmch_item->sf_alloc_end; // pmch_start=1, data sfs: 1..sf_alloc_end
    uint8_t  m        = cfg.pmch_time_interleaving_m;
    if (data_sfs % m != 0) {
      while (m > 1 && data_sfs % m != 0) { m--; }
      logger.warning("time_interleaving_m=%d does not divide sf_alloc_end=%d; clamping to %d",
                     cfg.pmch_time_interleaving_m, data_sfs, m);
      cfg.pmch_time_interleaving_m = m;
    }
    pmch_item->time_interleaving_m = cfg.pmch_time_interleaving_m;
  }
  /* pack_mcch() below reads cfg.pmch_time_interleaving_m to build the OTA v1900
   * extension IE — it must run after the divisor-clamp above, or the eNB would
   * broadcast an M value different from the one it actually uses for TX timing. */
  pack_mcch();
  using SP = srsran::pmch_info_t::mch_sched_period_t;
  switch (sched_period_rf) {
    case 4:   pmch_item->mch_sched_period = SP::rf4;   break;
    case 8:   pmch_item->mch_sched_period = SP::rf8;   break;
    case 16:  pmch_item->mch_sched_period = SP::rf16;  break;
    case 32:  pmch_item->mch_sched_period = SP::rf32;  break;
    default:  pmch_item->mch_sched_period = SP::rf64;  break;
  }
  pmch_item->use_mcs_table2       = cfg.pmch_use_mcs_table2;
  pmch_item->time_interleaving_n  = cfg.pmch_time_interleaving_n;
  pmch_item->time_interleaving_m  = cfg.pmch_time_interleaving_m;
  /* Only meaningful when the main N/M above are actually active with 2+ sessions
   * (reconfigure_embms() already zeroes these otherwise) — read by mac.cc's
   * get_mch_sched() to pick which of nof_mbms_session_info's sessions is "last"
   * and give it different (or, via N-last=1, no) time interleaving. */
  pmch_item->time_interleaving_n_last_mtch = cfg.pmch_time_interleaving_n_last_mtch;
  pmch_item->time_interleaving_m_last_mtch = cfg.pmch_time_interleaving_m_last_mtch;
  /* cyclic_shift_alpha can only be OTA-signaled to UEs inside the same v1900 IE as
   * time interleaving (see pack_mcch(): pmch_cyclic_shift_alpha_r19 lives inside
   * time_interleav_cfg_r19, only present when time_interleaving_n>1). Gate PHY's
   * application of the shift the same way, or the transmitted waveform would use a
   * rotation UEs were never told about and have no way to compensate for. */
  pmch_item->cyclic_shift       = (cfg.pmch_time_interleaving_n > 1) && (cfg.pmch_cyclic_shift_alpha > 0);
  pmch_item->cyclic_shift_alpha = (cfg.pmch_time_interleaving_n > 1) ? cfg.pmch_cyclic_shift_alpha : 0;
  pmch_item->freq_interleaving    = cfg.pmch_freq_interleaving;
  /* PMCH-SoftBufferSizeParameters-r19 (TS 36.212 §5.1.4.1.2 N_cb capping) — only
   * meaningful/OTA-signalled when time_interleaving_n > 1, same gating as cyclic_shift
   * above; harmless to always set on the internal struct otherwise. */
  pmch_item->n_soft_ref_category     = cfg.pmch_n_soft_ref_category;
  pmch_item->scaling_factor_beta_num = cfg.pmch_scaling_factor_beta_num;
  pmch_item->scaling_factor_beta_den = cfg.pmch_scaling_factor_beta_den;

  // Configure PHY when PHY is done being initialized
  //
  // cas_muting/k_cas/n_cas/additional_non_mbsfn_subframes are captured by value here (like
  // sibs2/sibs13/mcch_t above), not read fresh from cfg.cell inside the deferred lambda: this
  // keeps whatever reaches PHY a consistent snapshot from *this* configure_mbsfn_sibs() call,
  // not whatever cfg.cell happens to hold whenever the deferred task actually runs (which could
  // be from a newer, concurrent reconfigure by then).
  const bool    cas_muting_snapshot = cfg.cell.cas_muting;
  const uint8_t k_cas_snapshot      = cfg.cell.k_cas;
  const uint8_t n_cas_snapshot      = cfg.cell.n_cas;
  const uint8_t add_non_snapshot    = cfg.cell.additional_non_mbms_frames;
  task_sched.defer_task(
      [this, sibs2, sibs13, mcch_t, cas_muting_snapshot, k_cas_snapshot, n_cas_snapshot, add_non_snapshot]() mutable {
        phy->configure_mbsfn(&sibs2, &sibs13, mcch_t);
        // See phy_interface_rrc_lte::set_cell_cas_muting_cfg()'s doc comment: configure_mbsfn()
        // above only pushes the OTA-signalled SIB2/SIB13/MCCH content; PHY's own scheduling-
        // exclusion logic reads a separate, otherwise-never-refreshed cell config copy.
        phy->set_cell_cas_muting_cfg(cas_muting_snapshot, k_cas_snapshot, n_cas_snapshot, add_non_snapshot);
        mac->write_mcch(&sibs2, &sibs13, &mcch_t, mcch_payload_buffer, current_mcch_length);
      });

  // Rebuild the packed OTA SI buffers so UEs see the updated SIB13 (e.g. after
  // reconfigure_embms() changes pmch_bandwidth). Skip during init: generate_sibs()
  // hasn't been called yet, so cell_common_list is null.
  if (cell_common_list != nullptr) {
    generate_sibs();
    update_mac_sib_cfg();
  }
}

int rrc::pack_mcch()
{
  mcch.msg.set_c1();
  mbsfn_area_cfg_r9_s& area_cfg_r9      = mcch.msg.c1().mbsfn_area_cfg_r9();
  uint32_t sched_period_rf_p = cfg.mch_sched_period_rf ? cfg.mch_sched_period_rf : 64u;
  using CAP_r9 = mbsfn_area_cfg_r9_s::common_sf_alloc_period_r9_e_;
  mbsfn_area_cfg_r9_s::common_sf_alloc_period_r9_e_ cap_r9;
  switch (sched_period_rf_p) {
    case 4:   cap_r9 = CAP_r9::rf4;   break;
    case 8:   cap_r9 = CAP_r9::rf8;   break;
    case 16:  cap_r9 = CAP_r9::rf16;  break;
    case 32:  cap_r9 = CAP_r9::rf32;  break;
    default:  cap_r9 = CAP_r9::rf64;  break;
  }
  area_cfg_r9.common_sf_alloc_period_r9 = cap_r9;
  area_cfg_r9.common_sf_alloc_r9.resize(1);
  mbsfn_sf_cfg_s* sf_alloc_item          = &area_cfg_r9.common_sf_alloc_r9[0];
  sf_alloc_item->radioframe_alloc_offset = 0;
  sf_alloc_item->radioframe_alloc_period = mbsfn_sf_cfg_s::radioframe_alloc_period_e_::n1;
  sf_alloc_item->sf_alloc.set_one_frame().from_number(32 + 31);

  area_cfg_r9.pmch_info_list_r9.resize(1);
  pmch_info_r9_s* pmch_item = &area_cfg_r9.pmch_info_list_r9[0];
  if (mbms_sessions.empty()) {
    uint32_t nof_sessions_p = cfg.nof_mbms_sessions ? cfg.nof_mbms_sessions : 1u;
    pmch_item->mbms_session_info_list_r9.resize(nof_sessions_p);
    for (uint32_t s = 0; s < nof_sessions_p && s < 8u; s++) {
      auto& si = pmch_item->mbms_session_info_list_r9[s];
      si.lc_ch_id_r9           = (uint8_t)(s + 1);
      si.session_id_r9_present = true;
      si.session_id_r9[0]      = (uint8_t)s;
      si.tmgi_r9.plmn_id_r9.set_plmn_idx_r9() = 1;
      uint8_t sid[3]           = {0x0, 0x0, (uint8_t)s};
      memcpy(&si.tmgi_r9.service_id_r9[0], sid, 3);
    }
  } else {
    // Real, M3AP-driven session state (see mbms_session_start()) instead of the static/fabricated fallback
    // above. Always signals the TMGI's PLMN explicitly (set_explicit_value_r9()) rather than guessing a
    // plmn-idx-r9 into the cell's own broadcast PLMN-IdentityList, since a real MBMS session's PLMN isn't
    // necessarily known to match without cross-checking that list (out of scope for this pass).
    uint32_t nof_sessions_p = std::min<uint32_t>((uint32_t)mbms_sessions.size(), 8u);
    pmch_item->mbms_session_info_list_r9.resize(nof_sessions_p);
    uint32_t s = 0;
    for (auto& kv : mbms_sessions) {
      if (s >= nof_sessions_p) {
        break;
      }
      auto&                                  si  = pmch_item->mbms_session_info_list_r9[s];
      const srsran::pmch_info_t::mbms_session_info_t& info = kv.second;
      si.lc_ch_id_r9           = (uint8_t)(s + 1);
      si.session_id_r9_present = info.session_id_present;
      if (info.session_id_present) {
        si.session_id_r9[0] = info.session_id;
      }
      if (info.tmgi.plmn_id_type == srsran::tmgi_t::plmn_id_type_t::plmn_idx) {
        si.tmgi_r9.plmn_id_r9.set_plmn_idx_r9() = info.tmgi.plmn_id.plmn_idx;
      } else {
        srsran::to_asn1(&si.tmgi_r9.plmn_id_r9.set_explicit_value_r9(), info.tmgi.plmn_id.explicit_value);
      }
      memcpy(&si.tmgi_r9.service_id_r9[0], info.tmgi.serviced_id, 3);
      s++;
    }
  }

  uint16_t mbms_mcs = cfg.mbms_mcs;
  if (mbms_mcs > 26) {
    mbms_mcs = 26; // ETSI TS 103 720 Table 11.3.1-1/-2 (Physical layer capacity for
                   // LTE-based 5G Broadcast, all supported numerologies, full
                   // QPSK/16QAM/64QAM/256QAM range): both tables list MCS 0-26 only,
                   // never 27/28, across both the published V1.2.1 and the current
                   // draft. MCS 27/28 exist in the generic TS 36.213 clause 11.1 PMCH
                   // MCS tables (Table 7.1.7.1-1/1A, Table 11.1-1/11.1-2) but are
                   // outside the range this 5G Broadcast profile actually defines/uses.
    logger.warning("PMCH data MCS too high, setting it to 26");
  }
  if (cfg.pmch_subcarrier_spacing.empty() || cfg.pmch_subcarrier_spacing == "khz15") {
    uint32_t nof_prb_pmch = cfg.cell.mbsfn_prb ? cfg.cell.mbsfn_prb : cfg.cell.nof_prb;
    uint16_t feasible_mcs = clamp_pmch_mcs_to_feasible(mbms_mcs, nof_prb_pmch, cfg.pmch_use_mcs_table2);
    if (feasible_mcs < mbms_mcs) {
      logger.error("Configured PMCH MCS=%d is infeasible for %d PRB (code rate > 1, every "
                    "subframe would fail CRC); clamping to MCS=%d",
                    mbms_mcs, nof_prb_pmch, feasible_mcs);
      mbms_mcs = feasible_mcs;
    }
  }

  logger.debug("PMCH data MCS=%d", mbms_mcs);
  pmch_item->pmch_cfg_r9.data_mcs_r9         = mbms_mcs;
  using SP_r9 = pmch_cfg_r9_s::mch_sched_period_r9_e_;
  SP_r9 sp_r9;
  switch (sched_period_rf_p) {
    /* MCH-SchedulingPeriod-r9 has no rf4 value (r9 starts at rf8; rf4 was only
     * added by later PMCH-Config-r12/r19 extensions), so a period of 4 falls
     * through to the default below and is signalled at r9 level as rf64. */
    case 8:   sp_r9 = SP_r9::rf8;   break;
    case 16:  sp_r9 = SP_r9::rf16;  break;
    case 32:  sp_r9 = SP_r9::rf32;  break;
    default:  sp_r9 = SP_r9::rf64;  break;
  }
  pmch_item->pmch_cfg_r9.mch_sched_period_r9 = sp_r9;
  /* CAS candidate period is 4 frames for wide cells (nof_prb>=25) but 8 frames
   * for narrow cells (6<nof_prb<25, TS 36.211 §6.6.4.1) — see phy_common.cc. */
  bool     narrow_cell = cfg.cell.nof_prb > 6 && cfg.cell.nof_prb < 25;
  uint32_t nof_cas_p;
  if (cfg.cell.cas_muting) {
    uint32_t n_cas  = (uint32_t)cfg.cell.n_cas;
    uint32_t k_cas  = (uint32_t)cfg.cell.k_cas;
    uint32_t period = 16u * n_cas;
    uint32_t rem    = sched_period_rf_p % period;
    uint32_t cap    = 4u * k_cas;
    nof_cas_p = (sched_period_rf_p / period) * k_cas + (rem < cap ? rem : cap) / 4u;
  } else {
    nof_cas_p = sched_period_rf_p / (narrow_cell ? 8u : 4u);
  }
  uint32_t add_non = (uint32_t)cfg.cell.additional_non_mbms_frames;
  pmch_item->pmch_cfg_r9.sf_alloc_end_r9 = (uint16_t)(sched_period_rf_p * 10u - nof_cas_p * (1u + add_non) - 1u);

  // Rel-19 Phase 2: encode v1900 extension chain when any Phase 2 feature is enabled.
  // Also enable the chain (up to v1610, not v1900) whenever the cell is MBMS-dedicated,
  // so commonSF-Alloc-v1610 below can be signalled even with zero Phase 2 features on.
  const bool has_phase2 = (cfg.pmch_cyclic_shift_alpha > 0) || cfg.pmch_freq_interleaving ||
                          (cfg.pmch_time_interleaving_n > 1) || cfg.pmch_use_mcs_table2;
  if (has_phase2 || cfg.cell.mbms_dedicated) {
    using asn1::rrc::mbsfn_area_cfg_v1900_ies_s;
    using asn1::rrc::pmch_info_ext_r19_s;
    using asn1::rrc::pmch_tfi_cfg_r19_s;
    using asn1::rrc::pmch_soft_buf_size_params_r19_s;
    using asn1::rrc::pmch_cfg_r12_s;

    // Enable the extension chain: r9 → v930 → v1250 → v1430 → v1610 → v1900
    area_cfg_r9.non_crit_ext_present = true;
    auto& v930  = area_cfg_r9.non_crit_ext;
    v930.non_crit_ext_present = true;
    auto& v1250 = v930.non_crit_ext;
    v1250.non_crit_ext_present = true;
    auto& v1430 = v1250.non_crit_ext;
    v1430.non_crit_ext_present = true;
    /* commonSF-Alloc-r14 is mandatory (SIZE(1..8)) once this IE is present —
     * left unpopulated, pack() fails partway through the extension chain
     * (silently truncating the transmitted message; see caller below) and
     * v1610/v1900, the reason this chain was engaged, never gets encoded. */
    v1430.common_sf_alloc_r14.resize(1);
    v1430.common_sf_alloc_r14[0].sf_alloc_v1430.set_one_frame_v1430().from_number(3);
    auto& v1610 = v1430.non_crit_ext;

    /* commonSF-Alloc-v1610: TS 36.331 field description, "E-UTRAN includes commonSF-Alloc-
     * v1610 only when the cell is a MBMS-dedicated cell" — unlike v1430's commonSF-Alloc-r14
     * above, this field has its own presence bit and is genuinely optional. Signals both SF0
     * and SF5 allocated (oneFrame-v1610 = "11"), mirroring the v1430 idiom exactly — matches
     * is_mch_subframe()'s own scheduling, which (as of this fix) already treats SF5 (always)
     * and SF0 (on non-CAS-anchor frames) as ordinary MCH capacity on a dedicated cell.
     * Caveat, deliberately not modeled further here: this static "11" is not literally
     * accurate on a true/unmuted CAS-anchor frame, where SF0 is dynamically excluded by that
     * same function's CAS-muting logic — same "receivers must dynamically detect CAS
     * independent of static declarations" principle CAS-muting already establishes
     * elsewhere in this codebase, just applied to this IE for the first time. */
    if (cfg.cell.mbms_dedicated) {
      v1610.common_sf_alloc_v1610_present = true;
      v1610.common_sf_alloc_v1610.resize(1);
      v1610.common_sf_alloc_v1610[0].sf_alloc_v1610.set_one_frame_v1610().from_number(3);
    }

    // Continue into v1900 only when there is actual Rel-19 Phase 2 content to carry —
    // mbms_dedicated alone (with no Phase 2 features) stops at v1610.
    v1610.non_crit_ext_present = has_phase2;
    if (has_phase2) {
    auto& v1900 = v1610.non_crit_ext;

    v1900.pmch_info_list_ext_v1900_present = true;
    v1900.pmch_info_list_ext_v1900.resize(1);
    pmch_info_ext_r19_s& ext = v1900.pmch_info_list_ext_v1900[0];

    // Mirror the r9 PMCH in r12 form (same period and sf_alloc_end)
    ext.pmch_cfg_r19.sf_alloc_end_r12    = (uint16_t)(sched_period_rf_p * 10u - nof_cas_p * (1u + add_non) - 1u);
    using SP_r12 = pmch_cfg_r12_s::mch_sched_period_r12_e_;
    SP_r12 sp_r12;
    switch (sched_period_rf_p) {
      case 4:   sp_r12 = SP_r12::rf4;   break;
      case 8:   sp_r12 = SP_r12::rf8;   break;
      case 16:  sp_r12 = SP_r12::rf16;  break;
      case 32:  sp_r12 = SP_r12::rf32;  break;
      default:  sp_r12 = SP_r12::rf64;  break;
    }
    ext.pmch_cfg_r19.mch_sched_period_r12 = sp_r12;
    if (cfg.pmch_use_mcs_table2) {
      ext.pmch_cfg_r19.data_mcs_r12.set_higer_order_r12() = mbms_mcs;
    } else {
      ext.pmch_cfg_r19.data_mcs_r12.set_normal_r12() = mbms_mcs;
    }

    // TFI config (time interleaving + optional cyclic shift)
    /* pmch_time_interleav_n_r19 is a REQUIRED field inside time_interleav_cfg_r19_s_
     * with no n1 option — it cannot represent "no time interleaving". Therefore
     * time_interleav_cfg_r19_present may only be set when N >= 2. Cyclic shift lives
     * inside the same IE, so it can only be signalled alongside time interleaving. */
    if (cfg.pmch_time_interleaving_n > 1) {
      using N = pmch_tfi_cfg_r19_s::time_interleav_cfg_r19_s_::pmch_time_interleav_n_r19_e_;
      using M = pmch_tfi_cfg_r19_s::time_interleav_cfg_r19_s_::pmch_time_interleav_m_r19_e_;
      using A = pmch_tfi_cfg_r19_s::time_interleav_cfg_r19_s_::pmch_cyclic_shift_alpha_r19_e_;

      ext.pmch_tfi_cfg_r19_present        = true;
      auto& tfi                           = ext.pmch_tfi_cfg_r19;
      tfi.time_interleav_cfg_r19_present  = true;
      auto& tc                            = tfi.time_interleav_cfg_r19;

      tc.pmch_soft_buf_size_params_r19.pmch_time_interleaving_ref_ue_category_dl_r19  = cfg.pmch_n_soft_ref_category;
      tc.pmch_soft_buf_size_params_r19.pmch_time_interleaving_scaling_factor_beta_r19 =
        static_cast<pmch_soft_buf_size_params_r19_s::pmch_time_interleaving_scaling_factor_beta_r19_e_::options>(
            srsran::pmch_scaling_factor_beta_num_den_to_ordinal(cfg.pmch_scaling_factor_beta_num,
                                                                 cfg.pmch_scaling_factor_beta_den));

      switch (cfg.pmch_time_interleaving_n) {
        case 4:  tc.pmch_time_interleav_n_r19 = N::n4;  break;
        case 8:  tc.pmch_time_interleav_n_r19 = N::n8;  break;
        case 16: tc.pmch_time_interleav_n_r19 = N::n16; break;
        default: tc.pmch_time_interleav_n_r19 = N::n2;  break;
      }
      switch (cfg.pmch_time_interleaving_m) {
        case 8:  tc.pmch_time_interleav_m_r19 = M::sf8;  break;
        case 16: tc.pmch_time_interleav_m_r19 = M::sf16; break;
        case 32: tc.pmch_time_interleav_m_r19 = M::sf32; break;
        default: tc.pmch_time_interleav_m_r19 = M::sf4;  break;
      }

      /* pmch-TimeInterleavingN/M-LastMTCH-r19 (TS 36.331 CR5168r3): reconfigure_embms()
       * has already validated these against nof_mbms_sessions>1 and main N>1 (both
       * already guaranteed true in this block), so no further gating needed here
       * beyond checking each field is actually set (0 = absent/inherit main). The
       * two presence bits are independent — an operator may override just N, just
       * M, or both for the last of nof_mbms_sessions MTCH sessions. */
      using N_last = pmch_tfi_cfg_r19_s::time_interleav_cfg_r19_s_::pmch_time_interleav_n_last_mtch_r19_e_;
      if (cfg.pmch_time_interleaving_n_last_mtch > 0) {
        tc.pmch_time_interleav_n_last_mtch_r19_present = true;
        switch (cfg.pmch_time_interleaving_n_last_mtch) {
          case 1:  tc.pmch_time_interleav_n_last_mtch_r19 = N_last::n1;  break;
          case 4:  tc.pmch_time_interleav_n_last_mtch_r19 = N_last::n4;  break;
          case 8:  tc.pmch_time_interleav_n_last_mtch_r19 = N_last::n8;  break;
          case 16: tc.pmch_time_interleav_n_last_mtch_r19 = N_last::n16; break;
          default: tc.pmch_time_interleav_n_last_mtch_r19 = N_last::n2;  break;
        }
      }
      if (cfg.pmch_time_interleaving_m_last_mtch > 0) {
        tc.pmch_time_interleav_m_last_mtch_r19_present = true;
        switch (cfg.pmch_time_interleaving_m_last_mtch) {
          case 8:  tc.pmch_time_interleav_m_last_mtch_r19 = M::sf8;  break;
          case 16: tc.pmch_time_interleav_m_last_mtch_r19 = M::sf16; break;
          case 32: tc.pmch_time_interleav_m_last_mtch_r19 = M::sf32; break;
          default: tc.pmch_time_interleav_m_last_mtch_r19 = M::sf4;  break;
        }
      }

      if (cfg.pmch_cyclic_shift_alpha > 0) {
        tc.pmch_cyclic_shift_alpha_r19_present = true;
        switch (cfg.pmch_cyclic_shift_alpha) {
          case 1:  tc.pmch_cyclic_shift_alpha_r19 = A::alpha1; break;
          case 2:  tc.pmch_cyclic_shift_alpha_r19 = A::alpha2; break;
          default: tc.pmch_cyclic_shift_alpha_r19 = A::alpha3; break;
        }
      }
    } else if (cfg.pmch_cyclic_shift_alpha > 0) {
      logger.warning("pmch_cyclic_shift_alpha requires time_interleaving_n >= 2; cyclic shift ignored");
    }

    if (cfg.pmch_freq_interleaving) {
      if (!ext.pmch_tfi_cfg_r19_present) {
        ext.pmch_tfi_cfg_r19_present = true;
      }
      ext.pmch_tfi_cfg_r19.pmch_freq_interleav_r19_present = true;
      ext.pmch_tfi_cfg_r19.pmch_freq_interleav_r19 =
        pmch_tfi_cfg_r19_s::pmch_freq_interleav_r19_e_::enabled;
    }

    // Mirror session list from r9 PMCH
    ext.mbms_session_info_list_r19 = pmch_item->mbms_session_info_list_r9;

    logger.info("MCCH Phase 2 extension: cyclic_shift_alpha=%d freq_interleaving=%d time_interleaving_n=%d time_interleaving_m=%d "
                "time_interleaving_n_last_mtch=%d time_interleaving_m_last_mtch=%d use_mcs_table2=%d",
                cfg.pmch_cyclic_shift_alpha, (int)cfg.pmch_freq_interleaving,
                cfg.pmch_time_interleaving_n, cfg.pmch_time_interleaving_m,
                cfg.pmch_time_interleaving_n_last_mtch, cfg.pmch_time_interleaving_m_last_mtch,
                (int)cfg.pmch_use_mcs_table2);
    } // if (has_phase2) -- v1900 content
  } // if (has_phase2 || cfg.cell.mbms_dedicated) -- v1430/v1610 chain

  const int     rlc_header_len = 1;
  asn1::bit_ref bref(&mcch_payload_buffer[rlc_header_len], sizeof(mcch_payload_buffer) - rlc_header_len);
  if (mcch.pack(bref) != asn1::SRSASN_SUCCESS) {
    logger.error("Failed to pack MCCH message");
  }

  current_mcch_length = bref.distance_bytes(&mcch_payload_buffer[1]);
  current_mcch_length = current_mcch_length + rlc_header_len;
  return current_mcch_length;
}

/*******************************************************************************
  RRC run tti method
*******************************************************************************/

void rrc::tti_clock()
{
  // pop cmds from queue
  rrc_pdu p;
  while (rx_pdu_queue.try_pop(p)) {
    // check if user exists
    auto user_it = users.find(p.rnti);
    if (user_it == users.end()) {
      if (p.pdu != nullptr) {
        log_rx_pdu_fail(p.rnti, p.lcid, *p.pdu, "unknown rnti");
      } else {
        logger.warning("Ignoring rnti=0x%x command. Cause: unknown rnti", p.rnti);
      }
      continue;
    }
    ue& ue = *user_it->second;

    // handle queue cmd
    switch (p.lcid) {
      case srb_to_lcid(lte_srb::srb0):
        parse_ul_ccch(ue, std::move(p.pdu));
        break;
      case srb_to_lcid(lte_srb::srb1):
      case srb_to_lcid(lte_srb::srb2):
        parse_ul_dcch(ue, p.lcid, std::move(p.pdu));
        break;
      case LCID_REM_USER:
        rem_user(p.rnti);
        break;
      case LCID_REL_USER:
        process_release_complete(p.rnti);
        break;
      case LCID_ACT_USER:
        user_it->second->set_activity();
        break;
      case LCID_RADLINK_DL:
        user_it->second->set_radiolink_dl_state(p.arg);
        break;
      case LCID_RADLINK_UL:
        user_it->second->set_radiolink_ul_state(p.arg);
        break;
      case LCID_RLC_RTX:
        user_it->second->max_rlc_retx_reached();
        break;
      case LCID_PROT_FAIL:
        user_it->second->protocol_failure();
        break;
      case LCID_EXIT:
        logger.info("Exiting thread");
        break;
      default:
        logger.error("Rx PDU with invalid bearer id: %d", p.lcid);
        break;
    }
  }
}

void rrc::log_rx_pdu_fail(uint16_t rnti, uint32_t lcid, srsran::const_byte_span pdu, const char* cause_str)
{
  logger.error(
      pdu.data(), pdu.size(), "Rx %s PDU, rnti=0x%x - Discarding. Cause: %s", get_rb_name(lcid), rnti, cause_str);
}

void rrc::log_rxtx_pdu_impl(direction_t             dir,
                            uint16_t                rnti,
                            uint32_t                lcid,
                            srsran::const_byte_span pdu,
                            const char*             msg_type)
{
  static const char* dir_str[] = {"Rx", "Tx", "Tx S1AP", "Rx S1AP"};
  fmt::memory_buffer membuf;
  fmt::format_to(membuf, "{} ", dir_str[dir]);
  if (rnti != SRSRAN_PRNTI and rnti != SRSRAN_SIRNTI_MBMS_DEDICATED) {
    if (dir == Tx or dir == Rx) {
      fmt::format_to(membuf, "{} ", srsran::get_srb_name(srsran::lte_lcid_to_srb(lcid)));
    }
    fmt::format_to(membuf, "PDU, rnti=0x{:x} ", rnti);
  } else {
    fmt::format_to(membuf, "Broadcast PDU ");
  }
  fmt::format_to(membuf, "- {} ({} B)", msg_type, pdu.size());

  logger.info(pdu.data(), pdu.size(), "%s", srsran::to_c_str(membuf));
}

} // namespace srsenb
