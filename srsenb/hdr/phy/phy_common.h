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

#ifndef SRSENB_PHCH_COMMON_H
#define SRSENB_PHCH_COMMON_H

#include "phy_interfaces.h"
#include <array>
#include "srsenb/hdr/phy/phy_ue_db.h"
#include "srsran/common/gen_mch_tables.h"
#include "srsran/common/interfaces_common.h"
#include "srsran/common/standard_streams.h"
#include "srsran/common/thread_pool.h"
#include "srsran/common/threads.h"
#include "srsran/interfaces/enb_metrics_interface.h"
#include "srsran/interfaces/phy_common_interface.h"
#include "srsran/interfaces/radio_interfaces.h"
#include "srsran/phy/channel/channel.h"
#include "srsran/phy/phch/pmch.h"
#include "srsran/radio/radio.h"

#include <map>
#include <srsran/common/tti_sempahore.h>
#include <string.h>

namespace srsenb {

class phy_common : public srsran::phy_common_interface
{
public:
  phy_common() = default;

  bool init(const phy_cell_cfg_list_t&    cell_list_,
            const phy_cell_cfg_list_nr_t& cell_list_nr_,
            srsran::radio_interface_phy*  radio_handler,
            stack_interface_phy_lte*      mac);
  void reset();
  void stop();

  /**
   * TTI transmission semaphore, used for ensuring that PHY workers transmit following start order
   */
  srsran::tti_semaphore<void*> semaphore;

  /**
   * Performs common end worker transmission tasks such as transmission and stack TTI execution
   *
   * @param tx_sem_id Semaphore identifier, the worker thread pointer is used
   * @param buffer baseband IQ sample buffer
   * @param tx_time timestamp to transmit samples
   * @param is_nr flag is true if it is called from NR
   */
  void worker_end(const worker_context_t& w_ctx, const bool& tx_enable, srsran::rf_buffer_t& buffer) override;

  // Common objects
  phy_args_t params = {};

  uint32_t get_nof_carriers_lte() { return static_cast<uint32_t>(cell_list_lte.size()); }
  uint32_t get_nof_carriers_nr() { return static_cast<uint32_t>(cell_list_nr.size()); }
  uint32_t get_nof_carriers() { return static_cast<uint32_t>(cell_list_lte.size() + cell_list_nr.size()); }
  uint32_t get_nof_prb(uint32_t cc_idx)
  {
    uint32_t ret = 0;

    if (cc_idx >= get_nof_carriers()) {
      // invalid CC index
      return ret;
    }

    if (cc_idx < cell_list_lte.size()) {
      ret = cell_list_lte[cc_idx].cell.nof_prb;
    } else if (cc_idx >= cell_list_lte.size()) {
      // offset CC index by all LTE carriers
      cc_idx -= cell_list_lte.size();
      if (cc_idx < cell_list_nr.size()) {
        ret = cell_list_nr[cc_idx].carrier.nof_prb;
      }
    }
    return ret;
  }
  uint32_t get_nof_ports(uint32_t cc_idx)
  {
    uint32_t ret = 0;

    if (cc_idx < cell_list_lte.size()) {
      ret = cell_list_lte[cc_idx].cell.nof_ports;
    } else if (cc_idx == 1 && !cell_list_nr.empty()) {
      // one RF port for basic NSA config
      ret = 1;
    }

    return ret;
  }
  uint8_t get_semi_static_cfi(uint32_t cc_idx)
  {
    uint8_t ret = 0;

    if (cc_idx < cell_list_lte.size()) {
      ret = cell_list_lte[cc_idx].cell.semi_static_cfi;
    }

    return ret;
  }
  /* See pmch_ti_tx_buf's doc comment (below, private section) for why every
   * cc_worker must share this same array rather than each holding its own. */
  uint8_t** get_pmch_ti_tx_buf() { return pmch_ti_tx_buf; }
  uint32_t get_nof_rf_channels()
  {
    uint32_t count = 0;

    for (auto& cell : cell_list_lte) {
      count += cell.cell.nof_ports;
    }

    for (auto& cell : cell_list_nr) {
      count += cell.carrier.max_mimo_layers;
    }

    return count;
  }
  double get_ul_freq_hz(uint32_t cc_idx)
  {
    double ret = 0.0;

    if (cc_idx < cell_list_lte.size()) {
      ret = cell_list_lte[cc_idx].ul_freq_hz;
    }

    cc_idx -= cell_list_lte.size();
    if (cc_idx < cell_list_nr.size()) {
      ret = cell_list_nr[cc_idx].ul_freq_hz;
    }

    return ret;
  }
  double get_dl_freq_hz(uint32_t cc_idx)
  {
    double ret = 0.0;

    if (cc_idx < cell_list_lte.size()) {
      ret = cell_list_lte[cc_idx].dl_freq_hz;
    }

    cc_idx -= cell_list_lte.size();
    if (cc_idx < cell_list_nr.size()) {
      ret = cell_list_nr[cc_idx].dl_freq_hz;
    }

    return ret;
  }
  uint32_t get_rf_port(uint32_t cc_idx)
  {
    uint32_t ret = 0;

    if (cc_idx < cell_list_lte.size()) {
      ret = cell_list_lte[cc_idx].rf_port;
    }

    cc_idx -= cell_list_lte.size();
    if (cc_idx < cell_list_nr.size()) {
      ret = cell_list_nr[cc_idx].rf_port;
    }

    return ret;
  }
  srsran_cell_t get_cell(uint32_t cc_idx)
  {
    srsran_cell_t c = {};
    if (cc_idx < cell_list_lte.size()) {
      c = cell_list_lte[cc_idx].cell;
    }
    return c;
  }

