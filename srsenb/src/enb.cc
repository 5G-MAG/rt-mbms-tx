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

#include "srsenb/hdr/enb.h"
#include "srsenb/hdr/stack/enb_stack_lte.h"
#include "srsenb/hdr/stack/gnb_stack_nr.h"
#include "srsenb/hdr/x2_adapter.h"
#include "srsenb/src/enb_cfg_parser.h"
#include "srsran/build_info.h"
#include "srsran/common/enb_events.h"
#include "srsran/radio/radio_null.h"
#include <boost/program_options.hpp>
#include <boost/program_options/parsers.hpp>
#include <fstream>
#include <iostream>

namespace srsenb {

enb::enb(srslog::sink& log_sink) :
  started(false), log_sink(log_sink), enb_log(srslog::fetch_basic_logger("ENB", log_sink, false)), sys_proc(enb_log)
{
  // print build info
  std::cout << std::endl << get_build_string() << std::endl << std::endl;
}

enb::~enb()
{
  eutra_stack.reset();
  nr_stack.reset();
}

int enb::init(const all_args_t& args_)
{
  int ret = SRSRAN_SUCCESS;

  // Init eNB log
  enb_log.set_level(srslog::basic_levels::info);
  enb_log.info("%s", get_build_string().c_str());

  // Validate arguments
  if (parse_args(args_, rrc_cfg, rrc_nr_cfg)) {
    srsran::console("Error processing arguments.\n");
    return SRSRAN_ERROR;
  }

  srsran::byte_buffer_pool::get_instance()->enable_logger(true);

  // Create layers
  std::unique_ptr<enb_stack_lte> tmp_eutra_stack;
  if (not rrc_cfg.cell_list.empty()) {
    // add EUTRA stack
    tmp_eutra_stack.reset(new enb_stack_lte(log_sink));
    if (tmp_eutra_stack == nullptr) {
      srsran::console("Error creating EUTRA stack.\n");
      return SRSRAN_ERROR;
    }
  }

#ifdef SRSENB_HAS_5GNR_STACK
  std::unique_ptr<gnb_stack_nr> tmp_nr_stack;
  if (not rrc_nr_cfg.cell_list.empty()) {
    // add NR stack
    tmp_nr_stack.reset(new gnb_stack_nr(log_sink));
    if (tmp_nr_stack == nullptr) {
      srsran::console("Error creating NR stack.\n");
      return SRSRAN_ERROR;
    }
  }

  // If NR and EUTRA stacks were initiated, create an X2 adapter between the two.
  if (tmp_nr_stack != nullptr and tmp_eutra_stack != nullptr) {
    x2.reset(new x2_adapter(tmp_eutra_stack.get(), tmp_nr_stack.get()));
  }
#endif

  // Radio and PHY are RAT agnostic
  std::unique_ptr<srsran::radio> tmp_radio = std::unique_ptr<srsran::radio>(new srsran::radio);
  if (tmp_radio == nullptr) {
    srsran::console("Error creating radio multi instance.\n");
    return SRSRAN_ERROR;
  }

  std::unique_ptr<srsenb::phy> tmp_phy = std::unique_ptr<srsenb::phy>(new srsenb::phy(log_sink));
  if (tmp_phy == nullptr) {
    srsran::console("Error creating PHY instance.\n");
    return SRSRAN_ERROR;
  }

  // initialize layers, if they exist
  if (tmp_eutra_stack) {
    if (tmp_eutra_stack->init(args.stack, rrc_cfg, tmp_phy.get(), x2.get()) != SRSRAN_SUCCESS) {
      srsran::console("Error initializing EUTRA stack.\n");
      ret = SRSRAN_ERROR;
    }
  }

#ifdef SRSENB_HAS_5GNR_STACK
  if (tmp_nr_stack) {
    if (tmp_nr_stack->init(args.nr_stack, rrc_nr_cfg, tmp_phy.get(), x2.get()) != SRSRAN_SUCCESS) {
      srsran::console("Error initializing NR stack.\n");
      ret = SRSRAN_ERROR;
    }
  }
#endif

  // Init Radio
  if (tmp_radio->init(args.rf, tmp_phy.get())) {
    srsran::console("Error initializing radio.\n");
    return SRSRAN_ERROR;
  }

  // Derive MBSFN SCS for SDR sample rate before PHY starts (SCS not yet in srsran_cell_t).
  {
    const auto& scs_str = args.stack.embms.pmch_subcarrier_spacing;
    if (scs_str == "khz0dot37" || scs_str == "khz0dot37sl4" || scs_str == "khz0dot37sl2") {
      args.phy.mbsfn_scs = SRSRAN_SCS_370HZ;
    }
    // Other SCS values fall back to srsran_sampling_freq_hz() inside srsran_sampling_freq_hz_scs().
  }

  // Only Init PHY if radio could be initialized
  if (ret == SRSRAN_SUCCESS) {
    int phy_ret;
#ifdef SRSENB_HAS_5GNR_STACK
    if (tmp_nr_stack) {
      phy_ret = tmp_phy->init(args.phy, phy_cfg, tmp_radio.get(), tmp_eutra_stack.get(), *tmp_nr_stack, this);
    } else
#endif
    {
      phy_ret = tmp_phy->init(args.phy, phy_cfg, tmp_radio.get(), tmp_eutra_stack.get(), this);
    }
    if (phy_ret) {
      srsran::console("Error initializing PHY.\n");
      ret = SRSRAN_ERROR;
    }
  }

  if (tmp_eutra_stack) {
    eutra_stack = std::move(tmp_eutra_stack);
  }
  if (eutra_stack && args.control.enable) {
    ctrl_server.reset(new control_server(this));
    if (!ctrl_server->start(args.control.socket_path)) {
      enb_log.error("Failed to start control server on %s", args.control.socket_path.c_str());
      ctrl_server.reset();
    }
  }
#ifdef SRSENB_HAS_5GNR_STACK
  if (tmp_nr_stack) {
    nr_stack = std::move(tmp_nr_stack);
  }
#endif
  phy   = std::move(tmp_phy);
  radio = std::move(tmp_radio);

  started = true; // set to true in any case to allow stopping the eNB if an error happened

  // Now that everything is setup, log sector start events.
  // SIB9 (HeNB name) is optional and, when configured, is not guaranteed to
  // land at index 8 of rrc_cfg.sibs (that index reflects config file order,
  // not SIB type number) - check the actual discriminator before accessing
  // it as sib9 rather than assuming, which previously logged a spurious
  // "Invalid field access for choice type" ASN.1 error for any config
  // (like this FeMBMS/MBSFN template) that doesn't configure SIB9 at all.
  std::string sib9_hnb_name;
  if (rrc_cfg.sibs[8].type().value == asn1::rrc::sib_info_item_c::types_opts::sib9 &&
      rrc_cfg.sibs[8].sib9().hnb_name_present) {
    sib9_hnb_name = rrc_cfg.sibs[8].sib9().hnb_name.to_string();
  }
  for (unsigned i = 0, e = rrc_cfg.cell_list.size(); i != e; ++i) {
    event_logger::get().log_sector_start(i, rrc_cfg.cell_list[i].pci, rrc_cfg.cell_list[i].cell_id, sib9_hnb_name);
  }

  if (ret == SRSRAN_SUCCESS) {
    srsran::console("\n==== eNodeB started ===\n");
    srsran::console("Type <t> to view trace\n");
  } else {
    // if any of the layers failed to start, make sure the rest is stopped in a controlled manner
    stop();
  }

  return ret;
}

void enb::stop()
{
  if (started) {
    // tear down in reverse order
    if (phy) {
      phy->stop();
    }

    if (radio) {
      radio->stop();
    }

    if (ctrl_server) {
      ctrl_server->stop();
      ctrl_server.reset();
    }

    if (eutra_stack) {
      eutra_stack->stop();
    }

    if (nr_stack) {
      nr_stack->stop();
    }

    // Now that everything is teared down, log sector stop events.
    // See the matching check in init() above for why this can't assume
    // sibs[8] is sib9.
    std::string sib9_hnb_name;
    if (rrc_cfg.sibs[8].type().value == asn1::rrc::sib_info_item_c::types_opts::sib9 &&
        rrc_cfg.sibs[8].sib9().hnb_name_present) {
      sib9_hnb_name = rrc_cfg.sibs[8].sib9().hnb_name.to_string();
    }
    for (unsigned i = 0, e = rrc_cfg.cell_list.size(); i != e; ++i) {
      event_logger::get().log_sector_stop(i, rrc_cfg.cell_list[i].pci, rrc_cfg.cell_list[i].cell_id, sib9_hnb_name);
    }

    started = false;
  }
}

int enb::parse_args(const all_args_t& args_, rrc_cfg_t& rrc_cfg_, rrc_nr_cfg_t& rrc_cfg_nr_)
{
  // set member variable
  args = args_;
  return enb_conf_sections::parse_cfg_files(&args, &rrc_cfg_, &rrc_cfg_nr_, &phy_cfg);
}

embms_args_t enb::get_embms_config() const
{
  std::lock_guard<std::mutex> lock(embms_cfg_mutex);
  return args.stack.embms;
}

void enb::set_embms_config(const embms_args_t& embms_cfg)
{
  {
    std::lock_guard<std::mutex> lock(embms_cfg_mutex);
    args.stack.embms = embms_cfg;
  }
  enb_log.info("Applying eMBMS config update");
  if (eutra_stack) {
    eutra_stack->reload_embms_config(embms_cfg.pmch_bandwidth,
                                     embms_cfg.mcs,
                                     embms_cfg.time_interleaving_n,
                                     embms_cfg.time_interleaving_m,
                                     embms_cfg.time_interleaving_n_last_mtch,
                                     embms_cfg.time_interleaving_m_last_mtch,
                                     embms_cfg.cyclic_shift_alpha,
                                     embms_cfg.freq_interleaving,
                                     embms_cfg.use_mcs_table2,
                                     embms_cfg.cas_muting,
                                     embms_cfg.k_cas,
                                     embms_cfg.n_cas,
                                     embms_cfg.mch_sched_period_rf,
                                     embms_cfg.nof_mbms_sessions,
                                     embms_cfg.pmch_time_separation_sl2,
                                     embms_cfg.pmch_subcarrier_spacing);
  }
}

void enb::reload_embms_config()
{
  if (args.enb_files.config_file.empty()) {
    enb_log.warning("reload_embms_config: config file path not set — cannot reload");
    return;
  }
  namespace bpo = boost::program_options;
  embms_args_t embms = get_embms_config();
  bpo::options_description od("embms reload");
  // clang-format off
  od.add_options()
    // NOTE: see the matching comment in srsenb/src/main.cc -- uint8_t is char-sized, so
    // boost::program_options must never bind bpo::value<uint8_t> directly to these
    // fields (it parses char-sized targets by character code, not numeric value).
    // Parse into uint16_t and narrow-cast via a notifier instead.
    ("embms.mcs",                   bpo::value<uint16_t>(&embms.mcs)->default_value(embms.mcs))
    ("embms.pmch_bandwidth",        bpo::value<uint16_t>()->default_value(embms.pmch_bandwidth)->notifier([&embms](uint16_t v) { embms.pmch_bandwidth = static_cast<uint8_t>(v); }))
    ("embms.cyclic_shift_alpha",    bpo::value<uint16_t>()->default_value(embms.cyclic_shift_alpha)->notifier([&embms](uint16_t v) { embms.cyclic_shift_alpha = static_cast<uint8_t>(v); }))
    ("embms.freq_interleaving",     bpo::value<bool>(&embms.freq_interleaving)->default_value(embms.freq_interleaving))
    ("embms.time_interleaving_n",   bpo::value<uint16_t>()->default_value(embms.time_interleaving_n)->notifier([&embms](uint16_t v) { embms.time_interleaving_n = static_cast<uint8_t>(v); }))
    ("embms.time_interleaving_m",   bpo::value<uint16_t>()->default_value(embms.time_interleaving_m)->notifier([&embms](uint16_t v) { embms.time_interleaving_m = static_cast<uint8_t>(v); }))
    ("embms.time_interleaving_n_last_mtch", bpo::value<uint16_t>()->default_value(embms.time_interleaving_n_last_mtch)->notifier([&embms](uint16_t v) { embms.time_interleaving_n_last_mtch = static_cast<uint8_t>(v); }))
    ("embms.time_interleaving_m_last_mtch", bpo::value<uint16_t>()->default_value(embms.time_interleaving_m_last_mtch)->notifier([&embms](uint16_t v) { embms.time_interleaving_m_last_mtch = static_cast<uint8_t>(v); }))
    ("embms.use_mcs_table2",        bpo::value<bool>(&embms.use_mcs_table2)->default_value(embms.use_mcs_table2))
    ("embms.cas_muting",            bpo::value<bool>(&embms.cas_muting)->default_value(embms.cas_muting))
    ("embms.k_cas",                 bpo::value<uint16_t>()->default_value(embms.k_cas)->notifier([&embms](uint16_t v) { embms.k_cas = static_cast<uint8_t>(v); }))
    ("embms.n_cas",                 bpo::value<uint16_t>()->default_value(embms.n_cas)->notifier([&embms](uint16_t v) { embms.n_cas = static_cast<uint8_t>(v); }))
    ("embms.mch_sched_period_rf",   bpo::value<uint16_t>()->default_value(embms.mch_sched_period_rf)->notifier([&embms](uint16_t v) { embms.mch_sched_period_rf = static_cast<uint8_t>(v); }))
    ("embms.nof_mbms_sessions",     bpo::value<uint16_t>()->default_value(embms.nof_mbms_sessions)->notifier([&embms](uint16_t v) { embms.nof_mbms_sessions = static_cast<uint8_t>(v); }))
    ("embms.time_separation_sl2",   bpo::value<bool>(&embms.pmch_time_separation_sl2)->default_value(embms.pmch_time_separation_sl2))
    ("embms.subcarrier_spacing",    bpo::value<std::string>(&embms.pmch_subcarrier_spacing)->default_value(embms.pmch_subcarrier_spacing));
  // clang-format on
  std::ifstream conf(args.enb_files.config_file);
  if (!conf) {
    enb_log.error("reload_embms_config: cannot open %s", args.enb_files.config_file.c_str());
    return;
  }
  try {
    bpo::variables_map vm;
    bpo::store(bpo::parse_config_file(conf, od, true /* allow unregistered */), vm);
    bpo::notify(vm);
  } catch (const bpo::error& e) {
    enb_log.error("reload_embms_config: parse error: %s", e.what());
    return;
  }
  enb_log.info("Reloading EMBMS config from %s", args.enb_files.config_file.c_str());
  set_embms_config(embms);
}

void enb::reload_sib12(bool activate)
{
  if (eutra_stack) {
    eutra_stack->reload_sib12(activate);
  }
}

void enb::start_plot()
{
  phy->start_plot();
}

void enb::print_pool()
{
  srsran::byte_buffer_pool::get_instance()->print_all_buffers();
}

bool enb::get_metrics(enb_metrics_t* m)
{
  if (!started) {
    return false;
  }
  radio->get_metrics(&m->rf);
  phy->get_metrics(m->phy);
  if (eutra_stack) {
    eutra_stack->get_metrics(&m->stack);
  }
  if (nr_stack) {
    nr_stack->get_metrics(&m->nr_stack);
  }
  m->running = true;
  m->sys     = sys_proc.get_metrics();
  return true;
}

void enb::cmd_cell_gain(uint32_t cell_id, float gain)
{
  phy->cmd_cell_gain(cell_id, gain);
}

std::string enb::get_build_mode()
{
  return std::string(srsran_get_build_mode());
}

std::string enb::get_build_info()
{
  if (std::string(srsran_get_build_info()).find("  ") != std::string::npos) {
    return std::string(srsran_get_version());
  }
  return std::string(srsran_get_build_info());
}

std::string enb::get_build_string()
{
  std::stringstream ss;
  ss << "Built in " << get_build_mode() << " mode using " << get_build_info() << ".";
  return ss.str();
}

void enb::toggle_padding()
{
  if (!started) {
    return;
  }
  if (eutra_stack) {
    eutra_stack->toggle_padding();
  }
}

void enb::tti_clock()
{
  if (!started) {
    return;
  }
  if (eutra_stack) {
    eutra_stack->tti_clock();
  }
  if (nr_stack) {
    nr_stack->tti_clock();
  }
}

} // namespace srsenb
