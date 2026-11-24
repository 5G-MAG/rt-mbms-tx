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

// Round-trip pack/unpack tests for the hand-ported SBc-AP codec (CBC <-> MME).
// No captured reference wire traces exist yet for this brand-new codec (unlike
// s1ap_test.cc/ngap_test.cc, which check against real captured PCAP bytes) --
// these tests instead construct each message programmatically, pack it,
// unpack into a fresh instance, and assert every field round-trips correctly,
// plus test_pack_unpack_consistency() for a byte-level pack/unpack/re-pack
// check. Cross-check candidate for later: encode the same logical alert with
// the portal's independent JS encoder (see the implementation plan) and diff
// raw bytes against this codec's output.

#include "srsran/asn1/sbc_ap.h"
#include "srsran/common/test_common.h"

using namespace asn1;
using namespace asn1::sbc_ap;

int test_write_replace_warning_request()
{
  write_replace_warning_request_s req;
  req.protocol_ies.msg_id.value.from_number(0x1102);
  req.protocol_ies.serial_num.value.from_number(0x3000);
  req.protocol_ies.repeat_period.value                = 512;
  req.protocol_ies.nof_broadcasts_requested.value      = 10;
  req.protocol_ies.warning_type_present                = true;
  req.protocol_ies.warning_type.value.from_number(0x0000);
  req.protocol_ies.data_coding_scheme_present          = true;
  req.protocol_ies.data_coding_scheme.value.from_number(0x48);
  req.protocol_ies.warning_msg_content_present         = true;
  req.protocol_ies.warning_msg_content.value.from_string("C576597E2EBBC77950905D96D301");
  // Deliberately left absent to exercise the optional-skip path:
  TESTASSERT(not req.protocol_ies.warning_security_info_present);
  TESTASSERT(not req.protocol_ies.warning_area_coordinates_present);

  uint8_t       buf[2048];
  bit_ref       bref(buf, sizeof(buf));
  TESTASSERT(req.pack(bref) == SRSASN_SUCCESS);

  cbit_ref                        bref2(buf, sizeof(buf));
  write_replace_warning_request_s req2;
  TESTASSERT(req2.unpack(bref2) == SRSASN_SUCCESS);

  TESTASSERT(req2.protocol_ies.msg_id.value.to_number() == 0x1102);
  TESTASSERT(req2.protocol_ies.serial_num.value.to_number() == 0x3000);
  TESTASSERT((uint16_t)req2.protocol_ies.repeat_period.value == 512);
  TESTASSERT((uint32_t)req2.protocol_ies.nof_broadcasts_requested.value == 10);
  TESTASSERT(req2.protocol_ies.warning_type_present);
  TESTASSERT(req2.protocol_ies.warning_type.value.to_number() == 0x0000);
  TESTASSERT(req2.protocol_ies.data_coding_scheme_present);
  TESTASSERT(req2.protocol_ies.data_coding_scheme.value.to_number() == 0x48);
  TESTASSERT(req2.protocol_ies.warning_msg_content_present);
  TESTASSERT(req2.protocol_ies.warning_msg_content.value.to_string() == "c576597e2ebbc77950905d96d301");
  TESTASSERT(not req2.protocol_ies.warning_security_info_present);
  TESTASSERT(not req2.protocol_ies.warning_area_coordinates_present);

  TESTASSERT(test_pack_unpack_consistency(req) == SRSASN_SUCCESS);

  return SRSRAN_SUCCESS;
}

int test_write_replace_warning_request_boundary()
{
  // Repetition-Period upper bound is 4096 for SBc-AP -- deliberately different from
  // S1AP's own WriteReplaceWarningRequest, whose repeat_period tops out at 4095. See
  // sbc_ap.h's file header for why this isn't "fixed" to match S1AP.
  write_replace_warning_request_s req;
  req.protocol_ies.msg_id.value.from_number(0x1104);
  req.protocol_ies.serial_num.value.from_number(0x0001);
  req.protocol_ies.repeat_period.value           = 4096;
  req.protocol_ies.nof_broadcasts_requested.value = 65535;

  TESTASSERT(test_pack_unpack_consistency(req) == SRSASN_SUCCESS);

  return SRSRAN_SUCCESS;
}

