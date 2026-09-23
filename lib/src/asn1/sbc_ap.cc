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

#include "srsran/asn1/sbc_ap.h"
#include <sstream>

using namespace asn1;
using namespace asn1::sbc_ap;

/*******************************************************************************
 *                                Enum to string
 ******************************************************************************/

const char* crit_opts::to_string() const
{
  static const char* options[] = {"reject", "ignore", "notify"};
  return convert_enum_idx(options, 3, value, "crit_e");
}

const char* presence_opts::to_string() const
{
  static const char* options[] = {"optional", "conditional", "mandatory"};
  return convert_enum_idx(options, 3, value, "presence_e");
}

/*******************************************************************************
 *                       protocol_ie_field_s<ies_set_paramT_>
 ******************************************************************************/

template <class ies_set_paramT_>
SRSASN_CODE protocol_ie_field_s<ies_set_paramT_>::pack(bit_ref& bref) const
{
  HANDLE_CODE(pack_integer(bref, id, (uint32_t)0u, (uint32_t)65535u, false, true));
  HANDLE_CODE(crit.pack(bref));
  HANDLE_CODE(value.pack(bref));

  return SRSASN_SUCCESS;
}
template <class ies_set_paramT_>
SRSASN_CODE protocol_ie_field_s<ies_set_paramT_>::unpack(cbit_ref& bref)
{
  HANDLE_CODE(unpack_integer(id, bref, (uint32_t)0u, (uint32_t)65535u, false, true));
  HANDLE_CODE(crit.unpack(bref));
  value = ies_set_paramT_::get_value(id);
  HANDLE_CODE(value.unpack(bref));

  return SRSASN_SUCCESS;
}
template <class ies_set_paramT_>
void protocol_ie_field_s<ies_set_paramT_>::to_json(json_writer& j) const
{
  j.start_obj();
  j.write_int("id", id);
  j.write_str("criticality", crit.to_string());
  j.end_obj();
}
template <class ies_set_paramT_>
bool protocol_ie_field_s<ies_set_paramT_>::load_info_obj(const uint32_t& id_)
{
  if (not ies_set_paramT_::is_id_valid(id_)) {
    return false;
  }
  id    = id_;
  crit  = ies_set_paramT_::get_crit(id);
  value = ies_set_paramT_::get_value(id);
  return value.type().value != ies_set_paramT_::value_c::types_opts::nulltype;
}

/*******************************************************************************
 *                     protocol_ie_container_item_s<valueT_>
 ******************************************************************************/

template <class valueT_>
protocol_ie_container_item_s<valueT_>::protocol_ie_container_item_s(uint32_t id_, crit_e crit_) : id(id_), crit(crit_)
{}
template <class valueT_>
SRSASN_CODE protocol_ie_container_item_s<valueT_>::pack(bit_ref& bref) const
{
  HANDLE_CODE(pack_integer(bref, id, (uint32_t)0u, (uint32_t)65535u, false, true));
  HANDLE_CODE(crit.pack(bref));
  {
    varlength_field_pack_guard varlen_scope(bref, true);
    HANDLE_CODE(value.pack(bref));
  }
  return SRSASN_SUCCESS;
}
template <class valueT_>
SRSASN_CODE protocol_ie_container_item_s<valueT_>::unpack(cbit_ref& bref)
{
  HANDLE_CODE(unpack_integer(id, bref, (uint32_t)0u, (uint32_t)65535u, false, true));
  HANDLE_CODE(crit.unpack(bref));
  {
    varlength_field_unpack_guard varlen_scope(bref, true);
    HANDLE_CODE(value.unpack(bref));
  }
  return SRSASN_SUCCESS;
}
template <class valueT_>
void protocol_ie_container_item_s<valueT_>::to_json(json_writer& j) const
{
  j.start_obj();
  j.write_int("id", id);
  j.write_str("criticality", crit.to_string());
  j.end_obj();
}

/*******************************************************************************
 *                       Write-Replace-Warning-Request
 ******************************************************************************/

uint32_t write_replace_warning_request_ies_o::idx_to_id(uint32_t idx)
{
  static const uint32_t options[] = {5, 11, 10, 7, 18, 17, 3, 16, 46};
  return map_enum_number(options, 9, idx, "id");
}
bool write_replace_warning_request_ies_o::is_id_valid(const uint32_t& id)
{
  static const uint32_t options[] = {5, 11, 10, 7, 18, 17, 3, 16, 46};
  for (const auto& o : options) {
    if (o == id) {
      return true;
    }
  }
  return false;
}
crit_e write_replace_warning_request_ies_o::get_crit(const uint32_t& id)
{
  switch (id) {
    case 5:
      return crit_e::reject;
    case 11:
      return crit_e::reject;
    case 10:
      return crit_e::reject;
    case 7:
      return crit_e::reject;
    case 18:
      return crit_e::ignore;
    case 17:
      return crit_e::ignore;
    case 3:
      return crit_e::ignore;
    case 16:
      return crit_e::ignore;
    case 46:
      return crit_e::ignore;
    default:
      asn1::log_error("The id=%d is not recognized", id);
  }
  return {};
}
write_replace_warning_request_ies_o::value_c write_replace_warning_request_ies_o::get_value(const uint32_t& id)
{
  value_c ret{};
  switch (id) {
    case 5:
      ret.set(value_c::types::msg_id);
      break;
    case 11:
      ret.set(value_c::types::serial_num);
      break;
    case 10:
      ret.set(value_c::types::repeat_period);
      break;
    case 7:
      ret.set(value_c::types::nof_broadcasts_requested);
      break;
    case 18:
      ret.set(value_c::types::warning_type);
      break;
    case 17:
      ret.set(value_c::types::warning_security_info);
      break;
    case 3:
      ret.set(value_c::types::data_coding_scheme);
      break;
    case 16:
      ret.set(value_c::types::warning_msg_content);
      break;
    case 46:
      ret.set(value_c::types::warning_area_coordinates);
      break;
    default:
      asn1::log_error("The id=%d is not recognized", id);
  }
  return ret;
}
presence_e write_replace_warning_request_ies_o::get_presence(const uint32_t& id)
{
  switch (id) {
    case 5:
      return presence_e::mandatory;
    case 11:
      return presence_e::mandatory;
    case 10:
      return presence_e::mandatory;
    case 7:
      return presence_e::mandatory;
    case 18:
      return presence_e::optional;
    case 17:
      return presence_e::optional;
    case 3:
      return presence_e::optional;
    case 16:
      return presence_e::optional;
    case 46:
      return presence_e::optional;
    default:
      asn1::log_error("The id=%d is not recognized", id);
  }
  return {};
}

