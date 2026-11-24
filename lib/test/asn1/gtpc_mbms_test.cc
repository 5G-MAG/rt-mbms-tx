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

// Pack-then-unpack round-trip tests for the Sm interface GTPv2-C codec
// (MBMS Session Start/Update/Stop Request/Response), added alongside the
// first real TLV pack/unpack implementation in this codebase's GTP-C layer.

#include "srsran/asn1/gtpc.h"
#include "srsran/common/bcd_helpers.h"
#include "srsran/common/test_common.h"

using namespace srsran;

int test_header_round_trip()
{
  // Every combination of piggyback/mp_flag, since this is the highest
  // mechanical-error-risk code in the whole design (explicit bit shifts).
  for (int pb = 0; pb <= 1; pb++) {
    for (int mp = 0; mp <= 1; mp++) {
      gtpc_header_t hdr  = {};
      hdr.version        = GTPC_V2;
      hdr.piggyback      = pb != 0;
      hdr.teid_present   = true;
      hdr.mp_flag        = mp != 0;
      hdr.type           = GTPC_MSG_TYPE_MBMS_SESSION_START_REQUEST;
      hdr.teid           = 0xAABBCCDD;
      hdr.sequence       = 0x123456;
      hdr.message_priority = mp ? 7 : 0;

      // ie_body_len=0: this sub-test targets the 12-octet header's own bit
      // arithmetic in isolation, so no IE-body bytes are appended -- the
      // length field must therefore describe exactly what's in the buffer
      // (8 = TEID+Sequence+spare, no IE body) or gtpc_header_unpack's own
      // truncation check (by design) correctly rejects it as too short.
      byte_buffer_t buf;
      TESTASSERT(gtpc_header_pack(hdr, 0, &buf) == SRSRAN_SUCCESS);
      TESTASSERT(buf.N_bytes == 12);

      gtpc_header_t hdr2 = {};
      TESTASSERT(gtpc_header_unpack(buf, &hdr2) == SRSRAN_SUCCESS);
      TESTASSERT(hdr2.version == hdr.version);
      TESTASSERT(hdr2.piggyback == hdr.piggyback);
      TESTASSERT(hdr2.teid_present == hdr.teid_present);
      TESTASSERT(hdr2.mp_flag == hdr.mp_flag);
      TESTASSERT(hdr2.type == hdr.type);
      TESTASSERT(hdr2.length == 8);
      TESTASSERT(hdr2.teid == hdr.teid);
      TESTASSERT(hdr2.sequence == hdr.sequence);
      TESTASSERT(hdr2.message_priority == hdr.message_priority);
    }
  }
  return SRSRAN_SUCCESS;
}

