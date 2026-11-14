/*
 * Test of the positioning reference signal mapping (lib/src/phy/ch_estimation/prs.c) against golden vectors, and of its configuration checks.
 *
 * The golden file (argument 1) is written by tools/gen_prs_golden.py of rt-mbms-examples/positioning-assistance:
 *   case <name> <n_id_prs> <n_s> <n_rb_dl> <n_rb_prs> <four_ports> <n_f> <mbms_dedicated> <normal_cp> <count>   followed by <count> lines "k l re im"
 *   active <index> <n_prs> <muting or -> <count> <sum>
 */

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "srsran/phy/ch_estimation/prs.h"
#include "srsran/phy/common/phy_common.h"

static int failures = 0;

#define CHECK(cond, ...)                                                                                               \
  do {                                                                                                                 \
    if (!(cond)) {                                                                                                     \
      printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                                      \
      printf(__VA_ARGS__);                                                                                             \
      printf("\n");                                                                                                    \
      failures++;                                                                                                      \
    }                                                                                                                  \
  } while (0)

static srsran_cell_t make_cell(uint32_t nof_prb, uint32_t nof_ports, bool dedicated)
{
  srsran_cell_t cell;
  memset(&cell, 0, sizeof(cell));
  cell.nof_prb        = nof_prb;
  cell.nof_ports      = nof_ports;
  cell.id             = 1;
  cell.cp             = SRSRAN_CP_NORM;
  cell.frame_type     = SRSRAN_FDD;
  cell.mbms_dedicated = dedicated;
  return cell;
}

static srsran_prs_cfg_t make_cfg(uint32_t id, uint32_t index, uint32_t n_prs, uint32_t nof_prb)
{
  srsran_prs_cfg_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.enabled       = true;
  cfg.id            = (uint16_t)id;
  cfg.config_index  = (uint16_t)index;
  cfg.nof_subframes = (uint8_t)n_prs;
  cfg.nof_prb       = (uint8_t)nof_prb;
  return cfg;
}

static void set_muting(srsran_prs_cfg_t* cfg, const char* bits)
{
  memset(cfg->muting, 0, sizeof(cfg->muting));
  cfg->muting_len = (uint16_t)strlen(bits);
  for (size_t i = 0; i < strlen(bits); i++) {
    if (bits[i] == '1') {
      cfg->muting[i / 8] |= (uint8_t)(1u << (i % 8));
    }
  }
}

static int run_case(FILE* f, const char* header_rest, const char* name)
{
  unsigned nid, n_s, n_rb_dl, n_rb_prs, four, n_f, ded, normal_cp, count;
  if (sscanf(header_rest, "%u %u %u %u %u %u %u %u %u", &nid, &n_s, &n_rb_dl, &n_rb_prs, &four, &n_f, &ded, &normal_cp, &count) != 9) {
    printf("FAIL: bad case header for %s\n", name);
    return 1;
  }
  srsran_cell_t    cell = make_cell(n_rb_dl, four ? 4 : 2, ded != 0);
  cell.cp               = normal_cp ? SRSRAN_CP_NORM : SRSRAN_CP_EXT;
  srsran_prs_cfg_t cfg  = make_cfg(nid, 2435, 1, n_rb_prs);
  size_t           n_re = (size_t)SRSRAN_SF_LEN_RE(n_rb_dl, cell.cp);
  cf_t*            sf   = calloc(n_re, sizeof(cf_t));
  int              got  = srsran_prs_put_slot(&cfg, &cell, n_f, n_s, sf);
  CHECK(got == (int)count, "%s: wrote %d resource elements, golden has %u", name, got, count);

  /* compare: every golden element present with the right value, nothing else non-zero */
  size_t nonzero = 0;
  for (size_t i = 0; i < n_re; i++) {
    nonzero += (sf[i] != 0);
  }
  CHECK(nonzero == count, "%s: %zu non-zero elements in the grid, golden has %u", name, nonzero, count);
  uint32_t nsym = SRSRAN_CP_NSYMB(cell.cp);
  for (unsigned i = 0; i < count; i++) {
    unsigned k, l;
    double   re, im;
    if (fscanf(f, "%u %u %lf %lf", &k, &l, &re, &im) != 4) {
      printf("FAIL: truncated golden file in %s\n", name);
      free(sf);
      return 1;
    }
    cf_t v = sf[SRSRAN_RE_IDX(n_rb_dl, l + nsym * (n_s % 2), k)];
    CHECK(fabsf(crealf(v) - (float)re) < 1e-6f && fabsf(cimagf(v) - (float)im) < 1e-6f,
          "%s: element k=%u l=%u is %f%+fi, golden %f%+fi",
          name,
          k,
          l,
          crealf(v),
          cimagf(v),
          re,
          im);
  }
  free(sf);
  return 0;
}