void write_replace_warning_request_ies_o::value_c::destroy_()
{
  switch (type_) {
    case types::msg_id:
      c.destroy<fixed_bitstring<16, false, true> >();
      break;
    case types::serial_num:
      c.destroy<fixed_bitstring<16, false, true> >();
      break;
    case types::warning_type:
      c.destroy<fixed_octstring<2, true> >();
      break;
    case types::warning_security_info:
      c.destroy<fixed_octstring<50, true> >();
      break;
    case types::data_coding_scheme:
      c.destroy<fixed_bitstring<8, false, true> >();
      break;
    case types::warning_msg_content:
      c.destroy<bounded_octstring<1, 9600, true> >();
      break;
    case types::warning_area_coordinates:
      c.destroy<bounded_octstring<1, 1024, true> >();
      break;
    default:
      break;
  }
}
void write_replace_warning_request_ies_o::value_c::set(types::options e)
{
  destroy_();
  type_ = e;
  switch (type_) {
    case types::msg_id:
      c.init<fixed_bitstring<16, false, true> >();
      break;
    case types::serial_num:
      c.init<fixed_bitstring<16, false, true> >();
      break;
    case types::repeat_period:
      break;
    case types::nof_broadcasts_requested:
      break;
    case types::warning_type:
      c.init<fixed_octstring<2, true> >();
      break;
    case types::warning_security_info:
      c.init<fixed_octstring<50, true> >();
      break;
    case types::data_coding_scheme:
      c.init<fixed_bitstring<8, false, true> >();
      break;
    case types::warning_msg_content:
      c.init<bounded_octstring<1, 9600, true> >();
      break;
    case types::warning_area_coordinates:
      c.init<bounded_octstring<1, 1024, true> >();
      break;
    case types::nulltype:
      break;
    default:
      log_invalid_choice_id(type_, "write_replace_warning_request_ies_o::value_c");
  }
}
write_replace_warning_request_ies_o::value_c::value_c(const write_replace_warning_request_ies_o::value_c& other)
{
  type_ = other.type();
  switch (type_) {
    case types::msg_id:
      c.init(other.c.get<fixed_bitstring<16, false, true> >());
      break;
    case types::serial_num:
      c.init(other.c.get<fixed_bitstring<16, false, true> >());
      break;
    case types::repeat_period:
      c.init(other.c.get<uint16_t>());
      break;
    case types::nof_broadcasts_requested:
      c.init(other.c.get<uint32_t>());
      break;
    case types::warning_type:
      c.init(other.c.get<fixed_octstring<2, true> >());
      break;
    case types::warning_security_info:
      c.init(other.c.get<fixed_octstring<50, true> >());
      break;
    case types::data_coding_scheme:
      c.init(other.c.get<fixed_bitstring<8, false, true> >());
      break;
    case types::warning_msg_content:
      c.init(other.c.get<bounded_octstring<1, 9600, true> >());
      break;
    case types::warning_area_coordinates:
      c.init(other.c.get<bounded_octstring<1, 1024, true> >());
      break;
    case types::nulltype:
      break;
    default:
      log_invalid_choice_id(type_, "write_replace_warning_request_ies_o::value_c");
  }
}
write_replace_warning_request_ies_o::value_c&
write_replace_warning_request_ies_o::value_c::operator=(const write_replace_warning_request_ies_o::value_c& other)
{
  if (this == &other) {
    return *this;
  }
  set(other.type());
  switch (type_) {
    case types::msg_id:
      c.set(other.c.get<fixed_bitstring<16, false, true> >());
      break;
    case types::serial_num:
      c.set(other.c.get<fixed_bitstring<16, false, true> >());
      break;
    case types::repeat_period:
      c.set(other.c.get<uint16_t>());
      break;
    case types::nof_broadcasts_requested:
      c.set(other.c.get<uint32_t>());
      break;
    case types::warning_type:
      c.set(other.c.get<fixed_octstring<2, true> >());
      break;
    case types::warning_security_info:
      c.set(other.c.get<fixed_octstring<50, true> >());
      break;
    case types::data_coding_scheme:
      c.set(other.c.get<fixed_bitstring<8, false, true> >());
      break;
    case types::warning_msg_content:
      c.set(other.c.get<bounded_octstring<1, 9600, true> >());
      break;
    case types::warning_area_coordinates:
      c.set(other.c.get<bounded_octstring<1, 1024, true> >());
      break;
    case types::nulltype:
      break;
    default:
      log_invalid_choice_id(type_, "write_replace_warning_request_ies_o::value_c");
  }
  return *this;
}
fixed_bitstring<16, false, true>& write_replace_warning_request_ies_o::value_c::msg_id()
{
  assert_choice_type(types::msg_id, type_, "Value");
  return c.get<fixed_bitstring<16, false, true> >();
}
fixed_bitstring<16, false, true>& write_replace_warning_request_ies_o::value_c::serial_num()
{
  assert_choice_type(types::serial_num, type_, "Value");
  return c.get<fixed_bitstring<16, false, true> >();
}
uint16_t& write_replace_warning_request_ies_o::value_c::repeat_period()
{
  assert_choice_type(types::repeat_period, type_, "Value");
  return c.get<uint16_t>();
}
uint32_t& write_replace_warning_request_ies_o::value_c::nof_broadcasts_requested()
{
  assert_choice_type(types::nof_broadcasts_requested, type_, "Value");
  return c.get<uint32_t>();
}
fixed_octstring<2, true>& write_replace_warning_request_ies_o::value_c::warning_type()
{
  assert_choice_type(types::warning_type, type_, "Value");
  return c.get<fixed_octstring<2, true> >();
}
fixed_octstring<50, true>& write_replace_warning_request_ies_o::value_c::warning_security_info()
{
  assert_choice_type(types::warning_security_info, type_, "Value");
  return c.get<fixed_octstring<50, true> >();
}
fixed_bitstring<8, false, true>& write_replace_warning_request_ies_o::value_c::data_coding_scheme()
{
  assert_choice_type(types::data_coding_scheme, type_, "Value");
  return c.get<fixed_bitstring<8, false, true> >();
}
bounded_octstring<1, 9600, true>& write_replace_warning_request_ies_o::value_c::warning_msg_content()
{
  assert_choice_type(types::warning_msg_content, type_, "Value");
  return c.get<bounded_octstring<1, 9600, true> >();
}
bounded_octstring<1, 1024, true>& write_replace_warning_request_ies_o::value_c::warning_area_coordinates()
{
  assert_choice_type(types::warning_area_coordinates, type_, "Value");
  return c.get<bounded_octstring<1, 1024, true> >();
}
const fixed_bitstring<16, false, true>& write_replace_warning_request_ies_o::value_c::msg_id() const
{
  assert_choice_type(types::msg_id, type_, "Value");
  return c.get<fixed_bitstring<16, false, true> >();
}
const fixed_bitstring<16, false, true>& write_replace_warning_request_ies_o::value_c::serial_num() const
{
  assert_choice_type(types::serial_num, type_, "Value");
  return c.get<fixed_bitstring<16, false, true> >();
}
const uint16_t& write_replace_warning_request_ies_o::value_c::repeat_period() const
{
  assert_choice_type(types::repeat_period, type_, "Value");
  return c.get<uint16_t>();
}
const uint32_t& write_replace_warning_request_ies_o::value_c::nof_broadcasts_requested() const
{
  assert_choice_type(types::nof_broadcasts_requested, type_, "Value");
  return c.get<uint32_t>();
}
const fixed_octstring<2, true>& write_replace_warning_request_ies_o::value_c::warning_type() const
{
  assert_choice_type(types::warning_type, type_, "Value");
  return c.get<fixed_octstring<2, true> >();
}
const fixed_octstring<50, true>& write_replace_warning_request_ies_o::value_c::warning_security_info() const
{
  assert_choice_type(types::warning_security_info, type_, "Value");
  return c.get<fixed_octstring<50, true> >();
}
const fixed_bitstring<8, false, true>& write_replace_warning_request_ies_o::value_c::data_coding_scheme() const
{
  assert_choice_type(types::data_coding_scheme, type_, "Value");
  return c.get<fixed_bitstring<8, false, true> >();
}
const bounded_octstring<1, 9600, true>& write_replace_warning_request_ies_o::value_c::warning_msg_content() const
{
  assert_choice_type(types::warning_msg_content, type_, "Value");
  return c.get<bounded_octstring<1, 9600, true> >();
}
const bounded_octstring<1, 1024, true>&
write_replace_warning_request_ies_o::value_c::warning_area_coordinates() const
{
  assert_choice_type(types::warning_area_coordinates, type_, "Value");
  return c.get<bounded_octstring<1, 1024, true> >();
}
void write_replace_warning_request_ies_o::value_c::to_json(json_writer& j) const
{
  j.start_obj();
  switch (type_) {
    case types::msg_id:
      j.write_str("BIT STRING", c.get<fixed_bitstring<16, false, true> >().to_string());
      break;
    case types::serial_num:
      j.write_str("BIT STRING", c.get<fixed_bitstring<16, false, true> >().to_string());
      break;
    case types::repeat_period:
      j.write_int("INTEGER (0..4096)", c.get<uint16_t>());
      break;
    case types::nof_broadcasts_requested:
      j.write_int("INTEGER (0..65535)", c.get<uint32_t>());
      break;
    case types::warning_type:
      j.write_str("OCTET STRING", c.get<fixed_octstring<2, true> >().to_string());
      break;
    case types::warning_security_info:
      j.write_str("OCTET STRING", c.get<fixed_octstring<50, true> >().to_string());
      break;
    case types::data_coding_scheme:
      j.write_str("BIT STRING", c.get<fixed_bitstring<8, false, true> >().to_string());
      break;
    case types::warning_msg_content:
      j.write_str("OCTET STRING", c.get<bounded_octstring<1, 9600, true> >().to_string());
      break;
    case types::warning_area_coordinates:
      j.write_str("OCTET STRING", c.get<bounded_octstring<1, 1024, true> >().to_string());
      break;
    default:
      log_invalid_choice_id(type_, "write_replace_warning_request_ies_o::value_c");
  }
  j.end_obj();
}
SRSASN_CODE write_replace_warning_request_ies_o::value_c::pack(bit_ref& bref) const
{
  varlength_field_pack_guard varlen_scope(bref, true);
  switch (type_) {
    case types::msg_id:
      HANDLE_CODE((c.get<fixed_bitstring<16, false, true> >().pack(bref)));
      break;
    case types::serial_num:
      HANDLE_CODE((c.get<fixed_bitstring<16, false, true> >().pack(bref)));
      break;
    case types::repeat_period:
      HANDLE_CODE(pack_integer(bref, c.get<uint16_t>(), (uint16_t)0u, (uint16_t)4096u, false, true));
      break;
    case types::nof_broadcasts_requested:
      HANDLE_CODE(pack_integer(bref, c.get<uint32_t>(), (uint32_t)0u, (uint32_t)65535u, false, true));
      break;
    case types::warning_type:
      HANDLE_CODE((c.get<fixed_octstring<2, true> >().pack(bref)));
      break;
    case types::warning_security_info:
      HANDLE_CODE((c.get<fixed_octstring<50, true> >().pack(bref)));
      break;
    case types::data_coding_scheme:
      HANDLE_CODE((c.get<fixed_bitstring<8, false, true> >().pack(bref)));
      break;
    case types::warning_msg_content:
      HANDLE_CODE((c.get<bounded_octstring<1, 9600, true> >().pack(bref)));
      break;
    case types::warning_area_coordinates:
      HANDLE_CODE((c.get<bounded_octstring<1, 1024, true> >().pack(bref)));
      break;
    default:
      log_invalid_choice_id(type_, "write_replace_warning_request_ies_o::value_c");
      return SRSASN_ERROR_ENCODE_FAIL;
  }
  return SRSASN_SUCCESS;
}
SRSASN_CODE write_replace_warning_request_ies_o::value_c::unpack(cbit_ref& bref)
{
  varlength_field_unpack_guard varlen_scope(bref, true);
  switch (type_) {
    case types::msg_id:
      HANDLE_CODE((c.get<fixed_bitstring<16, false, true> >().unpack(bref)));
      break;
    case types::serial_num:
      HANDLE_CODE((c.get<fixed_bitstring<16, false, true> >().unpack(bref)));
      break;
    case types::repeat_period:
      HANDLE_CODE(unpack_integer(c.get<uint16_t>(), bref, (uint16_t)0u, (uint16_t)4096u, false, true));
      break;
    case types::nof_broadcasts_requested:
      HANDLE_CODE(unpack_integer(c.get<uint32_t>(), bref, (uint32_t)0u, (uint32_t)65535u, false, true));
      break;
    case types::warning_type:
      HANDLE_CODE((c.get<fixed_octstring<2, true> >().unpack(bref)));
      break;
    case types::warning_security_info:
      HANDLE_CODE((c.get<fixed_octstring<50, true> >().unpack(bref)));
      break;
    case types::data_coding_scheme:
      HANDLE_CODE((c.get<fixed_bitstring<8, false, true> >().unpack(bref)));
      break;
    case types::warning_msg_content:
      HANDLE_CODE((c.get<bounded_octstring<1, 9600, true> >().unpack(bref)));
      break;
    case types::warning_area_coordinates:
      HANDLE_CODE((c.get<bounded_octstring<1, 1024, true> >().unpack(bref)));
      break;
    default:
      log_invalid_choice_id(type_, "write_replace_warning_request_ies_o::value_c");
      return SRSASN_ERROR_DECODE_FAIL;
  }
  return SRSASN_SUCCESS;
}
const char* write_replace_warning_request_ies_o::value_c::types_opts::to_string() const
{
  static const char* options[] = {"Message-Identifier",
                                   "Serial-Number",
                                   "INTEGER (0..4096)",
                                   "INTEGER (0..65535)",
                                   "Warning-Type",
                                   "Warning-Security-Information",
                                   "Data-Coding-Scheme",
                                   "Warning-Message-Content",
                                   "Warning-Area-Coordinates"};
  return convert_enum_idx(options, 9, value, "write_replace_warning_request_ies_o::value_c::types");
}

