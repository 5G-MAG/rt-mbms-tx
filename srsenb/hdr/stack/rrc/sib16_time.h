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

#ifndef SRSENB_SIB16_TIME_H
#define SRSENB_SIB16_TIME_H

#include <cstdint>

// Pure arithmetic behind the dynamic SIB16 time (rrc::prepare_bcch_dlsch()), kept free of RRC state so it can be tested
// on its own. Baseline: 3GPP TS 36.331 V19.4.0.

namespace srsenb {
namespace sib16_time {

constexpr int64_t unix_to_1900_s     = 2208988800LL; // 1900-01-01 to 1970-01-01
constexpr int64_t gps_epoch_from_1900_s = 2524953600LL; // TS 36.331 clause 6.3.1, NOTE 1 of timeInfoUTC

/**
 * Whole subframes (ms) from `tti_tx_dl` to the SFN boundary at or immediately after the end of the SI-window of the SI
 * message that carries SIB16, or -1 if `tti_tx_dl` is not inside that window.
 *
 * TS 36.331 clause 5.2.3b (MBMS-dedicated cell): n is the order of entry of the SI message in the schedulingInfoList of
 * SIB1-MBMS, x = (n - 1)*w with w = si-WindowLength; "the SI-window starts always at the subframe #a, where a = x mod 10,
 * in the radio frame for which SFN mod T = FLOOR(x/10)", T being the si-Periodicity. The window therefore covers
 * [x, x + w) ms counted from the start of the period. Clause 6.3.2 (TimeReferenceInfo): the time "indicates the time at
 * the SFN boundary at or immediately after the ending boundary of the SI-window".
 *
 * A configuration whose window starts after the end of its own period (x/10 >= T) never occurs on the air; -1 is returned.
 * `period_rf` must divide 1024 (all si-Periodicity values do), so that `tti_tx_dl` modulo 10 T is the position in the period
 * across the SFN wrap.
 */
inline int64_t boundary_ahead_ms(uint32_t n, uint32_t w_ms, uint32_t period_rf, uint32_t tti_tx_dl)
{
  if (n == 0 || w_ms == 0 || period_rf == 0 || 1024 % period_rf != 0) {
    return -1;
  }
  const uint32_t x = (n - 1) * w_ms;
  if (x / 10 >= period_rf) {
    return -1;
  }
  const uint32_t pos = tti_tx_dl % (10 * period_rf);
  if (pos < x || pos >= x + w_ms) {
    return -1;
  }
  const uint32_t boundary = ((x + w_ms + 9) / 10) * 10; // first frame start at or after the window end
  return (int64_t)boundary - (int64_t)pos;
}

/// SFN (0..1023) of the boundary found by boundary_ahead_ms(), for logging and tests.
inline uint32_t boundary_sfn(uint32_t n, uint32_t w_ms, uint32_t period_rf, uint32_t tti_tx_dl)
{
  const int64_t ahead = boundary_ahead_ms(n, w_ms, period_rf, tti_tx_dl);
  return (uint32_t)((((int64_t)tti_tx_dl + (ahead < 0 ? 0 : ahead)) / 10) % 1024);
}

struct fields_t {
  uint64_t time_info_utc_10ms = 0; ///< timeInfoUTC-r11: UTC seconds in 10 ms units since 1900-01-01
  uint32_t ref_days           = 0; ///< ReferenceTime-r15, 0.25 us units from the GPS epoch
  uint32_t ref_seconds        = 0;
  uint16_t ref_milli_seconds  = 0;
  uint16_t ref_quarter_us     = 0;
};

/**
 * The SIB16 time fields for a network time given as nanoseconds since 1900-01-01 UTC.
 *
 * TS 36.331 clause 6.3.1: timeInfoUTC counts "UTC seconds in 10 ms units since 00:00:00 on Gregorian calendar date
 * 1 January 1900"; "GPS Time (in seconds) = timeInfoUTC (in seconds) - 2,524,953,600 (seconds) + leapSeconds".
 * Clause 6.3.2: "The indicated time in 0.25 us unit from the origin is refDays*86400*1000*4000 +
 * refSeconds*1000*4000 + refMilliSeconds*4000 + refQuarterMicroSeconds", the origin being the start of GPS time when
 * timeInfoType is absent. Returns false if a field would fall outside its range (INTEGER 0..549755813887 for timeInfoUTC,
 * 0..72999 for refDays), e.g. a clock set before 1980.
 */
inline bool make_fields(int64_t utc_1900_ns, int leap_seconds, fields_t* out)
{
  if (utc_1900_ns < 0) {
    return false;
  }
  const uint64_t utc_10ms = (uint64_t)(utc_1900_ns / 10000000LL);
  if (utc_10ms > 549755813887ULL) {
    return false;
  }
  const int64_t gps_ns = utc_1900_ns - gps_epoch_from_1900_s * 1000000000LL + (int64_t)leap_seconds * 1000000000LL;
  if (gps_ns < 0) {
    return false;
  }
  int64_t units = gps_ns / 250;
  const int64_t days = units / (86400LL * 1000 * 4000);
  if (days > 72999) {
    return false;
  }
  units %= 86400LL * 1000 * 4000;
  out->time_info_utc_10ms = utc_10ms;
  out->ref_days           = (uint32_t)days;
  out->ref_seconds        = (uint32_t)(units / (1000LL * 4000));
  units %= 1000LL * 4000;
  out->ref_milli_seconds = (uint16_t)(units / 4000);
  out->ref_quarter_us    = (uint16_t)(units % 4000);
  return true;
}

} // namespace sib16_time
} // namespace srsenb

#endif // SRSENB_SIB16_TIME_H
