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

/*******************************************************************************
 *
 *                     3GPP TS ASN1 SBc-AP v29.168 (CBC <-> MME)
 *
 * Hand-ported from the validated `.asn` reference at
 * asn1_spec/sbc-ap/sbc-ap_ts29168-j00.asn, mirroring the exact struct/pack/unpack
 * idiom used throughout s1ap.h (see e.g. write_replace_warning_request_ies_o
 * there, which carries near-identical IEs since S1AP forwards this same PWS
 * content). Deliberately scoped to just the messages/IEs needed for a first
 * working CBC->MME->eNB warning chain (see asn1_spec/README.md and the
 * project's implementation plan for what's excluded and why):
 *   - No List-of-TAIs / Warning-Area-List (this is a single-cell testbed;
 *     targeting is moot). Every request broadcasts to all connected eNBs.
 *   - No Concurrent-Warning-Message-Indicator, OMC-Id, Global-ENB-ID,
 *     Extended-Repetition-Period, Send-*-Indication, Stop-All-Indicator.
 *   - No 5GS extension IEs (this codebase is LTE-only).
 *   - No protocolExtensions handling on any message -- mirrors how the
 *     existing S1AP write_replace_warning_request_s in this same codebase
 *     already omits it too (see write_replace_warning_request_s::pack() in
 *     s1ap.cc: packs only the extension bit + protocolIEs, nothing else).
 *
 ******************************************************************************/

#ifndef SRSASN1_SBC_AP_H
#define SRSASN1_SBC_AP_H

#include "asn1_utils.h"
#include <cstdio>

namespace asn1 {
namespace sbc_ap {

/*******************************************************************************
 *                             Struct Definitions
 ******************************************************************************/

// Criticality ::= ENUMERATED
struct crit_opts {
  enum options { reject, ignore, notify, nulltype } value;

  const char* to_string() const;
};
typedef enumerated<crit_opts> crit_e;

// Presence ::= ENUMERATED
struct presence_opts {
  enum options { optional, conditional, mandatory, nulltype } value;

  const char* to_string() const;
};
typedef enumerated<presence_opts> presence_e;

// ProtocolIE-Field{SBC-AP-PROTOCOL-IES : IEsSetParam} ::= SEQUENCE{{SBC-AP-PROTOCOL-IES}}
template <class ies_set_paramT_>
struct protocol_ie_field_s {
  uint32_t                          id = 0;
  crit_e                            crit;
  typename ies_set_paramT_::value_c value;

  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
  void        to_json(json_writer& j) const;
  bool        load_info_obj(const uint32_t& id_);
};

// ProtocolIE-Container-Item -- concrete-typed IE (id, crit, value), used inside a container
// when the concrete field type is already known statically (no runtime dispatch needed).
template <class valueT_>
struct protocol_ie_container_item_s {
  uint32_t id = 0;
  crit_e   crit;
  valueT_  value;

  protocol_ie_container_item_s(uint32_t id_, crit_e crit_);
  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
  void        to_json(json_writer& j) const;
};

// Write-Replace-Warning-Request-IEs SBC-AP-PROTOCOL-IES ::= OBJECT SET
struct write_replace_warning_request_ies_o {
  // Value ::= OPEN TYPE
  struct value_c {
    struct types_opts {
      enum options {
        msg_id,
        serial_num,
        repeat_period,
        nof_broadcasts_requested,
        warning_type,
        warning_security_info,
        data_coding_scheme,
        warning_msg_content,
        warning_area_coordinates,
        nulltype
      } value;

      const char* to_string() const;
    };
    typedef enumerated<types_opts> types;