int test_tmgi_round_trip()
{
  // PLMN 001/01 (2-digit MNC) and 234/56 (2-digit) and 310/410 (3-digit MNC).
  struct {
    const char* mcc;
    const char* mnc;
  } cases[] = {{"001", "01"}, {"234", "56"}, {"310", "410"}};

  for (auto& c : cases) {
    uint16_t mcc_bcd, mnc_bcd;
    TESTASSERT(string_to_mcc(c.mcc, &mcc_bcd));
    TESTASSERT(string_to_mnc(c.mnc, &mnc_bcd));

    gtpc_tmgi_ie tmgi   = {};
    tmgi.mcc_bcd        = mcc_bcd;
    tmgi.mnc_bcd        = mnc_bcd;
    tmgi.mbms_service_id = 0xABCDEF; // max 3-octet value

    byte_buffer_t buf;
    TESTASSERT(gtpc_pack_tmgi_ie(tmgi, 0, &buf) == SRSRAN_SUCCESS);
    TESTASSERT(buf.N_bytes == 10); // 4-octet IE header + 6-octet value

    gtpc_ie_header_t ie_hdr;
    TESTASSERT(gtpc_ie_header_unpack(buf.msg, buf.N_bytes, &ie_hdr) == SRSRAN_SUCCESS);
    TESTASSERT(ie_hdr.type == GTPC_IE_TYPE_TMGI);
    TESTASSERT(ie_hdr.length == 6);

    gtpc_tmgi_ie tmgi2;
    TESTASSERT(gtpc_unpack_tmgi_ie(&buf.msg[4], ie_hdr, &tmgi2) == SRSRAN_SUCCESS);
    TESTASSERT(tmgi2.mcc_bcd == tmgi.mcc_bcd);
    TESTASSERT(tmgi2.mnc_bcd == tmgi.mnc_bcd);
    TESTASSERT(tmgi2.mbms_service_id == tmgi.mbms_service_id);

    // Round-trip the MCC/MNC back to strings too, as an end-to-end sanity check.
    std::string mcc_str, mnc_str;
    TESTASSERT(mcc_to_string(tmgi2.mcc_bcd, &mcc_str));
    TESTASSERT(mnc_to_string(tmgi2.mnc_bcd, &mnc_str));
    TESTASSERT(mcc_str == c.mcc);
    TESTASSERT(mnc_str == c.mnc);
  }

  // Service ID boundary: 0 and 0xFFFFFF (max 3-octet value) must not overrun
  // into the TMGI's PLMN bytes or beyond the 6-octet value.
  for (uint32_t sid : {0u, 0x1u, 0xFFFFFFu}) {
    gtpc_tmgi_ie tmgi     = {};
    tmgi.mcc_bcd          = 0xF001;
    tmgi.mnc_bcd          = 0xFF01;
    tmgi.mbms_service_id  = sid;
    byte_buffer_t buf;
    TESTASSERT(gtpc_pack_tmgi_ie(tmgi, 0, &buf) == SRSRAN_SUCCESS);
    TESTASSERT(buf.N_bytes == 10);
    gtpc_ie_header_t ie_hdr;
    TESTASSERT(gtpc_ie_header_unpack(buf.msg, buf.N_bytes, &ie_hdr) == SRSRAN_SUCCESS);
    gtpc_tmgi_ie tmgi2;
    TESTASSERT(gtpc_unpack_tmgi_ie(&buf.msg[4], ie_hdr, &tmgi2) == SRSRAN_SUCCESS);
    TESTASSERT(tmgi2.mbms_service_id == sid);
  }
  return SRSRAN_SUCCESS;
}

int test_session_duration_round_trip()
{
  // 0 (indefinite), an ordinary value, exactly one day, and the 19-day max.
  uint32_t cases[] = {0, 3600, 86400, 1641600};
  for (uint32_t sec : cases) {
    gtpc_mbms_session_duration_ie dur = {sec};
    byte_buffer_t                 buf;
    TESTASSERT(gtpc_pack_mbms_session_duration_ie(dur, 0, &buf) == SRSRAN_SUCCESS);
    TESTASSERT(buf.N_bytes == 7); // 4-octet IE header + 3-octet value

    gtpc_ie_header_t ie_hdr;
    TESTASSERT(gtpc_ie_header_unpack(buf.msg, buf.N_bytes, &ie_hdr) == SRSRAN_SUCCESS);
    TESTASSERT(ie_hdr.length == 3);

    gtpc_mbms_session_duration_ie dur2;
    TESTASSERT(gtpc_unpack_mbms_session_duration_ie(&buf.msg[4], ie_hdr, &dur2) == SRSRAN_SUCCESS);
    TESTASSERT(dur2.duration_sec == sec);
  }
  // All-zero wire bytes must unpack to 0 (indefinite), not error.
  {
    uint8_t          zero_val[3] = {0, 0, 0};
    gtpc_ie_header_t ie_hdr      = {GTPC_IE_TYPE_MBMS_SESSION_DURATION, 3, 0};
    gtpc_mbms_session_duration_ie dur;
    TESTASSERT(gtpc_unpack_mbms_session_duration_ie(zero_val, ie_hdr, &dur) == SRSRAN_SUCCESS);
    TESTASSERT(dur.duration_sec == 0);
  }
  return SRSRAN_SUCCESS;
}