static void run_active(const char* rest)
{
  unsigned index, n_prs, count;
  unsigned long long sum;
  char muting[1100];
  if (sscanf(rest, "%u %u %1099s %u %llu", &index, &n_prs, muting, &count, &sum) != 5) {
    printf("FAIL: bad active line\n");
    failures++;
    return;
  }
  srsran_prs_cfg_t cfg = make_cfg(1, index, n_prs, 25);
  if (strcmp(muting, "-") != 0) {
    set_muting(&cfg, muting);
  }
  unsigned long long got_sum = 0;
  unsigned           got_n   = 0;
  for (uint32_t t = 0; t < 10240; t++) {
    if (srsran_prs_active(&cfg, t)) {
      got_n++;
      got_sum += t;
    }
  }
  CHECK(got_n == count && got_sum == sum, "active index %u N_PRS %u muting %s: %u subframes (sum %llu), golden %u (sum %llu)", index, n_prs, muting, got_n, got_sum, count, sum);
  /* the subframe number wraps */
  CHECK(srsran_prs_active(&cfg, 10240 + 40) == srsran_prs_active(&cfg, 40), "index %u: tti does not wrap at 10240", index);
}

static void test_validation(void)
{
  char          why[200];
  srsran_cell_t ded   = make_cell(25, 2, true);
  srsran_cell_t plain = make_cell(25, 2, false);

  srsran_prs_cfg_t ok = make_cfg(1, 2435, 1, 25);
  CHECK(srsran_prs_cfg_validate(&ok, &ded, why, sizeof(why)) == 0, "index 2435 on a dedicated cell must be valid: %s", why);
  CHECK(srsran_prs_cfg_validate(&ok, &plain, why, sizeof(why)) == 0, "index 2435 on an ordinary cell must be valid: %s", why);

  srsran_prs_cfg_t off = make_cfg(1, 99999, 9, 3);
  off.enabled          = false;
  CHECK(srsran_prs_cfg_validate(&off, &ded, why, sizeof(why)) == 0, "a disabled configuration is always valid");

  struct {
    const char*   what;
    srsran_prs_cfg_t cfg;
    const srsran_cell_t* cell;
  } bad[] = {
      {"reserved index 2555", make_cfg(1, 2555, 1, 25), &plain},
      {"N_PRS 3", make_cfg(1, 2435, 3, 25), &plain},
      {"bandwidth 20 PRB", make_cfg(1, 2435, 1, 20), &plain},
      {"bandwidth above the cell", make_cfg(1, 2435, 1, 50), &plain},
      {"PRS ID 4096", make_cfg(4096, 2435, 1, 25), &plain},
      {"dedicated cell, subframe 1 (index 2436)", make_cfg(1, 2436, 1, 25), &ded},
      {"dedicated cell, every 5 ms (index 2400)", make_cfg(1, 2400, 1, 25), &ded},
      {"dedicated cell, N_PRS 2 (second subframe is MBSFN)", make_cfg(1, 2435, 2, 25), &ded},
  };
  for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
    int r = srsran_prs_cfg_validate(&bad[i].cfg, bad[i].cell, why, sizeof(why));
    CHECK(r != 0, "%s must be refused", bad[i].what);
    CHECK(r == 0 || strlen(why) > 10, "%s: a reason must be given", bad[i].what);
  }
  /* TDD refused */
  srsran_cell_t tdd = make_cell(25, 2, false);
  tdd.frame_type    = SRSRAN_TDD;
  CHECK(srsran_prs_cfg_validate(&ok, &tdd, why, sizeof(why)) != 0, "TDD must be refused");

  /* muting length */
  srsran_prs_cfg_t m = make_cfg(1, 2435, 1, 25);
  set_muting(&m, "101");
  CHECK(srsran_prs_cfg_validate(&m, &ded, why, sizeof(why)) != 0, "a 3 bit muting sequence must be refused");
  set_muting(&m, "10");
  CHECK(srsran_prs_cfg_validate(&m, &ded, why, sizeof(why)) == 0, "a 2 bit muting sequence is valid: %s", why);

  /* good dedicated configurations: every occasion in a cell acquisition subframe */
  uint32_t good[] = {2435, 2475, 2475 + 40, 0, 160, 480, 1120};   /* offsets 0 (or 40) of periods that are multiples of 40 */
  for (size_t i = 0; i < sizeof(good) / sizeof(good[0]); i++) {
    srsran_prs_cfg_t g = make_cfg(1, good[i], 1, 25);
    int r              = srsran_prs_cfg_validate(&g, &ded, why, sizeof(why));
    CHECK(r == 0, "index %u should be valid on a dedicated cell: %s", good[i], why);
  }
  /* a narrow dedicated cell has its cell acquisition frames at n_f mod 8 = 4 */
  srsran_cell_t narrow = make_cell(15, 2, true);
  srsran_prs_cfg_t n1  = make_cfg(1, 2435, 1, 15);
  CHECK(srsran_prs_cfg_validate(&n1, &narrow, why, sizeof(why)) != 0, "index 2435 puts PRS in frames n_f mod 4 = 0, not the narrow cell's mod 8 = 4");
  srsran_prs_cfg_t n2 = make_cfg(1, 2475 + 40, 1, 15);   /* T 80, Delta 40: subframe 0 of frames 4, 12, ... */
  CHECK(srsran_prs_cfg_validate(&n2, &narrow, why, sizeof(why)) == 0, "index 2515 matches the narrow cell's frames: %s", why);
}

