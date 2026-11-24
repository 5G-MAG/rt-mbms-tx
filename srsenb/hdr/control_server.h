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

/******************************************************************************
 * File:        control_server.h
 * Description: TCP (AF_INET/SOCK_STREAM) control endpoint for live eMBMS
 *              reconfiguration (GET/SET of embms.* parameters), intended as
 *              the backend an external control portal talks to instead of
 *              editing enb.conf and sending SIGHUP. Binds to a configurable
 *              address:port so the portal can run in a separate container.
 *              NOTE: the line protocol is unauthenticated -- bind to loopback
 *              or a trusted management network only.
 *****************************************************************************/

#ifndef SRSENB_CONTROL_SERVER_H
#define SRSENB_CONTROL_SERVER_H

#include "srsran/srslog/srslog.h"
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace srsenb {

class enb;

class control_server
{
public:
  explicit control_server(enb* enb_);
  ~control_server();

  control_server(const control_server&) = delete;
  control_server& operator=(const control_server&) = delete;

  /// Creates a TCP socket, binds it to bind_addr:port and listens, then spawns the accept thread.
  /// Returns false on failure (logged); the caller treats this as non-fatal.
  /// SECURITY: the line protocol has no authentication -- keep bind_addr on loopback or a trusted
  /// management network (see the warning logged for non-loopback binds).
  bool start(const std::string& bind_addr, uint16_t port);

  /// Signals the accept thread to stop, joins it, then closes the listening socket.
  void stop();

private:
  void        accept_loop();
  void        handle_connection(int conn_fd);
  std::string dispatch(const std::string& line);
  std::string handle_get() const;
  std::string handle_set(const std::string& args) const;

  enb*                   enb_ptr;
  srslog::basic_logger&  logger;
  std::string            bind_addr;
  uint16_t               port = 0;
  int                    listen_fd = -1;
  std::atomic<bool>      running{false};
  std::thread            worker;
};

} // namespace srsenb

#endif // SRSENB_CONTROL_SERVER_H
