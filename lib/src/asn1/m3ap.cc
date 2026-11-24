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

#include "srsran/asn1/m3ap.h"

using namespace asn1;
using namespace asn1::m3ap;

/*******************************************************************************
 *                        ProtocolIE-Field container helpers
 * Real M3AP ProtocolIE-Field encoding: id (INTEGER 0..65535) + criticality
 * (ENUMERATED) + value (open type: octet-aligned length-prefixed blob, via
 * varlength_field_pack_guard/unpack_guard). Each message below hand-writes
 * its own fixed IE list against these helpers instead of a generic
 * information-object-set container (see m3ap.h top comment for why).
 ******************************************************************************/
namespace {
SRSASN_CODE pack_ie_hdr(bit_ref& bref, uint32_t id, crit_e crit)
{
  HANDLE_CODE(pack_integer(bref, id, (uint32_t)0u, (uint32_t)65535u, false, true));
  HANDLE_CODE(crit.pack(bref));
  return SRSASN_SUCCESS;
}
SRSASN_CODE unpack_ie_hdr(uint32_t& id, crit_e& crit, cbit_ref& bref)
{
  HANDLE_CODE(unpack_integer(id, bref, (uint32_t)0u, (uint32_t)65535u, false, true));
  HANDLE_CODE(crit.unpack(bref));
  return SRSASN_SUCCESS;
}
} // namespace

/*******************************************************************************
 *                                Common enums
 ******************************************************************************/

const char* crit_opts::to_string() const
{
  static const char* options[] = {"reject", "ignore", "notify"};
  return convert_enum_idx(options, 3, value, "crit_e");
}

const char* trigger_msg_opts::to_string() const
{
  static const char* options[] = {"initiating-message", "successful-outcome", "unsuccessful-outcome"};
  return convert_enum_idx(options, 3, value, "trigger_msg_e");
}

const char* msg_type_opts::to_string() const
{
  static const char* options[] = {"initiatingMessage", "successfulOutcome", "unsuccessfulOutcome"};
  return convert_enum_idx(options, 3, value, "msg_type_e");
}

/*******************************************************************************
 *                                  IE Types
 ******************************************************************************/

// TMGI ::= SEQUENCE { pLMNidentity, serviceID, iE-Extensions OPTIONAL } -- not extensible.
SRSASN_CODE tmgi_s::pack(bit_ref& bref) const
{
  HANDLE_CODE(bref.pack(0, 1)); // iE-Extensions present? always false, see m3ap.h
  HANDLE_CODE(plmn_id.pack(bref));
  HANDLE_CODE(service_id.pack(bref));
  return SRSASN_SUCCESS;
}
SRSASN_CODE tmgi_s::unpack(cbit_ref& bref)
{
  bool ext_present;
  HANDLE_CODE(bref.unpack(ext_present, 1));
  HANDLE_CODE(plmn_id.unpack(bref));
  HANDLE_CODE(service_id.unpack(bref));
  return SRSASN_SUCCESS;
}

// GBR-QosInformation ::= SEQUENCE { mBMS-E-RAB-MaximumBitrateDL, mBMS-E-RAB-GuaranteedBitrateDL, iE-Extensions OPT, ... }
SRSASN_CODE gbr_qos_info_s::pack(bit_ref& bref) const
{
  HANDLE_CODE(bref.pack(0, 1)); // extension bit
  HANDLE_CODE(bref.pack(0, 1)); // iE-Extensions present?
  HANDLE_CODE(pack_integer(bref, max_bitrate_dl, (uint64_t)0u, (uint64_t)10000000000u, false, true));
  HANDLE_CODE(pack_integer(bref, guaranteed_bitrate_dl, (uint64_t)0u, (uint64_t)10000000000u, false, true));
  return SRSASN_SUCCESS;
}
SRSASN_CODE gbr_qos_info_s::unpack(cbit_ref& bref)
{
  bool ext, ext_ies_present;
  HANDLE_CODE(bref.unpack(ext, 1));
  HANDLE_CODE(bref.unpack(ext_ies_present, 1));
  HANDLE_CODE(unpack_integer(max_bitrate_dl, bref, (uint64_t)0u, (uint64_t)10000000000u, false, true));
  HANDLE_CODE(unpack_integer(guaranteed_bitrate_dl, bref, (uint64_t)0u, (uint64_t)10000000000u, false, true));
  return SRSASN_SUCCESS;
}

// MBMS-E-RAB-QoS-Parameters ::= SEQUENCE { qCI, gbrQosInformation OPT, iE-Extensions OPT, ... }
SRSASN_CODE mbms_e_rab_qos_params_s::pack(bit_ref& bref) const
{
  HANDLE_CODE(bref.pack(0, 1));                       // extension bit
  HANDLE_CODE(bref.pack(gbr_qos_info_present, 1));    // gbrQosInformation present?
  HANDLE_CODE(bref.pack(0, 1));                       // iE-Extensions present?
  HANDLE_CODE(pack_integer(bref, qci, (uint16_t)0u, (uint16_t)255u, false, true));
  if (gbr_qos_info_present) {
    HANDLE_CODE(gbr_qos_info.pack(bref));
  }
  return SRSASN_SUCCESS;
}
SRSASN_CODE mbms_e_rab_qos_params_s::unpack(cbit_ref& bref)
{
  bool ext, ext_ies_present;
  HANDLE_CODE(bref.unpack(ext, 1));
  HANDLE_CODE(bref.unpack(gbr_qos_info_present, 1));
  HANDLE_CODE(bref.unpack(ext_ies_present, 1));
  HANDLE_CODE(unpack_integer(qci, bref, (uint16_t)0u, (uint16_t)255u, false, true));
  if (gbr_qos_info_present) {
    HANDLE_CODE(gbr_qos_info.unpack(bref));
  }
  return SRSASN_SUCCESS;
}

// TNL-Information ::= SEQUENCE { iPMCAddress, iPSourceAddress, gTP-DLTEID, iE-Extensions OPT, ... }
SRSASN_CODE tnl_info_s::pack(bit_ref& bref) const
{
  HANDLE_CODE(bref.pack(0, 1)); // extension bit
  HANDLE_CODE(bref.pack(0, 1)); // iE-Extensions present?
  HANDLE_CODE(ip_mc_addr.pack(bref));
  HANDLE_CODE(ip_src_addr.pack(bref));
  HANDLE_CODE(gtp_dl_teid.pack(bref));
  return SRSASN_SUCCESS;
}
SRSASN_CODE tnl_info_s::unpack(cbit_ref& bref)
{
  bool ext, ext_ies_present;
  HANDLE_CODE(bref.unpack(ext, 1));
  HANDLE_CODE(bref.unpack(ext_ies_present, 1));
  HANDLE_CODE(ip_mc_addr.unpack(bref));
  HANDLE_CODE(ip_src_addr.unpack(bref));
  HANDLE_CODE(gtp_dl_teid.unpack(bref));
  return SRSASN_SUCCESS;
}

