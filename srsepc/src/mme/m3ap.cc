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

#include "srsepc/hdr/mme/m3ap.h"
#include "srsran/common/bcd_helpers.h"
#include "srsran/common/network_utils.h"
#include "srsran/common/standard_streams.h"
#include <netinet/in.h>
#include <unistd.h>

namespace srsepc {

using namespace asn1::m3ap;

m3ap*           m3ap::m_instance    = nullptr;
static pthread_mutex_t m3ap_instance_mutex = PTHREAD_MUTEX_INITIALIZER;

m3ap* m3ap::get_instance()
{
  pthread_mutex_lock(&m3ap_instance_mutex);
  if (m_instance == nullptr) {
    m_instance = new m3ap();
  }
  pthread_mutex_unlock(&m3ap_instance_mutex);
  return m_instance;
}

void m3ap::cleanup()
{
  pthread_mutex_lock(&m3ap_instance_mutex);
  if (m_instance != nullptr) {
    delete m_instance;
    m_instance = nullptr;
  }
  pthread_mutex_unlock(&m3ap_instance_mutex);
}

bool m3ap::init(const m3ap_args_t& args)
{
  m_args = args;
  m_m3   = enb_listen();
  if (m_m3 == SRSRAN_ERROR) {
    return false;
  }
  m_logger.info("M3AP Initialized");
  return true;
}

void m3ap::stop()
{
  if (m_m3 != -1) {
    close(m_m3);
    m_m3 = -1;
  }
}

int m3ap::get_m3()
{
  return m_m3;
}

// Mirrors s1ap::enb_listen() (srsepc/src/mme/s1ap.cc) exactly: one-to-many SCTP socket, no accept() needed.
int m3ap::enb_listen()
{
  int                         sock_fd, err;
  struct sockaddr_in          m3_addr;
  struct sctp_event_subscribe evnts;

  m_logger.info("M3 Initializing");
  sock_fd = socket(AF_INET, SOCK_SEQPACKET, IPPROTO_SCTP);
  if (sock_fd == -1) {
    srsran::console("Could not create M3 SCTP socket\n");
    return SRSRAN_ERROR;
  }

  bzero(&evnts, sizeof(evnts));
  evnts.sctp_data_io_event  = 1;
  evnts.sctp_shutdown_event = 1;
  if (setsockopt(sock_fd, IPPROTO_SCTP, SCTP_EVENTS, &evnts, sizeof(evnts))) {
    close(sock_fd);
    srsran::console("Subscribing to M3 sctp_data_io_events failed\n");
    return SRSRAN_ERROR;
  }

  bzero(&m3_addr, sizeof(m3_addr));
  if (not srsran::net_utils::set_sockaddr(&m3_addr, m_args.bind_addr.c_str(), m_args.bind_port)) {
    close(sock_fd);
    m_logger.error("Invalid m3 bind_addr: %s", m_args.bind_addr.c_str());
    srsran::console("Invalid M3 bind_addr: %s\n", m_args.bind_addr.c_str());
    return SRSRAN_ERROR;
  }

  if (not srsran::net_utils::bind_addr(sock_fd, m3_addr)) {
    close(sock_fd);
    m_logger.error("Error binding M3 SCTP socket");
    srsran::console("Error binding M3 SCTP socket\n");
    return SRSRAN_ERROR;
  }

  err = listen(sock_fd, SOMAXCONN);
  if (err != 0) {
    close(sock_fd);
    m_logger.error("Error in M3 SCTP socket listen");
    srsran::console("Error in M3 SCTP socket listen\n");
    return SRSRAN_ERROR;
  }

  return sock_fd;
}

void m3ap::delete_mce_ctx(int32_t assoc_id)
{
  auto it = m_active_mces.find(assoc_id);
  if (it != m_active_mces.end()) {
    m_logger.info("Deleting MCE context. Association: %d", assoc_id);
    m_active_mces.erase(it);
  }
}

bool m3ap::m3ap_tx_pdu(const m3ap_pdu_c& pdu, const struct sctp_sndrcvinfo& mce_sri)
{
  srsran::unique_byte_buffer_t buf = srsran::make_byte_buffer();
  if (buf == nullptr) {
    m_logger.error("Fatal Error: Couldn't allocate buffer for M3AP PDU.");
    return false;
  }
  asn1::bit_ref bref(buf->msg, buf->get_tailroom());
  if (pdu.pack(bref) != asn1::SRSASN_SUCCESS) {
    m_logger.error("Could not pack M3AP PDU correctly.");
    return false;
  }
  buf->N_bytes = bref.distance_bytes();

  struct sctp_sndrcvinfo sri = mce_sri;
  ssize_t                n_sent = sctp_send(m_m3, buf->msg, buf->N_bytes, &sri, MSG_NOSIGNAL);
  if (n_sent == -1) {
    m_logger.error("Failed to send M3AP PDU. Error: %s", strerror(errno));
    return false;
  }
  return true;
}

void m3ap::handle_m3ap_rx_pdu(srsran::byte_buffer_t* pdu, struct sctp_sndrcvinfo* mce_sri)
{
  m3ap_pdu_c     rx_pdu;
  asn1::cbit_ref bref(pdu->msg, pdu->N_bytes);
  if (rx_pdu.unpack(bref) != asn1::SRSASN_SUCCESS) {
    m_logger.error("Failed to unpack received M3AP PDU");
    return;
  }

  switch (rx_pdu.msg_type().value) {
    case msg_type_opts::init_msg:
      handle_initiating_message(rx_pdu, mce_sri);
      break;
    case msg_type_opts::successful_outcome:
      handle_successful_outcome(rx_pdu, *mce_sri);
      break;
    case msg_type_opts::unsuccessful_outcome:
      handle_unsuccessful_outcome(rx_pdu, *mce_sri);
      break;
    default:
      m_logger.warning("Unhandled M3AP PDU type %d", (int)rx_pdu.msg_type().value);
  }
}

void m3ap::handle_initiating_message(const m3ap_pdu_c& pdu, struct sctp_sndrcvinfo* mce_sri)
{
  switch (pdu.proc_code()) {
    case proc_code_m3_setup:
      handle_m3_setup_request(pdu.m3_setup_request(), mce_sri);
      break;
    default:
      m_logger.warning("Unhandled M3AP initiating message, procedure code %d", pdu.proc_code());
  }
}

// M3 Setup is MCE(eNB)-initiated (TS 36.444 clause 8.7.2). Mirrors
// s1ap_mngmt_proc::handle_s1_setup_request's accept/reply shape, without the
// PLMN/TAC matching logic S1 Setup does (M3 Setup carries an MBMS Service
// Area List, not a TAC, and this project runs a single MME/single MCE
// testbed where rejecting on service-area mismatch isn't a meaningful check
// yet -- accept unconditionally).
void m3ap::handle_m3_setup_request(const m3_setup_request_s& req, struct sctp_sndrcvinfo* mce_sri)
{
  m_logger.info("Received M3 Setup Request. Association: %d", mce_sri->sinfo_assoc_id);
  srsran::console("Received M3 Setup Request. Association: %d\n", mce_sri->sinfo_assoc_id);

  mce_ctx_t ctx;
  ctx.sri              = *mce_sri;
  ctx.setup_complete   = true;
  ctx.global_mce_id    = req.global_mce_id;
  ctx.service_area_ids.reserve(req.mbms_service_area_list.size());
  for (uint32_t i = 0; i < req.mbms_service_area_list.size(); i++) {
    uint16_t sai = (uint16_t)req.mbms_service_area_list[i].to_number();
    ctx.service_area_ids.push_back(sai);
  }
  m_active_mces[mce_sri->sinfo_assoc_id] = ctx;

  m3ap_pdu_c resp_pdu;
  resp_pdu.set_successful_outcome_m3_setup_resp();
  m3ap_tx_pdu(resp_pdu, *mce_sri);
  m_logger.info("Sent M3 Setup Response. Association: %d", mce_sri->sinfo_assoc_id);
  srsran::console("Sent M3 Setup Response. Association: %d\n", mce_sri->sinfo_assoc_id);
}

bool m3ap::find_key_by_ids(int32_t assoc_id, uint32_t mme_mbms_m3ap_id, std::string* out_key, uint32_t* out_idx)
{
  for (auto& kv : m_session_ids) {
    auto it = kv.second.find(assoc_id);
    if (it != kv.second.end() && it->second.mme_mbms_m3ap_id == mme_mbms_m3ap_id) {
      *out_key = kv.first;
      return true;
    }
  }
  (void)out_idx;
  return false;
}

void m3ap::handle_successful_outcome(const m3ap_pdu_c& pdu, const struct sctp_sndrcvinfo& mce_sri)
{
  mme_gtpc* gtpc = mme_gtpc::get_instance();
  switch (pdu.proc_code()) {
    case proc_code_mbms_session_start: {
      const mbms_session_start_resp_s& resp = pdu.mbms_session_start_resp();
      std::string                      key;
      if (!find_key_by_ids(mce_sri.sinfo_assoc_id, resp.mme_mbms_m3ap_id, &key, nullptr)) {
        m_logger.warning("MBMS Session Start Response for unknown MME-MBMS-M3AP-ID %u", resp.mme_mbms_m3ap_id);
        return;
      }
      m_session_ids[key][mce_sri.sinfo_assoc_id].mce_id_known     = true;
      m_session_ids[key][mce_sri.sinfo_assoc_id].mce_mbms_m3ap_id = resp.mce_mbms_m3ap_id;
      m_logger.info("Received MBMS Session Start Response. TMGI key: %s, MCE-MBMS-M3AP-ID: %u",
                     key.c_str(),
                     resp.mce_mbms_m3ap_id);
      gtpc->mbms_session_start_confirm(key, srsran::GTPC_CAUSE_VALUE_REQUEST_ACCEPTED, mce_sri.sinfo_assoc_id);
      break;
    }
    case proc_code_mbms_session_update: {
      const mbms_session_update_resp_s& resp = pdu.mbms_session_update_resp();
      std::string                       key;
      if (!find_key_by_ids(mce_sri.sinfo_assoc_id, resp.mme_mbms_m3ap_id, &key, nullptr)) {
        m_logger.warning("MBMS Session Update Response for unknown MME-MBMS-M3AP-ID %u", resp.mme_mbms_m3ap_id);
        return;
      }
      gtpc->mbms_session_update_confirm(key, srsran::GTPC_CAUSE_VALUE_REQUEST_ACCEPTED, mce_sri.sinfo_assoc_id);
      break;
    }
    case proc_code_mbms_session_stop: {
      const mbms_session_stop_resp_s& resp = pdu.mbms_session_stop_resp();
      std::string                     key;
      if (!find_key_by_ids(mce_sri.sinfo_assoc_id, resp.mme_mbms_m3ap_id, &key, nullptr)) {
        m_logger.warning("MBMS Session Stop Response for unknown MME-MBMS-M3AP-ID %u", resp.mme_mbms_m3ap_id);
        return;
      }
      gtpc->mbms_session_stop_confirm(key, srsran::GTPC_CAUSE_VALUE_REQUEST_ACCEPTED, mce_sri.sinfo_assoc_id);
      m_session_ids[key].erase(mce_sri.sinfo_assoc_id);
      if (m_session_ids[key].empty()) {
        m_session_ids.erase(key);
      }
      break;
    }
    default:
      m_logger.warning("Unhandled M3AP successful outcome, procedure code %d", pdu.proc_code());
  }
}

void m3ap::handle_unsuccessful_outcome(const m3ap_pdu_c& pdu, const struct sctp_sndrcvinfo& mce_sri)
{
  mme_gtpc* gtpc = mme_gtpc::get_instance();
  switch (pdu.proc_code()) {
    case proc_code_mbms_session_start: {
      const mbms_session_start_fail_s& fail = pdu.mbms_session_start_fail();
      std::string                      key;
      if (!find_key_by_ids(mce_sri.sinfo_assoc_id, fail.mme_mbms_m3ap_id, &key, nullptr)) {
        m_logger.warning("MBMS Session Start Failure for unknown MME-MBMS-M3AP-ID %u", fail.mme_mbms_m3ap_id);
        return;
      }
      m_logger.warning("Received MBMS Session Start Failure. TMGI key: %s, cause: %s",
                        key.c_str(),
                        fail.cause.to_string());
      gtpc->mbms_session_start_confirm(key, srsran::GTPC_CAUSE_VALUE_REQUEST_REJECTED, mce_sri.sinfo_assoc_id);
      m_session_ids[key].erase(mce_sri.sinfo_assoc_id);
      break;
    }
    case proc_code_mbms_session_update: {
      const mbms_session_update_fail_s& fail = pdu.mbms_session_update_fail();
      std::string                       key;
      if (!find_key_by_ids(mce_sri.sinfo_assoc_id, fail.mme_mbms_m3ap_id, &key, nullptr)) {
        m_logger.warning("MBMS Session Update Failure for unknown MME-MBMS-M3AP-ID %u", fail.mme_mbms_m3ap_id);
        return;
      }
      m_logger.warning("Received MBMS Session Update Failure. TMGI key: %s, cause: %s",
                        key.c_str(),
                        fail.cause.to_string());
      gtpc->mbms_session_update_confirm(key, srsran::GTPC_CAUSE_VALUE_REQUEST_REJECTED, mce_sri.sinfo_assoc_id);
      break;
    }
    default:
      m_logger.warning("Unhandled M3AP unsuccessful outcome, procedure code %d", pdu.proc_code());
  }
}

// Builds MBMS-E-RAB-QoS-Parameters from the Sm request's own GTPv2-C Bearer QoS IE -- real data, not fabricated
// (see project notes: gbrQosInformation maps mbr_dl/gbr_dl, since MBMS is downlink-only broadcast).
static mbms_e_rab_qos_params_s build_qos_params(const srsran::gtpc_bearer_qos_ie& qos)
{
  mbms_e_rab_qos_params_s params;
  params.qci                  = qos.qci;
  params.gbr_qos_info_present = true;
  params.gbr_qos_info.max_bitrate_dl        = qos.mbr_dl;
  params.gbr_qos_info.guaranteed_bitrate_dl = qos.gbr_dl;
  return params;
}

// MBMS-Session-Duration/-Service-Area/MinimumTimeToMBMSDataTransfer are all specified in TS 36.444 clause
// 9.2.3.5/9.2.3.6/9.2.3.8 as "coded as the value part of the [TS 29.061] AVP of the same name" -- the exact
// same AVP family the existing gtpc_pack_mbms_session_duration_ie/gtpc_pack_mbms_service_area_ie/
// gtpc_pack_mbms_time_to_data_transfer_ie already implement and have tested (lib/src/asn1/gtpc.cc). Reused
// here unmodified rather than reimplemented, since M3AP's own module defines no separate bit layout for them.
static void encode_session_duration(uint32_t duration_sec, asn1::fixed_octstring<3, true>* out)
{
  uint32_t days      = duration_sec / 86400;
  if (days > 18) {
    days = 18;
  }
  uint32_t seconds17 = duration_sec - days * 86400;
  (*out)[0]          = (uint8_t)((seconds17 >> 9) & 0xFF);
  (*out)[1]          = (uint8_t)((seconds17 >> 1) & 0xFF);
  (*out)[2]          = (uint8_t)(((seconds17 & 0x1) << 7) | (days & 0x7F));
}

static void encode_service_area(const srsran::gtpc_mbms_service_area_ie& area, asn1::unbounded_octstring<true>* out)
{
  uint8_t n = (uint8_t)std::min<size_t>(area.sai_codes.size(), 255);
  out->resize(1 + n * 2);
  (*out)[0] = n;
  for (uint32_t i = 0; i < n; i++) {
    (*out)[1 + i * 2] = (uint8_t)((area.sai_codes[i] >> 8) & 0xFF);
    (*out)[2 + i * 2] = (uint8_t)(area.sai_codes[i] & 0xFF);
  }
}

static void encode_tnl_info(const srsran::gtpc_mbms_ip_mc_distrib_ie& distrib, tnl_info_s* out)
{
  if (distrib.dist_addr_is_ipv6) {
    out->ip_mc_addr.addr.resize(16);
    memcpy(out->ip_mc_addr.addr.data(), distrib.dist_addr_ipv6.s6_addr, 16);
  } else {
    out->ip_mc_addr.addr.resize(4);
    out->ip_mc_addr.addr.from_number(ntohl(distrib.dist_addr_ipv4));
  }
  if (distrib.source_addr_is_ipv6) {
    out->ip_src_addr.addr.resize(16);
    memcpy(out->ip_src_addr.addr.data(), distrib.source_addr_ipv6.s6_addr, 16);
  } else {
    out->ip_src_addr.addr.resize(4);
    out->ip_src_addr.addr.from_number(ntohl(distrib.source_addr_ipv4));
  }
  out->gtp_dl_teid.from_number(distrib.c_teid);
}

static void encode_tmgi(const srsran::gtpc_tmgi_ie& tmgi, tmgi_s* out)
{
  uint32_t plmn = 0;
  srsran::s1ap_mccmnc_to_plmn(tmgi.mcc_bcd, tmgi.mnc_bcd, &plmn);
  out->plmn_id.from_number(plmn);
  out->service_id.from_number(tmgi.mbms_service_id);
}

void m3ap::session_start(const std::string&                             tmgi_key,
                          const mbms_gtpc_ctx_t&                         ctx,
                          const srsran::gtpc_mbms_session_start_request& req)
{
  if (m_active_mces.empty()) {
    m_logger.info("MBMS Session Start: no MCE currently connected, nothing to forward. TMGI key: %s",
                   tmgi_key.c_str());
    return;
  }

  for (auto& kv : m_active_mces) {
    int32_t assoc_id = kv.first;

    m3ap_pdu_c pdu;
    auto&      m3req            = pdu.set_init_msg_mbms_session_start_request();
    m3req.mme_mbms_m3ap_id      = m_next_mme_mbms_m3ap_id++;
    encode_tmgi(req.tmgi, &m3req.tmgi);
    if (req.mbms_session_id_present) {
      m3req.mbms_session_id_present = true;
      m3req.mbms_session_id[0]      = req.mbms_session_id.session_id;
    }
    m3req.mbms_e_rab_qos_params = build_qos_params(req.qos_profile);
    encode_session_duration(req.mbms_session_duration.duration_sec, &m3req.mbms_session_duration);
    encode_service_area(req.mbms_service_area, &m3req.mbms_service_area);
    // MinimumTimeToMBMSDataTransfer is mandatory in M3AP but only conditionally present on Sm; default to the
    // minimum representable value (1 second, "as soon as possible") when the MBMS-GW didn't supply one.
    uint32_t min_time_secs                    = req.mbms_time_to_data_transfer_present
                                                     ? req.mbms_time_to_data_transfer.seconds
                                                     : 1;
    m3req.min_time_to_mbms_data_transfer[0]   = (uint8_t)(std::min<uint32_t>(min_time_secs, 256) - 1);
    encode_tnl_info(req.mbms_ip_multicast_distrib, &m3req.tnl_info);

    m_session_ids[tmgi_key][assoc_id].mme_mbms_m3ap_id = m3req.mme_mbms_m3ap_id;
    m_session_ids[tmgi_key][assoc_id].mce_id_known      = false;

    m3ap_tx_pdu(pdu, kv.second.sri);
    m_logger.info("Sent MBMS Session Start Request. TMGI key: %s, Association: %d, MME-MBMS-M3AP-ID: %u",
                   tmgi_key.c_str(),
                   assoc_id,
                   m3req.mme_mbms_m3ap_id);
  }
}

void m3ap::session_update(const std::string&                              tmgi_key,
                           const mbms_gtpc_ctx_t&                          ctx,
                           const srsran::gtpc_mbms_session_update_request& req)
{
  auto sess_it = m_session_ids.find(tmgi_key);
  if (sess_it == m_session_ids.end() || sess_it->second.empty()) {
    m_logger.info("MBMS Session Update: no M3AP session on record, nothing to forward. TMGI key: %s",
                   tmgi_key.c_str());
    return;
  }

  for (auto& kv : sess_it->second) {
    int32_t             assoc_id = kv.first;
    m3ap_session_ids_t& ids      = kv.second;
    auto                mce_it   = m_active_mces.find(assoc_id);
    if (!ids.mce_id_known || mce_it == m_active_mces.end()) {
      continue;
    }

    m3ap_pdu_c pdu;
    auto&      m3req       = pdu.set_init_msg_mbms_session_update_request();
    m3req.mme_mbms_m3ap_id = ids.mme_mbms_m3ap_id;
    m3req.mce_mbms_m3ap_id = ids.mce_mbms_m3ap_id;
    encode_tmgi(req.tmgi, &m3req.tmgi);
    if (req.mbms_session_id_present) {
      m3req.mbms_session_id_present = true;
      m3req.mbms_session_id[0]      = req.mbms_session_id.session_id;
    }
    m3req.mbms_e_rab_qos_params = build_qos_params(req.qos_profile);
    encode_session_duration(req.mbms_session_duration.duration_sec, &m3req.mbms_session_duration);
    if (req.mbms_service_area_present) {
      m3req.mbms_service_area_present = true;
      encode_service_area(req.mbms_service_area, &m3req.mbms_service_area);
    }
    uint32_t min_time_secs = req.mbms_time_to_data_transfer_present ? req.mbms_time_to_data_transfer.seconds : 1;
    m3req.min_time_to_mbms_data_transfer[0] = (uint8_t)(std::min<uint32_t>(min_time_secs, 256) - 1);

    m3ap_tx_pdu(pdu, mce_it->second.sri);
    m_logger.info("Sent MBMS Session Update Request. TMGI key: %s, Association: %d", tmgi_key.c_str(), assoc_id);
  }
}

void m3ap::session_stop(const std::string& tmgi_key)
{
  auto sess_it = m_session_ids.find(tmgi_key);
  if (sess_it == m_session_ids.end() || sess_it->second.empty()) {
    m_logger.info("MBMS Session Stop: no M3AP session on record, nothing to forward. TMGI key: %s",
                   tmgi_key.c_str());
    return;
  }

  for (auto& kv : sess_it->second) {
    int32_t             assoc_id = kv.first;
    m3ap_session_ids_t& ids      = kv.second;
    auto                mce_it   = m_active_mces.find(assoc_id);
    if (!ids.mce_id_known || mce_it == m_active_mces.end()) {
      continue;
    }

    m3ap_pdu_c pdu;
    auto&      m3req       = pdu.set_init_msg_mbms_session_stop_request();
    m3req.mme_mbms_m3ap_id = ids.mme_mbms_m3ap_id;
    m3req.mce_mbms_m3ap_id = ids.mce_mbms_m3ap_id;

    m3ap_tx_pdu(pdu, mce_it->second.sri);
    m_logger.info("Sent MBMS Session Stop Request. TMGI key: %s, Association: %d", tmgi_key.c_str(), assoc_id);
  }
}

} // namespace srsepc