static void test_collect_then_write(void)
{
  /* the transmitter's flow: collect while only the base signals are in the grid, write after PDSCH has filled the rest */
  srsran_cell_t    ded = make_cell(25, 2, true);
  srsran_prs_cfg_t cfg = make_cfg(1, 2435, 1, 25);
  size_t           n   = (size_t)SRSRAN_SF_LEN_RE(25, ded.cp);
  cf_t*            g   = calloc(n, sizeof(cf_t));
  cf_t*            ref = calloc(n, sizeof(cf_t));
  srsran_prs_re_t* res = calloc(SRSRAN_PRS_MAX_RES, sizeof(srsran_prs_re_t));

  int full = srsran_prs_put_sf(&cfg, &ded, 0, ref);
  CHECK(full > 0, "PRS in the cell acquisition subframe");

  /* a base signal sits on the first PRS element of symbol 3 (v_shift 1: k = 6 m + 4) */
  uint32_t base_idx = SRSRAN_RE_IDX(25, 3, 4);
  g[base_idx]       = 5.0f;
  int got           = srsran_prs_collect_sf(&cfg, &ded, 0, g, res, SRSRAN_PRS_MAX_RES);
  CHECK(got == full - 1, "collect must skip the occupied element: %d against %d", got, full - 1);
  size_t nonzero = 0;
  for (size_t i = 0; i < n; i++) {
    nonzero += (g[i] != 0);
  }
  CHECK(nonzero == 1 && g[base_idx] == 5.0f, "collect must not write anything");
  CHECK(srsran_prs_collect_sf(&cfg, &ded, 10, g, res, SRSRAN_PRS_MAX_RES) == 0, "nothing collected where PRS is not active");

  /* PDSCH fills everything but the base element; PRS then takes its places */
  for (size_t i = 0; i < n; i++) {
    g[i] = (i == base_idx) ? 5.0f : 3.0f;
  }
  for (int i = 0; i < got; i++) {
    g[res[i].index] = res[i].value;
  }
  size_t still_data = 0, prs = 0;
  for (size_t i = 0; i < n; i++) {
    still_data += (g[i] == 3.0f);
  }
  for (int i = 0; i < got; i++) {
    prs += (g[res[i].index] == res[i].value);
  }
  CHECK(g[base_idx] == 5.0f, "the base signal must be untouched");
  CHECK(prs == (size_t)got, "every collected element must hold its PRS value");
  CHECK(still_data == n - (size_t)got - 1, "exactly the collected elements are taken from the data: %zu", still_data);

  /* too small a buffer is an error, not a truncation */
  memset(g, 0, n * sizeof(cf_t));
  CHECK(srsran_prs_collect_sf(&cfg, &ded, 0, g, res, 10) < 0, "a buffer that is too small must be refused");
  free(g);
  free(ref);
  free(res);
}

