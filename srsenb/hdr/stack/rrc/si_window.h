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

#ifndef SRSENB_SI_WINDOW_H
#define SRSENB_SI_WINDOW_H

#include <cstdint>

namespace srsenb {
namespace si_window {

/**
 * Whether the SI-window of SI message n contains a subframe on which an MBMS-dedicated cell can carry SI.
 *
 * The window is [x, x + w) ms from the start of the SI period, x = (n - 1)*w (TS 36.331 V19.4.0 clause 5.2.3b). An
 * MBMS-dedicated cell transmits system information in the cell acquisition subframe, subframe 0 of the frames fulfilling
 * "nf mod 4 = 0 for 25<=NRBDL" and "nf mod 8 = 4 for 6<NRBDL<25" (TS 36.211 V19.3.0 clause 6.6.4.1). A window without such a
 * subframe can never be used, so its SI message is never sent. CAS muting (cas-MutingConfig) can only remove occasions and
 * is not considered here. Every si-Periodicity is a multiple of 8 frames, so the frame within the period has the same
 * residue modulo 4 and 8 as the SFN.
 */
inline bool has_cas_opportunity(uint32_t n, uint32_t w_ms, uint32_t period_rf, bool narrow_cell)
{
  if (n == 0 || w_ms == 0 || period_rf == 0) {
    return false;
  }
  const uint32_t x = (n - 1) * w_ms;
  for (uint32_t frame = x / 10; frame <= (x + w_ms - 1) / 10 && frame < period_rf; frame++) {
    const uint32_t pos = 10 * frame; // subframe 0 of the frame
    if (pos >= x && pos < x + w_ms && (narrow_cell ? frame % 8 == 4 : frame % 4 == 0)) {
      return true;
    }
  }
  return false;
}

} // namespace si_window
} // namespace srsenb

#endif // SRSENB_SI_WINDOW_H