    // choice methods
    value_c() = default;
    value_c(const value_c& other);
    value_c& operator=(const value_c& other);
    ~value_c() { destroy_(); }
    void        set(types::options e = types::nulltype);
    types       type() const { return type_; }
    SRSASN_CODE pack(bit_ref& bref) const;
    SRSASN_CODE unpack(cbit_ref& bref);
    void        to_json(json_writer& j) const;
    // getters
    fixed_bitstring<16, false, true>&        msg_id();
    fixed_bitstring<16, false, true>&        serial_num();
    uint16_t&                                repeat_period();
    uint32_t&                                nof_broadcasts_requested();
    fixed_octstring<2, true>&                warning_type();
    fixed_octstring<50, true>&               warning_security_info();
    fixed_bitstring<8, false, true>&         data_coding_scheme();
    bounded_octstring<1, 9600, true>&        warning_msg_content();
    bounded_octstring<1, 1024, true>&        warning_area_coordinates();
    const fixed_bitstring<16, false, true>&  msg_id() const;
    const fixed_bitstring<16, false, true>&  serial_num() const;
    const uint16_t&                          repeat_period() const;
    const uint32_t&                          nof_broadcasts_requested() const;
    const fixed_octstring<2, true>&          warning_type() const;
    const fixed_octstring<50, true>&         warning_security_info() const;
    const fixed_bitstring<8, false, true>&   data_coding_scheme() const;
    const bounded_octstring<1, 9600, true>&  warning_msg_content() const;
    const bounded_octstring<1, 1024, true>&  warning_area_coordinates() const;

  private:
    types type_;
    choice_buffer_t<bounded_octstring<1, 1024, true>,
                    bounded_octstring<1, 9600, true>,
                    fixed_bitstring<16, false, true>,
                    fixed_octstring<2, true>,
                    fixed_octstring<50, true> >
        c;

    void destroy_();
  };

  // members lookup methods
  static uint32_t   idx_to_id(uint32_t idx);
  static bool       is_id_valid(const uint32_t& id);
  static crit_e     get_crit(const uint32_t& id);
  static value_c    get_value(const uint32_t& id);
  static presence_e get_presence(const uint32_t& id);
};

struct write_replace_warning_request_ies_container {
  template <class valueT_>
  using ie_field_s = protocol_ie_container_item_s<valueT_>;

  // member variables
  bool                                                   warning_type_present             = false;
  bool                                                   warning_security_info_present    = false;
  bool                                                   data_coding_scheme_present       = false;
  bool                                                   warning_msg_content_present      = false;
  bool                                                   warning_area_coordinates_present = false;
  ie_field_s<fixed_bitstring<16, false, true> >          msg_id;
  ie_field_s<fixed_bitstring<16, false, true> >          serial_num;
  ie_field_s<integer<uint16_t, 0, 4096, false, true> >   repeat_period;
  ie_field_s<integer<uint32_t, 0, 65535, false, true> >  nof_broadcasts_requested;
  ie_field_s<fixed_octstring<2, true> >                  warning_type;
  ie_field_s<fixed_octstring<50, true> >                 warning_security_info;
  ie_field_s<fixed_bitstring<8, false, true> >           data_coding_scheme;
  ie_field_s<bounded_octstring<1, 9600, true> >          warning_msg_content;
  ie_field_s<bounded_octstring<1, 1024, true> >          warning_area_coordinates;

  // sequence methods
  write_replace_warning_request_ies_container();
  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
  void        to_json(json_writer& j) const;
};

// Write-Replace-Warning-Request ::= SEQUENCE
struct write_replace_warning_request_s {
  bool                                         ext = false;
  write_replace_warning_request_ies_container protocol_ies;
  // protocolExtensions OPTIONAL -- deliberately not modeled, see file header

  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
  void        to_json(json_writer& j) const;
};

// Write-Replace-Warning-Response-IEs SBC-AP-PROTOCOL-IES ::= OBJECT SET
struct write_replace_warning_resp_ies_o {
  // Value ::= OPEN TYPE
  struct value_c {
    struct types_opts {
      enum options { msg_id, serial_num, cause, nulltype } value;

      const char* to_string() const;
    };
    typedef enumerated<types_opts> types;

    // choice methods
    value_c() = default;
    value_c(const value_c& other);
    value_c& operator=(const value_c& other);
    ~value_c() { destroy_(); }
    void        set(types::options e = types::nulltype);
    types       type() const { return type_; }
    SRSASN_CODE pack(bit_ref& bref) const;
    SRSASN_CODE unpack(cbit_ref& bref);
    void        to_json(json_writer& j) const;
    // getters
    fixed_bitstring<16, false, true>&       msg_id();
    fixed_bitstring<16, false, true>&       serial_num();
    uint16_t&                               cause();
    const fixed_bitstring<16, false, true>& msg_id() const;
    const fixed_bitstring<16, false, true>& serial_num() const;
    const uint16_t&                         cause() const;

