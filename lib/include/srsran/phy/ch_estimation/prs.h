/*
 * Positioning reference signals (TS 36.211 clause 6.10.4), transmit side.
 *
 * This file is part of rt-mbms-tx. It is distributed under the GNU Affero General Public License, as the rest of this library.
 */

#ifndef SRSRAN_PRS_H
#define SRSRAN_PRS_H

#include <stdbool.h>
#include <stdint.h>

#include "srsran/config.h"
#include "srsran/phy/common/phy_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Checks a configuration against the cell it is for. Returns SRSRAN_SUCCESS, or SRSRAN_ERROR_INVALID_INPUTS after writing the reason to `why` (at least
 * 160 bytes). A disabled configuration is always valid. For an MBMS-dedicated cell every PRS subframe must be a cell acquisition subframe: PRS is defined for
 * 15 kHz subcarrier spacing only (clause 6.10.4) and those are the only 15 kHz subframes of such a cell. */
SRSRAN_API int srsran_prs_cfg_validate(const srsran_prs_cfg_t* cfg, const srsran_cell_t* cell, char* why, uint32_t why_len);

/* T_PRS and Delta_PRS of Table 6.10.4.3-1 for a configuration index; SRSRAN_ERROR_INVALID_INPUTS for a reserved one. */
SRSRAN_API int srsran_prs_period_offset(uint32_t config_index, uint32_t* period, uint32_t* offset);

/* True if PRS is transmitted in the subframe with number `tti` = 10 n_f + subframe (taken modulo 10240): one of the N_PRS consecutive subframes of an
 * occasion, and the occasion not muted. Muting follows TS 37.355 prs-MutingInfo: bit i of the sequence is for occasion i counted from SFN 0, a 0 mutes it,
 * and when T_REP x T_PRS > 10240 only the first 10240 / T_PRS bits are used. */
SRSRAN_API bool srsran_prs_active(const srsran_prs_cfg_t* cfg, uint32_t tti);

/* Resource elements of one slot that carry the PSS, SSS or the core set of the PBCH, which PRS shall not be mapped onto (clause 6.10.4). `mask` has
 * 12 * nof_prb * symbols-per-slot entries, laid out as symbol * 12 * nof_prb + subcarrier. */
SRSRAN_API void srsran_prs_sync_pbch_mask(const srsran_cell_t* cell, uint32_t n_f, uint32_t n_s, bool* mask);

/* One PRS resource element of a subframe: its index in the subframe array (SRSRAN_RE_IDX) and its value. */
typedef struct SRSRAN_API {
  uint32_t index;
  cf_t     value;
} srsran_prs_re_t;

#define SRSRAN_PRS_MAX_RES 2400 /* 6 symbols x 2 resource elements per PRB x 100 PRB x 2 slots, rounded up */

/* The resource elements that PRS would occupy in the subframe `tti`, on the grid `sf_symbols` as it is now: not the PSS, SSS and PBCH core set, and not any
 * element already holding a value. Nothing is written. Returns the number of elements (0 if PRS is not active in the subframe) or a negative error. This is
 * what a transmitter calls once the base signals are in the grid, to write the elements later, after PDSCH, so that PRS takes the place of PDSCH but of
 * none of the base signals. */
SRSRAN_API int srsran_prs_collect_sf(const srsran_prs_cfg_t* cfg, const srsran_cell_t* cell, uint32_t tti, const cf_t* sf_symbols, srsran_prs_re_t* out, uint32_t max);

/* Writes the PRS of one slot `n_s` (number in the radio frame `n_f`) into the subframe array `sf_symbols`, whether or not PRS is active in the subframe, with
 * the same rules as srsran_prs_put_sf. Returns the number of resource elements written. For tests and for srsran_prs_put_sf. */
SRSRAN_API int srsran_prs_put_slot(const srsran_prs_cfg_t* cfg, const srsran_cell_t* cell, uint32_t n_f, uint32_t n_s, cf_t* sf_symbols);

/* Writes the PRS of the subframe `tti` into `sf_symbols` (antenna port 6 is sent from physical port 0), on the resource elements that are free: not the ones
 * of the PSS, SSS and PBCH core set, and not any already holding a value (cell-specific reference signals, repeated PBCH). Does nothing if PRS is not active in
 * the subframe. Returns the number of resource elements written, or a negative error. */
SRSRAN_API int srsran_prs_put_sf(const srsran_prs_cfg_t* cfg, const srsran_cell_t* cell, uint32_t tti, cf_t* sf_symbols);

#ifdef __cplusplus
}
#endif

#endif // SRSRAN_PRS_H