// Global-MCE-ID ::= SEQUENCE { pLMN-Identity, mCE-ID, extendedMCE-ID OPT, iE-Extensions OPT, ... }
SRSASN_CODE global_mce_id_s::pack(bit_ref& bref) const
{
  HANDLE_CODE(bref.pack(0, 1));                    // extension bit
  HANDLE_CODE(bref.pack(ext_mce_id_present, 1));   // extendedMCE-ID present?
  HANDLE_CODE(bref.pack(0, 1));                    // iE-Extensions present?
  HANDLE_CODE(plmn_id.pack(bref));
  HANDLE_CODE(mce_id.pack(bref));
  if (ext_mce_id_present) {
    HANDLE_CODE(ext_mce_id.pack(bref));
  }
  return SRSASN_SUCCESS;
}
SRSASN_CODE global_mce_id_s::unpack(cbit_ref& bref)
{
  bool ext, ext_ies_present;
  HANDLE_CODE(bref.unpack(ext, 1));
  HANDLE_CODE(bref.unpack(ext_mce_id_present, 1));
  HANDLE_CODE(bref.unpack(ext_ies_present, 1));
  HANDLE_CODE(plmn_id.unpack(bref));
  HANDLE_CODE(mce_id.unpack(bref));
  if (ext_mce_id_present) {
    HANDLE_CODE(ext_mce_id.unpack(bref));
  }
  return SRSASN_SUCCESS;
}

/*******************************************************************************
 *                                   Cause
 ******************************************************************************/

const char* cause_radio_network_opts::to_string() const
{
  static const char* options[] = {"unknown-or-already-allocated-MME-MBMS-M3AP-ID",
                                   "unknown-or-already-allocated-MCE-MBMS-M3AP-ID",
                                   "unknown-or-inconsistent-pair-of-MBMS-M3AP-IDs",
                                   "radio-resources-not-available",
                                   "invalid-QoS-combination",
                                   "interaction-with-other-procedure",
                                   "not-supported-QCI-value",
                                   "unspecified",
                                   "uninvolved-MCE"};
  return convert_enum_idx(options, 9, value, "cause_radio_network_e");
}
const char* cause_transport_opts::to_string() const
{
  static const char* options[] = {"transport-resource-unavailable", "unspecified"};
  return convert_enum_idx(options, 2, value, "cause_transport_e");
}
const char* cause_nas_opts::to_string() const
{
  static const char* options[] = {"unspecified"};
  return convert_enum_idx(options, 1, value, "cause_nas_e");
}
const char* cause_protocol_opts::to_string() const
{
  static const char* options[] = {"transfer-syntax-error",
                                   "abstract-syntax-error-reject",
                                   "abstract-syntax-error-ignore-and-notify",
                                   "message-not-compatible-with-receiver-state",
                                   "semantic-error",
                                   "abstract-syntax-error-falsely-constructed-message",
                                   "unspecified"};
  return convert_enum_idx(options, 7, value, "cause_protocol_e");
}
const char* cause_misc_opts::to_string() const
{
  static const char* options[] = {"control-processing-overload",
                                   "not-enough-user-plane-processing-resources",
                                   "hardware-failure",
                                   "om-intervention",
                                   "unspecified"};
  return convert_enum_idx(options, 5, value, "cause_misc_e");
}

const char* cause_c::types_opts::to_string() const
{
  static const char* options[] = {"radioNetwork", "transport", "nAS", "protocol", "misc"};
  return convert_enum_idx(options, 5, value, "cause_c::types");
}

void cause_c::set_radio_network(cause_radio_network_e::options v)
{
  type_ = types_opts::radio_network;
  val_  = (uint8_t)v;
}
void cause_c::set_transport(cause_transport_e::options v)
{
  type_ = types_opts::transport;
  val_  = (uint8_t)v;
}
void cause_c::set_nas(cause_nas_e::options v)
{
  type_ = types_opts::nas;
  val_  = (uint8_t)v;
}
void cause_c::set_protocol(cause_protocol_e::options v)
{
  type_ = types_opts::protocol;
  val_  = (uint8_t)v;
}
void cause_c::set_misc(cause_misc_e::options v)
{
  type_ = types_opts::misc;
  val_  = (uint8_t)v;
}
const char* cause_c::to_string() const
{
  switch ((types_opts::options)type_) {
    case types_opts::radio_network:
      return cause_radio_network_e((cause_radio_network_opts::options)val_).to_string();
    case types_opts::transport:
      return cause_transport_e((cause_transport_opts::options)val_).to_string();
    case types_opts::nas:
      return cause_nas_e((cause_nas_opts::options)val_).to_string();
    case types_opts::protocol:
      return cause_protocol_e((cause_protocol_opts::options)val_).to_string();
    case types_opts::misc:
      return cause_misc_e((cause_misc_opts::options)val_).to_string();
    default:
      return "unknown";
  }
}
SRSASN_CODE cause_c::pack(bit_ref& bref) const
{
  HANDLE_CODE(type_.pack(bref));
  switch ((types_opts::options)type_) {
    case types_opts::radio_network:
      HANDLE_CODE(cause_radio_network_e((cause_radio_network_opts::options)val_).pack(bref));
      break;
    case types_opts::transport:
      HANDLE_CODE(cause_transport_e((cause_transport_opts::options)val_).pack(bref));
      break;
    case types_opts::nas:
      HANDLE_CODE(cause_nas_e((cause_nas_opts::options)val_).pack(bref));
      break;
    case types_opts::protocol:
      HANDLE_CODE(cause_protocol_e((cause_protocol_opts::options)val_).pack(bref));
      break;
    case types_opts::misc:
      HANDLE_CODE(cause_misc_e((cause_misc_opts::options)val_).pack(bref));
      break;
    default:
      return SRSASN_ERROR_ENCODE_FAIL;
  }
  return SRSASN_SUCCESS;
}
SRSASN_CODE cause_c::unpack(cbit_ref& bref)
{
  HANDLE_CODE(type_.unpack(bref));
  switch ((types_opts::options)type_) {
    case types_opts::radio_network: {
      cause_radio_network_e e;
      HANDLE_CODE(e.unpack(bref));
      val_ = (uint8_t)(cause_radio_network_opts::options)e;
      break;
    }
    case types_opts::transport: {
      cause_transport_e e;
      HANDLE_CODE(e.unpack(bref));
      val_ = (uint8_t)(cause_transport_opts::options)e;
      break;
    }
    case types_opts::nas: {
      cause_nas_e e;
      HANDLE_CODE(e.unpack(bref));
      val_ = (uint8_t)(cause_nas_opts::options)e;
      break;
    }
    case types_opts::protocol: {
      cause_protocol_e e;
      HANDLE_CODE(e.unpack(bref));
      val_ = (uint8_t)(cause_protocol_opts::options)e;
      break;
    }
    case types_opts::misc: {
      cause_misc_e e;
      HANDLE_CODE(e.unpack(bref));
      val_ = (uint8_t)(cause_misc_opts::options)e;
      break;
    }
    default:
      return SRSASN_ERROR_DECODE_FAIL;
  }
  return SRSASN_SUCCESS;
}