  private:
    types                                          type_;
    choice_buffer_t<fixed_bitstring<16, false, true> > c;

    void destroy_();
  };

  static uint32_t   idx_to_id(uint32_t idx);
  static bool       is_id_valid(const uint32_t& id);
  static crit_e     get_crit(const uint32_t& id);
  static value_c    get_value(const uint32_t& id);
  static presence_e get_presence(const uint32_t& id);
};

struct write_replace_warning_resp_ies_container {
  template <class valueT_>
  using ie_field_s = protocol_ie_container_item_s<valueT_>;

  ie_field_s<fixed_bitstring<16, false, true> >       msg_id;
  ie_field_s<fixed_bitstring<16, false, true> >       serial_num;
  ie_field_s<integer<uint16_t, 0, 255, false, true> > cause;

  write_replace_warning_resp_ies_container();
  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
  void        to_json(json_writer& j) const;
};

// Write-Replace-Warning-Response ::= SEQUENCE
struct write_replace_warning_resp_s {
  bool                                      ext = false;
  write_replace_warning_resp_ies_container protocol_ies;

  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
  void        to_json(json_writer& j) const;
};

// Stop-Warning-Request-IEs SBC-AP-PROTOCOL-IES ::= OBJECT SET
struct stop_warning_request_ies_o {
  // Value ::= OPEN TYPE
  struct value_c {
    struct types_opts {
      enum options { msg_id, serial_num, nulltype } value;

      const char* to_string() const;
    };
    typedef enumerated<types_opts> types;

    // choice methods
    value_c() = default;
    value_c(const value_c& other);
    value_c& operator=(const value_c& other);
    ~value_c() { destroy_(); }
    void        set(types::options e = types::nulltype);
    types       type() const { return type_; }
    SRSASN_CODE pack(bit_ref& bref) const;
    SRSASN_CODE unpack(cbit_ref& bref);
    void        to_json(json_writer& j) const;
    // getters
    fixed_bitstring<16, false, true>&       msg_id();
    fixed_bitstring<16, false, true>&       serial_num();
    const fixed_bitstring<16, false, true>& msg_id() const;
    const fixed_bitstring<16, false, true>& serial_num() const;

  private:
    types                                              type_;
    choice_buffer_t<fixed_bitstring<16, false, true> > c;

    void destroy_();
  };

  static uint32_t   idx_to_id(uint32_t idx);
  static bool       is_id_valid(const uint32_t& id);
  static crit_e     get_crit(const uint32_t& id);
  static value_c    get_value(const uint32_t& id);
  static presence_e get_presence(const uint32_t& id);
};

struct stop_warning_request_ies_container {
  template <class valueT_>
  using ie_field_s = protocol_ie_container_item_s<valueT_>;

  ie_field_s<fixed_bitstring<16, false, true> > msg_id;
  ie_field_s<fixed_bitstring<16, false, true> > serial_num;

  stop_warning_request_ies_container();
  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
  void        to_json(json_writer& j) const;
};

// Stop-Warning-Request ::= SEQUENCE
struct stop_warning_request_s {
  bool                                ext = false;
  stop_warning_request_ies_container protocol_ies;

  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
  void        to_json(json_writer& j) const;
};

// Stop-Warning-Response-IEs SBC-AP-PROTOCOL-IES ::= OBJECT SET
struct stop_warning_resp_ies_o {
  // Value ::= OPEN TYPE
  struct value_c {
    struct types_opts {
      enum options { msg_id, serial_num, cause, nulltype } value;

      const char* to_string() const;
    };
    typedef enumerated<types_opts> types;

    // choice methods
    value_c() = default;
    value_c(const value_c& other);
    value_c& operator=(const value_c& other);
    ~value_c() { destroy_(); }
    void        set(types::options e = types::nulltype);
    types       type() const { return type_; }
    SRSASN_CODE pack(bit_ref& bref) const;
    SRSASN_CODE unpack(cbit_ref& bref);
    void        to_json(json_writer& j) const;
    // getters
    fixed_bitstring<16, false, true>&       msg_id();
    fixed_bitstring<16, false, true>&       serial_num();
    uint16_t&                               cause();
    const fixed_bitstring<16, false, true>& msg_id() const;
    const fixed_bitstring<16, false, true>& serial_num() const;
    const uint16_t&                         cause() const;