  void set_cell_gain(uint32_t cell_id, float gain_db)
  {
    // Find LTE cell
    auto it_lte = std::find_if(
        cell_list_lte.begin(), cell_list_lte.end(), [cell_id](phy_cell_cfg_t& x) { return x.cell_id == cell_id; });

    // Check if the lte cell was found;
    if (it_lte != cell_list_lte.end()) {
      std::lock_guard<std::mutex> lock(cell_gain_mutex);
      it_lte->gain_db = gain_db;
      return;
    }

    // Find NR cell
    auto it_nr = std::find_if(
        cell_list_nr.begin(), cell_list_nr.end(), [cell_id](phy_cell_cfg_nr_t& x) { return x.cell_id == cell_id; });

    // Check if the nr cell was found;
    if (it_nr != cell_list_nr.end()) {
      std::lock_guard<std::mutex> lock(cell_gain_mutex);
      it_nr->gain_db = gain_db;
      return;
    }

    srsran::console("cell ID %d not found\n", cell_id);
  }

  float get_cell_gain(uint32_t cc_idx)
  {
    std::lock_guard<std::mutex> lock(cell_gain_mutex);
    if (cc_idx < cell_list_lte.size()) {
      return cell_list_lte.at(cc_idx).gain_db;
    }

    cc_idx -= cell_list_lte.size();
    if (cc_idx < cell_list_nr.size()) {
      return cell_list_nr.at(cc_idx).gain_db;
    }

    return 0.0f;
  }

  /* Snapshot of the cell-wide CAS-muting/additionalNonMBSFNSubframes config, kept in sync with
   * cell_list_lte[0].cell via set_cell_cas_muting_cfg() below rather than only set once at PHY
   * startup -- see that method's doc comment for why. */
  struct cell_cas_muting_cfg_t {
    bool    cas_muting                 = false;
    uint8_t k_cas                      = 0;
    uint8_t n_cas                      = 0;
    uint8_t additional_non_mbms_frames = 0;
  };