int test_write_replace_warning_resp()
{
  write_replace_warning_resp_s resp;
  resp.protocol_ies.msg_id.value.from_number(0x1102);
  resp.protocol_ies.serial_num.value.from_number(0x3000);
  resp.protocol_ies.cause.value = 0; // message-accepted

  uint8_t buf[256];
  bit_ref bref(buf, sizeof(buf));
  TESTASSERT(resp.pack(bref) == SRSASN_SUCCESS);

  cbit_ref                     bref2(buf, sizeof(buf));
  write_replace_warning_resp_s resp2;
  TESTASSERT(resp2.unpack(bref2) == SRSASN_SUCCESS);

  TESTASSERT(resp2.protocol_ies.msg_id.value.to_number() == 0x1102);
  TESTASSERT(resp2.protocol_ies.serial_num.value.to_number() == 0x3000);
  TESTASSERT((uint16_t)resp2.protocol_ies.cause.value == 0);

  TESTASSERT(test_pack_unpack_consistency(resp) == SRSASN_SUCCESS);

  return SRSRAN_SUCCESS;
}

int test_stop_warning_request()
{
  stop_warning_request_s req;
  req.protocol_ies.msg_id.value.from_number(0x1102);
  req.protocol_ies.serial_num.value.from_number(0x3000);

  uint8_t buf[256];
  bit_ref bref(buf, sizeof(buf));
  TESTASSERT(req.pack(bref) == SRSASN_SUCCESS);

  cbit_ref                bref2(buf, sizeof(buf));
  stop_warning_request_s req2;
  TESTASSERT(req2.unpack(bref2) == SRSASN_SUCCESS);

  TESTASSERT(req2.protocol_ies.msg_id.value.to_number() == 0x1102);
  TESTASSERT(req2.protocol_ies.serial_num.value.to_number() == 0x3000);

  TESTASSERT(test_pack_unpack_consistency(req) == SRSASN_SUCCESS);

  return SRSRAN_SUCCESS;
}

int test_stop_warning_resp()
{
  stop_warning_resp_s resp;
  resp.protocol_ies.msg_id.value.from_number(0x1102);
  resp.protocol_ies.serial_num.value.from_number(0x3000);
  resp.protocol_ies.cause.value = 0;

  uint8_t buf[256];
  bit_ref bref(buf, sizeof(buf));
  TESTASSERT(resp.pack(bref) == SRSASN_SUCCESS);

  cbit_ref             bref2(buf, sizeof(buf));
  stop_warning_resp_s resp2;
  TESTASSERT(resp2.unpack(bref2) == SRSASN_SUCCESS);

  TESTASSERT(resp2.protocol_ies.msg_id.value.to_number() == 0x1102);
  TESTASSERT(resp2.protocol_ies.serial_num.value.to_number() == 0x3000);
  TESTASSERT((uint16_t)resp2.protocol_ies.cause.value == 0);

  TESTASSERT(test_pack_unpack_consistency(resp) == SRSASN_SUCCESS);

  return SRSRAN_SUCCESS;
}

