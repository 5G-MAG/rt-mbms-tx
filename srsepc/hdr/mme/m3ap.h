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

// M3AP (TS 36.444), MME side. Connects the MME to whatever MCE(s) are
// attached -- in this project, the eNB itself acts as its own distributed
// MCE (TS 23.246 clause 5.9.1), so this is MME<->eNB directly. Mirrors
// srsepc/hdr/mme/s1ap.h's own SCTP-listener structure, scoped to the 4
// procedures m3ap.h/.cc implement: M3 Setup, MBMS Session Start/Update/Stop.

#ifndef SRSEPC_M3AP_H
#define SRSEPC_M3AP_H

#include "mme_gtpc.h"
#include "srsran/asn1/gtpc_msg.h"
#include "srsran/asn1/m3ap.h"
#include "srsran/srslog/srslog.h"
#include <map>
#include <netinet/sctp.h>
#include <string>

namespace srsepc {

// TS 36.444 does not itself specify an SCTP port number for M3 (checked
// clauses 6-7 directly), but 36444 is confirmed registered to "m3ap" / "M3
// Application Part" / SCTP in the IANA Service Name and Transport Protocol
// Port Number Registry (registered 2011-02-07, assignee Dario S. Tonesi,
// Nokia Siemens Networks) -- verified directly against
// https://www.iana.org/assignments/service-names-port-numbers/ rather than
// just commonly-cited industry material. Still kept configurable via
// m3ap_args_t.bind_port, since IANA registration is a default, not a
// TS 36.444 mandate.
const uint16_t M3_PORT_DEFAULT = 36444;

typedef struct {
  std::string bind_addr = "0.0.0.0";
  uint16_t    bind_port = M3_PORT_DEFAULT;
} m3ap_args_t;

// One connected MCE (eNB), keyed by its SCTP association id -- deliberately
// NOT keyed by eNB/MCE id, sidestepping the pre-existing uint16_t (s1ap.h's
// m_active_enbs) vs uint32_t (enb_ctx_t.enb_id) key-width mismatch noted in
// mme_gtpc.h, which this module has no need to touch or reconcile.
typedef struct {
  struct sctp_sndrcvinfo sri;
  bool                   setup_complete = false;
  asn1::m3ap::global_mce_id_s global_mce_id;
  std::vector<uint16_t>       service_area_ids;
} mce_ctx_t;

// Per-(TMGI, MCE association) M3AP identifiers for an active MBMS session,
// established at Session Start and reused unchanged for Update/Stop of the
// same session (mirrors how an S1AP UE context's MME-UE-S1AP-ID/eNB-UE-S1AP-ID
// pair persists across a UE's whole lifetime, not reallocated per-procedure).
typedef struct {
  uint32_t mme_mbms_m3ap_id = 0;
  bool     mce_id_known     = false;
  uint32_t mce_mbms_m3ap_id = 0;
} m3ap_session_ids_t;

class m3ap
{
public:
  static m3ap* get_instance();
  static void  cleanup();

  bool init(const m3ap_args_t& args);
  void stop();
  int  get_m3();

  void handle_m3ap_rx_pdu(srsran::byte_buffer_t* pdu, struct sctp_sndrcvinfo* mce_sri);
  void delete_mce_ctx(int32_t assoc_id);

  // Called from mme_gtpc.cc's Sm request handlers (see project notes on
  // mbms_session_{start,update,stop}_confirm in mme_gtpc.h) to forward the
  // corresponding session control to every currently-connected MCE.
  void session_start(const std::string&                              tmgi_key,
                      const mbms_gtpc_ctx_t&                          ctx,
                      const srsran::gtpc_mbms_session_start_request&  req);
  void session_update(const std::string&                               tmgi_key,
                       const mbms_gtpc_ctx_t&                           ctx,
                       const srsran::gtpc_mbms_session_update_request&  req);
  void session_stop(const std::string& tmgi_key);

private:
  m3ap()  = default;
  ~m3ap() = default;
  static m3ap* m_instance;

  int         enb_listen();
  bool        m3ap_tx_pdu(const asn1::m3ap::m3ap_pdu_c& pdu, const struct sctp_sndrcvinfo& mce_sri);
  void        handle_initiating_message(const asn1::m3ap::m3ap_pdu_c& pdu, struct sctp_sndrcvinfo* mce_sri);
  void        handle_successful_outcome(const asn1::m3ap::m3ap_pdu_c& pdu, const struct sctp_sndrcvinfo& mce_sri);
  void        handle_unsuccessful_outcome(const asn1::m3ap::m3ap_pdu_c& pdu, const struct sctp_sndrcvinfo& mce_sri);
  void        handle_m3_setup_request(const asn1::m3ap::m3_setup_request_s& req, struct sctp_sndrcvinfo* mce_sri);

  // Finds the tmgi_key whose per-assoc session ids match (assoc_id, mme_mbms_m3ap_id) -- used to correlate an
  // incoming Session Start/Update Response/Failure (and Session Stop Response, which carries no TMGI) back to
  // the Sm-side session it belongs to. Linear scan: the number of concurrently active MBMS sessions in this
  // project is always small.
  bool find_key_by_ids(int32_t assoc_id, uint32_t mme_mbms_m3ap_id, std::string* out_key, uint32_t* out_idx);

  m3ap_args_t           m_args;
  srslog::basic_logger& m_logger = srslog::fetch_basic_logger("M3AP");

  int m_m3 = -1;

  std::map<int32_t, mce_ctx_t> m_active_mces; // keyed by SCTP association id

  // tmgi_key -> one entry per connected MCE association this session was sent to.
  std::map<std::string, std::map<int32_t, m3ap_session_ids_t>> m_session_ids;

  uint32_t m_next_mme_mbms_m3ap_id = 1;
};

} // namespace srsepc

#endif // SRSEPC_M3AP_H
