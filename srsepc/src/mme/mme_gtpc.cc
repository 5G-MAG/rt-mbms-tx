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

#include "srsepc/hdr/mme/mme_gtpc.h"
#include "srsepc/hdr/mme/m3ap.h"
#include "srsepc/hdr/mme/s1ap.h"
#include "srsepc/hdr/spgw/spgw.h"
#include "srsran/asn1/gtpc.h"
#include <arpa/inet.h>
#include <inttypes.h> // for printing uint64_t
#include <unistd.h>   // for close()

namespace srsepc {

mme_gtpc* mme_gtpc::get_instance()
{
  static std::unique_ptr<mme_gtpc> instance = std::unique_ptr<mme_gtpc>(new mme_gtpc);
  return instance.get();
}

bool mme_gtpc::init()
{
  m_next_ctrl_teid = 1;

  m_s1ap = s1ap::get_instance();

  if (!init_s11()) {
    m_logger.error("Error Initializing MME S11 Interface");
    return false;
  }

  m_logger.info("MME GTP-C Initialized");
  srsran::console("MME GTP-C Initialized\n");
  return true;
}

bool mme_gtpc::init_s11()
{

  socklen_t sock_len;
  char      mme_addr_name[]  = "@mme_s11";
  char      spgw_addr_name[] = "@spgw_s11";

  // Logs
  m_logger.info("Initializing MME S11 interface.");

  // Open Socket
  m_s11 = socket(AF_UNIX, SOCK_DGRAM, 0);
  if (m_s11 < 0) {
    m_logger.error("Error opening UNIX socket. Error %s", strerror(errno));
    return false;
  }

  // Set MME Address
  memset(&m_mme_addr, 0, sizeof(struct sockaddr_un));
  m_mme_addr.sun_family = AF_UNIX;
  snprintf(m_mme_addr.sun_path, sizeof(m_mme_addr.sun_path), "%s", mme_addr_name);
  m_mme_addr.sun_path[0] = '\0';

  // Bind socket to address
  if (bind(m_s11, (const struct sockaddr*)&m_mme_addr, sizeof(m_mme_addr)) == -1) {
    m_logger.error("Error binding UNIX socket. Error %s", strerror(errno));
    return false;
  }

  // Set SPGW Address for later use
  memset(&m_spgw_addr, 0, sizeof(struct sockaddr_un));
  m_spgw_addr.sun_family = AF_UNIX;
  snprintf(m_spgw_addr.sun_path, sizeof(m_spgw_addr.sun_path), "%s", spgw_addr_name);
  m_spgw_addr.sun_path[0] = '\0';

  m_logger.info("MME S11 Initialized");
  srsran::console("MME S11 Initialized\n");
  return true;
}

bool mme_gtpc::send_s11_pdu(const srsran::gtpc_pdu& pdu)
{
  int n;
  m_logger.debug("Sending S-11 GTP-C PDU");

  // TODO Add GTP-C serialization code
  // Send S11 message to SPGW
  n = sendto(m_s11, &pdu, sizeof(pdu), 0, (const sockaddr*)&m_spgw_addr, sizeof(m_spgw_addr));
  if (n < 0) {
    m_logger.error("Error sending to socket. Error %s", strerror(errno));
    srsran::console("Error sending to socket. Error %s\n", strerror(errno));
    return false;
  } else {
    m_logger.debug("MME S11 Sent %d Bytes.", n);
  }
  return true;
}

void mme_gtpc::handle_s11_pdu(srsran::byte_buffer_t* msg)
{
  m_logger.debug("Received S11 message");

  srsran::gtpc_pdu* pdu;
  pdu = (srsran::gtpc_pdu*)msg->msg;
  m_logger.debug("MME Received GTP-C PDU. Message type %s", srsran::gtpc_msg_type_to_str(pdu->header.type));
  switch (pdu->header.type) {
    case srsran::GTPC_MSG_TYPE_CREATE_SESSION_RESPONSE:
      handle_create_session_response(pdu);
      break;
    case srsran::GTPC_MSG_TYPE_MODIFY_BEARER_RESPONSE:
      handle_modify_bearer_response(pdu);
      break;
    case srsran::GTPC_MSG_TYPE_DOWNLINK_DATA_NOTIFICATION:
      handle_downlink_data_notification(pdu);
      break;
    default:
      m_logger.error("Unhandled GTP-C Message type");
  }
  return;
}

bool mme_gtpc::send_create_session_request(uint64_t imsi)
{
  m_logger.info("Sending Create Session Request.");
  srsran::console("Sending Create Session Request.\n");
  struct srsran::gtpc_pdu cs_req_pdu;
  // Initialize GTP-C message to zero
  std::memset(&cs_req_pdu, 0, sizeof(cs_req_pdu));

  struct srsran::gtpc_create_session_request* cs_req = &cs_req_pdu.choice.create_session_request;

  // Setup GTP-C Header. TODO: Length, sequence and other fields need to be added.
  cs_req_pdu.header.piggyback    = false;
  cs_req_pdu.header.teid_present = true;
  cs_req_pdu.header.teid         = 0; // Send create session request to the butler TEID
  cs_req_pdu.header.type         = srsran::GTPC_MSG_TYPE_CREATE_SESSION_REQUEST;

  // Setup GTP-C Create Session Request IEs
  cs_req->imsi = imsi;
  // Control TEID allocated
  cs_req->sender_f_teid.teid = get_new_ctrl_teid();

  m_logger.info("Next MME control TEID: %d", m_next_ctrl_teid);
  m_logger.info("Allocated MME control TEID: %d", cs_req->sender_f_teid.teid);
  srsran::console("Creating Session Response -- IMSI: %" PRIu64 "\n", imsi);
  srsran::console("Creating Session Response -- MME control TEID: %d\n", cs_req->sender_f_teid.teid);

  // APN
  strncpy(cs_req->apn, m_s1ap->m_s1ap_args.mme_apn.c_str(), sizeof(cs_req->apn) - 1);
  cs_req->apn[sizeof(cs_req->apn) - 1] = 0;

  // RAT Type
  // cs_req->rat_type = srsran::GTPC_RAT_TYPE::EUTRAN;

  // Bearer QoS
  cs_req->eps_bearer_context_created.ebi = 5;

  // Check whether this UE is already registed
  std::map<uint64_t, struct gtpc_ctx>::iterator it = m_imsi_to_gtpc_ctx.find(imsi);
  if (it != m_imsi_to_gtpc_ctx.end()) {
    m_logger.warning("Create Session Request being called for an UE with an active GTP-C connection.");
    m_logger.warning("Deleting previous GTP-C connection.");
    std::map<uint32_t, uint64_t>::iterator jt = m_mme_ctr_teid_to_imsi.find(it->second.mme_ctr_fteid.teid);
    if (jt == m_mme_ctr_teid_to_imsi.end()) {
      m_logger.error("Could not find IMSI from MME Ctrl TEID. MME Ctr TEID: %d", it->second.mme_ctr_fteid.teid);
    } else {
      m_mme_ctr_teid_to_imsi.erase(jt);
    }
    m_imsi_to_gtpc_ctx.erase(it);
    // No need to send delete session request to the SPGW.
    // The create session request will be interpreted as a new request and SPGW will delete locally in existing context.
  }

  // Save RX Control TEID
  m_mme_ctr_teid_to_imsi.insert(std::pair<uint32_t, uint64_t>(cs_req->sender_f_teid.teid, imsi));

  // Save GTP-C context
  gtpc_ctx_t gtpc_ctx;
  std::memset(&gtpc_ctx, 0, sizeof(gtpc_ctx_t));
  gtpc_ctx.mme_ctr_fteid = cs_req->sender_f_teid;
  m_imsi_to_gtpc_ctx.insert(std::pair<uint64_t, gtpc_ctx_t>(imsi, gtpc_ctx));

  // Send msg to SPGW
  send_s11_pdu(cs_req_pdu);
  return true;
}

bool mme_gtpc::handle_create_session_response(srsran::gtpc_pdu* cs_resp_pdu)
{
  struct srsran::gtpc_create_session_response* cs_resp = &cs_resp_pdu->choice.create_session_response;
  m_logger.info("Received Create Session Response");
  srsran::console("Received Create Session Response\n");
  if (cs_resp_pdu->header.type != srsran::GTPC_MSG_TYPE_CREATE_SESSION_RESPONSE) {
    m_logger.warning("Could not create GTPC session. Not a create session response");
    // TODO Handle error
    return false;
  }
  if (cs_resp->cause.cause_value != srsran::GTPC_CAUSE_VALUE_REQUEST_ACCEPTED) {
    m_logger.warning("Could not create GTPC session. Create Session Request not accepted");
    // TODO Handle error
    return false;
  }

  // Get IMSI from the control TEID
  std::map<uint32_t, uint64_t>::iterator id_it = m_mme_ctr_teid_to_imsi.find(cs_resp_pdu->header.teid);
  if (id_it == m_mme_ctr_teid_to_imsi.end()) {
    m_logger.warning("Could not find IMSI from Ctrl TEID.");
    return false;
  }
  uint64_t imsi = id_it->second;

  m_logger.info("MME GTPC Ctrl TEID %" PRIu64 ", IMSI %" PRIu64 "", cs_resp_pdu->header.teid, imsi);

  // Get S-GW Control F-TEID
  srsran::gtp_fteid_t sgw_ctr_fteid = {};
  sgw_ctr_fteid.teid                = cs_resp_pdu->header.teid;
  sgw_ctr_fteid.ipv4 = 0; // TODO This is not used for now. In the future it will be obtained from the socket addr_info

  // Get S-GW S1-u F-TEID
  if (cs_resp->eps_bearer_context_created.s1_u_sgw_f_teid_present == false) {
    m_logger.error("Did not receive SGW S1-U F-TEID in create session response");
    return false;
  }
  srsran::console("Create Session Response -- SPGW control TEID %d\n", sgw_ctr_fteid.teid);
  m_logger.info("Create Session Response -- SPGW control TEID %d", sgw_ctr_fteid.teid);
  in_addr s1u_addr;
  s1u_addr.s_addr = cs_resp->eps_bearer_context_created.s1_u_sgw_f_teid.ipv4;
  srsran::console("Create Session Response -- SPGW S1-U Address: %s\n", inet_ntoa(s1u_addr));
  m_logger.info("Create Session Response -- SPGW S1-U Address: %s", inet_ntoa(s1u_addr));

  // Check UE Ipv4 address was allocated
  if (cs_resp->paa_present != true) {
    m_logger.error("PDN Adress Allocation not present");
    return false;
  }
  if (cs_resp->paa.pdn_type != srsran::GTPC_PDN_TYPE_IPV4) {
    m_logger.error("IPv6 not supported yet");
    return false;
  }

  // Save create session response info to E-RAB context
  nas* nas_ctx = m_s1ap->find_nas_ctx_from_imsi(imsi);
  if (nas_ctx == NULL) {
    m_logger.error("Could not find UE context. IMSI %015" PRIu64 "", imsi);
    return false;
  }
  emm_ctx_t* emm_ctx = &nas_ctx->m_emm_ctx;
  ecm_ctx_t* ecm_ctx = &nas_ctx->m_ecm_ctx;

  // Save UE IP to nas ctxt
  emm_ctx->ue_ip.s_addr = cs_resp->paa.ipv4;
  srsran::console("SPGW Allocated IP %s to IMSI %015" PRIu64 "\n", inet_ntoa(emm_ctx->ue_ip), emm_ctx->imsi);

  // Save SGW ctrl F-TEID in GTP-C context
  std::map<uint64_t, struct gtpc_ctx>::iterator it_g = m_imsi_to_gtpc_ctx.find(imsi);
  if (it_g == m_imsi_to_gtpc_ctx.end()) {
    // Could not find GTP-C Context
    m_logger.error("Could not find GTP-C context");
    return false;
  }
  gtpc_ctx_t* gtpc_ctx    = &it_g->second;
  gtpc_ctx->sgw_ctr_fteid = sgw_ctr_fteid;

  // Set EPS bearer context
  // TODO default EPS bearer is hard-coded
  int        default_bearer = 5;
  esm_ctx_t* esm_ctx        = &nas_ctx->m_esm_ctx[default_bearer];
  esm_ctx->pdn_addr_alloc   = cs_resp->paa;
  esm_ctx->sgw_s1u_fteid    = cs_resp->eps_bearer_context_created.s1_u_sgw_f_teid;
  m_s1ap->m_s1ap_ctx_mngmt_proc->send_initial_context_setup_request(nas_ctx, default_bearer);
  return true;
}

bool mme_gtpc::send_modify_bearer_request(uint64_t imsi, uint16_t erab_to_modify, srsran::gtp_fteid_t* enb_fteid)
{
  m_logger.info("Sending GTP-C Modify bearer request");
  srsran::gtpc_pdu mb_req_pdu;
  std::memset(&mb_req_pdu, 0, sizeof(mb_req_pdu));

  std::map<uint64_t, gtpc_ctx_t>::iterator it = m_imsi_to_gtpc_ctx.find(imsi);
  if (it == m_imsi_to_gtpc_ctx.end()) {
    m_logger.error("Modify bearer request for UE without GTP-C connection");
    return false;
  }
  srsran::gtp_fteid_t sgw_ctr_fteid = it->second.sgw_ctr_fteid;

  srsran::gtpc_header* header = &mb_req_pdu.header;
  header->teid_present        = true;
  header->teid                = sgw_ctr_fteid.teid;
  header->type                = srsran::GTPC_MSG_TYPE_MODIFY_BEARER_REQUEST;

  srsran::gtpc_modify_bearer_request* mb_req                = &mb_req_pdu.choice.modify_bearer_request;
  mb_req->eps_bearer_context_to_modify.ebi                  = erab_to_modify;
  mb_req->eps_bearer_context_to_modify.s1_u_enb_f_teid.ipv4 = enb_fteid->ipv4;
  mb_req->eps_bearer_context_to_modify.s1_u_enb_f_teid.teid = enb_fteid->teid;

  m_logger.info("GTP-C Modify bearer request -- S-GW Control TEID %d", sgw_ctr_fteid.teid);
  struct in_addr addr;
  addr.s_addr = enb_fteid->ipv4;
  m_logger.info("GTP-C Modify bearer request -- S1-U TEID 0x%x, IP %s", enb_fteid->teid, inet_ntoa(addr));

  // Send msg to SPGW
  send_s11_pdu(mb_req_pdu);
  return true;
}

void mme_gtpc::handle_modify_bearer_response(srsran::gtpc_pdu* mb_resp_pdu)
{
  uint32_t                               mme_ctrl_teid = mb_resp_pdu->header.teid;
  std::map<uint32_t, uint64_t>::iterator imsi_it       = m_mme_ctr_teid_to_imsi.find(mme_ctrl_teid);
  if (imsi_it == m_mme_ctr_teid_to_imsi.end()) {
    m_logger.error("Could not find IMSI from control TEID");
    return;
  }

  uint8_t ebi = mb_resp_pdu->choice.modify_bearer_response.eps_bearer_context_modified.ebi;
  m_logger.debug("Activating EPS bearer with id %d", ebi);
  m_s1ap->activate_eps_bearer(imsi_it->second, ebi);

  return;
}

bool mme_gtpc::send_delete_session_request(uint64_t imsi)
{
  m_logger.info("Sending GTP-C Delete Session Request request. IMSI %" PRIu64 "", imsi);
  srsran::gtpc_pdu del_req_pdu;
  std::memset(&del_req_pdu, 0, sizeof(del_req_pdu));
  srsran::gtp_fteid_t sgw_ctr_fteid;
  srsran::gtp_fteid_t mme_ctr_fteid;

  // Get S-GW Ctr TEID
  std::map<uint64_t, gtpc_ctx_t>::iterator it_ctx = m_imsi_to_gtpc_ctx.find(imsi);
  if (it_ctx == m_imsi_to_gtpc_ctx.end()) {
    m_logger.error("Could not find GTP-C context to remove");
    return false;
  }

  sgw_ctr_fteid               = it_ctx->second.sgw_ctr_fteid;
  mme_ctr_fteid               = it_ctx->second.mme_ctr_fteid;
  srsran::gtpc_header* header = &del_req_pdu.header;
  header->teid_present        = true;
  header->teid                = sgw_ctr_fteid.teid;
  header->type                = srsran::GTPC_MSG_TYPE_DELETE_SESSION_REQUEST;

  srsran::gtpc_delete_session_request* del_req = &del_req_pdu.choice.delete_session_request;
  del_req->cause.cause_value                   = srsran::GTPC_CAUSE_VALUE_ISR_DEACTIVATION;
  m_logger.info("GTP-C Delete Session Request -- S-GW Control TEID %d", sgw_ctr_fteid.teid);

  // Send msg to SPGW
  send_s11_pdu(del_req_pdu);

  // Delete GTP-C context
  std::map<uint32_t, uint64_t>::iterator it_imsi = m_mme_ctr_teid_to_imsi.find(mme_ctr_fteid.teid);
  if (it_imsi == m_mme_ctr_teid_to_imsi.end()) {
    m_logger.error("Could not find IMSI from MME ctr TEID");
  } else {
    m_mme_ctr_teid_to_imsi.erase(it_imsi);
  }
  m_imsi_to_gtpc_ctx.erase(it_ctx);
  return true;
}

void mme_gtpc::send_release_access_bearers_request(uint64_t imsi)
{
  // The GTP-C connection will not be torn down, just the user plane bearers.
  m_logger.info("Sending GTP-C Release Access Bearers Request");
  srsran::gtpc_pdu rel_req_pdu;
  std::memset(&rel_req_pdu, 0, sizeof(rel_req_pdu));
  srsran::gtp_fteid_t sgw_ctr_fteid;

  // Get S-GW Ctr TEID
  std::map<uint64_t, gtpc_ctx_t>::iterator it_ctx = m_imsi_to_gtpc_ctx.find(imsi);
  if (it_ctx == m_imsi_to_gtpc_ctx.end()) {
    m_logger.error("Could not find GTP-C context to remove");
    return;
  }
  sgw_ctr_fteid = it_ctx->second.sgw_ctr_fteid;

  // Set GTP-C header
  srsran::gtpc_header* header = &rel_req_pdu.header;
  header->teid_present        = true;
  header->teid                = sgw_ctr_fteid.teid;
  header->type                = srsran::GTPC_MSG_TYPE_RELEASE_ACCESS_BEARERS_REQUEST;

  srsran::gtpc_release_access_bearers_request* rel_req = &rel_req_pdu.choice.release_access_bearers_request;
  m_logger.info("GTP-C Release Access Berarers Request -- S-GW Control TEID %d", sgw_ctr_fteid.teid);

  // Send msg to SPGW
  send_s11_pdu(rel_req_pdu);

  return;
}

bool mme_gtpc::handle_downlink_data_notification(srsran::gtpc_pdu* dl_not_pdu)
{
  uint32_t                                 mme_ctrl_teid = dl_not_pdu->header.teid;
  srsran::gtpc_downlink_data_notification* dl_not        = &dl_not_pdu->choice.downlink_data_notification;
  std::map<uint32_t, uint64_t>::iterator   imsi_it       = m_mme_ctr_teid_to_imsi.find(mme_ctrl_teid);
  if (imsi_it == m_mme_ctr_teid_to_imsi.end()) {
    m_logger.error("Could not find IMSI from control TEID");
    return false;
  }

  if (!dl_not->eps_bearer_id_present) {
    m_logger.error("No EPS bearer Id in downlink data notification");
    return false;
  }
  uint8_t ebi = dl_not->eps_bearer_id;
  m_logger.debug("Downlink Data Notification -- IMSI: %015" PRIu64 ", EBI %d", imsi_it->second, ebi);

  m_s1ap->send_paging(imsi_it->second, ebi);
  return true;
}

void mme_gtpc::send_downlink_data_notification_acknowledge(uint64_t imsi, enum srsran::gtpc_cause_value cause)
{
  m_logger.debug("Sending GTP-C Data Notification Acknowledge. Cause %d", cause);
  srsran::gtpc_pdu    not_ack_pdu;
  srsran::gtp_fteid_t sgw_ctr_fteid;
  std::memset(&not_ack_pdu, 0, sizeof(not_ack_pdu));

  // get s-gw ctr teid
  std::map<uint64_t, gtpc_ctx_t>::iterator it_ctx = m_imsi_to_gtpc_ctx.find(imsi);
  if (it_ctx == m_imsi_to_gtpc_ctx.end()) {
    m_logger.error("could not find gtp-c context to remove");
    return;
  }
  sgw_ctr_fteid = it_ctx->second.sgw_ctr_fteid;

  // set gtp-c header
  srsran::gtpc_header* header = &not_ack_pdu.header;
  header->teid_present        = true;
  header->teid                = sgw_ctr_fteid.teid;
  header->type                = srsran::GTPC_MSG_TYPE_DOWNLINK_DATA_NOTIFICATION_ACKNOWLEDGE;

  srsran::gtpc_downlink_data_notification_acknowledge* not_ack =
      &not_ack_pdu.choice.downlink_data_notification_acknowledge;
  m_logger.info("gtp-c downlink data notification acknowledge -- s-gw control teid %d", sgw_ctr_fteid.teid);

  // send msg to spgw
  send_s11_pdu(not_ack_pdu);
  return;
}

bool mme_gtpc::send_downlink_data_notification_failure_indication(uint64_t imsi, enum srsran::gtpc_cause_value cause)
{
  m_logger.debug("Sending GTP-C Data Notification Failure Indication. Cause %d", cause);
  srsran::gtpc_pdu    not_fail_pdu;
  srsran::gtp_fteid_t sgw_ctr_fteid;
  std::memset(&not_fail_pdu, 0, sizeof(not_fail_pdu));

  // get s-gw ctr teid
  std::map<uint64_t, gtpc_ctx_t>::iterator it_ctx = m_imsi_to_gtpc_ctx.find(imsi);
  if (it_ctx == m_imsi_to_gtpc_ctx.end()) {
    m_logger.error("could not find gtp-c context to send paging failure");
    return false;
  }
  sgw_ctr_fteid = it_ctx->second.sgw_ctr_fteid;

  // set gtp-c header
  srsran::gtpc_header* header = &not_fail_pdu.header;
  header->teid_present        = true;
  header->teid                = sgw_ctr_fteid.teid;
  header->type                = srsran::GTPC_MSG_TYPE_DOWNLINK_DATA_NOTIFICATION_FAILURE_INDICATION;

  srsran::gtpc_downlink_data_notification_failure_indication* not_fail =
      &not_fail_pdu.choice.downlink_data_notification_failure_indication;
  not_fail->cause.cause_value = cause;
  m_logger.info("Downlink Data Notification Failure Indication -- SP-GW control teid %d", sgw_ctr_fteid.teid);

  // send msg to spgw
  send_s11_pdu(not_fail_pdu);
  return true;
}

/****************************************************************************
 * Sm interface (MME <-> MBMS-GW session control, TS 23.246 / TS 29.274).
 * MME is purely reactive: every bearer-context attribute comes from the
 * received Request; it never allocates a sequence number and never
 * retransmits (only the MBMS-GW side does, per TS 29.274 clause 7.6).
 ***************************************************************************/
std::string mme_gtpc::tmgi_key(const srsran::gtpc_tmgi_ie& tmgi)
{
  char buf[64];
  snprintf(buf, sizeof(buf), "%04x:%04x:%06x", tmgi.mcc_bcd, tmgi.mnc_bcd, tmgi.mbms_service_id & 0xFFFFFF);
  return std::string(buf);
}

bool mme_gtpc::init_sm(const std::string& bind_addr, uint16_t bind_port)
{
  m_sm = socket(AF_INET, SOCK_DGRAM, 0);
  if (m_sm < 0) {
    m_logger.error("Error opening Sm UDP socket. Error %s", strerror(errno));
    return false;
  }
  struct sockaddr_in addr = {};
  addr.sin_family         = AF_INET;
  addr.sin_port           = htons(bind_port);
  if (inet_pton(AF_INET, bind_addr.c_str(), &addr.sin_addr) != 1) {
    m_logger.error("Invalid Sm bind address: %s", bind_addr.c_str());
    close(m_sm);
    m_sm = -1;
    return false;
  }
  m_sm_bind_ipv4 = addr.sin_addr.s_addr;
  if (bind(m_sm, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    m_logger.error("Error binding Sm UDP socket to %s:%d. Error %s", bind_addr.c_str(), bind_port, strerror(errno));
    close(m_sm);
    m_sm = -1;
    return false;
  }
  m_logger.info("MME Sm Interface Initialized on %s:%d", bind_addr.c_str(), bind_port);
  srsran::console("MME Sm Interface Initialized on %s:%d\n", bind_addr.c_str(), bind_port);
  return true;
}

void mme_gtpc::handle_sm_pdu(srsran::byte_buffer_t* msg, const struct sockaddr_in& from_addr)
{
  srsran::gtpc_header_t header = {};
  if (srsran::gtpc_header_unpack(*msg, &header) != SRSRAN_SUCCESS) {
    m_logger.error("Error unpacking Sm GTP-C header");
    return;
  }
  m_logger.debug("Received Sm message. Type: %s", srsran::gtpc_msg_type_to_str(header.type));
  const uint8_t* body_ptr = &msg->msg[12];
  uint32_t       body_len = header.length >= 8 ? header.length - 8 : 0;

  switch (header.type) {
    case srsran::GTPC_MSG_TYPE_MBMS_SESSION_START_REQUEST: {
      srsran::gtpc_mbms_session_start_request req;
      if (srsran::gtpc_unpack_mbms_session_start_request(body_ptr, body_len, &req) != SRSRAN_SUCCESS) {
        m_logger.error("Error unpacking MBMS Session Start Request");
        return;
      }
      handle_mbms_session_start_request(req, header, from_addr);
      break;
    }
    case srsran::GTPC_MSG_TYPE_MBMS_SESSION_UPDATE_REQUEST: {
      srsran::gtpc_mbms_session_update_request req;
      if (srsran::gtpc_unpack_mbms_session_update_request(body_ptr, body_len, &req) != SRSRAN_SUCCESS) {
        m_logger.error("Error unpacking MBMS Session Update Request");
        return;
      }
      handle_mbms_session_update_request(req, header, from_addr);
      break;
    }
    case srsran::GTPC_MSG_TYPE_MBMS_SESSION_STOP_REQUEST: {
      srsran::gtpc_mbms_session_stop_request req;
      if (srsran::gtpc_unpack_mbms_session_stop_request(body_ptr, body_len, &req) != SRSRAN_SUCCESS) {
        m_logger.error("Error unpacking MBMS Session Stop Request");
        return;
      }
      handle_mbms_session_stop_request(req, header, from_addr);
      break;
    }
    default:
      m_logger.error("Unhandled Sm GTP-C Message type %s", srsran::gtpc_msg_type_to_str(header.type));
  }
}

void mme_gtpc::handle_mbms_session_start_request(const srsran::gtpc_mbms_session_start_request& req,
                                                  const srsran::gtpc_header_t&                   req_header,
                                                  const struct sockaddr_in&                      from_addr)
{
  std::string key = tmgi_key(req.tmgi);
  m_logger.info("MBMS Session Start Request. TMGI key: %s", key.c_str());

  mbms_gtpc_ctx_t ctx;
  ctx.tmgi               = req.tmgi;
  ctx.flow_id_present    = req.mbms_flow_id_present;
  ctx.flow_id            = req.mbms_flow_id.flow_id;
  ctx.session_id_present = req.mbms_session_id_present;
  ctx.session_id         = req.mbms_session_id.session_id;
  ctx.state              = MME_MBMS_BEARER_ACTIVE;
  ctx.peer_c_teid        = req.sender_f_teid.teid;
  ctx.local_teid         = m_next_mbms_local_teid++;
  ctx.peer_addr          = from_addr;
  m_tmgi_to_mbms_ctx[key] = ctx;

  // Real M3AP attachment point: forward to every connected MCE (eNB) concurrently. The Sm response below is
  // NOT gated on this round trip -- TS 23.246 clause 8.3.2 step 6 permits responding "as soon as accepted by
  // one E-UTRAN node", and mbms_session_start_confirm() (called back by m3ap once a real Response/Failure
  // arrives) only updates confirmed_enbs/logs, it does not affect the already-sent Sm response.
  m3ap::get_instance()->session_start(key, ctx, req);

  srsran::gtpc_mbms_session_start_response resp = {};
  resp.cause.cause_value                        = srsran::GTPC_CAUSE_VALUE_REQUEST_ACCEPTED;
  resp.sender_f_teid.ipv4_present                = true;
  resp.sender_f_teid.interface_type              = srsran::SM_MME_GTP_C_INTERFACE;
  resp.sender_f_teid.teid                        = ctx.local_teid;
  resp.sender_f_teid.ipv4                        = m_sm_bind_ipv4;

  srsran::byte_buffer_t body;
  if (srsran::gtpc_pack_mbms_session_start_response(resp, &body) != SRSRAN_SUCCESS) {
    m_logger.error("Error packing MBMS Session Start Response");
    return;
  }
  srsran::gtpc_header_t resp_header = {};
  resp_header.version               = srsran::GTPC_V2;
  resp_header.teid_present          = true;
  resp_header.type                  = srsran::GTPC_MSG_TYPE_MBMS_SESSION_START_RESPONSE;
  resp_header.teid                  = ctx.peer_c_teid;
  resp_header.sequence              = req_header.sequence; // echo, per TS 29.274 clause 7.6

  srsran::byte_buffer_t full;
  if (srsran::gtpc_header_pack(resp_header, body.N_bytes, &full) != SRSRAN_SUCCESS) {
    m_logger.error("Error packing Sm response header");
    return;
  }
  full.append_bytes(body.msg, body.N_bytes);
  if (sendto(m_sm, full.msg, full.N_bytes, 0, (const struct sockaddr*)&from_addr, sizeof(from_addr)) < 0) {
    m_logger.error("Error sending MBMS Session Start Response. Error %s", strerror(errno));
  }
}

void mme_gtpc::handle_mbms_session_update_request(const srsran::gtpc_mbms_session_update_request& req,
                                                    const srsran::gtpc_header_t&                    req_header,
                                                    const struct sockaddr_in&                       from_addr)
{
  std::string key = tmgi_key(req.tmgi);
  m_logger.info("MBMS Session Update Request. TMGI key: %s", key.c_str());

  auto it = m_tmgi_to_mbms_ctx.find(key);
  if (it == m_tmgi_to_mbms_ctx.end()) {
    m_logger.error("MBMS Session Update Request for unknown TMGI: %s", key.c_str());
    // Still respond, per GTPv2-C convention -- Context Not Found.
    srsran::gtpc_mbms_session_update_response resp = {};
    resp.cause.cause_value                          = srsran::GTPC_CAUSE_VALUE_CONTEXT_NOT_FOUND;
    srsran::byte_buffer_t body;
    if (srsran::gtpc_pack_mbms_session_update_response(resp, &body) == SRSRAN_SUCCESS) {
      srsran::gtpc_header_t resp_header = {};
      resp_header.version               = srsran::GTPC_V2;
      resp_header.teid_present          = true;
      resp_header.type                  = srsran::GTPC_MSG_TYPE_MBMS_SESSION_UPDATE_RESPONSE;
      resp_header.teid                  = req.sender_f_teid_present ? req.sender_f_teid.teid : 0;
      resp_header.sequence              = req_header.sequence;
      srsran::byte_buffer_t full;
      if (srsran::gtpc_header_pack(resp_header, body.N_bytes, &full) == SRSRAN_SUCCESS) {
        full.append_bytes(body.msg, body.N_bytes);
        sendto(m_sm, full.msg, full.N_bytes, 0, (const struct sockaddr*)&from_addr, sizeof(from_addr));
      }
    }
    return;
  }

  // Correlation: TMGI+Session Identifier when the request carries one (TS
  // 23.246 clause 8.8.4 step 1), else TMGI+FlowID (flagged inference, since
  // this fallback rule itself isn't verified clause text -- see project
  // notes). The TMGI-keyed map lookup above already matched on TMGI; this
  // additional check catches the case of two sessions sharing a TMGI+FlowID
  // but distinguished by Session Identifier.
  if (req.mbms_session_id_present && it->second.session_id_present &&
      req.mbms_session_id.session_id != it->second.session_id) {
    m_logger.warning("MBMS Session Update Request Session Identifier mismatch for TMGI %s", key.c_str());
  }

  it->second.state = MME_MBMS_BEARER_UPDATING;
  if (req.mbms_flow_id_present) {
    it->second.flow_id_present = true;
    it->second.flow_id         = req.mbms_flow_id.flow_id;
  }
  if (req.mbms_session_id_present) {
    it->second.session_id_present = true;
    it->second.session_id         = req.mbms_session_id.session_id;
  }
  it->second.peer_addr = from_addr;
  it->second.state     = MME_MBMS_BEARER_ACTIVE;

  m3ap::get_instance()->session_update(key, it->second, req);

  srsran::gtpc_mbms_session_update_response resp = {};
  resp.cause.cause_value                          = srsran::GTPC_CAUSE_VALUE_REQUEST_ACCEPTED;

  srsran::byte_buffer_t body;
  if (srsran::gtpc_pack_mbms_session_update_response(resp, &body) != SRSRAN_SUCCESS) {
    m_logger.error("Error packing MBMS Session Update Response");
    return;
  }
  srsran::gtpc_header_t resp_header = {};
  resp_header.version               = srsran::GTPC_V2;
  resp_header.teid_present          = true;
  resp_header.type                  = srsran::GTPC_MSG_TYPE_MBMS_SESSION_UPDATE_RESPONSE;
  resp_header.teid                  = it->second.peer_c_teid;
  resp_header.sequence              = req_header.sequence;

  srsran::byte_buffer_t full;
  if (srsran::gtpc_header_pack(resp_header, body.N_bytes, &full) != SRSRAN_SUCCESS) {
    m_logger.error("Error packing Sm response header");
    return;
  }
  full.append_bytes(body.msg, body.N_bytes);
  if (sendto(m_sm, full.msg, full.N_bytes, 0, (const struct sockaddr*)&from_addr, sizeof(from_addr)) < 0) {
    m_logger.error("Error sending MBMS Session Update Response. Error %s", strerror(errno));
  }
}

void mme_gtpc::handle_mbms_session_stop_request(const srsran::gtpc_mbms_session_stop_request& req,
                                                  const srsran::gtpc_header_t&                  req_header,
                                                  const struct sockaddr_in&                     from_addr)
{
  // Correlation is by TMGI (TS 23.246 clause 8.5.2 step 1: "identified by
  // TMGI or TMGI+Flow Identifier"), not header TEID alone -- the critical
  // fix from adversarial verification against that exact clause.
  std::string key = tmgi_key(req.tmgi);
  m_logger.info("MBMS Session Stop Request. TMGI key: %s", key.c_str());

  auto     it          = m_tmgi_to_mbms_ctx.find(key);
  uint32_t peer_c_teid = 0;
  if (it != m_tmgi_to_mbms_ctx.end()) {
    // Header TEID cross-checked against the stored context as a consistency
    // check only, not used as the identifier itself.
    if (req_header.teid != 0 && req_header.teid != it->second.local_teid) {
      m_logger.warning("MBMS Session Stop Request header TEID mismatch for TMGI %s (expected %u, got %" PRIu64 ")",
                        key.c_str(),
                        it->second.local_teid,
                        req_header.teid);
    }
    peer_c_teid = it->second.peer_c_teid;
    it->second.state = MME_MBMS_BEARER_STOPPING;
  } else {
    m_logger.warning("MBMS Session Stop Request for unknown TMGI: %s", key.c_str());
  }

  // Real M3AP attachment point for TS 23.246 clause 8.5.2 step 3's MME-to-eNB Stop forwarding.
  m3ap::get_instance()->session_stop(key);

  srsran::gtpc_mbms_session_stop_response resp = {};
  resp.cause.cause_value                        = srsran::GTPC_CAUSE_VALUE_REQUEST_ACCEPTED;

  srsran::byte_buffer_t body;
  if (srsran::gtpc_pack_mbms_session_stop_response(resp, &body) != SRSRAN_SUCCESS) {
    m_logger.error("Error packing MBMS Session Stop Response");
    return;
  }
  srsran::gtpc_header_t resp_header = {};
  resp_header.version               = srsran::GTPC_V2;
  resp_header.teid_present          = true;
  resp_header.type                  = srsran::GTPC_MSG_TYPE_MBMS_SESSION_STOP_RESPONSE;
  resp_header.teid                  = peer_c_teid;
  resp_header.sequence              = req_header.sequence;

  srsran::byte_buffer_t full;
  if (srsran::gtpc_header_pack(resp_header, body.N_bytes, &full) != SRSRAN_SUCCESS) {
    m_logger.error("Error packing Sm response header");
    return;
  }
  full.append_bytes(body.msg, body.N_bytes);
  if (sendto(m_sm, full.msg, full.N_bytes, 0, (const struct sockaddr*)&from_addr, sizeof(from_addr)) < 0) {
    m_logger.error("Error sending MBMS Session Stop Response. Error %s", strerror(errno));
  }

  if (it != m_tmgi_to_mbms_ctx.end()) {
    m_tmgi_to_mbms_ctx.erase(it);
  }
}

void mme_gtpc::mbms_session_start_confirm(const std::string& key, uint8_t cause, int32_t mce_assoc_id)
{
  m_logger.info(
      "mbms_session_start_confirm: TMGI %s, cause %d, MCE association %d", key.c_str(), cause, mce_assoc_id);
  auto it = m_tmgi_to_mbms_ctx.find(key);
  if (it != m_tmgi_to_mbms_ctx.end() && cause == srsran::GTPC_CAUSE_VALUE_REQUEST_ACCEPTED) {
    it->second.confirmed_enbs.push_back(mce_assoc_id);
  }
}

void mme_gtpc::mbms_session_update_confirm(const std::string& key, uint8_t cause, int32_t mce_assoc_id)
{
  m_logger.info(
      "mbms_session_update_confirm: TMGI %s, cause %d, MCE association %d", key.c_str(), cause, mce_assoc_id);
}

void mme_gtpc::mbms_session_stop_confirm(const std::string& key, uint8_t cause, int32_t mce_assoc_id)
{
  m_logger.info(
      "mbms_session_stop_confirm: TMGI %s, cause %d, MCE association %d", key.c_str(), cause, mce_assoc_id);
}

} // namespace srsepc