  /**
   * Updates the live CAS-muting/additionalNonMBSFNSubframes config for the (single, FeMBMS-only
   * supports one) LTE cell, independent of cell_list_lte's other fields (nof_prb, mbsfn_prb,
   * etc.), which are set once at PHY startup from the boot-time config and never refreshed.
   * Before this method existed, a live eMBMS reconfigure (SIGHUP or the control socket) updated
   * only rrc::cfg.cell -- which drives what's *signalled* in SIB1/SIB13/MCCH -- while
   * is_mch_subframe()'s own scheduling-exclusion logic kept reading
   * cell_list_lte[0].cell.cas_muting/k_cas/n_cas/additional_non_mbms_frames, frozen at whatever
   * the startup config said: the eNB would tell UEs one CAS-muting pattern while actually
   * transmitting a different (or no) one. Called from rrc::configure_mbsfn_sibs() every time it
   * runs, including at startup (a harmless no-op re-write of the same boot-time values there).
   */
  void set_cell_cas_muting_cfg(bool cas_muting, uint8_t k_cas, uint8_t n_cas, uint8_t additional_non_mbms_frames)
  {
    if (cell_list_lte.empty()) {
      return;
    }
    std::lock_guard<std::mutex> lock(cell_cas_muting_mutex);
    cell_list_lte[0].cell.cas_muting                 = cas_muting;
    cell_list_lte[0].cell.k_cas                      = k_cas;
    cell_list_lte[0].cell.n_cas                      = n_cas;
    cell_list_lte[0].cell.additional_non_mbms_frames = additional_non_mbms_frames;
  }

  /** Thread-safe read of the live CAS-muting config set by set_cell_cas_muting_cfg() above.
   * Returns a default (cas_muting=false) snapshot if no LTE cell is configured yet, matching
   * is_mch_subframe()'s prior `!cell_list_lte.empty()` guards at each of its 4 read sites. */
  cell_cas_muting_cfg_t get_cell_cas_muting_cfg()
  {
    std::lock_guard<std::mutex> lock(cell_cas_muting_mutex);
    cell_cas_muting_cfg_t       cfg;
    if (!cell_list_lte.empty()) {
      cfg.cas_muting                 = cell_list_lte[0].cell.cas_muting;
      cfg.k_cas                      = cell_list_lte[0].cell.k_cas;
      cfg.n_cas                      = cell_list_lte[0].cell.n_cas;
      cfg.additional_non_mbms_frames = cell_list_lte[0].cell.additional_non_mbms_frames;
    }
    return cfg;
  }

  // Common Physical Uplink DMRS configuration
  srsran_refsignal_dmrs_pusch_cfg_t dmrs_pusch_cfg = {};

  srsran::radio_interface_phy* radio      = nullptr;
  stack_interface_phy_lte*     stack      = nullptr;
  srsran::channel_ptr          dl_channel = nullptr;

  /**
   * UE Database object, direct public access, all PHY threads should be able to access this attribute directly
   */
  phy_ue_db ue_db;

  void configure_mbsfn(srsran::phy_cfg_mbsfn_t* cfg);
  void build_mch_table();
  void build_mcch_table();
  bool is_mbsfn_sf(srsran_mbsfn_cfg_t* cfg, uint32_t phy_tti);
  void set_mch_period_stop(uint32_t stop);

  /* pmch-TimeInterleavingN/M-LastMTCH-r19 cross-layer channel (TS 36.331 CR5168r3):
   * mac.cc's get_mch_sched() calls the setter once per scheduling period (in the
   * same is_mcch branch that already calls set_mch_period_stop() above) to tell
   * is_mch_subframe() where, within this PMCH's own data region, the last of
   * nof_mbms_sessions MTCH sessions' window starts (0 = no distinct last-session
   * window this period, i.e. num_mtch_sched<=1 or no LastMTCH override configured
   * -- the single-session path must degenerate to today's flat counter exactly).
   * Deliberately a plain atomic write/read, not the blocking pthread_cond_wait
   * pattern set_mch_period_stop() above uses for mch_period_stop -- that consumer
   * is dead code (see is_mch_subframe()'s own comment), and reviving a blocking
   * wait here would risk stalling the PHY worker on a MAC-thread write that may
   * never come for cells that never use this feature. */
  void     set_last_mtch_start(uint8_t pmch_idx, uint32_t start_sf);
  uint32_t get_last_mtch_start(uint8_t pmch_idx) const;