int test_service_area_round_trip()
{
  std::vector<std::vector<uint16_t>> cases = {{}, {42}, {}};
  cases[2].resize(255);
  for (uint32_t i = 0; i < 255; i++) {
    cases[2][i] = static_cast<uint16_t>(i + 1);
  }

  for (auto& codes : cases) {
    gtpc_mbms_service_area_ie area = {codes};
    byte_buffer_t             buf;
    TESTASSERT(gtpc_pack_mbms_service_area_ie(area, 0, &buf) == SRSRAN_SUCCESS);
    TESTASSERT(buf.N_bytes == 4 + 1 + codes.size() * 2);

    gtpc_ie_header_t ie_hdr;
    TESTASSERT(gtpc_ie_header_unpack(buf.msg, buf.N_bytes, &ie_hdr) == SRSRAN_SUCCESS);

    gtpc_mbms_service_area_ie area2;
    TESTASSERT(gtpc_unpack_mbms_service_area_ie(&buf.msg[4], ie_hdr, &area2) == SRSRAN_SUCCESS);
    TESTASSERT(area2.sai_codes == codes);
  }
  return SRSRAN_SUCCESS;
}

int test_bearer_qos_round_trip()
{
  gtpc_bearer_qos_ie qos = {};
  qos.arp.pvi            = 1;
  qos.arp.pl              = 9;
  qos.arp.pci             = 1;
  qos.qci                 = 5;
  qos.mbr_ul              = 0xABCDEF1234ULL & 0xFFFFFFFFFFULL; // 40-bit max-ish value
  qos.mbr_dl              = 1000000;
  qos.gbr_ul              = 0;
  qos.gbr_dl              = 0xFFFFFFFFFFULL; // full 40-bit max

  byte_buffer_t buf;
  TESTASSERT(gtpc_pack_bearer_qos_ie(qos, 0, &buf) == SRSRAN_SUCCESS);
  TESTASSERT(buf.N_bytes == 4 + 22);

  gtpc_ie_header_t ie_hdr;
  TESTASSERT(gtpc_ie_header_unpack(buf.msg, buf.N_bytes, &ie_hdr) == SRSRAN_SUCCESS);

  gtpc_bearer_qos_ie qos2;
  TESTASSERT(gtpc_unpack_bearer_qos_ie(&buf.msg[4], ie_hdr, &qos2) == SRSRAN_SUCCESS);
  TESTASSERT(qos2.arp.pvi == qos.arp.pvi);
  TESTASSERT(qos2.arp.pl == qos.arp.pl);
  TESTASSERT(qos2.arp.pci == qos.arp.pci);
  TESTASSERT(qos2.qci == qos.qci);
  TESTASSERT(qos2.mbr_ul == qos.mbr_ul);
  TESTASSERT(qos2.mbr_dl == qos.mbr_dl);
  TESTASSERT(qos2.gbr_ul == qos.gbr_ul);
  TESTASSERT(qos2.gbr_dl == qos.gbr_dl);
  return SRSRAN_SUCCESS;
}

int test_ecgi_list_round_trip()
{
  gtpc_ecgi_list_ie list;
  gtpc_ecgi_field_t f1, f2;
  string_to_mcc("001", &f1.mcc_bcd);
  string_to_mnc("01", &f1.mnc_bcd);
  f1.eci = 0x0FFFFFFF; // max 28-bit value
  string_to_mcc("999", &f2.mcc_bcd);
  string_to_mnc("999", &f2.mnc_bcd);
  f2.eci = 0;
  list.ecgi_list.push_back(f1);
  list.ecgi_list.push_back(f2);

  byte_buffer_t buf;
  TESTASSERT(gtpc_pack_ecgi_list_ie(list, 0, &buf) == SRSRAN_SUCCESS);
  TESTASSERT(buf.N_bytes == 4 + 2 + 2 * 7);

  gtpc_ie_header_t ie_hdr;
  TESTASSERT(gtpc_ie_header_unpack(buf.msg, buf.N_bytes, &ie_hdr) == SRSRAN_SUCCESS);

  gtpc_ecgi_list_ie list2;
  TESTASSERT(gtpc_unpack_ecgi_list_ie(&buf.msg[4], ie_hdr, &list2) == SRSRAN_SUCCESS);
  TESTASSERT(list2.ecgi_list.size() == 2);
  TESTASSERT(list2.ecgi_list[0].mcc_bcd == f1.mcc_bcd);
  TESTASSERT(list2.ecgi_list[0].mnc_bcd == f1.mnc_bcd);
  TESTASSERT(list2.ecgi_list[0].eci == f1.eci);
  TESTASSERT(list2.ecgi_list[1].eci == f2.eci);
  return SRSRAN_SUCCESS;
}

