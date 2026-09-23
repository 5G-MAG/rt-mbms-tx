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

#include "srsepc/hdr/mme/sbc.h"
#include "srsepc/hdr/mme/s1ap.h"
#include "srsran/common/network_utils.h"
#include "srsran/common/standard_streams.h"
#include <arpa/inet.h>
#include <cstdlib>

namespace srsepc {

sbc*            sbc::m_instance    = nullptr;
pthread_mutex_t sbc_instance_mutex = PTHREAD_MUTEX_INITIALIZER;

sbc::sbc() = default;
sbc::~sbc() = default;

sbc* sbc::get_instance()
{
  pthread_mutex_lock(&sbc_instance_mutex);
  if (m_instance == nullptr) {
    m_instance = new sbc();
  }
  pthread_mutex_unlock(&sbc_instance_mutex);
  return m_instance;
}

void sbc::cleanup()
{
  pthread_mutex_lock(&sbc_instance_mutex);
  if (m_instance != nullptr) {
    delete m_instance;
    m_instance = nullptr;
  }
  pthread_mutex_unlock(&sbc_instance_mutex);
}

bool sbc::init(const sbc_args_t& args)
{
  m_sbc_args = args;

  m_sbc_fd = cbc_listen();
  if (m_sbc_fd == -1) {
    return false;
  }

  m_bridge_fd = bridge_listen();
  if (m_bridge_fd == -1) {
    return false;
  }

  m_logger.info("SBc-AP Initialized. Bind addr: %s, port: %d", args.sbc_bind_addr.c_str(), args.sbc_bind_port);
  srsran::console("SBc-AP Initialized. Bind addr: %s, port: %d\n", args.sbc_bind_addr.c_str(), args.sbc_bind_port);
  m_logger.info("SBc-AP portal bridge listening on tcp %s:%u", args.bridge_bind_addr.c_str(), args.bridge_port);
  srsran::console("SBc-AP portal bridge listening on tcp %s:%u\n", args.bridge_bind_addr.c_str(), args.bridge_port);
  return true;
}

void sbc::stop()
{
  if (m_sbc_fd != -1) {
    close(m_sbc_fd);
    m_sbc_fd = -1;
  }
  if (m_bridge_fd != -1) {
    close(m_bridge_fd);
    m_bridge_fd = -1;
  }
}

int sbc::get_sbc()
{
  return m_sbc_fd;
}

int sbc::get_bridge()
{
  return m_bridge_fd;
}

int sbc::bridge_listen()
{
  // Portal-facing control-plane socket (see file header). TCP (AF_INET/SOCK_STREAM) so the
  // control portal can run in a separate container/host. Message framing is done by connection
  // lifetime rather than datagram boundaries: the client half-closes its write side
  // (shutdown/end) right after writing the request, so handle_bridge_connection() can simply
  // read() in a loop until EOF to know the whole request has arrived.
  int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd == -1) {
    srsran::console("Could not create SBc-AP bridge TCP socket\n");
    return -1;
  }

  // Allow immediate rebind after a restart (avoid TIME_WAIT on the bridge port).
  int one = 1;
  setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

  struct sockaddr_in addr;
  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port   = htons(m_sbc_args.bridge_port);
  if (inet_pton(AF_INET, m_sbc_args.bridge_bind_addr.c_str(), &addr.sin_addr) != 1) {
    close(sock_fd);
    m_logger.error("Invalid SBc-AP bridge bind address: %s", m_sbc_args.bridge_bind_addr.c_str());
    srsran::console("Invalid SBc-AP bridge bind address: %s\n", m_sbc_args.bridge_bind_addr.c_str());
    return -1;
  }

  if (bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
    close(sock_fd);
    m_logger.error("Error binding SBc-AP bridge socket at %s:%u",
                   m_sbc_args.bridge_bind_addr.c_str(),
                   m_sbc_args.bridge_port);
    srsran::console("Error binding SBc-AP bridge socket at %s:%u\n",
                    m_sbc_args.bridge_bind_addr.c_str(),
                    m_sbc_args.bridge_port);
    return -1;
  }

