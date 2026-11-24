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
#ifndef SRSEPC_MME_GTPC_H
#define SRSEPC_MME_GTPC_H

#include "nas.h"
#include "srsran/asn1/gtpc.h"
#include "srsran/common/buffer_pool.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>

namespace srsepc {

class spgw;
class s1ap;

// Sm interface (MME <-> MBMS-GW session control, TS 23.246 / TS 29.274).
// State the MME needs per MBMS Bearer Context. Kept apart from the
// per-IMSI S11/EPS-bearer gtpc_ctx_t above, since MBMS bearer state is
// semantically unrelated (broadcast, not per-UE).
// Named distinctly from mbms_gw_gtpc.h's mbms_bearer_state_t: the two are
// genuinely different state machines (MME is purely reactive with no
// STARTING/STANDBY transient states), and both headers can end up included
// in the same translation unit (e.g. an integration test exercising both
// sides), where two same-named enums in one namespace would be an ODR
// violation.
enum mme_mbms_bearer_state_t {
  MME_MBMS_BEARER_NULL,
  MME_MBMS_BEARER_ACTIVE,
  MME_MBMS_BEARER_UPDATING,
  MME_MBMS_BEARER_STOPPING
};

typedef struct mbms_gtpc_ctx {
  srsran::gtpc_tmgi_ie      tmgi;
  bool                      flow_id_present = false;
  uint16_t                  flow_id         = 0;
  bool                      session_id_present = false;
  uint8_t                   session_id      = 0;
  enum mme_mbms_bearer_state_t state        = MME_MBMS_BEARER_NULL;
  uint32_t                  peer_c_teid     = 0; // MBMS-GW's F-TEID.teid (Sender F-TEID for Control Plane)
  uint32_t                  local_teid      = 0; // this MME's own control-plane TEID for this bearer
  struct sockaddr_in        peer_addr       = {}; // the originating MBMS-GW's address (for the eventual Stop Response)
  // MCEs (eNBs) that have confirmed this session over M3AP, identified by SCTP association id -- populated by
  // mbms_session_{start,update}_confirm() in mme_gtpc.cc. Deliberately NOT keyed by eNB id, sidestepping the
  // pre-existing uint16_t (s1ap.h's m_active_enbs) vs uint32_t (enb_ctx_t.enb_id) key-width mismatch.
  std::vector<int32_t> confirmed_enbs;
} mbms_gtpc_ctx_t;

class mme_gtpc : public gtpc_interface_nas
{
public:
  typedef struct gtpc_ctx {
    srsran::gtp_fteid_t mme_ctr_fteid;
    srsran::gtp_fteid_t sgw_ctr_fteid;
  } gtpc_ctx_t;

  virtual ~mme_gtpc() = default;

  static mme_gtpc* get_instance();

  bool init();
  bool send_s11_pdu(const srsran::gtpc_pdu& pdu);
  void handle_s11_pdu(srsran::byte_buffer_t* msg);

  virtual bool send_create_session_request(uint64_t imsi);
  bool         handle_create_session_response(srsran::gtpc_pdu* cs_resp_pdu);
  virtual bool send_modify_bearer_request(uint64_t imsi, uint16_t erab_to_modify, srsran::gtp_fteid_t* enb_fteid);
  void         handle_modify_bearer_response(srsran::gtpc_pdu* mb_resp_pdu);
  void         send_release_access_bearers_request(uint64_t imsi);
  virtual bool send_delete_session_request(uint64_t imsi);
  bool         handle_downlink_data_notification(srsran::gtpc_pdu* dl_not_pdu);
  void         send_downlink_data_notification_acknowledge(uint64_t imsi, enum srsran::gtpc_cause_value cause);
  virtual bool send_downlink_data_notification_failure_indication(uint64_t imsi, enum srsran::gtpc_cause_value cause);

  int get_s11();

  // Sm interface (see mbms_gtpc_ctx_t above)
  bool init_sm(const std::string& bind_addr, uint16_t bind_port);
  int  get_sm();
  void handle_sm_pdu(srsran::byte_buffer_t* msg, const struct sockaddr_in& from_addr);

  // M3AP attachment points (TS 36.444, srsepc/hdr/mme/m3ap.h) -- called by m3ap once a real MBMS Session
  // Start/Update/Stop Response or Failure arrives from a connected MCE. mce_assoc_id identifies which MCE
  // confirmed, for recording into mbms_gtpc_ctx_t::confirmed_enbs (SCTP association id, not eNB id -- see
  // m3ap.h's own note on why it avoids the s1ap.h uint16_t/enb_ctx_t uint32_t enb-id key-width mismatch).
  void mbms_session_start_confirm(const std::string& key, uint8_t cause, int32_t mce_assoc_id);
  void mbms_session_update_confirm(const std::string& key, uint8_t cause, int32_t mce_assoc_id);
  void mbms_session_stop_confirm(const std::string& key, uint8_t cause, int32_t mce_assoc_id);

private:
  mme_gtpc() = default;

  srslog::basic_logger& m_logger = srslog::fetch_basic_logger("MME GTPC");
  s1ap*                 m_s1ap;

  uint32_t                            m_next_ctrl_teid;
  std::map<uint32_t, uint64_t>        m_mme_ctr_teid_to_imsi;
  std::map<uint64_t, struct gtpc_ctx> m_imsi_to_gtpc_ctx;

  int                m_s11;
  struct sockaddr_un m_mme_addr, m_spgw_addr;

  bool     init_s11();
  uint32_t get_new_ctrl_teid();

  // Sm interface state
  int       m_sm            = -1;
  in_addr_t m_sm_bind_ipv4  = 0; // this MME's own Sm bind address, for F-TEID responses
  uint32_t  m_next_mbms_local_teid = 1;
  // Keyed by a canonical "mcc:mnc:serviceid" string (mirroring the
  // control-socket layer's own "no natively-orderable key" pattern for
  // sockaddr_in-adjacent state) rather than the raw gtpc_tmgi_ie struct,
  // which has no operator< defined.
  std::map<std::string, mbms_gtpc_ctx_t> m_tmgi_to_mbms_ctx;

  static std::string tmgi_key(const srsran::gtpc_tmgi_ie& tmgi);

  // req_header is passed alongside the unpacked body because the Response
  // must echo the Request's Sequence Number (TS 29.274 clause 7.6, needed
  // for the MBMS-GW's retransmission matching) -- not available from the
  // unpacked message body struct alone.
  void handle_mbms_session_start_request(const srsran::gtpc_mbms_session_start_request& req,
                                          const srsran::gtpc_header_t&                   req_header,
                                          const struct sockaddr_in&                      from_addr);
  void handle_mbms_session_update_request(const srsran::gtpc_mbms_session_update_request& req,
                                           const srsran::gtpc_header_t&                    req_header,
                                           const struct sockaddr_in&                       from_addr);
  void handle_mbms_session_stop_request(const srsran::gtpc_mbms_session_stop_request& req,
                                         const srsran::gtpc_header_t&                  req_header,
                                         const struct sockaddr_in&                     from_addr);

};

inline uint32_t mme_gtpc::get_new_ctrl_teid()
{
  return m_next_ctrl_teid++;
}

inline int mme_gtpc::get_s11()
{
  return m_s11;
}

inline int mme_gtpc::get_sm()
{
  return m_sm;
}

} // namespace srsepc
#endif // SRSEPC_MME_GTPC_H
