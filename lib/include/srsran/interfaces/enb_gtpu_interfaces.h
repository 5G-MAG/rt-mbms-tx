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

#ifndef SRSRAN_ENB_GTPU_INTERFACES_H
#define SRSRAN_ENB_GTPU_INTERFACES_H

#include "srsran/adt/expected.h"
#include "srsran/common/byte_buffer.h"
#include <string>
#include <vector>

namespace srsenb {

struct gtpu_args_t {
  std::string gtp_bind_addr;
  std::string mme_addr;
  std::string embms_m1u_multiaddr;
  std::string embms_m1u_if_addr;
  /* Comma-separated per-session TEIDs for M1-U demux (e.g. "0xAAAAAAAA,0xAAAAAAAB"),
   * index i -> LCID i+1. Empty = legacy single-bearer behavior. See m1u_handler. */
  std::string embms_session_teids;
  /* Same format/semantics as embms_session_teids above, but one entry per configured
   * extra PMCH (index i here == PMCH i+1's own session_teids, i.e. cfg.extra_pmch[i].
   * session_teids on the RRC side). Plain strings, not srsenb::pmch_cfg_t, since lib/
   * must not depend on a srsenb/-level header -- see m1u_handler::init(). */
  std::vector<std::string> embms_extra_session_teids;
  bool        embms_enable                 = false;
  uint32_t    indirect_tunnel_timeout_msec = 0;
};

// GTPU interface for PDCP
class gtpu_interface_pdcp
{
public:
  // PDCP will only know LCIDs, translation to EPS bearer id will be done by gtpu_pdcp_adapter
  virtual void write_pdu(uint16_t rnti, uint32_t lcid, srsran::unique_byte_buffer_t pdu) = 0;
};

// GTPU interface for RRC
class gtpu_interface_rrc
{
public:
  struct bearer_props {
    bool     forward_from_teidin_present = false;
    bool     flush_before_teidin_present = false;
    uint32_t forward_from_teidin         = 0;
    uint32_t flush_before_teidin         = 0;
  };

  virtual srsran::expected<uint32_t> add_bearer(uint16_t            rnti,
                                                uint32_t            eps_bearer_id,
                                                uint32_t            addr,
                                                uint32_t            teid_out,
                                                uint32_t&           addr_in,
                                                const bearer_props* props = nullptr)       = 0;
  virtual void                       set_tunnel_status(uint32_t teidin, bool dl_active)    = 0;
  virtual void                       rem_bearer(uint16_t rnti, uint32_t eps_bearer_id)     = 0;
  virtual void                       mod_bearer_rnti(uint16_t old_rnti, uint16_t new_rnti) = 0;
  virtual void                       rem_user(uint16_t rnti)                               = 0;
};

} // namespace srsenb

#endif // SRSRAN_ENB_GTPU_INTERFACES_H