  if (listen(sock_fd, SOMAXCONN) != 0) {
    close(sock_fd);
    m_logger.error("Error in SBc-AP bridge socket listen");
    srsran::console("Error in SBc-AP bridge socket listen\n");
    return -1;
  }

  // SECURITY: the AF_UNIX version restricted access with filesystem ownership/permissions (0660,
  // chown'd to the invoking user under sudo). A TCP endpoint has no such model and this bridge
  // can inject emergency-alert (Write-Replace Warning) PDUs, so anyone who can reach it can forge
  // public warnings. Keep bridge_bind_addr on loopback or a trusted management network and
  // firewall it. The default is loopback.
  if (m_sbc_args.bridge_bind_addr != "127.0.0.1" && m_sbc_args.bridge_bind_addr != "localhost") {
    m_logger.warning("SBc-AP bridge bound to %s:%u (not loopback) -- this TCP endpoint can inject emergency "
                     "alerts and has NO authentication; restrict it to a trusted management network.",
                     m_sbc_args.bridge_bind_addr.c_str(),
                     m_sbc_args.bridge_port);
  }

  return sock_fd;
}

int sbc::cbc_listen()
{
  // Mirrors s1ap::enb_listen() -- SBc-AP, like S1AP, is a one-to-many SCTP socket;
  // there is no explicit accept(), associations are distinguished by sinfo_assoc_id.
  int                         sock_fd, err;
  struct sockaddr_in          sbc_addr;
  struct sctp_event_subscribe evnts;

  m_logger.info("SBc Initializing");
  sock_fd = socket(AF_INET, SOCK_SEQPACKET, IPPROTO_SCTP);
  if (sock_fd == -1) {
    srsran::console("Could not create SBc-AP SCTP socket\n");
    return -1;
  }

  bzero(&evnts, sizeof(evnts));
  evnts.sctp_data_io_event  = 1;
  evnts.sctp_shutdown_event = 1;
  if (setsockopt(sock_fd, IPPROTO_SCTP, SCTP_EVENTS, &evnts, sizeof(evnts))) {
    close(sock_fd);
    srsran::console("Subscribing to SBc-AP sctp_data_io_events failed\n");
    return -1;
  }

  bzero(&sbc_addr, sizeof(sbc_addr));
  if (not srsran::net_utils::set_sockaddr(&sbc_addr, m_sbc_args.sbc_bind_addr.c_str(), m_sbc_args.sbc_bind_port)) {
    close(sock_fd);
    m_logger.error("Invalid sbc_bind_addr: %s", m_sbc_args.sbc_bind_addr.c_str());
    srsran::console("Invalid sbc_bind_addr: %s\n", m_sbc_args.sbc_bind_addr.c_str());
    return -1;
  }

  if (not srsran::net_utils::bind_addr(sock_fd, sbc_addr)) {
    close(sock_fd);
    m_logger.error("Error binding SBc-AP SCTP socket");
    srsran::console("Error binding SBc-AP SCTP socket\n");
    return -1;
  }

  err = listen(sock_fd, SOMAXCONN);
  if (err != 0) {
    close(sock_fd);
    m_logger.error("Error in SBc-AP SCTP socket listen");
    srsran::console("Error in SBc-AP SCTP socket listen\n");
    return -1;
  }

  return sock_fd;
}

bool sbc::sbc_tx_pdu(const asn1::sbc_ap::sbc_ap_pdu_c& pdu, struct sctp_sndrcvinfo* cbc_sri)
{
  m_logger.debug("Transmitting SBc-AP PDU. CBC SCTP association Id: %d", cbc_sri->sinfo_assoc_id);

  srsran::unique_byte_buffer_t buf = srsran::make_byte_buffer();
  if (buf == nullptr) {
    m_logger.error("Fatal Error: Couldn't allocate buffer for SBc-AP PDU.");
    return false;
  }
  asn1::bit_ref bref(buf->msg, buf->get_tailroom());
  if (pdu.pack(bref) != asn1::SRSASN_SUCCESS) {
    m_logger.error("Could not pack SBc-AP PDU correctly.");
    return false;
  }
  buf->N_bytes = bref.distance_bytes();

  ssize_t n_sent = sctp_send(m_sbc_fd, buf->msg, buf->N_bytes, cbc_sri, MSG_NOSIGNAL);
  if (n_sent == -1) {
    srsran::console("Failed to send SBc-AP PDU. Error: %s\n", strerror(errno));
    m_logger.error("Failed to send SBc-AP PDU. Error: %s", strerror(errno));
    return false;
  }

  return true;
}