write_replace_warning_request_ies_container::write_replace_warning_request_ies_container() :
  msg_id(5, crit_e::reject),
  serial_num(11, crit_e::reject),
  repeat_period(10, crit_e::reject),
  nof_broadcasts_requested(7, crit_e::reject),
  warning_type(18, crit_e::ignore),
  warning_security_info(17, crit_e::ignore),
  data_coding_scheme(3, crit_e::ignore),
  warning_msg_content(16, crit_e::ignore),
  warning_area_coordinates(46, crit_e::ignore)
{}
SRSASN_CODE write_replace_warning_request_ies_container::pack(bit_ref& bref) const
{
  uint32_t nof_ies = 4;
  nof_ies += warning_type_present ? 1 : 0;
  nof_ies += warning_security_info_present ? 1 : 0;
  nof_ies += data_coding_scheme_present ? 1 : 0;
  nof_ies += warning_msg_content_present ? 1 : 0;
  nof_ies += warning_area_coordinates_present ? 1 : 0;
  pack_length(bref, nof_ies, 0u, 65535u, true);

  HANDLE_CODE(msg_id.pack(bref));
  HANDLE_CODE(serial_num.pack(bref));
  HANDLE_CODE(repeat_period.pack(bref));
  HANDLE_CODE(nof_broadcasts_requested.pack(bref));
  if (warning_type_present) {
    HANDLE_CODE(warning_type.pack(bref));
  }
  if (warning_security_info_present) {
    HANDLE_CODE(warning_security_info.pack(bref));
  }
  if (data_coding_scheme_present) {
    HANDLE_CODE(data_coding_scheme.pack(bref));
  }
  if (warning_msg_content_present) {
    HANDLE_CODE(warning_msg_content.pack(bref));
  }
  if (warning_area_coordinates_present) {
    HANDLE_CODE(warning_area_coordinates.pack(bref));
  }

  return SRSASN_SUCCESS;
}
SRSASN_CODE write_replace_warning_request_ies_container::unpack(cbit_ref& bref)
{
  uint32_t nof_ies = 0;
  unpack_length(nof_ies, bref, 0u, 65535u, true);

  uint32_t nof_mandatory_ies = 4;

  for (; nof_ies > 0; --nof_ies) {
    protocol_ie_field_s<write_replace_warning_request_ies_o> c;
    HANDLE_CODE(c.unpack(bref));
    switch (c.id) {
      case 5:
        nof_mandatory_ies--;
        msg_id.id    = c.id;
        msg_id.crit  = c.crit;
        msg_id.value = c.value.msg_id();
        break;
      case 11:
        nof_mandatory_ies--;
        serial_num.id    = c.id;
        serial_num.crit  = c.crit;
        serial_num.value = c.value.serial_num();
        break;
      case 10:
        nof_mandatory_ies--;
        repeat_period.id    = c.id;
        repeat_period.crit  = c.crit;
        repeat_period.value = c.value.repeat_period();
        break;
      case 7:
        nof_mandatory_ies--;
        nof_broadcasts_requested.id    = c.id;
        nof_broadcasts_requested.crit  = c.crit;
        nof_broadcasts_requested.value = c.value.nof_broadcasts_requested();
        break;
      case 18:
        warning_type_present = true;
        warning_type.id      = c.id;
        warning_type.crit    = c.crit;
        warning_type.value   = c.value.warning_type();
        break;
      case 17:
        warning_security_info_present = true;
        warning_security_info.id      = c.id;
        warning_security_info.crit    = c.crit;
        warning_security_info.value   = c.value.warning_security_info();
        break;
      case 3:
        data_coding_scheme_present = true;
        data_coding_scheme.id      = c.id;
        data_coding_scheme.crit    = c.crit;
        data_coding_scheme.value   = c.value.data_coding_scheme();
        break;
      case 16:
        warning_msg_content_present = true;
        warning_msg_content.id      = c.id;
        warning_msg_content.crit    = c.crit;
        warning_msg_content.value   = c.value.warning_msg_content();
        break;
      case 46:
        warning_area_coordinates_present = true;
        warning_area_coordinates.id      = c.id;
        warning_area_coordinates.crit    = c.crit;
        warning_area_coordinates.value   = c.value.warning_area_coordinates();
        break;
      default:
        asn1::log_error("Unpacked object ID=%d is not recognized\n", c.id);
        return SRSASN_ERROR_DECODE_FAIL;
    }
  }
  if (nof_mandatory_ies > 0) {
    asn1::log_error("Mandatory fields are missing\n");
    return SRSASN_ERROR_DECODE_FAIL;
  }
  return SRSASN_SUCCESS;
}
void write_replace_warning_request_ies_container::to_json(json_writer& j) const
{
  j.start_obj();
  j.write_fieldname("");
  msg_id.to_json(j);
  j.write_fieldname("");
  serial_num.to_json(j);
  j.write_fieldname("");
  repeat_period.to_json(j);
  j.write_fieldname("");
  nof_broadcasts_requested.to_json(j);
  if (warning_type_present) {
    j.write_fieldname("");
    warning_type.to_json(j);
  }
  if (warning_security_info_present) {
    j.write_fieldname("");
    warning_security_info.to_json(j);
  }
  if (data_coding_scheme_present) {
    j.write_fieldname("");
    data_coding_scheme.to_json(j);
  }
  if (warning_msg_content_present) {
    j.write_fieldname("");
    warning_msg_content.to_json(j);
  }
  if (warning_area_coordinates_present) {
    j.write_fieldname("");
    warning_area_coordinates.to_json(j);
  }
  j.end_obj();
}

SRSASN_CODE write_replace_warning_request_s::pack(bit_ref& bref) const
{
  bref.pack(ext, 1);
  HANDLE_CODE(protocol_ies.pack(bref));

  return SRSASN_SUCCESS;
}
SRSASN_CODE write_replace_warning_request_s::unpack(cbit_ref& bref)
{
  bref.unpack(ext, 1);
  HANDLE_CODE(protocol_ies.unpack(bref));

  return SRSASN_SUCCESS;
}
void write_replace_warning_request_s::to_json(json_writer& j) const
{
  j.start_obj();
  j.write_fieldname("protocolIEs");
  protocol_ies.to_json(j);
  j.end_obj();
}

/*******************************************************************************
 *                       Write-Replace-Warning-Response
 ******************************************************************************/

uint32_t write_replace_warning_resp_ies_o::idx_to_id(uint32_t idx)
{
  static const uint32_t options[] = {5, 11, 1};
  return map_enum_number(options, 3, idx, "id");
}
bool write_replace_warning_resp_ies_o::is_id_valid(const uint32_t& id)
{
  static const uint32_t options[] = {5, 11, 1};
  for (const auto& o : options) {
    if (o == id) {
      return true;
    }
  }
  return false;
}
crit_e write_replace_warning_resp_ies_o::get_crit(const uint32_t& id)
{
  switch (id) {
    case 5:
      return crit_e::reject;
    case 11:
      return crit_e::reject;
    case 1:
      return crit_e::reject;
    default:
      asn1::log_error("The id=%d is not recognized", id);
  }
  return {};
}
write_replace_warning_resp_ies_o::value_c write_replace_warning_resp_ies_o::get_value(const uint32_t& id)
{
  value_c ret{};
  switch (id) {
    case 5:
      ret.set(value_c::types::msg_id);
      break;
    case 11:
      ret.set(value_c::types::serial_num);
      break;
    case 1:
      ret.set(value_c::types::cause);
      break;
    default:
      asn1::log_error("The id=%d is not recognized", id);
  }
  return ret;
}
presence_e write_replace_warning_resp_ies_o::get_presence(const uint32_t& id)
{
  switch (id) {
    case 5:
      return presence_e::mandatory;
    case 11:
      return presence_e::mandatory;
    case 1:
      return presence_e::mandatory;
    default:
      asn1::log_error("The id=%d is not recognized", id);
  }
  return {};
}

