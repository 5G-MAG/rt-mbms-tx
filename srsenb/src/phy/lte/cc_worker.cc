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

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "srsran/common/threads.h"
#include "srsran/srsran.h"

#include "srsenb/hdr/phy/lte/cc_worker.h"

#define Error(fmt, ...)                                                                                                \
  if (SRSRAN_DEBUG_ENABLED)                                                                                            \
  logger.error(fmt, ##__VA_ARGS__)
#define Warning(fmt, ...)                                                                                              \
  if (SRSRAN_DEBUG_ENABLED)                                                                                            \
  logger.warning(fmt, ##__VA_ARGS__)
#define Info(fmt, ...)                                                                                                 \
  if (SRSRAN_DEBUG_ENABLED)                                                                                            \
  logger.info(fmt, ##__VA_ARGS__)
#define Debug(fmt, ...)                                                                                                \
  if (SRSRAN_DEBUG_ENABLED)                                                                                            \
  logger.debug(fmt, ##__VA_ARGS__)

using namespace std;

// Enable this to log SI
//#define LOG_THIS(a) 1

// Enable this one to skip SI-RNTI
#define LOG_THIS(rnti) (rnti != 0xFFFF)

/*********************************************/

using namespace asn1::rrc;

//#define DEBUG_WRITE_FILE

namespace srsenb {
namespace lte {

cc_worker::cc_worker(srslog::basic_logger& logger) : logger(logger)
{
  reset();
}

cc_worker::~cc_worker()
{
  srsran_softbuffer_tx_free(&temp_mbsfn_softbuffer);
  srsran_enb_dl_free(&enb_dl);
  srsran_enb_ul_free(&enb_ul);

  for (int p = 0; p < SRSRAN_MAX_PORTS; p++) {
    if (signal_buffer_rx[p]) {
      free(signal_buffer_rx[p]);
    }
    if (signal_buffer_tx[p]) {
      free(signal_buffer_tx[p]);
    }
  }

  // Delete all users
  for (auto& it : ue_db) {
    delete it.second;
  }
}

#ifdef DEBUG_WRITE_FILE
FILE* f;
#endif

void cc_worker::init(phy_common* phy_, uint32_t cc_idx_)
{
  phy                   = phy_;
  cc_idx                = cc_idx_;
  srsran_cell_t cell    = phy_->get_cell(cc_idx);
  uint32_t      nof_prb = phy_->get_nof_prb(cc_idx);
  uint32_t      sf_len  = SRSRAN_SF_LEN_PRB(nof_prb);
  // PMCH (the MBSFN/broadcast data channel) can be signalled wider than the
  // carrier itself (cell.mbsfn_prb, via pmch-Bandwidth-r17) for FeMBMS
  // extended coverage: a narrow CAS carrier with a wider PMCH allocation.
  // Every downlink buffer/FFT plan sized for the carrier alone would
  // overflow once PMCH's own RE-mapping (which already correctly uses
  // mbsfn_prb) exceeds it, so downlink allocations are sized from whichever
  // is wider. Uplink (signal_buffer_rx, enb_ul_*) has no MBSFN direction and
  // stays sized by the carrier's own nof_prb.
  uint32_t      dl_max_prb = std::max(nof_prb, static_cast<uint32_t>(cell.mbsfn_prb));
  uint32_t      dl_sf_len  = SRSRAN_SF_LEN_PRB(dl_max_prb);

  // Init cell here
  for (uint32_t p = 0; p < phy->get_nof_ports(cc_idx); p++) {
    signal_buffer_rx[p] = srsran_vec_cf_malloc(2 * sf_len);
    if (!signal_buffer_rx[p]) {
      ERROR("Error allocating memory");
      return;
    }
    srsran_vec_cf_zero(signal_buffer_rx[p], 2 * sf_len);
    signal_buffer_tx[p] = srsran_vec_cf_malloc(3 * 2 * dl_sf_len);
    if (!signal_buffer_tx[p]) {
      ERROR("Error allocating memory");
      return;
    }
    srsran_vec_cf_zero(signal_buffer_tx[p], 3 * 2 * dl_sf_len);
  }
  if (srsran_enb_dl_init(&enb_dl, signal_buffer_tx, dl_max_prb)) {
    ERROR("Error initiating ENB DL (cc=%d)", cc_idx);
    return;
  }
  if (srsran_enb_dl_set_cell(&enb_dl, cell)) {
    ERROR("Error initiating ENB DL (cc=%d)", cc_idx);
    return;
  }
  if (srsran_enb_dl_set_mbsfn_subcarrier_spacing(&enb_dl, SRSRAN_SCS_1KHZ25)) {
    ERROR("Error initiating ENB DL (cc=%d)", cc_idx);
    return;
  }
  if (srsran_enb_ul_init(&enb_ul, signal_buffer_rx[0], nof_prb)) {
    ERROR("Error initiating ENB UL");
    return;
  }

  if (srsran_enb_ul_set_cell(&enb_ul, cell, &phy->dmrs_pusch_cfg, nullptr)) {
    ERROR("Error initiating ENB UL");
    return;
  }

  /* Setup SI-RNTI in PHY -- value legally differs by cell type (TS 36.321 §7.1), matching the
   * same cell.mbms_dedicated branch already used for MIB packing and SIB1/SI-DCI generation. */
  add_rnti(cell.mbms_dedicated ? SRSRAN_SIRNTI_MBMS_DEDICATED : SRSRAN_SIRNTI);

  /* Setup P-RNTI in PHY */
  add_rnti(SRSRAN_PRNTI);

  /* Setup RA-RNTI in PHY */
  for (int i = SRSRAN_RARNTI_START; i <= SRSRAN_RARNTI_END; i++) {
    add_rnti(i);
  }

  if (srsran_softbuffer_tx_init(&temp_mbsfn_softbuffer, dl_max_prb)) {
    ERROR("Error initiating soft buffer");
    exit(-1);
  }

  srsran_softbuffer_tx_reset(&temp_mbsfn_softbuffer);

  Info("Component Carrier Worker %d configured cell %d PRB", cc_idx, nof_prb);

  if (phy->params.pusch_8bit_decoder) {
    enb_ul.pusch.llr_is_8bit        = true;
    enb_ul.pusch.ul_sch.llr_is_8bit = true;
  }
  initiated = true;

#ifdef DEBUG_WRITE_FILE
  f = fopen("test.dat", "w");
#endif
}

void cc_worker::reset()
{
  initiated = false;
  ue_db.clear();
}

cf_t* cc_worker::get_buffer_rx(uint32_t antenna_idx)
{
  return signal_buffer_rx[antenna_idx];
}

cf_t* cc_worker::get_buffer_tx(uint32_t antenna_idx)
{
  return signal_buffer_tx[antenna_idx];
}

void cc_worker::set_tti(uint32_t tti_)
{
  tti_rx    = tti_;
  tti_tx_dl = TTI_ADD(tti_rx, FDD_HARQ_DELAY_UL_MS);
  tti_tx_ul = TTI_RX_ACK(tti_rx);
}

int cc_worker::add_rnti(uint16_t rnti)
{
  std::unique_lock<std::mutex> lock(mutex);

  // Create user unless already exists
  if (ue_db.count(rnti) == 0) {
    ue_db[rnti] = new ue(rnti);
  }
  return SRSRAN_SUCCESS;
}

void cc_worker::rem_rnti(uint16_t rnti)
{
  std::lock_guard<std::mutex> lock(mutex);
  if (ue_db.count(rnti)) {
    delete ue_db[rnti];
    ue_db.erase(rnti);
  }
}

uint32_t cc_worker::get_nof_rnti()
{
  std::lock_guard<std::mutex> lock(mutex);
  return ue_db.size();
}

void cc_worker::work_ul(const srsran_ul_sf_cfg_t& ul_sf_cfg, stack_interface_phy_lte::ul_sched_t& ul_grants)
{
  std::lock_guard<std::mutex> lock(mutex);
  ul_sf = ul_sf_cfg;
  logger.set_context(ul_sf.tti);

  // Process UL signal
  srsran_enb_ul_fft(&enb_ul);

  // Decode pending UL grants for the tti they were scheduled
  decode_pusch(ul_grants.pusch, ul_grants.nof_grants);

  // Decode remaining PUCCH ACKs not associated with PUSCH transmission and SR signals
  decode_pucch();
}

void cc_worker::work_dl(const srsran_dl_sf_cfg_t&            dl_sf_cfg,
                        stack_interface_phy_lte::dl_sched_t& dl_grants,
                        stack_interface_phy_lte::ul_sched_t& ul_grants,
                        srsran_mbsfn_cfg_t*                  mbsfn_cfg)
{
  std::lock_guard<std::mutex> lock(mutex);
  dl_sf = dl_sf_cfg;
  /* Propagate the MBSFN SCS from the PHY config on the first MBSFN subframe.
   * Both the dl_sf SCS (used by encode_pmch/put_refs) and the enb_dl internal
   * pilot sequence SCS must match. The lazy update only fires when they diverge
   * (once, on the first MBSFN subframe after configure_mbsfn is called). */
  srsran_scs_t mbsfn_scs = SRSRAN_SCS_1KHZ25;
  if (dl_sf.sf_type == SRSRAN_SF_MBSFN && mbsfn_cfg != nullptr) {
    mbsfn_scs = mbsfn_cfg->subcarrier_spacing;
    if (mbsfn_scs != enb_dl.subcarrier_spacing) {
      srsran_enb_dl_set_mbsfn_subcarrier_spacing(&enb_dl, mbsfn_scs);
    }
    if (mbsfn_cfg->mbsfn_area_id != enb_dl.mbsfnr_signal.mbsfn_area_id) {
      srsran_enb_dl_set_mbsfn_area_id(&enb_dl, mbsfn_cfg->mbsfn_area_id);
    }
  }
  dl_sf.subcarrier_spacing = mbsfn_scs;

  // Put base signals (references, PBCH, PCFICH and PSS/SSS) into the resource grid
  srsran_enb_dl_put_base(&enb_dl, &dl_sf);

  // Put DL grants to resource grid. PDSCH data will be encoded as well.
  if (dl_sf_cfg.sf_type == SRSRAN_SF_NORM) {
    encode_pdcch_dl(dl_grants.pdsch, dl_grants.nof_grants);
    encode_pdsch(dl_grants.pdsch, dl_grants.nof_grants);
  } else {
    if (mbsfn_cfg->enable) {
      encode_pmch(dl_grants.pdsch, mbsfn_cfg);
    } else if (getenv("CAS_MUTE_DIAG")) {
      fprintf(stderr, "CAS_MUTE_DIAG_ENCPMCH tti=%u sf=%u SKIPPED mbsfn_cfg.enable=false\n",
              dl_sf.tti, dl_sf.tti % 10);
    }
  }

  // Put UL grants to resource grid(if necessary) 
   //   if (dl_sf->csubcarrier_spacing != SRSRAN_SCS_1KHZ25) {
   //     encode_pdcch_ul(ul_grants.pusch, ul_grants.nof_grants);
   //   }
  // Put pending PHICH HARQ ACK/NACK indications into subframe
  //encode_phich(ul_grants.phich, ul_grants.nof_phich);

  // Generate signal and transmit
  srsran_enb_dl_gen_signal(&enb_dl);

  // Scale if cell gain is set
  /*float cell_gain_db = phy->get_cell_gain(cc_idx);
  if (std::isnormal(cell_gain_db)) {
    float    scale  = srsran_convert_dB_to_amplitude(cell_gain_db);
    uint32_t sf_len = SRSRAN_SF_LEN_PRB(enb_dl.cell.nof_prb);
    for (uint32_t i = 0; i < enb_dl.cell.nof_ports; i++) {
      srsran_vec_sc_prod_cfc(signal_buffer_tx[i], scale, signal_buffer_tx[i], sf_len);
    }
  }*/
}

bool cc_worker::decode_pusch_rnti(stack_interface_phy_lte::ul_sched_grant_t& ul_grant,
                                  srsran_ul_cfg_t&                           ul_cfg,
                                  srsran_pusch_res_t&                        pusch_res)
{
  uint16_t rnti = ul_grant.dci.rnti;

  // Invalid RNTI
  if (rnti == SRSRAN_INVALID_RNTI) {
    return false;
  }

  // RNTI does not exist
  if (ue_db.count(rnti) == 0) {
    return false;
  }

  // Get UE configuration
  if (phy->ue_db.get_ul_config(rnti, cc_idx, ul_cfg) < SRSRAN_SUCCESS) {
    // It could happen that the UL configuration is missing due to intra-enb HO which is not an error
    Info("Failed retrieving UL configuration for cc=%d rnti=0x%x", cc_idx, rnti);
    return false;
  }

  // Fill UCI configuration
  bool uci_required =
      phy->ue_db.fill_uci_cfg(tti_rx, cc_idx, rnti, ul_grant.dci.cqi_request, true, ul_cfg.pusch.uci_cfg);

  // Compute UL grant
  srsran_pusch_grant_t& grant = ul_cfg.pusch.grant;
  if (srsran_ra_ul_dci_to_grant(&enb_ul.cell, &ul_sf, &ul_cfg.hopping, &ul_grant.dci, &grant)) {
    Error("Computing PUSCH dci for RNTI %x", rnti);
    return false;
  }

  // Handle Format0 adaptive retx
  // Use last TBS for this TB in case of mcs>28
  if (ul_grant.dci.tb.mcs_idx > 28) {
    int rv_idx = grant.tb.rv;
    if (phy->ue_db.get_last_ul_tb(rnti, cc_idx, ul_grant.pid, grant.tb) < SRSRAN_SUCCESS) {
      Error("Error retrieving last UL TB for RNTI %x, CC %d, PID %d", rnti, cc_idx, ul_grant.pid);
      return false;
    }
    grant.tb.rv = rv_idx;
    Info("Adaptive retx: rnti=0x%x, pid=%d, rv_idx=%d, mcs=%d, old_tbs=%d",
         rnti,
         ul_grant.pid,
         grant.tb.rv,
         ul_grant.dci.tb.mcs_idx,
         grant.tb.tbs / 8);
  }

  if (phy->ue_db.set_last_ul_tb(rnti, cc_idx, ul_grant.pid, grant.tb) < SRSRAN_SUCCESS) {
    Error("Error setting last UL TB for RNTI %x, CC %d, PID %d", rnti, cc_idx, ul_grant.pid);
  }

  // Run PUSCH decoder
  ul_cfg.pusch.softbuffers.rx = ul_grant.softbuffer_rx;
  pusch_res.data              = ul_grant.data;
  if (pusch_res.data) {
    if (srsran_enb_ul_get_pusch(&enb_ul, &ul_sf, &ul_cfg.pusch, &pusch_res)) {
      Error("Decoding PUSCH for RNTI %x", rnti);
      return false;
    }
  }
  // Save PHICH scheduling for this user. Each user can have just 1 PUSCH dci per TTI
  ue_db[rnti]->phich_grant.n_prb_lowest = grant.n_prb_tilde[0];
  ue_db[rnti]->phich_grant.n_dmrs       = ul_grant.dci.n_dmrs;

  float snr_db = enb_ul.chest_res.snr_db;

  // Notify MAC of RL status
  if (snr_db >= PUSCH_RL_SNR_DB_TH) {
    // Notify MAC UL channel quality
    phy->stack->snr_info(ul_sf.tti, rnti, cc_idx, snr_db, mac_interface_phy_lte::PUSCH);

    // Notify MAC of Time Alignment only if it enabled and valid measurement, ignore value otherwise
    if (ul_cfg.pusch.meas_ta_en and not std::isnan(enb_ul.chest_res.ta_us) and not std::isinf(enb_ul.chest_res.ta_us)) {
      phy->stack->ta_info(ul_sf.tti, rnti, enb_ul.chest_res.ta_us);
    }
  }

  // Send UCI data to MAC
  if (uci_required) {
    phy->ue_db.send_uci_data(tti_rx, rnti, cc_idx, ul_cfg.pusch.uci_cfg, pusch_res.uci);
  }

  // Save statistics only if data was provided
  if (ul_grant.data != nullptr) {
    // Save metrics stats
    ue_db[rnti]->metrics_ul(ul_grant.dci.tb.mcs_idx, 0, enb_ul.chest_res.snr_db, pusch_res.avg_iterations_block);
  }
  return true;
}

void cc_worker::decode_pusch(stack_interface_phy_lte::ul_sched_grant_t* grants, uint32_t nof_pusch)
{
  // Iterate over all the grants, all the grants need to report MAC the CRC status
  for (uint32_t i = 0; i < nof_pusch; i++) {
    // Get grant itself and RNTI
    stack_interface_phy_lte::ul_sched_grant_t& ul_grant = grants[i];
    uint16_t                                   rnti     = ul_grant.dci.rnti;

    srsran_pusch_res_t pusch_res = {};
    srsran_ul_cfg_t    ul_cfg    = {};

    // Decodes PUSCH for the given grant
    if (!decode_pusch_rnti(ul_grant, ul_cfg, pusch_res)) {
      return;
    }

    // Notify MAC new received data and HARQ Indication value
    if (ul_grant.data != nullptr) {
      // Inform MAC about the CRC result
      phy->stack->crc_info(tti_rx, rnti, cc_idx, ul_cfg.pusch.grant.tb.tbs / 8, pusch_res.crc);
      // Push PDU buffer
      phy->stack->push_pdu(
          tti_rx, rnti, cc_idx, ul_cfg.pusch.grant.tb.tbs / 8, pusch_res.crc, ul_cfg.pusch.grant.L_prb);
      // Logging
      if (logger.info.enabled()) {
        char str[512];
        srsran_pusch_rx_info(&ul_cfg.pusch, &pusch_res, &enb_ul.chest_res, str, sizeof(str));
        logger.info("PUSCH: cc=%d, %s", cc_idx, str);
      }
    }
  }
}

int cc_worker::decode_pucch()
{
  srsran_pucch_res_t pucch_res = {};

  for (auto& iter : ue_db) {
    uint16_t rnti = iter.first;

    // If it's a User RNTI and doesn't have PUSCH grant in this TTI
    if (SRSRAN_RNTI_ISUSER(rnti) and phy->ue_db.is_pcell(rnti, cc_idx)) {
      srsran_ul_cfg_t ul_cfg = {};

      if (phy->ue_db.get_ul_config(rnti, cc_idx, ul_cfg) < SRSRAN_SUCCESS) {
        Error("Error retrieving last UL configuration for RNTI %x, CC %d", rnti, cc_idx);
        continue;
      }

      // Check if user needs to receive PUCCH
      int ret = phy->ue_db.fill_uci_cfg(tti_rx, cc_idx, rnti, false, false, ul_cfg.pucch.uci_cfg);
      if (ret < SRSRAN_SUCCESS) {
        Error("Error retrieving UCI configuration for RNTI %x, CC %d", rnti, cc_idx);
        continue;
      }

      // If ret is more than success, UCI is present
      if (ret > SRSRAN_SUCCESS) {
        // Decode PUCCH
        if (srsran_enb_ul_get_pucch(&enb_ul, &ul_sf, &ul_cfg.pucch, &pucch_res)) {
          Error("Error getting PUCCH");
          continue;
        }

        // Send UCI data to MAC
        if (phy->ue_db.send_uci_data(tti_rx, rnti, cc_idx, ul_cfg.pucch.uci_cfg, pucch_res.uci_data) < SRSRAN_SUCCESS) {
          Error("Error sending UCI data for RNTI %x, CC %d", rnti, cc_idx);
          continue;
        }

        if (pucch_res.detected and pucch_res.ta_valid) {
          phy->stack->ta_info(tti_rx, rnti, pucch_res.ta_us);
          phy->stack->snr_info(tti_rx, rnti, cc_idx, pucch_res.snr_db, mac_interface_phy_lte::PUCCH);
        }

        // Logging
        if (logger.info.enabled()) {
          char str[512];
          srsran_pucch_rx_info(&ul_cfg.pucch, &pucch_res, str, sizeof(str));
          logger.info("PUCCH: cc=%d; %s", cc_idx, str);
        }

        // Save metrics
        ue_db[rnti]->metrics_ul_pucch(pucch_res.snr_db);
      }
    }
  }
  return 0;
}

int cc_worker::encode_phich(stack_interface_phy_lte::ul_sched_ack_t* acks, uint32_t nof_acks)
{
  for (uint32_t i = 0; i < nof_acks; i++) {
    if (acks[i].rnti && ue_db.count(acks[i].rnti)) {
      srsran_enb_dl_put_phich(&enb_dl, &ue_db[acks[i].rnti]->phich_grant, acks[i].ack);

      Info("PHICH: rnti=0x%x, hi=%d, I_lowest=%d, n_dmrs=%d, tti_tx_dl=%d",
           acks[i].rnti,
           acks[i].ack,
           ue_db[acks[i].rnti]->phich_grant.n_prb_lowest,
           ue_db[acks[i].rnti]->phich_grant.n_dmrs,
           tti_tx_dl);
    }
  }
  return SRSRAN_SUCCESS;
}

int cc_worker::encode_pdcch_ul(stack_interface_phy_lte::ul_sched_grant_t* grants, uint32_t nof_grants)
{
  for (uint32_t i = 0; i < nof_grants; i++) {
    if (grants[i].needs_pdcch) {
      srsran_dci_cfg_t dci_cfg = {};

      if (phy->ue_db.get_dci_ul_config(grants[i].dci.rnti, cc_idx, dci_cfg) < SRSRAN_SUCCESS) {
        Error("Error retrieving DCI UL configuration for RNTI %x, CC %d", grants[i].dci.rnti, cc_idx);
        continue;
      }

      if (SRSRAN_RNTI_ISUSER(grants[i].dci.rnti)) {
        if (srsran_enb_dl_location_is_common_ncce(&enb_dl, &grants[i].dci.location) &&
            phy->ue_db.is_pcell(grants[i].dci.rnti, cc_idx)) {
          // Disable extended CSI request and SRS request in common SS
          srsran_dci_cfg_set_common_ss(&dci_cfg);
        }
      }

      if (srsran_enb_dl_put_pdcch_ul(&enb_dl, &dci_cfg, &grants[i].dci)) {
        Error("Error putting PUSCH %d", i);
        return SRSRAN_ERROR;
      }

      // Logging
      if (logger.info.enabled()) {
        char str[512];
        srsran_dci_ul_info(&grants[i].dci, str, 512);
        logger.info("PDCCH: cc=%d, rnti=0x%x, %s, tti_tx_dl=%d", cc_idx, grants[i].dci.rnti, str, tti_tx_dl);
      }
    }
  }
  return SRSRAN_SUCCESS;
}

int cc_worker::encode_pdcch_dl(stack_interface_phy_lte::dl_sched_grant_t* grants, uint32_t nof_grants)
{
  for (uint32_t i = 0; i < nof_grants; i++) {
    uint16_t rnti = grants[i].dci.rnti;
    if (rnti) {
      srsran_dci_cfg_t dci_cfg = {};

      if (phy->ue_db.get_dci_dl_config(grants[i].dci.rnti, cc_idx, dci_cfg) < SRSRAN_SUCCESS) {
        Error("Error retrieving DCI DL configuration for RNTI %x, CC %d", grants[i].dci.rnti, cc_idx);
        continue;
      }

      // Detect if the DCI location is in common SS, if that is the case, flag it as common SS
      // This makes possible UE specific DCI fields to be disabled, so it uses a fallback DCI size
      if (SRSRAN_RNTI_ISUSER(grants[i].dci.rnti) && grants[i].dci.format == SRSRAN_DCI_FORMAT1A) {
        if (srsran_enb_dl_location_is_common_ncce(&enb_dl, &grants[i].dci.location) &&
            phy->ue_db.is_pcell(grants[i].dci.rnti, cc_idx)) {
          srsran_dci_cfg_set_common_ss(&dci_cfg);
        }
      }

      if (srsran_enb_dl_put_pdcch_dl(&enb_dl, &dci_cfg, &grants[i].dci)) {
        ERROR("Error putting PDCCH %d", i);
        return SRSRAN_ERROR;
      }

      if (LOG_THIS(rnti) and logger.info.enabled()) {
        // Logging
        char str[512];
        srsran_dci_dl_info(&grants[i].dci, str, 512);
        logger.info("PDCCH: cc=%d, rnti=0x%x, %s, tti_tx_dl=%d", cc_idx, grants[i].dci.rnti, str, tti_tx_dl);
      }
    }
  }
  return 0;
}

int cc_worker::encode_pmch(stack_interface_phy_lte::dl_sched_grant_t* grant, srsran_mbsfn_cfg_t* mbsfn_cfg)
{
  /* MAC sets dci.rnti=0 (mac.cc) when there is nothing to actually transmit
   * this subframe (e.g. mtch_stop==0, no MBMS traffic queued) - distinct from
   * grant->data[0]==NULL, which is also used intentionally for a time-
   * interleaving continuation subframe (slot_n>0) that must still encode the
   * slot's cached ti_tx_buf content, so data[0] alone can't be the guard.
   * Without this check, srsran_enb_dl_put_pmch() encoded a well-formed PMCH TB
   * from srsran_pmch_t's zeroed, never-primed ti_tx_buf[] regardless - RX,
   * decoding that all-zero payload successfully, saw its first MAC subheader
   * byte (0x00: E-bit=0, LCID=0) and misrouted it to the MCCH/RRC handler
   * (LCID 0 is MCCH's reserved bearer ID), corrupting Phy::_mcch until the
   * next real MCCH occasion. Mirrors encode_pdsch()'s existing
   * `rnti && ue_db.count(rnti)` gate for the same reason. */
  if (getenv("PMCH_TI_DIAG")) {
    fprintf(stderr, "TI_DIAG_ENCPMCH tti=%u rnti=0x%x data=%p subframe_idx=%u\n", dl_sf.tti, grant->dci.rnti,
            (void*)grant->data[0], mbsfn_cfg->mch_subframe_idx);
  }
  if (getenv("CAS_MUTE_DIAG")) {
    fprintf(stderr, "CAS_MUTE_DIAG_ENCPMCH tti=%u sf=%u rnti=0x%x data=%p mch_subframe_idx=%u is_mcch=%d\n",
            dl_sf.tti, dl_sf.tti % 10, grant->dci.rnti, (void*)grant->data[0], mbsfn_cfg->mch_subframe_idx,
            (int)mbsfn_cfg->is_mcch);
  }
  if (!grant->dci.rnti) {
    /* FIXED 2026-07-19: this early return left enb_dl.sf_symbols[0] holding
     * whatever the LAST real PMCH encode wrote there - srsran_enb_dl_gen_signal()
     * unconditionally IFFTs that buffer for every MBSFN subframe regardless of
     * whether fresh content was written this occasion, so an idle occasion
     * (no MAC grant) was retransmitting stale, real-looking-but-wrong-TB
     * content instead of silence. Confirmed live: at pmch_bandwidth=40, MAC
     * grants real data on ~0.16% of MBSFN subframes (content-rate-limited -
     * build_mch_sched() correctly drains the queue fast over a much bigger
     * per-subframe TBS and goes idle for the rest of the period), but the
     * receiver's own DTX/idle detection (MCHIDLE, power-threshold based) was
     * only catching ~52% of subframes as idle - the other ~48% looked like a
     * real, powered signal worth attempting to decode (and correctly failing
     * every time, since it's stale/wrong-TB, not this subframe's actual
     * content) - inflating BLER on subframes that were never really
     * scheduled. Zeroing the RE grid here makes an idle occasion transmit
     * genuine silence, so the receiver's existing power-based DTX detection
     * can correctly recognize it instead of attempting a doomed decode.
     *
     * CORRECTED 2026-07-20: ifft_mbsfn.nof_re is a PER-SYMBOL RE count (see
     * ofdm.c's single nof_re = cfg.nof_prb * SRSRAN_NRE_SCS(...) assignment),
     * not a per-subframe one - the original comment's "no more/less" claim was
     * wrong. ofdm_tx_slot_mbsfn() (the IFFT that actually reads this buffer)
     * loops total_syms = nof_symbols_mbsfn * SRSRAN_MBSFN_NOF_SLOTS(scs) times,
     * consuming nof_re samples EACH time (mirrors ue_dl_wide_sf_len()'s
     * identical total_syms computation on the RX side, used for the analogous
     * purpose there). Zeroing only nof_re left every symbol after the first
     * still holding stale content - for every SCS where total_syms>1 (i.e.
     * every SCS except 1.25kHz, where nof_symbols_mbsfn==SRSRAN_MBSFN_NOF_SLOTS
     * ==1 makes total_syms==1 and this bug was coincidentally invisible), most
     * of a stale PMCH TB would still be transmitted un-silenced on an idle
     * occasion. Zero the full total_syms*nof_re span instead. */
    uint32_t mbsfn_total_syms =
        enb_dl.ifft_mbsfn.nof_symbols_mbsfn * SRSRAN_MBSFN_NOF_SLOTS(enb_dl.ifft_mbsfn.cfg.subcarrier_spacing);
    srsran_vec_cf_zero(enb_dl.sf_symbols[0], enb_dl.ifft_mbsfn.nof_re * mbsfn_total_syms);
    /* REGRESSION FIX (same day): the zero-fill above also wipes the MBSFN
     * reference signals (MBSFN-RS) that enb_dl.c's put_refs() writes into this
     * same buffer independently of PMCH data - real FeMBMS/LTE requires those
     * pilots on EVERY MBSFN subframe regardless of whether user data is
     * present, specifically so receivers can maintain channel estimation
     * across idle occasions. Without this, the receiver's chest_res.ce[] goes
     * completely empty on muted subframes (confirmed live: CIR/CE dashboard
     * panels showed nothing but the -80dB floor). Re-write the pilots
     * immediately after the zero-fill, mirroring put_refs()'s exact call -
     * safe and idempotent regardless of whether put_refs() already ran this
     * subframe (before or after this function, either order). */
    {
      srsran_scs_t scs = dl_sf.subcarrier_spacing;
      uint32_t     sf_idx;
      if (SRSRAN_SCS_IS_370HZ(scs)) {
        uint32_t pos40 = dl_sf.tti % 40u;
        sf_idx = (pos40 > 0u) ? (pos40 - 1u) / 3u : 0u;
      } else {
        sf_idx = dl_sf.tti % 10u;
      }
      srsran_refsignal_mbsfn_put_sf(enb_dl.cell, 0, enb_dl.csr_signal.pilots[0][dl_sf.tti % 10u],
          enb_dl.mbsfnr_signal.pilots[0][sf_idx], enb_dl.sf_symbols[0], scs, dl_sf.tti);
    }
    return SRSRAN_SUCCESS;
  }
  srsran_pmch_cfg_t pmch_cfg;
  ZERO_OBJECT(pmch_cfg);
  srsran_configure_pmch(&pmch_cfg, &enb_dl.cell, mbsfn_cfg);
  srsran_ra_dl_compute_nof_re(&enb_dl.cell, &dl_sf, &pmch_cfg.pdsch_cfg.grant);

  pmch_cfg.cyclic_shift       = mbsfn_cfg->cyclic_shift;
  pmch_cfg.cyclic_shift_alpha = mbsfn_cfg->cyclic_shift_alpha;
  pmch_cfg.freq_interleaving  = mbsfn_cfg->freq_interleaving;
  pmch_cfg.subframe_idx       = mbsfn_cfg->mch_subframe_idx;

  // Set soft buffer
  pmch_cfg.pdsch_cfg.softbuffers.tx[0] = &temp_mbsfn_softbuffer;

  // Encode PMCH. Shared, cell-wide ti_tx_buf (not this worker's own
  // enb_dl.pmch.ti_tx_buf[]) - see phy_common::pmch_ti_tx_buf's doc comment
  // for why a per-worker buffer is unsafe with time interleaving active.
  if (srsran_enb_dl_put_pmch(&enb_dl, &pmch_cfg, grant->data[0], phy->get_pmch_ti_tx_buf())) {
    Error("Error putting PMCH");
    return SRSRAN_ERROR;
  }

  // Logging
  if (logger.info.enabled()) {
    char str[512];
    srsran_pdsch_tx_info(&pmch_cfg.pdsch_cfg, str, 512);
    logger.info("PMCH: %s", str);
  }

  // Save metrics stats
  if (ue_db.count(SRSRAN_MRNTI)) {
    ue_db[SRSRAN_MRNTI]->metrics_dl(mbsfn_cfg->mbsfn_mcs);
  }
  return SRSRAN_SUCCESS;
}

int cc_worker::encode_pdsch(stack_interface_phy_lte::dl_sched_grant_t* grants, uint32_t nof_grants)
{
  /* Scales the Resources Elements affected by the power allocation (p_b) */
  // srsran_enb_dl_prepare_power_allocation(&enb_dl);
  for (uint32_t i = 0; i < nof_grants; i++) {
    uint16_t rnti = grants[i].dci.rnti;

    if (rnti && ue_db.count(rnti)) {
      srsran_dl_cfg_t dl_cfg = {};

      if (phy->ue_db.get_dl_config(rnti, cc_idx, dl_cfg) < SRSRAN_SUCCESS) {
        Error("Error retrieving DCI DL configuration for RNTI %x, CC %d", grants[i].dci.rnti, cc_idx);
        continue;
      }

      // Compute DL grant
      if (srsran_ra_dl_dci_to_grant(
              &enb_dl.cell, &dl_sf, dl_cfg.tm, dl_cfg.pdsch.use_tbs_index_alt, &grants[i].dci, &dl_cfg.pdsch.grant)) {
        Error("Computing DL grant");
        continue;
      }

      // Set soft buffer
      for (uint32_t j = 0; j < SRSRAN_MAX_CODEWORDS; j++) {
        dl_cfg.pdsch.softbuffers.tx[j] = grants[i].softbuffer_tx[j];
      }

      // Encode PDSCH
      if (srsran_enb_dl_put_pdsch(&enb_dl, &dl_cfg.pdsch, grants[i].data)) {
        Error("Error putting PDSCH %d", i);
        return SRSRAN_ERROR;
      }

      // Save pending ACK
      if (SRSRAN_RNTI_ISUSER(rnti)) {
        // Push whole DCI
        phy->ue_db.set_ack_pending(tti_tx_ul, cc_idx, grants[i].dci);
      }

      if (LOG_THIS(rnti) and logger.info.enabled()) {
        // Logging
        char str[512];
        srsran_pdsch_tx_info(&dl_cfg.pdsch, str, 512);
        logger.info("PDSCH: cc=%d, %s, tti_tx_dl=%d", cc_idx, str, tti_tx_dl);
      }

      // Save metrics stats
      ue_db[rnti]->metrics_dl(grants[i].dci.tb[0].mcs_idx);
    } else {
      Error("User rnti=0x%x not found in cc_worker=%d", rnti, cc_idx);
    }
  }

  // srsran_enb_dl_apply_power_allocation(&enb_dl);

  return SRSRAN_SUCCESS;
}

/************ METRICS interface ********************/
uint32_t cc_worker::get_metrics(std::vector<phy_metrics_t>& metrics)
{
  std::lock_guard<std::mutex> lock(mutex);
  uint32_t                    cnt = 0;
  metrics.resize(ue_db.size());
  for (auto& ue : ue_db) {
    if ((SRSRAN_RNTI_ISUSER(ue.first) || ue.first == SRSRAN_MRNTI)) {
      ue.second->metrics_read(&metrics[cnt++]);
    }
  }
  metrics.resize(cnt);
  return cnt;
}

void cc_worker::ue::metrics_read(phy_metrics_t* metrics_)
{
  if (metrics_) {
    *metrics_ = metrics;
  }
  bzero(&metrics, sizeof(phy_metrics_t));
}

void cc_worker::ue::metrics_dl(uint32_t mcs)
{
  metrics.dl.mcs = SRSRAN_VEC_CMA(mcs, metrics.dl.mcs, metrics.dl.n_samples);
  metrics.dl.n_samples++;
}

void cc_worker::ue::metrics_ul(uint32_t mcs, float rssi, float sinr, float turbo_iters)
{
  metrics.ul.mcs         = SRSRAN_VEC_CMA((float)mcs, metrics.ul.mcs, metrics.ul.n_samples);
  metrics.ul.pusch_sinr  = SRSRAN_VEC_CMA((float)sinr, metrics.ul.pusch_sinr, metrics.ul.n_samples);
  metrics.ul.rssi        = SRSRAN_VEC_CMA((float)rssi, metrics.ul.rssi, metrics.ul.n_samples);
  metrics.ul.turbo_iters = SRSRAN_VEC_CMA((float)turbo_iters, metrics.ul.turbo_iters, metrics.ul.n_samples);
  metrics.ul.n_samples++;
}

void cc_worker::ue::metrics_ul_pucch(float sinr)
{
  metrics.ul.pucch_sinr = SRSRAN_VEC_CMA((float)sinr, metrics.ul.pucch_sinr, metrics.ul.n_samples_pucch);
  metrics.ul.n_samples_pucch++;
}

int cc_worker::read_ce_abs(float* ce_abs)
{
  int sz = srsran_symbol_sz(phy->get_nof_prb(cc_idx));
  srsran_vec_f_zero(ce_abs, sz);
  int g = (sz - SRSRAN_NRE * phy->get_nof_prb(cc_idx)) / 2;
  srsran_vec_abs_dB_cf(enb_ul.chest_res.ce, -80.0f, &ce_abs[g], SRSRAN_NRE * phy->get_nof_prb(cc_idx));
  return sz;
}

int cc_worker::read_ce_arg(float* ce_arg)
{
  int sz = srsran_symbol_sz(phy->get_nof_prb(cc_idx));
  srsran_vec_f_zero(ce_arg, sz);
  int g = (sz - SRSRAN_NRE * phy->get_nof_prb(cc_idx)) / 2;
  srsran_vec_arg_deg_cf(enb_ul.chest_res.ce, -80.0f, &ce_arg[g], SRSRAN_NRE * phy->get_nof_prb(cc_idx));
  return sz;
}

int cc_worker::read_pusch_d(cf_t* pdsch_d)
{
  int nof_re = enb_ul.pusch.max_re;
  memcpy(pdsch_d, enb_ul.pusch.d, nof_re * sizeof(cf_t));
  return nof_re;
}

int cc_worker::read_pucch_d(cf_t* pdsch_d)
{
  int nof_re = SRSRAN_PUCCH_MAX_BITS / 2;
  memcpy(pdsch_d, enb_ul.pucch.z_tmp, nof_re * sizeof(cf_t));
  return nof_re;
}

} // namespace lte
} // namespace srsenb
