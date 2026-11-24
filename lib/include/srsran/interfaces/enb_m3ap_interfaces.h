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
#include <string>
#include <vector>

#ifndef SRSRAN_ENB_M3AP_INTERFACES_H
#define SRSRAN_ENB_M3AP_INTERFACES_H

namespace srsenb {

// This eNB acts as its own distributed MCE (TS 23.246 clause 5.9.1) -- no separate physical MCE box, so M3AP
// (TS 36.444) here means MME<->eNB directly, mirroring s1ap_args_t's shape.
struct m3ap_args_t {
  std::string mme_addr; // M3 peer (MME) address
  uint16_t    mme_m3_port; // see srsenb/hdr/stack/m3ap/m3ap.h's M3_PORT_DEFAULT note on why this is configurable
  std::string m3c_bind_addr;
  uint16_t    m3c_bind_port;
  uint32_t    enb_id;  // Global-MCE-ID reuses this eNB's own S1AP identity (mCE-ID ::= OCTET STRING(2))
  uint16_t    mcc;     // BCD-coded with 0xF filler, same convention as s1ap_args_t
  uint16_t    mnc;
  std::string mce_name;
  std::vector<uint16_t> mbms_service_area_ids;
};

class rrc_interface_m3ap
{
public:
  // Real per-session MBMS state driven by M3AP -- see srsenb/hdr/stack/rrc/rrc.h's own doc comment on these.
  // teid is the session's downlink M1-U GTP TEID (M3AP TNL-Information IE, tnl_info.gtp_dl_teid) -- 0 if
  // the session start request carried none, which RRC treats the same as "no TEID-based PMCH routing".
  virtual void mbms_session_start(const std::string&    tmgi_key,
                                  const srsran::tmgi_t& tmgi,
                                  uint8_t               session_id,
                                  bool                  session_id_present,
                                  uint32_t              teid) = 0;
  virtual void mbms_session_stop(const std::string& tmgi_key) = 0;
};

} // namespace srsenb

#endif // SRSRAN_ENB_M3AP_INTERFACES_H