const char* time_to_wait_opts::to_string() const
{
  static const char* options[] = {"v1s", "v2s", "v5s", "v10s", "v20s", "v60s"};
  return convert_enum_idx(options, 6, value, "time_to_wait_e");
}

/*******************************************************************************
 *                              M3 Setup messages
 ******************************************************************************/

SRSASN_CODE m3_setup_request_s::pack(bit_ref& bref) const
{
  HANDLE_CODE(bref.pack(0, 1)); // message-level extension bit
  uint32_t count = 2 + (mce_name_present ? 1 : 0);
  HANDLE_CODE(pack_length(bref, count, 0u, 65535u, true));

  HANDLE_CODE(pack_ie_hdr(bref, id_Global_MCE_ID, crit_opts::reject));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(global_mce_id.pack(bref));
  }
  if (mce_name_present) {
    HANDLE_CODE(pack_ie_hdr(bref, id_MCEname, crit_opts::ignore));
    {
      varlength_field_pack_guard g(bref, true);
      HANDLE_CODE(mce_name.pack(bref));
    }
  }
  HANDLE_CODE(pack_ie_hdr(bref, id_MBMSServiceAreaList, crit_opts::reject));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(mbms_service_area_list.pack(bref));
  }
  return SRSASN_SUCCESS;
}
SRSASN_CODE m3_setup_request_s::unpack(cbit_ref& bref)
{
  bool ext;
  HANDLE_CODE(bref.unpack(ext, 1));
  uint32_t count;
  HANDLE_CODE(unpack_length(count, bref, 0u, 65535u, true));
  bool have_global_mce_id = false, have_service_area_list = false;
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t id;
    crit_e   crit;
    HANDLE_CODE(unpack_ie_hdr(id, crit, bref));
    varlength_field_unpack_guard g(bref, true);
    switch (id) {
      case id_Global_MCE_ID:
        HANDLE_CODE(global_mce_id.unpack(bref));
        have_global_mce_id = true;
        break;
      case id_MCEname:
        HANDLE_CODE(mce_name.unpack(bref));
        mce_name_present = true;
        break;
      case id_MBMSServiceAreaList:
        HANDLE_CODE(mbms_service_area_list.unpack(bref));
        have_service_area_list = true;
        break;
      default:
        break; // unknown/deferred IE, skipped by the guard above
    }
  }
  return (have_global_mce_id && have_service_area_list) ? SRSASN_SUCCESS : SRSASN_ERROR_DECODE_FAIL;
}

SRSASN_CODE m3_setup_resp_s::pack(bit_ref& bref) const
{
  HANDLE_CODE(bref.pack(0, 1)); // message-level extension bit
  HANDLE_CODE(pack_length(bref, (uint32_t)0u, 0u, 65535u, true)); // CriticalityDiagnostics never generated
  return SRSASN_SUCCESS;
}
SRSASN_CODE m3_setup_resp_s::unpack(cbit_ref& bref)
{
  bool ext;
  HANDLE_CODE(bref.unpack(ext, 1));
  uint32_t count;
  HANDLE_CODE(unpack_length(count, bref, 0u, 65535u, true));
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t id;
    crit_e   crit;
    HANDLE_CODE(unpack_ie_hdr(id, crit, bref));
    varlength_field_unpack_guard g(bref, true); // no IEs consumed in this pass, all skipped
  }
  return SRSASN_SUCCESS;
}

SRSASN_CODE m3_setup_fail_s::pack(bit_ref& bref) const
{
  HANDLE_CODE(bref.pack(0, 1));
  uint32_t count = 1 + (time_to_wait_present ? 1 : 0);
  HANDLE_CODE(pack_length(bref, count, 0u, 65535u, true));

  HANDLE_CODE(pack_ie_hdr(bref, id_Cause, crit_opts::ignore));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(cause.pack(bref));
  }
  if (time_to_wait_present) {
    HANDLE_CODE(pack_ie_hdr(bref, id_TimeToWait, crit_opts::ignore));
    {
      varlength_field_pack_guard g(bref, true);
      HANDLE_CODE(time_to_wait.pack(bref));
    }
  }
  return SRSASN_SUCCESS;
}
SRSASN_CODE m3_setup_fail_s::unpack(cbit_ref& bref)
{
  bool ext;
  HANDLE_CODE(bref.unpack(ext, 1));
  uint32_t count;
  HANDLE_CODE(unpack_length(count, bref, 0u, 65535u, true));
  bool have_cause = false;
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t id;
    crit_e   crit;
    HANDLE_CODE(unpack_ie_hdr(id, crit, bref));
    varlength_field_unpack_guard g(bref, true);
    switch (id) {
      case id_Cause:
        HANDLE_CODE(cause.unpack(bref));
        have_cause = true;
        break;
      case id_TimeToWait:
        HANDLE_CODE(time_to_wait.unpack(bref));
        time_to_wait_present = true;
        break;
      default:
        break;
    }
  }
  return have_cause ? SRSASN_SUCCESS : SRSASN_ERROR_DECODE_FAIL;
}

/*******************************************************************************
 *                        MBMS Session Start messages
 ******************************************************************************/

