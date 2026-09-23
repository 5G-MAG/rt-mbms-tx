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

#include "srsenb/hdr/stack/m3ap/m3ap.h"
#include "srsran/common/bcd_helpers.h"
#include "srsran/common/standard_streams.h"
#include <cerrno>
#include <cstring>
#include <unistd.h>

using namespace asn1::m3ap;

namespace srsenb {

m3ap::m3ap(srsran::task_sched_handle   task_sched_,
           srslog::basic_logger&       logger_,
           srsran::socket_manager_itf* rx_socket_handler_) :
  logger(logger_), task_sched(task_sched_), rx_socket_handler(rx_socket_handler_)
{
  mme_task_queue = task_sched.make_task_queue();
}

int m3ap::init(const m3ap_args_t& args_, rrc_interface_m3ap* rrc_)
{
  rrc  = rrc_;
  args = args_;

  mme_connect_timer = task_sched.get_unique_timer();
  /* mme_connect_timer is one-shot (srsran::unique_timer has no periodic mode) - without
   * re-arming here on failure, a SECOND failed connection attempt (the one this very timer
   * triggers) would silently give up on M3AP forever, never retrying again. */
  auto mme_connect_run = [this](uint32_t tid) {
    if (!connect_mme()) {
      logger.warning("Failed to connect to MME for M3, will retry");
      mme_connect_timer.run();
    }
  };
  mme_connect_timer.set(10000, mme_connect_run);

  m3setup_timeout = task_sched.get_unique_timer();
  m3setup_timeout.set(5000, [this](uint32_t tid) {
    logger.warning("M3 Setup timed out, retrying");
    srsran::console("M3 Setup timed out, retrying\n");
    // Deregister before closing: unique_socket::close() resets fd() to -1,
    // and this fd is already registered with rx_socket_handler (from
    // connect_mme()'s add_socket_handler() call) -- closing it first, or not
    // removing it at all, leaves a stale, already-closed fd registered
    // forever, which makes every subsequent select() in the socket manager's
    // run_thread fail with EBADF until something else happens to overwrite
    // that fd slot.
    rx_socket_handler->remove_socket(mme_socket.fd());
    mme_socket.close();
    mme_connected = false;
    mme_connect_timer.run();
  });

  running = true;
  if (!connect_mme()) {
    logger.warning("Failed to connect to MME for M3, will retry");
    mme_connect_timer.run();
  }

  return SRSRAN_SUCCESS;
}

void m3ap::stop()
{
  running = false;
  rx_socket_handler->remove_socket(mme_socket.fd());
  mme_socket.close();
}

bool m3ap::connect_mme()
{
  using namespace srsran::net_utils;
  logger.info("Connecting to MME %s:%d for M3", args.mme_addr.c_str(), args.mme_m3_port ? args.mme_m3_port : MME_PORT_DEFAULT);

  if (not srsran::net_utils::sctp_init_socket(
          &mme_socket, socket_type::seqpacket, args.m3c_bind_addr.c_str(), args.m3c_bind_port)) {
    return false;
  }

  /* Every failure path below must close mme_socket before returning false: sctp_init_socket()
   * above already opened it, and the next mme_connect_timer-triggered retry calls back into this
   * same function, whose own sctp_init_socket() call refuses to re-init an already-open socket
   * ("Socket is already open") - silently breaking every retry after the first failure. */
  if (not mme_socket.connect_to(args.mme_addr.c_str(), args.mme_m3_port ? args.mme_m3_port : MME_PORT_DEFAULT, &mme_addr)) {
    mme_socket.close();
    return false;
  }
  logger.info("SCTP socket connected with MME for M3. fd=%d", mme_socket.fd());

  auto rx_callback =
      [this](srsran::unique_byte_buffer_t pdu, const sockaddr_in& from, const sctp_sndrcvinfo& sri, int flags) {
        handle_mce_rx_msg(std::move(pdu), from, sri, flags);
      };
  rx_socket_handler->add_socket_handler(mme_socket.fd(),
                                        srsran::make_sctp_sdu_handler(logger, mme_task_queue, rx_callback));

  if (!setup_m3()) {
    rx_socket_handler->remove_socket(mme_socket.fd());
    mme_socket.close();
    return false;
  }
  m3setup_timeout.run();
  return true;
}

bool m3ap::setup_m3()
{
  m3ap_pdu_c pdu;
  auto&      req = pdu.set_init_msg_m3_setup_request();

  uint32_t plmn = 0;
  srsran::s1ap_mccmnc_to_plmn(args.mcc, args.mnc, &plmn);
  req.global_mce_id.plmn_id.from_number(plmn);
  // MCE-ID (2 octets): reuses this eNB's own S1AP enb_id, since this project has no separate MCE identity space.
  req.global_mce_id.mce_id[0] = (uint8_t)((args.enb_id >> 8) & 0xFF);
  req.global_mce_id.mce_id[1] = (uint8_t)(args.enb_id & 0xFF);

  if (!args.mce_name.empty()) {
    req.mce_name_present = true;
    req.mce_name.from_string(args.mce_name);
  }

  if (args.mbms_service_area_ids.empty()) {
    logger.error("M3 Setup: no MBMS Service Area configured, cannot send M3SetupRequest (mandatory IE)");
    return false;
  }
  req.mbms_service_area_list.resize(args.mbms_service_area_ids.size());
  for (uint32_t i = 0; i < args.mbms_service_area_ids.size(); i++) {
    req.mbms_service_area_list[i].from_number(args.mbms_service_area_ids[i]);
  }

  return sctp_send_m3ap_pdu(pdu, "M3SetupRequest");
}

bool m3ap::sctp_send_m3ap_pdu(const m3ap_pdu_c& pdu, const char* procedure_name)
{
  srsran::unique_byte_buffer_t buf = srsran::make_byte_buffer();
  if (buf == nullptr) {
    logger.error("Fatal Error: Couldn't allocate buffer for %s.", procedure_name);
    return false;
  }
  asn1::bit_ref bref(buf->msg, buf->get_tailroom());
  if (pdu.pack(bref) != asn1::SRSASN_SUCCESS) {
    logger.error("Could not pack %s.", procedure_name);
    return false;
  }
  buf->N_bytes = bref.distance_bytes();

  /* sctp_sendmsg() with an explicit destination (mme_addr, populated by connect_to()'s own
   * out-param), mirroring s1ap::sctp_send_s1ap_pdu()'s identical call exactly. A one-to-many
   * (SOCK_SEQPACKET) SCTP socket's connect() does NOT necessarily leave an implicit "default
   * peer" the way a one-to-one (SOCK_STREAM) socket's does - sending via plain sctp_send() with
   * no destination (msg_name=NULL) was silently relying on that assumption and got EPIPE on
   * every attempt (confirmed via strace: same behavior across many independent connection
   * attempts, never transient), while s1ap's explicit-destination sctp_sendmsg() over the exact
   * same connect()-then-send pattern, same process, same kernel, always works. */
  ssize_t n_sent = sctp_sendmsg(mme_socket.fd(),
                                buf->msg,
                                buf->N_bytes,
                                (struct sockaddr*)&mme_addr,
                                sizeof(struct sockaddr_in),
                                htonl(PPID),
                                0,
                                0,
                                0,
                                0);
  if (n_sent == -1) {
    logger.error("Failed to send %s. Error: %s", procedure_name, strerror(errno));
    return false;
  }
  return true;
}

bool m3ap::handle_mce_rx_msg(srsran::unique_byte_buffer_t pdu,
                             const sockaddr_in&           from,
                             const sctp_sndrcvinfo&       sri,
                             int                          flags)
{
  if (flags & MSG_NOTIFICATION) {
    union sctp_notification* notification = (union sctp_notification*)pdu->msg;
    logger.info("M3 SCTP Notification %04x", notification->sn_header.sn_type);
    if (getenv("M3AP_SCTP_DIAG")) {
      if (notification->sn_header.sn_type == SCTP_REMOTE_ERROR) {
        fprintf(stderr, "M3AP_SCTP_DIAG REMOTE_ERROR error_cause=0x%04x length=%u assoc=%d\n",
                notification->sn_remote_error.sre_error, notification->sn_remote_error.sre_length,
                notification->sn_remote_error.sre_assoc_id);
      } else if (notification->sn_header.sn_type == SCTP_ASSOC_CHANGE) {
        fprintf(stderr, "M3AP_SCTP_DIAG ASSOC_CHANGE state=%u error=0x%04x assoc=%d\n",
                notification->sn_assoc_change.sac_state, notification->sn_assoc_change.sac_error,
                notification->sn_assoc_change.sac_assoc_id);
      } else if (notification->sn_header.sn_type == SCTP_SEND_FAILED) {
        fprintf(stderr, "M3AP_SCTP_DIAG SEND_FAILED error=0x%04x\n", notification->sn_send_failed.ssf_error);
      }
    }
    if (notification->sn_header.sn_type == SCTP_SHUTDOWN_EVENT ||
        (notification->sn_header.sn_type == SCTP_PEER_ADDR_CHANGE &&
         notification->sn_paddr_change.spc_state == SCTP_ADDR_UNREACHABLE)) {
      logger.info("M3 SCTP association lost, will reconnect");
      srsran::console("M3 SCTP association lost, will reconnect\n");
      // remove_socket() BEFORE close(): unique_socket::close() resets fd()
      // to -1, so calling it first means remove_socket(mme_socket.fd())
      // below removes fd=-1 (a silent no-op) instead of the real, still-
      // registered fd -- leaving that fd stuck in the socket manager's
      // total_fd_set even though its underlying OS descriptor is already
      // closed, which makes every select() call in its run_thread fail with
      // EBADF until something else happens to overwrite that fd slot.
      rx_socket_handler->remove_socket(mme_socket.fd());
      mme_socket.close();
      mme_connected = false;
      mme_connect_timer.run();
    }
    return true;
  }
  return handle_m3ap_rx_pdu(pdu.get());
}

bool m3ap::handle_m3ap_rx_pdu(srsran::byte_buffer_t* pdu)
{
  m3ap_pdu_c     rx_pdu;
  asn1::cbit_ref bref(pdu->msg, pdu->N_bytes);
  if (rx_pdu.unpack(bref) != asn1::SRSASN_SUCCESS) {
    logger.error("Failed to unpack received M3AP PDU");
    return false;
  }

  switch (rx_pdu.msg_type().value) {
    case msg_type_opts::init_msg:
      return handle_initiating_message(rx_pdu);
    case msg_type_opts::successful_outcome:
      return handle_successful_outcome(rx_pdu);
    case msg_type_opts::unsuccessful_outcome:
      return handle_unsuccessful_outcome(rx_pdu);
    default:
      logger.warning("Unhandled M3AP PDU type %d", (int)rx_pdu.msg_type().value);
      return false;
  }
}

bool m3ap::handle_initiating_message(const m3ap_pdu_c& pdu)
{
  switch (pdu.proc_code()) {
    case proc_code_mbms_session_start:
      return handle_mbms_session_start_request(pdu.mbms_session_start_request());
    case proc_code_mbms_session_update:
      return handle_mbms_session_update_request(pdu.mbms_session_update_request());
    case proc_code_mbms_session_stop:
      return handle_mbms_session_stop_request(pdu.mbms_session_stop_request());
    default:
      logger.warning("Unhandled M3AP initiating message, procedure code %d", pdu.proc_code());
      return false;
  }
}

bool m3ap::handle_successful_outcome(const m3ap_pdu_c& pdu)
{
  if (pdu.proc_code() == proc_code_m3_setup) {
    logger.info("Received M3 Setup Response, M3 association established");
    srsran::console("Received M3 Setup Response, M3 association established\n");
    mme_connected = true;
    m3setup_timeout.stop();
    return true;
  }
  logger.warning("Unhandled M3AP successful outcome, procedure code %d", pdu.proc_code());
  return false;
}

bool m3ap::handle_unsuccessful_outcome(const m3ap_pdu_c& pdu)
{
  if (pdu.proc_code() == proc_code_m3_setup) {
    const m3_setup_fail_s& fail = pdu.m3_setup_fail();
    logger.error("M3 Setup Failure. Cause: %s", fail.cause.to_string());
    srsran::console("M3 Setup Failure. Cause: %s\n", fail.cause.to_string());
    m3setup_timeout.stop();
    // See the M3-association-lost handler above for why remove_socket()
    // must come before close().
    rx_socket_handler->remove_socket(mme_socket.fd());
    mme_socket.close();
    mme_connected = false;
    mme_connect_timer.run();
    return true;
  }
  logger.warning("Unhandled M3AP unsuccessful outcome, procedure code %d", pdu.proc_code());
  return false;
}

std::string m3ap::tmgi_key(const tmgi_s& tmgi)
{
  uint32_t plmn = ((uint32_t)tmgi.plmn_id[0] << 16) | ((uint32_t)tmgi.plmn_id[1] << 8) | tmgi.plmn_id[2];
  uint16_t mcc, mnc;
  srsran::s1ap_plmn_to_mccmnc(plmn, &mcc, &mnc);
  uint32_t service_id =
      ((uint32_t)tmgi.service_id[0] << 16) | ((uint32_t)tmgi.service_id[1] << 8) | tmgi.service_id[2];
  char buf[64];
  snprintf(buf, sizeof(buf), "%04x:%04x:%06x", mcc, mnc, service_id & 0xFFFFFF);
  return std::string(buf);
}

// Converts a wire-format M3AP TMGI into the common srsran::tmgi_t the RRC session table stores (see
// srsenb/hdr/stack/rrc/rrc.h's mbms_sessions). Always uses explicit_value (not plmn_idx into the cell's own
// broadcast PLMN-IdentityList) -- see the matching note in rrc.cc's pack_mcch().
static srsran::tmgi_t to_tmgi_t(const tmgi_s& tmgi)
{
  uint32_t plmn = ((uint32_t)tmgi.plmn_id[0] << 16) | ((uint32_t)tmgi.plmn_id[1] << 8) | tmgi.plmn_id[2];
  uint16_t mcc_bcd, mnc_bcd;
  srsran::s1ap_plmn_to_mccmnc(plmn, &mcc_bcd, &mnc_bcd);

  srsran::tmgi_t t;
  t.plmn_id_type = srsran::tmgi_t::plmn_id_type_t::explicit_value;
  t.plmn_id.explicit_value.from_number(mcc_bcd, mnc_bcd);
  t.serviced_id[0] = tmgi.service_id[0];
  t.serviced_id[1] = tmgi.service_id[1];
  t.serviced_id[2] = tmgi.service_id[2];
  return t;
}

bool m3ap::handle_mbms_session_start_request(const mbms_session_start_request_s& req)
{
  std::string key = tmgi_key(req.tmgi);
  logger.info("Received MBMS Session Start Request. TMGI key: %s", key.c_str());
  srsran::console("Received MBMS Session Start Request. TMGI key: %s\n", key.c_str());

  uint32_t mce_id                  = next_mce_mbms_m3ap_id++;
  sessions[req.mme_mbms_m3ap_id]   = m3ap_session_t{key, mce_id};

  uint8_t session_id         = req.mbms_session_id_present ? req.mbms_session_id[0] : 0;
  bool    session_id_present = req.mbms_session_id_present;
  rrc->mbms_session_start(key, to_tmgi_t(req.tmgi), session_id, session_id_present);

  m3ap_pdu_c resp_pdu;
  auto&      resp             = resp_pdu.set_successful_outcome_mbms_session_start_resp();
  resp.mme_mbms_m3ap_id       = req.mme_mbms_m3ap_id;
  resp.mce_mbms_m3ap_id       = mce_id;
  bool ok                     = sctp_send_m3ap_pdu(resp_pdu, "MBMSSessionStartResponse");
  logger.info("Sent MBMS Session Start Response. TMGI key: %s", key.c_str());
  return ok;
}

bool m3ap::handle_mbms_session_update_request(const mbms_session_update_request_s& req)
{
  auto it = sessions.find(req.mme_mbms_m3ap_id);
  if (it == sessions.end()) {
    logger.warning("MBMS Session Update Request for unknown MME-MBMS-M3AP-ID %u", req.mme_mbms_m3ap_id);
    return false;
  }
  logger.info("Received MBMS Session Update Request. TMGI key: %s", it->second.tmgi_key.c_str());

  // No MCCH-visible content changes on Update in this pass (TMGI/session-id are immutable for the life of a
  // session; QoS/duration/service-area aren't broadcast in MCCH at all) -- acknowledge only. See project notes.
  m3ap_pdu_c resp_pdu;
  auto&      resp       = resp_pdu.set_successful_outcome_mbms_session_update_resp();
  resp.mme_mbms_m3ap_id = req.mme_mbms_m3ap_id;
  resp.mce_mbms_m3ap_id = it->second.mce_mbms_m3ap_id;
  return sctp_send_m3ap_pdu(resp_pdu, "MBMSSessionUpdateResponse");
}

bool m3ap::handle_mbms_session_stop_request(const mbms_session_stop_request_s& req)
{
  auto it = sessions.find(req.mme_mbms_m3ap_id);
  if (it == sessions.end()) {
    logger.warning("MBMS Session Stop Request for unknown MME-MBMS-M3AP-ID %u", req.mme_mbms_m3ap_id);
    return false;
  }
  std::string key         = it->second.tmgi_key;
  uint32_t    mce_m3ap_id = it->second.mce_mbms_m3ap_id;
  logger.info("Received MBMS Session Stop Request. TMGI key: %s", key.c_str());
  srsran::console("Received MBMS Session Stop Request. TMGI key: %s\n", key.c_str());

  rrc->mbms_session_stop(key);
  sessions.erase(it);

  m3ap_pdu_c resp_pdu;
  auto&      resp       = resp_pdu.set_successful_outcome_mbms_session_stop_resp();
  resp.mme_mbms_m3ap_id = req.mme_mbms_m3ap_id;
  resp.mce_mbms_m3ap_id = mce_m3ap_id;
  return sctp_send_m3ap_pdu(resp_pdu, "MBMSSessionStopResponse");
}

} // namespace srsenb
