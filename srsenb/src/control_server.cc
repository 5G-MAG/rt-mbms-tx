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

#include "srsenb/hdr/control_server.h"
#include "srsenb/hdr/enb.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <poll.h>
#include <set>
#include <sstream>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace srsenb {

namespace {

// Parses an unsigned integer in [0, max_val], rejecting anything else (leading/trailing
// junk, negative signs, overflow). Deliberately not boost::program_options -- see the
// uint8_t/character-code landmine documented next to enb.cc's reload_embms_config().
bool parse_uint(const std::string& v, unsigned long max_val, unsigned long& out)
{
  if (v.empty()) {
    return false;
  }
  char*         endptr = nullptr;
  errno              = 0;
  unsigned long val    = std::strtoul(v.c_str(), &endptr, 10);
  if (errno != 0 || endptr == v.c_str() || *endptr != '\0' || val > max_val) {
    return false;
  }
  out = val;
  return true;
}

bool parse_bool(const std::string& v, bool& out)
{
  if (v == "true" || v == "1") {
    out = true;
    return true;
  }
  if (v == "false" || v == "0") {
    out = false;
    return true;
  }
  return false;
}

} // namespace

control_server::control_server(enb* enb_) : enb_ptr(enb_), logger(srslog::fetch_basic_logger("ENB")) {}

control_server::~control_server()
{
  stop();
}

bool control_server::start(const std::string& socket_path_)
{
  socket_path = socket_path_;

  listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    logger.error("control_server: failed to create socket: %s", strerror(errno));
    return false;
  }

  // Remove a stale socket file left over from an unclean prior shutdown.
  unlink(socket_path.c_str());

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  if (socket_path.size() >= sizeof(addr.sun_path)) {
    logger.error("control_server: socket path too long: %s", socket_path.c_str());
    close(listen_fd);
    listen_fd = -1;
    return false;
  }
  std::strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

  if (bind(listen_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
    logger.error("control_server: failed to bind %s: %s", socket_path.c_str(), strerror(errno));
    close(listen_fd);
    listen_fd = -1;
    return false;
  }

  // Real filesystem-path socket (not Linux abstract-namespace) specifically so it can be
  // chmod'd: this endpoint can change live broadcast parameters and abstract-namespace
  // sockets have no filesystem permission model at all.
  if (chmod(socket_path.c_str(), 0600) < 0) {
    logger.error("control_server: failed to chmod %s: %s", socket_path.c_str(), strerror(errno));
    close(listen_fd);
    listen_fd = -1;
    unlink(socket_path.c_str());
    return false;
  }

  if (listen(listen_fd, 4) < 0) {
    logger.error("control_server: failed to listen on %s: %s", socket_path.c_str(), strerror(errno));
    close(listen_fd);
    listen_fd = -1;
    unlink(socket_path.c_str());
    return false;
  }

  running.store(true);
  worker = std::thread(&control_server::accept_loop, this);
  logger.info("control_server: listening on %s", socket_path.c_str());
  return true;
}

void control_server::stop()
{
  if (!running.load()) {
    return;
  }
  running.store(false);
  if (worker.joinable()) {
    worker.join();
  }
  if (listen_fd >= 0) {
    close(listen_fd);
    listen_fd = -1;
  }
  unlink(socket_path.c_str());
}

void control_server::accept_loop()
{
  struct pollfd pfd;
  pfd.fd     = listen_fd;
  pfd.events = POLLIN;
  while (running.load()) {
    pfd.revents = 0;
    int ret     = poll(&pfd, 1, 500); // 500ms so stop() is noticed promptly
    if (ret > 0 && (pfd.revents & POLLIN)) {
      int conn_fd = accept(listen_fd, nullptr, nullptr);
      if (conn_fd >= 0) {
        handle_connection(conn_fd);
        close(conn_fd);
      }
    }
  }
}