void write_replace_warning_resp_ies_o::value_c::destroy_()
{
  switch (type_) {
    case types::msg_id:
      c.destroy<fixed_bitstring<16, false, true> >();
      break;
    case types::serial_num:
      c.destroy<fixed_bitstring<16, false, true> >();
      break;
    default:
      break;
  }
}
void write_replace_warning_resp_ies_o::value_c::set(types::options e)
{
  destroy_();
  type_ = e;
  switch (type_) {
    case types::msg_id:
      c.init<fixed_bitstring<16, false, true> >();
      break;
    case types::serial_num:
      c.init<fixed_bitstring<16, false, true> >();
      break;
    case types::cause:
      break;
    case types::nulltype:
      break;
    default:
      log_invalid_choice_id(type_, "write_replace_warning_resp_ies_o::value_c");
  }
}
write_replace_warning_resp_ies_o::value_c::value_c(const write_replace_warning_resp_ies_o::value_c& other)
{
  type_ = other.type();
  switch (type_) {
    case types::msg_id:
      c.init(other.c.get<fixed_bitstring<16, false, true> >());
      break;
    case types::serial_num:
      c.init(other.c.get<fixed_bitstring<16, false, true> >());
      break;
    case types::cause:
      c.init(other.c.get<uint16_t>());
      break;
    case types::nulltype:
      break;
    default:
      log_invalid_choice_id(type_, "write_replace_warning_resp_ies_o::value_c");
  }
}
write_replace_warning_resp_ies_o::value_c&
write_replace_warning_resp_ies_o::value_c::operator=(const write_replace_warning_resp_ies_o::value_c& other)
{
  if (this == &other) {
    return *this;
  }
  set(other.type());
  switch (type_) {
    case types::msg_id:
      c.set(other.c.get<fixed_bitstring<16, false, true> >());
      break;
    case types::serial_num:
      c.set(other.c.get<fixed_bitstring<16, false, true> >());
      break;
    case types::cause:
      c.set(other.c.get<uint16_t>());
      break;
    case types::nulltype:
      break;
    default:
      log_invalid_choice_id(type_, "write_replace_warning_resp_ies_o::value_c");
  }
  return *this;
}
fixed_bitstring<16, false, true>& write_replace_warning_resp_ies_o::value_c::msg_id()
{
  assert_choice_type(types::msg_id, type_, "Value");
  return c.get<fixed_bitstring<16, false, true> >();
}
fixed_bitstring<16, false, true>& write_replace_warning_resp_ies_o::value_c::serial_num()
{
  assert_choice_type(types::serial_num, type_, "Value");
  return c.get<fixed_bitstring<16, false, true> >();
}
uint16_t& write_replace_warning_resp_ies_o::value_c::cause()
{
  assert_choice_type(types::cause, type_, "Value");
  return c.get<uint16_t>();
}
const fixed_bitstring<16, false, true>& write_replace_warning_resp_ies_o::value_c::msg_id() const
{
  assert_choice_type(types::msg_id, type_, "Value");
  return c.get<fixed_bitstring<16, false, true> >();
}
const fixed_bitstring<16, false, true>& write_replace_warning_resp_ies_o::value_c::serial_num() const
{
  assert_choice_type(types::serial_num, type_, "Value");
  return c.get<fixed_bitstring<16, false, true> >();
}
const uint16_t& write_replace_warning_resp_ies_o::value_c::cause() const
{
  assert_choice_type(types::cause, type_, "Value");
  return c.get<uint16_t>();
}
void write_replace_warning_resp_ies_o::value_c::to_json(json_writer& j) const
{
  j.start_obj();
  switch (type_) {
    case types::msg_id:
      j.write_str("BIT STRING", c.get<fixed_bitstring<16, false, true> >().to_string());
      break;
    case types::serial_num:
      j.write_str("BIT STRING", c.get<fixed_bitstring<16, false, true> >().to_string());
      break;
    case types::cause:
      j.write_int("INTEGER (0..255)", c.get<uint16_t>());
      break;
    default:
      log_invalid_choice_id(type_, "write_replace_warning_resp_ies_o::value_c");
  }
  j.end_obj();
}
SRSASN_CODE write_replace_warning_resp_ies_o::value_c::pack(bit_ref& bref) const
{
  varlength_field_pack_guard varlen_scope(bref, true);
  switch (type_) {
    case types::msg_id:
      HANDLE_CODE((c.get<fixed_bitstring<16, false, true> >().pack(bref)));
      break;
    case types::serial_num:
      HANDLE_CODE((c.get<fixed_bitstring<16, false, true> >().pack(bref)));
      break;
    case types::cause:
      HANDLE_CODE(pack_integer(bref, c.get<uint16_t>(), (uint16_t)0u, (uint16_t)255u, false, true));
      break;
    default:
      log_invalid_choice_id(type_, "write_replace_warning_resp_ies_o::value_c");
      return SRSASN_ERROR_ENCODE_FAIL;
  }
  return SRSASN_SUCCESS;
}
SRSASN_CODE write_replace_warning_resp_ies_o::value_c::unpack(cbit_ref& bref)
{
  varlength_field_unpack_guard varlen_scope(bref, true);
  switch (type_) {
    case types::msg_id:
      HANDLE_CODE((c.get<fixed_bitstring<16, false, true> >().unpack(bref)));
      break;
    case types::serial_num:
      HANDLE_CODE((c.get<fixed_bitstring<16, false, true> >().unpack(bref)));
      break;
    case types::cause:
      HANDLE_CODE(unpack_integer(c.get<uint16_t>(), bref, (uint16_t)0u, (uint16_t)255u, false, true));
      break;
    default:
      log_invalid_choice_id(type_, "write_replace_warning_resp_ies_o::value_c");
      return SRSASN_ERROR_DECODE_FAIL;
  }
  return SRSASN_SUCCESS;
}
const char* write_replace_warning_resp_ies_o::value_c::types_opts::to_string() const
{
  static const char* options[] = {"Message-Identifier", "Serial-Number", "INTEGER (0..255)"};
  return convert_enum_idx(options, 3, value, "write_replace_warning_resp_ies_o::value_c::types");
}

write_replace_warning_resp_ies_container::write_replace_warning_resp_ies_container() :
  msg_id(5, crit_e::reject), serial_num(11, crit_e::reject), cause(1, crit_e::reject)
{}
SRSASN_CODE write_replace_warning_resp_ies_container::pack(bit_ref& bref) const
{
  pack_length(bref, (uint32_t)3u, 0u, 65535u, true);

  HANDLE_CODE(msg_id.pack(bref));
  HANDLE_CODE(serial_num.pack(bref));
  HANDLE_CODE(cause.pack(bref));

  return SRSASN_SUCCESS;
}
SRSASN_CODE write_replace_warning_resp_ies_container::unpack(cbit_ref& bref)
{
  uint32_t nof_ies = 0;
  unpack_length(nof_ies, bref, 0u, 65535u, true);

  uint32_t nof_mandatory_ies = 3;

  for (; nof_ies > 0; --nof_ies) {
    protocol_ie_field_s<write_replace_warning_resp_ies_o> c;
    HANDLE_CODE(c.unpack(bref));
    switch (c.id) {
      case 5:
        nof_mandatory_ies--;
        msg_id.id    = c.id;
        msg_id.crit  = c.crit;
        msg_id.value = c.value.msg_id();
        break;
      case 11:
        nof_mandatory_ies--;
        serial_num.id    = c.id;
        serial_num.crit  = c.crit;
        serial_num.value = c.value.serial_num();
        break;
      case 1:
        nof_mandatory_ies--;
        cause.id    = c.id;
        cause.crit  = c.crit;
        cause.value = c.value.cause();
        break;
      default:
        asn1::log_error("Unpacked object ID=%d is not recognized\n", c.id);
        return SRSASN_ERROR_DECODE_FAIL;
    }
  }
  if (nof_mandatory_ies > 0) {
    asn1::log_error("Mandatory fields are missing\n");
    return SRSASN_ERROR_DECODE_FAIL;
  }
  return SRSASN_SUCCESS;
}
void write_replace_warning_resp_ies_container::to_json(json_writer& j) const
{
  j.start_obj();
  j.write_fieldname("");
  msg_id.to_json(j);
  j.write_fieldname("");
  serial_num.to_json(j);
  j.write_fieldname("");
  cause.to_json(j);
  j.end_obj();
}

SRSASN_CODE write_replace_warning_resp_s::pack(bit_ref& bref) const
{
  bref.pack(ext, 1);
  HANDLE_CODE(protocol_ies.pack(bref));

  return SRSASN_SUCCESS;
}
SRSASN_CODE write_replace_warning_resp_s::unpack(cbit_ref& bref)
{
  bref.unpack(ext, 1);
  HANDLE_CODE(protocol_ies.unpack(bref));

  return SRSASN_SUCCESS;
}
void write_replace_warning_resp_s::to_json(json_writer& j) const
{
  j.start_obj();
  j.write_fieldname("protocolIEs");
  protocol_ies.to_json(j);
  j.end_obj();
}

/*******************************************************************************
 *                             Stop-Warning-Request
 ******************************************************************************/

uint32_t stop_warning_request_ies_o::idx_to_id(uint32_t idx)
{
  static const uint32_t options[] = {5, 11};
  return map_enum_number(options, 2, idx, "id");
}
bool stop_warning_request_ies_o::is_id_valid(const uint32_t& id)
{
  static const uint32_t options[] = {5, 11};
  for (const auto& o : options) {
    if (o == id) {
      return true;
    }
  }
  return false;
}
crit_e stop_warning_request_ies_o::get_crit(const uint32_t& id)
{
  switch (id) {
    case 5:
      return crit_e::reject;
    case 11:
      return crit_e::reject;
    default:
      asn1::log_error("The id=%d is not recognized", id);
  }
  return {};
}
stop_warning_request_ies_o::value_c stop_warning_request_ies_o::get_value(const uint32_t& id)
{
  value_c ret{};
  switch (id) {
    case 5:
      ret.set(value_c::types::msg_id);
      break;
    case 11:
      ret.set(value_c::types::serial_num);
      break;
    default:
      asn1::log_error("The id=%d is not recognized", id);
  }
  return ret;
}
presence_e stop_warning_request_ies_o::get_presence(const uint32_t& id)
{
  switch (id) {
    case 5:
      return presence_e::mandatory;
    case 11:
      return presence_e::mandatory;
    default:
      asn1::log_error("The id=%d is not recognized", id);
  }
  return {};
}