int test_time_to_data_transfer_round_trip()
{
  // Resolved via TS 48.018 clause 11.3.92 Table 11.3.92.b: raw octet N ->
  // (N+1) seconds. Boundary values: 1s (octet 0x00) and 256s (octet 0xFF).
  for (uint32_t sec : {1u, 2u, 100u, 256u}) {
    gtpc_mbms_time_to_data_transfer_ie t = {sec};
    byte_buffer_t                      buf;
    TESTASSERT(gtpc_pack_mbms_time_to_data_transfer_ie(t, 0, &buf) == SRSRAN_SUCCESS);
    TESTASSERT(buf.N_bytes == 5); // 4-octet IE header + 1-octet value

    gtpc_ie_header_t ie_hdr;
    TESTASSERT(gtpc_ie_header_unpack(buf.msg, buf.N_bytes, &ie_hdr) == SRSRAN_SUCCESS);
    TESTASSERT(ie_hdr.length == 1);

    gtpc_mbms_time_to_data_transfer_ie t2;
    TESTASSERT(gtpc_unpack_mbms_time_to_data_transfer_ie(&buf.msg[4], ie_hdr, &t2) == SRSRAN_SUCCESS);
    TESTASSERT(t2.seconds == sec);
  }
  // Out-of-range values (0 and >256) must be rejected, not silently clamped.
  {
    gtpc_mbms_time_to_data_transfer_ie bad = {0};
    byte_buffer_t                      buf;
    TESTASSERT(gtpc_pack_mbms_time_to_data_transfer_ie(bad, 0, &buf) == SRSRAN_ERROR);
  }
  {
    gtpc_mbms_time_to_data_transfer_ie bad = {257};
    byte_buffer_t                      buf;
    TESTASSERT(gtpc_pack_mbms_time_to_data_transfer_ie(bad, 0, &buf) == SRSRAN_ERROR);
  }
  return SRSRAN_SUCCESS;
}

int test_opaque_round_trip()
{
  // The one remaining genuinely-opaque field (Private Extension): confirm
  // bytes survive verbatim with zero reinterpretation.
  gtpc_opaque_ie raw;
  raw.value = {0x42, 0x01, 0xFF};
  byte_buffer_t buf;
  TESTASSERT(gtpc_pack_private_extension_ie(raw, 0, &buf) == SRSRAN_SUCCESS);
  gtpc_ie_header_t ie_hdr;
  TESTASSERT(gtpc_ie_header_unpack(buf.msg, buf.N_bytes, &ie_hdr) == SRSRAN_SUCCESS);
  gtpc_opaque_ie raw2;
  TESTASSERT(gtpc_unpack_private_extension_ie(&buf.msg[4], ie_hdr, &raw2) == SRSRAN_SUCCESS);
  TESTASSERT(raw2.value == raw.value);
  return SRSRAN_SUCCESS;
}