SRSASN_CODE mbms_session_start_request_s::pack(bit_ref& bref) const
{
  HANDLE_CODE(bref.pack(0, 1));
  uint32_t count = 7 + (mbms_session_id_present ? 1 : 0);
  HANDLE_CODE(pack_length(bref, count, 0u, 65535u, true));

  HANDLE_CODE(pack_ie_hdr(bref, id_MME_MBMS_M3AP_ID, crit_opts::reject));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(pack_integer(bref, mme_mbms_m3ap_id, (uint32_t)0u, (uint32_t)65535u, false, true));
  }
  HANDLE_CODE(pack_ie_hdr(bref, id_TMGI, crit_opts::reject));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(tmgi.pack(bref));
  }
  if (mbms_session_id_present) {
    HANDLE_CODE(pack_ie_hdr(bref, id_MBMS_Session_ID, crit_opts::ignore));
    {
      varlength_field_pack_guard g(bref, true);
      HANDLE_CODE(mbms_session_id.pack(bref));
    }
  }
  HANDLE_CODE(pack_ie_hdr(bref, id_MBMS_E_RAB_QoS_Parameters, crit_opts::reject));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(mbms_e_rab_qos_params.pack(bref));
  }
  HANDLE_CODE(pack_ie_hdr(bref, id_MBMS_Session_Duration, crit_opts::reject));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(mbms_session_duration.pack(bref));
  }
  HANDLE_CODE(pack_ie_hdr(bref, id_MBMS_Service_Area, crit_opts::reject));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(mbms_service_area.pack(bref));
  }
  HANDLE_CODE(pack_ie_hdr(bref, id_MinimumTimeToMBMSDataTransfer, crit_opts::reject));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(min_time_to_mbms_data_transfer.pack(bref));
  }
  HANDLE_CODE(pack_ie_hdr(bref, id_TNL_Information, crit_opts::reject));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(tnl_info.pack(bref));
  }
  return SRSASN_SUCCESS;
}
SRSASN_CODE mbms_session_start_request_s::unpack(cbit_ref& bref)
{
  bool ext;
  HANDLE_CODE(bref.unpack(ext, 1));
  uint32_t count;
  HANDLE_CODE(unpack_length(count, bref, 0u, 65535u, true));
  bool have_mme_id = false, have_tmgi = false, have_qos = false, have_dur = false, have_area = false,
       have_min_time = false, have_tnl = false;
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t id;
    crit_e   crit;
    HANDLE_CODE(unpack_ie_hdr(id, crit, bref));
    varlength_field_unpack_guard g(bref, true);
    switch (id) {
      case id_MME_MBMS_M3AP_ID:
        HANDLE_CODE(unpack_integer(mme_mbms_m3ap_id, bref, (uint32_t)0u, (uint32_t)65535u, false, true));
        have_mme_id = true;
        break;
      case id_TMGI:
        HANDLE_CODE(tmgi.unpack(bref));
        have_tmgi = true;
        break;
      case id_MBMS_Session_ID:
        HANDLE_CODE(mbms_session_id.unpack(bref));
        mbms_session_id_present = true;
        break;
      case id_MBMS_E_RAB_QoS_Parameters:
        HANDLE_CODE(mbms_e_rab_qos_params.unpack(bref));
        have_qos = true;
        break;
      case id_MBMS_Session_Duration:
        HANDLE_CODE(mbms_session_duration.unpack(bref));
        have_dur = true;
        break;
      case id_MBMS_Service_Area:
        HANDLE_CODE(mbms_service_area.unpack(bref));
        have_area = true;
        break;
      case id_MinimumTimeToMBMSDataTransfer:
        HANDLE_CODE(min_time_to_mbms_data_transfer.unpack(bref));
        have_min_time = true;
        break;
      case id_TNL_Information:
        HANDLE_CODE(tnl_info.unpack(bref));
        have_tnl = true;
        break;
      default:
        break;
    }
  }
  return (have_mme_id && have_tmgi && have_qos && have_dur && have_area && have_min_time && have_tnl)
             ? SRSASN_SUCCESS
             : SRSASN_ERROR_DECODE_FAIL;
}

SRSASN_CODE mbms_session_start_resp_s::pack(bit_ref& bref) const
{
  HANDLE_CODE(bref.pack(0, 1));
  HANDLE_CODE(pack_length(bref, (uint32_t)2u, 0u, 65535u, true));
  HANDLE_CODE(pack_ie_hdr(bref, id_MME_MBMS_M3AP_ID, crit_opts::ignore));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(pack_integer(bref, mme_mbms_m3ap_id, (uint32_t)0u, (uint32_t)65535u, false, true));
  }
  HANDLE_CODE(pack_ie_hdr(bref, id_MCE_MBMS_M3AP_ID, crit_opts::ignore));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(pack_integer(bref, mce_mbms_m3ap_id, (uint32_t)0u, (uint32_t)65535u, false, true));
  }
  return SRSASN_SUCCESS;
}
SRSASN_CODE mbms_session_start_resp_s::unpack(cbit_ref& bref)
{
  bool ext;
  HANDLE_CODE(bref.unpack(ext, 1));
  uint32_t count;
  HANDLE_CODE(unpack_length(count, bref, 0u, 65535u, true));
  bool have_mme_id = false, have_mce_id = false;
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t id;
    crit_e   crit;
    HANDLE_CODE(unpack_ie_hdr(id, crit, bref));
    varlength_field_unpack_guard g(bref, true);
    switch (id) {
      case id_MME_MBMS_M3AP_ID:
        HANDLE_CODE(unpack_integer(mme_mbms_m3ap_id, bref, (uint32_t)0u, (uint32_t)65535u, false, true));
        have_mme_id = true;
        break;
      case id_MCE_MBMS_M3AP_ID:
        HANDLE_CODE(unpack_integer(mce_mbms_m3ap_id, bref, (uint32_t)0u, (uint32_t)65535u, false, true));
        have_mce_id = true;
        break;
      default:
        break;
    }
  }
  return (have_mme_id && have_mce_id) ? SRSASN_SUCCESS : SRSASN_ERROR_DECODE_FAIL;
}

SRSASN_CODE mbms_session_start_fail_s::pack(bit_ref& bref) const
{
  HANDLE_CODE(bref.pack(0, 1));
  HANDLE_CODE(pack_length(bref, (uint32_t)2u, 0u, 65535u, true));
  HANDLE_CODE(pack_ie_hdr(bref, id_MME_MBMS_M3AP_ID, crit_opts::ignore));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(pack_integer(bref, mme_mbms_m3ap_id, (uint32_t)0u, (uint32_t)65535u, false, true));
  }
  HANDLE_CODE(pack_ie_hdr(bref, id_Cause, crit_opts::ignore));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(cause.pack(bref));
  }
  return SRSASN_SUCCESS;
}
SRSASN_CODE mbms_session_start_fail_s::unpack(cbit_ref& bref)
{
  bool ext;
  HANDLE_CODE(bref.unpack(ext, 1));
  uint32_t count;
  HANDLE_CODE(unpack_length(count, bref, 0u, 65535u, true));
  bool have_mme_id = false, have_cause = false;
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t id;
    crit_e   crit;
    HANDLE_CODE(unpack_ie_hdr(id, crit, bref));
    varlength_field_unpack_guard g(bref, true);
    switch (id) {
      case id_MME_MBMS_M3AP_ID:
        HANDLE_CODE(unpack_integer(mme_mbms_m3ap_id, bref, (uint32_t)0u, (uint32_t)65535u, false, true));
        have_mme_id = true;
        break;
      case id_Cause:
        HANDLE_CODE(cause.unpack(bref));
        have_cause = true;
        break;
      default:
        break;
    }
  }
  return (have_mme_id && have_cause) ? SRSASN_SUCCESS : SRSASN_ERROR_DECODE_FAIL;
}