void stop_warning_request_ies_o::value_c::destroy_()
{
  switch (type_) {
    case types::msg_id:
      c.destroy<fixed_bitstring<16, false, true> >();
      break;
    case types::serial_num:
      c.destroy<fixed_bitstring<16, false, true> >();
      break;
    default:
      break;
  }
}
void stop_warning_request_ies_o::value_c::set(types::options e)
{
  destroy_();
  type_ = e;
  switch (type_) {
    case types::msg_id:
      c.init<fixed_bitstring<16, false, true> >();
      break;
    case types::serial_num:
      c.init<fixed_bitstring<16, false, true> >();
      break;
    case types::nulltype:
      break;
    default:
      log_invalid_choice_id(type_, "stop_warning_request_ies_o::value_c");
  }
}
stop_warning_request_ies_o::value_c::value_c(const stop_warning_request_ies_o::value_c& other)
{
  type_ = other.type();
  switch (type_) {
    case types::msg_id:
      c.init(other.c.get<fixed_bitstring<16, false, true> >());
      break;
    case types::serial_num:
      c.init(other.c.get<fixed_bitstring<16, false, true> >());
      break;
    case types::nulltype:
      break;
    default:
      log_invalid_choice_id(type_, "stop_warning_request_ies_o::value_c");
  }
}
stop_warning_request_ies_o::value_c&
stop_warning_request_ies_o::value_c::operator=(const stop_warning_request_ies_o::value_c& other)
{
  if (this == &other) {
    return *this;
  }
  set(other.type());
  switch (type_) {
    case types::msg_id:
      c.set(other.c.get<fixed_bitstring<16, false, true> >());
      break;
    case types::serial_num:
      c.set(other.c.get<fixed_bitstring<16, false, true> >());
      break;
    case types::nulltype:
      break;
    default:
      log_invalid_choice_id(type_, "stop_warning_request_ies_o::value_c");
  }
  return *this;
}
fixed_bitstring<16, false, true>& stop_warning_request_ies_o::value_c::msg_id()
{
  assert_choice_type(types::msg_id, type_, "Value");
  return c.get<fixed_bitstring<16, false, true> >();
}
fixed_bitstring<16, false, true>& stop_warning_request_ies_o::value_c::serial_num()
{
  assert_choice_type(types::serial_num, type_, "Value");
  return c.get<fixed_bitstring<16, false, true> >();
}
const fixed_bitstring<16, false, true>& stop_warning_request_ies_o::value_c::msg_id() const
{
  assert_choice_type(types::msg_id, type_, "Value");
  return c.get<fixed_bitstring<16, false, true> >();
}
const fixed_bitstring<16, false, true>& stop_warning_request_ies_o::value_c::serial_num() const
{
  assert_choice_type(types::serial_num, type_, "Value");
  return c.get<fixed_bitstring<16, false, true> >();
}
void stop_warning_request_ies_o::value_c::to_json(json_writer& j) const
{
  j.start_obj();
  switch (type_) {
    case types::msg_id:
      j.write_str("BIT STRING", c.get<fixed_bitstring<16, false, true> >().to_string());
      break;
    case types::serial_num:
      j.write_str("BIT STRING", c.get<fixed_bitstring<16, false, true> >().to_string());
      break;
    default:
      log_invalid_choice_id(type_, "stop_warning_request_ies_o::value_c");
  }
  j.end_obj();
}
SRSASN_CODE stop_warning_request_ies_o::value_c::pack(bit_ref& bref) const
{
  varlength_field_pack_guard varlen_scope(bref, true);
  switch (type_) {
    case types::msg_id:
      HANDLE_CODE((c.get<fixed_bitstring<16, false, true> >().pack(bref)));
      break;
    case types::serial_num:
      HANDLE_CODE((c.get<fixed_bitstring<16, false, true> >().pack(bref)));
      break;
    default:
      log_invalid_choice_id(type_, "stop_warning_request_ies_o::value_c");
      return SRSASN_ERROR_ENCODE_FAIL;
  }
  return SRSASN_SUCCESS;
}
SRSASN_CODE stop_warning_request_ies_o::value_c::unpack(cbit_ref& bref)
{
  varlength_field_unpack_guard varlen_scope(bref, true);
  switch (type_) {
    case types::msg_id:
      HANDLE_CODE((c.get<fixed_bitstring<16, false, true> >().unpack(bref)));
      break;
    case types::serial_num:
      HANDLE_CODE((c.get<fixed_bitstring<16, false, true> >().unpack(bref)));
      break;
    default:
      log_invalid_choice_id(type_, "stop_warning_request_ies_o::value_c");
      return SRSASN_ERROR_DECODE_FAIL;
  }
  return SRSASN_SUCCESS;
}
const char* stop_warning_request_ies_o::value_c::types_opts::to_string() const
{
  static const char* options[] = {"Message-Identifier", "Serial-Number"};
  return convert_enum_idx(options, 2, value, "stop_warning_request_ies_o::value_c::types");
}

stop_warning_request_ies_container::stop_warning_request_ies_container() :
  msg_id(5, crit_e::reject), serial_num(11, crit_e::reject)
{}
SRSASN_CODE stop_warning_request_ies_container::pack(bit_ref& bref) const
{
  pack_length(bref, (uint32_t)2u, 0u, 65535u, true);

  HANDLE_CODE(msg_id.pack(bref));
  HANDLE_CODE(serial_num.pack(bref));

  return SRSASN_SUCCESS;
}
SRSASN_CODE stop_warning_request_ies_container::unpack(cbit_ref& bref)
{
  uint32_t nof_ies = 0;
  unpack_length(nof_ies, bref, 0u, 65535u, true);

  uint32_t nof_mandatory_ies = 2;

  for (; nof_ies > 0; --nof_ies) {
    protocol_ie_field_s<stop_warning_request_ies_o> c;
    HANDLE_CODE(c.unpack(bref));
    switch (c.id) {
      case 5:
        nof_mandatory_ies--;
        msg_id.id    = c.id;
        msg_id.crit  = c.crit;
        msg_id.value = c.value.msg_id();
        break;
      case 11:
        nof_mandatory_ies--;
        serial_num.id    = c.id;
        serial_num.crit  = c.crit;
        serial_num.value = c.value.serial_num();
        break;
      default:
        asn1::log_error("Unpacked object ID=%d is not recognized\n", c.id);
        return SRSASN_ERROR_DECODE_FAIL;
    }
  }
  if (nof_mandatory_ies > 0) {
    asn1::log_error("Mandatory fields are missing\n");
    return SRSASN_ERROR_DECODE_FAIL;
  }
  return SRSASN_SUCCESS;
}
void stop_warning_request_ies_container::to_json(json_writer& j) const
{
  j.start_obj();
  j.write_fieldname("");
  msg_id.to_json(j);
  j.write_fieldname("");
  serial_num.to_json(j);
  j.end_obj();
}

SRSASN_CODE stop_warning_request_s::pack(bit_ref& bref) const
{
  bref.pack(ext, 1);
  HANDLE_CODE(protocol_ies.pack(bref));

  return SRSASN_SUCCESS;
}
SRSASN_CODE stop_warning_request_s::unpack(cbit_ref& bref)
{
  bref.unpack(ext, 1);
  HANDLE_CODE(protocol_ies.unpack(bref));

  return SRSASN_SUCCESS;
}
void stop_warning_request_s::to_json(json_writer& j) const
{
  j.start_obj();
  j.write_fieldname("protocolIEs");
  protocol_ies.to_json(j);
  j.end_obj();
}

/*******************************************************************************
 *                             Stop-Warning-Response
 ******************************************************************************/

uint32_t stop_warning_resp_ies_o::idx_to_id(uint32_t idx)
{
  static const uint32_t options[] = {5, 11, 1};
  return map_enum_number(options, 3, idx, "id");
}
bool stop_warning_resp_ies_o::is_id_valid(const uint32_t& id)
{
  static const uint32_t options[] = {5, 11, 1};
  for (const auto& o : options) {
    if (o == id) {
      return true;
    }
  }
  return false;
}
crit_e stop_warning_resp_ies_o::get_crit(const uint32_t& id)
{
  switch (id) {
    case 5:
      return crit_e::reject;
    case 11:
      return crit_e::reject;
    case 1:
      return crit_e::reject;
    default:
      asn1::log_error("The id=%d is not recognized", id);
  }
  return {};
}
stop_warning_resp_ies_o::value_c stop_warning_resp_ies_o::get_value(const uint32_t& id)
{
  value_c ret{};
  switch (id) {
    case 5:
      ret.set(value_c::types::msg_id);
      break;
    case 11:
      ret.set(value_c::types::serial_num);
      break;
    case 1:
      ret.set(value_c::types::cause);
      break;
    default:
      asn1::log_error("The id=%d is not recognized", id);
  }
  return ret;
}
presence_e stop_warning_resp_ies_o::get_presence(const uint32_t& id)
{
  switch (id) {
    case 5:
      return presence_e::mandatory;
    case 11:
      return presence_e::mandatory;
    case 1:
      return presence_e::mandatory;
    default:
      asn1::log_error("The id=%d is not recognized", id);
  }
  return {};
}