static void test_default_id(void)
{
  /* the PRS ID is the cell identity unless one is configured (TS 36.211 clause 6.10.4.1) */
  char          why[200];
  srsran_cell_t cell = make_cell(25, 2, false);
  cell.id            = 5;
  srsran_prs_cfg_t by_default = make_cfg(SRSRAN_PRS_ID_CELL, 2402, 1, 25);
  srsran_prs_cfg_t explicit5  = make_cfg(5, 2402, 1, 25);
  srsran_prs_cfg_t other      = make_cfg(6, 2402, 1, 25);
  CHECK(srsran_prs_cfg_validate(&by_default, &cell, why, sizeof(why)) == 0, "the cell-identity marker must be valid: %s", why);
  size_t n  = (size_t)SRSRAN_SF_LEN_RE(25, cell.cp);
  cf_t*  a  = calloc(n, sizeof(cf_t));
  cf_t*  b  = calloc(n, sizeof(cf_t));
  cf_t*  c  = calloc(n, sizeof(cf_t));
  int    na = srsran_prs_put_slot(&by_default, &cell, 1, 4, a);
  int    nb = srsran_prs_put_slot(&explicit5, &cell, 1, 4, b);
  int    nc = srsran_prs_put_slot(&other, &cell, 1, 4, c);
  CHECK(na > 0 && na == nb && memcmp(a, b, n * sizeof(cf_t)) == 0, "the default ID must equal the cell identity (%d, %d)", na, nb);
  CHECK(nc > 0 && memcmp(a, c, n * sizeof(cf_t)) != 0, "a different PRS ID must give a different signal");
  free(a);
  free(b);
  free(c);
}

static void test_put_sf(void)
{
  /* a dedicated cell: PRS only in the cell acquisition subframe, nothing elsewhere or when CAS muting suppresses it */
  srsran_cell_t    ded = make_cell(25, 2, true);
  srsran_prs_cfg_t cfg = make_cfg(1, 2435, 1, 25);
  size_t           n   = (size_t)SRSRAN_SF_LEN_RE(25, ded.cp);
  cf_t*            sf  = calloc(n, sizeof(cf_t));

  CHECK(srsran_prs_put_sf(&cfg, &ded, 0, sf) > 0, "PRS must be written in the cell acquisition subframe of frame 0");
  memset(sf, 0, n * sizeof(cf_t));
  CHECK(srsran_prs_put_sf(&cfg, &ded, 10, sf) == 0, "no PRS in frame 1 (not an occasion)");
  CHECK(srsran_prs_put_sf(&cfg, &ded, 1, sf) == 0, "no PRS in subframe 1");
  CHECK(srsran_prs_put_sf(&cfg, &ded, 40, sf) > 0, "PRS in frame 4");
  memset(sf, 0, n * sizeof(cf_t));
  CHECK(srsran_prs_put_sf(&cfg, &ded, 10240 + 40, sf) > 0, "tti wraps");

  /* the guard in srsran_prs_put_sf itself, with a configuration the validator would have refused: index 2436 is active in subframe 1, an MBSFN
   * subframe of a dedicated cell, where nothing may be written */
  srsran_prs_cfg_t wrong = make_cfg(1, 2436, 1, 25);
  memset(sf, 0, n * sizeof(cf_t));
  CHECK(srsran_prs_active(&wrong, 1), "index 2436 is active in subframe 1");
  CHECK(srsran_prs_put_sf(&wrong, &ded, 1, sf) == 0, "no PRS in an MBSFN subframe of a dedicated cell, whatever the configuration");
  srsran_cell_t plain_cell = make_cell(25, 2, false);
  CHECK(srsran_prs_put_sf(&wrong, &plain_cell, 1, sf) > 0, "the same configuration writes PRS on an ordinary cell");

  /* CAS muting: in a muted frame the subframe is MBSFN and carries no PRS */
  srsran_cell_t muted = ded;
  muted.cas_muting    = true;
  muted.k_cas         = 4;
  muted.n_cas         = 2;                                /* active when n_f % 32 < 16 */
  memset(sf, 0, n * sizeof(cf_t));
  CHECK(srsran_prs_put_sf(&cfg, &muted, 10 * 4, sf) > 0, "PRS in an active cell acquisition frame (4 < 16)");
  memset(sf, 0, n * sizeof(cf_t));
  CHECK(srsran_prs_put_sf(&cfg, &muted, 10 * 16, sf) == 0, "no PRS in a muted cell acquisition frame (16 >= 16)");

  /* muting sequence: occasion k of index 2435 is subframe 40 k; '0110' mutes k = 0, 3, 4, ... */
  srsran_prs_cfg_t mc = cfg;
  set_muting(&mc, "0110");
  memset(sf, 0, n * sizeof(cf_t));
  CHECK(srsran_prs_put_sf(&mc, &ded, 0, sf) == 0, "occasion 0 muted");
  CHECK(srsran_prs_put_sf(&mc, &ded, 40, sf) > 0, "occasion 1 not muted");

  /* an element that already holds a signal is not overwritten, and not counted */
  srsran_cell_t plain = make_cell(25, 2, false);
  srsran_prs_cfg_t pc = make_cfg(2, 2402, 1, 25);   /* Delta 2: subframe 2, no sync signals there */
  memset(sf, 0, n * sizeof(cf_t));
  int full = srsran_prs_put_slot(&pc, &plain, 0, 4, sf);   /* slot 4 is in subframe 2 */
  memset(sf, 0, n * sizeof(cf_t));
  sf[SRSRAN_RE_IDX(25, 3, 5)] = 7.0f;                       /* the first PRS element of symbol 3: v_shift 2, k = 6 m + ((6 - 3 + 2) % 6) */
  int partial = srsran_prs_put_slot(&pc, &plain, 0, 4, sf);
  CHECK(partial == full - 1, "an occupied element must be skipped: %d against %d", partial, full);
  CHECK(sf[SRSRAN_RE_IDX(25, 3, 5)] == 7.0f, "an occupied element must keep its value");
  free(sf);
}