/*******************************************************************************
 *                        MBMS Session Stop messages
 ******************************************************************************/

SRSASN_CODE mbms_session_stop_request_s::pack(bit_ref& bref) const
{
  HANDLE_CODE(bref.pack(0, 1));
  HANDLE_CODE(pack_length(bref, (uint32_t)2u, 0u, 65535u, true));
  HANDLE_CODE(pack_ie_hdr(bref, id_MME_MBMS_M3AP_ID, crit_opts::reject));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(pack_integer(bref, mme_mbms_m3ap_id, (uint32_t)0u, (uint32_t)65535u, false, true));
  }
  HANDLE_CODE(pack_ie_hdr(bref, id_MCE_MBMS_M3AP_ID, crit_opts::reject));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(pack_integer(bref, mce_mbms_m3ap_id, (uint32_t)0u, (uint32_t)65535u, false, true));
  }
  return SRSASN_SUCCESS;
}
SRSASN_CODE mbms_session_stop_request_s::unpack(cbit_ref& bref)
{
  bool ext;
  HANDLE_CODE(bref.unpack(ext, 1));
  uint32_t count;
  HANDLE_CODE(unpack_length(count, bref, 0u, 65535u, true));
  bool have_mme_id = false, have_mce_id = false;
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t id;
    crit_e   crit;
    HANDLE_CODE(unpack_ie_hdr(id, crit, bref));
    varlength_field_unpack_guard g(bref, true);
    switch (id) {
      case id_MME_MBMS_M3AP_ID:
        HANDLE_CODE(unpack_integer(mme_mbms_m3ap_id, bref, (uint32_t)0u, (uint32_t)65535u, false, true));
        have_mme_id = true;
        break;
      case id_MCE_MBMS_M3AP_ID:
        HANDLE_CODE(unpack_integer(mce_mbms_m3ap_id, bref, (uint32_t)0u, (uint32_t)65535u, false, true));
        have_mce_id = true;
        break;
      default:
        break;
    }
  }
  return (have_mme_id && have_mce_id) ? SRSASN_SUCCESS : SRSASN_ERROR_DECODE_FAIL;
}

SRSASN_CODE mbms_session_stop_resp_s::pack(bit_ref& bref) const
{
  HANDLE_CODE(bref.pack(0, 1));
  HANDLE_CODE(pack_length(bref, (uint32_t)2u, 0u, 65535u, true));
  HANDLE_CODE(pack_ie_hdr(bref, id_MME_MBMS_M3AP_ID, crit_opts::ignore));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(pack_integer(bref, mme_mbms_m3ap_id, (uint32_t)0u, (uint32_t)65535u, false, true));
  }
  HANDLE_CODE(pack_ie_hdr(bref, id_MCE_MBMS_M3AP_ID, crit_opts::ignore));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(pack_integer(bref, mce_mbms_m3ap_id, (uint32_t)0u, (uint32_t)65535u, false, true));
  }
  return SRSASN_SUCCESS;
}
SRSASN_CODE mbms_session_stop_resp_s::unpack(cbit_ref& bref)
{
  bool ext;
  HANDLE_CODE(bref.unpack(ext, 1));
  uint32_t count;
  HANDLE_CODE(unpack_length(count, bref, 0u, 65535u, true));
  bool have_mme_id = false, have_mce_id = false;
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t id;
    crit_e   crit;
    HANDLE_CODE(unpack_ie_hdr(id, crit, bref));
    varlength_field_unpack_guard g(bref, true);
    switch (id) {
      case id_MME_MBMS_M3AP_ID:
        HANDLE_CODE(unpack_integer(mme_mbms_m3ap_id, bref, (uint32_t)0u, (uint32_t)65535u, false, true));
        have_mme_id = true;
        break;
      case id_MCE_MBMS_M3AP_ID:
        HANDLE_CODE(unpack_integer(mce_mbms_m3ap_id, bref, (uint32_t)0u, (uint32_t)65535u, false, true));
        have_mce_id = true;
        break;
      default:
        break;
    }
  }
  return (have_mme_id && have_mce_id) ? SRSASN_SUCCESS : SRSASN_ERROR_DECODE_FAIL;
}

/*******************************************************************************
 *                       MBMS Session Update messages
 ******************************************************************************/