void stop_warning_resp_ies_o::value_c::destroy_()
{
  switch (type_) {
    case types::msg_id:
      c.destroy<fixed_bitstring<16, false, true> >();
      break;
    case types::serial_num:
      c.destroy<fixed_bitstring<16, false, true> >();
      break;
    default:
      break;
  }
}
void stop_warning_resp_ies_o::value_c::set(types::options e)
{
  destroy_();
  type_ = e;
  switch (type_) {
    case types::msg_id:
      c.init<fixed_bitstring<16, false, true> >();
      break;
    case types::serial_num:
      c.init<fixed_bitstring<16, false, true> >();
      break;
    case types::cause:
      break;
    case types::nulltype:
      break;
    default:
      log_invalid_choice_id(type_, "stop_warning_resp_ies_o::value_c");
  }
}
stop_warning_resp_ies_o::value_c::value_c(const stop_warning_resp_ies_o::value_c& other)
{
  type_ = other.type();
  switch (type_) {
    case types::msg_id:
      c.init(other.c.get<fixed_bitstring<16, false, true> >());
      break;
    case types::serial_num:
      c.init(other.c.get<fixed_bitstring<16, false, true> >());
      break;
    case types::cause:
      c.init(other.c.get<uint16_t>());
      break;
    case types::nulltype:
      break;
    default:
      log_invalid_choice_id(type_, "stop_warning_resp_ies_o::value_c");
  }
}
stop_warning_resp_ies_o::value_c&
stop_warning_resp_ies_o::value_c::operator=(const stop_warning_resp_ies_o::value_c& other)
{
  if (this == &other) {
    return *this;
  }
  set(other.type());
  switch (type_) {
    case types::msg_id:
      c.set(other.c.get<fixed_bitstring<16, false, true> >());
      break;
    case types::serial_num:
      c.set(other.c.get<fixed_bitstring<16, false, true> >());
      break;
    case types::cause:
      c.set(other.c.get<uint16_t>());
      break;
    case types::nulltype:
      break;
    default:
      log_invalid_choice_id(type_, "stop_warning_resp_ies_o::value_c");
  }
  return *this;
}
fixed_bitstring<16, false, true>& stop_warning_resp_ies_o::value_c::msg_id()
{
  assert_choice_type(types::msg_id, type_, "Value");
  return c.get<fixed_bitstring<16, false, true> >();
}
fixed_bitstring<16, false, true>& stop_warning_resp_ies_o::value_c::serial_num()
{
  assert_choice_type(types::serial_num, type_, "Value");
  return c.get<fixed_bitstring<16, false, true> >();
}
uint16_t& stop_warning_resp_ies_o::value_c::cause()
{
  assert_choice_type(types::cause, type_, "Value");
  return c.get<uint16_t>();
}
const fixed_bitstring<16, false, true>& stop_warning_resp_ies_o::value_c::msg_id() const
{
  assert_choice_type(types::msg_id, type_, "Value");
  return c.get<fixed_bitstring<16, false, true> >();
}
const fixed_bitstring<16, false, true>& stop_warning_resp_ies_o::value_c::serial_num() const
{
  assert_choice_type(types::serial_num, type_, "Value");
  return c.get<fixed_bitstring<16, false, true> >();
}
const uint16_t& stop_warning_resp_ies_o::value_c::cause() const
{
  assert_choice_type(types::cause, type_, "Value");
  return c.get<uint16_t>();
}
void stop_warning_resp_ies_o::value_c::to_json(json_writer& j) const
{
  j.start_obj();
  switch (type_) {
    case types::msg_id:
      j.write_str("BIT STRING", c.get<fixed_bitstring<16, false, true> >().to_string());
      break;
    case types::serial_num:
      j.write_str("BIT STRING", c.get<fixed_bitstring<16, false, true> >().to_string());
      break;
    case types::cause:
      j.write_int("INTEGER (0..255)", c.get<uint16_t>());
      break;
    default:
      log_invalid_choice_id(type_, "stop_warning_resp_ies_o::value_c");
  }
  j.end_obj();
}
SRSASN_CODE stop_warning_resp_ies_o::value_c::pack(bit_ref& bref) const
{
  varlength_field_pack_guard varlen_scope(bref, true);
  switch (type_) {
    case types::msg_id:
      HANDLE_CODE((c.get<fixed_bitstring<16, false, true> >().pack(bref)));
      break;
    case types::serial_num:
      HANDLE_CODE((c.get<fixed_bitstring<16, false, true> >().pack(bref)));
      break;
    case types::cause:
      HANDLE_CODE(pack_integer(bref, c.get<uint16_t>(), (uint16_t)0u, (uint16_t)255u, false, true));
      break;
    default:
      log_invalid_choice_id(type_, "stop_warning_resp_ies_o::value_c");
      return SRSASN_ERROR_ENCODE_FAIL;
  }
  return SRSASN_SUCCESS;
}
SRSASN_CODE stop_warning_resp_ies_o::value_c::unpack(cbit_ref& bref)
{
  varlength_field_unpack_guard varlen_scope(bref, true);
  switch (type_) {
    case types::msg_id:
      HANDLE_CODE((c.get<fixed_bitstring<16, false, true> >().unpack(bref)));
      break;
    case types::serial_num:
      HANDLE_CODE((c.get<fixed_bitstring<16, false, true> >().unpack(bref)));
      break;
    case types::cause:
      HANDLE_CODE(unpack_integer(c.get<uint16_t>(), bref, (uint16_t)0u, (uint16_t)255u, false, true));
      break;
    default:
      log_invalid_choice_id(type_, "stop_warning_resp_ies_o::value_c");
      return SRSASN_ERROR_DECODE_FAIL;
  }
  return SRSASN_SUCCESS;
}
const char* stop_warning_resp_ies_o::value_c::types_opts::to_string() const
{
  static const char* options[] = {"Message-Identifier", "Serial-Number", "INTEGER (0..255)"};
  return convert_enum_idx(options, 3, value, "stop_warning_resp_ies_o::value_c::types");
}

stop_warning_resp_ies_container::stop_warning_resp_ies_container() :
  msg_id(5, crit_e::reject), serial_num(11, crit_e::reject), cause(1, crit_e::reject)
{}
SRSASN_CODE stop_warning_resp_ies_container::pack(bit_ref& bref) const
{
  pack_length(bref, (uint32_t)3u, 0u, 65535u, true);

  HANDLE_CODE(msg_id.pack(bref));
  HANDLE_CODE(serial_num.pack(bref));
  HANDLE_CODE(cause.pack(bref));

  return SRSASN_SUCCESS;
}
SRSASN_CODE stop_warning_resp_ies_container::unpack(cbit_ref& bref)
{
  uint32_t nof_ies = 0;
  unpack_length(nof_ies, bref, 0u, 65535u, true);

  uint32_t nof_mandatory_ies = 3;

  for (; nof_ies > 0; --nof_ies) {
    protocol_ie_field_s<stop_warning_resp_ies_o> c;
    HANDLE_CODE(c.unpack(bref));
    switch (c.id) {
      case 5:
        nof_mandatory_ies--;
        msg_id.id    = c.id;
        msg_id.crit  = c.crit;
        msg_id.value = c.value.msg_id();
        break;
      case 11:
        nof_mandatory_ies--;
        serial_num.id    = c.id;
        serial_num.crit  = c.crit;
        serial_num.value = c.value.serial_num();
        break;
      case 1:
        nof_mandatory_ies--;
        cause.id    = c.id;
        cause.crit  = c.crit;
        cause.value = c.value.cause();
        break;
      default:
        asn1::log_error("Unpacked object ID=%d is not recognized\n", c.id);
        return SRSASN_ERROR_DECODE_FAIL;
    }
  }
  if (nof_mandatory_ies > 0) {
    asn1::log_error("Mandatory fields are missing\n");
    return SRSASN_ERROR_DECODE_FAIL;
  }
  return SRSASN_SUCCESS;
}
void stop_warning_resp_ies_container::to_json(json_writer& j) const
{
  j.start_obj();
  j.write_fieldname("");
  msg_id.to_json(j);
  j.write_fieldname("");
  serial_num.to_json(j);
  j.write_fieldname("");
  cause.to_json(j);
  j.end_obj();
}

SRSASN_CODE stop_warning_resp_s::pack(bit_ref& bref) const
{
  bref.pack(ext, 1);
  HANDLE_CODE(protocol_ies.pack(bref));

  return SRSASN_SUCCESS;
}
SRSASN_CODE stop_warning_resp_s::unpack(cbit_ref& bref)
{
  bref.unpack(ext, 1);
  HANDLE_CODE(protocol_ies.unpack(bref));

  return SRSASN_SUCCESS;
}
void stop_warning_resp_s::to_json(json_writer& j) const
{
  j.start_obj();
  j.write_fieldname("protocolIEs");
  protocol_ies.to_json(j);
  j.end_obj();
}

/*******************************************************************************
 *                    SBC-AP-PDU top-level PDU wrapper
 ******************************************************************************/

uint16_t sbc_ap_elem_procs_o::idx_to_proc_code(uint32_t idx)
{
  static const uint16_t options[] = {0, 1};
  return map_enum_number(options, 2, idx, "proc_code");
}
bool sbc_ap_elem_procs_o::is_proc_code_valid(const uint16_t& proc_code)
{
  return proc_code == 0 || proc_code == 1;
}
crit_e sbc_ap_elem_procs_o::get_crit(const uint16_t& proc_code)
{
  switch (proc_code) {
    case 0:
      return crit_e::reject;
    case 1:
      return crit_e::reject;
    default:
      asn1::log_error("The proc_code=%d is not recognized", proc_code);
  }
  return {};
}
sbc_ap_elem_procs_o::init_msg_c sbc_ap_elem_procs_o::get_init_msg(const uint16_t& proc_code)
{
  init_msg_c ret{};
  switch (proc_code) {
    case 0:
      ret.set(init_msg_c::types::write_replace_warning_request);
      break;
    case 1:
      ret.set(init_msg_c::types::stop_warning_request);
      break;
    default:
      asn1::log_error("The proc_code=%d is not recognized", proc_code);
  }
  return ret;
}
sbc_ap_elem_procs_o::successful_outcome_c sbc_ap_elem_procs_o::get_successful_outcome(const uint16_t& proc_code)
{
  successful_outcome_c ret{};
  switch (proc_code) {
    case 0:
      ret.set(successful_outcome_c::types::write_replace_warning_resp);
      break;
    case 1:
      ret.set(successful_outcome_c::types::stop_warning_resp);
      break;
    default:
      asn1::log_error("The proc_code=%d is not recognized", proc_code);
  }
  return ret;
}