  // Getters and setters for ul grants which need to be shared between workers
  const stack_interface_phy_lte::ul_sched_list_t get_ul_grants(uint32_t tti);
  void set_ul_grants(uint32_t tti, const stack_interface_phy_lte::ul_sched_list_t& ul_grants);
  void clear_grants(uint16_t rnti);

private:
  // Common objects for scheduling grants
  srsran::circular_array<stack_interface_phy_lte::ul_sched_list_t, TTIMOD_SZ> ul_grants   = {};
  std::mutex                                                                  grant_mutex = {};

  phy_cell_cfg_list_t    cell_list_lte;
  phy_cell_cfg_list_nr_t cell_list_nr;
  std::mutex             cell_gain_mutex;
  std::mutex             cell_cas_muting_mutex;

  bool                    have_mtch_stop   = false;
  pthread_mutex_t         mtch_mutex       = {};
  pthread_cond_t          mtch_cvar        = {};
  srsran::phy_cfg_mbsfn_t mbsfn            = {};
  bool                    sib13_configured = false;
  bool                    mcch_configured  = false;
  uint8_t                 mch_table[40]    = {};
  uint8_t                 mcch_table[10]   = {};
  uint32_t                mch_period_stop  = 0;
  /* Indexed by pmch_idx, sized to match mcch_msg_t::pmch_info_list's own capacity. */
  std::array<uint32_t, 15> last_mtch_start = {};
  mutable std::mutex       last_mtch_start_mutex;
  srsran::rf_buffer_t     tx_buffer        = {};
  bool                    is_mch_subframe(srsran_mbsfn_cfg_t* cfg, uint32_t phy_tti);
  bool                    is_mcch_subframe(srsran_mbsfn_cfg_t* cfg, uint32_t phy_tti);

  /* Shared, per-cell (not per-worker) cache of each PMCH time-interleaving
   * slot's raw TB payload, for srsran_pmch_encode()'s re-encode-from-cache
   * across a slot's own N subframes. srsran_pmch_t's own ti_tx_buf[] is
   * embedded by value inside cc_worker's per-thread srsran_enb_dl_t - one
   * instance per PHY worker thread. srsran::thread_pool assigns TTIs to
   * whichever worker finishes first (thread_pool::find_finished_worker() -
   * not a fixed tti%nof_workers formula), so a given slot m's successive
   * subframes (spaced M apart) can land on a different worker thread
   * whenever processing-time jitter causes a different worker to become
   * idle first - even when M == nof_phy_threads. Each worker's private
   * ti_tx_buf[m] would then see only some of slot m's subframes, silently
   * losing the cached n==0 payload for the rest. Allocated once here
   * (init_pmch_ti_tx_bufs(), called from configure_mbsfn() once cell
   * bandwidth is known) and shared by every cc_worker via encode_pmch(), so
   * the encode hot path never needs to allocate - and so never needs to
   * synchronize concurrent allocation. See the roadmap doc's "TX-side
   * ti_tx_buf per-worker fragmentation" design section for the full
   * rationale and why pinning worker assignment instead isn't viable
   * (thread_pool is generic infrastructure shared by every PHY channel,
   * not just PMCH). */
  uint8_t* pmch_ti_tx_buf[SRSRAN_PMCH_MAX_TI_M] = {};
  void     init_pmch_ti_tx_bufs();
  void     free_pmch_ti_tx_bufs();
};

} // namespace srsenb

#endif // SRSENB_PHCH_COMMON_H
