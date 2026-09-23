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
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <set>
#include <sstream>
#include <sys/socket.h>
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

bool control_server::start(const std::string& bind_addr_, uint16_t port_)
{
  bind_addr = bind_addr_;
  port      = port_;

  listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    logger.error("control_server: failed to create socket: %s", strerror(errno));
    return false;
  }

  // Allow immediate rebind after a restart (avoid TIME_WAIT on the control port).
  int one = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port   = htons(port);
  if (inet_pton(AF_INET, bind_addr.c_str(), &addr.sin_addr) != 1) {
    logger.error("control_server: invalid bind address: %s", bind_addr.c_str());
    close(listen_fd);
    listen_fd = -1;
    return false;
  }

  if (bind(listen_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
    logger.error("control_server: failed to bind %s:%u: %s", bind_addr.c_str(), port, strerror(errno));
    close(listen_fd);
    listen_fd = -1;
    return false;
  }

  if (listen(listen_fd, 4) < 0) {
    logger.error("control_server: failed to listen on %s:%u: %s", bind_addr.c_str(), port, strerror(errno));
    close(listen_fd);
    listen_fd = -1;
    return false;
  }

  running.store(true);
  worker = std::thread(&control_server::accept_loop, this);
  logger.info("control_server: listening on tcp %s:%u", bind_addr.c_str(), port);
  // SECURITY: unlike the previous AF_UNIX socket (chmod 0600), a TCP control endpoint has no
  // filesystem permission model and this line protocol has no authentication of its own. Anyone
  // who can reach this port can change live RF/broadcast parameters. Keep it on loopback or a
  // trusted management network (e.g. an internal Docker network) and firewall it accordingly.
  if (bind_addr != "127.0.0.1" && bind_addr != "localhost") {
    logger.warning("control_server: bound to %s:%u (not loopback) -- this TCP control endpoint has NO "
                   "authentication; restrict it to a trusted management network and firewall it.",
                   bind_addr.c_str(), port);
  }
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

  /* Reads and dispatches every complete (newline-terminated) line the client sends in
   * this one connection, not just the first -- previously, several GET/SET commands
   * piped through a single connection (e.g. "printf 'SET a\nSET b\n' | nc host port",
   * a natural way to batch commands) silently discarded every line after the first:
   * they were often already sitting in the same recv() buffer as the first line (TCP
   * doesn't preserve per-write message boundaries), and the old code truncated the
   * buffer to the first line and returned without ever looking at the rest. Confirmed
   * live during the SIB13/MBSFN campaign: batched SETs for embms.k_cas/n_cas/
   * cas_muting and embms.pmch1.time_interleaving_n were silently dropped this way,
   * with no error -- every dropped command still individually returned "OK" when
   * re-sent alone, which is what made the drops so easy to miss at the time.
   * accept_loop() still closes the connection after this one call (unchanged, still a
   * deliberately one-shot-per-connection design) -- this only fixes what happens
   * *within* that one connection when the client wrote more than one line to it. */
  static const size_t MAX_BUFFERED = 4096; // cap on data awaiting a newline, not on total lines/connection
  std::string         buf_str;
  std::string         responses;
  char                buf[256];
  bool                got_any_line = false;
  while (buf_str.size() < MAX_BUFFERED) {
    ssize_t n = recv(conn_fd, buf, sizeof(buf), 0);
    if (n <= 0) {
      break; // EOF, error, or timeout -- process whatever complete lines arrived
    }
    buf_str.append(buf, static_cast<size_t>(n));
    size_t nl;
    while ((nl = buf_str.find('\n')) != std::string::npos) {
      std::string one_line = buf_str.substr(0, nl);
      buf_str.erase(0, nl + 1);
      if (!one_line.empty()) {
        got_any_line = true;
        responses += dispatch(one_line);
      }
    }
  }

  if (!got_any_line) {
    return;
  }

  ssize_t n = send(conn_fd, responses.c_str(), responses.size(), MSG_NOSIGNAL);
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
  oss << "embms.nof_pmch=" << (1 + cfg.extra_pmch.size()) << "\n";
  for (size_t i = 0; i < cfg.extra_pmch.size(); i++) {
    const pmch_cfg_t& p      = cfg.extra_pmch[i];
    std::string       prefix = "embms.pmch" + std::to_string(i + 1) + ".";
    oss << prefix << "mcs=" << p.mcs << "\n";
    oss << prefix << "cyclic_shift_alpha=" << static_cast<unsigned>(p.cyclic_shift_alpha) << "\n";
    oss << prefix << "freq_interleaving=" << (p.freq_interleaving ? "true" : "false") << "\n";
    oss << prefix << "time_interleaving_n=" << static_cast<unsigned>(p.time_interleaving_n) << "\n";
    oss << prefix << "time_interleaving_m=" << static_cast<unsigned>(p.time_interleaving_m) << "\n";
    oss << prefix << "time_interleaving_n_last_mtch=" << static_cast<unsigned>(p.time_interleaving_n_last_mtch) << "\n";
    oss << prefix << "time_interleaving_m_last_mtch=" << static_cast<unsigned>(p.time_interleaving_m_last_mtch) << "\n";
    oss << prefix << "n_soft_ref_category=" << p.n_soft_ref_category << "\n";
    oss << prefix << "scaling_factor_beta=" << p.scaling_factor_beta << "\n";
    oss << prefix << "use_mcs_table2=" << (p.use_mcs_table2 ? "true" : "false") << "\n";
    oss << prefix << "mch_sched_period_rf=" << static_cast<unsigned>(p.mch_sched_period_rf) << "\n";
    oss << prefix << "nof_mbms_sessions=" << static_cast<unsigned>(p.nof_mbms_sessions) << "\n";
    oss << prefix << "session_teids=" << p.session_teids << "\n";
  }
  return oss.str();
}

