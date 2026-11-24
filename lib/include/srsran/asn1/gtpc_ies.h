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

#ifndef SRSRAN_GTPC_IES_H
#define SRSRAN_GTPC_IES_H

#include "srsran/common/byte_buffer.h"
#include "srsran/phy/io/netsource.h"
#include <vector>

namespace srsran {

/****************************************************************
 *
 * GTP-C IE Types
 * Ref: TS 29.274 v10.14.0 Table 8.1-1
 *
 ****************************************************************/
enum gtpc_ie_type {
  // const uint8_t GTPC_IE_TYPE_RESERVED = 0;
  GTPC_IE_TYPE_IMSI     = 1,
  GTPC_IE_TYPE_CAUSE    = 2,
  GTPC_IE_TYPE_RECOVERY = 3,
  // 4 to 50 RESERVED_FOR_S101_INTERFACE
  GTPC_IE_TYPE_STN_SR = 51,
  // 52 to 70 RESERVED_FOR_SV_INTERFACE
  GTPC_IE_TYPE_APN                           = 71,
  GTPC_IE_TYPE_AMBR                          = 72,
  GTPC_IE_TYPE_EBI                           = 73,
  GTPC_IE_TYPE_IP_ADDRESS                    = 74,
  GTPC_IE_TYPE_MEI                           = 75,
  GTPC_IE_TYPE_MSISDN                        = 76,
  GTPC_IE_TYPE_INDICATION                    = 77,
  GTPC_IE_TYPE_PCO                           = 78,
  GTPC_IE_TYPE_PDN_ADDRESS_ALLOCATION        = 79,
  GTPC_IE_TYPE_BEARER_QOS                    = 80,
  GTPC_IE_TYPE_FLOW_QOS                      = 81,
  GTPC_IE_TYPE_RAT_TYPE                      = 82,
  GTPC_IE_TYPE_SERVING_NETWORK               = 83,
  GTPC_IE_TYPE_BEARER_TFT                    = 84,
  GTPC_IE_TYPE_TAD                           = 85,
  GTPC_IE_TYPE_ULI                           = 86,
  GTPC_IE_TYPE_F_TEID                        = 87,
  GTPC_IE_TYPE_TMSI                          = 88,
  GTPC_IE_TYPE_GLOBAL_CN_ID                  = 89,
  GTPC_IE_TYPE_S103_PDN_DATA_FORWARDING_INFO = 90,
  GTPC_IE_TYPE_S1_U_DATA_FORWARDING_INFO     = 91,
  GTPC_IE_TYPE_DELAY_VALUE                   = 92,
  GTPC_IE_TYPE_BEARER_CONTEXT                = 93,
  GTPC_IE_TYPE_CHARGING_ID                   = 94,
  GTPC_IE_TYPE_CHARGING_CHARACTERISTICS      = 95,
  GTPC_IE_TYPE_TRACE_INFORMATION             = 96,
  GTPC_IE_TYPE_BEARER_FLAGS                  = 97,
  // 98 Reserved
  GTPC_IE_TYPE_PDN_TYPE                 = 99,
  GTPC_IE_TYPE_PROCEDURE_TRANSACTION_ID = 100,
  GTPC_IE_TYPE_DRX_PARAMETER            = 101,
  // 102 Reserved
  GTPC_IE_TYPE_MM_CONTEXT_GSM_KEY_AND_TRIPLETS                             = 103,
  GTPC_IE_TYPE_MM_CONTEXT_UMTS_KEY_USED_CIPHER_AND_QUINTUPLETS             = 104,
  GTPC_IE_TYPE_MM_CONTEXT_GSM_KEY_USED_CIPHER_AND_QUINTUPLETS              = 105,
  GTPC_IE_TYPE_MM_CONTEXT_UMTS_KEY_AND_QUINTUPLETS                         = 106,
  GTPC_IE_TYPE_MM_CONTEXT_EPS_SECURITY_CONTEXT_QUADRUPLETS_AND_QUINTUPLETS = 107,
  GTPC_IE_TYPE_MM_CONTEXT_UMTS_KEY_QUADRUPLETS_AND_QUINTUPLETS             = 108,
  GTPC_IE_TYPE_PDN_CONNECTION                                              = 109,
  GTPC_IE_TYPE_PDU_NUMBERS                                                 = 110,
  GTPC_IE_TYPE_P_TMSI                                                      = 111,
  GTPC_IE_TYPE_P_TMSI_SIGNATURE                                            = 112,
  GTPC_IE_TYPE_HOP_COUNTER                                                 = 113,
  GTPC_IE_TYPE_UE_TIME_ZONE                                                = 114,
  GTPC_IE_TYPE_TRACE_REFERENCE                                             = 115,
  GTPC_IE_TYPE_COMPLETE_REQUEST_MESSAGE                                    = 116,
  GTPC_IE_TYPE_GUTI                                                        = 117,
  GTPC_IE_TYPE_F_CONTAINER                                                 = 118,
  GTPC_IE_TYPE_F_CAUSE                                                     = 119,
  GTPC_IE_TYPE_SELECTED_PLMN_ID                                            = 120,
  GTPC_IE_TYPE_TARGET_IDENTIFICATION                                       = 121,
  // 122 Reserved
  GTPC_IE_TYPE_PACKET_FLOW_ID               = 123,
  GTPC_IE_TYPE_RAB_CONTEXT                  = 124,
  GTPC_IE_TYPE_SOURCE_RNC_PDCP_CONTEXT_INFO = 125,
  GTPC_IE_TYPE_UDP_SOURCE_PORT_NUMBER       = 126,
  GTPC_IE_TYPE_APN_RESTRICTION              = 127,
  GTPC_IE_TYPE_SELECTION_MODE               = 128,
  GTPC_IE_TYPE_SOURCE_IDENTIFICATION        = 129,
  // 130 RESERVED
  GTPC_IE_TYPE_CHANGE_REPORTING_ACTION          = 131,
  GTPC_IE_TYPE_FQ_CSID                          = 132,
  GTPC_IE_TYPE_CHANNEL_NEEDED                   = 133,
  GTPC_IE_TYPE_EMLPP_PRIORITY                   = 134,
  GTPC_IE_TYPE_NODE_TYPE                        = 135,
  GTPC_IE_TYPE_FQDN                             = 136,
  GTPC_IE_TYPE_TI                               = 137,
  GTPC_IE_TYPE_MBMS_SESSION_DURATION            = 138,
  GTPC_IE_TYPE_MBMS_SERVICE_AREA                = 139,
  GTPC_IE_TYPE_MBMS_SESSION_IDENTIFIER          = 140,
  GTPC_IE_TYPE_MBMS_FLOW_IDENTIFIER             = 141,
  GTPC_IE_TYPE_MBMS_IP_MULTICAST_DISTRIBUTION   = 142,
  GTPC_IE_TYPE_MBMS_DISTRIBUTION_ACKNOWLEDGE    = 143,
  GTPC_IE_TYPE_RFSP_INDEX                       = 144,
  GTPC_IE_TYPE_UCI                              = 145,
  GTPC_IE_TYPE_CSG_INFORMATION_REPORTING_ACTION = 146,
  GTPC_IE_TYPE_CSG_ID                           = 147,
  GTPC_IE_TYPE_CMI                              = 148,
  GTPC_IE_TYPE_SERVICE_INDICATOR                = 149,
  GTPC_IE_TYPE_DETACH_TYPE                      = 150,
  GTPC_IE_TYPE_LDN                              = 151,
  GTPC_IE_TYPE_NODE_FEATURES                    = 152,
  GTPC_IE_TYPE_MBMS_TIME_TO_DATA_TRANSFER       = 153,
  GTPC_IE_TYPE_THROTTLING                       = 154,
  GTPC_IE_TYPE_ARP                              = 155,
  GTPC_IE_TYPE_EPC_TIMER                        = 156,
  GTPC_IE_TYPE_SIGNALLING_PRIORITY_INDICATION   = 157,
  GTPC_IE_TYPE_TMGI                             = 158,
  GTPC_IE_TYPE_ADDITIONAL_MM_CONTEXT_FOR_SRVCC  = 159,
  GTPC_IE_TYPE_ADDITIONAL_FLAGS_FOR_SRVCC       = 160,
  // 161 RESERVED
  GTPC_IE_TYPE_MDT_CONFIGURATION                     = 162,
  GTPC_IE_TYPE_APCO                                  = 163,
  GTPC_IE_TYPE_ABSOLUTE_TIME_OF_MBMS_DATA_TRANSFER   = 164, // TS 29.274 v19.6.0 clause 8.95
  GTPC_IE_TYPE_CHANGE_TO_REPORT_FLAGS                = 165,
  // 166-167 SPARE
  // 168 SPARE (Deprecated: was Global MBMS Bearer Service Identifier / superseded by v19.6.0)
  // 169-170 SPARE
  GTPC_IE_TYPE_MBMS_FLAGS = 171, // TS 29.274 v19.6.0 clause 8.102
  // 172-189 SPARE. FOR FUTURE USE.
  GTPC_IE_TYPE_ECGI_LIST = 190, // TS 29.274 v19.6.0 clause 8.121
  // 191-254 SPARE. FOR FUTURE USE.
  GTPC_IE_TYPE_PRIVATE_EXTENSION = 255
};

/****************************************************************************
 *
 * GTP-C IE Header (TLIV: Type, Length, Instance, Value)
 * Ref: 3GPP TS 29.274 v19.6.0 clause 8.1/8.2
 *
 * Every IE is: 1 octet Type, 2 octets Length (of the Value field only),
 * 1 octet (4 spare bits + 4-bit Instance), then Length octets of Value.
 *
 ***************************************************************************/
struct gtpc_ie_header_t {
  uint8_t  type;
  uint16_t length;
  uint8_t  instance; // low 4 bits significant, high 4 bits spare
};

/**
 * Packs a 4-octet IE header (Type + Length + spare/Instance) into pdu.
 */
int gtpc_ie_header_pack(const gtpc_ie_header_t& ie_header, srsran::byte_buffer_t* pdu);

/**
 * Unpacks a 4-octet IE header from ptr. remaining is the number of valid
 * octets available at ptr; returns SRSRAN_ERROR if fewer than 4 are
 * available. Does not validate that `length` octets of Value actually
 * follow -- callers must check that themselves against their own remaining
 * count before reading the Value.
 */
int gtpc_ie_header_unpack(const uint8_t* ptr, uint32_t remaining, gtpc_ie_header_t* ie_header);

/****************************************************************
 *
 * GTP-C IMSI IE
 * Ref: TS 29.274 v10.14.0 Figure 8.3-1
 *
 ****************************************************************/
/*
 * The IMSI should be kept as an uint64_t.
 * The responsibility to convert from uint64_t to BCD coded is on
 * the pack_imsi_ie function
 */

/****************************************************************************
 *
 * GTP-C Cause IE
 * Ref: 3GPP TS 29.274 v10.14.0 Figure 8.4-1 and Table 8.4-1
 *
 ***************************************************************************/
enum gtpc_cause_value {
  // Reserved
  GTPC_CAUSE_VALUE_LOCAL_DETACH                                      = 2,
  GTPC_CAUSE_VALUE_COMPLETE_DETACH                                   = 3,
  GTPC_CAUSE_VALUE_RAT_CHANGED_FROM_3GPP_TO_NON_3GPP                 = 4,
  GTPC_CAUSE_VALUE_ISR_DEACTIVATION                                  = 5,
  GTPC_CAUSE_VALUE_ERROR_INDICATION_RECEIVED_FROM_RNC_ENODEB_S4_SGSN = 6,
  GTPC_CAUSE_VALUE_IMSI_DETACH_ONLY                                  = 7,
  GTPC_CAUSE_VALUE_REACTIVATION_REQUESTED                            = 8,
  GTPC_CAUSE_VALUE_PDN_RECONNECTION_TO_THIS_APN_DISALLOWED           = 9,
  GTPC_CAUSE_VALUE_ACCESS_CHANGED_FROM_NON_3GPP_TO_3GPP              = 10,
  GTPC_CAUSE_VALUE_PDN_CONNECTION_INACTIVITY_TIMER_EXPIRES           = 11,
  // Spare. This value range shall be used by Cause values in an initial/request message.
  GTPC_CAUSE_VALUE_REQUEST_ACCEPTED                               = 16,
  GTPC_CAUSE_VALUE_REQUEST_ACCEPTED_PARTIALLY                     = 17,
  GTPC_CAUSE_VALUE_NEW_PDN_TYPE_DUE_TO_NETWORK_PREFERENCE         = 18,
  GTPC_CAUSE_VALUE_NEW_PDN_TYPE_DUE_TO_SINGLE_ADDRESS_BEARER_ONLY = 19,
  // 20-63 Spare.
  GTPC_CAUSE_VALUE_CONTEXT_NOT_FOUND                  = 64,
  GTPC_CAUSE_VALUE_INVALID_MESSAGE_FORMAT             = 65,
  GTPC_CAUSE_VALUE_VERSION_NOT_SUPPORTED_BY_NEXT_PEER = 66,
  GTPC_CAUSE_VALUE_INVALID_LENGTH                     = 67,
  GTPC_CAUSE_VALUE_SERVICE_NOT_SUPPORTED              = 68,
  GTPC_CAUSE_VALUE_MANDATORY_IE_INCORRECT             = 69,
  GTPC_CAUSE_VALUE_MANDATORY_IE_MISSING               = 70,
  // 71 Shall not be used.
  GTPC_CAUSE_VALUE_SYSTEM_FAILURE                       = 72,
  GTPC_CAUSE_VALUE_NO_RESOURCES_AVAILABLE               = 73,
  GTPC_CAUSE_VALUE_SEMANTIC_ERROR_IN_THE_TFT_OPERATION  = 74,
  GTPC_CAUSE_VALUE_SYNTACTIC_ERROR_IN_THE_TFT_OPERATION = 75,
  GTPC_CAUSE_VALUE_SEMANTIC_ERRORS_IN_PACKET_FILTER     = 76,
  GTPC_CAUSE_VALUE_SYNTACTIC_ERRORS_IN_PACKET_FILTER    = 77,
  GTPC_CAUSE_VALUE_MISSING_OR_UNKNOWN_APN               = 78,
  // 79 Shall not be used.
  GTPC_CAUSE_VALUE_GRE_KEY_NOT_FOUND                        = 80,
  GTPC_CAUSE_VALUE_RELOCATION_FAILURE                       = 81,
  GTPC_CAUSE_VALUE_DENIED_IN_RAT                            = 82,
  GTPC_CAUSE_VALUE_PREFERRED_PDN_TYPE_NOT_SUPPORTED         = 83,
  GTPC_CAUSE_VALUE_ALL_DYNAMIC_ADDRESSES_ARE_OCCUPIED       = 84,
  GTPC_CAUSE_VALUE_UE_CONTEXT_WITHOUT_TFT_ALREADY_ACTIVATED = 85,
  GTPC_CAUSE_VALUE_PROTOCOL_TYPE_NOT_SUPPORTED              = 86,
  GTPC_CAUSE_VALUE_UE_NOT_RESPONDING                        = 87,
  GTPC_CAUSE_VALUE_UE_REFUSES                               = 88,
  GTPC_CAUSE_VALUE_SERVICE_DENIED                           = 89,
  GTPC_CAUSE_VALUE_UNABLE_TO_PAGE_UE                        = 90,
  GTPC_CAUSE_VALUE_NO_MEMORY_AVAILABLE                      = 91,
  GTPC_CAUSE_VALUE_USER_AUTHENTICATION_FAILED               = 92,
  GTPC_CAUSE_VALUE_APN_ACCESS_DENIED_NO_SUBSCRIPTION        = 93,
  GTPC_CAUSE_VALUE_REQUEST_REJECTED                         = 94,
  GTPC_CAUSE_VALUE_P_TMSI_SIGNATURE_MISMATCH                = 95,
  GTPC_CAUSE_VALUE_IMSI_IMEI_NOT_KNOWN                      = 96,
  GTPC_CAUSE_VALUE_SEMANTIC_ERROR_IN_THE_TAD_OPERATION      = 97,
  GTPC_CAUSE_VALUE_SYNTACTIC_ERROR_IN_THE_TAD_OPERATION     = 98,
  // 99 Shall not be used.
  GTPC_CAUSE_VALUE_REMOTE_PEER_NOT_RESPONDING                                                         = 100,
  GTPC_CAUSE_VALUE_COLLISION_WITH_NETWORK_INITIATED_REQUEST                                           = 101,
  GTPC_CAUSE_VALUE_UNABLE_TO_PAGE_UE_DUE_TO_SUSPENSION                                                = 102,
  GTPC_CAUSE_VALUE_CONDITIONAL_IE_MISSING                                                             = 103,
  GTPC_CAUSE_VALUE_APN_RESTRICTION_TYPE_INCOMPATIBLE_WITH_CURRENTLY_ACTIVE_PDN_CONNECTION             = 104,
  GTPC_CAUSE_VALUE_INVALID_OVERALL_LENGTH_OF_THE_TRIGGERED_RESPONSE_MSG_AND_A_PIGGYBACKED_INITIAL_MSG = 105,
  GTPC_CAUSE_VALUE_DATA_FORWARDING_NOT_SUPPORTED                                                      = 106,
  GTPC_CAUSE_VALUE_INVALID_REPLY_FROM_REMOTE_PEER                                                     = 107,
  GTPC_CAUSE_VALUE_FALLBACK_TO_GTPV1                                                                  = 108,
  GTPC_CAUSE_VALUE_INVALID_PEER                                                                       = 109,
  GTPC_CAUSE_VALUE_TEMPORARILY_REJECTED_DUE_TO_HANDOVER_PROCEDURE_IN_PROGRESS                         = 110,
  GTPC_CAUSE_VALUE_MODIFICATIONS_NOT_LIMITED_TO_S1_U_BEARERS                                          = 111,
  GTPC_CAUSE_VALUE_REQUEST_REJECTED_FOR_A_PMIPV6_REASON                                               = 112,
  GTPC_CAUSE_VALUE_APN_CONGESTION                                                                     = 113,
  GTPC_CAUSE_VALUE_BEARER_HANDLING_NOT_SUPPORTED                                                      = 114,
  GTPC_CAUSE_VALUE_UE_ALREADY_RE_ATTACHED                                                             = 115,
  GTPC_CAUSE_VALUE_MULTIPLE_PDN_CONNECTIONS_FOR_A_GIVEN_APN_NOT_ALLOWED                               = 116
  // 117-239 Spare. For future use in a triggered/response message.
  // 240-255 Spare. For future use in an initial/request message.
};

struct gtpc_cause_ie {
  enum gtpc_cause_value cause_value;
  bool                  pce;
  bool                  bce;
  bool                  cs;
  bool                  offending_ie_present; // selects the 10-octet (n=6) vs 6-octet (n=2) wire form
  enum gtpc_ie_type     offending_ie_type;
  uint16_t              length_of_offending_ie; // per clause 8.4, always packed as 0 when offending_ie_present
  uint8_t               offending_ie_instance;
};

/**
 * Packs a Cause IE. If cause.length_of_offending_ie != 0 (or offending_ie_type
 * is otherwise meaningfully set), the 4-octet offending-IE block is included
 * (n=6, 10 octets total); otherwise the short form is used (n=2, 6 octets
 * total), per clause 8.4's two valid lengths. Per clause 8.4, when the
 * offending-IE block is present, its own Length field is always written as 0.
 */
int gtpc_pack_cause_ie(const gtpc_cause_ie& cause, uint8_t instance, srsran::byte_buffer_t* pdu);
int gtpc_unpack_cause_ie(const uint8_t* ptr, const gtpc_ie_header_t& ie_header, gtpc_cause_ie* cause);

/****************************************************************************
 *
 * GTP-C Recovery IE
 * Ref: 3GPP TS 29.274 v10.14.0 Figure 8.5-1
 *
 ***************************************************************************/
/*
 * The Recovery (Restart Counter) IE should be kept as an uint8_t.
 */
int gtpc_pack_recovery_ie(uint8_t restart_counter, uint8_t instance, srsran::byte_buffer_t* pdu);
int gtpc_unpack_recovery_ie(const uint8_t* ptr, const gtpc_ie_header_t& ie_header, uint8_t* restart_counter);

/****************************************************************************
 *
 * GTP-C Access Point Name IE
 * Ref: 3GPP TS 29.274 v10.14.0 Figure 8.6-1
 *
 ***************************************************************************/
/*
 * APN IE should be kept as an null terminated string.
 * This string will be kept in a char[MAX_APN_LENGTH] buffer.
 */
#define MAX_APN_LENGTH 1024

/****************************************************************************
 *
 * GTP-C Aggregate Maximum bit-rate IE
 * Ref: 3GPP TS 29.274 v10.14.0 Table 8.7-1
 *
 ***************************************************************************/
struct gtpc_ambr_ie {
  uint32_t apn_ambr_uplink;
  uint32_t apn_ambr_downlink;
};

/****************************************************************************
 *
 * GTP-C EPS Bearer ID address IE
 * Ref: 3GPP TS 29.274 v10.14.0 Figure 8.8-1
 *
 ***************************************************************************/
/*
 * The EPS Bearer ID (EBI) IE should be kept as an uint8_t.
 */

/****************************************************************************
 *
 * GTP-C IP address IE
 * Ref: 3GPP TS 29.274 v10.14.0 Figure 8.9-1
 *
 ***************************************************************************/
/*
 * IP addresse IEs should the sockaddr_storage struct, which can hold IPv4
 * and IPv6 addresses.
 */

// TODO
// TODO IEs between 8.10 and 8.13 missing
// TODO

/****************************************************************************
 *
 * GTP-C PDN Type IE
 * Ref: 3GPP TS 29.274 v10.14.0 Figure 8.14-1
 *
 ***************************************************************************/
enum gtpc_pdn_type { GTPC_PDN_TYPE_IPV4 = 1, GTPC_PDN_TYPE_IPV6 = 2, GTPC_PDN_TYPE_IPV4V6 = 3 };

struct gtpc_pdn_address_allocation_ie {
  enum gtpc_pdn_type pdn_type;
  bool               ipv4_present;
  bool               ipv6_present;
  in_addr_t          ipv4;
  struct in6_addr    ipv6;
};

/****************************************************************************
 *
 * GTP-C Bearer Quality of Service IE
 * Ref: 3GPP TS 29.274 v19.6.0 clause 8.15, Figure 8.15-1
 *
 * NOTE: prior to this revision, mbr_ul/mbr_dl/gbr_ul/gbr_dl were uint8_t --
 * incorrect against the spec, which gives each of these 4 fields 5 octets
 * (40 bits) on the wire (kbps, binary value). uint8_t cannot represent a
 * real bit rate. Widened to uint64_t (40 bits fits) since nothing in this
 * codebase currently packs/unpacks this struct (no working Create Session
 * Request/Response serialization exists yet), so this is a safe fix, not a
 * behavior change to anything working today.
 *
 ***************************************************************************/
struct gtpc_bearer_qos_ie {
  struct {
    uint8_t pvi : 1;
    uint8_t spare : 1;
    uint8_t pl : 4;
    uint8_t pci : 1;
    uint8_t spare2 : 1;
  } arp;
  uint8_t  qci;
  uint64_t mbr_ul; // 40 bits significant (5 octets on the wire)
  uint64_t mbr_dl; // 40 bits significant
  uint64_t gbr_ul; // 40 bits significant
  uint64_t gbr_dl; // 40 bits significant
};

/**
 * Packs/unpacks a Bearer QoS IE (26 octets: 4-octet IE header + 1 ARP octet +
 * 1 QCI octet + 4x5-octet rate fields). mbr_ul/mbr_dl/gbr_ul/gbr_dl are
 * written/read as the low 40 bits only, big-endian.
 */
int gtpc_pack_bearer_qos_ie(const gtpc_bearer_qos_ie& qos, uint8_t instance, srsran::byte_buffer_t* pdu);
int gtpc_unpack_bearer_qos_ie(const uint8_t* ptr, const gtpc_ie_header_t& ie_header, gtpc_bearer_qos_ie* qos);

// TODO
// TODO IEs between 8.16 and 8.17 missing
// TODO

/****************************************************************************
 *
 * GTP-C RAT Type IE
 * Ref: 3GPP TS 29.274 v10.14.0 Figure 8.17-1
 *
 ***************************************************************************/

enum gtpc_rat_type { UTRAN = 1, GERAN, WLAN, GAN, HSPA_EVOLUTION, EUTRAN, Virtual };

// TODO
// TODO IEs between 8.17 and 8.22 missing
// TODO

/****************************************************************************
 *
 * GTP-C Fully Qualified Tunnel End-point Identifier (F-TEID) IE
 * Ref: 3GPP TS 29.274 v10.14.0 Figure 8.22-1
 *
 ***************************************************************************/
enum gtpc_interface_type {
  S1_U_ENODEB_GTP_U_INTERFACE,
  S1_U_SGW_GTP_U_INTERFACE,
  S12_RNC_GTP_U_INTERFACE,
  S12_SGW_GTP_U_INTERFACE,
  S5_S8_SGW_GTP_U_INTERFACE,
  S5_S8_PGW_GTP_U_INTERFACE,
  S5_S8_SGW_GTP_C_INTERFACE,
  S5_S8_PGW_GTP_C_INTERFACE,
  S5_S8_SGW_PMIPV6_INTERFACE, //(the 32 bit GRE key is encoded in 32 bit TEID field and since alternate CoA is not used
                              // the control plane and user plane addresses are the same for PMIPv6)
  S5_S8_PGW_PMIPV6_INTERFACE, //(the 32 bit GRE key is encoded in 32 bit TEID field and the control plane and user plane
                              // addresses are the same for PMIPv6)
  S11_MME_GTP_C_INTERFACE,
  S11_S4_SGW_GTP_C_INTERFACE,
  S10_MME_GTP_C_INTERFACE,
  S3_MME_GTP_C_INTERFACE,
  S3_SGSN_GTP_C_INTERFACE,
  S4_SGSN_GTP_U_INTERFACE,
  S4_SGW_GTP_U_INTERFACE,
  S4_SGSN_GTP_C_INTERFACE,
  S16_SGSN_GTP_C_INTERFACE,
  ENODEB_GTP_U_INTERFACE_FOR_DL_DATA_FORWARDING,
  ENODEB_GTP_U_INTERFACE_FOR_UL_DATA_FORWARDING,
  RNC_GTP_U_INTERFACE_FOR_DATA_FORWARDING,
  SGSN_GTP_U_INTERFACE_FOR_DATA_FORWARDING,
  SGW_GTP_U_INTERFACE_FOR_DL_DATA_FORWARDING,
  SM_MBMS_GW_GTP_C_INTERFACE,
  SN_MBMS_GW_GTP_C_INTERFACE,
  SM_MME_GTP_C_INTERFACE,
  SN_SGSN_GTP_C_INTERFACE,
  SGW_GTP_U_INTERFACE_FOR_UL_DATA_FORWARDING,
  SN_SGSN_GTP_U_INTERFACE,
  S2B_EPDG_GTP_C_INTERFACE,
  S2B_U_EPDG_GTP_U_INTERFACE,
  S2B_PGW_GTP_C_INTERFACE,
  S2B_U_PGW_GTP_U_INTERFACE
};

typedef struct gtpc_f_teid_ie {
  bool                     ipv4_present;
  bool                     ipv6_present;
  enum gtpc_interface_type interface_type;
  uint32_t                 teid;
  in_addr_t                ipv4;
  struct in6_addr          ipv6; // TODO
} gtp_fteid_t;

/**
 * Packs/unpacks an F-TEID IE. Confirmed against TS 29.274 v19.6.0 clause 8.22:
 * octet 5 = V4(bit8)|V6(bit7)|Interface Type(bits6-1); octets 6-9 = TEID/GRE
 * key; then IPv4 (4 octets) if V4 set, then IPv6 (16 octets) if V6 set. The
 * gtpc_interface_type enum's implicit ordinal values were verified to match
 * the spec's numeric Interface Type codes exactly (e.g. SM_MBMS_GW_GTP_C_INTERFACE
 * = 24, SN_MBMS_GW_GTP_C_INTERFACE = 25, SM_MME_GTP_C_INTERFACE = 26, matching
 * Table 8.22 verbatim), so the enum value can be cast directly to the 6-bit
 * wire field.
 */
int gtpc_pack_f_teid_ie(const gtpc_f_teid_ie& fteid, uint8_t instance, srsran::byte_buffer_t* pdu);
int gtpc_unpack_f_teid_ie(const uint8_t* ptr, const gtpc_ie_header_t& ie_header, gtpc_f_teid_ie* fteid);

// TODO
// TODO IEs between 8.22 and 8.28 missing
// TODO

/****************************************************************************
 *
 * GTP-C Bearer Context IE
 * Ref: 3GPP TS 29.274 v10.14.0 Table 8.28-1
 *
 ***************************************************************************/
// The usage of this grouped IE is specific to the GTP-C message being sent.
// As such, each GTP-C message will define it's bearer context structures
// locally, according to the rules of  TS 29.274 v10.14.0 Section 7.

/****************************************************************************
 *
 * GTP-C generic opaque IE
 *
 * Used for IEs whose internal Value-field layout is not available to this
 * codebase from primary source. As of this revision the only remaining
 * user is Private Extension (no clause text obtained at all) -- MBMS Time
 * to Data Transfer was resolved via TS 48.018 (see below) and no longer
 * needs this. The raw bytes are stored/forwarded verbatim, never
 * interpreted. std::vector (not a fixed-size array) avoids picking an
 * arbitrary bound before the real format is known.
 *
 ***************************************************************************/
struct gtpc_opaque_ie {
  std::vector<uint8_t> value;
};

/****************************************************************************
 *
 * GTP-C Private Extension IE
 * Ref: 3GPP TS 29.274 -- no clause-8.x text available to this codebase.
 * Treated as fully opaque (type + length + instance + raw value bytes);
 * do NOT assume the commonly-known Enterprise-ID+Value split without the
 * actual clause text.
 *
 ***************************************************************************/
int gtpc_pack_private_extension_ie(const gtpc_opaque_ie& ext, uint8_t instance, srsran::byte_buffer_t* pdu);
int gtpc_unpack_private_extension_ie(const uint8_t* ptr, const gtpc_ie_header_t& ie_header, gtpc_opaque_ie* ext);

/****************************************************************************
 *
 * GTP-C MBMS Session Duration IE
 * Ref: 3GPP TS 29.274 v19.6.0 clause 8.69 (envelope) + TS 29.061 v20.0.0
 * clause 17.7.7 / AVP 904 (internal 3-octet seconds+days encoding, obtained
 * after the initial design and confirmed to fully resolve this IE).
 *
 ***************************************************************************/
struct gtpc_mbms_session_duration_ie {
  // Total duration in seconds, 0-1641600 (19 days). 0 is the spec's own
  // reserved value meaning "indefinite / always-on".
  uint32_t duration_sec;
};
int gtpc_pack_mbms_session_duration_ie(const gtpc_mbms_session_duration_ie& dur,
                                        uint8_t                              instance,
                                        srsran::byte_buffer_t*               pdu);
int gtpc_unpack_mbms_session_duration_ie(const uint8_t*                  ptr,
                                          const gtpc_ie_header_t&         ie_header,
                                          gtpc_mbms_session_duration_ie*  dur);

/****************************************************************************
 *
 * GTP-C MBMS Service Area IE
 * Ref: 3GPP TS 29.274 v19.6.0 clause 8.70 (envelope) + TS 29.061 v20.0.0
 * clause 17.7.6 / AVP 903 (internal count+list encoding, obtained after the
 * initial design and confirmed to fully resolve this IE).
 *
 ***************************************************************************/
struct gtpc_mbms_service_area_ie {
  // Each code is an opaque 16-bit MBMS Service Area Identity (TS 23.003
  // clause 15.3 defines the geographic meaning; not needed by this codec).
  std::vector<uint16_t> sai_codes;
};
int gtpc_pack_mbms_service_area_ie(const gtpc_mbms_service_area_ie& area,
                                    uint8_t                          instance,
                                    srsran::byte_buffer_t*           pdu);
int gtpc_unpack_mbms_service_area_ie(const uint8_t*              ptr,
                                      const gtpc_ie_header_t&     ie_header,
                                      gtpc_mbms_service_area_ie* area);

/****************************************************************************
 *
 * GTP-C MBMS Session Identifier IE
 * Ref: 3GPP TS 29.274 v19.6.0 clause 8.71 -- opaque 1-octet value allocated
 * by the BM-SC (fully positioned on the wire; only its semantic allowed-value
 * range is deferred to TS 29.061, which does not affect wire encoding).
 *
 ***************************************************************************/
struct gtpc_mbms_session_id_ie {
  uint8_t session_id;
};
int gtpc_pack_mbms_session_id_ie(const gtpc_mbms_session_id_ie& id, uint8_t instance, srsran::byte_buffer_t* pdu);
int gtpc_unpack_mbms_session_id_ie(const uint8_t* ptr, const gtpc_ie_header_t& ie_header, gtpc_mbms_session_id_ie* id);

/****************************************************************************
 *
 * GTP-C MBMS Flow Identifier IE
 * Ref: 3GPP TS 29.274 v19.6.0 clause 8.72 -- opaque 2-octet value.
 *
 ***************************************************************************/
struct gtpc_mbms_flow_id_ie {
  uint16_t flow_id;
};
int gtpc_pack_mbms_flow_id_ie(const gtpc_mbms_flow_id_ie& flow, uint8_t instance, srsran::byte_buffer_t* pdu);
int gtpc_unpack_mbms_flow_id_ie(const uint8_t* ptr, const gtpc_ie_header_t& ie_header, gtpc_mbms_flow_id_ie* flow);

/****************************************************************************
 *
 * GTP-C MBMS IP Multicast Distribution IE
 * Ref: 3GPP TS 29.274 v19.6.0 clause 8.73, Figure 8.73-1
 *
 ***************************************************************************/
enum gtpc_mbms_hc_indicator { GTPC_MBMS_HC_UNCOMPRESSED = 0, GTPC_MBMS_HC_COMPRESSED = 1 };

struct gtpc_mbms_ip_mc_distrib_ie {
  uint32_t                   c_teid;
  bool                       dist_addr_is_ipv6; // false = IPv4 (4 octets), true = IPv6 (16 octets)
  in_addr_t                  dist_addr_ipv4;
  struct in6_addr            dist_addr_ipv6;
  bool                       source_addr_is_ipv6;
  in_addr_t                  source_addr_ipv4;
  struct in6_addr            source_addr_ipv6;
  enum gtpc_mbms_hc_indicator hc_indicator;
};
int gtpc_pack_mbms_ip_mc_distrib_ie(const gtpc_mbms_ip_mc_distrib_ie& distrib,
                                     uint8_t                          instance,
                                     srsran::byte_buffer_t*           pdu);
int gtpc_unpack_mbms_ip_mc_distrib_ie(const uint8_t*               ptr,
                                       const gtpc_ie_header_t&      ie_header,
                                       gtpc_mbms_ip_mc_distrib_ie* distrib);

/****************************************************************************
 *
 * GTP-C MBMS Distribution Acknowledge IE
 * Ref: 3GPP TS 29.274 v19.6.0 clause 8.74, Figure 8.74-1 -- Sn-interface
 * only per the Sm/Sn message tables, included here for union/catalog
 * symmetry; never populated on the Sm send path in this codebase.
 *
 ***************************************************************************/
enum gtpc_mbms_distribution_indication {
  GTPC_MBMS_DISTR_NONE = 0,
  GTPC_MBMS_DISTR_ALL  = 1,
  GTPC_MBMS_DISTR_SOME = 2
};
struct gtpc_mbms_distribution_ack_ie {
  enum gtpc_mbms_distribution_indication distr_ind;
};
int gtpc_pack_mbms_distribution_ack_ie(const gtpc_mbms_distribution_ack_ie& ack,
                                        uint8_t                              instance,
                                        srsran::byte_buffer_t*               pdu);
int gtpc_unpack_mbms_distribution_ack_ie(const uint8_t*                  ptr,
                                          const gtpc_ie_header_t&         ie_header,
                                          gtpc_mbms_distribution_ack_ie*  ack);

/****************************************************************************
 *
 * GTP-C TMGI IE
 * Ref: 3GPP TS 29.274 v19.6.0 clause 8.89, Figure 8.89-1
 *
 * mcc_bcd/mnc_bcd use this codebase's existing bcd_helpers.h BCD
 * representation (e.g. produced by string_to_mcc()/string_to_mnc()), NOT a
 * plmn_id_t class (no such class exists in bcd_helpers.h -- verified by
 * reading the actual file, correcting an unverified assumption from the
 * initial design). mbms_service_id is stored as uint32_t for convenience but
 * is exactly 3 wire octets (octets 8-10 of the 6-octet TMGI value) -- pack
 * writes only the low 3 bytes, unpack zero-extends into the top byte.
 *
 ***************************************************************************/
struct gtpc_tmgi_ie {
  uint16_t mcc_bcd;
  uint16_t mnc_bcd;
  uint32_t mbms_service_id; // low 24 bits significant
};
int gtpc_pack_tmgi_ie(const gtpc_tmgi_ie& tmgi, uint8_t instance, srsran::byte_buffer_t* pdu);
int gtpc_unpack_tmgi_ie(const uint8_t* ptr, const gtpc_ie_header_t& ie_header, gtpc_tmgi_ie* tmgi);

/****************************************************************************
 *
 * GTP-C Absolute Time of MBMS Data Transfer IE
 * Ref: 3GPP TS 29.274 v19.6.0 clause 8.95, Figure 8.95-1 -- standard NTP-64
 * timestamp (32-bit seconds since 1900-01-01, 32-bit fraction, granularity
 * 1/2^32 s).
 *
 ***************************************************************************/
struct gtpc_abs_time_mbms_data_transfer_ie {
  uint32_t ntp_seconds;
  uint32_t ntp_fraction;
};
int gtpc_pack_abs_time_mbms_data_transfer_ie(const gtpc_abs_time_mbms_data_transfer_ie& t,
                                              uint8_t                                    instance,
                                              srsran::byte_buffer_t*                     pdu);
int gtpc_unpack_abs_time_mbms_data_transfer_ie(const uint8_t*                        ptr,
                                                const gtpc_ie_header_t&               ie_header,
                                                gtpc_abs_time_mbms_data_transfer_ie* t);

/****************************************************************************
 *
 * GTP-C MBMS Flags IE
 * Ref: 3GPP TS 29.274 v19.6.0 clause 8.102, Figure 8.102-1
 *
 ***************************************************************************/
struct gtpc_mbms_flags_ie {
  bool msri; // MBMS Session Re-establishment Indication (bit 1)
  bool lmri; // Local MBMS Bearer Context Release Indication (bit 2)
};
int gtpc_pack_mbms_flags_ie(const gtpc_mbms_flags_ie& flags, uint8_t instance, srsran::byte_buffer_t* pdu);
int gtpc_unpack_mbms_flags_ie(const uint8_t* ptr, const gtpc_ie_header_t& ie_header, gtpc_mbms_flags_ie* flags);

/****************************************************************************
 *
 * GTP-C MBMS Time to Data Transfer IE
 * Ref: 3GPP TS 29.274 v19.6.0 clause 8.x (envelope: confirmed exactly 1
 * octet by TS 29.061 clause 17.7.14) + TS 48.018 v17.0.0 clause 11.3.92 /
 * Table 11.3.92.b (internal value-part coding, obtained after the initial
 * design and confirmed to fully resolve this IE -- the last one that had
 * been left opaque).
 *
 * Table 11.3.92.b's coding is a plain linear count: raw octet value N
 * (0-255) represents (N+1) seconds, i.e. 0x00=1s, 0x01=2s, ..., 0xFF=256s.
 *
 ***************************************************************************/
struct gtpc_mbms_time_to_data_transfer_ie {
  // Seconds between the Session Start/Update Request and the actual start
  // of data transfer, 1-256 inclusive (TS 48.018 Table 11.3.92.b -- there
  // is no "0 seconds" or "indefinite" value, unlike MBMS Session Duration).
  uint32_t seconds;
};
int gtpc_pack_mbms_time_to_data_transfer_ie(const gtpc_mbms_time_to_data_transfer_ie& t,
                                             uint8_t                                   instance,
                                             srsran::byte_buffer_t*                    pdu);
int gtpc_unpack_mbms_time_to_data_transfer_ie(const uint8_t*                        ptr,
                                               const gtpc_ie_header_t&               ie_header,
                                               gtpc_mbms_time_to_data_transfer_ie*  t);

/****************************************************************************
 *
 * GTP-C ECGI field
 * Ref: 3GPP TS 29.274 v19.6.0 clause 8.21.5, Figure 8.21.5-1
 *
 * Verified directly against clause text (an earlier draft of this codec had
 * flagged this as an unverified 3GPP-convention inference; the actual clause
 * confirms it): 7 octets total -- 3-octet PLMN (same swapped-BCD convention
 * as TMGI, reusing bcd_helpers.h) followed by a 28-bit ECI (E-UTRAN Cell
 * Identifier): its top 4 bits occupy the low nibble of the 4th octet (with
 * the high nibble spare), then 3 full octets of the remaining 24 bits.
 *
 ***************************************************************************/
struct gtpc_ecgi_field_t {
  uint16_t mcc_bcd;
  uint16_t mnc_bcd;
  uint32_t eci; // 28 bits significant
};

/****************************************************************************
 *
 * GTP-C ECGI List IE
 * Ref: 3GPP TS 29.274 v19.6.0 clause 8.121, Figure 8.121-1
 *
 ***************************************************************************/
struct gtpc_ecgi_list_ie {
  std::vector<gtpc_ecgi_field_t> ecgi_list;
};
int gtpc_pack_ecgi_list_ie(const gtpc_ecgi_list_ie& list, uint8_t instance, srsran::byte_buffer_t* pdu);
int gtpc_unpack_ecgi_list_ie(const uint8_t* ptr, const gtpc_ie_header_t& ie_header, gtpc_ecgi_list_ie* list);

} // namespace srsran
#endif // SRSRAN_GTPC_IES_H