void control_server::handle_connection(int conn_fd)
{
  // A stuck or malicious local client must not hang the single accept thread indefinitely.
  struct timeval tv;
  tv.tv_sec  = 2;
  tv.tv_usec = 0;
  setsockopt(conn_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(conn_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

  static const size_t MAX_LINE = 4096;
  std::string         line;
  char                buf[256];
  while (line.size() < MAX_LINE) {
    ssize_t n = recv(conn_fd, buf, sizeof(buf), 0);
    if (n <= 0) {
      break; // EOF, error, or timeout -- process whatever was received so far
    }
    line.append(buf, static_cast<size_t>(n));
    size_t nl = line.find('\n');
    if (nl != std::string::npos) {
      line.resize(nl);
      break;
    }
  }

  if (line.empty()) {
    return;
  }

  std::string response = dispatch(line);
  ssize_t     n         = send(conn_fd, response.c_str(), response.size(), MSG_NOSIGNAL);
  (void)n;
}

std::string control_server::dispatch(const std::string& line)
{
  std::string trimmed = line;
  while (!trimmed.empty() && (trimmed.back() == '\r' || trimmed.back() == '\n')) {
    trimmed.pop_back();
  }

  size_t      sp   = trimmed.find(' ');
  std::string cmd  = (sp == std::string::npos) ? trimmed : trimmed.substr(0, sp);
  std::string rest = (sp == std::string::npos) ? std::string() : trimmed.substr(sp + 1);

  if (cmd == "GET") {
    return handle_get();
  }
  if (cmd == "SET") {
    return handle_set(rest);
  }
  return "ERROR unknown command\n";
}

std::string control_server::handle_get() const
{
  embms_args_t       cfg = enb_ptr->get_embms_config();
  std::ostringstream oss;
  oss << "embms.mcs=" << cfg.mcs << "\n";
  oss << "embms.pmch_bandwidth=" << static_cast<unsigned>(cfg.pmch_bandwidth) << "\n";
  oss << "embms.cyclic_shift_alpha=" << static_cast<unsigned>(cfg.cyclic_shift_alpha) << "\n";
  oss << "embms.freq_interleaving=" << (cfg.freq_interleaving ? "true" : "false") << "\n";
  oss << "embms.time_interleaving_n=" << static_cast<unsigned>(cfg.time_interleaving_n) << "\n";
  oss << "embms.time_interleaving_m=" << static_cast<unsigned>(cfg.time_interleaving_m) << "\n";
  oss << "embms.time_interleaving_n_last_mtch=" << static_cast<unsigned>(cfg.time_interleaving_n_last_mtch) << "\n";
  oss << "embms.time_interleaving_m_last_mtch=" << static_cast<unsigned>(cfg.time_interleaving_m_last_mtch) << "\n";
  oss << "embms.n_soft_ref_category=" << cfg.n_soft_ref_category << "\n";
  oss << "embms.scaling_factor_beta=" << cfg.scaling_factor_beta << "\n";
  oss << "embms.use_mcs_table2=" << (cfg.use_mcs_table2 ? "true" : "false") << "\n";
  oss << "embms.cas_muting=" << (cfg.cas_muting ? "true" : "false") << "\n";
  oss << "embms.k_cas=" << static_cast<unsigned>(cfg.k_cas) << "\n";
  oss << "embms.n_cas=" << static_cast<unsigned>(cfg.n_cas) << "\n";
  oss << "embms.mch_sched_period_rf=" << static_cast<unsigned>(cfg.mch_sched_period_rf) << "\n";
  oss << "embms.nof_mbms_sessions=" << static_cast<unsigned>(cfg.nof_mbms_sessions) << "\n";
  oss << "embms.time_separation_sl2=" << (cfg.pmch_time_separation_sl2 ? "true" : "false") << "\n";
  oss << "embms.subcarrier_spacing=" << cfg.pmch_subcarrier_spacing << "\n";
  oss << "embms.session_teids=" << cfg.session_teids << "\n";
  return oss.str();
}

// SET does only syntax validation (known key, correctly-typed value) -- it must not
// duplicate rrc::reconfigure_embms()'s semantic range/legality checks, which are
// authoritative and already exist. Like today's SIGHUP reload, this is fire-and-forget:
// accepted means syntactically valid and enqueued, not "confirmed applied as requested".
// Any server-side clamping still only shows up in the eNB log.
std::string control_server::handle_set(const std::string& args) const
{
  static const std::set<std::string> restart_only_keys = {"embms.enable",
                                                            "embms.mbms_dedicated",
                                                            "embms.m1u_multiaddr",
                                                            "embms.m1u_if_addr",
                                                            "embms.additional_non_mbsfn_subframes",
                                                            "embms.session_teids",
                                                            // Not part of reload_embms_config()'s live-reconfigure
                                                            // chain (unlike time_interleaving_n/m) -- only take
                                                            // effect on the next full config parse/eNB restart.
                                                            "embms.n_soft_ref_category",
                                                            "embms.scaling_factor_beta"};

  embms_args_t cfg = enb_ptr->get_embms_config();

  std::istringstream iss(args);
  std::string        token;
  while (iss >> token) {
    size_t eq = token.find('=');
    if (eq == std::string::npos || eq == 0) {
      return "ERROR malformed token: " + token + "\n";
    }
    std::string key = token.substr(0, eq);
    std::string val = token.substr(eq + 1);

    if (restart_only_keys.count(key) != 0) {
      return "ERROR key not live-reconfigurable: " + key + "\n";
    }

    bool          bval = false;
    unsigned long uval = 0;

    if (key == "embms.mcs") {
      if (!parse_uint(val, 65535, uval)) {
        return "ERROR invalid value for " + key + ": '" + val + "'\n";
      }
      cfg.mcs = static_cast<uint16_t>(uval);
    } else if (key == "embms.pmch_bandwidth") {
      if (!parse_uint(val, 255, uval)) {
        return "ERROR invalid value for " + key + ": '" + val + "'\n";
      }
      cfg.pmch_bandwidth = static_cast<uint8_t>(uval);
    } else if (key == "embms.cyclic_shift_alpha") {
      if (!parse_uint(val, 255, uval)) {
        return "ERROR invalid value for " + key + ": '" + val + "'\n";
      }
      cfg.cyclic_shift_alpha = static_cast<uint8_t>(uval);
    } else if (key == "embms.freq_interleaving") {
      if (!parse_bool(val, bval)) {
        return "ERROR invalid value for " + key + ": '" + val + "' (expected true|false)\n";
      }
      cfg.freq_interleaving = bval;
    } else if (key == "embms.time_interleaving_n") {
      if (!parse_uint(val, 255, uval)) {
        return "ERROR invalid value for " + key + ": '" + val + "'\n";
      }
      cfg.time_interleaving_n = static_cast<uint8_t>(uval);
    } else if (key == "embms.time_interleaving_m") {
      if (!parse_uint(val, 255, uval)) {
        return "ERROR invalid value for " + key + ": '" + val + "'\n";
      }
      cfg.time_interleaving_m = static_cast<uint8_t>(uval);
    } else if (key == "embms.time_interleaving_n_last_mtch") {
      if (!parse_uint(val, 255, uval)) {
        return "ERROR invalid value for " + key + ": '" + val + "'\n";
      }
      cfg.time_interleaving_n_last_mtch = static_cast<uint8_t>(uval);
    } else if (key == "embms.time_interleaving_m_last_mtch") {
      if (!parse_uint(val, 255, uval)) {
        return "ERROR invalid value for " + key + ": '" + val + "'\n";
      }
      cfg.time_interleaving_m_last_mtch = static_cast<uint8_t>(uval);
    } else if (key == "embms.use_mcs_table2") {
      if (!parse_bool(val, bval)) {
        return "ERROR invalid value for " + key + ": '" + val + "' (expected true|false)\n";
      }
      cfg.use_mcs_table2 = bval;
    } else if (key == "embms.cas_muting") {
      if (!parse_bool(val, bval)) {
        return "ERROR invalid value for " + key + ": '" + val + "' (expected true|false)\n";
      }
      cfg.cas_muting = bval;
    } else if (key == "embms.k_cas") {
      if (!parse_uint(val, 255, uval)) {
        return "ERROR invalid value for " + key + ": '" + val + "'\n";
      }
      cfg.k_cas = static_cast<uint8_t>(uval);
    } else if (key == "embms.n_cas") {
      if (!parse_uint(val, 255, uval)) {
        return "ERROR invalid value for " + key + ": '" + val + "'\n";
      }
      cfg.n_cas = static_cast<uint8_t>(uval);
    } else if (key == "embms.mch_sched_period_rf") {
      if (!parse_uint(val, 255, uval)) {
        return "ERROR invalid value for " + key + ": '" + val + "'\n";
      }
      cfg.mch_sched_period_rf = static_cast<uint8_t>(uval);
    } else if (key == "embms.nof_mbms_sessions") {
      if (!parse_uint(val, 255, uval)) {
        return "ERROR invalid value for " + key + ": '" + val + "'\n";
      }
      cfg.nof_mbms_sessions = static_cast<uint8_t>(uval);
    } else if (key == "embms.time_separation_sl2") {
      if (!parse_bool(val, bval)) {
        return "ERROR invalid value for " + key + ": '" + val + "' (expected true|false)\n";
      }
      cfg.pmch_time_separation_sl2 = bval;
    } else if (key == "embms.subcarrier_spacing") {
      cfg.pmch_subcarrier_spacing = val;
    } else {
      return "ERROR unknown key: " + key + "\n";
    }
  }

  enb_ptr->set_embms_config(cfg);
  return "OK\n";
}

} // namespace srsenb
