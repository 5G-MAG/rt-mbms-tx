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

#include "srsran/interfaces/rrc_interface_types.h"
#include "srsran/phy/common/phy_common.h"

#ifndef SRSRAN_ENB_PHY_INTERFACES_H
#define SRSRAN_ENB_PHY_INTERFACES_H

namespace srsenb {

/* Interface MAC -> PHY */
class phy_interface_mac_lte
{
public:
  /**
   * Removes an RNTI context from all the physical layer components, including secondary cells
   * @param rnti identifier of the user
   */
  virtual void rem_rnti(uint16_t rnti) = 0;

  /**
   *
   * @param stop
   */
  virtual void set_mch_period_stop(uint32_t stop) = 0;

  /**
   * pmch-TimeInterleavingN/M-LastMTCH-r19 (TS 36.331 CR5168r3) cross-layer channel:
   * tells the PHY, once per scheduling period, where (relative to this PMCH's own
   * data region) the last of several MTCH sessions' window starts, so PHY can use
   * a different N/M for just that window. 0 = no distinct last-session window this
   * period.
   * @param pmch_idx index into this cell's pmch_info_list
   * @param start_sf  0-based subframe offset, same convention as mch_subframe_idx
   */
  virtual void set_last_mtch_start(uint8_t pmch_idx, uint32_t start_sf) = 0;

  /**
   * Activates and/or deactivates Secondary Cells in the PHY for a given RNTI. Requires the RNTI of the given UE and a
   * vector with the activation/deactivation values. Use true for activation and false for deactivation. The index 0 is
   * reserved for PCell and will not be used.
   *
   * @param rnti identifier of the user
   * @param activation vector with the activate/deactivate.
   */
  virtual void set_activation_deactivation_scell(uint16_t                                     rnti,
                                                 const std::array<bool, SRSRAN_MAX_CARRIERS>& activation) = 0;
};

/* Interface RRC -> PHY */
class phy_interface_rrc_lte
{
public:
  srsran::phy_cfg_mbsfn_t mbsfn_cfg;

  virtual void configure_mbsfn(srsran::sib2_mbms_t* sib2, srsran::sib13_t* sib13, const srsran::mcch_msg_t& mcch) = 0;

  /**
   * Propagates the cell-wide CAS-muting/additionalNonMBSFNSubframes config to PHY's own live
   * cell state, independent of configure_mbsfn()'s SIB2/SIB13/MCCH content. Without this, a live
   * eMBMS reconfigure only changes what's *signalled* to UEs (via configure_mbsfn_sibs()'s SIB1/
   * SIB13 rebuild) while PHY's actual subframe-scheduling logic keeps using the cell config
   * snapshotted once at startup -- the eNB would tell UEs one CAS-muting pattern while actually
   * transmitting a different one. Called from rrc::configure_mbsfn_sibs() every time it runs
   * (including at startup, where it's a harmless no-op re-write of the same boot-time values).
   */
  virtual void set_cell_cas_muting_cfg(bool    cas_muting,
                                       uint8_t k_cas,
                                       uint8_t n_cas,
                                       uint8_t additional_non_mbsfn_subframes) = 0;

  struct phy_rrc_cfg_t {
    bool              configured = false; ///< Indicates whether PHY shall consider configuring this cell/carrier
    uint32_t          enb_cc_idx = 0;     ///< eNb Cell index
    srsran::phy_cfg_t phy_cfg    = {};    ///< Dedicated physical layer configuration
  };

  typedef std::vector<phy_rrc_cfg_t> phy_rrc_cfg_list_t;

  /**
   * Sets the physical layer dedicated configuration for a given RNTI. The dedicated configuration list shall provide
   * all the required information configuration for the following cases:
   * - Add an RNTI straight from RRC
   * - Moving primary to another serving cell
   * - Add/Remove secondary serving cells
   *
   * Remind this call will partially reconfigure the primary serving cell, `complete_config``shall be called
   * in order to complete the configuration.
   *
   * @param rnti the given RNTI
   * @param phy_cfg_list Physical layer configuration for the indicated eNb cell
   */
  virtual void set_config(uint16_t rnti, const phy_rrc_cfg_list_t& phy_cfg_list) = 0;

  /**
   * Instructs the physical layer the configuration has been complete from upper layers for a given RNTI
   *
   * @param rnti the given UE identifier (RNTI)
   */
  virtual void complete_config(uint16_t rnti) = 0;
};

// Combined interface for stack (MAC and RRC) to access PHY
class phy_interface_stack_lte : public phy_interface_mac_lte, public phy_interface_rrc_lte
{};

} // namespace srsenb

#endif // SRSRAN_ENB_PHY_INTERFACES_H