void sbc_ap_elem_procs_o::init_msg_c::destroy_()
{
  switch (type_) {
    case types::write_replace_warning_request:
      c.destroy<write_replace_warning_request_s>();
      break;
    case types::stop_warning_request:
      c.destroy<stop_warning_request_s>();
      break;
    default:
      break;
  }
}
void sbc_ap_elem_procs_o::init_msg_c::set(types::options e)
{
  destroy_();
  type_ = e;
  switch (type_) {
    case types::write_replace_warning_request:
      c.init<write_replace_warning_request_s>();
      break;
    case types::stop_warning_request:
      c.init<stop_warning_request_s>();
      break;
    case types::nulltype:
      break;
    default:
      log_invalid_choice_id(type_, "sbc_ap_elem_procs_o::init_msg_c");
  }
}
sbc_ap_elem_procs_o::init_msg_c::init_msg_c(const sbc_ap_elem_procs_o::init_msg_c& other)
{
  type_ = other.type();
  switch (type_) {
    case types::write_replace_warning_request:
      c.init(other.c.get<write_replace_warning_request_s>());
      break;
    case types::stop_warning_request:
      c.init(other.c.get<stop_warning_request_s>());
      break;
    case types::nulltype:
      break;
    default:
      log_invalid_choice_id(type_, "sbc_ap_elem_procs_o::init_msg_c");
  }
}
sbc_ap_elem_procs_o::init_msg_c&
sbc_ap_elem_procs_o::init_msg_c::operator=(const sbc_ap_elem_procs_o::init_msg_c& other)
{
  if (this == &other) {
    return *this;
  }
  set(other.type());
  switch (type_) {
    case types::write_replace_warning_request:
      c.set(other.c.get<write_replace_warning_request_s>());
      break;
    case types::stop_warning_request:
      c.set(other.c.get<stop_warning_request_s>());
      break;
    case types::nulltype:
      break;
    default:
      log_invalid_choice_id(type_, "sbc_ap_elem_procs_o::init_msg_c");
  }
  return *this;
}
write_replace_warning_request_s& sbc_ap_elem_procs_o::init_msg_c::write_replace_warning_request()
{
  assert_choice_type(types::write_replace_warning_request, type_, "InitiatingMessage");
  return c.get<write_replace_warning_request_s>();
}
stop_warning_request_s& sbc_ap_elem_procs_o::init_msg_c::stop_warning_request()
{
  assert_choice_type(types::stop_warning_request, type_, "InitiatingMessage");
  return c.get<stop_warning_request_s>();
}
const write_replace_warning_request_s& sbc_ap_elem_procs_o::init_msg_c::write_replace_warning_request() const
{
  assert_choice_type(types::write_replace_warning_request, type_, "InitiatingMessage");
  return c.get<write_replace_warning_request_s>();
}
const stop_warning_request_s& sbc_ap_elem_procs_o::init_msg_c::stop_warning_request() const
{
  assert_choice_type(types::stop_warning_request, type_, "InitiatingMessage");
  return c.get<stop_warning_request_s>();
}
void sbc_ap_elem_procs_o::init_msg_c::to_json(json_writer& j) const
{
  j.start_obj();
  switch (type_) {
    case types::write_replace_warning_request:
      j.write_fieldname("Write-Replace-Warning-Request");
      c.get<write_replace_warning_request_s>().to_json(j);
      break;
    case types::stop_warning_request:
      j.write_fieldname("Stop-Warning-Request");
      c.get<stop_warning_request_s>().to_json(j);
      break;
    default:
      log_invalid_choice_id(type_, "sbc_ap_elem_procs_o::init_msg_c");
  }
  j.end_obj();
}
SRSASN_CODE sbc_ap_elem_procs_o::init_msg_c::pack(bit_ref& bref) const
{
  switch (type_) {
    case types::write_replace_warning_request:
      HANDLE_CODE(c.get<write_replace_warning_request_s>().pack(bref));
      break;
    case types::stop_warning_request:
      HANDLE_CODE(c.get<stop_warning_request_s>().pack(bref));
      break;
    default:
      log_invalid_choice_id(type_, "sbc_ap_elem_procs_o::init_msg_c");
      return SRSASN_ERROR_ENCODE_FAIL;
  }
  return SRSASN_SUCCESS;
}
SRSASN_CODE sbc_ap_elem_procs_o::init_msg_c::unpack(cbit_ref& bref)
{
  switch (type_) {
    case types::write_replace_warning_request:
      HANDLE_CODE(c.get<write_replace_warning_request_s>().unpack(bref));
      break;
    case types::stop_warning_request:
      HANDLE_CODE(c.get<stop_warning_request_s>().unpack(bref));
      break;
    default:
      log_invalid_choice_id(type_, "sbc_ap_elem_procs_o::init_msg_c");
      return SRSASN_ERROR_DECODE_FAIL;
  }
  return SRSASN_SUCCESS;
}
const char* sbc_ap_elem_procs_o::init_msg_c::types_opts::to_string() const
{
  static const char* options[] = {"Write-Replace-Warning-Request", "Stop-Warning-Request"};
  return convert_enum_idx(options, 2, value, "sbc_ap_elem_procs_o::init_msg_c::types");
}

void sbc_ap_elem_procs_o::successful_outcome_c::destroy_()
{
  switch (type_) {
    case types::write_replace_warning_resp:
      c.destroy<write_replace_warning_resp_s>();
      break;
    case types::stop_warning_resp:
      c.destroy<stop_warning_resp_s>();
      break;
    default:
      break;
  }
}
void sbc_ap_elem_procs_o::successful_outcome_c::set(types::options e)
{
  destroy_();
  type_ = e;
  switch (type_) {
    case types::write_replace_warning_resp:
      c.init<write_replace_warning_resp_s>();
      break;
    case types::stop_warning_resp:
      c.init<stop_warning_resp_s>();
      break;
    case types::nulltype:
      break;
    default:
      log_invalid_choice_id(type_, "sbc_ap_elem_procs_o::successful_outcome_c");
  }
}
sbc_ap_elem_procs_o::successful_outcome_c::successful_outcome_c(
    const sbc_ap_elem_procs_o::successful_outcome_c& other)
{
  type_ = other.type();
  switch (type_) {
    case types::write_replace_warning_resp:
      c.init(other.c.get<write_replace_warning_resp_s>());
      break;
    case types::stop_warning_resp:
      c.init(other.c.get<stop_warning_resp_s>());
      break;
    case types::nulltype:
      break;
    default:
      log_invalid_choice_id(type_, "sbc_ap_elem_procs_o::successful_outcome_c");
  }
}
sbc_ap_elem_procs_o::successful_outcome_c&
sbc_ap_elem_procs_o::successful_outcome_c::operator=(const sbc_ap_elem_procs_o::successful_outcome_c& other)
{
  if (this == &other) {
    return *this;
  }
  set(other.type());
  switch (type_) {
    case types::write_replace_warning_resp:
      c.set(other.c.get<write_replace_warning_resp_s>());
      break;
    case types::stop_warning_resp:
      c.set(other.c.get<stop_warning_resp_s>());
      break;
    case types::nulltype:
      break;
    default:
      log_invalid_choice_id(type_, "sbc_ap_elem_procs_o::successful_outcome_c");
  }
  return *this;
}
write_replace_warning_resp_s& sbc_ap_elem_procs_o::successful_outcome_c::write_replace_warning_resp()
{
  assert_choice_type(types::write_replace_warning_resp, type_, "SuccessfulOutcome");
  return c.get<write_replace_warning_resp_s>();
}
stop_warning_resp_s& sbc_ap_elem_procs_o::successful_outcome_c::stop_warning_resp()
{
  assert_choice_type(types::stop_warning_resp, type_, "SuccessfulOutcome");
  return c.get<stop_warning_resp_s>();
}
const write_replace_warning_resp_s& sbc_ap_elem_procs_o::successful_outcome_c::write_replace_warning_resp() const
{
  assert_choice_type(types::write_replace_warning_resp, type_, "SuccessfulOutcome");
  return c.get<write_replace_warning_resp_s>();
}
const stop_warning_resp_s& sbc_ap_elem_procs_o::successful_outcome_c::stop_warning_resp() const
{
  assert_choice_type(types::stop_warning_resp, type_, "SuccessfulOutcome");
  return c.get<stop_warning_resp_s>();
}
void sbc_ap_elem_procs_o::successful_outcome_c::to_json(json_writer& j) const
{
  j.start_obj();
  switch (type_) {
    case types::write_replace_warning_resp:
      j.write_fieldname("Write-Replace-Warning-Response");
      c.get<write_replace_warning_resp_s>().to_json(j);
      break;
    case types::stop_warning_resp:
      j.write_fieldname("Stop-Warning-Response");
      c.get<stop_warning_resp_s>().to_json(j);
      break;
    default:
      log_invalid_choice_id(type_, "sbc_ap_elem_procs_o::successful_outcome_c");
  }
  j.end_obj();
}
SRSASN_CODE sbc_ap_elem_procs_o::successful_outcome_c::pack(bit_ref& bref) const
{
  switch (type_) {
    case types::write_replace_warning_resp:
      HANDLE_CODE(c.get<write_replace_warning_resp_s>().pack(bref));
      break;
    case types::stop_warning_resp:
      HANDLE_CODE(c.get<stop_warning_resp_s>().pack(bref));
      break;
    default:
      log_invalid_choice_id(type_, "sbc_ap_elem_procs_o::successful_outcome_c");
      return SRSASN_ERROR_ENCODE_FAIL;
  }
  return SRSASN_SUCCESS;
}
SRSASN_CODE sbc_ap_elem_procs_o::successful_outcome_c::unpack(cbit_ref& bref)
{
  switch (type_) {
    case types::write_replace_warning_resp:
      HANDLE_CODE(c.get<write_replace_warning_resp_s>().unpack(bref));
      break;
    case types::stop_warning_resp:
      HANDLE_CODE(c.get<stop_warning_resp_s>().unpack(bref));
      break;
    default:
      log_invalid_choice_id(type_, "sbc_ap_elem_procs_o::successful_outcome_c");
      return SRSASN_ERROR_DECODE_FAIL;
  }
  return SRSASN_SUCCESS;
}
const char* sbc_ap_elem_procs_o::successful_outcome_c::types_opts::to_string() const
{
  static const char* options[] = {"Write-Replace-Warning-Response", "Stop-Warning-Response"};
  return convert_enum_idx(options, 2, value, "sbc_ap_elem_procs_o::successful_outcome_c::types");
}

SRSASN_CODE sbc_ap_init_msg_s::pack(bit_ref& bref) const
{
  HANDLE_CODE(pack_integer(bref, proc_code, (uint16_t)0u, (uint16_t)255u, false, true));
  HANDLE_CODE(crit.pack(bref));
  HANDLE_CODE(value.pack(bref));

  return SRSASN_SUCCESS;
}
SRSASN_CODE sbc_ap_init_msg_s::unpack(cbit_ref& bref)
{
  HANDLE_CODE(unpack_integer(proc_code, bref, (uint16_t)0u, (uint16_t)255u, false, true));
  HANDLE_CODE(crit.unpack(bref));
  value = sbc_ap_elem_procs_o::get_init_msg(proc_code);
  HANDLE_CODE(value.unpack(bref));

  return SRSASN_SUCCESS;
}
void sbc_ap_init_msg_s::to_json(json_writer& j) const
{
  j.start_obj();
  j.write_int("procedureCode", proc_code);
  j.write_str("criticality", crit.to_string());
  j.write_fieldname("value");
  value.to_json(j);
  j.end_obj();
}
bool sbc_ap_init_msg_s::load_info_obj(const uint16_t& proc_code_)
{
  if (not sbc_ap_elem_procs_o::is_proc_code_valid(proc_code_)) {
    return false;
  }
  proc_code = proc_code_;
  crit      = sbc_ap_elem_procs_o::get_crit(proc_code);
  value     = sbc_ap_elem_procs_o::get_init_msg(proc_code);
  return value.type().value != sbc_ap_elem_procs_o::init_msg_c::types_opts::nulltype;
}