void sbc::handle_sbc_rx_pdu(srsran::byte_buffer_t* pdu, struct sctp_sndrcvinfo* cbc_sri)
{
  asn1::sbc_ap::sbc_ap_pdu_c rx_pdu;
  asn1::cbit_ref             bref(pdu->msg, pdu->N_bytes);
  if (rx_pdu.unpack(bref) != asn1::SRSASN_SUCCESS) {
    m_logger.error("Failed to unpack received SBc-AP PDU");
    return;
  }

  if (rx_pdu.type().value != asn1::sbc_ap::sbc_ap_pdu_c::types_opts::init_msg) {
    m_logger.warning("Unhandled SBc-AP PDU type %d", rx_pdu.type().value);
    return;
  }

  asn1::sbc_ap::sbc_ap_pdu_c resp_pdu;
  switch (rx_pdu.init_msg().value.type().value) {
    case asn1::sbc_ap::sbc_ap_elem_procs_o::init_msg_c::types_opts::write_replace_warning_request:
      m_logger.info("Received Write-Replace-Warning-Request");
      resp_pdu = build_write_replace_warning_response(rx_pdu.init_msg().value.write_replace_warning_request());
      break;
    case asn1::sbc_ap::sbc_ap_elem_procs_o::init_msg_c::types_opts::stop_warning_request:
      m_logger.info("Received Stop-Warning-Request");
      resp_pdu = build_stop_warning_response(rx_pdu.init_msg().value.stop_warning_request());
      break;
    default:
      m_logger.warning("Unhandled SBc-AP initiating message");
      return;
  }
  sbc_tx_pdu(resp_pdu, cbc_sri);
}

