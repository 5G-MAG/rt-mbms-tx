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

#ifndef SRSRAN_M3AP_H
#define SRSRAN_M3AP_H

// M3AP (TS 36.444), MME<->MCE control plane. This project's eNB acts as its
// own distributed MCE (TS 23.246 clause 5.9.1), so in practice this is
// MME<->eNB directly.
//
// Scoped to 4 of M3AP's 7 elementary procedures: M3 Setup, MBMS Session
// Start/Update/Stop (see project plan notes). Reset, Error Indication and
// MCE Configuration Update are not implemented.
//
// Unlike lib/include/srsran/asn1/s1ap.h, this is NOT built around a generic,
// information-object-set-driven ProtocolIE-Container template (S1AP's own
// hand-generated code mirrors what an ASN.1 compiler would emit for its much
// larger procedure set). With only 4 procedures and a small, fixed IE list
// each, each message below hand-implements its own ProtocolIE-Container
// pack()/unpack() directly against the primitives in asn1_utils.h. The wire
// format produced is the real M3AP ProtocolIE-Field encoding (id + criticality
// + open-type-wrapped value, one per present IE) -- not a simplified/invented
// substitute -- so it decodes correctly in a real M3AP analyzer. Per-IE
// values not in this pass's scope are safely skippable on receive because
// each IE value is length-prefixed as an open type (varlength_field_pack_guard
// / varlength_field_unpack_guard already do this, see .cc).

#include "asn1_utils.h"

namespace asn1 {
namespace m3ap {

/*******************************************************************************
 *                              Common Data Types
 ******************************************************************************/

// Criticality ::= ENUMERATED { reject, ignore, notify }
struct crit_opts {
  enum options { reject, ignore, notify, nulltype } value;
  const char* to_string() const;
};
using crit_e = enumerated<crit_opts>;

// TriggeringMessage ::= ENUMERATED { initiating-message, successful-outcome, unsuccessful-outcome }
struct trigger_msg_opts {
  enum options { init_msg, successful_outcome, unsuccessful_outcome, nulltype } value;
  const char* to_string() const;
};
using trigger_msg_e = enumerated<trigger_msg_opts>;

// ProcedureCode ::= INTEGER (0..255) -- TS 36.444 clause 9.3.4
enum proc_code_e : uint8_t {
  proc_code_mbms_session_start  = 0,
  proc_code_mbms_session_stop   = 1,
  proc_code_error_ind           = 2,
  proc_code_private_msg         = 3,
  proc_code_reset               = 4,
  proc_code_mbms_session_update = 5,
  proc_code_mce_cfg_upd         = 6,
  proc_code_m3_setup            = 7
};

// ProtocolIE-ID ::= INTEGER (0..maxProtocolIEs) -- only the values used by
// this pass's in-scope IEs (clause 9.3.4).
enum ie_id_e : uint16_t {
  id_MME_MBMS_M3AP_ID              = 0,
  id_MCE_MBMS_M3AP_ID              = 1,
  id_TMGI                          = 2,
  id_MBMS_Session_ID               = 3,
  id_MBMS_E_RAB_QoS_Parameters     = 4,
  id_MBMS_Session_Duration         = 5,
  id_MBMS_Service_Area             = 6,
  id_TNL_Information               = 7,
  id_CriticalityDiagnostics        = 8,
  id_Cause                         = 9,
  id_TimeToWait                    = 12,
  id_MinimumTimeToMBMSDataTransfer = 16,
  id_Global_MCE_ID                 = 18,
  id_MCEname                       = 19,
  id_MBMSServiceAreaList           = 20
};

/*******************************************************************************
 *                                  IE Types
 ******************************************************************************/

// TMGI ::= SEQUENCE { pLMNidentity PLMN-Identity, serviceID OCTET STRING(3), ... }
// iE-Extensions omitted (never populated by either side of this codec).
struct tmgi_s {
  fixed_octstring<3, true> plmn_id;
  fixed_octstring<3, true> service_id;

  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
};

// GBR-QosInformation ::= SEQUENCE { mBMS-E-RAB-MaximumBitrateDL BitRate, mBMS-E-RAB-GuaranteedBitrateDL BitRate, ... }
// BitRate ::= INTEGER (0..10000000000)
struct gbr_qos_info_s {
  uint64_t max_bitrate_dl        = 0;
  uint64_t guaranteed_bitrate_dl = 0;

  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
};

// MBMS-E-RAB-QoS-Parameters ::= SEQUENCE { qCI QCI, gbrQosInformation GBR-QosInformation OPTIONAL, ... }
// QCI ::= INTEGER (0..255)
struct mbms_e_rab_qos_params_s {
  uint16_t        qci                   = 0; // QCI ::= INTEGER(0..255); see mme_mbms_m3ap_id comment on mbms_session_start_resp_s
  bool            gbr_qos_info_present  = false;
  gbr_qos_info_s  gbr_qos_info;

  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
};

// IPAddress ::= OCTET STRING (SIZE(4..16, ...))
// The "..." extension marker (allowing future sizes outside 4..16) is not
// modeled: both ends of this codec are this same implementation, so it is
// only ever encoded/decoded as a plain bounded(4..16) octet string.
struct ip_address_s {
  bounded_octstring<4, 16, true> addr;