SRSASN_CODE mbms_session_update_request_s::pack(bit_ref& bref) const
{
  HANDLE_CODE(bref.pack(0, 1));
  uint32_t count = 6 + (mbms_session_id_present ? 1 : 0) + (mbms_service_area_present ? 1 : 0);
  HANDLE_CODE(pack_length(bref, count, 0u, 65535u, true));

  HANDLE_CODE(pack_ie_hdr(bref, id_MME_MBMS_M3AP_ID, crit_opts::reject));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(pack_integer(bref, mme_mbms_m3ap_id, (uint32_t)0u, (uint32_t)65535u, false, true));
  }
  HANDLE_CODE(pack_ie_hdr(bref, id_MCE_MBMS_M3AP_ID, crit_opts::reject));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(pack_integer(bref, mce_mbms_m3ap_id, (uint32_t)0u, (uint32_t)65535u, false, true));
  }
  HANDLE_CODE(pack_ie_hdr(bref, id_TMGI, crit_opts::reject));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(tmgi.pack(bref));
  }
  if (mbms_session_id_present) {
    HANDLE_CODE(pack_ie_hdr(bref, id_MBMS_Session_ID, crit_opts::ignore));
    {
      varlength_field_pack_guard g(bref, true);
      HANDLE_CODE(mbms_session_id.pack(bref));
    }
  }
  HANDLE_CODE(pack_ie_hdr(bref, id_MBMS_E_RAB_QoS_Parameters, crit_opts::reject));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(mbms_e_rab_qos_params.pack(bref));
  }
  HANDLE_CODE(pack_ie_hdr(bref, id_MBMS_Session_Duration, crit_opts::reject));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(mbms_session_duration.pack(bref));
  }
  if (mbms_service_area_present) {
    HANDLE_CODE(pack_ie_hdr(bref, id_MBMS_Service_Area, crit_opts::ignore));
    {
      varlength_field_pack_guard g(bref, true);
      HANDLE_CODE(mbms_service_area.pack(bref));
    }
  }
  HANDLE_CODE(pack_ie_hdr(bref, id_MinimumTimeToMBMSDataTransfer, crit_opts::reject));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(min_time_to_mbms_data_transfer.pack(bref));
  }
  return SRSASN_SUCCESS;
}
SRSASN_CODE mbms_session_update_request_s::unpack(cbit_ref& bref)
{
  bool ext;
  HANDLE_CODE(bref.unpack(ext, 1));
  uint32_t count;
  HANDLE_CODE(unpack_length(count, bref, 0u, 65535u, true));
  bool have_mme_id = false, have_mce_id = false, have_tmgi = false, have_qos = false, have_dur = false,
       have_min_time = false;
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t id;
    crit_e   crit;
    HANDLE_CODE(unpack_ie_hdr(id, crit, bref));
    varlength_field_unpack_guard g(bref, true);
    switch (id) {
      case id_MME_MBMS_M3AP_ID:
        HANDLE_CODE(unpack_integer(mme_mbms_m3ap_id, bref, (uint32_t)0u, (uint32_t)65535u, false, true));
        have_mme_id = true;
        break;
      case id_MCE_MBMS_M3AP_ID:
        HANDLE_CODE(unpack_integer(mce_mbms_m3ap_id, bref, (uint32_t)0u, (uint32_t)65535u, false, true));
        have_mce_id = true;
        break;
      case id_TMGI:
        HANDLE_CODE(tmgi.unpack(bref));
        have_tmgi = true;
        break;
      case id_MBMS_Session_ID:
        HANDLE_CODE(mbms_session_id.unpack(bref));
        mbms_session_id_present = true;
        break;
      case id_MBMS_E_RAB_QoS_Parameters:
        HANDLE_CODE(mbms_e_rab_qos_params.unpack(bref));
        have_qos = true;
        break;
      case id_MBMS_Session_Duration:
        HANDLE_CODE(mbms_session_duration.unpack(bref));
        have_dur = true;
        break;
      case id_MBMS_Service_Area:
        HANDLE_CODE(mbms_service_area.unpack(bref));
        mbms_service_area_present = true;
        break;
      case id_MinimumTimeToMBMSDataTransfer:
        HANDLE_CODE(min_time_to_mbms_data_transfer.unpack(bref));
        have_min_time = true;
        break;
      default:
        break;
    }
  }
  return (have_mme_id && have_mce_id && have_tmgi && have_qos && have_dur && have_min_time) ? SRSASN_SUCCESS
                                                                                             : SRSASN_ERROR_DECODE_FAIL;
}

SRSASN_CODE mbms_session_update_resp_s::pack(bit_ref& bref) const
{
  HANDLE_CODE(bref.pack(0, 1));
  HANDLE_CODE(pack_length(bref, (uint32_t)2u, 0u, 65535u, true));
  HANDLE_CODE(pack_ie_hdr(bref, id_MME_MBMS_M3AP_ID, crit_opts::ignore));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(pack_integer(bref, mme_mbms_m3ap_id, (uint32_t)0u, (uint32_t)65535u, false, true));
  }
  HANDLE_CODE(pack_ie_hdr(bref, id_MCE_MBMS_M3AP_ID, crit_opts::ignore));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(pack_integer(bref, mce_mbms_m3ap_id, (uint32_t)0u, (uint32_t)65535u, false, true));
  }
  return SRSASN_SUCCESS;
}
SRSASN_CODE mbms_session_update_resp_s::unpack(cbit_ref& bref)
{
  bool ext;
  HANDLE_CODE(bref.unpack(ext, 1));
  uint32_t count;
  HANDLE_CODE(unpack_length(count, bref, 0u, 65535u, true));
  bool have_mme_id = false, have_mce_id = false;
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t id;
    crit_e   crit;
    HANDLE_CODE(unpack_ie_hdr(id, crit, bref));
    varlength_field_unpack_guard g(bref, true);
    switch (id) {
      case id_MME_MBMS_M3AP_ID:
        HANDLE_CODE(unpack_integer(mme_mbms_m3ap_id, bref, (uint32_t)0u, (uint32_t)65535u, false, true));
        have_mme_id = true;
        break;
      case id_MCE_MBMS_M3AP_ID:
        HANDLE_CODE(unpack_integer(mce_mbms_m3ap_id, bref, (uint32_t)0u, (uint32_t)65535u, false, true));
        have_mce_id = true;
        break;
      default:
        break;
    }
  }
  return (have_mme_id && have_mce_id) ? SRSASN_SUCCESS : SRSASN_ERROR_DECODE_FAIL;
}

SRSASN_CODE mbms_session_update_fail_s::pack(bit_ref& bref) const
{
  HANDLE_CODE(bref.pack(0, 1));
  HANDLE_CODE(pack_length(bref, (uint32_t)3u, 0u, 65535u, true));
  HANDLE_CODE(pack_ie_hdr(bref, id_MME_MBMS_M3AP_ID, crit_opts::ignore));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(pack_integer(bref, mme_mbms_m3ap_id, (uint32_t)0u, (uint32_t)65535u, false, true));
  }
  HANDLE_CODE(pack_ie_hdr(bref, id_MCE_MBMS_M3AP_ID, crit_opts::ignore));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(pack_integer(bref, mce_mbms_m3ap_id, (uint32_t)0u, (uint32_t)65535u, false, true));
  }
  HANDLE_CODE(pack_ie_hdr(bref, id_Cause, crit_opts::ignore));
  {
    varlength_field_pack_guard g(bref, true);
    HANDLE_CODE(cause.pack(bref));
  }
  return SRSASN_SUCCESS;
}
SRSASN_CODE mbms_session_update_fail_s::unpack(cbit_ref& bref)
{
  bool ext;
  HANDLE_CODE(bref.unpack(ext, 1));
  uint32_t count;
  HANDLE_CODE(unpack_length(count, bref, 0u, 65535u, true));
  bool have_mme_id = false, have_mce_id = false, have_cause = false;
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t id;
    crit_e   crit;
    HANDLE_CODE(unpack_ie_hdr(id, crit, bref));
    varlength_field_unpack_guard g(bref, true);
    switch (id) {
      case id_MME_MBMS_M3AP_ID:
        HANDLE_CODE(unpack_integer(mme_mbms_m3ap_id, bref, (uint32_t)0u, (uint32_t)65535u, false, true));
        have_mme_id = true;
        break;
      case id_MCE_MBMS_M3AP_ID:
        HANDLE_CODE(unpack_integer(mce_mbms_m3ap_id, bref, (uint32_t)0u, (uint32_t)65535u, false, true));
        have_mce_id = true;
        break;
      case id_Cause:
        HANDLE_CODE(cause.unpack(bref));
        have_cause = true;
        break;
      default:
        break;
    }
  }
  return (have_mme_id && have_mce_id && have_cause) ? SRSASN_SUCCESS : SRSASN_ERROR_DECODE_FAIL;
}

