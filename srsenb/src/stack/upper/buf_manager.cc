// #include "srslte/upper/gtpu.h"
#include "srsenb/hdr/stack/upper/buf_manager.h"
// #include "srslte/common/network_utils.h"
#include "srslte/common/log.h"
#include <errno.h>
#include <fcntl.h>
// #include <linux/ip.h>
#include <stdio.h>
// #include <sys/socket.h>
#include <unistd.h>

// using namespace srslte;
namespace srsenb {

void gtpu_buffer_manager::init(srsenb::pdcp_interface_gtpu* pdcp_) {
// int gtpu_buffer_manager::init(srsenb::rlc_interface_bufmng* rlc_) {
  // rlc = rlc_;
  pdcp = pdcp_;
  buf_log->set_level(srslte::LOG_LEVEL_INFO);

  // pthread_rwlock_init(&rwlock, nullptr);
  pthread_mutex_init(&mutex, NULL);
}

void gtpu_buffer_manager::stop() {
  // pthread_rwlock_wrlock(&rwlock);
  buffer_map.clear();
  // pthread_rwlock_unlock(&rwlock);
  // pthread_rwlock_destroy(&rwlock);
  pthread_mutex_destroy(&mutex);
}

void gtpu_buffer_manager::rem_user(uint16_t rnti)
{
  // pthread_rwlock_wrlock(&rwlock);
  pthread_mutex_lock(&mutex);
  buf_log->info("[buf-debug] Remove user rnti=0x%x\n", rnti);

  buffer_map.erase(rnti);
  user_first_pkt.erase(rnti);
  for (auto it = common_queue.begin(); it != common_queue.end(); ) {
    if (it->first.first == rnti)
      it = common_queue.erase(it);
    else
      it++;
  }
  buffer_usage.erase(rnti);
  pthread_mutex_unlock(&mutex);
  // pthread_rwlock_unlock(&rwlock);
}

void gtpu_buffer_manager::update_buffer_state(uint16_t rnti, uint32_t lcid, uint32_t nof_unread_packets, uint32_t nof_unread_bytes)
{
  pthread_mutex_lock(&mutex);
  // pthread_rwlock_wrlock(&rwlock);
  if (!buffer_map.count(rnti)) {
    // buf_log->info("[buf-debug] New rnti: nof_unread_packets=%u, nof_unread_bytes=%u, nof_packets=%u, nof_bytes=%u\n", nof_unread_packets, nof_unread_bytes, nof_packets, nof_bytes);
    buffer_map[rnti] = user_buffer_state();
    // gtpu_queue.push(std::make_pair(rnti, &buffer_map[rnti]))
  }
  buf_log->info("[buf-debug] Update from RLC rnti=0x%x, lcid=%u, nof_unread_packets=%u, nof_unread_bytes=%u\n", rnti, lcid, nof_unread_packets, nof_unread_bytes);
  buffer_map[rnti].update_buffer_state(lcid, nof_unread_packets, nof_unread_bytes);

  uint32_t space = BEARER_CAPACITY_PKT - nof_unread_packets;
  while (space > 0 && user_first_pkt.count(rnti) && user_first_pkt[rnti].count(lcid)) {
    buffer_map[rnti].update_buffer_state_delta(lcid, 1, user_first_pkt.at(rnti).at(lcid)->second->N_bytes);
    pdcp->write_sdu(rnti, lcid, std::move(user_first_pkt.at(rnti).at(lcid)->second));
    erase_oldest_and_move(rnti, lcid);
    space--;
  }
  buf_log->info("[buf-debug] Update after common_buf -> rlc_buf rnti=0x%x, lcid=%u, rlc buffer size=%u\n", rnti, lcid, buffer_map.at(rnti).get_bearer_nof_packets(lcid));
  // pthread_rwlock_unlock(&rwlock);
  pthread_mutex_unlock(&mutex);
}

// uint32_t gtpu_buffer_manager::compute_nof_packets()
// {
//   uint32_t total = 0;
//   for (auto it = buffer_map.begin(); it != buffer_map.end(); it++) {
//     total += it->second.compute_nof_packets();
//   }
//   return total;
// }

void gtpu_buffer_manager::push_sdu(uint16_t rnti, uint32_t lcid, srslte::unique_byte_buffer_t sdu)
{
  // pthread_rwlock_wrlock(&rwlock);
  pthread_mutex_lock(&mutex);
  if (!buffer_map.count(rnti)) {
    buffer_map[rnti] = user_buffer_state();
  }
  if (buffer_map.at(rnti).get_bearer_nof_packets(lcid) < BEARER_CAPACITY_PKT) {
    buffer_map.at(rnti).update_buffer_state_delta(lcid, 1, sdu->N_bytes);
    buf_log->info("[buf-debug] Push into RLC buffer rnti=0x%x, lcid=%u, rlc buffer size=%u\n", rnti, lcid, buffer_map.at(rnti).get_bearer_nof_packets(lcid));
    pdcp->write_sdu(rnti, lcid, std::move(sdu));
  }
  
  else if (common_queue.size() < COMMON_CAPACITY_PKT) {
    push_sdu_(rnti, lcid, std::move(sdu));
  } else {
    // uint32_t max_lcid;
    // uint16_t max_user = get_user_to_drop(max_lcid);
    // if (max_user != rnti || max_lcid != lcid) {
    //   if (max_user > 0 && max_lcid > 0)
    //     erase_oldest_and_move(max_user, max_lcid);
    //   push_sdu_(rnti, lcid, std::move(sdu));
    // }
    buf_log->info("[buf-debug] Directly drop packet rnti=0x%x, lcid=%u, size=%u\n", rnti, lcid, sdu->N_bytes);
  }
  // pthread_rwlock_unlock(&rwlock);
  pthread_mutex_unlock(&mutex);
}

uint16_t gtpu_buffer_manager::get_user_to_drop(uint32_t &lcid)
{
  // pthread_mutex_lock(&mutex);
  if (buffer_usage.size() == 0)
    return 0;
  uint16_t max_rnti = buffer_usage.begin()->first;
  // uint32_t max_lcid;
  lcid = 0;
  for (auto it = buffer_usage.begin(); it != buffer_usage.end(); it++) {
    if (it->second.size() == 0)
      continue;
    lcid = it->second.begin()->first;
    for (auto it2 = it->second.begin(); it2 != it->second.end(); it2++){
      if (it2->second > buffer_usage[max_rnti][lcid]) {
        max_rnti = it->first;
        lcid = it2->first;
      }
    }
  }
  return max_rnti;
  // pthread_mutex_unlock(&mutex);
}

void gtpu_buffer_manager::push_sdu_(uint16_t rnti, uint32_t lcid, srslte::unique_byte_buffer_t sdu)
{
  uint32_t length = common_queue.size();
  buf_log->info("[buf-debug] Push to commen buffer: rnti=0x%x, lcid=%u, common_buffer_size=%u\n", rnti, lcid, length);

  if (!buffer_usage.count(rnti)) {
    buffer_usage[rnti] = lcid_nof_pkts();
  }
  if (!buffer_usage[rnti].count(lcid)) {
    buffer_usage[rnti][lcid] = 0;
  }
  buffer_usage[rnti][lcid] += 1;
  buf_log->info("[buf-debug] Push to commen buffer: rnti=0x%x, lcid=%u, buffer_usage[rnti][lcid]=%u\n", rnti, lcid, buffer_usage[rnti][lcid]);

  std::pair<uint16_t, uint32_t> identity = {rnti, lcid};
  common_queue.push_back(std::make_pair(identity, std::move(sdu)));
  if (!user_first_pkt.count(rnti)) {
    user_first_pkt[rnti] = lcid_first_pkt();
  }
  if (!user_first_pkt[rnti].count(lcid)) {
    std::list<pending_pkt>::iterator tmp = common_queue.end();
    tmp--;
    user_first_pkt[rnti][lcid] = tmp;
  }
}

void gtpu_buffer_manager::erase_oldest_and_move(uint16_t rnti, uint32_t lcid)
{
  if (!buffer_usage.count(rnti) || !buffer_usage[rnti].count(lcid))
    return;

  // if (buffer_usage.count(rnti))
  //   buffer_usage[rnti] -= 1;
  buffer_usage[rnti][lcid] -= 1;
  std::list<pending_pkt>::iterator tmp = common_queue.erase(user_first_pkt[rnti][lcid]);
  for (; tmp != common_queue.end(); tmp++) {
    if (tmp->first.first == rnti && tmp->first.second == lcid)
      break;
  }
  if (tmp == common_queue.end()) {
    user_first_pkt[rnti].erase(lcid);
    if (user_first_pkt[rnti].size() == 0)
      user_first_pkt.erase(rnti);
    buffer_usage[rnti].erase(lcid);
    if (buffer_usage[rnti].size() == 0)
      buffer_usage.erase(rnti);
  }

}

// void gtpu_buffer_manager::user_buffer_state::update_buffer_state(uint32_t lcid, uint32_t nof_unread_packets, uint32_t nof_unread_bytes)
void user_buffer_state::update_buffer_state(uint32_t lcid, uint32_t nof_unread_packets, uint32_t nof_unread_bytes)
{
  // pthread_mutex_lock(&mutex);
  user_buffer_map[lcid] = std::make_pair(nof_unread_packets, nof_unread_bytes);
  // pthread_mutex_unlock(&mutex);
}

void user_buffer_state::update_buffer_state_delta(uint32_t lcid, uint32_t delta_nof_packets, uint32_t delta_nof_bytes)
{
  // pthread_mutex_lock(&mutex);
  if (!user_buffer_map.count(lcid)) {
    user_buffer_map[lcid] = std::make_pair(0, 0);
  }
  user_buffer_map[lcid].first += delta_nof_packets;
  user_buffer_map[lcid].second += delta_nof_bytes;
  // pthread_mutex_unlock(&mutex);
}

// void gtpu_buffer_manager::user_buffer_state::set_buffer_state(uint32_t nof_unread_packets, uint32_t nof_unread_bytes)
// void user_buffer_state::set_buffer_state(uint32_t nof_unread_packets, uint32_t nof_unread_bytes)
// {
//   pthread_mutex_lock(&mutex);
//   user_nof_packets = nof_unread_packets;
//   user_nof_bytes = nof_unread_bytes;
//   pthread_mutex_unlock(&mutex);
// }

// uint32_t user_buffer_state::compute_nof_packets()
// {
//   pthread_mutex_lock(&mutex);
//   uint32_t total = 0;
//   for (auto it=user_buffer_map.begin(); it != user_buffer_map.end(); it++) {
//     total += it->second.first;
//   }
//   pthread_mutex_unlock(&mutex);
//   return total;
// }

uint32_t user_buffer_state::get_bearer_nof_packets(uint32_t lcid)
{
  // pthread_mutex_lock(&mutex);
  uint32_t ret = 0;
  if (user_buffer_map.count(lcid))
    ret = user_buffer_map.at(lcid).first;
  // pthread_mutex_unlock(&mutex);
  return ret;
}

} // namespace srsenb