int test_session_start_request_round_trip()
{
  gtpc_mbms_session_start_request req = {};
  req.sender_f_teid.ipv4_present      = true;
  req.sender_f_teid.interface_type    = SM_MBMS_GW_GTP_C_INTERFACE;
  req.sender_f_teid.teid              = 0x11223344;
  req.sender_f_teid.ipv4              = 0x0100007F; // 127.0.0.1 network-order-ish for test purposes

  string_to_mcc("001", &req.tmgi.mcc_bcd);
  string_to_mnc("01", &req.tmgi.mnc_bcd);
  req.tmgi.mbms_service_id = 1;

  req.mbms_session_duration.duration_sec = 3600;
  req.mbms_service_area.sai_codes        = {1, 2, 3};

  req.qos_profile.qci    = 1;
  req.qos_profile.mbr_dl = 0;

  req.mbms_ip_multicast_distrib.c_teid              = 0xAAAA;
  req.mbms_ip_multicast_distrib.dist_addr_is_ipv6    = false;
  req.mbms_ip_multicast_distrib.dist_addr_ipv4       = 0x0100A8C0; // arbitrary
  req.mbms_ip_multicast_distrib.source_addr_is_ipv6  = false;
  req.mbms_ip_multicast_distrib.source_addr_ipv4     = 0x0200A8C0;
  req.mbms_ip_multicast_distrib.hc_indicator         = GTPC_MBMS_HC_UNCOMPRESSED;

  req.mbms_session_id_present    = true;
  req.mbms_session_id.session_id = 7;
  req.mbms_flow_id_present       = true;
  req.mbms_flow_id.flow_id       = 42;

  byte_buffer_t body;
  TESTASSERT(gtpc_pack_mbms_session_start_request(req, &body) == SRSRAN_SUCCESS);

  gtpc_mbms_session_start_request req2;
  TESTASSERT(gtpc_unpack_mbms_session_start_request(body.msg, body.N_bytes, &req2) == SRSRAN_SUCCESS);

  TESTASSERT(req2.sender_f_teid.teid == req.sender_f_teid.teid);
  TESTASSERT(req2.tmgi.mcc_bcd == req.tmgi.mcc_bcd);
  TESTASSERT(req2.tmgi.mbms_service_id == req.tmgi.mbms_service_id);
  TESTASSERT(req2.mbms_session_duration.duration_sec == req.mbms_session_duration.duration_sec);
  TESTASSERT(req2.mbms_service_area.sai_codes == req.mbms_service_area.sai_codes);
  TESTASSERT(req2.qos_profile.qci == req.qos_profile.qci);
  TESTASSERT(req2.mbms_ip_multicast_distrib.c_teid == req.mbms_ip_multicast_distrib.c_teid);
  TESTASSERT(req2.mbms_session_id_present);
  TESTASSERT(req2.mbms_session_id.session_id == 7);
  TESTASSERT(req2.mbms_flow_id_present);
  TESTASSERT(req2.mbms_flow_id.flow_id == 42);
  // Fields never set should not spuriously appear as present.
  TESTASSERT(!req2.recovery_present);
  TESTASSERT(!req2.private_extension_present);
  TESTASSERT(!req2.ecgi_list_present);
  return SRSRAN_SUCCESS;
}

int test_session_stop_request_round_trip()
{
  // The TMGI-based correlation fix: confirm the wire bytes actually carry a
  // TMGI IE (an earlier, since-corrected draft had none at all).
  gtpc_mbms_session_stop_request req = {};
  string_to_mcc("310", &req.tmgi.mcc_bcd);
  string_to_mnc("410", &req.tmgi.mnc_bcd);
  req.tmgi.mbms_service_id = 99;

  byte_buffer_t body;
  TESTASSERT(gtpc_pack_mbms_session_stop_request(req, &body) == SRSRAN_SUCCESS);

  // Confirm a TMGI IE (type 158) is actually present on the wire.
  bool found_tmgi = false;
  uint32_t off    = 0;
  while (off + 4 <= body.N_bytes) {
    gtpc_ie_header_t ie_hdr;
    TESTASSERT(gtpc_ie_header_unpack(&body.msg[off], body.N_bytes - off, &ie_hdr) == SRSRAN_SUCCESS);
    if (ie_hdr.type == GTPC_IE_TYPE_TMGI) {
      found_tmgi = true;
    }
    off += 4 + ie_hdr.length;
  }
  TESTASSERT(found_tmgi);

  gtpc_mbms_session_stop_request req2;
  TESTASSERT(gtpc_unpack_mbms_session_stop_request(body.msg, body.N_bytes, &req2) == SRSRAN_SUCCESS);
  TESTASSERT(req2.tmgi.mcc_bcd == req.tmgi.mcc_bcd);
  TESTASSERT(req2.tmgi.mnc_bcd == req.tmgi.mnc_bcd);
  TESTASSERT(req2.tmgi.mbms_service_id == req.tmgi.mbms_service_id);
  return SRSRAN_SUCCESS;
}