/* The PSS, SSS and PBCH-core exclusion of an MBMS-dedicated cell is in the frames with n_f mod 4 = 0 whatever the bandwidth (clauses 6.11.1.2, 6.6.4), not in the
 * frames of the cell acquisition subframes (clause 6.6.4.1: n_f mod 8 = 4 below 25 PRB). */
static void test_mask_frames(void)
{
  const uint32_t widths[] = {6, 15, 25, 50};
  for (size_t w = 0; w < sizeof(widths) / sizeof(widths[0]); w++) {
    srsran_cell_t cell    = make_cell(widths[w], 1, true);
    uint32_t      width   = SRSRAN_NRE * widths[w];
    uint32_t      centre  = 6 * widths[w];
    bool*         mask    = calloc(width * SRSRAN_CP_NORM_NSYMB, sizeof(bool));
    for (uint32_t n_f = 0; n_f < 16; n_f++) {
      srsran_prs_sync_pbch_mask(&cell, n_f, 0, mask);
      bool pss = mask[(SRSRAN_CP_NSYMB(cell.cp) - 1) * width + centre], sss = mask[(SRSRAN_CP_NSYMB(cell.cp) - 2) * width + centre];
      CHECK(pss == (n_f % 4 == 0) && sss == pss, "%u PRB, frame %u: PSS/SSS mask %d, wanted %d", widths[w], n_f, pss, n_f % 4 == 0);
      srsran_prs_sync_pbch_mask(&cell, n_f, 1, mask);
      bool pbch = mask[0 * width + centre - 36] && mask[3 * width + centre + 35];
      CHECK(pbch == (n_f % 4 == 0), "%u PRB, frame %u: PBCH core mask %d, wanted %d", widths[w], n_f, pbch, n_f % 4 == 0);
      srsran_prs_sync_pbch_mask(&cell, n_f, 10, mask);
      CHECK(!mask[(SRSRAN_CP_NSYMB(cell.cp) - 1) * width + centre], "%u PRB, frame %u: no PSS in slot 10 of an MBMS-dedicated cell", widths[w], n_f);
    }
    free(mask);
  }
}

int main(int argc, char** argv)
{
  if (argc < 2) {
    printf("usage: prs_test golden.txt\n");
    return 2;
  }
  FILE* f = fopen(argv[1], "r");
  if (!f) {
    printf("cannot open %s\n", argv[1]);
    return 2;
  }
  char line[2048];
  int  cases = 0, actives = 0;
  while (fgets(line, sizeof(line), f)) {
    if (line[0] == '#' || line[0] == '\n') {
      continue;
    }
    if (strncmp(line, "case ", 5) == 0) {
      char name[64];
      int  off = 0;
      if (sscanf(line + 5, "%63s %n", name, &off) < 1) {
        printf("FAIL: bad case line\n");
        return 1;
      }
      if (run_case(f, line + 5 + off, name)) {
        return 1;
      }
      cases++;
    } else if (strncmp(line, "active ", 7) == 0) {
      run_active(line + 7);
      actives++;
    }
  }
  fclose(f);
  CHECK(cases >= 45, "only %d golden cases read", cases);
  CHECK(actives >= 12, "only %d activity cases read", actives);
  test_validation();
  test_default_id();
  test_collect_then_write();
  test_put_sf();
  test_mask_frames();
  printf("prs_test: %d mapping cases, %d activity cases, %d failures\n", cases, actives, failures);
  return failures ? 1 : 0;
}