void sbc::handle_bridge_connection()
{
  struct sockaddr_in peer_addr;
  socklen_t          peer_len = sizeof(peer_addr);
  int                conn_fd  = accept(m_bridge_fd, (struct sockaddr*)&peer_addr, &peer_len);
  if (conn_fd == -1) {
    m_logger.error("Error accepting SBc-AP bridge connection: %s", strerror(errno));
    return;
  }

  // A stuck or malicious local client must not hang the single accept thread indefinitely
  // (mirrors srsenb's control_server.cc::handle_connection()).
  struct timeval tv;
  tv.tv_sec  = 2;
  tv.tv_usec = 0;
  setsockopt(conn_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(conn_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

  // SOCK_STREAM has no datagram boundaries -- read until the client half-closes its write
  // side (EOF here), which is how bridge-client.js signals "the whole request has been sent"
  // (see bridge_listen()'s comment above for why SOCK_STREAM was chosen over SOCK_SEQPACKET).
  uint8_t buf[SRSRAN_MAX_BUFFER_SIZE_BYTES];
  size_t  n_total = 0;
  while (n_total < sizeof(buf)) {
    ssize_t n = read(conn_fd, buf + n_total, sizeof(buf) - n_total);
    if (n < 0) {
      m_logger.error("Error reading from SBc-AP bridge connection: %s", strerror(errno));
      close(conn_fd);
      return;
    }
    if (n == 0) {
      break; // EOF -- client finished sending the request
    }
    n_total += (size_t)n;
  }
  if (n_total == 0) {
    m_logger.error("Empty SBc-AP bridge connection request");
    close(conn_fd);
    return;
  }

  asn1::sbc_ap::sbc_ap_pdu_c rx_pdu;
  asn1::cbit_ref             bref(buf, (uint32_t)n_total);
  if (rx_pdu.unpack(bref) != asn1::SRSASN_SUCCESS) {
    m_logger.error("Failed to unpack SBc-AP PDU from bridge connection");
    close(conn_fd);
    return;
  }

  asn1::sbc_ap::sbc_ap_pdu_c resp_pdu;
  bool                       have_resp = true;
  if (rx_pdu.type().value != asn1::sbc_ap::sbc_ap_pdu_c::types_opts::init_msg) {
    m_logger.warning("Unhandled SBc-AP PDU type %d on bridge connection", rx_pdu.type().value);
    have_resp = false;
  } else {
    switch (rx_pdu.init_msg().value.type().value) {
      case asn1::sbc_ap::sbc_ap_elem_procs_o::init_msg_c::types_opts::write_replace_warning_request:
        m_logger.info("Received Write-Replace-Warning-Request (bridge)");
        resp_pdu = build_write_replace_warning_response(rx_pdu.init_msg().value.write_replace_warning_request());
        break;
      case asn1::sbc_ap::sbc_ap_elem_procs_o::init_msg_c::types_opts::stop_warning_request:
        m_logger.info("Received Stop-Warning-Request (bridge)");
        resp_pdu = build_stop_warning_response(rx_pdu.init_msg().value.stop_warning_request());
        break;
      default:
        m_logger.warning("Unhandled SBc-AP initiating message on bridge connection");
        have_resp = false;
    }
  }

  if (have_resp) {
    srsran::unique_byte_buffer_t resp_buf = srsran::make_byte_buffer();
    asn1::bit_ref                resp_bref(resp_buf->msg, resp_buf->get_tailroom());
    if (resp_pdu.pack(resp_bref) == asn1::SRSASN_SUCCESS) {
      resp_buf->N_bytes = resp_bref.distance_bytes();
      size_t n_sent = 0;
      while (n_sent < resp_buf->N_bytes) {
        ssize_t n = write(conn_fd, resp_buf->msg + n_sent, resp_buf->N_bytes - n_sent);
        if (n <= 0) {
          m_logger.error("Failed to write SBc-AP response on bridge connection: %s", strerror(errno));
          break;
        }
        n_sent += (size_t)n;
      }
    } else {
      m_logger.error("Could not pack SBc-AP bridge response");
    }
  }

  close(conn_fd);
}

asn1::sbc_ap::sbc_ap_pdu_c
sbc::build_write_replace_warning_response(const asn1::sbc_ap::write_replace_warning_request_s& req)
{
  const auto& ies = req.protocol_ies;

  // Forward to every connected eNB as a native S1AP WriteReplaceWarningRequest. The codec
  // for this already exists in this codebase (lib/include/srsran/asn1/s1ap.h) -- only this
  // dispatch/forwarding logic and the CBC-facing SBc-AP side were missing.
  s1ap*                        s1ap_ctx = s1ap::get_instance();
  s1ap_pdu_t       tx_pdu;
  tx_pdu.set_init_msg().load_info_obj(ASN1_S1AP_ID_WRITE_REPLACE_WARNING);
  asn1::s1ap::write_replace_warning_request_ies_container& s1ap_ies =
      tx_pdu.init_msg().value.write_replace_warning_request().protocol_ies;

  s1ap_ies.msg_id.value.from_number(ies.msg_id.value.to_number());
  s1ap_ies.serial_num.value.from_number(ies.serial_num.value.to_number());

  // SBc-AP's Repetition-Period is INTEGER(0..4096); S1AP's own copy of the same-named field
  // is INTEGER(0..4095) -- see sbc_ap.h's file header for why this genuine spec discrepancy
  // isn't "fixed" to match. Clamp the one value SBc-AP allows that S1AP doesn't, rather than
  // let it silently corrupt the S1AP encoding.
  uint16_t repeat_period = ies.repeat_period.value;
  if (repeat_period > 4095) {
    m_logger.warning("Clamping Repetition-Period %d to S1AP's max of 4095", repeat_period);
    repeat_period = 4095;
  }
  s1ap_ies.repeat_period.value           = repeat_period;
  s1ap_ies.numof_broadcast_request.value = ies.nof_broadcasts_requested.value;

  if (ies.warning_type_present) {
    s1ap_ies.warning_type_present = true;
    s1ap_ies.warning_type.value.from_number(ies.warning_type.value.to_number());
  }
  if (ies.warning_security_info_present) {
    s1ap_ies.warning_security_info_present = true;
    s1ap_ies.warning_security_info.value.from_number(ies.warning_security_info.value.to_number());
  }
  if (ies.data_coding_scheme_present) {
    s1ap_ies.data_coding_scheme_present = true;
    s1ap_ies.data_coding_scheme.value.from_number(ies.data_coding_scheme.value.to_number());
  }
  if (ies.warning_msg_content_present) {
    s1ap_ies.warning_msg_contents_present = true;
    s1ap_ies.warning_msg_contents.value.resize(ies.warning_msg_content.value.size());
    memcpy(s1ap_ies.warning_msg_contents.value.data(),
           ies.warning_msg_content.value.data(),
           ies.warning_msg_content.value.size());
  }
  if (ies.warning_area_coordinates_present) {
    // Opaque octet string, per ATIS-0700041 -- carried through untouched, never decoded here.
    s1ap_ies.warning_area_coordinates_present = true;
    s1ap_ies.warning_area_coordinates.value.resize(ies.warning_area_coordinates.value.size());
    memcpy(s1ap_ies.warning_area_coordinates.value.data(),
           ies.warning_area_coordinates.value.data(),
           ies.warning_area_coordinates.value.size());
  }

  bool all_sent = true;
  for (auto& it : s1ap_ctx->m_active_enbs) {
    enb_ctx_t* enb_ctx = it.second;
    if (!s1ap_ctx->s1ap_tx_pdu(tx_pdu, &enb_ctx->sri)) {
      m_logger.error("Error forwarding Write-Replace-Warning to eNB. eNB Id: 0x%x.", enb_ctx->enb_id);
      all_sent = false;
    }
  }

  // Ack the CBC immediately, without waiting for eNB responses -- TS 29.168 clause 4.3.4.2.1:
  // "The MME shall return a WRITE-REPLACE WARNING RESPONSE to the CBC immediately after the
  // reception of the WRITE-REPLACE WARNING REQUEST message without waiting responses from eNBs."
  asn1::sbc_ap::sbc_ap_pdu_c resp_pdu;
  auto&                      resp = resp_pdu.set_successful_outcome();
  resp.load_info_obj(0); // id-Write-Replace-Warning
  auto& resp_ies = resp.value.write_replace_warning_resp().protocol_ies;
  resp_ies.msg_id.value.from_number(ies.msg_id.value.to_number());
  resp_ies.serial_num.value.from_number(ies.serial_num.value.to_number());
  resp_ies.cause.value = all_sent ? 0 : 12; // 0 = message-accepted, 12 = unspecifed-error

  return resp_pdu;
}

asn1::sbc_ap::sbc_ap_pdu_c sbc::build_stop_warning_response(const asn1::sbc_ap::stop_warning_request_s& req)
{
  const auto& ies = req.protocol_ies;

  // Maps to S1AP KillRequest, the standard PWS "cancel" procedure mapping.
  s1ap*      s1ap_ctx = s1ap::get_instance();
  s1ap_pdu_t tx_pdu;
  tx_pdu.set_init_msg().load_info_obj(ASN1_S1AP_ID_KILL);
  asn1::s1ap::kill_request_ies_container& s1ap_ies = tx_pdu.init_msg().value.kill_request().protocol_ies;
  s1ap_ies.msg_id.value.from_number(ies.msg_id.value.to_number());
  s1ap_ies.serial_num.value.from_number(ies.serial_num.value.to_number());

  bool all_sent = true;
  for (auto& it : s1ap_ctx->m_active_enbs) {
    enb_ctx_t* enb_ctx = it.second;
    if (!s1ap_ctx->s1ap_tx_pdu(tx_pdu, &enb_ctx->sri)) {
      m_logger.error("Error forwarding Kill Request to eNB. eNB Id: 0x%x.", enb_ctx->enb_id);
      all_sent = false;
    }
  }

  asn1::sbc_ap::sbc_ap_pdu_c resp_pdu;
  auto&                      resp = resp_pdu.set_successful_outcome();
  resp.load_info_obj(1); // id-Stop-Warning
  auto& resp_ies = resp.value.stop_warning_resp().protocol_ies;
  resp_ies.msg_id.value.from_number(ies.msg_id.value.to_number());
  resp_ies.serial_num.value.from_number(ies.serial_num.value.to_number());
  resp_ies.cause.value = all_sent ? 0 : 12;

  return resp_pdu;
}

} // namespace srsepc
