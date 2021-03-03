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

  pthread_rwlock_init(&rwlock, nullptr);
}

void gtpu_buffer_manager::stop() {
  pthread_rwlock_wrlock(&rwlock);
  buffer_map.clear();
  pthread_rwlock_unlock(&rwlock);
  pthread_rwlock_destroy(&rwlock);
}

void gtpu_buffer_manager::rem_user(uint16_t rnti)
{
  pthread_rwlock_wrlock(&rwlock);
  buf_log->info("[buf-debug] Remove user rnti=0x%x\n", rnti);

  if (buffer_map.count(rnti)) {
  	nof_packets -= buffer_map[rnti].get_user_nof_packets();
  	nof_bytes -= buffer_map[rnti].get_user_nof_bytes();

	// buffer_map[rnti].set_buffer_state(BUF_CAPACITY_PKT * 3, BUF_CAPACITY_PKT * 5000);
	// if (gtpu_queue.top().first == rnti) 
	//   gtpu_queue.pop();
	  buffer_map.erase(rnti);
  }
  pthread_rwlock_unlock(&rwlock);
}

void gtpu_buffer_manager::update_buffer_state(uint16_t rnti, uint32_t lcid, uint32_t nof_unread_packets, uint32_t nof_unread_bytes)
{
  // if (buffer_map.count(rnti)) {
  //   buf_log->info("[buf-debug] Before update: nof_unread_packets=%u, nof_unread_bytes=%u, nof_packets=%u, nof_bytes=%u\n", nof_unread_packets, nof_unread_bytes, nof_packets, nof_bytes);
  // 	nof_packets -= buffer_map[rnti].get_user_nof_packets();
  // 	nof_bytes -= buffer_map[rnti].get_user_nof_bytes();
  // }
  // else {
  //   buf_log->info("[buf-debug] New rnti: nof_unread_packets=%u, nof_unread_bytes=%u, nof_packets=%u, nof_bytes=%u\n", nof_unread_packets, nof_unread_bytes, nof_packets, nof_bytes);
  // 	buffer_map[rnti] = user_buffer_state();
  // 	// gtpu_queue.push(std::make_pair(rnti, &buffer_map[rnti]))
  // }
  // buffer_map[rnti].update_buffer_state(lcid, nof_unread_packets, nof_unread_bytes);
  // nof_packets += buffer_map[rnti].get_user_nof_packets();
  // nof_bytes += buffer_map[rnti].get_user_nof_bytes();

  // buf_log->info("[buf-debug] After update new nof_packets=%u, new nof_bytes=%u\n", nof_packets, nof_bytes);
  pthread_rwlock_wrlock(&rwlock);
  if (!buffer_map.count(rnti)) {
    // buf_log->info("[buf-debug] New rnti: nof_unread_packets=%u, nof_unread_bytes=%u, nof_packets=%u, nof_bytes=%u\n", nof_unread_packets, nof_unread_bytes, nof_packets, nof_bytes);
    buffer_map[rnti] = user_buffer_state();
    // gtpu_queue.push(std::make_pair(rnti, &buffer_map[rnti]))
  }
  buf_log->info("[buf-debug] Update rnti=0x%x, lcid=%u, nof_unread_packets=%u, nof_unread_bytes=%u\n", rnti, lcid, nof_unread_packets, nof_unread_bytes);
  buffer_map[rnti].update_buffer_state(lcid, nof_unread_packets, nof_unread_bytes);

  uint32_t space = BEARER_CAPACITY_PKT - nof_unread_packets;
  while (space > 0 && first_pkt_iter.count(rnti)) {
    buffer_map[rnti].update_buffer_state_delta(lcid, 1, first_pkt_iter.at(rnti)->second->N_bytes);
    pdcp->write_sdu(rnti, lcid, std::move(first_pkt_iter.at(rnti)->second));
    erase_oldest_and_move(rnti);
  }
  // if (nof_unread_packets < BEARER_CAPACITY_PKT) {
  //   if (first_pkt_iter.count(rnti) && first_pkt_iter.at(rnti) != common_queue.end()) {
  //     pdcp->write_sdu(rnti, lcid, std::move(first_pkt_iter.at(rnti)->second));
  //     erase_oldest_and_move(rnti);
  //   }
  // }
  pthread_rwlock_unlock(&rwlock);
  // nof_packets += buffer_map[rnti].get_user_nof_packets();
  // nof_bytes += buffer_map[rnti].get_user_nof_bytes();
}

uint32_t gtpu_buffer_manager::compute_nof_packets()
{
  uint32_t total = 0;
  for (auto it = buffer_map.begin(); it != buffer_map.end(); it++) {
    total += it->second.compute_nof_packets();
  }
  return total;
}

void gtpu_buffer_manager::push_sdu(uint16_t rnti, uint32_t lcid, srslte::unique_byte_buffer_t sdu)
{
  pthread_rwlock_wrlock(&rwlock);
  if (!buffer_map.count(rnti)) {
    buffer_map[rnti] = user_buffer_state();
  }
  if (buffer_map.at(rnti).get_bearer_nof_packets() < BEARER_CAPACITY_PKT) {
    buffer_map.at(rnti).update_buffer_state_delta(lcid, 1, sdu->N_bytes);
    pdcp->write_sdu(rnti, lcid, std::move(sdu));
    return;
  }
  
  if (common_queue.size() < BUF_CAPACITY_PKT) {
    push_sdu_(rnti, lcid, std::move(sdu));
  } else {
    uint16_t max_user = get_user_to_drop();
    if (max_user != rnti){
      erase_oldest_and_move(max_user);
      push_sdu_(rnti, lcid, std::move(sdu));
    }
  }
  pthread_rwlock_unlock(&rwlock);
  // else if (common_queue.size() < BUF_CAPACITY_PKT)
  //   common_queue.push(std::move(sdu));
}

