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

// Regression test for rrc::mbms_session_start()/mbms_session_stop() (the M3AP attachment points, see
// srsenb/hdr/stack/rrc/rrc.h): confirms configure_mbsfn_sibs() broadcasts the REAL TMGI/session-id once a
// session exists, instead of pack_mcch()'s pre-existing fabricated-TMGI-from-loop-index fallback.

#include "srsenb/hdr/enb.h"
#include "srsenb/test/rrc/test_helpers.h"
#include "srsran/common/test_common.h"
#include <iostream>

using namespace srsenb;

namespace {
// Captures configure_mbsfn()'s mcch argument -- the PHY-facing common struct configure_mbsfn_sibs() builds,
// carrying srsran::tmgi_t/session_id directly (not the ASN.1 mcch_msg_s, which is private to rrc).
class phy_capture_dummy : public phy_dummy
{
public:
  void configure_mbsfn(srsran::sib2_mbms_t* sib2, srsran::sib13_t* sib13, const srsran::mcch_msg_t& mcch) override
  {
    last_mcch = mcch;
    nof_calls++;
  }
  srsran::mcch_msg_t last_mcch;
  uint32_t           nof_calls = 0;
};
} // namespace

int test_mbms_session_start_stop()
{
  printf("\n===== TEST: test_mbms_session_start_stop()  =====\n");

  srsran::task_scheduler task_sched;

  srsenb::all_args_t args;
  rrc_cfg_t           cfg;
  TESTASSERT(test_helpers::parse_default_cfg(&cfg, args) == SRSRAN_SUCCESS);

  enb_bearer_manager  bearers;
  srsenb::rrc         rrc{&task_sched, bearers};
  mac_dummy           mac;
  rlc_dummy           rlc;
  test_dummies::pdcp_mobility_dummy pdcp;
  phy_capture_dummy   phy;
  test_dummies::s1ap_mobility_dummy s1ap;
  gtpu_dummy          gtpu;
  rrc.init(cfg, &phy, &mac, &rlc, &pdcp, &s1ap, &gtpu);

  // Real M3AP-derived TMGI: MCC=901, MNC=56, Service ID=0x00abcd (mirrors the M3AP eNB-side conversion in
  // srsenb/src/stack/m3ap/m3ap.cc's to_tmgi_t()).
  srsran::tmgi_t tmgi;
  tmgi.plmn_id_type = srsran::tmgi_t::plmn_id_type_t::explicit_value;
  uint16_t mcc_bcd, mnc_bcd;
  TESTASSERT(srsran::string_to_mcc("901", &mcc_bcd));
  TESTASSERT(srsran::string_to_mnc("56", &mnc_bcd));
  tmgi.plmn_id.explicit_value.from_number(mcc_bcd, mnc_bcd);
  tmgi.serviced_id[0] = 0x00;
  tmgi.serviced_id[1] = 0xab;
  tmgi.serviced_id[2] = 0xcd;

  rrc.mbms_session_start("901:56:00abcd", tmgi, 7, true);
  // configure_mbsfn_sibs() defers the actual phy->configure_mbsfn() call via defer_task(), which lands on the
  // internal task queue -- run_pending_tasks() drains that non-blockingly; run_next_task() would instead block
  // waiting on the (here, unused) external task queue.
  task_sched.run_pending_tasks();

  TESTASSERT(phy.nof_calls == 1);
  TESTASSERT(phy.last_mcch.pmch_info_list[0].nof_mbms_session_info == 1);
  const auto& info = phy.last_mcch.pmch_info_list[0].mbms_session_info_list[0];
  TESTASSERT(info.session_id_present);
  TESTASSERT(info.session_id == 7);
  TESTASSERT(info.lc_ch_id == 1);
  TESTASSERT(info.tmgi.plmn_id_type == srsran::tmgi_t::plmn_id_type_t::explicit_value);
  TESTASSERT(info.tmgi.serviced_id[1] == 0xab);
  TESTASSERT(info.tmgi.serviced_id[2] == 0xcd);

  // Stop must remove the session -- pack_mcch()/configure_mbsfn_sibs() fall back to the pre-existing
  // static/fabricated behavior again (nof_mbms_sessions from static cfg, defaulting to 1 fabricated session).
  rrc.mbms_session_stop("901:56:00abcd");
  task_sched.run_pending_tasks();
  TESTASSERT(phy.nof_calls == 2);
  TESTASSERT(phy.last_mcch.pmch_info_list[0].mbms_session_info_list[0].tmgi.plmn_id_type ==
             srsran::tmgi_t::plmn_id_type_t::plmn_idx);

  return SRSRAN_SUCCESS;
}

int main(int argc, char** argv)
{
  srslog::init();

  if (argc < 3) {
    argparse::usage(argv[0]);
    return -1;
  }
  argparse::parse_args(argc, argv);

  TESTASSERT(test_mbms_session_start_stop() == SRSRAN_SUCCESS);

  srslog::flush();
  printf("\nSuccess\n");
  return SRSRAN_SUCCESS;
}