  SRSASN_CODE pack(bit_ref& bref) const { return addr.pack(bref); }
  SRSASN_CODE unpack(cbit_ref& bref) { return addr.unpack(bref); }
};

// TNL-Information ::= SEQUENCE { iPMCAddress IPAddress, iPSourceAddress IPAddress, gTP-DLTEID GTP-TEID, ... }
// GTP-TEID ::= OCTET STRING (SIZE(4))
struct tnl_info_s {
  ip_address_s             ip_mc_addr;
  ip_address_s             ip_src_addr;
  fixed_octstring<4, true> gtp_dl_teid;

  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
};

// Global-MCE-ID ::= SEQUENCE { pLMN-Identity PLMN-Identity, mCE-ID MCE-ID, extendedMCE-ID ExtendedMCE-ID OPTIONAL, ... }
// MCE-ID ::= OCTET STRING(SIZE(2)); ExtendedMCE-ID ::= OCTET STRING(SIZE(1))
struct global_mce_id_s {
  fixed_octstring<3, true> plmn_id;
  fixed_octstring<2, true> mce_id;
  bool                     ext_mce_id_present = false;
  fixed_octstring<1, true> ext_mce_id;

  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
};

// CauseRadioNetwork/Transport/NAS/Protocol/Misc ::= ENUMERATED { ... }
struct cause_radio_network_opts {
  enum options {
    unknown_or_already_allocated_mme_mbms_m3ap_id,
    unknown_or_already_allocated_mce_mbms_m3ap_id,
    unknown_or_inconsistent_pair_of_mbms_m3ap_ids,
    radio_res_not_available,
    invalid_qos_combination,
    interaction_with_other_proc,
    not_supported_qci_value,
    unspecified,
    // extension addition (after "...")
    uninvolved_mce,
    nulltype
  } value;
  const char* to_string() const;
};
using cause_radio_network_e = enumerated<cause_radio_network_opts, true, 1>;

struct cause_transport_opts {
  enum options { transport_res_unavailable, unspecified, nulltype } value;
  const char* to_string() const;
};
using cause_transport_e = enumerated<cause_transport_opts, true, 0>;

struct cause_nas_opts {
  enum options { unspecified, nulltype } value;
  const char* to_string() const;
};
using cause_nas_e = enumerated<cause_nas_opts, true, 0>;

struct cause_protocol_opts {
  enum options {
    transfer_syntax_error,
    abstract_syntax_error_reject,
    abstract_syntax_error_ignore_and_notify,
    msg_not_compatible_with_receiver_state,
    semantic_error,
    abstract_syntax_error_falsely_constructed_msg,
    unspecified,
    nulltype
  } value;
  const char* to_string() const;
};
using cause_protocol_e = enumerated<cause_protocol_opts, true, 0>;

struct cause_misc_opts {
  enum options {
    ctrl_processing_overload,
    not_enough_user_plane_processing_res,
    hardware_fail,
    om_intervention,
    unspecified,
    nulltype
  } value;
  const char* to_string() const;
};
using cause_misc_e = enumerated<cause_misc_opts, true, 0>;

// Cause ::= CHOICE { radioNetwork, transport, nAS, protocol, misc, ... }
struct cause_c {
  struct types_opts {
    enum options { radio_network, transport, nas, protocol, misc, nulltype } value;
    const char* to_string() const;
  };
  using types = enumerated<types_opts, true, 0>;

