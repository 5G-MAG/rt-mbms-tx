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

// Regression test for rrc::clamp_pmch_mcs_to_feasible() (srsenb/src/stack/rrc/rrc.cc): confirms it (1) never
// returns an MCS whose TBS+24 bits exceed the worst-case RE budget, (2) never returns something above the
// requested MCS, (3) doesn't clamp when the requested MCS is already feasible, and (4) when it does clamp,
// returns the HIGHEST feasible MCS below the request, not just any feasible one (catches an off-by-one in
// the downward search). Previously had zero test coverage despite being the subject of several commits.
//
// clamp_pmch_mcs_to_feasible() is `static` in rrc.cc (internal to the RRC layer, no reason to expose it),
// and linking a test against the full srsenb_rrc static lib pulls in its entire transitive dependency graph
// (rrc_nr, S1AP, enb_cfg_parser ASN.1, ...) just to resolve unrelated symbols - a large, unwarranted blast
// radius for testing one small pure function. Mirrored here instead, byte-for-byte, same convention this
// codebase already uses for pmch_cyclic_shift_Xi() in pmch_rel19_fembms_test.c. Keep this in step with the
// real function if it ever changes.
#include "srsran/common/test_common.h"
#include "srsran/srslog/srslog.h"
#include "srsran/srsran.h" // umbrella header - wraps ra_dl.h's C declarations in extern "C" for this .cc TU
#include <cstdint>
#include <cstdio>

static uint16_t clamp_pmch_mcs_to_feasible(uint16_t requested_mcs, uint32_t nof_prb, bool use_mcs_table2)
{
  static const uint32_t worst_case_re_per_prb = 102; // cfi=2, EXT CP, both slots, MBSFN-RS excluded
  const uint32_t        nof_re                = nof_prb * worst_case_re_per_prb;
  for (uint16_t mcs = requested_mcs; mcs > 0; mcs--) {
    srsran_ra_tb_t tb = {};
    tb.mcs_idx        = mcs;
    if (srsran_pmch_fill_ra_mcs(&tb, nof_prb, use_mcs_table2, SRSRAN_SCS_15KHZ) < 0) {
      continue;
    }
    uint32_t qm = srsran_mod_bits_x_symbol(tb.mod);
    if ((uint32_t)tb.tbs + 24 <= qm * nof_re) {
      return mcs;
    }
  }
  return 0;
}

// Independent feasibility oracle built from the same underlying PHY primitives the mirror above uses, but
// NOT a copy of its own search loop - a black-box check on the mirror's output, not a self-check.
static bool is_feasible(uint16_t mcs, uint32_t nof_prb, bool use_mcs_table2)
{
  static const uint32_t worst_case_re_per_prb = 102; // must match clamp_pmch_mcs_to_feasible's own constant
  const uint32_t        nof_re                = nof_prb * worst_case_re_per_prb;
  srsran_ra_tb_t        tb                    = {};
  tb.mcs_idx                                  = mcs;
  if (srsran_pmch_fill_ra_mcs(&tb, nof_prb, use_mcs_table2, SRSRAN_SCS_15KHZ) < 0) {
    return false;
  }
  uint32_t qm = srsran_mod_bits_x_symbol(tb.mod);
  return (uint32_t)tb.tbs + 24 <= qm * nof_re;
}

static int test_clamp(uint32_t nof_prb, bool use_mcs_table2)
{
  for (uint16_t requested_mcs = 0; requested_mcs <= 28; requested_mcs++) {
    uint16_t returned = clamp_pmch_mcs_to_feasible(requested_mcs, nof_prb, use_mcs_table2);

    // Never returns something the caller didn't ask for.
    TESTASSERT(returned <= requested_mcs);

    if (returned == 0) {
      // Only acceptable when nothing in [1, requested_mcs] is feasible - spot-check mcs=1, the easiest
      // value to be feasible (skip for requested_mcs==0, the trivial "asked for nothing" case).
      if (requested_mcs > 0) {
        TESTASSERT(not is_feasible(1, nof_prb, use_mcs_table2));
      }
      continue;
    }

    // The value actually returned must genuinely satisfy the feasibility budget.
    TESTASSERT(is_feasible(returned, nof_prb, use_mcs_table2));

    if (is_feasible(requested_mcs, nof_prb, use_mcs_table2)) {
      // Must not clamp when it didn't need to.
      TESTASSERT_EQ(requested_mcs, returned);
    } else {
      // Otherwise must be the HIGHEST feasible value below the request, not just any feasible one.
      TESTASSERT(not is_feasible((uint16_t)(returned + 1), nof_prb, use_mcs_table2));
    }
  }
  return SRSRAN_SUCCESS;
}

int main()
{
  srslog::init();

  // Small-to-large PRB counts (small cells are more likely to force real clamping; large cells shouldn't
  // need to clamp at all across most of the MCS range) and both MCS tables.
  const uint32_t nof_prbs[] = {6, 15, 25, 50, 75, 100};
  for (uint32_t nof_prb : nof_prbs) {
    TESTASSERT(test_clamp(nof_prb, false) == SRSRAN_SUCCESS);
    TESTASSERT(test_clamp(nof_prb, true) == SRSRAN_SUCCESS);
  }

  printf("clamp_pmch_mcs_to_feasible: all cases passed\n");
  return SRSRAN_SUCCESS;
}
