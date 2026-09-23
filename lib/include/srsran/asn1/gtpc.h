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
#ifndef SRSRAN_GTPC_H
#define SRSRAN_GTPC_H

#include "srsran/asn1/gtpc_msg.h"
#include "srsran/common/byte_buffer.h"
#include <stdint.h>

namespace srsran {

/*GTP-C Version*/
const uint8_t GTPC_V2 = 2;

/****************************************************************************
 * GTP-C v2 Header
 * Ref: 3GPP TS 29.274 v19.6.0 Section 5.1/5.4
 *
 * EPC-specific header (T always set to 1 for every message this codebase
 * sends, per section 5.5.1): 12 octets before any IEs.
 *
 *        | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 |
 *
 * 1      |  Version  | P | T | MP| S | S |
 * 2      |           Message Type        |
 * 3      |         Length (1st Octet)    |
 * 4      |         Length (2nd Octet)    |
 * 5-8    |              TEID             |
 * 9-11   |            Sequence            |
 * 12     |  Msg Priority | S | S | S | S |
 ***************************************************************************/
typedef struct gtpc_header {
  uint8_t  version;
  bool     piggyback;
  bool     teid_present;
  bool     mp_flag;
  uint8_t  type;
  uint16_t length; // message length in octets, excluding the mandatory first 4 header octets
  uint64_t teid;
  uint64_t sequence;
  uint8_t  message_priority; // low 4 bits significant, only meaningful if mp_flag is set
} gtpc_header_t;

/**
 * Packs the 12-octet EPC-specific GTPv2-C header. Computes and writes
 * length = 8 (TEID + Sequence + spare) + ie_body_len itself.
 */
int gtpc_header_pack(const gtpc_header_t& header, uint32_t ie_body_len, srsran::byte_buffer_t* pdu);

/**
 * Unpacks the 12-octet EPC-specific GTPv2-C header. Returns SRSRAN_ERROR if
 * the buffer is too short, version != 2, or teid_present (T flag) != true --
 * every Sm message uses the EPC-specific (T=1) header variant.
 */
int gtpc_header_unpack(const srsran::byte_buffer_t& pdu, gtpc_header_t* header);

/****************************************************************************
 * GTP-C v2 Payload
 * Ref: 3GPP TS 29.274 v10.14.0 Section 5
 *
 * Union that hold the different structures for the possible message types.
 ***************************************************************************/
typedef union gtpc_msg_choice {
  struct gtpc_create_session_request                        create_session_request;
  struct gtpc_create_session_response                       create_session_response;
  struct gtpc_modify_bearer_request                         modify_bearer_request;
  struct gtpc_modify_bearer_response                        modify_bearer_response;
  struct gtpc_release_access_bearers_request                release_access_bearers_request;
  struct gtpc_release_access_bearers_response               release_access_bearers_response;
  struct gtpc_delete_session_request                        delete_session_request;
  struct gtpc_delete_session_response                       delete_session_response;
  struct gtpc_downlink_data_notification                    downlink_data_notification;
  struct gtpc_downlink_data_notification_acknowledge        downlink_data_notification_acknowledge;
  struct gtpc_downlink_data_notification_failure_indication downlink_data_notification_failure_indication;
  // NOTE: the 6 MBMS session messages (gtpc_mbms_session_{start,update,stop}_{request,response})
  // are deliberately NOT added here. They contain std::vector-bearing members
  // (via gtpc_ecgi_list_ie / gtpc_mbms_service_area_ie / gtpc_opaque_ie), which
  // are non-trivial types; a plain C-style union cannot hold a non-trivial
  // member without the union itself gaining manual constructor/destructor
  // lifetime management. Rather than add that complexity, MBMS message
  // pack/unpack (gtpc_msg.h) operates on its own struct types directly (struct
  // + raw byte buffer, no union), and the transport layer (mme_gtpc.cc /
  // mbms_gw_gtpc.cc) dispatches on header.type with an explicit switch,
  // calling the matching typed pack/unpack function -- see gtpc_msg.h.
} gtpc_msg_choice_t;

/****************************************************************************
 * GTP-C v2 Message
 * Ref: 3GPP TS 29.274 v10.14.0
 *
 * This is the main structure to represent a GTP-C message. It is composed
 * of one GTP-C header and one union of structures, which can hold
 * all the possible GTP-C messages
 ***************************************************************************/
typedef struct gtpc_pdu {
  struct gtpc_header    header;
  union gtpc_msg_choice choice;
} gtpc_pdu_t;
} // namespace srsran
#endif // SRSRAN_GTPC_H