  private:
    types                                          type_;
    choice_buffer_t<fixed_bitstring<16, false, true> > c;

    void destroy_();
  };

  static uint32_t   idx_to_id(uint32_t idx);
  static bool       is_id_valid(const uint32_t& id);
  static crit_e     get_crit(const uint32_t& id);
  static value_c    get_value(const uint32_t& id);
  static presence_e get_presence(const uint32_t& id);
};

struct stop_warning_resp_ies_container {
  template <class valueT_>
  using ie_field_s = protocol_ie_container_item_s<valueT_>;

  ie_field_s<fixed_bitstring<16, false, true> >       msg_id;
  ie_field_s<fixed_bitstring<16, false, true> >       serial_num;
  ie_field_s<integer<uint16_t, 0, 255, false, true> > cause;

  stop_warning_resp_ies_container();
  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
  void        to_json(json_writer& j) const;
};

// Stop-Warning-Response ::= SEQUENCE
struct stop_warning_resp_s {
  bool                             ext = false;
  stop_warning_resp_ies_container protocol_ies;

  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
  void        to_json(json_writer& j) const;
};

/*******************************************************************************
 *                    SBC-AP-PDU top-level PDU wrapper
 *
 * Mirrors s1ap_pdu_c/s1ap_elem_procs_o in s1ap.h. Real wire traffic must be
 * self-describing (procedure code + initiating/successful-outcome
 * discriminator) for a receiver to know which message follows -- this is
 * what a bare Write-Replace-Warning-Request/etc. struct on its own cannot
 * express.
 ******************************************************************************/

// SBC-AP-ELEMENTARY-PROCEDURE object set, scoped to the two procedures this
// codec implements: write-Replace-Warning (0) and stop-Warning (1).
struct sbc_ap_elem_procs_o {
  // InitiatingMessage ::= OPEN TYPE
  struct init_msg_c {
    struct types_opts {
      enum options { write_replace_warning_request, stop_warning_request, nulltype } value;

      const char* to_string() const;
    };
    typedef enumerated<types_opts> types;

    init_msg_c() = default;
    init_msg_c(const init_msg_c& other);
    init_msg_c& operator=(const init_msg_c& other);
    ~init_msg_c() { destroy_(); }
    void        set(types::options e = types::nulltype);
    types       type() const { return type_; }
    SRSASN_CODE pack(bit_ref& bref) const;
    SRSASN_CODE unpack(cbit_ref& bref);
    void        to_json(json_writer& j) const;
    write_replace_warning_request_s&       write_replace_warning_request();
    stop_warning_request_s&                stop_warning_request();
    const write_replace_warning_request_s& write_replace_warning_request() const;
    const stop_warning_request_s&          stop_warning_request() const;

  private:
    types                                                                type_;
    choice_buffer_t<write_replace_warning_request_s, stop_warning_request_s> c;

    void destroy_();
  };
  // SuccessfulOutcome ::= OPEN TYPE
  struct successful_outcome_c {
    struct types_opts {
      enum options { write_replace_warning_resp, stop_warning_resp, nulltype } value;

      const char* to_string() const;
    };
    typedef enumerated<types_opts> types;

    successful_outcome_c() = default;
    successful_outcome_c(const successful_outcome_c& other);
    successful_outcome_c& operator=(const successful_outcome_c& other);
    ~successful_outcome_c() { destroy_(); }
    void        set(types::options e = types::nulltype);
    types       type() const { return type_; }
    SRSASN_CODE pack(bit_ref& bref) const;
    SRSASN_CODE unpack(cbit_ref& bref);
    void        to_json(json_writer& j) const;
    write_replace_warning_resp_s&       write_replace_warning_resp();
    stop_warning_resp_s&                stop_warning_resp();
    const write_replace_warning_resp_s& write_replace_warning_resp() const;
    const stop_warning_resp_s&          stop_warning_resp() const;

  private:
    types                                                          type_;
    choice_buffer_t<write_replace_warning_resp_s, stop_warning_resp_s> c;

    void destroy_();
  };