  cause_c() = default;
  void        set_radio_network(cause_radio_network_e::options v = cause_radio_network_opts::unspecified);
  void        set_transport(cause_transport_e::options v = cause_transport_opts::unspecified);
  void        set_nas(cause_nas_e::options v = cause_nas_opts::unspecified);
  void        set_protocol(cause_protocol_e::options v = cause_protocol_opts::unspecified);
  void        set_misc(cause_misc_e::options v = cause_misc_opts::unspecified);
  types       type() const { return type_; }
  const char* to_string() const;

  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);

private:
  types   type_ = types_opts::nulltype;
  uint8_t val_  = 0; // group-local enum value index
};

// TimeToWait ::= ENUMERATED {v1s, v2s, v5s, v10s, v20s, v60s, ...}
struct time_to_wait_opts {
  enum options { v1s, v2s, v5s, v10s, v20s, v60s, nulltype } value;
  const char* to_string() const;
};
using time_to_wait_e = enumerated<time_to_wait_opts, true, 0>;

// MCEname ::= PrintableString (SIZE (1..150,...))
using mce_name_s = printable_string<1, 150, true, true>;

/*******************************************************************************
 *                              Messages
 ******************************************************************************/

// M3SetupRequest ::= SEQUENCE { protocolIEs { Global-MCE-ID, MCEname OPT, MBMSServiceAreaListItem }, ... }
// MBMSServiceAreaListItem ::= SEQUENCE (SIZE(1..maxnoofMBMSServiceAreaIdentitiesPerMCE)) OF MBMSServiceArea1(2 octets)
struct m3_setup_request_s {
  global_mce_id_s                                   global_mce_id;
  bool                                               mce_name_present = false;
  mce_name_s                                         mce_name;
  dyn_seq_of<fixed_octstring<2, true>, 1, 65536, true> mbms_service_area_list;

  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
};

// M3SetupResponse ::= SEQUENCE { protocolIEs { CriticalityDiagnostics OPT }, ... }
// CriticalityDiagnostics is never generated/consumed by this pass -- kept as
// an always-empty IE container on this message.
struct m3_setup_resp_s {
  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
};

// M3SetupFailure ::= SEQUENCE { protocolIEs { Cause, TimeToWait OPT, CriticalityDiagnostics OPT }, ... }
struct m3_setup_fail_s {
  cause_c              cause;
  bool                 time_to_wait_present = false;
  time_to_wait_e       time_to_wait;

  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
};

// MBMSSessionStartRequest-IEs (clause 9.2, in-scope subset -- see m3ap.h top comment)
struct mbms_session_start_request_s {
  uint32_t                 mme_mbms_m3ap_id = 0; // MME-MBMS-M3AP-ID ::= INTEGER(0..65535); see mbms_session_start_resp_s
  tmgi_s                   tmgi;
  bool                     mbms_session_id_present = false;
  fixed_octstring<1, true> mbms_session_id;
  mbms_e_rab_qos_params_s  mbms_e_rab_qos_params;
  fixed_octstring<3, true> mbms_session_duration;
  unbounded_octstring<true> mbms_service_area;
  fixed_octstring<1, true> min_time_to_mbms_data_transfer;
  tnl_info_s               tnl_info;

  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
};

struct mbms_session_start_resp_s {
  uint32_t mme_mbms_m3ap_id = 0; // MME-MBMS-M3AP-ID ::= INTEGER(0..65535); wider storage avoids a pack_integer/unpack_integer quirk where bound==IntType max silently no-ops (see m3ap.cc)
  uint32_t mce_mbms_m3ap_id = 0; // see mme_mbms_m3ap_id comment

  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
};

struct mbms_session_start_fail_s {
  uint32_t mme_mbms_m3ap_id = 0; // MME-MBMS-M3AP-ID ::= INTEGER(0..65535); wider storage avoids a pack_integer/unpack_integer quirk where bound==IntType max silently no-ops (see m3ap.cc)
  cause_c  cause;

  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
};

struct mbms_session_stop_request_s {
  uint32_t mme_mbms_m3ap_id = 0; // MME-MBMS-M3AP-ID ::= INTEGER(0..65535); wider storage avoids a pack_integer/unpack_integer quirk where bound==IntType max silently no-ops (see m3ap.cc)
  uint32_t mce_mbms_m3ap_id = 0; // see mme_mbms_m3ap_id comment

  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
};

// MBMSSessionStopResponse-IEs -- note TS 36.444 defines no Failure message
// for this procedure (clause 8.3).
struct mbms_session_stop_resp_s {
  uint32_t mme_mbms_m3ap_id = 0; // MME-MBMS-M3AP-ID ::= INTEGER(0..65535); wider storage avoids a pack_integer/unpack_integer quirk where bound==IntType max silently no-ops (see m3ap.cc)
  uint32_t mce_mbms_m3ap_id = 0; // see mme_mbms_m3ap_id comment

  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
};

struct mbms_session_update_request_s {
  uint32_t                  mme_mbms_m3ap_id = 0; // see mbms_session_start_resp_s
  uint32_t                  mce_mbms_m3ap_id = 0; // see mbms_session_start_resp_s
  tmgi_s                    tmgi;
  bool                      mbms_session_id_present = false;
  fixed_octstring<1, true>  mbms_session_id;
  mbms_e_rab_qos_params_s   mbms_e_rab_qos_params;
  fixed_octstring<3, true>  mbms_session_duration;
  bool                      mbms_service_area_present = false;
  unbounded_octstring<true> mbms_service_area;
  fixed_octstring<1, true>  min_time_to_mbms_data_transfer;

  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
};

struct mbms_session_update_resp_s {
  uint32_t mme_mbms_m3ap_id = 0; // MME-MBMS-M3AP-ID ::= INTEGER(0..65535); wider storage avoids a pack_integer/unpack_integer quirk where bound==IntType max silently no-ops (see m3ap.cc)
  uint32_t mce_mbms_m3ap_id = 0; // see mme_mbms_m3ap_id comment

  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
};

struct mbms_session_update_fail_s {
  uint32_t mme_mbms_m3ap_id = 0; // MME-MBMS-M3AP-ID ::= INTEGER(0..65535); wider storage avoids a pack_integer/unpack_integer quirk where bound==IntType max silently no-ops (see m3ap.cc)
  uint32_t mce_mbms_m3ap_id = 0; // see mme_mbms_m3ap_id comment
  cause_c  cause;

  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
};

/*******************************************************************************
 *                              M3AP-PDU
 ******************************************************************************/

// M3AP-PDU ::= CHOICE { initiatingMessage, successfulOutcome, unsuccessfulOutcome, ... }
// InitiatingMessage/SuccessfulOutcome/UnsuccessfulOutcome ::= SEQUENCE { procedureCode, criticality, value }
// "value" is an information-object-class-constrained open type (clause 9.1) --
// encoded/decoded as an octet-aligned length-prefixed blob, exactly as each
// ProtocolIE-Field's own value is (see the .cc for both).
// M3AP-PDU's own choice tag between initiatingMessage/successfulOutcome/unsuccessfulOutcome. A real, explicit
// CHOICE index in the wire bytes (unlike a ProtocolIE value, which is an open type governed by context) --
// mirrors s1ap_pdu_c::types in s1ap.h exactly.
struct msg_type_opts {
  enum options { init_msg, successful_outcome, unsuccessful_outcome, nulltype } value;
  const char* to_string() const;
};
using msg_type_e = enumerated<msg_type_opts, true, 0>;

struct m3ap_pdu_c {
  m3ap_pdu_c() = default;
  m3ap_pdu_c(const m3ap_pdu_c& other) { *this = other; }
  m3ap_pdu_c& operator=(const m3ap_pdu_c& other);
  ~m3ap_pdu_c() { destroy_(); }

