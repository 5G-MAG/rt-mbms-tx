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
#include "srsran/asn1/gtpc.h"
#include "srsran/common/bcd_helpers.h"
#include "srsran/common/byte_buffer.h"
#include "srsran/config.h"
#include <cstring>
#include <stdint.h>

namespace srsran {

const char* gtpc_msg_type_to_str(uint8_t type)
{
  switch (type) {
    case GTPC_MSG_TYPE_RESERVED:
      return "GTPC_MSG_TYPE_RESERVED";
    case GTPC_MSG_TYPE_ECHO_REQUEST:
      return "GTPC_MSG_TYPE_ECHO_REQUEST";
    case GTPC_MSG_TYPE_ECHO_RESPONSE:
      return "GTPC_MSG_TYPE_ECHO_RESPONSE";
    case GTPC_MSG_TYPE_VERSION_SUPPORT:
      return "GTPC_MSG_TYPE_VERSION_SUPPORT";
    case GTPC_MSG_TYPE_CREATE_SESSION_REQUEST:
      return "GTPC_MSG_TYPE_CREATE_SESSION_REQUEST";
    case GTPC_MSG_TYPE_CREATE_SESSION_RESPONSE:
      return "GTPC_MSG_TYPE_CREATE_SESSION_RESPONSE";
    case GTPC_MSG_TYPE_DELETE_SESSION_REQUEST:
      return "GTPC_MSG_TYPE_DELETE_SESSION_REQUEST";
    case GTPC_MSG_TYPE_DELETE_SESSION_RESPONSE:
      return "GTPC_MSG_TYPE_DELETE_SESSION_RESPONSE";
    case GTPC_MSG_TYPE_MODIFY_BEARER_REQUEST:
      return "GTPC_MSG_TYPE_MODIFY_BEARER_REQUEST";
    case GTPC_MSG_TYPE_MODIFY_BEARER_RESPONSE:
      return "GTPC_MSG_TYPE_MODIFY_BEARER_RESPONSE";
    case GTPC_MSG_TYPE_CHANGE_NOTIFICATION_REQUEST:
      return "GTPC_MSG_TYPE_CHANGE_NOTIFICATION_REQUEST";
    case GTPC_MSG_TYPE_CHANGE_NOTIFICATION_RESPONSE:
      return "GTPC_MSG_TYPE_CHANGE_NOTIFICATION_RESPONSE";
    case GTPC_MSG_TYPE_RESUME_NOTIFICATION:
      return "GTPC_MSG_TYPE_RESUME_NOTIFICATION";
    case GTPC_MSG_TYPE_RESUME_ACKNOWLEDGE:
      return "GTPC_MSG_TYPE_RESUME_ACKNOWLEDGE";
    case GTPC_MSG_TYPE_MODIFY_BEARER_COMMAND:
      return "GTPC_MSG_TYPE_MODIFY_BEARER_COMMAND";
    case GTPC_MSG_TYPE_MODIFY_BEARER_FAILURE_INDICATION:
      return "GTPC_MSG_TYPE_MODIFY_BEARER_FAILURE_INDICATION";
    case GTPC_MSG_TYPE_DELETE_BEARER_COMMAND:
      return "GTPC_MSG_TYPE_DELETE_BEARER_COMMAND";
    case GTPC_MSG_TYPE_DELETE_BEARER_FAILURE_INDICATION:
      return "GTPC_MSG_TYPE_DELETE_BEARER_FAILURE_INDICATION";
    case GTPC_MSG_TYPE_BEARER_RESOURCE_COMMAND:
      return "GTPC_MSG_TYPE_BEARER_RESOURCE_COMMAND";
    case GTPC_MSG_TYPE_BEARER_RESOURCE_FAILURE_INDICATION:
      return "GTPC_MSG_TYPE_BEARER_RESOURCE_FAILURE_INDICATION";
    case GTPC_MSG_TYPE_DOWNLINK_DATA_NOTIFICATION_FAILURE_INDICATION:
      return "GTPC_MSG_TYPE_DOWNLINK_DATA_NOTIFICATION_FAILURE_INDICATION";
    case GTPC_MSG_TYPE_TRACE_SESSION_ACTIVATION:
      return "GTPC_MSG_TYPE_TRACE_SESSION_ACTIVATION";
    case GTPC_MSG_TYPE_TRACE_SESSION_DEACTIVATION:
      return "GTPC_MSG_TYPE_TRACE_SESSION_DEACTIVATION";
    case GTPC_MSG_TYPE_STOP_PAGING_INDICATION:
      return "GTPC_MSG_TYPE_STOP_PAGING_INDICATION";
    case GTPC_MSG_TYPE_CREATE_BEARER_REQUEST:
      return "GTPC_MSG_TYPE_CREATE_BEARER_REQUEST";
    case GTPC_MSG_TYPE_CREATE_BEARER_RESPONSE:
      return "GTPC_MSG_TYPE_CREATE_BEARER_RESPONSE";
    case GTPC_MSG_TYPE_UPDATE_BEARER_REQUEST:
      return "GTPC_MSG_TYPE_UPDATE_BEARER_REQUEST";
    case GTPC_MSG_TYPE_UPDATE_BEARER_RESPONSE:
      return "GTPC_MSG_TYPE_UPDATE_BEARER_RESPONSE";
    case GTPC_MSG_TYPE_DELETE_BEARER_REQUEST:
      return "GTPC_MSG_TYPE_DELETE_BEARER_REQUEST";
    case GTPC_MSG_TYPE_DELETE_BEARER_RESPONSE:
      return "GTPC_MSG_TYPE_DELETE_BEARER_RESPONSE";
    case GTPC_MSG_TYPE_DELETE_PDN_CONNECTION_SET_REQUEST:
      return "GTPC_MSG_TYPE_DELETE_PDN_CONNECTION_SET_REQUEST";
    case GTPC_MSG_TYPE_DELETE_PDN_CONNECTION_SET_RESPONSE:
      return "GTPC_MSG_TYPE_DELETE_PDN_CONNECTION_SET_RESPONSE";
    case GTPC_MSG_TYPE_IDENTIFICATION_REQUEST:
      return "GTPC_MSG_TYPE_IDENTIFICATION_REQUEST";
    case GTPC_MSG_TYPE_IDENTIFICATION_RESPONSE:
      return "GTPC_MSG_TYPE_IDENTIFICATION_RESPONSE";
    case GTPC_MSG_TYPE_CONTEXT_REQUEST:
      return "GTPC_MSG_TYPE_CONTEXT_REQUEST";
    case GTPC_MSG_TYPE_CONTEXT_RESPONSE:
      return "GTPC_MSG_TYPE_CONTEXT_RESPONSE";
    case GTPC_MSG_TYPE_CONTEXT_ACKNOWLEDGE:
      return "GTPC_MSG_TYPE_CONTEXT_ACKNOWLEDGE";
    case GTPC_MSG_TYPE_FORWARD_RELOCATION_REQUEST:
      return "GTPC_MSG_TYPE_FORWARD_RELOCATION_REQUEST";
    case GTPC_MSG_TYPE_FORWARD_RELOCATION_RESPONSE:
      return "GTPC_MSG_TYPE_FORWARD_RELOCATION_RESPONSE";
    case GTPC_MSG_TYPE_FORWARD_RELOCATION_COMPLETE_NOTIFICATION:
      return "GTPC_MSG_TYPE_FORWARD_RELOCATION_COMPLETE_NOTIFICATION";
    case GTPC_MSG_TYPE_FORWARD_RELOCATION_COMPLETE_ACKNOWLEDGE:
      return "GTPC_MSG_TYPE_FORWARD_RELOCATION_COMPLETE_ACKNOWLEDGE";
    case GTPC_MSG_TYPE_FORWARD_ACCESS_CONTEXT_NOTIFICATION:
      return "GTPC_MSG_TYPE_FORWARD_ACCESS_CONTEXT_NOTIFICATION";
    case GTPC_MSG_TYPE_FORWARD_ACCESS_CONTEXT_ACKNOWLEDGE:
      return "GTPC_MSG_TYPE_FORWARD_ACCESS_CONTEXT_ACKNOWLEDGE";
    case GTPC_MSG_TYPE_RELOCATION_CANCEL_REQUEST:
      return "GTPC_MSG_TYPE_RELOCATION_CANCEL_REQUEST";
    case GTPC_MSG_TYPE_RELOCATION_CANCEL_RESPONSE:
      return "GTPC_MSG_TYPE_RELOCATION_CANCEL_RESPONSE";
    case GTPC_MSG_TYPE_CONFIGURATION_TRANSFER_TUNNEL:
      return "GTPC_MSG_TYPE_CONFIGURATION_TRANSFER_TUNNEL";
    case GTPC_MSG_TYPE_RAN_INFORMATION_RELAY:
      return "GTPC_MSG_TYPE_RAN_INFORMATION_RELAY";
    case GTPC_MSG_TYPE_DETACH_NOTIFICATION:
      return "GTPC_MSG_TYPE_DETACH_NOTIFICATION";
    case GTPC_MSG_TYPE_DETACH_ACKNOWLEDGE:
      return "GTPC_MSG_TYPE_DETACH_ACKNOWLEDGE";
    case GTPC_MSG_TYPE_CS_PAGING_INDICATION:
      return "GTPC_MSG_TYPE_CS_PAGING_INDICATION";
    case GTPC_MSG_TYPE_ALERT_MME_NOTIFICATION:
      return "GTPC_MSG_TYPE_ALERT_MME_NOTIFICATION";
    case GTPC_MSG_TYPE_ALERT_MME_ACKNOWLEDGE:
      return "GTPC_MSG_TYPE_ALERT_MME_ACKNOWLEDGE";
    case GTPC_MSG_TYPE_UE_ACTIVITY_NOTIFICATION:
      return "GTPC_MSG_TYPE_UE_ACTIVITY_NOTIFICATION";
    case GTPC_MSG_TYPE_UE_ACTIVITY_ACKNOWLEDGE:
      return "GTPC_MSG_TYPE_UE_ACTIVITY_ACKNOWLEDGE";
    case GTPC_MSG_TYPE_SUSPEND_NOTIFICATION:
      return "GTPC_MSG_TYPE_SUSPEND_NOTIFICATION";
    case GTPC_MSG_TYPE_SUSPEND_ACKNOWLEDGE:
      return "GTPC_MSG_TYPE_SUSPEND_ACKNOWLEDGE";
    case GTPC_MSG_TYPE_CREATE_FORWARDING_TUNNEL_REQUEST:
      return "GTPC_MSG_TYPE_CREATE_FORWARDING_TUNNEL_REQUEST";
    case GTPC_MSG_TYPE_CREATE_FORWARDING_TUNNEL_RESPONSE:
      return "GTPC_MSG_TYPE_CREATE_FORWARDING_TUNNEL_RESPONSE";
    case GTPC_MSG_TYPE_CREATE_INDIRECT_DATA_FORWARDING_TUNNEL_REQUEST:
      return "GTPC_MSG_TYPE_CREATE_INDIRECT_DATA_FORWARDING_TUNNEL_REQUEST";
    case GTPC_MSG_TYPE_CREATE_INDIRECT_DATA_FORWARDING_TUNNEL_RESPONSE:
      return "GTPC_MSG_TYPE_CREATE_INDIRECT_DATA_FORWARDING_TUNNEL_RESPONSE";
    case GTPC_MSG_TYPE_DELETE_INDIRECT_DATA_FORWARDING_TUNNEL_REQUEST:
      return "GTPC_MSG_TYPE_DELETE_INDIRECT_DATA_FORWARDING_TUNNEL_REQUEST";
    case GTPC_MSG_TYPE_DELETE_INDIRECT_DATA_FORWARDING_TUNNEL_RESPONSE:
      return "GTPC_MSG_TYPE_DELETE_INDIRECT_DATA_FORWARDING_TUNNEL_RESPONSE";
    case GTPC_MSG_TYPE_RELEASE_ACCESS_BEARERS_REQUEST:
      return "GTPC_MSG_TYPE_RELEASE_ACCESS_BEARERS_REQUEST";
    case GTPC_MSG_TYPE_RELEASE_ACCESS_BEARERS_RESPONSE:
      return "GTPC_MSG_TYPE_RELEASE_ACCESS_BEARERS_RESPONSE";
    case GTPC_MSG_TYPE_DOWNLINK_DATA_NOTIFICATION:
      return "GTPC_MSG_TYPE_DOWNLINK_DATA_NOTIFICATION";
    case GTPC_MSG_TYPE_DOWNLINK_DATA_NOTIFICATION_ACKNOWLEDGE:
      return "GTPC_MSG_TYPE_DOWNLINK_DATA_NOTIFICATION_ACKNOWLEDGE";
    case GTPC_MSG_TYPE_PGW_RESTART_NOTIFICATION:
      return "GTPC_MSG_TYPE_PGW_RESTART_NOTIFICATION";
    case GTPC_MSG_TYPE_PGW_RESTART_NOTIFICATION_ACKNOWLEDGE:
      return "GTPC_MSG_TYPE_PGW_RESTART_NOTIFICATION_ACKNOWLEDGE";
    case GTPC_MSG_TYPE_UPDATE_PDN_CONNECTION_SET_REQUEST:
      return "GTPC_MSG_TYPE_UPDATE_PDN_CONNECTION_SET_REQUEST";
    case GTPC_MSG_TYPE_UPDATE_PDN_CONNECTION_SET_RESPONSE:
      return "GTPC_MSG_TYPE_UPDATE_PDN_CONNECTION_SET_RESPONSE";
    case GTPC_MSG_TYPE_MODIFY_ACCESS_BEARERS_REQUEST:
      return "GTPC_MSG_TYPE_MODIFY_ACCESS_BEARERS_REQUEST";
    case GTPC_MSG_TYPE_MODIFY_ACCESS_BEARERS_RESPONSE:
      return "GTPC_MSG_TYPE_MODIFY_ACCESS_BEARERS_RESPONSE";
    case GTPC_MSG_TYPE_MBMS_SESSION_START_REQUEST:
      return "GTPC_MSG_TYPE_MBMS_SESSION_START_REQUEST";
    case GTPC_MSG_TYPE_MBMS_SESSION_START_RESPONSE:
      return "GTPC_MSG_TYPE_MBMS_SESSION_START_RESPONSE";
    case GTPC_MSG_TYPE_MBMS_SESSION_UPDATE_REQUEST:
      return "GTPC_MSG_TYPE_MBMS_SESSION_UPDATE_REQUEST";
    case GTPC_MSG_TYPE_MBMS_SESSION_UPDATE_RESPONSE:
      return "GTPC_MSG_TYPE_MBMS_SESSION_UPDATE_RESPONSE";
    case GTPC_MSG_TYPE_MBMS_SESSION_STOP_REQUEST:
      return "GTPC_MSG_TYPE_MBMS_SESSION_STOP_REQUEST";
    case GTPC_MSG_TYPE_MBMS_SESSION_STOP_RESPONSE:
      return "GTPC_MSG_TYPE_MBMS_SESSION_STOP_RESPONSE";
  }
  return "GTPC_MSG_TYPE_INVALID";
}

int gtpc_pack_create_session_request(struct gtpc_create_session_request* cs_req, srsran::byte_buffer_t&)
{
  // TODO
  return 0;
}

/****************************************************************************
 * GTP-C v2 EPC-specific header pack/unpack
 * Ref: 3GPP TS 29.274 v19.6.0 clause 5.1/5.4
 ***************************************************************************/
int gtpc_header_pack(const gtpc_header_t& header, uint32_t ie_body_len, srsran::byte_buffer_t* pdu)
{
  if (pdu == nullptr) {
    return SRSRAN_ERROR;
  }
  uint8_t hdr[12];
  hdr[0] = static_cast<uint8_t>(((header.version & 0x7u) << 5) | ((header.piggyback ? 1u : 0u) << 4) |
                                 ((header.teid_present ? 1u : 0u) << 3) | ((header.mp_flag ? 1u : 0u) << 2));
  hdr[1] = header.type;

  uint16_t length = static_cast<uint16_t>(8 + ie_body_len); // TEID(4) + Sequence(3) + spare/priority(1) + IE body
  hdr[2]          = static_cast<uint8_t>((length >> 8) & 0xFF);
  hdr[3]          = static_cast<uint8_t>(length & 0xFF);

  hdr[4] = static_cast<uint8_t>((header.teid >> 24) & 0xFF);
  hdr[5] = static_cast<uint8_t>((header.teid >> 16) & 0xFF);
  hdr[6] = static_cast<uint8_t>((header.teid >> 8) & 0xFF);
  hdr[7] = static_cast<uint8_t>(header.teid & 0xFF);

  hdr[8]  = static_cast<uint8_t>((header.sequence >> 16) & 0xFF);
  hdr[9]  = static_cast<uint8_t>((header.sequence >> 8) & 0xFF);
  hdr[10] = static_cast<uint8_t>(header.sequence & 0xFF);

  hdr[11] = header.mp_flag ? static_cast<uint8_t>((header.message_priority & 0xF) << 4) : 0;

  pdu->append_bytes(hdr, sizeof(hdr));
  return SRSRAN_SUCCESS;
}

int gtpc_header_unpack(const srsran::byte_buffer_t& pdu, gtpc_header_t* header)
{
  if (header == nullptr || pdu.N_bytes < 12) {
    return SRSRAN_ERROR;
  }
  const uint8_t* p = pdu.msg;

  header->version      = static_cast<uint8_t>((p[0] >> 5) & 0x7);
  header->piggyback    = ((p[0] >> 4) & 0x1) != 0;
  header->teid_present = ((p[0] >> 3) & 0x1) != 0;
  header->mp_flag      = ((p[0] >> 2) & 0x1) != 0;
  if (header->version != GTPC_V2 || !header->teid_present) {
    // Every Sm message uses the EPC-specific (T=1) GTPv2 header variant.
    return SRSRAN_ERROR;
  }
  header->type   = p[1];
  header->length = static_cast<uint16_t>((p[2] << 8) | p[3]);
  if (pdu.N_bytes < static_cast<uint32_t>(4 + header->length)) {
    // Declared length would run past the actual buffer -- truncated/malformed datagram.
    return SRSRAN_ERROR;
  }
  header->teid = (static_cast<uint64_t>(p[4]) << 24) | (static_cast<uint64_t>(p[5]) << 16) |
                 (static_cast<uint64_t>(p[6]) << 8) | static_cast<uint64_t>(p[7]);
  header->sequence =
      (static_cast<uint64_t>(p[8]) << 16) | (static_cast<uint64_t>(p[9]) << 8) | static_cast<uint64_t>(p[10]);
  header->message_priority = header->mp_flag ? static_cast<uint8_t>((p[11] >> 4) & 0xF) : 0;
  return SRSRAN_SUCCESS;
}

/****************************************************************************
 * GTP-C v2 IE header (TLIV) pack/unpack
 * Ref: 3GPP TS 29.274 v19.6.0 clause 8.1/8.2
 ***************************************************************************/
int gtpc_ie_header_pack(const gtpc_ie_header_t& ie_header, srsran::byte_buffer_t* pdu)
{
  if (pdu == nullptr) {
    return SRSRAN_ERROR;
  }
  uint8_t hdr[4];
  hdr[0] = ie_header.type;
  hdr[1] = static_cast<uint8_t>((ie_header.length >> 8) & 0xFF);
  hdr[2] = static_cast<uint8_t>(ie_header.length & 0xFF);
  hdr[3] = static_cast<uint8_t>(ie_header.instance & 0xF);
  pdu->append_bytes(hdr, sizeof(hdr));
  return SRSRAN_SUCCESS;
}

int gtpc_ie_header_unpack(const uint8_t* ptr, uint32_t remaining, gtpc_ie_header_t* ie_header)
{
  if (ptr == nullptr || ie_header == nullptr || remaining < 4) {
    return SRSRAN_ERROR;
  }
  ie_header->type     = ptr[0];
  ie_header->length   = static_cast<uint16_t>((ptr[1] << 8) | ptr[2]);
  ie_header->instance = static_cast<uint8_t>(ptr[3] & 0xF);
  return SRSRAN_SUCCESS;
}

/****************************************************************************
 * GTP-C Cause IE
 * Ref: 3GPP TS 29.274 v19.6.0 clause 8.4, Figure 8.4-1
 ***************************************************************************/
int gtpc_pack_cause_ie(const gtpc_cause_ie& cause, uint8_t instance, srsran::byte_buffer_t* pdu)
{
  if (pdu == nullptr) {
    return SRSRAN_ERROR;
  }
  const uint16_t   value_len = cause.offending_ie_present ? 6 : 2;
  gtpc_ie_header_t ie_hdr    = {GTPC_IE_TYPE_CAUSE, value_len, instance};
  if (gtpc_ie_header_pack(ie_hdr, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  uint8_t val[6];
  val[0] = static_cast<uint8_t>(cause.cause_value);
  val[1] = static_cast<uint8_t>(((cause.pce ? 1u : 0u) << 2) | ((cause.bce ? 1u : 0u) << 1) | (cause.cs ? 1u : 0u));
  if (cause.offending_ie_present) {
    val[2] = static_cast<uint8_t>(cause.offending_ie_type);
    val[3] = 0; // Length of the offending IE shall always be set to 0, per clause 8.4
    val[4] = 0;
    val[5] = static_cast<uint8_t>(cause.offending_ie_instance & 0xF);
  }
  pdu->append_bytes(val, value_len);
  return SRSRAN_SUCCESS;
}

int gtpc_unpack_cause_ie(const uint8_t* ptr, const gtpc_ie_header_t& ie_header, gtpc_cause_ie* cause)
{
  if (ptr == nullptr || cause == nullptr || (ie_header.length != 2 && ie_header.length != 6)) {
    return SRSRAN_ERROR;
  }
  cause->cause_value = static_cast<gtpc_cause_value>(ptr[0]);
  cause->pce          = ((ptr[1] >> 2) & 0x1) != 0;
  cause->bce          = ((ptr[1] >> 1) & 0x1) != 0;
  cause->cs           = (ptr[1] & 0x1) != 0;
  cause->offending_ie_present = (ie_header.length == 6);
  if (cause->offending_ie_present) {
    cause->offending_ie_type       = static_cast<gtpc_ie_type>(ptr[2]);
    cause->length_of_offending_ie  = 0;
    cause->offending_ie_instance   = static_cast<uint8_t>(ptr[5] & 0xF);
  } else {
    cause->offending_ie_type      = static_cast<gtpc_ie_type>(0);
    cause->length_of_offending_ie = 0;
    cause->offending_ie_instance  = 0;
  }
  return SRSRAN_SUCCESS;
}

/****************************************************************************
 * GTP-C Recovery (Restart Counter) IE
 * Ref: 3GPP TS 29.274 v19.6.0 clause 8.5 -- single-octet value, kept as a
 * plain uint8_t per the codebase's own existing comment (gtpc_ies.h).
 ***************************************************************************/
int gtpc_pack_recovery_ie(uint8_t restart_counter, uint8_t instance, srsran::byte_buffer_t* pdu)
{
  if (pdu == nullptr) {
    return SRSRAN_ERROR;
  }
  gtpc_ie_header_t ie_hdr = {GTPC_IE_TYPE_RECOVERY, 1, instance};
  if (gtpc_ie_header_pack(ie_hdr, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  pdu->append_bytes(&restart_counter, 1);
  return SRSRAN_SUCCESS;
}

int gtpc_unpack_recovery_ie(const uint8_t* ptr, const gtpc_ie_header_t& ie_header, uint8_t* restart_counter)
{
  if (ptr == nullptr || restart_counter == nullptr || ie_header.length < 1) {
    return SRSRAN_ERROR;
  }
  *restart_counter = ptr[0];
  return SRSRAN_SUCCESS;
}

/****************************************************************************
 * GTP-C Bearer QoS IE
 * Ref: 3GPP TS 29.274 v19.6.0 clause 8.15, Figure 8.15-1
 ***************************************************************************/
int gtpc_pack_bearer_qos_ie(const gtpc_bearer_qos_ie& qos, uint8_t instance, srsran::byte_buffer_t* pdu)
{
  if (pdu == nullptr) {
    return SRSRAN_ERROR;
  }
  gtpc_ie_header_t ie_hdr = {GTPC_IE_TYPE_BEARER_QOS, 22, instance}; // 1 ARP + 1 QCI + 4*5 rate octets
  if (gtpc_ie_header_pack(ie_hdr, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  uint8_t val[22];
  val[0] = static_cast<uint8_t>(((qos.arp.pci & 0x1) << 6) | ((qos.arp.pl & 0xF) << 2) | (qos.arp.pvi & 0x1));
  val[1] = qos.qci;
  auto pack40 = [](uint64_t v, uint8_t* dst) {
    dst[0] = static_cast<uint8_t>((v >> 32) & 0xFF);
    dst[1] = static_cast<uint8_t>((v >> 24) & 0xFF);
    dst[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    dst[3] = static_cast<uint8_t>((v >> 8) & 0xFF);
    dst[4] = static_cast<uint8_t>(v & 0xFF);
  };
  pack40(qos.mbr_ul, &val[2]);
  pack40(qos.mbr_dl, &val[7]);
  pack40(qos.gbr_ul, &val[12]);
  pack40(qos.gbr_dl, &val[17]);
  pdu->append_bytes(val, sizeof(val));
  return SRSRAN_SUCCESS;
}

int gtpc_unpack_bearer_qos_ie(const uint8_t* ptr, const gtpc_ie_header_t& ie_header, gtpc_bearer_qos_ie* qos)
{
  if (ptr == nullptr || qos == nullptr || ie_header.length < 22) {
    return SRSRAN_ERROR;
  }
  qos->arp.pvi    = ptr[0] & 0x1;
  qos->arp.spare  = 0;
  qos->arp.pl     = (ptr[0] >> 2) & 0xF;
  qos->arp.pci    = (ptr[0] >> 6) & 0x1;
  qos->arp.spare2 = 0;
  qos->qci        = ptr[1];
  auto unpack40 = [](const uint8_t* src) -> uint64_t {
    return (static_cast<uint64_t>(src[0]) << 32) | (static_cast<uint64_t>(src[1]) << 24) |
           (static_cast<uint64_t>(src[2]) << 16) | (static_cast<uint64_t>(src[3]) << 8) |
           static_cast<uint64_t>(src[4]);
  };
  qos->mbr_ul = unpack40(&ptr[2]);
  qos->mbr_dl = unpack40(&ptr[7]);
  qos->gbr_ul = unpack40(&ptr[12]);
  qos->gbr_dl = unpack40(&ptr[17]);
  return SRSRAN_SUCCESS;
}

/****************************************************************************
 * GTP-C F-TEID IE
 * Ref: 3GPP TS 29.274 v19.6.0 clause 8.22, Figure 8.22-1
 ***************************************************************************/
int gtpc_pack_f_teid_ie(const gtpc_f_teid_ie& fteid, uint8_t instance, srsran::byte_buffer_t* pdu)
{
  if (pdu == nullptr || (!fteid.ipv4_present && !fteid.ipv6_present)) {
    // At least one of V4/V6 shall be set, per clause 8.22.
    return SRSRAN_ERROR;
  }
  uint16_t value_len = 5; // octet5 (flags+iface) + 4 octets TEID/GRE key
  if (fteid.ipv4_present) {
    value_len += 4;
  }
  if (fteid.ipv6_present) {
    value_len += 16;
  }
  gtpc_ie_header_t ie_hdr = {GTPC_IE_TYPE_F_TEID, value_len, instance};
  if (gtpc_ie_header_pack(ie_hdr, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  uint8_t val[25];
  uint32_t off = 0;
  val[off++]   = static_cast<uint8_t>(((fteid.ipv4_present ? 1u : 0u) << 7) | ((fteid.ipv6_present ? 1u : 0u) << 6) |
                                     (static_cast<uint8_t>(fteid.interface_type) & 0x3F));
  val[off++]   = static_cast<uint8_t>((fteid.teid >> 24) & 0xFF);
  val[off++]   = static_cast<uint8_t>((fteid.teid >> 16) & 0xFF);
  val[off++]   = static_cast<uint8_t>((fteid.teid >> 8) & 0xFF);
  val[off++]   = static_cast<uint8_t>(fteid.teid & 0xFF);
  if (fteid.ipv4_present) {
    uint32_t ipv4_be = fteid.ipv4; // in_addr_t is already network-byte-order
    memcpy(&val[off], &ipv4_be, 4);
    off += 4;
  }
  if (fteid.ipv6_present) {
    memcpy(&val[off], fteid.ipv6.s6_addr, 16);
    off += 16;
  }
  pdu->append_bytes(val, off);
  return SRSRAN_SUCCESS;
}

int gtpc_unpack_f_teid_ie(const uint8_t* ptr, const gtpc_ie_header_t& ie_header, gtpc_f_teid_ie* fteid)
{
  if (ptr == nullptr || fteid == nullptr || ie_header.length < 5) {
    return SRSRAN_ERROR;
  }
  fteid->ipv4_present  = ((ptr[0] >> 7) & 0x1) != 0;
  fteid->ipv6_present  = ((ptr[0] >> 6) & 0x1) != 0;
  fteid->interface_type = static_cast<gtpc_interface_type>(ptr[0] & 0x3F);
  fteid->teid = (static_cast<uint32_t>(ptr[1]) << 24) | (static_cast<uint32_t>(ptr[2]) << 16) |
                (static_cast<uint32_t>(ptr[3]) << 8) | static_cast<uint32_t>(ptr[4]);
  uint32_t off             = 5;
  uint32_t expected_length = 5;
  if (fteid->ipv4_present) {
    expected_length += 4;
  }
  if (fteid->ipv6_present) {
    expected_length += 16;
  }
  if (ie_header.length < expected_length) {
    return SRSRAN_ERROR;
  }
  if (fteid->ipv4_present) {
    memcpy(&fteid->ipv4, &ptr[off], 4);
    off += 4;
  }
  if (fteid->ipv6_present) {
    memcpy(fteid->ipv6.s6_addr, &ptr[off], 16);
    off += 16;
  }
  return SRSRAN_SUCCESS;
}

/****************************************************************************
 * GTP-C Private Extension IE -- fully opaque, no clause-8.x text available.
 ***************************************************************************/
int gtpc_pack_private_extension_ie(const gtpc_opaque_ie& ext, uint8_t instance, srsran::byte_buffer_t* pdu)
{
  if (pdu == nullptr || ext.value.size() > UINT16_MAX) {
    return SRSRAN_ERROR;
  }
  gtpc_ie_header_t ie_hdr = {GTPC_IE_TYPE_PRIVATE_EXTENSION, static_cast<uint16_t>(ext.value.size()), instance};
  if (gtpc_ie_header_pack(ie_hdr, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (!ext.value.empty()) {
    pdu->append_bytes(const_cast<uint8_t*>(ext.value.data()), static_cast<uint32_t>(ext.value.size()));
  }
  return SRSRAN_SUCCESS;
}

int gtpc_unpack_private_extension_ie(const uint8_t* ptr, const gtpc_ie_header_t& ie_header, gtpc_opaque_ie* ext)
{
  if (ext == nullptr || (ie_header.length > 0 && ptr == nullptr)) {
    return SRSRAN_ERROR;
  }
  ext->value.assign(ptr, ptr + ie_header.length);
  return SRSRAN_SUCCESS;
}

/****************************************************************************
 * GTP-C MBMS Session Duration IE
 * Ref: TS 29.274 v19.6.0 clause 8.69 (envelope) + TS 29.061 v20.0.0 clause
 * 17.7.7 / AVP 904 (internal encoding: 17-bit seconds + 7-bit days).
 ***************************************************************************/
int gtpc_pack_mbms_session_duration_ie(const gtpc_mbms_session_duration_ie& dur,
                                        uint8_t                              instance,
                                        srsran::byte_buffer_t*               pdu)
{
  if (pdu == nullptr || dur.duration_sec > 1641600) {
    return SRSRAN_ERROR;
  }
  gtpc_ie_header_t ie_hdr = {GTPC_IE_TYPE_MBMS_SESSION_DURATION, 3, instance};
  if (gtpc_ie_header_pack(ie_hdr, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  uint32_t days = dur.duration_sec / 86400;
  if (days > 18) {
    days = 18;
  }
  uint32_t seconds17 = dur.duration_sec - days * 86400;
  uint8_t  val[3];
  val[0] = static_cast<uint8_t>((seconds17 >> 9) & 0xFF);
  val[1] = static_cast<uint8_t>((seconds17 >> 1) & 0xFF);
  val[2] = static_cast<uint8_t>(((seconds17 & 0x1) << 7) | (days & 0x7F));
  pdu->append_bytes(val, sizeof(val));
  return SRSRAN_SUCCESS;
}

int gtpc_unpack_mbms_session_duration_ie(const uint8_t*                 ptr,
                                          const gtpc_ie_header_t&        ie_header,
                                          gtpc_mbms_session_duration_ie* dur)
{
  if (ptr == nullptr || dur == nullptr || ie_header.length < 3) {
    return SRSRAN_ERROR;
  }
  uint32_t seconds17 =
      (static_cast<uint32_t>(ptr[0]) << 9) | (static_cast<uint32_t>(ptr[1]) << 1) | ((ptr[2] >> 7) & 0x1);
  uint32_t days     = ptr[2] & 0x7F;
  dur->duration_sec = days * 86400 + seconds17;
  return SRSRAN_SUCCESS;
}

/****************************************************************************
 * GTP-C MBMS Service Area IE
 * Ref: TS 29.274 v19.6.0 clause 8.70 (envelope) + TS 29.061 v20.0.0 clause
 * 17.7.6 / AVP 903 (internal encoding: 1-octet count N + N 2-octet codes).
 ***************************************************************************/
int gtpc_pack_mbms_service_area_ie(const gtpc_mbms_service_area_ie& area,
                                    uint8_t                          instance,
                                    srsran::byte_buffer_t*           pdu)
{
  if (pdu == nullptr || area.sai_codes.size() > 255) {
    return SRSRAN_ERROR;
  }
  uint16_t         value_len = static_cast<uint16_t>(1 + area.sai_codes.size() * 2);
  gtpc_ie_header_t ie_hdr    = {GTPC_IE_TYPE_MBMS_SERVICE_AREA, value_len, instance};
  if (gtpc_ie_header_pack(ie_hdr, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  uint8_t n = static_cast<uint8_t>(area.sai_codes.size());
  pdu->append_bytes(&n, 1);
  for (uint16_t code : area.sai_codes) {
    uint8_t b[2] = {static_cast<uint8_t>((code >> 8) & 0xFF), static_cast<uint8_t>(code & 0xFF)};
    pdu->append_bytes(b, 2);
  }
  return SRSRAN_SUCCESS;
}

int gtpc_unpack_mbms_service_area_ie(const uint8_t*             ptr,
                                      const gtpc_ie_header_t&    ie_header,
                                      gtpc_mbms_service_area_ie* area)
{
  if (ptr == nullptr || area == nullptr || ie_header.length < 1) {
    return SRSRAN_ERROR;
  }
  uint8_t n = ptr[0];
  if (ie_header.length < static_cast<uint16_t>(1 + n * 2)) {
    return SRSRAN_ERROR;
  }
  area->sai_codes.clear();
  area->sai_codes.reserve(n);
  for (uint32_t i = 0; i < n; i++) {
    uint16_t code = static_cast<uint16_t>((static_cast<uint16_t>(ptr[1 + i * 2]) << 8) | ptr[2 + i * 2]);
    area->sai_codes.push_back(code);
  }
  return SRSRAN_SUCCESS;
}

/****************************************************************************
 * GTP-C MBMS Session Identifier IE
 * Ref: TS 29.274 v19.6.0 clause 8.71 -- opaque 1-octet value.
 ***************************************************************************/
int gtpc_pack_mbms_session_id_ie(const gtpc_mbms_session_id_ie& id, uint8_t instance, srsran::byte_buffer_t* pdu)
{
  if (pdu == nullptr) {
    return SRSRAN_ERROR;
  }
  gtpc_ie_header_t ie_hdr = {GTPC_IE_TYPE_MBMS_SESSION_IDENTIFIER, 1, instance};
  if (gtpc_ie_header_pack(ie_hdr, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  uint8_t val = id.session_id;
  pdu->append_bytes(&val, 1);
  return SRSRAN_SUCCESS;
}

int gtpc_unpack_mbms_session_id_ie(const uint8_t* ptr, const gtpc_ie_header_t& ie_header, gtpc_mbms_session_id_ie* id)
{
  if (ptr == nullptr || id == nullptr || ie_header.length < 1) {
    return SRSRAN_ERROR;
  }
  id->session_id = ptr[0];
  return SRSRAN_SUCCESS;
}

/****************************************************************************
 * GTP-C MBMS Flow Identifier IE
 * Ref: TS 29.274 v19.6.0 clause 8.72 -- opaque 2-octet value.
 ***************************************************************************/
int gtpc_pack_mbms_flow_id_ie(const gtpc_mbms_flow_id_ie& flow, uint8_t instance, srsran::byte_buffer_t* pdu)
{
  if (pdu == nullptr) {
    return SRSRAN_ERROR;
  }
  gtpc_ie_header_t ie_hdr = {GTPC_IE_TYPE_MBMS_FLOW_IDENTIFIER, 2, instance};
  if (gtpc_ie_header_pack(ie_hdr, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  uint8_t val[2] = {static_cast<uint8_t>((flow.flow_id >> 8) & 0xFF), static_cast<uint8_t>(flow.flow_id & 0xFF)};
  pdu->append_bytes(val, 2);
  return SRSRAN_SUCCESS;
}

int gtpc_unpack_mbms_flow_id_ie(const uint8_t* ptr, const gtpc_ie_header_t& ie_header, gtpc_mbms_flow_id_ie* flow)
{
  if (ptr == nullptr || flow == nullptr || ie_header.length < 2) {
    return SRSRAN_ERROR;
  }
  flow->flow_id = static_cast<uint16_t>((static_cast<uint16_t>(ptr[0]) << 8) | ptr[1]);
  return SRSRAN_SUCCESS;
}

/****************************************************************************
 * GTP-C MBMS IP Multicast Distribution IE
 * Ref: TS 29.274 v19.6.0 clause 8.73, Figure 8.73-1
 ***************************************************************************/
int gtpc_pack_mbms_ip_mc_distrib_ie(const gtpc_mbms_ip_mc_distrib_ie& distrib,
                                    uint8_t                           instance,
                                    srsran::byte_buffer_t*            pdu)
{
  if (pdu == nullptr) {
    return SRSRAN_ERROR;
  }
  uint16_t dist_len  = distrib.dist_addr_is_ipv6 ? 16 : 4;
  uint16_t src_len   = distrib.source_addr_is_ipv6 ? 16 : 4;
  uint16_t value_len = static_cast<uint16_t>(4 + 1 + dist_len + 1 + src_len + 1);
  gtpc_ie_header_t ie_hdr = {GTPC_IE_TYPE_MBMS_IP_MULTICAST_DISTRIBUTION, value_len, instance};
  if (gtpc_ie_header_pack(ie_hdr, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  uint8_t  val[4 + 1 + 16 + 1 + 16 + 1];
  uint32_t off = 0;
  val[off++]   = static_cast<uint8_t>((distrib.c_teid >> 24) & 0xFF);
  val[off++]   = static_cast<uint8_t>((distrib.c_teid >> 16) & 0xFF);
  val[off++]   = static_cast<uint8_t>((distrib.c_teid >> 8) & 0xFF);
  val[off++]   = static_cast<uint8_t>(distrib.c_teid & 0xFF);

  val[off++] = static_cast<uint8_t>(((distrib.dist_addr_is_ipv6 ? 1u : 0u) << 6) | (dist_len & 0x3F));
  if (distrib.dist_addr_is_ipv6) {
    memcpy(&val[off], distrib.dist_addr_ipv6.s6_addr, 16);
  } else {
    uint32_t ip = distrib.dist_addr_ipv4;
    memcpy(&val[off], &ip, 4);
  }
  off += dist_len;

  val[off++] = static_cast<uint8_t>(((distrib.source_addr_is_ipv6 ? 1u : 0u) << 6) | (src_len & 0x3F));
  if (distrib.source_addr_is_ipv6) {
    memcpy(&val[off], distrib.source_addr_ipv6.s6_addr, 16);
  } else {
    uint32_t ip = distrib.source_addr_ipv4;
    memcpy(&val[off], &ip, 4);
  }
  off += src_len;

  val[off++] = static_cast<uint8_t>(distrib.hc_indicator);
  pdu->append_bytes(val, off);
  return SRSRAN_SUCCESS;
}

int gtpc_unpack_mbms_ip_mc_distrib_ie(const uint8_t*              ptr,
                                      const gtpc_ie_header_t&     ie_header,
                                      gtpc_mbms_ip_mc_distrib_ie* distrib)
{
  if (ptr == nullptr || distrib == nullptr || ie_header.length < (4 + 1 + 4 + 1 + 4 + 1)) {
    return SRSRAN_ERROR; // minimum length: both addresses IPv4
  }
  distrib->c_teid = (static_cast<uint32_t>(ptr[0]) << 24) | (static_cast<uint32_t>(ptr[1]) << 16) |
                    (static_cast<uint32_t>(ptr[2]) << 8) | static_cast<uint32_t>(ptr[3]);
  uint32_t off = 4;

  uint8_t dist_type_len       = ptr[off++];
  distrib->dist_addr_is_ipv6  = ((dist_type_len >> 6) & 0x3) == 1;
  uint32_t dist_len           = distrib->dist_addr_is_ipv6 ? 16 : 4;
  if (ie_header.length < off + dist_len + 1) {
    return SRSRAN_ERROR;
  }
  if (distrib->dist_addr_is_ipv6) {
    memcpy(distrib->dist_addr_ipv6.s6_addr, &ptr[off], 16);
  } else {
    memcpy(&distrib->dist_addr_ipv4, &ptr[off], 4);
  }
  off += dist_len;

  uint8_t src_type_len          = ptr[off++];
  distrib->source_addr_is_ipv6  = ((src_type_len >> 6) & 0x3) == 1;
  uint32_t src_len              = distrib->source_addr_is_ipv6 ? 16 : 4;
  if (ie_header.length < off + src_len + 1) {
    return SRSRAN_ERROR;
  }
  if (distrib->source_addr_is_ipv6) {
    memcpy(distrib->source_addr_ipv6.s6_addr, &ptr[off], 16);
  } else {
    memcpy(&distrib->source_addr_ipv4, &ptr[off], 4);
  }
  off += src_len;

  distrib->hc_indicator = static_cast<gtpc_mbms_hc_indicator>(ptr[off]);
  return SRSRAN_SUCCESS;
}

/****************************************************************************
 * GTP-C MBMS Distribution Acknowledge IE
 * Ref: TS 29.274 v19.6.0 clause 8.74, Figure 8.74-1 -- Sn-only, never
 * populated on the Sm send path in this codebase; included for symmetry.
 ***************************************************************************/
int gtpc_pack_mbms_distribution_ack_ie(const gtpc_mbms_distribution_ack_ie& ack,
                                        uint8_t                              instance,
                                        srsran::byte_buffer_t*               pdu)
{
  if (pdu == nullptr) {
    return SRSRAN_ERROR;
  }
  gtpc_ie_header_t ie_hdr = {GTPC_IE_TYPE_MBMS_DISTRIBUTION_ACKNOWLEDGE, 1, instance};
  if (gtpc_ie_header_pack(ie_hdr, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  uint8_t val = static_cast<uint8_t>(static_cast<uint8_t>(ack.distr_ind) & 0x3);
  pdu->append_bytes(&val, 1);
  return SRSRAN_SUCCESS;
}

int gtpc_unpack_mbms_distribution_ack_ie(const uint8_t*                 ptr,
                                          const gtpc_ie_header_t&        ie_header,
                                          gtpc_mbms_distribution_ack_ie* ack)
{
  if (ptr == nullptr || ack == nullptr || ie_header.length < 1) {
    return SRSRAN_ERROR;
  }
  ack->distr_ind = static_cast<gtpc_mbms_distribution_indication>(ptr[0] & 0x3);
  return SRSRAN_SUCCESS;
}

/****************************************************************************
 * GTP-C TMGI IE
 * Ref: TS 29.274 v19.6.0 clause 8.89, Figure 8.89-1. PLMN portion reuses
 * this codebase's existing bcd_helpers.h s1ap_mccmnc_to_plmn()/
 * s1ap_plmn_to_mccmnc() (already used for real S1AP PLMN signaling
 * elsewhere in this codebase) rather than a plmn_id_t class, which does
 * not exist in bcd_helpers.h.
 ***************************************************************************/
int gtpc_pack_tmgi_ie(const gtpc_tmgi_ie& tmgi, uint8_t instance, srsran::byte_buffer_t* pdu)
{
  if (pdu == nullptr) {
    return SRSRAN_ERROR;
  }
  gtpc_ie_header_t ie_hdr = {GTPC_IE_TYPE_TMGI, 6, instance};
  if (gtpc_ie_header_pack(ie_hdr, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  uint32_t plmn = 0;
  s1ap_mccmnc_to_plmn(tmgi.mcc_bcd, tmgi.mnc_bcd, &plmn);
  uint8_t val[6];
  val[0] = static_cast<uint8_t>((plmn >> 16) & 0xFF);
  val[1] = static_cast<uint8_t>((plmn >> 8) & 0xFF);
  val[2] = static_cast<uint8_t>(plmn & 0xFF);
  val[3] = static_cast<uint8_t>((tmgi.mbms_service_id >> 16) & 0xFF);
  val[4] = static_cast<uint8_t>((tmgi.mbms_service_id >> 8) & 0xFF);
  val[5] = static_cast<uint8_t>(tmgi.mbms_service_id & 0xFF);
  pdu->append_bytes(val, 6);
  return SRSRAN_SUCCESS;
}

int gtpc_unpack_tmgi_ie(const uint8_t* ptr, const gtpc_ie_header_t& ie_header, gtpc_tmgi_ie* tmgi)
{
  if (ptr == nullptr || tmgi == nullptr || ie_header.length < 6) {
    return SRSRAN_ERROR;
  }
  uint32_t plmn =
      (static_cast<uint32_t>(ptr[0]) << 16) | (static_cast<uint32_t>(ptr[1]) << 8) | static_cast<uint32_t>(ptr[2]);
  s1ap_plmn_to_mccmnc(plmn, &tmgi->mcc_bcd, &tmgi->mnc_bcd);
  tmgi->mbms_service_id = (static_cast<uint32_t>(ptr[3]) << 16) | (static_cast<uint32_t>(ptr[4]) << 8) |
                           static_cast<uint32_t>(ptr[5]);
  return SRSRAN_SUCCESS;
}

/****************************************************************************
 * GTP-C Absolute Time of MBMS Data Transfer IE
 * Ref: TS 29.274 v19.6.0 clause 8.95, Figure 8.95-1 -- standard NTP-64.
 ***************************************************************************/
int gtpc_pack_abs_time_mbms_data_transfer_ie(const gtpc_abs_time_mbms_data_transfer_ie& t,
                                              uint8_t                                    instance,
                                              srsran::byte_buffer_t*                     pdu)
{
  if (pdu == nullptr) {
    return SRSRAN_ERROR;
  }
  gtpc_ie_header_t ie_hdr = {GTPC_IE_TYPE_ABSOLUTE_TIME_OF_MBMS_DATA_TRANSFER, 8, instance};
  if (gtpc_ie_header_pack(ie_hdr, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  uint8_t val[8];
  val[0] = static_cast<uint8_t>((t.ntp_seconds >> 24) & 0xFF);
  val[1] = static_cast<uint8_t>((t.ntp_seconds >> 16) & 0xFF);
  val[2] = static_cast<uint8_t>((t.ntp_seconds >> 8) & 0xFF);
  val[3] = static_cast<uint8_t>(t.ntp_seconds & 0xFF);
  val[4] = static_cast<uint8_t>((t.ntp_fraction >> 24) & 0xFF);
  val[5] = static_cast<uint8_t>((t.ntp_fraction >> 16) & 0xFF);
  val[6] = static_cast<uint8_t>((t.ntp_fraction >> 8) & 0xFF);
  val[7] = static_cast<uint8_t>(t.ntp_fraction & 0xFF);
  pdu->append_bytes(val, 8);
  return SRSRAN_SUCCESS;
}

int gtpc_unpack_abs_time_mbms_data_transfer_ie(const uint8_t*                        ptr,
                                                const gtpc_ie_header_t&               ie_header,
                                                gtpc_abs_time_mbms_data_transfer_ie* t)
{
  if (ptr == nullptr || t == nullptr || ie_header.length < 8) {
    return SRSRAN_ERROR;
  }
  t->ntp_seconds = (static_cast<uint32_t>(ptr[0]) << 24) | (static_cast<uint32_t>(ptr[1]) << 16) |
                   (static_cast<uint32_t>(ptr[2]) << 8) | static_cast<uint32_t>(ptr[3]);
  t->ntp_fraction = (static_cast<uint32_t>(ptr[4]) << 24) | (static_cast<uint32_t>(ptr[5]) << 16) |
                    (static_cast<uint32_t>(ptr[6]) << 8) | static_cast<uint32_t>(ptr[7]);
  return SRSRAN_SUCCESS;
}

/****************************************************************************
 * GTP-C MBMS Flags IE
 * Ref: TS 29.274 v19.6.0 clause 8.102, Figure 8.102-1
 ***************************************************************************/
int gtpc_pack_mbms_flags_ie(const gtpc_mbms_flags_ie& flags, uint8_t instance, srsran::byte_buffer_t* pdu)
{
  if (pdu == nullptr) {
    return SRSRAN_ERROR;
  }
  gtpc_ie_header_t ie_hdr = {GTPC_IE_TYPE_MBMS_FLAGS, 1, instance};
  if (gtpc_ie_header_pack(ie_hdr, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  uint8_t val = static_cast<uint8_t>(((flags.lmri ? 1u : 0u) << 1) | (flags.msri ? 1u : 0u));
  pdu->append_bytes(&val, 1);
  return SRSRAN_SUCCESS;
}

int gtpc_unpack_mbms_flags_ie(const uint8_t* ptr, const gtpc_ie_header_t& ie_header, gtpc_mbms_flags_ie* flags)
{
  if (ptr == nullptr || flags == nullptr || ie_header.length < 1) {
    return SRSRAN_ERROR;
  }
  flags->msri = (ptr[0] & 0x1) != 0;
  flags->lmri = ((ptr[0] >> 1) & 0x1) != 0;
  return SRSRAN_SUCCESS;
}

/****************************************************************************
 * GTP-C MBMS Time to Data Transfer IE
 * Ref: TS 29.274 v19.6.0 clause 8.x (envelope) + TS 48.018 v17.0.0 clause
 * 11.3.92, Table 11.3.92.b (value-part coding: raw octet N -> (N+1)
 * seconds, 1-256s range, no "indefinite" sentinel).
 ***************************************************************************/
int gtpc_pack_mbms_time_to_data_transfer_ie(const gtpc_mbms_time_to_data_transfer_ie& t,
                                             uint8_t                                   instance,
                                             srsran::byte_buffer_t*                    pdu)
{
  if (pdu == nullptr || t.seconds < 1 || t.seconds > 256) {
    return SRSRAN_ERROR;
  }
  gtpc_ie_header_t ie_hdr = {GTPC_IE_TYPE_MBMS_TIME_TO_DATA_TRANSFER, 1, instance};
  if (gtpc_ie_header_pack(ie_hdr, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  uint8_t val = static_cast<uint8_t>(t.seconds - 1);
  pdu->append_bytes(&val, 1);
  return SRSRAN_SUCCESS;
}

int gtpc_unpack_mbms_time_to_data_transfer_ie(const uint8_t*                       ptr,
                                               const gtpc_ie_header_t&              ie_header,
                                               gtpc_mbms_time_to_data_transfer_ie* t)
{
  if (ptr == nullptr || t == nullptr || ie_header.length < 1) {
    return SRSRAN_ERROR;
  }
  t->seconds = static_cast<uint32_t>(ptr[0]) + 1;
  return SRSRAN_SUCCESS;
}

/****************************************************************************
 * GTP-C ECGI List IE
 * Ref: TS 29.274 v19.6.0 clause 8.121 (envelope) + clause 8.21.5 (per-field
 * layout, verified directly -- see gtpc_ies.h).
 ***************************************************************************/
int gtpc_pack_ecgi_list_ie(const gtpc_ecgi_list_ie& list, uint8_t instance, srsran::byte_buffer_t* pdu)
{
  if (pdu == nullptr || list.ecgi_list.size() > UINT16_MAX) {
    return SRSRAN_ERROR;
  }
  uint16_t         m         = static_cast<uint16_t>(list.ecgi_list.size());
  uint16_t         value_len = static_cast<uint16_t>(2 + m * 7);
  gtpc_ie_header_t ie_hdr    = {GTPC_IE_TYPE_ECGI_LIST, value_len, instance};
  if (gtpc_ie_header_pack(ie_hdr, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  uint8_t cnt[2] = {static_cast<uint8_t>((m >> 8) & 0xFF), static_cast<uint8_t>(m & 0xFF)};
  pdu->append_bytes(cnt, 2);
  for (const auto& f : list.ecgi_list) {
    uint32_t plmn = 0;
    s1ap_mccmnc_to_plmn(f.mcc_bcd, f.mnc_bcd, &plmn);
    uint8_t val[7];
    val[0] = static_cast<uint8_t>((plmn >> 16) & 0xFF);
    val[1] = static_cast<uint8_t>((plmn >> 8) & 0xFF);
    val[2] = static_cast<uint8_t>(plmn & 0xFF);
    val[3] = static_cast<uint8_t>((f.eci >> 24) & 0xF);
    val[4] = static_cast<uint8_t>((f.eci >> 16) & 0xFF);
    val[5] = static_cast<uint8_t>((f.eci >> 8) & 0xFF);
    val[6] = static_cast<uint8_t>(f.eci & 0xFF);
    pdu->append_bytes(val, 7);
  }
  return SRSRAN_SUCCESS;
}

int gtpc_unpack_ecgi_list_ie(const uint8_t* ptr, const gtpc_ie_header_t& ie_header, gtpc_ecgi_list_ie* list)
{
  if (ptr == nullptr || list == nullptr || ie_header.length < 2) {
    return SRSRAN_ERROR;
  }
  uint16_t m = static_cast<uint16_t>((static_cast<uint16_t>(ptr[0]) << 8) | ptr[1]);
  if (ie_header.length < static_cast<uint32_t>(2 + m * 7)) {
    return SRSRAN_ERROR;
  }
  list->ecgi_list.clear();
  list->ecgi_list.reserve(m);
  for (uint16_t i = 0; i < m; i++) {
    const uint8_t*    f = &ptr[2 + i * 7];
    uint32_t           plmn = (static_cast<uint32_t>(f[0]) << 16) | (static_cast<uint32_t>(f[1]) << 8) |
                     static_cast<uint32_t>(f[2]);
    gtpc_ecgi_field_t field;
    s1ap_plmn_to_mccmnc(plmn, &field.mcc_bcd, &field.mnc_bcd);
    field.eci = (static_cast<uint32_t>(f[3] & 0xF) << 24) | (static_cast<uint32_t>(f[4]) << 16) |
                (static_cast<uint32_t>(f[5]) << 8) | static_cast<uint32_t>(f[6]);
    list->ecgi_list.push_back(field);
  }
  return SRSRAN_SUCCESS;
}

/****************************************************************************
 * GTP-C v2 MBMS Session Start Request
 * Ref: 3GPP TS 29.274 v19.6.0 Table 7.13.1-1
 ***************************************************************************/
int gtpc_pack_mbms_session_start_request(const gtpc_mbms_session_start_request& req, srsran::byte_buffer_t* pdu)
{
  if (pdu == nullptr) {
    return SRSRAN_ERROR;
  }
  if (gtpc_pack_f_teid_ie(req.sender_f_teid, 0, pdu) != SRSRAN_SUCCESS ||
      gtpc_pack_tmgi_ie(req.tmgi, 0, pdu) != SRSRAN_SUCCESS ||
      gtpc_pack_mbms_session_duration_ie(req.mbms_session_duration, 0, pdu) != SRSRAN_SUCCESS ||
      gtpc_pack_mbms_service_area_ie(req.mbms_service_area, 0, pdu) != SRSRAN_SUCCESS ||
      gtpc_pack_bearer_qos_ie(req.qos_profile, 0, pdu) != SRSRAN_SUCCESS ||
      gtpc_pack_mbms_ip_mc_distrib_ie(req.mbms_ip_multicast_distrib, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (req.mbms_session_id_present && gtpc_pack_mbms_session_id_ie(req.mbms_session_id, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (req.mbms_flow_id_present && gtpc_pack_mbms_flow_id_ie(req.mbms_flow_id, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (req.recovery_present && gtpc_pack_recovery_ie(req.recovery, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (req.mbms_time_to_data_transfer_present &&
      gtpc_pack_mbms_time_to_data_transfer_ie(req.mbms_time_to_data_transfer, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (req.mbms_data_transfer_start_present &&
      gtpc_pack_abs_time_mbms_data_transfer_ie(req.mbms_data_transfer_start, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (req.mbms_flags_present && gtpc_pack_mbms_flags_ie(req.mbms_flags, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (req.alternative_ip_multicast_distrib_present &&
      gtpc_pack_mbms_ip_mc_distrib_ie(req.alternative_ip_multicast_distrib, 1, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (req.ecgi_list_present && gtpc_pack_ecgi_list_ie(req.ecgi_list, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (req.private_extension_present &&
      gtpc_pack_private_extension_ie(req.private_extension, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  return SRSRAN_SUCCESS;
}

int gtpc_unpack_mbms_session_start_request(const uint8_t* ptr, uint32_t len, gtpc_mbms_session_start_request* req)
{
  if (ptr == nullptr || req == nullptr) {
    return SRSRAN_ERROR;
  }
  *req = gtpc_mbms_session_start_request{};
  bool     have_f_teid = false, have_tmgi = false, have_duration = false, have_area = false, have_qos = false,
       have_distrib = false;
  uint32_t off = 0;
  while (off + 4 <= len) {
    gtpc_ie_header_t ie_hdr;
    if (gtpc_ie_header_unpack(&ptr[off], len - off, &ie_hdr) != SRSRAN_SUCCESS) {
      return SRSRAN_ERROR;
    }
    if (off + 4 + ie_hdr.length > len) {
      return SRSRAN_ERROR;
    }
    const uint8_t* v = &ptr[off + 4];
    switch (ie_hdr.type) {
      case GTPC_IE_TYPE_F_TEID:
        if (ie_hdr.instance == 0) {
          if (gtpc_unpack_f_teid_ie(v, ie_hdr, &req->sender_f_teid) != SRSRAN_SUCCESS)
            return SRSRAN_ERROR;
          have_f_teid = true;
        }
        break;
      case GTPC_IE_TYPE_TMGI:
        if (gtpc_unpack_tmgi_ie(v, ie_hdr, &req->tmgi) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        have_tmgi = true;
        break;
      case GTPC_IE_TYPE_MBMS_SESSION_DURATION:
        if (gtpc_unpack_mbms_session_duration_ie(v, ie_hdr, &req->mbms_session_duration) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        have_duration = true;
        break;
      case GTPC_IE_TYPE_MBMS_SERVICE_AREA:
        if (gtpc_unpack_mbms_service_area_ie(v, ie_hdr, &req->mbms_service_area) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        have_area = true;
        break;
      case GTPC_IE_TYPE_MBMS_SESSION_IDENTIFIER:
        if (gtpc_unpack_mbms_session_id_ie(v, ie_hdr, &req->mbms_session_id) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        req->mbms_session_id_present = true;
        break;
      case GTPC_IE_TYPE_MBMS_FLOW_IDENTIFIER:
        if (gtpc_unpack_mbms_flow_id_ie(v, ie_hdr, &req->mbms_flow_id) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        req->mbms_flow_id_present = true;
        break;
      case GTPC_IE_TYPE_BEARER_QOS:
        if (gtpc_unpack_bearer_qos_ie(v, ie_hdr, &req->qos_profile) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        have_qos = true;
        break;
      case GTPC_IE_TYPE_MBMS_IP_MULTICAST_DISTRIBUTION:
        if (ie_hdr.instance == 0) {
          if (gtpc_unpack_mbms_ip_mc_distrib_ie(v, ie_hdr, &req->mbms_ip_multicast_distrib) != SRSRAN_SUCCESS)
            return SRSRAN_ERROR;
          have_distrib = true;
        } else if (ie_hdr.instance == 1) {
          if (gtpc_unpack_mbms_ip_mc_distrib_ie(v, ie_hdr, &req->alternative_ip_multicast_distrib) != SRSRAN_SUCCESS)
            return SRSRAN_ERROR;
          req->alternative_ip_multicast_distrib_present = true;
        }
        break;
      case GTPC_IE_TYPE_RECOVERY:
        if (gtpc_unpack_recovery_ie(v, ie_hdr, &req->recovery) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        req->recovery_present = true;
        break;
      case GTPC_IE_TYPE_MBMS_TIME_TO_DATA_TRANSFER:
        if (gtpc_unpack_mbms_time_to_data_transfer_ie(v, ie_hdr, &req->mbms_time_to_data_transfer) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        req->mbms_time_to_data_transfer_present = true;
        break;
      case GTPC_IE_TYPE_ABSOLUTE_TIME_OF_MBMS_DATA_TRANSFER:
        if (gtpc_unpack_abs_time_mbms_data_transfer_ie(v, ie_hdr, &req->mbms_data_transfer_start) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        req->mbms_data_transfer_start_present = true;
        break;
      case GTPC_IE_TYPE_MBMS_FLAGS:
        if (gtpc_unpack_mbms_flags_ie(v, ie_hdr, &req->mbms_flags) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        req->mbms_flags_present = true;
        break;
      case GTPC_IE_TYPE_ECGI_LIST:
        if (gtpc_unpack_ecgi_list_ie(v, ie_hdr, &req->ecgi_list) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        req->ecgi_list_present = true;
        break;
      case GTPC_IE_TYPE_PRIVATE_EXTENSION:
        if (gtpc_unpack_private_extension_ie(v, ie_hdr, &req->private_extension) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        req->private_extension_present = true;
        break;
      default:
        break; // unrecognized IE types silently skipped, per GTPv2-C convention
    }
    off += 4 + ie_hdr.length;
  }
  if (!have_f_teid || !have_tmgi || !have_duration || !have_area || !have_qos || !have_distrib) {
    return SRSRAN_ERROR; // missing mandatory IE
  }
  return SRSRAN_SUCCESS;
}

/****************************************************************************
 * GTP-C v2 MBMS Session Start Response
 * Ref: 3GPP TS 29.274 v19.6.0 Table 7.13.2-1
 ***************************************************************************/
int gtpc_pack_mbms_session_start_response(const gtpc_mbms_session_start_response& resp, srsran::byte_buffer_t* pdu)
{
  if (pdu == nullptr) {
    return SRSRAN_ERROR;
  }
  if (gtpc_pack_cause_ie(resp.cause, 0, pdu) != SRSRAN_SUCCESS ||
      gtpc_pack_f_teid_ie(resp.sender_f_teid, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (resp.mbms_distribution_ack_present &&
      gtpc_pack_mbms_distribution_ack_ie(resp.mbms_distribution_ack, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (resp.sn_u_sgsn_f_teid_present && gtpc_pack_f_teid_ie(resp.sn_u_sgsn_f_teid, 1, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (resp.recovery_present && gtpc_pack_recovery_ie(resp.recovery, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (resp.private_extension_present &&
      gtpc_pack_private_extension_ie(resp.private_extension, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  return SRSRAN_SUCCESS;
}

int gtpc_unpack_mbms_session_start_response(const uint8_t*                    ptr,
                                             uint32_t                          len,
                                             gtpc_mbms_session_start_response* resp)
{
  if (ptr == nullptr || resp == nullptr) {
    return SRSRAN_ERROR;
  }
  *resp                    = gtpc_mbms_session_start_response{};
  bool     have_cause      = false;
  bool     have_f_teid     = false;
  uint32_t off             = 0;
  while (off + 4 <= len) {
    gtpc_ie_header_t ie_hdr;
    if (gtpc_ie_header_unpack(&ptr[off], len - off, &ie_hdr) != SRSRAN_SUCCESS) {
      return SRSRAN_ERROR;
    }
    if (off + 4 + ie_hdr.length > len) {
      return SRSRAN_ERROR;
    }
    const uint8_t* v = &ptr[off + 4];
    switch (ie_hdr.type) {
      case GTPC_IE_TYPE_CAUSE:
        if (gtpc_unpack_cause_ie(v, ie_hdr, &resp->cause) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        have_cause = true;
        break;
      case GTPC_IE_TYPE_F_TEID:
        if (ie_hdr.instance == 0) {
          if (gtpc_unpack_f_teid_ie(v, ie_hdr, &resp->sender_f_teid) != SRSRAN_SUCCESS)
            return SRSRAN_ERROR;
          have_f_teid = true;
        } else if (ie_hdr.instance == 1) {
          if (gtpc_unpack_f_teid_ie(v, ie_hdr, &resp->sn_u_sgsn_f_teid) != SRSRAN_SUCCESS)
            return SRSRAN_ERROR;
          resp->sn_u_sgsn_f_teid_present = true;
        }
        break;
      case GTPC_IE_TYPE_MBMS_DISTRIBUTION_ACKNOWLEDGE:
        if (gtpc_unpack_mbms_distribution_ack_ie(v, ie_hdr, &resp->mbms_distribution_ack) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        resp->mbms_distribution_ack_present = true;
        break;
      case GTPC_IE_TYPE_RECOVERY:
        if (gtpc_unpack_recovery_ie(v, ie_hdr, &resp->recovery) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        resp->recovery_present = true;
        break;
      case GTPC_IE_TYPE_PRIVATE_EXTENSION:
        if (gtpc_unpack_private_extension_ie(v, ie_hdr, &resp->private_extension) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        resp->private_extension_present = true;
        break;
      default:
        break;
    }
    off += 4 + ie_hdr.length;
  }
  if (!have_cause || !have_f_teid) {
    return SRSRAN_ERROR;
  }
  return SRSRAN_SUCCESS;
}

/****************************************************************************
 * GTP-C v2 MBMS Session Update Request
 * Ref: 3GPP TS 29.274 v19.6.0 Table 7.13.3-1
 ***************************************************************************/
int gtpc_pack_mbms_session_update_request(const gtpc_mbms_session_update_request& req, srsran::byte_buffer_t* pdu)
{
  if (pdu == nullptr) {
    return SRSRAN_ERROR;
  }
  if (req.mbms_service_area_present &&
      gtpc_pack_mbms_service_area_ie(req.mbms_service_area, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (gtpc_pack_tmgi_ie(req.tmgi, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (req.sender_f_teid_present && gtpc_pack_f_teid_ie(req.sender_f_teid, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (gtpc_pack_mbms_session_duration_ie(req.mbms_session_duration, 0, pdu) != SRSRAN_SUCCESS ||
      gtpc_pack_bearer_qos_ie(req.qos_profile, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (req.mbms_session_id_present && gtpc_pack_mbms_session_id_ie(req.mbms_session_id, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (req.mbms_flow_id_present && gtpc_pack_mbms_flow_id_ie(req.mbms_flow_id, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (req.mbms_time_to_data_transfer_present &&
      gtpc_pack_mbms_time_to_data_transfer_ie(req.mbms_time_to_data_transfer, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (req.mbms_data_transfer_start_update_stop_present &&
      gtpc_pack_abs_time_mbms_data_transfer_ie(req.mbms_data_transfer_start_update_stop, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (req.ecgi_list_present && gtpc_pack_ecgi_list_ie(req.ecgi_list, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (req.private_extension_present &&
      gtpc_pack_private_extension_ie(req.private_extension, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  return SRSRAN_SUCCESS;
}

int gtpc_unpack_mbms_session_update_request(const uint8_t* ptr, uint32_t len, gtpc_mbms_session_update_request* req)
{
  if (ptr == nullptr || req == nullptr) {
    return SRSRAN_ERROR;
  }
  *req              = gtpc_mbms_session_update_request{};
  bool     have_tmgi = false, have_duration = false, have_qos = false;
  uint32_t off       = 0;
  while (off + 4 <= len) {
    gtpc_ie_header_t ie_hdr;
    if (gtpc_ie_header_unpack(&ptr[off], len - off, &ie_hdr) != SRSRAN_SUCCESS) {
      return SRSRAN_ERROR;
    }
    if (off + 4 + ie_hdr.length > len) {
      return SRSRAN_ERROR;
    }
    const uint8_t* v = &ptr[off + 4];
    switch (ie_hdr.type) {
      case GTPC_IE_TYPE_MBMS_SERVICE_AREA:
        if (gtpc_unpack_mbms_service_area_ie(v, ie_hdr, &req->mbms_service_area) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        req->mbms_service_area_present = true;
        break;
      case GTPC_IE_TYPE_TMGI:
        if (gtpc_unpack_tmgi_ie(v, ie_hdr, &req->tmgi) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        have_tmgi = true;
        break;
      case GTPC_IE_TYPE_F_TEID:
        if (gtpc_unpack_f_teid_ie(v, ie_hdr, &req->sender_f_teid) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        req->sender_f_teid_present = true;
        break;
      case GTPC_IE_TYPE_MBMS_SESSION_DURATION:
        if (gtpc_unpack_mbms_session_duration_ie(v, ie_hdr, &req->mbms_session_duration) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        have_duration = true;
        break;
      case GTPC_IE_TYPE_BEARER_QOS:
        if (gtpc_unpack_bearer_qos_ie(v, ie_hdr, &req->qos_profile) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        have_qos = true;
        break;
      case GTPC_IE_TYPE_MBMS_SESSION_IDENTIFIER:
        if (gtpc_unpack_mbms_session_id_ie(v, ie_hdr, &req->mbms_session_id) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        req->mbms_session_id_present = true;
        break;
      case GTPC_IE_TYPE_MBMS_FLOW_IDENTIFIER:
        if (gtpc_unpack_mbms_flow_id_ie(v, ie_hdr, &req->mbms_flow_id) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        req->mbms_flow_id_present = true;
        break;
      case GTPC_IE_TYPE_MBMS_TIME_TO_DATA_TRANSFER:
        if (gtpc_unpack_mbms_time_to_data_transfer_ie(v, ie_hdr, &req->mbms_time_to_data_transfer) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        req->mbms_time_to_data_transfer_present = true;
        break;
      case GTPC_IE_TYPE_ABSOLUTE_TIME_OF_MBMS_DATA_TRANSFER:
        if (gtpc_unpack_abs_time_mbms_data_transfer_ie(v, ie_hdr, &req->mbms_data_transfer_start_update_stop) !=
            SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        req->mbms_data_transfer_start_update_stop_present = true;
        break;
      case GTPC_IE_TYPE_ECGI_LIST:
        if (gtpc_unpack_ecgi_list_ie(v, ie_hdr, &req->ecgi_list) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        req->ecgi_list_present = true;
        break;
      case GTPC_IE_TYPE_PRIVATE_EXTENSION:
        if (gtpc_unpack_private_extension_ie(v, ie_hdr, &req->private_extension) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        req->private_extension_present = true;
        break;
      default:
        break;
    }
    off += 4 + ie_hdr.length;
  }
  if (!have_tmgi || !have_duration || !have_qos) {
    return SRSRAN_ERROR;
  }
  return SRSRAN_SUCCESS;
}

/****************************************************************************
 * GTP-C v2 MBMS Session Update Response
 * Ref: 3GPP TS 29.274 v19.6.0 Table 7.13.4-1
 ***************************************************************************/
int gtpc_pack_mbms_session_update_response(const gtpc_mbms_session_update_response& resp, srsran::byte_buffer_t* pdu)
{
  if (pdu == nullptr) {
    return SRSRAN_ERROR;
  }
  if (gtpc_pack_cause_ie(resp.cause, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (resp.mbms_distribution_ack_present &&
      gtpc_pack_mbms_distribution_ack_ie(resp.mbms_distribution_ack, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (resp.sn_u_sgsn_f_teid_present && gtpc_pack_f_teid_ie(resp.sn_u_sgsn_f_teid, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (resp.recovery_present && gtpc_pack_recovery_ie(resp.recovery, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (resp.private_extension_present &&
      gtpc_pack_private_extension_ie(resp.private_extension, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  return SRSRAN_SUCCESS;
}

int gtpc_unpack_mbms_session_update_response(const uint8_t*                     ptr,
                                              uint32_t                           len,
                                              gtpc_mbms_session_update_response* resp)
{
  if (ptr == nullptr || resp == nullptr) {
    return SRSRAN_ERROR;
  }
  *resp               = gtpc_mbms_session_update_response{};
  bool     have_cause = false;
  uint32_t off        = 0;
  while (off + 4 <= len) {
    gtpc_ie_header_t ie_hdr;
    if (gtpc_ie_header_unpack(&ptr[off], len - off, &ie_hdr) != SRSRAN_SUCCESS) {
      return SRSRAN_ERROR;
    }
    if (off + 4 + ie_hdr.length > len) {
      return SRSRAN_ERROR;
    }
    const uint8_t* v = &ptr[off + 4];
    switch (ie_hdr.type) {
      case GTPC_IE_TYPE_CAUSE:
        if (gtpc_unpack_cause_ie(v, ie_hdr, &resp->cause) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        have_cause = true;
        break;
      case GTPC_IE_TYPE_MBMS_DISTRIBUTION_ACKNOWLEDGE:
        if (gtpc_unpack_mbms_distribution_ack_ie(v, ie_hdr, &resp->mbms_distribution_ack) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        resp->mbms_distribution_ack_present = true;
        break;
      case GTPC_IE_TYPE_F_TEID:
        if (gtpc_unpack_f_teid_ie(v, ie_hdr, &resp->sn_u_sgsn_f_teid) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        resp->sn_u_sgsn_f_teid_present = true;
        break;
      case GTPC_IE_TYPE_RECOVERY:
        if (gtpc_unpack_recovery_ie(v, ie_hdr, &resp->recovery) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        resp->recovery_present = true;
        break;
      case GTPC_IE_TYPE_PRIVATE_EXTENSION:
        if (gtpc_unpack_private_extension_ie(v, ie_hdr, &resp->private_extension) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        resp->private_extension_present = true;
        break;
      default:
        break;
    }
    off += 4 + ie_hdr.length;
  }
  if (!have_cause) {
    return SRSRAN_ERROR;
  }
  return SRSRAN_SUCCESS;
}

/****************************************************************************
 * GTP-C v2 MBMS Session Stop Request
 * Ref: 3GPP TS 29.274 v19.6.0 Table 7.13.5-1 (+ TMGI, added per TS 23.246
 * clause 8.5.2 step 1 -- see note in gtpc_msg.h)
 ***************************************************************************/
int gtpc_pack_mbms_session_stop_request(const gtpc_mbms_session_stop_request& req, srsran::byte_buffer_t* pdu)
{
  if (pdu == nullptr) {
    return SRSRAN_ERROR;
  }
  if (gtpc_pack_tmgi_ie(req.tmgi, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (req.mbms_flow_id_present && gtpc_pack_mbms_flow_id_ie(req.mbms_flow_id, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (req.mbms_data_transfer_stop_present &&
      gtpc_pack_abs_time_mbms_data_transfer_ie(req.mbms_data_transfer_stop, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (req.mbms_flags_present && gtpc_pack_mbms_flags_ie(req.mbms_flags, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (req.private_extension_present &&
      gtpc_pack_private_extension_ie(req.private_extension, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  return SRSRAN_SUCCESS;
}

int gtpc_unpack_mbms_session_stop_request(const uint8_t* ptr, uint32_t len, gtpc_mbms_session_stop_request* req)
{
  if (ptr == nullptr || req == nullptr) {
    return SRSRAN_ERROR;
  }
  *req              = gtpc_mbms_session_stop_request{};
  bool     have_tmgi = false;
  uint32_t off       = 0;
  while (off + 4 <= len) {
    gtpc_ie_header_t ie_hdr;
    if (gtpc_ie_header_unpack(&ptr[off], len - off, &ie_hdr) != SRSRAN_SUCCESS) {
      return SRSRAN_ERROR;
    }
    if (off + 4 + ie_hdr.length > len) {
      return SRSRAN_ERROR;
    }
    const uint8_t* v = &ptr[off + 4];
    switch (ie_hdr.type) {
      case GTPC_IE_TYPE_TMGI:
        if (gtpc_unpack_tmgi_ie(v, ie_hdr, &req->tmgi) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        have_tmgi = true;
        break;
      case GTPC_IE_TYPE_MBMS_FLOW_IDENTIFIER:
        if (gtpc_unpack_mbms_flow_id_ie(v, ie_hdr, &req->mbms_flow_id) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        req->mbms_flow_id_present = true;
        break;
      case GTPC_IE_TYPE_ABSOLUTE_TIME_OF_MBMS_DATA_TRANSFER:
        if (gtpc_unpack_abs_time_mbms_data_transfer_ie(v, ie_hdr, &req->mbms_data_transfer_stop) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        req->mbms_data_transfer_stop_present = true;
        break;
      case GTPC_IE_TYPE_MBMS_FLAGS:
        if (gtpc_unpack_mbms_flags_ie(v, ie_hdr, &req->mbms_flags) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        req->mbms_flags_present = true;
        break;
      case GTPC_IE_TYPE_PRIVATE_EXTENSION:
        if (gtpc_unpack_private_extension_ie(v, ie_hdr, &req->private_extension) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        req->private_extension_present = true;
        break;
      default:
        break;
    }
    off += 4 + ie_hdr.length;
  }
  if (!have_tmgi) {
    return SRSRAN_ERROR;
  }
  return SRSRAN_SUCCESS;
}

/****************************************************************************
 * GTP-C v2 MBMS Session Stop Response
 * Ref: 3GPP TS 29.274 v19.6.0 Table 7.13.6-1
 ***************************************************************************/
int gtpc_pack_mbms_session_stop_response(const gtpc_mbms_session_stop_response& resp, srsran::byte_buffer_t* pdu)
{
  if (pdu == nullptr) {
    return SRSRAN_ERROR;
  }
  if (gtpc_pack_cause_ie(resp.cause, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (resp.recovery_present && gtpc_pack_recovery_ie(resp.recovery, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  if (resp.private_extension_present &&
      gtpc_pack_private_extension_ie(resp.private_extension, 0, pdu) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }
  return SRSRAN_SUCCESS;
}

int gtpc_unpack_mbms_session_stop_response(const uint8_t* ptr, uint32_t len, gtpc_mbms_session_stop_response* resp)
{
  if (ptr == nullptr || resp == nullptr) {
    return SRSRAN_ERROR;
  }
  *resp               = gtpc_mbms_session_stop_response{};
  bool     have_cause = false;
  uint32_t off        = 0;
  while (off + 4 <= len) {
    gtpc_ie_header_t ie_hdr;
    if (gtpc_ie_header_unpack(&ptr[off], len - off, &ie_hdr) != SRSRAN_SUCCESS) {
      return SRSRAN_ERROR;
    }
    if (off + 4 + ie_hdr.length > len) {
      return SRSRAN_ERROR;
    }
    const uint8_t* v = &ptr[off + 4];
    switch (ie_hdr.type) {
      case GTPC_IE_TYPE_CAUSE:
        if (gtpc_unpack_cause_ie(v, ie_hdr, &resp->cause) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        have_cause = true;
        break;
      case GTPC_IE_TYPE_RECOVERY:
        if (gtpc_unpack_recovery_ie(v, ie_hdr, &resp->recovery) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        resp->recovery_present = true;
        break;
      case GTPC_IE_TYPE_PRIVATE_EXTENSION:
        if (gtpc_unpack_private_extension_ie(v, ie_hdr, &resp->private_extension) != SRSRAN_SUCCESS)
          return SRSRAN_ERROR;
        resp->private_extension_present = true;
        break;
      default:
        break;
    }
    off += 4 + ie_hdr.length;
  }
  if (!have_cause) {
    return SRSRAN_ERROR;
  }
  return SRSRAN_SUCCESS;
}

} // namespace srsran