  static uint16_t             idx_to_proc_code(uint32_t idx);
  static bool                 is_proc_code_valid(const uint16_t& proc_code);
  static init_msg_c           get_init_msg(const uint16_t& proc_code);
  static successful_outcome_c get_successful_outcome(const uint16_t& proc_code);
  static crit_e               get_crit(const uint16_t& proc_code);
};

// InitiatingMessage ::= SEQUENCE{{SBC-AP-ELEMENTARY-PROCEDURE}}
struct sbc_ap_init_msg_s {
  uint16_t                         proc_code = 0;
  crit_e                           crit;
  sbc_ap_elem_procs_o::init_msg_c value;

  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
  void        to_json(json_writer& j) const;
  bool        load_info_obj(const uint16_t& proc_code_);
};

// SuccessfulOutcome ::= SEQUENCE{{SBC-AP-ELEMENTARY-PROCEDURE}}
struct sbc_ap_successful_outcome_s {
  uint16_t                                   proc_code = 0;
  crit_e                                     crit;
  sbc_ap_elem_procs_o::successful_outcome_c value;

  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
  void        to_json(json_writer& j) const;
  bool        load_info_obj(const uint16_t& proc_code_);
};

// UnsuccessfulOutcome ::= SEQUENCE{{SBC-AP-ELEMENTARY-PROCEDURE}} -- structural stub only.
// Neither procedure in this scoped-down codec (write-Replace-Warning, stop-Warning)
// declares an UnsuccessfulOutcome in the spec, so no message content is modeled here.
// Present only so SBC-AP-PDU's CHOICE selector below uses the real 3-value bit width
// for wire compatibility with a genuine third-party peer; pack()/unpack() log an error
// and fail if ever actually exercised, since that should be structurally unreachable.
struct sbc_ap_unsuccessful_outcome_s {
  uint16_t proc_code = 0;
  crit_e   crit;

  SRSASN_CODE pack(bit_ref& bref) const;
  SRSASN_CODE unpack(cbit_ref& bref);
  void        to_json(json_writer& j) const;
};

// SBC-AP-PDU ::= CHOICE
struct sbc_ap_pdu_c {
  struct types_opts {
    enum options { init_msg, successful_outcome, unsuccessful_outcome, nulltype } value;

    const char* to_string() const;
  };
  typedef enumerated<types_opts, true> types; // extensible ("...")

  sbc_ap_pdu_c() = default;
  sbc_ap_pdu_c(const sbc_ap_pdu_c& other);
  sbc_ap_pdu_c& operator=(const sbc_ap_pdu_c& other);
  ~sbc_ap_pdu_c() { destroy_(); }
  void                           set(types::options e = types::nulltype);
  types                          type() const { return type_; }
  SRSASN_CODE                    pack(bit_ref& bref) const;
  SRSASN_CODE                    unpack(cbit_ref& bref);
  void                           to_json(json_writer& j) const;
  sbc_ap_init_msg_s&             set_init_msg();
  sbc_ap_successful_outcome_s&   set_successful_outcome();
  sbc_ap_unsuccessful_outcome_s& set_unsuccessful_outcome();
  sbc_ap_init_msg_s& init_msg()
  {
    assert_choice_type(types::init_msg, type_, "SBC-AP-PDU");
    return c.get<sbc_ap_init_msg_s>();
  }
  sbc_ap_successful_outcome_s& successful_outcome()
  {
    assert_choice_type(types::successful_outcome, type_, "SBC-AP-PDU");
    return c.get<sbc_ap_successful_outcome_s>();
  }
  const sbc_ap_init_msg_s& init_msg() const
  {
    assert_choice_type(types::init_msg, type_, "SBC-AP-PDU");
    return c.get<sbc_ap_init_msg_s>();
  }
  const sbc_ap_successful_outcome_s& successful_outcome() const
  {
    assert_choice_type(types::successful_outcome, type_, "SBC-AP-PDU");
    return c.get<sbc_ap_successful_outcome_s>();
  }

private:
  types type_;
  choice_buffer_t<sbc_ap_init_msg_s, sbc_ap_successful_outcome_s, sbc_ap_unsuccessful_outcome_s> c;

  void destroy_();
};

} // namespace sbc_ap
} // namespace asn1

#endif // SRSASN1_SBC_AP_H
