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

// M3AP (TS 36.444), eNB side. This eNB acts as its own distributed MCE (TS
// 23.246 clause 5.9.1), so this is the client side of MME<->eNB direct M3AP
// signalling. Mirrors srsenb/hdr/stack/s1ap/s1ap.h's own SCTP-client shape
// (connect_mme()/setup pattern, socket_manager-based rx dispatch), scoped to
// the 4 procedures lib/include/srsran/asn1/m3ap.h implements. Unlike S1AP's
// s1setup_proc, M3 Setup here uses a plain retry timer rather than a full
// stack_procedure state machine -- there is no other concurrent RRC
// procedure it needs to interleave with, so that generality isn't needed.

#ifndef SRSENB_M3AP_H
#define SRSENB_M3AP_H

#include "srsran/asn1/m3ap.h"
#include "srsran/common/network_utils.h"
#include "srsran/common/task_scheduler.h"
#include "srsran/interfaces/enb_m3ap_interfaces.h"
#include "srsran/srslog/srslog.h"
#include <map>
#include <netinet/sctp.h>

namespace srsenb {

class m3ap
{
public:
  m3ap(srsran::task_sched_handle task_sched_, srslog::basic_logger& logger_, srsran::socket_manager_itf* rx_socket_handler_);

  int  init(const m3ap_args_t& args_, rrc_interface_m3ap* rrc_);
  void stop();

  bool handle_mce_rx_msg(srsran::unique_byte_buffer_t pdu, const sockaddr_in& from, const sctp_sndrcvinfo& sri, int flags);

private:
  // IANA-registered to "m3ap"/SCTP (verified against the IANA Service Name and Port Number Registry,
  // registered 2011-02-07); TS 36.444 itself does not mandate a port for M3.
  static const int MME_PORT_DEFAULT = 36444;

  rrc_interface_m3ap*         rrc = nullptr;
  m3ap_args_t                 args;
  srslog::basic_logger&       logger;
  srsran::task_sched_handle   task_sched;
  srsran::task_queue_handle   mme_task_queue;
  srsran::socket_manager_itf* rx_socket_handler;

  srsran::unique_socket mme_socket;
  struct sockaddr_in    mme_addr      = {};
  bool                  mme_connected = false;
  bool                  running       = false;
  srsran::unique_timer  mme_connect_timer, m3setup_timeout;

  // Per-session M3AP state, keyed by MME-MBMS-M3AP-ID (the id the MME allocated at Session Start -- Update/Stop
  // Requests only carry ids, not TMGI, so this is the only way to recover which session they refer to).
  struct m3ap_session_t {
    std::string tmgi_key;
    uint32_t    mce_mbms_m3ap_id;
  };
  std::map<uint32_t, m3ap_session_t> sessions;
  uint32_t                           next_mce_mbms_m3ap_id = 1;

  bool connect_mme();
  bool setup_m3();
  bool sctp_send_m3ap_pdu(const asn1::m3ap::m3ap_pdu_c& pdu, const char* procedure_name);

  bool handle_m3ap_rx_pdu(srsran::byte_buffer_t* pdu);
  bool handle_initiating_message(const asn1::m3ap::m3ap_pdu_c& pdu);
  bool handle_successful_outcome(const asn1::m3ap::m3ap_pdu_c& pdu);
  bool handle_unsuccessful_outcome(const asn1::m3ap::m3ap_pdu_c& pdu);

  bool handle_mbms_session_start_request(const asn1::m3ap::mbms_session_start_request_s& req);
  bool handle_mbms_session_update_request(const asn1::m3ap::mbms_session_update_request_s& req);
  bool handle_mbms_session_stop_request(const asn1::m3ap::mbms_session_stop_request_s& req);

  static std::string tmgi_key(const asn1::m3ap::tmgi_s& tmgi);
};

} // namespace srsenb

#endif // SRSENB_M3AP_H
