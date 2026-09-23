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

#ifndef SRSRAN_ENB_STACK_BASE_H
#define SRSRAN_ENB_STACK_BASE_H

#include "srsran/interfaces/enb_interfaces.h"
#include "srsran/interfaces/enb_m3ap_interfaces.h"
#include "srsran/interfaces/enb_mac_interfaces.h"
#include "srsran/interfaces/enb_s1ap_interfaces.h"
#include "srsue/hdr/stack/upper/gw.h"
#include <string>
#include <vector>

namespace srsenb {

/* Per-PMCH config for a PMCH *beyond* the first (PMCH0, which stays governed
 * by embms_args_t's own flat fields below, unchanged, for backward
 * compatibility). Added for TS 36.300 §15.3.3 compliance: PMCH0 always
 * carries MCCH and must never have time interleaving configured on it; a
 * time-interleaved MTCH session needs a *separate* PMCH, described by one of
 * these. Only the fields that are genuinely per-PMCH are here - area-wide
 * settings (cas_muting, k_cas/n_cas, m1u_*, additional_non_mbsfn_subframes,
 * etc.) stay on embms_args_t and apply to the whole MBSFN area regardless of
 * how many PMCHs it has. */
typedef struct {
  uint16_t    mcs                          = 9;
  uint8_t     pmch_bandwidth               = 0;
  uint8_t     cyclic_shift_alpha           = 0;
  bool        freq_interleaving            = false;
  uint8_t     time_interleaving_n          = 0;
  uint8_t     time_interleaving_m          = 0;
  uint8_t     time_interleaving_n_last_mtch = 0;
  uint8_t     time_interleaving_m_last_mtch = 0;
  uint16_t    n_soft_ref_category          = 4;
  std::string scaling_factor_beta;
  bool        use_mcs_table2               = false;
  uint8_t     mch_sched_period_rf          = 64;
  uint8_t     nof_mbms_sessions            = 1;
  bool        pmch_time_separation_sl2     = false;
  std::string pmch_subcarrier_spacing;
  /* This PMCH's own session TEIDs, same format/semantics as embms_args_t::session_teids
   * but scoped to just this PMCH's sessions. */
  std::string session_teids;
} pmch_cfg_t;

typedef struct {
  bool        enable;
  std::string filename;
} pcap_args_t;

typedef struct {
  bool        enable;
  std::string client_ip;
  std::string bind_ip;
  uint16_t    client_port;
  uint16_t    bind_port;
} pcap_net_args_t;

typedef struct {
  bool        enable;
  bool        mbms_dedicated;
  std::string m1u_multiaddr;
  std::string m1u_if_addr;
  uint16_t    mcs;
  bool        cas_muting;
  uint8_t     k_cas;
  uint8_t     n_cas;
  /* Rel-17 LTE_terr_bcast */
  uint8_t     pmch_bandwidth;        /* pmch-Bandwidth-r17: 0=off, 30/35/40 PRBs (TS 36.331 §6.3.1) */
  /* Rel-19 LTE_terr_bcast_Ph2 per-PMCH config */
  uint8_t     cyclic_shift_alpha;   /* 0 = disabled, 1/2/3 = alpha1/2/3 (TS 36.211 §6.5.1) */
  bool        freq_interleaving;    /* TS 36.211 §6.5.2 */
  uint8_t     time_interleaving_n;  /* NTimePMCH: 0/1 = disabled, 2/4/8/16 (TS 36.213 §11.1) */
  uint8_t     time_interleaving_m;  /* MTimePMCH: 4/8/16/32 subframes per period (TS 36.211 §6.5.3) */
  /* pmch-TimeInterleavingN/M-LastMTCH-r19 (TS 36.331 CR5168r3): lets the last MTCH
   * session in nof_mbms_sessions differ from the main N/M above. 0 = absent/inherit
   * main N (for N-last) or main M (for M-last); N-last also accepts 1 (n1 = disabled
   * for the last session only, the one value the main N field cannot express). */
  uint8_t     time_interleaving_n_last_mtch;
  uint8_t     time_interleaving_m_last_mtch;
  /* PMCH-SoftBufferSizeParameters-r19 (TS 36.212 §5.1.4.1.2 N_cb capping), mandatory
   * sibling of time_interleaving_n/m whenever time_interleaving_n > 1. Category is the
   * plain UE category number (TS 36.306 Table 4.1-1); scaling_factor_beta is the RRC
   * enum token name (one32nd/one5th/one3rd/three8th/five12th/onehalf/five8th/two3rd/
   * five6th/one), converted to a num/den fraction in enb_cfg_parser.cc. */
  uint16_t    n_soft_ref_category;  /* TS 36.306 UE DL category, default 4 */
  std::string scaling_factor_beta;  /* "" = default "one" (num=1,den=1) */
  bool        use_mcs_table2;       /* use TS 36.213 Table 11.1-2 (256QAM) */
  uint8_t     mch_sched_period_rf;       /* PMCH scheduling period in radio frames (4/8/16/32/64) */
  uint8_t     nof_mbms_sessions;         /* number of MBMS sessions (MTCH bearers) in the PMCH (1-8) */
  bool        pmch_time_separation_sl2;  /* false=SL4 (default), true=SL2 (TS 36.211 §4.1 timeSeparation) */
  std::string pmch_subcarrier_spacing;   /* "" (derive from sib.conf r9 SCS), or "khz1dot25"/"khz2dot5"/"khz7dot5"/"khz0dot37" */
  /* sf-AllocInfo-r16: 10-bit MCCH subframe allocation (TS 36.331 MBSFN-AreaInfo-r16), first bit = SF0.
   * The legacy r9 sf_alloc_info (sib.conf, BIT STRING(6)) can only place MCCH on SF{1,2,3,6,7,8};
   * a FeMBMS-dedicated cell may also use SF0/4/5/9, which only the r16 field can express. 0 (default)
   * = not set: derive the r16 field from the r9 config as before (no behavior change). >0 = use this
   * 10-bit value directly for the r16 sf-AllocInfo and the eNB's own MCCH subframe table. */
  uint16_t    sf_alloc_info_r16 = 0;
  uint8_t     additional_non_mbsfn_subframes; /* MIB-MBMS bits[9-10]: 0..3 non-MBSFN SFs after SF0 in active CAS frames (TS 36.331 §6.7.4.1) */
  /* Comma-separated per-session GTP-U TEIDs for M1-U demux (e.g. "0xAAAAAAAA,0xAAAAAAAB"),
   * index i (0-based) -> LCID i+1, matching rrc.cc's mbms_session_info_list[s].lc_ch_id.
   * Empty (default) = legacy behavior: every M1-U packet goes to a single fixed bearer
   * regardless of TEID. Must be kept consistent with the MBMS-GW's own per-session C-TEID
   * config -- this eNB has no M2/M3AP to learn the mapping dynamically. See gtpu.h's
   * m1u_handler. */
  std::string session_teids;
  /* Extra PMCHs beyond PMCH0 (empty = today's exact single-PMCH behavior).
   * See pmch_cfg_t above. PMCH0 is always the one carrying MCCH (TS 36.331:
   * "E-UTRAN configures mch-SchedulingPeriod of the (P)MCH listed first in
   * PMCH-InfoList to be smaller than or equal to mcch-RepetitionPeriod") -
   * that's not configurable and is enforced in rrc.cc's reconfigure_embms(). */
  std::vector<pmch_cfg_t> extra_pmch;
} embms_args_t;

typedef struct {
  std::string mac_level;
  std::string rlc_level;
  std::string pdcp_level;
  std::string rrc_level;
  std::string gtpu_level;
  std::string s1ap_level;
  std::string stack_level;

  int mac_hex_limit;
  int rlc_hex_limit;
  int pdcp_hex_limit;
  int rrc_hex_limit;
  int gtpu_hex_limit;
  int s1ap_hex_limit;
  int stack_hex_limit;
} stack_log_args_t;

typedef struct {
  uint32_t         sync_queue_size; // Max allowed difference between PHY and Stack clocks (in TTI)
  uint32_t         gtpu_indirect_tunnel_timeout_msec;
  mac_args_t       mac;
  s1ap_args_t      s1ap;
  m3ap_args_t      m3ap;
  pcap_args_t      mac_pcap;
  pcap_net_args_t  mac_pcap_net;
  pcap_args_t      s1ap_pcap;
  stack_log_args_t log;
  embms_args_t     embms;
} stack_args_t;

struct stack_metrics_t;

class enb_stack_base
{
public:
  virtual ~enb_stack_base() = default;

  virtual std::string get_type() = 0;

  virtual void stop() = 0;

  virtual void toggle_padding() = 0;
  // eNB metrics interface
  virtual bool get_metrics(stack_metrics_t* metrics) = 0;

  virtual void tti_clock() = 0;

  virtual void reload_embms_config(uint8_t            pmch_bandwidth,
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
                                   const std::string& subcarrier_spacing,
                                   const std::vector<pmch_cfg_t>& extra_pmch = {}) {}

  virtual void reload_sib12(bool activate) {}
};

} // namespace srsenb

#endif // SRSRAN_ENB_STACK_BASE_H