SRSASN_CODE sbc_ap_successful_outcome_s::pack(bit_ref& bref) const
{
  HANDLE_CODE(pack_integer(bref, proc_code, (uint16_t)0u, (uint16_t)255u, false, true));
  HANDLE_CODE(crit.pack(bref));
  HANDLE_CODE(value.pack(bref));

  return SRSASN_SUCCESS;
}
SRSASN_CODE sbc_ap_successful_outcome_s::unpack(cbit_ref& bref)
{
  HANDLE_CODE(unpack_integer(proc_code, bref, (uint16_t)0u, (uint16_t)255u, false, true));
  HANDLE_CODE(crit.unpack(bref));
  value = sbc_ap_elem_procs_o::get_successful_outcome(proc_code);
  HANDLE_CODE(value.unpack(bref));

  return SRSASN_SUCCESS;
}
void sbc_ap_successful_outcome_s::to_json(json_writer& j) const
{
  j.start_obj();
  j.write_int("procedureCode", proc_code);
  j.write_str("criticality", crit.to_string());
  j.write_fieldname("value");
  value.to_json(j);
  j.end_obj();
}
bool sbc_ap_successful_outcome_s::load_info_obj(const uint16_t& proc_code_)
{
  if (not sbc_ap_elem_procs_o::is_proc_code_valid(proc_code_)) {
    return false;
  }
  proc_code = proc_code_;
  crit      = sbc_ap_elem_procs_o::get_crit(proc_code);
  value     = sbc_ap_elem_procs_o::get_successful_outcome(proc_code);
  return value.type().value != sbc_ap_elem_procs_o::successful_outcome_c::types_opts::nulltype;
}

SRSASN_CODE sbc_ap_unsuccessful_outcome_s::pack(bit_ref& bref) const
{
  asn1::log_error("UnsuccessfulOutcome is not implemented for any SBc-AP procedure in this codec");
  return SRSASN_ERROR_ENCODE_FAIL;
}
SRSASN_CODE sbc_ap_unsuccessful_outcome_s::unpack(cbit_ref& bref)
{
  asn1::log_error("UnsuccessfulOutcome is not implemented for any SBc-AP procedure in this codec");
  return SRSASN_ERROR_DECODE_FAIL;
}
void sbc_ap_unsuccessful_outcome_s::to_json(json_writer& j) const
{
  j.start_obj();
  j.write_int("procedureCode", proc_code);
  j.write_str("criticality", crit.to_string());
  j.end_obj();
}

void sbc_ap_pdu_c::destroy_()
{
  switch (type_) {
    case types::init_msg:
      c.destroy<sbc_ap_init_msg_s>();
      break;
    case types::successful_outcome:
      c.destroy<sbc_ap_successful_outcome_s>();
      break;
    case types::unsuccessful_outcome:
      c.destroy<sbc_ap_unsuccessful_outcome_s>();
      break;
    default:
      break;
  }
}
void sbc_ap_pdu_c::set(types::options e)
{
  destroy_();
  type_ = e;
  switch (type_) {
    case types::init_msg:
      c.init<sbc_ap_init_msg_s>();
      break;
    case types::successful_outcome:
      c.init<sbc_ap_successful_outcome_s>();
      break;
    case types::unsuccessful_outcome:
      c.init<sbc_ap_unsuccessful_outcome_s>();
      break;
    case types::nulltype:
      break;
    default:
      log_invalid_choice_id(type_, "sbc_ap_pdu_c");
  }
}
sbc_ap_pdu_c::sbc_ap_pdu_c(const sbc_ap_pdu_c& other)
{
  type_ = other.type();
  switch (type_) {
    case types::init_msg:
      c.init(other.c.get<sbc_ap_init_msg_s>());
      break;
    case types::successful_outcome:
      c.init(other.c.get<sbc_ap_successful_outcome_s>());
      break;
    case types::unsuccessful_outcome:
      c.init(other.c.get<sbc_ap_unsuccessful_outcome_s>());
      break;
    case types::nulltype:
      break;
    default:
      log_invalid_choice_id(type_, "sbc_ap_pdu_c");
  }
}
sbc_ap_pdu_c& sbc_ap_pdu_c::operator=(const sbc_ap_pdu_c& other)
{
  if (this == &other) {
    return *this;
  }
  set(other.type());
  switch (type_) {
    case types::init_msg:
      c.set(other.c.get<sbc_ap_init_msg_s>());
      break;
    case types::successful_outcome:
      c.set(other.c.get<sbc_ap_successful_outcome_s>());
      break;
    case types::unsuccessful_outcome:
      c.set(other.c.get<sbc_ap_unsuccessful_outcome_s>());
      break;
    case types::nulltype:
      break;
    default:
      log_invalid_choice_id(type_, "sbc_ap_pdu_c");
  }
  return *this;
}
sbc_ap_init_msg_s& sbc_ap_pdu_c::set_init_msg()
{
  set(types::init_msg);
  return c.get<sbc_ap_init_msg_s>();
}
sbc_ap_successful_outcome_s& sbc_ap_pdu_c::set_successful_outcome()
{
  set(types::successful_outcome);
  return c.get<sbc_ap_successful_outcome_s>();
}
sbc_ap_unsuccessful_outcome_s& sbc_ap_pdu_c::set_unsuccessful_outcome()
{
  set(types::unsuccessful_outcome);
  return c.get<sbc_ap_unsuccessful_outcome_s>();
}
void sbc_ap_pdu_c::to_json(json_writer& j) const
{
  j.start_obj();
  switch (type_) {
    case types::init_msg:
      j.write_fieldname("initiatingMessage");
      c.get<sbc_ap_init_msg_s>().to_json(j);
      break;
    case types::successful_outcome:
      j.write_fieldname("successfulOutcome");
      c.get<sbc_ap_successful_outcome_s>().to_json(j);
      break;
    case types::unsuccessful_outcome:
      j.write_fieldname("unsuccessfulOutcome");
      c.get<sbc_ap_unsuccessful_outcome_s>().to_json(j);
      break;
    default:
      log_invalid_choice_id(type_, "sbc_ap_pdu_c");
  }
  j.end_obj();
}
SRSASN_CODE sbc_ap_pdu_c::pack(bit_ref& bref) const
{
  type_.pack(bref);
  switch (type_) {
    case types::init_msg:
      HANDLE_CODE(c.get<sbc_ap_init_msg_s>().pack(bref));
      break;
    case types::successful_outcome:
      HANDLE_CODE(c.get<sbc_ap_successful_outcome_s>().pack(bref));
      break;
    case types::unsuccessful_outcome:
      HANDLE_CODE(c.get<sbc_ap_unsuccessful_outcome_s>().pack(bref));
      break;
    default:
      log_invalid_choice_id(type_, "sbc_ap_pdu_c");
      return SRSASN_ERROR_ENCODE_FAIL;
  }
  return SRSASN_SUCCESS;
}
SRSASN_CODE sbc_ap_pdu_c::unpack(cbit_ref& bref)
{
  types e;
  e.unpack(bref);
  set(e);
  switch (type_) {
    case types::init_msg:
      HANDLE_CODE(c.get<sbc_ap_init_msg_s>().unpack(bref));
      break;
    case types::successful_outcome:
      HANDLE_CODE(c.get<sbc_ap_successful_outcome_s>().unpack(bref));
      break;
    case types::unsuccessful_outcome:
      HANDLE_CODE(c.get<sbc_ap_unsuccessful_outcome_s>().unpack(bref));
      break;
    default:
      log_invalid_choice_id(type_, "sbc_ap_pdu_c");
      return SRSASN_ERROR_DECODE_FAIL;
  }
  return SRSASN_SUCCESS;
}
const char* sbc_ap_pdu_c::types_opts::to_string() const
{
  static const char* options[] = {"initiatingMessage", "successfulOutcome", "unsuccessfulOutcome"};
  return convert_enum_idx(options, 3, value, "sbc_ap_pdu_c::types");
}

/*******************************************************************************
 *                          Explicit template instantiations
 ******************************************************************************/

template struct asn1::sbc_ap::protocol_ie_field_s<write_replace_warning_request_ies_o>;
template struct asn1::sbc_ap::protocol_ie_field_s<write_replace_warning_resp_ies_o>;
template struct asn1::sbc_ap::protocol_ie_field_s<stop_warning_request_ies_o>;
template struct asn1::sbc_ap::protocol_ie_field_s<stop_warning_resp_ies_o>;

template struct asn1::sbc_ap::protocol_ie_container_item_s<fixed_bitstring<16, false, true> >;
template struct asn1::sbc_ap::protocol_ie_container_item_s<integer<uint16_t, 0, 4096, false, true> >;
template struct asn1::sbc_ap::protocol_ie_container_item_s<integer<uint32_t, 0, 65535, false, true> >;
template struct asn1::sbc_ap::protocol_ie_container_item_s<fixed_octstring<2, true> >;
template struct asn1::sbc_ap::protocol_ie_container_item_s<fixed_octstring<50, true> >;
template struct asn1::sbc_ap::protocol_ie_container_item_s<fixed_bitstring<8, false, true> >;
template struct asn1::sbc_ap::protocol_ie_container_item_s<bounded_octstring<1, 9600, true> >;
template struct asn1::sbc_ap::protocol_ie_container_item_s<bounded_octstring<1, 1024, true> >;
template struct asn1::sbc_ap::protocol_ie_container_item_s<integer<uint16_t, 0, 255, false, true> >;