/*******************************************************************************
 *                                 M3AP-PDU
 ******************************************************************************/

void m3ap_pdu_c::destroy_()
{
  switch (msg_type_) {
    case msg_type_opts::init_msg:
      switch (proc_code_) {
        case proc_code_m3_setup:
          c.destroy<m3_setup_request_s>();
          break;
        case proc_code_mbms_session_start:
          c.destroy<mbms_session_start_request_s>();
          break;
        case proc_code_mbms_session_stop:
          c.destroy<mbms_session_stop_request_s>();
          break;
        case proc_code_mbms_session_update:
          c.destroy<mbms_session_update_request_s>();
          break;
        default:
          break;
      }
      break;
    case msg_type_opts::successful_outcome:
      switch (proc_code_) {
        case proc_code_m3_setup:
          c.destroy<m3_setup_resp_s>();
          break;
        case proc_code_mbms_session_start:
          c.destroy<mbms_session_start_resp_s>();
          break;
        case proc_code_mbms_session_stop:
          c.destroy<mbms_session_stop_resp_s>();
          break;
        case proc_code_mbms_session_update:
          c.destroy<mbms_session_update_resp_s>();
          break;
        default:
          break;
      }
      break;
    case msg_type_opts::unsuccessful_outcome:
      switch (proc_code_) {
        case proc_code_m3_setup:
          c.destroy<m3_setup_fail_s>();
          break;
        case proc_code_mbms_session_start:
          c.destroy<mbms_session_start_fail_s>();
          break;
        case proc_code_mbms_session_update:
          c.destroy<mbms_session_update_fail_s>();
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
  msg_type_ = msg_type_opts::nulltype;
}

void m3ap_pdu_c::copy_from_(const m3ap_pdu_c& other)
{
  msg_type_  = other.msg_type_;
  proc_code_ = other.proc_code_;
  crit_      = other.crit_;
  switch (msg_type_) {
    case msg_type_opts::init_msg:
      switch (proc_code_) {
        case proc_code_m3_setup:
          c.init(other.c.get<m3_setup_request_s>());
          break;
        case proc_code_mbms_session_start:
          c.init(other.c.get<mbms_session_start_request_s>());
          break;
        case proc_code_mbms_session_stop:
          c.init(other.c.get<mbms_session_stop_request_s>());
          break;
        case proc_code_mbms_session_update:
          c.init(other.c.get<mbms_session_update_request_s>());
          break;
        default:
          break;
      }
      break;
    case msg_type_opts::successful_outcome:
      switch (proc_code_) {
        case proc_code_m3_setup:
          c.init(other.c.get<m3_setup_resp_s>());
          break;
        case proc_code_mbms_session_start:
          c.init(other.c.get<mbms_session_start_resp_s>());
          break;
        case proc_code_mbms_session_stop:
          c.init(other.c.get<mbms_session_stop_resp_s>());
          break;
        case proc_code_mbms_session_update:
          c.init(other.c.get<mbms_session_update_resp_s>());
          break;
        default:
          break;
      }
      break;
    case msg_type_opts::unsuccessful_outcome:
      switch (proc_code_) {
        case proc_code_m3_setup:
          c.init(other.c.get<m3_setup_fail_s>());
          break;
        case proc_code_mbms_session_start:
          c.init(other.c.get<mbms_session_start_fail_s>());
          break;
        case proc_code_mbms_session_update:
          c.init(other.c.get<mbms_session_update_fail_s>());
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}

m3ap_pdu_c& m3ap_pdu_c::operator=(const m3ap_pdu_c& other)
{
  if (this == &other) {
    return *this;
  }
  destroy_();
  copy_from_(other);
  return *this;
}

m3_setup_request_s& m3ap_pdu_c::set_init_msg_m3_setup_request()
{
  destroy_();
  msg_type_  = msg_type_opts::init_msg;
  proc_code_ = proc_code_m3_setup;
  crit_      = crit_opts::reject;
  c.init<m3_setup_request_s>();
  return c.get<m3_setup_request_s>();
}
mbms_session_start_request_s& m3ap_pdu_c::set_init_msg_mbms_session_start_request()
{
  destroy_();
  msg_type_  = msg_type_opts::init_msg;
  proc_code_ = proc_code_mbms_session_start;
  crit_      = crit_opts::reject;
  c.init<mbms_session_start_request_s>();
  return c.get<mbms_session_start_request_s>();
}
mbms_session_stop_request_s& m3ap_pdu_c::set_init_msg_mbms_session_stop_request()
{
  destroy_();
  msg_type_  = msg_type_opts::init_msg;
  proc_code_ = proc_code_mbms_session_stop;
  crit_      = crit_opts::reject;
  c.init<mbms_session_stop_request_s>();
  return c.get<mbms_session_stop_request_s>();
}
mbms_session_update_request_s& m3ap_pdu_c::set_init_msg_mbms_session_update_request()
{
  destroy_();
  msg_type_  = msg_type_opts::init_msg;
  proc_code_ = proc_code_mbms_session_update;
  crit_      = crit_opts::reject;
  c.init<mbms_session_update_request_s>();
  return c.get<mbms_session_update_request_s>();
}

m3_setup_resp_s& m3ap_pdu_c::set_successful_outcome_m3_setup_resp()
{
  destroy_();
  msg_type_  = msg_type_opts::successful_outcome;
  proc_code_ = proc_code_m3_setup;
  crit_      = crit_opts::reject;
  c.init<m3_setup_resp_s>();
  return c.get<m3_setup_resp_s>();
}
mbms_session_start_resp_s& m3ap_pdu_c::set_successful_outcome_mbms_session_start_resp()
{
  destroy_();
  msg_type_  = msg_type_opts::successful_outcome;
  proc_code_ = proc_code_mbms_session_start;
  crit_      = crit_opts::reject;
  c.init<mbms_session_start_resp_s>();
  return c.get<mbms_session_start_resp_s>();
}
mbms_session_stop_resp_s& m3ap_pdu_c::set_successful_outcome_mbms_session_stop_resp()
{
  destroy_();
  msg_type_  = msg_type_opts::successful_outcome;
  proc_code_ = proc_code_mbms_session_stop;
  crit_      = crit_opts::reject;
  c.init<mbms_session_stop_resp_s>();
  return c.get<mbms_session_stop_resp_s>();
}
mbms_session_update_resp_s& m3ap_pdu_c::set_successful_outcome_mbms_session_update_resp()
{
  destroy_();
  msg_type_  = msg_type_opts::successful_outcome;
  proc_code_ = proc_code_mbms_session_update;
  crit_      = crit_opts::reject;
  c.init<mbms_session_update_resp_s>();
  return c.get<mbms_session_update_resp_s>();
}

m3_setup_fail_s& m3ap_pdu_c::set_unsuccessful_outcome_m3_setup_fail()
{
  destroy_();
  msg_type_  = msg_type_opts::unsuccessful_outcome;
  proc_code_ = proc_code_m3_setup;
  crit_      = crit_opts::reject;
  c.init<m3_setup_fail_s>();
  return c.get<m3_setup_fail_s>();
}
mbms_session_start_fail_s& m3ap_pdu_c::set_unsuccessful_outcome_mbms_session_start_fail()
{
  destroy_();
  msg_type_  = msg_type_opts::unsuccessful_outcome;
  proc_code_ = proc_code_mbms_session_start;
  crit_      = crit_opts::reject;
  c.init<mbms_session_start_fail_s>();
  return c.get<mbms_session_start_fail_s>();
}
mbms_session_update_fail_s& m3ap_pdu_c::set_unsuccessful_outcome_mbms_session_update_fail()
{
  destroy_();
  msg_type_  = msg_type_opts::unsuccessful_outcome;
  proc_code_ = proc_code_mbms_session_update;
  crit_      = crit_opts::reject;
  c.init<mbms_session_update_fail_s>();
  return c.get<mbms_session_update_fail_s>();
}

SRSASN_CODE m3ap_pdu_c::pack(bit_ref& bref) const
{
  HANDLE_CODE(msg_type_.pack(bref));
  HANDLE_CODE(pack_integer(bref, proc_code_, (uint16_t)0u, (uint16_t)255u, false, true));
  HANDLE_CODE(crit_.pack(bref));
  varlength_field_pack_guard varlen_scope(bref, true);
  switch (msg_type_) {
    case msg_type_opts::init_msg:
      switch (proc_code_) {
        case proc_code_m3_setup:
          HANDLE_CODE(c.get<m3_setup_request_s>().pack(bref));
          break;
        case proc_code_mbms_session_start:
          HANDLE_CODE(c.get<mbms_session_start_request_s>().pack(bref));
          break;
        case proc_code_mbms_session_stop:
          HANDLE_CODE(c.get<mbms_session_stop_request_s>().pack(bref));
          break;
        case proc_code_mbms_session_update:
          HANDLE_CODE(c.get<mbms_session_update_request_s>().pack(bref));
          break;
        default:
          return SRSASN_ERROR_ENCODE_FAIL;
      }
      break;
    case msg_type_opts::successful_outcome:
      switch (proc_code_) {
        case proc_code_m3_setup:
          HANDLE_CODE(c.get<m3_setup_resp_s>().pack(bref));
          break;
        case proc_code_mbms_session_start:
          HANDLE_CODE(c.get<mbms_session_start_resp_s>().pack(bref));
          break;
        case proc_code_mbms_session_stop:
          HANDLE_CODE(c.get<mbms_session_stop_resp_s>().pack(bref));
          break;
        case proc_code_mbms_session_update:
          HANDLE_CODE(c.get<mbms_session_update_resp_s>().pack(bref));
          break;
        default:
          return SRSASN_ERROR_ENCODE_FAIL;
      }
      break;
    case msg_type_opts::unsuccessful_outcome:
      switch (proc_code_) {
        case proc_code_m3_setup:
          HANDLE_CODE(c.get<m3_setup_fail_s>().pack(bref));
          break;
        case proc_code_mbms_session_start:
          HANDLE_CODE(c.get<mbms_session_start_fail_s>().pack(bref));
          break;
        case proc_code_mbms_session_update:
          HANDLE_CODE(c.get<mbms_session_update_fail_s>().pack(bref));
          break;
        default:
          return SRSASN_ERROR_ENCODE_FAIL;
      }
      break;
    default:
      return SRSASN_ERROR_ENCODE_FAIL;
  }
  return SRSASN_SUCCESS;
}

SRSASN_CODE m3ap_pdu_c::unpack(cbit_ref& bref)
{
  destroy_();
  HANDLE_CODE(msg_type_.unpack(bref));
  HANDLE_CODE(unpack_integer(proc_code_, bref, (uint16_t)0u, (uint16_t)255u, false, true));
  HANDLE_CODE(crit_.unpack(bref));

  varlength_field_unpack_guard varlen_scope(bref, true);
  switch (msg_type_) {
    case msg_type_opts::init_msg:
      switch (proc_code_) {
        case proc_code_m3_setup:
          c.init<m3_setup_request_s>();
          return c.get<m3_setup_request_s>().unpack(bref);
        case proc_code_mbms_session_start:
          c.init<mbms_session_start_request_s>();
          return c.get<mbms_session_start_request_s>().unpack(bref);
        case proc_code_mbms_session_stop:
          c.init<mbms_session_stop_request_s>();
          return c.get<mbms_session_stop_request_s>().unpack(bref);
        case proc_code_mbms_session_update:
          c.init<mbms_session_update_request_s>();
          return c.get<mbms_session_update_request_s>().unpack(bref);
        default:
          return SRSASN_ERROR_DECODE_FAIL;
      }
    case msg_type_opts::successful_outcome:
      switch (proc_code_) {
        case proc_code_m3_setup:
          c.init<m3_setup_resp_s>();
          return c.get<m3_setup_resp_s>().unpack(bref);
        case proc_code_mbms_session_start:
          c.init<mbms_session_start_resp_s>();
          return c.get<mbms_session_start_resp_s>().unpack(bref);
        case proc_code_mbms_session_stop:
          c.init<mbms_session_stop_resp_s>();
          return c.get<mbms_session_stop_resp_s>().unpack(bref);
        case proc_code_mbms_session_update:
          c.init<mbms_session_update_resp_s>();
          return c.get<mbms_session_update_resp_s>().unpack(bref);
        default:
          return SRSASN_ERROR_DECODE_FAIL;
      }
    case msg_type_opts::unsuccessful_outcome:
      switch (proc_code_) {
        case proc_code_m3_setup:
          c.init<m3_setup_fail_s>();
          return c.get<m3_setup_fail_s>().unpack(bref);
        case proc_code_mbms_session_start:
          c.init<mbms_session_start_fail_s>();
          return c.get<mbms_session_start_fail_s>().unpack(bref);
        case proc_code_mbms_session_update:
          c.init<mbms_session_update_fail_s>();
          return c.get<mbms_session_update_fail_s>().unpack(bref);
        default:
          return SRSASN_ERROR_DECODE_FAIL;
      }
    default:
      return SRSASN_ERROR_DECODE_FAIL;
  }
}
