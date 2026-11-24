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

/******************************************************************************
 * File:        sbc.h
 * Description: SBc-AP interface (CBC <-> MME), the Public Warning System
 *              origination path. A real CBC connects here over SCTP and
 *              sends Write-Replace-Warning-Request/Stop-Warning-Request;
 *              this component forwards the warning to every connected eNB
 *              as a native S1AP WriteReplaceWarningRequest/KillRequest (that
 *              codec already exists in this codebase -- see s1ap.h), and
 *              acks the CBC immediately per TS 29.168 (does not wait for
 *              eNB responses before replying).
 *
 *              Mirrors s1ap.h's own SCTP listener (enb_listen()/s1ap_tx_pdu())
 *              structurally, and reuses s1ap::m_active_enbs the same way
 *              s1ap_paging::send_paging() already does to broadcast to every
 *              connected eNB.
 *
 *              Portal bridge: a second TCP (AF_INET/SOCK_STREAM) listener
 *              (mirroring rt-mbms-application-provider's own existing control pattern
 *              for srsenb, srsenb/src/control_server.cc: connect, write one
 *              already-encoded SBc-AP PDU, half-close the write side, read the
 *              response until the far end closes, then disconnect). Framing is
 *              done by connection lifetime (one request/response per
 *              connection) rather than message boundaries, and it binds to a
 *              configurable address:port so the portal can run in a separate
 *              container/host. NOTE: unauthenticated and able to inject
 *              emergency-alert PDUs -- bind to loopback or a trusted
 *              management network only. Added after a real end-to-end test found a genuine
 *              bug in the `sctp` npm package (sends a spurious ABORT right
 *              after receiving the SACK for its own data, confirmed via
 *              packet capture -- this MME side was never the problem). The
 *              portal still builds/encodes real SBc-AP PDUs with its own
 *              independent codec (verified byte-for-byte against this one);
 *              only the transport between portal and MME is local instead of
 *              real SCTP. Shares all decode/dispatch/forwarding logic with
 *              the real SCTP path via build_write_replace_warning_response()/
 *              build_stop_warning_response() below -- only the "how to send
 *              the reply" part differs.
 *****************************************************************************/

#ifndef SRSEPC_SBC_H
#define SRSEPC_SBC_H

#include "srsran/asn1/sbc_ap.h"
#include "srsran/common/byte_buffer.h"
#include "srsran/srslog/srslog.h"
// <netinet/in.h> must be pulled in (transitively, via the socket headers below) before
// <netinet/sctp.h> -- including sctp.h first makes its IPPROTO_SCTP macro clobber in.h's
// own enum-based definition of the same name, corrupting the enum's syntax.
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <string>

namespace srsepc {

typedef struct {
  std::string sbc_bind_addr    = "0.0.0.0";
  uint16_t    sbc_bind_port    = 29168; // Registered SCTP port for SBc-AP, per TS 29.168 clause 4 Annex A
  std::string bridge_bind_addr = "127.0.0.1"; // portal-facing TCP bridge; loopback by default (see file header)
  uint16_t    bridge_port      = 2102;        // TCP port the control portal connects to for SBc-AP
} sbc_args_t;

class sbc
{
public:
  static sbc* get_instance();
  static void cleanup();

  bool init(const sbc_args_t& args);
  void stop();
  int  get_sbc();
  int  get_bridge();

  void handle_sbc_rx_pdu(srsran::byte_buffer_t* pdu, struct sctp_sndrcvinfo* cbc_sri);
  void handle_bridge_connection();

private:
  sbc();
  ~sbc();
  static sbc* m_instance;

  int  cbc_listen();
  int  bridge_listen();
  bool sbc_tx_pdu(const asn1::sbc_ap::sbc_ap_pdu_c& pdu, struct sctp_sndrcvinfo* cbc_sri);

  // Decodes+dispatches an already-unpacked request, forwards to eNBs, and returns the response
  // PDU to send back -- transport-agnostic, shared by both handle_sbc_rx_pdu() (real SCTP) and
  // handle_bridge_connection() (local bridge).
  asn1::sbc_ap::sbc_ap_pdu_c
  build_write_replace_warning_response(const asn1::sbc_ap::write_replace_warning_request_s& req);
  asn1::sbc_ap::sbc_ap_pdu_c build_stop_warning_response(const asn1::sbc_ap::stop_warning_request_s& req);

  sbc_args_t m_sbc_args;
  int        m_sbc_fd    = -1;
  int        m_bridge_fd = -1;

  srslog::basic_logger& m_logger = srslog::fetch_basic_logger("SBC");
};

} // namespace srsepc

#endif // SRSEPC_SBC_H
