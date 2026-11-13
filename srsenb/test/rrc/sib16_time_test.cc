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

// Tests for srsenb/hdr/stack/rrc/sib16_time.h: the SI-window arithmetic of TS 36.331 V19.4.0 clause 5.2.3b and the SIB16
// time fields of clauses 6.3.1 and 6.3.2. Expected values are worked out from the clause text, not from the code.
#include "srsenb/hdr/stack/rrc/si_window.h"
#include "srsenb/hdr/stack/rrc/sib16_time.h"
#include "srsran/common/test_common.h"
#include "srsran/srslog/srslog.h"

using namespace srsenb::sib16_time;
using srsenb::si_window::has_cas_opportunity;

static int test_window_arithmetic()
{
  // n=1, w=20 ms, T=64: x=0, window [0,20) ms of every 640 ms period, boundary at +20 ms (SFN 2 of the period).
  TESTASSERT(boundary_ahead_ms(1, 20, 64, 640) == 20);   // first subframe of the window
  TESTASSERT(boundary_ahead_ms(1, 20, 64, 649) == 11);   // later in the same frame
  TESTASSERT(boundary_ahead_ms(1, 20, 64, 659) == 1);    // last subframe of the window
  TESTASSERT(boundary_ahead_ms(1, 20, 64, 660) == -1);   // window over
  TESTASSERT(boundary_ahead_ms(1, 20, 64, 639) == -1);   // before the window (end of the previous period)
  TESTASSERT(boundary_sfn(1, 20, 64, 640) == 66);

  // n=2, w=20, T=256 (the shipped sib.conf.mbsfn): x=20, window [20,40) ms, SFN mod 256 = 2.
  TESTASSERT(boundary_ahead_ms(2, 20, 256, 2580) == 20); // SFN 258, subframe 0
  TESTASSERT(boundary_ahead_ms(2, 20, 256, 2585) == 15); // SFN 258, subframe 5
  TESTASSERT(boundary_ahead_ms(2, 20, 256, 2599) == 1);
  TESTASSERT(boundary_ahead_ms(2, 20, 256, 2579) == -1);
  TESTASSERT(boundary_ahead_ms(2, 20, 256, 2600) == -1);
  TESTASSERT(boundary_sfn(2, 20, 256, 2580) == 260);

  // n=2, w=40, T=256: x=40, window [40,80), SFN mod 256 = 4, boundary SFN 8 mod 256 (observed live: window SFN 516, boundary 520).
  TESTASSERT(boundary_ahead_ms(2, 40, 256, 5160) == 40);
  TESTASSERT(boundary_sfn(2, 40, 256, 5160) == 520);
  TESTASSERT(boundary_ahead_ms(2, 40, 256, 5199) == 1);
  TESTASSERT(boundary_ahead_ms(2, 40, 256, 5200) == -1);

  // A window that starts inside a frame: n=2, w=15, x=15, a=5, window [15,30) ms, boundary at 30.
  TESTASSERT(boundary_ahead_ms(2, 15, 64, 15) == 15);
  TESTASSERT(boundary_ahead_ms(2, 15, 64, 29) == 1);
  TESTASSERT(boundary_ahead_ms(2, 15, 64, 30) == -1);
  // Window length that is a multiple of 10 ending exactly on a boundary: "at or immediately after".
  TESTASSERT(boundary_ahead_ms(1, 10, 8, 79) == -1);
  TESTASSERT(boundary_ahead_ms(1, 10, 8, 0) == 10);

  // Across the SFN wrap: T=64 divides 1024, so position in the period is continuous through 10239 -> 0.
  TESTASSERT(boundary_ahead_ms(1, 20, 64, 10239) == -1);
  TESTASSERT(boundary_ahead_ms(1, 20, 64, 0) == 20);
  TESTASSERT(boundary_sfn(1, 20, 64, 9600) == 962);
  // Window that ends exactly at the SFN wrap: n=8, w=80, T=64 -> x=560, window [560,640) of the period that starts at SFN 960.
  TESTASSERT(boundary_ahead_ms(8, 80, 64, 9600 + 560) == 80);
  TESTASSERT(boundary_ahead_ms(8, 80, 64, 9600 + 639) == 1);
  TESTASSERT(boundary_sfn(8, 80, 64, 9600 + 600) == 0);
  // T=512: the position in the period is taken modulo 5120 ms.
  TESTASSERT(boundary_ahead_ms(2, 40, 512, 5120 + 40 + 10) == 30);
  return SRSRAN_SUCCESS;
}

static int test_invalid_configurations()
{
  TESTASSERT(boundary_ahead_ms(0, 20, 64, 0) == -1);   // n counts from 1
  TESTASSERT(boundary_ahead_ms(1, 0, 64, 0) == -1);    // empty window
  TESTASSERT(boundary_ahead_ms(1, 20, 0, 0) == -1);    // no period
  TESTASSERT(boundary_ahead_ms(1, 20, 3, 0) == -1);    // period does not divide 1024
  TESTASSERT(boundary_ahead_ms(5, 20, 8, 800) == -1);  // x/10 = 8 >= T = 8: the window never occurs
  TESTASSERT(boundary_ahead_ms(2, 80, 8, 800) == -1);
  return SRSRAN_SUCCESS;
}

