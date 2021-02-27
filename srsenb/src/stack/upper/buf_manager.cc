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

void gtpu_buffer_manager::init() {
// int gtpu_buffer_manager::init(srsenb::rlc_interface_bufmng* rlc_) {
  // rlc = rlc_;
  buf_log->set_level(srslte::LOG_LEVEL_INFO);
}

void gtpu_buffer_manager::stop() {
  buffer_map.clear();
}

void gtpu_buffer_manager::rem_user(uint16_t rnti)
{
  buf_log->info("[buf-debug] Remove user rnti=%u\n", rnti);

  if (buffer_map.count(rnti)) {
  	nof_packets -= buffer_map[rnti].get_user_nof_packets();
  	nof_bytes -= buffer_map[rnti].get_user_nof_bytes();

	// buffer_map[rnti].set_buffer_state(BUF_CAPACITY_PKT * 3, BUF_CAPACITY_PKT * 5000);
	// if (gtpu_queue.top().first == rnti) 
	//   gtpu_queue.pop();
	  buffer_map.erase(rnti);
  }
}

void gtpu_buffer_manager::update_buffer_state(uint16_t rnti, uint32_t lcid, uint32_t nof_unread_packets, uint32_t nof_unread_bytes)
{
  if (buffer_map.count(rnti)) {
  	nof_packets -= buffer_map[rnti].get_user_nof_packets();
  	nof_bytes -= buffer_map[rnti].get_user_nof_bytes();
  }
  else {
  	buffer_map[rnti] = user_buffer_state();
  	// gtpu_queue.push(std::make_pair(rnti, &buffer_map[rnti]))
  }
  buffer_map[rnti].update_buffer_state(lcid, nof_unread_packets, nof_unread_bytes);
  nof_packets += buffer_map[rnti].get_user_nof_packets();
  nof_bytes += buffer_map[rnti].get_user_nof_bytes();

  buf_log->info("[buf-debug] Update nof_unread_packets=%u, nof_unread_bytes=%u, new nof_packets=%u, new nof_bytes=%u\n", nof_unread_packets, nof_unread_bytes, nof_packets, nof_bytes);
}

bool gtpu_buffer_manager::check_space_new_sdu(uint16_t rnti)
{
  int size = buffer_map.size();
  buf_log->info("[buf-debug] user rnti=%u, size_of_map=%u\n", rnti, size);
  if (buffer_map.size() > 0) {
    uint32_t max_rnti = buffer_map.begin()->first;
    for (user_buffer_state_map_t::iterator it = buffer_map.begin(); it != buffer_map.end(); ++it ){
      buf_log->info("[buf-debug] Iterate user rnti=%u, nof_packets=%u\n", it->first, it->second.get_user_nof_packets());
      if (it->second.get_user_nof_packets() > buffer_map.at(max_rnti).get_user_nof_packets())
        max_rnti = it->first;
    } 
  }
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
  user_nof_packets += nof_unread_packets;
  user_nof_bytes += nof_unread_bytes;
  if (user_buffer_map.count(lcid)) {
  	user_nof_packets -= user_buffer_map[lcid].first;
  	user_nof_bytes -= user_buffer_map[lcid].second;
  }
  user_buffer_map[lcid] = std::make_pair(nof_unread_packets, nof_unread_bytes);
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

} // namespace srsenb