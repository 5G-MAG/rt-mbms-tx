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

#ifndef SRSRAN_RRC_CONFIG_H
#define SRSRAN_RRC_CONFIG_H

#include "rrc_config_common.h"
#include "srsran/asn1/rrc.h"
#include "srsran/common/security.h"
#include "srsran/interfaces/enb_rrc_interface_types.h"
#include "srsran/phy/common/phy_common.h"
#include <array>
#include <string>

namespace srsenb {

struct rrc_cfg_sr_t {
  uint32_t                                                   period;
  asn1::rrc::sched_request_cfg_c::setup_s_::dsr_trans_max_e_ dsr_max;
  uint32_t                                                   nof_prb;
  uint32_t                                                   sf_mapping[80];
  uint32_t                                                   nof_subframes;
};

struct rrc_cfg_qci_t {
  bool                                          configured            = false;
  int                                           enb_dl_max_retx_thres = -1;
  asn1::rrc::lc_ch_cfg_s::ul_specific_params_s_ lc_cfg;
  asn1::rrc::pdcp_cfg_s                         pdcp_cfg;
  asn1::rrc::rlc_cfg_c                          rlc_cfg;
};

struct srb_cfg_t {
  int                                     enb_dl_max_retx_thres = -1;
  asn1::rrc::srb_to_add_mod_s::rlc_cfg_c_ rlc_cfg;
};

// Parameter required for NR cell measurement handling
struct rrc_endc_cfg_t {
  bool     act_from_b1_event;
  uint32_t abs_frequency_ssb;
  uint32_t nr_band;
  using ssb_nr_cfg = asn1::rrc::mtc_ssb_nr_r15_s;
  using ssb_rs_cfg = asn1::rrc::rs_cfg_ssb_nr_r15_s;
  ssb_nr_cfg::periodicity_and_offset_r15_c_ ssb_period_offset;
  ssb_nr_cfg::ssb_dur_r15_e_                ssb_duration;
  ssb_rs_cfg::subcarrier_spacing_ssb_r15_e_ ssb_ssc;
};

struct rrc_cfg_t {
  uint32_t enb_id; ///< Required to pack SIB1
  // Per eNB SIBs
  asn1::rrc::sib_type1_mbms_r14_s     sib1;
  asn1::rrc::sib_info_item_c sibs[ASN1_RRC_MAX_SIB];
  asn1::rrc::mac_main_cfg_s  mac_cnfg;

  asn1::rrc::pusch_cfg_ded_s                                                              pusch_cfg;
  asn1::rrc::ant_info_ded_s                                                               antenna_info;
  asn1::rrc::pdsch_cfg_ded_s::p_a_e_                                                      pdsch_cfg;
  rrc_cfg_sr_t                                                                            sr_cfg;
  rrc_cfg_cqi_t                                                                           cqi_cfg;
  std::map<uint32_t, rrc_cfg_qci_t>                                                       qci_cfg;
  bool                                                                                    enable_mbsfn;
  uint16_t                                                                                mbms_mcs;
  /* Rel-17 LTE_terr_bcast */
  uint8_t                                                                                 pmch_bandwidth;           /* 0=off, 30/35/40 PRBs (TS 36.331 pmch-Bandwidth-r17) */
  /* Rel-19 LTE_terr_bcast_Ph2 */
  uint8_t                                                                                 pmch_cyclic_shift_alpha;  /* 0=off, 1/2/3 */
  bool                                                                                    pmch_freq_interleaving;
  uint8_t                                                                                 pmch_time_interleaving_n; /* 0/1=off, 2/4/8/16 */
  uint8_t                                                                                 pmch_time_interleaving_m; /* 4/8/16/32 subframes per period */
  /* pmch-TimeInterleavingN/M-LastMTCH-r19 (TS 36.331 CR5168r3): 0=absent/inherit main */
  uint8_t                                                                                 pmch_time_interleaving_n_last_mtch;
  uint8_t                                                                                 pmch_time_interleaving_m_last_mtch;
  /* PMCH-SoftBufferSizeParameters-r19 (TS 36.212 §5.1.4.1.2 N_cb capping). Only
   * meaningful/signalled when pmch_time_interleaving_n > 1; see rrc.cc's pack_mcch(). */
  uint8_t                                                                                 pmch_n_soft_ref_category = 4; /* TS 36.306 Table 4.1-1 UE category */
  uint8_t                                                                                 pmch_scaling_factor_beta_num = 1;
  uint8_t                                                                                 pmch_scaling_factor_beta_den = 1;
  bool                                                                                    pmch_use_mcs_table2;
  uint8_t                                                                                 mch_sched_period_rf;          /* scheduling period in radio frames (default 64) */
  uint8_t                                                                                 nof_mbms_sessions;            /* number of MTCH sessions (default 1) */
  bool                                                                                    pmch_time_separation_sl2;     /* false=SL4 (default), true=SL2 (TS 36.211 §4.1) */
  std::string                                                                             pmch_subcarrier_spacing;      /* "" = derive from r9 SCS; "khz1dot25"/"khz2dot5"/"khz7dot5"/"khz0dot37" = override */
  uint32_t                                                                                inactivity_timeout_ms;
  std::array<srsran::CIPHERING_ALGORITHM_ID_ENUM, srsran::CIPHERING_ALGORITHM_ID_N_ITEMS> eea_preference_list;
  std::array<srsran::INTEGRITY_ALGORITHM_ID_ENUM, srsran::INTEGRITY_ALGORITHM_ID_N_ITEMS> eia_preference_list;
  bool                                                                                    meas_cfg_present = false;
  srsran_cell_t                                                                           cell;
  cell_list_t                                                                             cell_list;
  uint32_t  num_nr_cells = 0; /// number of configured NR cells (used to configure RF)
  uint32_t  max_mac_dl_kos;
  uint32_t  max_mac_ul_kos;
  uint32_t  rlf_release_timer_ms;
  srb_cfg_t srb1_cfg;
  srb_cfg_t srb2_cfg;
  rrc_endc_cfg_t endc_cfg;
  std::string    sib12_alert_file;  /* path to sib12_alert.conf for runtime SIGUSR1/SIGUSR2 emergency alerts */
  std::string    sib_tag_state_file; /* path to persist sys_info_value_tag_r14 across restarts */
};

constexpr uint32_t UE_PCELL_CC_IDX = 0;

struct ue_var_cfg_t {
  asn1::rrc::rr_cfg_ded_s                rr_cfg;
  asn1::rrc::meas_cfg_s                  meas_cfg;
  asn1::rrc::scell_to_add_mod_list_r10_l scells;
};

} // namespace srsenb

#endif // SRSRAN_RRC_CONFIG_H