// Parses "pmchN" from a key of the form "embms.pmchN.<field>", returning N (1-based,
// i.e. extra_pmch index N-1) and the trailing "<field>" via out-params. Returns false
// (key untouched) if the key doesn't match this shape at all, so callers can fall
// through to their own "unknown key" handling.
static bool parse_pmch_index_key(const std::string& key, uint32_t& index, std::string& field)
{
  const std::string prefix = "embms.pmch";
  if (key.size() <= prefix.size() || key.compare(0, prefix.size(), prefix) != 0) {
    return false;
  }
  size_t dot = key.find('.', prefix.size());
  if (dot == std::string::npos) {
    return false;
  }
  std::string idx_str = key.substr(prefix.size(), dot - prefix.size());
  if (idx_str.empty() || idx_str.find_first_not_of("0123456789") != std::string::npos) {
    return false;
  }
  index = static_cast<uint32_t>(std::stoul(idx_str));
  field = key.substr(dot + 1);
  return true;
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
                                                            // As of the TEID-based multi-PMCH session routing
                                                            // added to rrc.cc's resolve_pmch_sessions(),
                                                            // session_teids DOES now reach RRC live (threaded
                                                            // through reload_embms_config()). Kept restart-only
                                                            // anyway: gtpu.cc's m1u_handler (the actual M1-U
                                                            // packet-to-LCID demux) still only reads this same
                                                            // config string once at startup, not on live reload
                                                            // -- letting RRC's signalled LCID mapping change live
                                                            // while GTP-U's delivery mapping stays stale would
                                                            // silently desync the two. Revisit once GTP-U's own
                                                            // demux also live-reloads.
                                                            "embms.session_teids",
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
    } else if (key == "embms.nof_pmch") {
      /* Live-settable (unlike the plan's original "restart-only" framing) for
       * this pass, since configure_mbsfn_sibs()/pack_mcch() already recompute
       * nof_pmch fresh from cfg.extra_pmch.size() on every call and mac.cc's
       * scheduler already loops mcch.nof_pmch_info generically - there's no
       * stale-sizing hazard a restart would be protecting against. 1..15
       * (maxPMCH-PerMBSFN); 1 = today's single-PMCH behavior (extra_pmch
       * empty). New entries default-construct (pmch_cfg_t's own defaults:
       * mcs=9, no time interleaving, mch_sched_period_rf=64, 1 session). */
      if (!parse_uint(val, 15, uval) || uval < 1) {
        return "ERROR invalid value for " + key + ": '" + val + "' (must be 1..15)\n";
      }
      cfg.extra_pmch.resize(uval - 1);
    } else {
      uint32_t    pmch_idx;
      std::string pmch_field;
      if (!parse_pmch_index_key(key, pmch_idx, pmch_field)) {
        return "ERROR unknown key: " + key + "\n";
      }
      /* embms.nof_pmch must already have been set (either earlier in this
       * same SET command, or by a previous one) large enough to cover this
       * index - there is no implicit auto-grow here, so a typo'd index
       * doesn't silently create a sparse/garbage-filled extra_pmch tail. */
      if (pmch_idx < 1 || pmch_idx > cfg.extra_pmch.size()) {
        return "ERROR embms.pmch" + std::to_string(pmch_idx) + ".* set but embms.nof_pmch=" +
               std::to_string(1 + cfg.extra_pmch.size()) + " (set embms.nof_pmch=" +
               std::to_string(pmch_idx + 1) + " first, either earlier in this same command or a previous one)\n";
      }
      pmch_cfg_t& p = cfg.extra_pmch[pmch_idx - 1];
      if (pmch_field == "mcs") {
        if (!parse_uint(val, 65535, uval)) {
          return "ERROR invalid value for " + key + ": '" + val + "'\n";
        }
        p.mcs = static_cast<uint16_t>(uval);
      } else if (pmch_field == "cyclic_shift_alpha") {
        if (!parse_uint(val, 255, uval)) {
          return "ERROR invalid value for " + key + ": '" + val + "'\n";
        }
        p.cyclic_shift_alpha = static_cast<uint8_t>(uval);
      } else if (pmch_field == "freq_interleaving") {
        if (!parse_bool(val, bval)) {
          return "ERROR invalid value for " + key + ": '" + val + "' (expected true|false)\n";
        }
        p.freq_interleaving = bval;
      } else if (pmch_field == "time_interleaving_n") {
        if (!parse_uint(val, 255, uval)) {
          return "ERROR invalid value for " + key + ": '" + val + "'\n";
        }
        p.time_interleaving_n = static_cast<uint8_t>(uval);
      } else if (pmch_field == "time_interleaving_m") {
        if (!parse_uint(val, 255, uval)) {
          return "ERROR invalid value for " + key + ": '" + val + "'\n";
        }
        p.time_interleaving_m = static_cast<uint8_t>(uval);
      } else if (pmch_field == "time_interleaving_n_last_mtch") {
        if (!parse_uint(val, 255, uval)) {
          return "ERROR invalid value for " + key + ": '" + val + "'\n";
        }
        p.time_interleaving_n_last_mtch = static_cast<uint8_t>(uval);
      } else if (pmch_field == "time_interleaving_m_last_mtch") {
        if (!parse_uint(val, 255, uval)) {
          return "ERROR invalid value for " + key + ": '" + val + "'\n";
        }
        p.time_interleaving_m_last_mtch = static_cast<uint8_t>(uval);
      } else if (pmch_field == "use_mcs_table2") {
        if (!parse_bool(val, bval)) {
          return "ERROR invalid value for " + key + ": '" + val + "' (expected true|false)\n";
        }
        p.use_mcs_table2 = bval;
      } else if (pmch_field == "mch_sched_period_rf") {
        if (!parse_uint(val, 255, uval)) {
          return "ERROR invalid value for " + key + ": '" + val + "'\n";
        }
        p.mch_sched_period_rf = static_cast<uint8_t>(uval);
      } else if (pmch_field == "nof_mbms_sessions") {
        if (!parse_uint(val, 255, uval)) {
          return "ERROR invalid value for " + key + ": '" + val + "'\n";
        }
        p.nof_mbms_sessions = static_cast<uint8_t>(uval);
      } else if (pmch_field == "n_soft_ref_category") {
        if (!parse_uint(val, 65535, uval)) {
          return "ERROR invalid value for " + key + ": '" + val + "'\n";
        }
        p.n_soft_ref_category = static_cast<uint16_t>(uval);
      } else if (pmch_field == "scaling_factor_beta") {
        p.scaling_factor_beta = val;
      } else if (pmch_field == "session_teids") {
        p.session_teids = val;
      } else {
        return "ERROR unknown key: " + key + "\n";
      }
    }
  }

  enb_ptr->set_embms_config(cfg);
  return "OK\n";
}

} // namespace srsenb