static int test_cas_opportunity()
{
  // >= 25 PRB: CAS in frames with nf mod 4 = 0, subframe 0 only (TS 36.211 clause 6.6.4.1).
  TESTASSERT(has_cas_opportunity(1, 20, 64, false));   // [0,20): frame 0
  TESTASSERT(!has_cas_opportunity(2, 20, 256, false)); // [20,40): frames 2 and 3, no CAS (the shipped sib.conf.mbsfn)
  TESTASSERT(has_cas_opportunity(2, 40, 256, false));  // [40,80): frame 4
  TESTASSERT(has_cas_opportunity(2, 30, 256, false));  // [30,60): window starts in frame 3, holds frame 4's subframe 0
  TESTASSERT(!has_cas_opportunity(2, 15, 64, false));  // [15,30): subframe 5 of frame 1 to frame 2
  TESTASSERT(!has_cas_opportunity(2, 10, 64, false));  // [10,20)
  TESTASSERT(!has_cas_opportunity(2, 5, 64, false));   // [5,10): subframe 5 of frame 0 is not subframe 0
  TESTASSERT(has_cas_opportunity(3, 20, 256, false));  // [40,60): frame 4
  TESTASSERT(!has_cas_opportunity(4, 20, 256, false)); // [60,80): frames 6 and 7
  // 6 < PRB < 25: CAS in frames with nf mod 8 = 4.
  TESTASSERT(!has_cas_opportunity(1, 20, 64, true));   // [0,20): frames 0 and 1
  TESTASSERT(has_cas_opportunity(1, 80, 64, true));    // [0,80): frame 4
  TESTASSERT(!has_cas_opportunity(6, 10, 64, true));   // [50,60): frame 5
  TESTASSERT(has_cas_opportunity(5, 10, 64, true));    // [40,50): frame 4
  return SRSRAN_SUCCESS;
}

static int test_fields()
{
  fields_t f;
  // TS 36.331 clause 6.3.1 NOTE 1: GPS = timeInfoUTC(s) - 2,524,953,600 + leapSeconds. At UTC = GPS epoch (1980-01-06) with
  // leapSeconds = 18 the GPS time is 18 s.
  const int64_t gps_epoch_utc_1900_ns = gps_epoch_from_1900_s * 1000000000LL;
  TESTASSERT(make_fields(gps_epoch_utc_1900_ns, 18, &f));
  TESTASSERT(f.time_info_utc_10ms == (uint64_t)gps_epoch_from_1900_s * 100);
  TESTASSERT(f.ref_days == 0 && f.ref_seconds == 18 && f.ref_milli_seconds == 0 && f.ref_quarter_us == 0);

  // 0.25 us granularity: +250 ns is one quarter microsecond, +249 ns is none.
  TESTASSERT(make_fields(gps_epoch_utc_1900_ns + 250, 0, &f) && f.ref_quarter_us == 1);
  TESTASSERT(make_fields(gps_epoch_utc_1900_ns + 249, 0, &f) && f.ref_quarter_us == 0);
  TESTASSERT(make_fields(gps_epoch_utc_1900_ns + 999750, 0, &f) && f.ref_milli_seconds == 0 && f.ref_quarter_us == 3999);
  TESTASSERT(make_fields(gps_epoch_utc_1900_ns + 1000000, 0, &f) && f.ref_milli_seconds == 1 && f.ref_quarter_us == 0);

  // One day, one second, one millisecond after the GPS epoch (leapSeconds 0).
  TESTASSERT(make_fields(gps_epoch_utc_1900_ns + 86400LL * 1000000000LL + 1001000000LL, 0, &f));
  TESTASSERT(f.ref_days == 1 && f.ref_seconds == 1 && f.ref_milli_seconds == 1 && f.ref_quarter_us == 0);
  // timeInfoUTC keeps its 10 ms granularity: 19 ms truncates to one unit.
  TESTASSERT(make_fields(gps_epoch_utc_1900_ns + 19000000, 0, &f) && f.time_info_utc_10ms == (uint64_t)gps_epoch_from_1900_s * 100 + 1);

  // Out of range: before the GPS epoch, before 1900, refDays above 72999.
  TESTASSERT(!make_fields(gps_epoch_utc_1900_ns - 1, 0, &f));
  TESTASSERT(make_fields(gps_epoch_utc_1900_ns - 1000000000LL, 18, &f)); // negative GPS avoided by the leap offset
  TESTASSERT(!make_fields(-1, 18, &f));
  // timeInfoUTC (INTEGER 0..549755813887 in 10 ms units) runs out in 2074, long before refDays (0..72999) does.
  TESTASSERT(!make_fields(gps_epoch_utc_1900_ns + 73000LL * 86400LL * 1000000000LL, 0, &f));
  TESTASSERT(!make_fields(gps_epoch_utc_1900_ns + 72999LL * 86400LL * 1000000000LL, 0, &f));
  TESTASSERT(make_fields(gps_epoch_utc_1900_ns + 32850LL * 86400LL * 1000000000LL, 0, &f) && f.ref_days == 32850);
  return SRSRAN_SUCCESS;
}

int main()
{
  srslog::init();
  TESTASSERT(test_window_arithmetic() == SRSRAN_SUCCESS);
  TESTASSERT(test_invalid_configurations() == SRSRAN_SUCCESS);
  TESTASSERT(test_cas_opportunity() == SRSRAN_SUCCESS);
  TESTASSERT(test_fields() == SRSRAN_SUCCESS);
  printf("sib16_time_test: all passed\n");
  return SRSRAN_SUCCESS;
}