void gtpu_buffer_manager::push_sdu_(uint16_t rnti, uint32_t lcid, srslte::unique_byte_buffer_t sdu)
{
  if (!buffer_usage.count(rnti)) {
    buffer_usage[rnti] = 0;
  }
  buffer_map[rnti] += 1;

  pair<uint16_t, uint32_t> identity = {rnti, lcid};
  common_queue.push(std::make_pair(identity, std::move(sdu)));
  if (!first_pkt_iter.count(rnti)) {
    list<pending_pkt>::iterator tmp = common_queue.end();
    tmp--;
    first_pkt_iter[rnti] = tmp;
  }
}

bool gtpu_buffer_manager::check_space_new_sdu(uint16_t rnti)
{
  uint32_t total_packets = compute_nof_packets();
  if (total_packets >= BUF_CAPACITY_PKT) {
    int size = buffer_map.size();
    buf_log->info("[buf-debug] Drop packet user rnti=0x%x, num_of_users=%d, nof_packets=%u\n", rnti, size, total_packets);
    return false;
  }
  // if (buffer_map.size() > 0) {
  //   uint32_t max_rnti = buffer_map.begin()->first;
  //   for (user_buffer_state_map_t::iterator it = buffer_map.begin(); it != buffer_map.end(); ++it ){
  //     buf_log->info("[buf-debug] Iterate user rnti=%u, nof_packets=%u\n", it->first, it->second.get_user_nof_packets());
  //     if (it->second.get_user_nof_packets() > buffer_map.at(max_rnti).get_user_nof_packets())
  //       max_rnti = it->first;
  //   } 
  // }
	return true;
	// if (nof_packets >= BUF_CAPACITY_PKT) {
	//   if (gtpu_queue.top().first != rnti) {
 //        rlc->pop_unread_sdu_user(rnti);
 //        return true;
	//   }
	//   return false;
	// }
	// return true;
}

void gtpu_buffer_manager::erase_oldest_and_move(uint16_t rnti)
{
  if (!first_pkt_iter.count(rnti))
    return;

  if (buffer_usage.count(rnti))
    buffer_usage[rnti] -= 1;
  list<pending_pkt>::iterator tmp = common_queue.erase(first_pkt_iter.at(rnti));
  for (; tmp != common_queue.end(); tmp++) {
    if (tmp->first->first == rnti && tmp->first->second == lcid)
      break;
  }
  if (tmp == common_queue.end()) {
    first_pkt_iter.erase(rnti)
  }
}

// void gtpu_buffer_manager::update_priority_value(uint16_t rnti, uint32_t lcid)
// {

// }

// void gtpu_buffer_manager::add_bearer(uint16_t rnti, uint32_t lcid)
// {
  
// }

// void gtpu_buffer_manager::add_user(uint16_t rnti)
// {
	
// }
  
// void gtpu_buffer_manager::rem_bearer(uint16_t rnti, uint32_t lcid)
// {

// }

// void gtpu_buffer_manager::user_buffer_state::update_buffer_state(uint32_t lcid, uint32_t nof_unread_packets, uint32_t nof_unread_bytes)
void user_buffer_state::update_buffer_state(uint32_t lcid, uint32_t nof_unread_packets, uint32_t nof_unread_bytes)
{
  pthread_mutex_lock(&mutex);
  // user_nof_packets += nof_unread_packets;
  // user_nof_bytes += nof_unread_bytes;
  // if (user_buffer_map.count(lcid)) {
  // 	user_nof_packets -= user_buffer_map[lcid].first;
  // 	user_nof_bytes -= user_buffer_map[lcid].second;
  // }
  user_buffer_map[lcid] = std::make_pair(nof_unread_packets, nof_unread_bytes);
  pthread_mutex_unlock(&mutex);
}

void user_buffer_state::update_buffer_state_delta(uint32_t lcid, uint32_t delta_nof_packets, uint32_t delta_nof_bytes)
{
  pthread_mutex_lock(&mutex);
  if (!user_buffer_map.count(lcid)) {
    user_buffer_map[lcid] = std::make_pair(0, 0);
  }
  user_buffer_map[lcid].first += delta_nof_packets;
  user_buffer_map[lcid].second += delta_nof_bytes;
  pthread_mutex_unlock(&mutex);
}

// void gtpu_buffer_manager::user_buffer_state::set_buffer_state(uint32_t nof_unread_packets, uint32_t nof_unread_bytes)
void user_buffer_state::set_buffer_state(uint32_t nof_unread_packets, uint32_t nof_unread_bytes)
{
  pthread_mutex_lock(&mutex);
  user_nof_packets = nof_unread_packets;
  user_nof_bytes = nof_unread_bytes;
  pthread_mutex_unlock(&mutex);
}

uint32_t user_buffer_state::compute_nof_packets()
{
  pthread_mutex_lock(&mutex);
  uint32_t total = 0;
  for (auto it=user_buffer_map.begin(); it != user_buffer_map.end(); it++) {
    total += it->second.first;
  }
  pthread_mutex_unlock(&mutex);
  return total;
}

uint32_t user_buffer_state::get_bearer_nof_packets(uint32_t lcid)
{
  pthread_mutex_lock(&mutex);
  if (!user_buffer_map.count(lcid))
    return 0;
  return user_buffer_map.at(lcid).first;
}

} // namespace srsenb