int test_malformed_input_rejected()
{
  // Truncated buffer (less than 12 octets) must fail header unpack.
  byte_buffer_t short_buf;
  uint8_t       three_bytes[3] = {0x48, 0x01, 0x00};
  short_buf.append_bytes(three_bytes, 3);
  gtpc_header_t hdr;
  TESTASSERT(gtpc_header_unpack(short_buf, &hdr) == SRSRAN_ERROR);

  // Header claiming a longer body than actually present (declared length
  // says a body follows, but the buffer is only the 12-octet header) must
  // also be rejected -- this is the exact truncated/malformed-datagram case
  // the length cross-check in gtpc_header_unpack exists to catch.
  {
    gtpc_header_t hdr_full  = {};
    hdr_full.version        = GTPC_V2;
    hdr_full.teid_present   = true;
    hdr_full.type           = GTPC_MSG_TYPE_MBMS_SESSION_START_REQUEST;
    hdr_full.teid           = 1;
    hdr_full.sequence       = 1;
    byte_buffer_t claims_body;
    TESTASSERT(gtpc_header_pack(hdr_full, 42, &claims_body) == SRSRAN_SUCCESS); // claims 42 body octets
    gtpc_header_t hdr_check;
    TESTASSERT(gtpc_header_unpack(claims_body, &hdr_check) == SRSRAN_ERROR); // but none were appended
  }

  // Message missing a mandatory IE (Session Start Request with no TMGI) must
  // fail unpack, not silently succeed with a zeroed struct.
  gtpc_mbms_session_start_request req = {};
  req.sender_f_teid.ipv4_present      = true;
  req.sender_f_teid.interface_type    = SM_MBMS_GW_GTP_C_INTERFACE;
  req.sender_f_teid.teid              = 1;
  req.sender_f_teid.ipv4              = 1;
  req.mbms_session_duration.duration_sec = 1;
  req.mbms_service_area.sai_codes        = {1};
  req.mbms_ip_multicast_distrib.c_teid   = 1;
  req.mbms_ip_multicast_distrib.dist_addr_ipv4   = 1;
  req.mbms_ip_multicast_distrib.source_addr_ipv4 = 1;
  // req.tmgi left default/zero -- but since gtpc_pack always packs tmgi
  // unconditionally (it's Mandatory), we instead directly build a buffer
  // with the TMGI IE stripped out to simulate a genuinely malformed peer.
  byte_buffer_t full_body;
  TESTASSERT(gtpc_pack_mbms_session_start_request(req, &full_body) == SRSRAN_SUCCESS);

  byte_buffer_t stripped;
  uint32_t      off = 0;
  while (off + 4 <= full_body.N_bytes) {
    gtpc_ie_header_t ie_hdr;
    TESTASSERT(gtpc_ie_header_unpack(&full_body.msg[off], full_body.N_bytes - off, &ie_hdr) == SRSRAN_SUCCESS);
    if (ie_hdr.type != GTPC_IE_TYPE_TMGI) {
      stripped.append_bytes(&full_body.msg[off], 4 + ie_hdr.length);
    }
    off += 4 + ie_hdr.length;
  }
  gtpc_mbms_session_start_request req2;
  TESTASSERT(gtpc_unpack_mbms_session_start_request(stripped.msg, stripped.N_bytes, &req2) == SRSRAN_ERROR);

  return SRSRAN_SUCCESS;
}

int main()
{
  TESTASSERT(test_header_round_trip() == SRSRAN_SUCCESS);
  TESTASSERT(test_tmgi_round_trip() == SRSRAN_SUCCESS);
  TESTASSERT(test_session_duration_round_trip() == SRSRAN_SUCCESS);
  TESTASSERT(test_service_area_round_trip() == SRSRAN_SUCCESS);
  TESTASSERT(test_bearer_qos_round_trip() == SRSRAN_SUCCESS);
  TESTASSERT(test_ecgi_list_round_trip() == SRSRAN_SUCCESS);
  TESTASSERT(test_time_to_data_transfer_round_trip() == SRSRAN_SUCCESS);
  TESTASSERT(test_opaque_round_trip() == SRSRAN_SUCCESS);
  TESTASSERT(test_session_start_request_round_trip() == SRSRAN_SUCCESS);
  TESTASSERT(test_session_stop_request_round_trip() == SRSRAN_SUCCESS);
  TESTASSERT(test_malformed_input_rejected() == SRSRAN_SUCCESS);

  printf("Success\n");
  return 0;
}