  msg_type_e msg_type() const { return msg_type_; }
  uint16_t   proc_code() const { return proc_code_; }
  crit_e     crit() const { return crit_; }

  m3_setup_request_s&               set_init_msg_m3_setup_request();
  mbms_session_start_request_s&     set_init_msg_mbms_session_start_request();
  mbms_session_stop_request_s&      set_init_msg_mbms_session_stop_request();
  mbms_session_update_request_s&    set_init_msg_mbms_session_update_request();

  m3_setup_resp_s&                  set_successful_outcome_m3_setup_resp();
  mbms_session_start_resp_s&        set_successful_outcome_mbms_session_start_resp();
  mbms_session_stop_resp_s&         set_successful_outcome_mbms_session_stop_resp();
  mbms_session_update_resp_s&       set_successful_outcome_mbms_session_update_resp();

  m3_setup_fail_s&                  set_unsuccessful_outcome_m3_setup_fail();
  mbms_session_start_fail_s&        set_unsuccessful_outcome_mbms_session_start_fail();
  mbms_session_update_fail_s&       set_unsuccessful_outcome_mbms_session_update_fail();

  const m3_setup_request_s&               m3_setup_request() const { return c.get<m3_setup_request_s>(); }
  const mbms_session_start_request_s&     mbms_session_start_request() const
  {
    return c.get<mbms_session_start_request_s>();
  }
  const mbms_session_stop_request_s& mbms_session_stop_request() const
  {
    return c.get<mbms_session_stop_request_s>();
  }
  const mbms_session_update_request_s& mbms_session_update_request() const
  {
    return c.get<mbms_session_update_request_s>();
  }
  const m3_setup_resp_s&           m3_setup_resp() const { return c.get<m3_setup_resp_s>(); }
  const mbms_session_start_resp_s& mbms_session_start_resp() const { return c.get<mbms_session_start_resp_s>(); }
  const mbms_session_stop_resp_s&  mbms_session_stop_resp() const { return c.get<mbms_session_stop_resp_s>(); }
  const mbms_session_update_resp_s& mbms_session_update_resp() const
  {
    return c.get<mbms_session_update_resp_s>();
  }
  const m3_setup_fail_s&             m3_setup_fail() const { return c.get<m3_setup_fail_s>(); }
  const mbms_session_start_fail_s&   mbms_session_start_fail() const { return c.get<mbms_session_start_fail_s>(); }
  const mbms_session_update_fail_s&  mbms_session_update_fail() const { return c.get<mbms_session_update_fail_s>(); }

  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);

private:
  msg_type_e msg_type_;
  uint16_t   proc_code_ = 0; // ProcedureCode ::= INTEGER(0..255); see mme_mbms_m3ap_id comment on mbms_session_start_resp_s
  crit_e     crit_;
  choice_buffer_t<m3_setup_request_s,
                  m3_setup_resp_s,
                  m3_setup_fail_s,
                  mbms_session_start_request_s,
                  mbms_session_start_resp_s,
                  mbms_session_start_fail_s,
                  mbms_session_stop_request_s,
                  mbms_session_stop_resp_s,
                  mbms_session_update_request_s,
                  mbms_session_update_resp_s,
                  mbms_session_update_fail_s>
      c;

  void destroy_();
  void copy_from_(const m3ap_pdu_c& other);
};

} // namespace m3ap
} // namespace asn1

#endif // SRSRAN_M3AP_H
