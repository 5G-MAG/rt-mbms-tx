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

#include "srsran/common/byte_buffer.h"
#include "srsran/interfaces/pdcp_interface_types.h"
#include <map>

#ifndef SRSRAN_ENB_PDCP_INTERFACES_H
#define SRSRAN_ENB_PDCP_INTERFACES_H

namespace srsenb {

/* PDCP, GTP-U's own bearer table, and bearer_manager have no notion of "which PMCH" at
 * all -- they're flat maps keyed by a single "lcid"/"eps_bearer_id" integer (see
 * pdcp::pdcp_array_mrb, gtpu_interface_rrc::add_bearer(), bearer_manager::add_eps_bearer()).
 * RLC's own inner object, by contrast, genuinely needs a real mch_idx (rlc_array_mrb is
 * nested [mch_idx][lcid], and MAC's scheduler already tracks pmch_idx to read specific
 * (pmch, lcid) pairs -- see mac.cc's read_pdu_mch call site).
 *
 * Rather than threading a new mch_idx parameter through every one of PDCP/GTP-U/
 * bearer_manager's interfaces (which don't need to interpret it, only carry it around
 * as an opaque unique key), every MBMS bearer is registered and addressed by this single
 * composite value everywhere EXCEPT the two points that talk to RLC's inner, genuinely
 * mch_idx-aware object (srsenb::rlc's outer wrapper, which decomposes it back via
 * decompose_mch_lcid() right before calling in) -- see rrc.cc's add_user()/
 * configure_mbms_bearers() (registration) and gtpu.cc's m1u_handler (the write path)
 * for where it's composed.
 *
 * PMCH_LCID_STRIDE's ceiling, and the spec grounding behind it (verified against TS
 * 36.331 clause 6.3.7 before picking a number, not assumed): MBMS-SessionInfo-r9's own
 * logicalChannelIdentity-r9 is INTEGER(0..maxSessionPerPMCH-1) = 0..28 (maxSessionPerPMCH
 * =29, clause 6.4) -- a distinct ASN.1 field from the regular unicast DRB's
 * logicalChannelIdentity (clause 6.3.2, INTEGER(3..10)), not a narrower view of the same
 * one. gtpu_tunnel_manager::add_tunnel()/find_rnti_bearer_tunnels() (gtpu.cc) used to gate
 * eps_bearer_id through is_lte_rb() (common_lte.h, MAX_LTE_LCID=10) regardless of rnti --
 * an implementation bug, not a spec limit, confirmed live (composing pmch_idx=1 with
 * stride=16 produced composite=17, well within MBMS's real 0..28 range, but rejected with
 * "invalid eps-BearerID=17" and silently dropped) and fixed there (is_valid_eps_bearer_id(),
 * same file, exempts SRSRAN_MRNTI to the real 0..28 range instead).
 *
 * The remaining real ceiling after that fix is bearer_manager::add_eps_bearer()'s
 * eps_bearer_id parameter, genuinely uint8_t (0..255) -- appropriately narrow for regular
 * EPS bearers (TS 24.301's EBI is itself a small field) and not something to widen just
 * for MBMS's sake. stride=16 fits maxPMCH-PerMBSFN=15 PMCHs (pmch_idx 0..14) at up to 15
 * sessions each within that budget (14*16+15=239 < 255) -- short of MBMS's full 0..28
 * per-PMCH range, but already a superset of the 8-session cap used elsewhere in this same
 * codebase's fabricated-fallback loops, and configure_mbms_bearers() logs an explicit
 * error and skips (never silently drops) anything that would still exceed it. */
constexpr uint32_t PMCH_LCID_STRIDE = 16;
constexpr uint32_t PMCH_LCID_MAX_COMPOSITE = 255; // bearer_manager::add_eps_bearer()'s uint8_t eps_bearer_id

inline uint32_t compose_mch_lcid(uint32_t mch_idx, uint32_t lcid)
{
  return mch_idx * PMCH_LCID_STRIDE + lcid;
}

inline void decompose_mch_lcid(uint32_t composite, uint32_t& mch_idx, uint32_t& lcid)
{
  mch_idx = composite / PMCH_LCID_STRIDE;
  lcid    = composite % PMCH_LCID_STRIDE;
}

// PDCP interface for GTPU
class pdcp_interface_gtpu
{
public:
  virtual void write_sdu(uint16_t rnti, uint32_t lcid, srsran::unique_byte_buffer_t sdu, int pdcp_sn = -1) = 0;
  virtual std::map<uint32_t, srsran::unique_byte_buffer_t> get_buffered_pdus(uint16_t rnti, uint32_t lcid) = 0;
};

// PDCP interface for RRC
class pdcp_interface_rrc
{
public:
  virtual void set_enabled(uint16_t rnti, uint32_t lcid, bool enable)                                      = 0;
  virtual void reset(uint16_t rnti)                                                                        = 0;
  virtual void add_user(uint16_t rnti)                                                                     = 0;
  virtual void rem_user(uint16_t rnti)                                                                     = 0;
  virtual void write_sdu(uint16_t rnti, uint32_t lcid, srsran::unique_byte_buffer_t sdu, int pdcp_sn = -1) = 0;
  virtual void add_bearer(uint16_t rnti, uint32_t lcid, const srsran::pdcp_config_t& cnfg)                 = 0;
  virtual void del_bearer(uint16_t rnti, uint32_t lcid)                                                    = 0;
  virtual void config_security(uint16_t rnti, uint32_t lcid, const srsran::as_security_config_t& sec_cfg)  = 0;
  virtual void enable_integrity(uint16_t rnti, uint32_t lcid)                                              = 0;
  virtual void enable_encryption(uint16_t rnti, uint32_t lcid)                                             = 0;
  virtual void send_status_report(uint16_t rnti)                                                           = 0;
  virtual void send_status_report(uint16_t rnti, uint32_t lcid)                                            = 0;
  virtual bool get_bearer_state(uint16_t rnti, uint32_t lcid, srsran::pdcp_lte_state_t* state)             = 0;
  virtual bool set_bearer_state(uint16_t rnti, uint32_t lcid, const srsran::pdcp_lte_state_t& state)       = 0;
  virtual void reestablish(uint16_t rnti)                                                                  = 0;
};

// PDCP interface for RLC
class pdcp_interface_rlc
{
public:
  /* RLC calls PDCP to push a PDCP PDU. */
  virtual void write_pdu(uint16_t rnti, uint32_t lcid, srsran::unique_byte_buffer_t pdu)               = 0;
  virtual void notify_delivery(uint16_t rnti, uint32_t lcid, const srsran::pdcp_sn_vector_t& pdcp_sns) = 0;
  virtual void notify_failure(uint16_t rnti, uint32_t lcid, const srsran::pdcp_sn_vector_t& pdcp_sns)  = 0;
};

} // namespace srsenb

#endif // SRSRAN_ENB_PDCP_INTERFACES_H