int test_sbc_ap_pdu_wrapper()
{
  sbc_ap_pdu_c tx_pdu;
  sbc_ap_init_msg_s& init_msg = tx_pdu.set_init_msg();
  TESTASSERT(init_msg.load_info_obj(0)); // id-Write-Replace-Warning
  write_replace_warning_request_s& req = init_msg.value.write_replace_warning_request();
  req.protocol_ies.msg_id.value.from_number(0x1102);
  req.protocol_ies.serial_num.value.from_number(0x3000);
  req.protocol_ies.repeat_period.value            = 512;
  req.protocol_ies.nof_broadcasts_requested.value = 10;

  uint8_t buf[2048];
  bit_ref bref(buf, sizeof(buf));
  TESTASSERT(tx_pdu.pack(bref) == SRSASN_SUCCESS);

  cbit_ref     bref2(buf, sizeof(buf));
  sbc_ap_pdu_c rx_pdu;
  TESTASSERT(rx_pdu.unpack(bref2) == SRSASN_SUCCESS);
  TESTASSERT(rx_pdu.type().value == sbc_ap_pdu_c::types_opts::init_msg);
  TESTASSERT(rx_pdu.init_msg().proc_code == 0);
  TESTASSERT(rx_pdu.init_msg().crit.value == crit_opts::reject);
  TESTASSERT(rx_pdu.init_msg().value.type().value ==
             sbc_ap_elem_procs_o::init_msg_c::types_opts::write_replace_warning_request);
  write_replace_warning_request_s& rx_req = rx_pdu.init_msg().value.write_replace_warning_request();
  TESTASSERT(rx_req.protocol_ies.msg_id.value.to_number() == 0x1102);
  TESTASSERT(rx_req.protocol_ies.serial_num.value.to_number() == 0x3000);
  TESTASSERT((uint16_t)rx_req.protocol_ies.repeat_period.value == 512);

  TESTASSERT(test_pack_unpack_consistency(tx_pdu) == SRSASN_SUCCESS);

  // Same wrapper, Stop-Warning-Request (proc code 1) this time.
  sbc_ap_pdu_c tx_pdu2;
  sbc_ap_init_msg_s& init_msg2 = tx_pdu2.set_init_msg();
  TESTASSERT(init_msg2.load_info_obj(1)); // id-Stop-Warning
  stop_warning_request_s& stop_req = init_msg2.value.stop_warning_request();
  stop_req.protocol_ies.msg_id.value.from_number(0x1102);
  stop_req.protocol_ies.serial_num.value.from_number(0x3000);

  TESTASSERT(test_pack_unpack_consistency(tx_pdu2) == SRSASN_SUCCESS);

  // SuccessfulOutcome wrapper too.
  sbc_ap_pdu_c tx_resp_pdu;
  sbc_ap_successful_outcome_s& outcome = tx_resp_pdu.set_successful_outcome();
  TESTASSERT(outcome.load_info_obj(0)); // id-Write-Replace-Warning
  write_replace_warning_resp_s& resp = outcome.value.write_replace_warning_resp();
  resp.protocol_ies.msg_id.value.from_number(0x1102);
  resp.protocol_ies.serial_num.value.from_number(0x3000);
  resp.protocol_ies.cause.value = 0;

  TESTASSERT(test_pack_unpack_consistency(tx_resp_pdu) == SRSASN_SUCCESS);

  return SRSRAN_SUCCESS;
}

int main()
{
  // Setup the log spy to intercept error and warning log entries.
  if (!srslog::install_custom_sink(
          srsran::log_sink_spy::name(),
          std::unique_ptr<srsran::log_sink_spy>(new srsran::log_sink_spy(srslog::get_default_log_formatter())))) {
    return SRSRAN_ERROR;
  }

  auto* spy = static_cast<srsran::log_sink_spy*>(srslog::find_sink(srsran::log_sink_spy::name()));
  if (!spy) {
    return SRSRAN_ERROR;
  }

  auto& asn1_logger = srslog::fetch_basic_logger("ASN1", *spy, false);
  asn1_logger.set_level(srslog::basic_levels::debug);
  asn1_logger.set_hex_dump_max_size(-1);

  srslog::init();

  TESTASSERT(test_write_replace_warning_request() == 0);
  TESTASSERT(test_write_replace_warning_request_boundary() == 0);
  TESTASSERT(test_write_replace_warning_resp() == 0);
  TESTASSERT(test_stop_warning_request() == 0);
  TESTASSERT(test_stop_warning_resp() == 0);
  TESTASSERT(test_sbc_ap_pdu_wrapper() == 0);

  srslog::flush();

  printf("Success\n");
  return 0;
}
