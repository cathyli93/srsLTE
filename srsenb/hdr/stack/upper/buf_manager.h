/*
 * Copyright 2013-2020 Software Radio Systems Limited
 *
 * This file is part of srsLTE.
 *
 * srsLTE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsLTE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 * Author: Qianru Li
 */

#include <queue>
#include <list>
#include <utility>
// #include <string.h>

// #include "common_enb.h"
#include "srslte/common/buffer_pool.h"
#include "srslte/common/logmap.h"
#include "srslte/common/common.h"
#include "srslte/common/block_queue.h"
// #include "srslte/common/threads.h"
#include "srslte/interfaces/enb_interfaces.h"
#include "srslte/srslte.h"

#ifndef SRSENB_BUFMNG_H
#define SRSENB_BUFMNG_H

namespace srsenb {
  class user_buffer_state {
  public:
    // user_buffer_state() {  }
    // ~user_buffer_state() { pthread_mutex_destroy(&mutex); }
    void update_buffer_state(uint32_t lcid, uint32_t nof_unread_packets, uint32_t nof_unread_bytes);
    void update_buffer_state_delta(uint32_t lcid, uint32_t delta_nof_packets, uint32_t delta_nof_bytes);
    // uint32_t get_user_nof_packets() { return user_nof_packets; }
    // uint32_t get_user_nof_bytes() { return user_nof_bytes; }
    uint32_t get_bearer_nof_packets(uint32_t lcid);
    // int get_priority_value() { return -user_nof_packets; }
    // void set_buffer_state(uint32_t nof_unread_packets, uint32_t nof_unread_bytes);
    // uint32_t compute_nof_packets();

  private:
    typedef std::pair<uint32_t, uint32_t> buffer_state_pair_t;
    typedef std::map<uint32_t, buffer_state_pair_t> lch_buffer_state_map_t;
    lch_buffer_state_map_t user_buffer_map;

    // uint32_t user_nof_packets = 0;
    // uint32_t user_nof_bytes = 0;
  };

// bool my_cmp(std::pair<uint16_t, user_buffer_state*> left, std::pair<uint16_t, user_buffer_state*> right) {
//   return left.second->get_user_nof_packets() < right.second->get_user_nof_packets();
// }

class gtpu_buffer_manager : public buffer_interface_gtpu, public buffer_interface_rlc, public buffer_interface_rrc
{
public:
  gtpu_buffer_manager() : buf_log("BUFM") { }

  // int init(rlc_interface_bufmng* rlc_);
  void init(pdcp_interface_gtpu* pdcp_);
  void stop();

  // interfaces for GTPU
  // void add_bearer(uint16_t rnti, uint32_t lcid);
  // void add_user(uint16_t rnti);
  // void rem_bearer(uint16_t rnti, uint32_t lcid);
  // void rem_user(uint16_t rnti);
  // bool check_space_new_sdu(uint16_t rnti);
  void push_sdu(uint16_t rnti, uint32_t lcid, srslte::unique_byte_buffer_t sdu);

  // interface for RLC
  void update_buffer_state(uint16_t rnti, uint32_t lcid, uint32_t nof_unread_packets, uint32_t nof_unread_bytes);

  // interface for RRC
  void rem_user(uint16_t rnti);

  // bool is_buffer_full() { return nof_packets >= BUF_CAPACITY_PKT; }

  // gtpu_interface_pdcp
  // void write_pdu(uint16_t rnti, uint32_t lcid, srslte::unique_byte_buffer_t pdu) override;

  // stack interface
  // void handle_gtpu_s1u_rx_packet(srslte::unique_byte_buffer_t pdu, const sockaddr_in& addr);
  // void handle_gtpu_m1u_rx_packet(srslte::unique_byte_buffer_t pdu, const sockaddr_in& addr);

private:

  // void update_priority_value(uint16_t rnti, uint32_t lcid);
  // uint32_t compute_nof_packets();
  void erase_oldest_and_move(uint16_t rnti, uint32_t lcid);
  void push_sdu_(uint16_t rnti, uint32_t lcid, srslte::unique_byte_buffer_t sdu);
  uint16_t get_user_to_drop(uint32_t &lcid);

  static const int COMMON_CAPACITY_PKT = 128;
  static const int BEARER_CAPACITY_PKT = 16;

  // rlc_interface_bufmng* rlc = nullptr;
  pdcp_interface_gtpu* pdcp = nullptr;

  // auto cmp = [](std::pair<uint16_t, double> left, std::pair<uint16_t, double> right) { return left.second > right.second; };
  // std::priority_queue<std::pair<uint16_t, double>, std::vector<std::pair<uint16_t, double>>, decltype(cmp)> gtpu_queue(cmp);
  
  // std::priority_queue<std::pair<uint16_t, user_buffer_state*>, std::vector<std::pair<uint16_t, user_buffer_state*>>, std::function<bool(std::pair<uint16_t, user_buffer_state*>, std::pair<uint16_t, user_buffer_state*>)>> gtpu_queue(my_cmp);
  
  typedef std::map<uint16_t, user_buffer_state>  user_buffer_state_map_t;
  user_buffer_state_map_t buffer_map;

  // std::queue<srslte::unique_byte_buffer_t> common_queue;
  // srslte::block_queue<unique_byte_buffer_t>(COMMON_CAPACITY_PKT) queue;
  typedef std::pair<uint16_t, uint32_t> pkt_identity;
  typedef std::pair<pkt_identity, srslte::unique_byte_buffer_t> pending_pkt;
  // typedef list<pending_pkt> pkt_identity;
  std::list<pending_pkt> common_queue;

  typedef std::map<uint32_t, std::list<pending_pkt>::iterator> lcid_first_pkt;
  typedef std::map<uint16_t, lcid_first_pkt> first_pkt;
  first_pkt user_first_pkt;

  typedef std::map<uint32_t, uint32_t> lcid_nof_pkts;
  typedef std::map<uint16_t, lcid_nof_pkts> user_nof_pkts;
  user_nof_pkts buffer_usage;

  // std::map<uint16_t, std::list<pending_pkt>::iterator> first_pkt_iter;
  // std::map<uint16_t, uint32_t> buffer_usage;

  // int nof_packets = 0;
  // int nof_bytes = 0;

  srslte::log_ref  buf_log;

  // pthread_rwlock_t rwlock;
  pthread_mutex_t mutex;
};

} // namespace srsenb

#endif // SRSENB_BUFMNG_H
