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
 */

/******************************************************************************
 * File:        logger.h
 * Description: Interface for logging output
 *****************************************************************************/

#ifndef SRSLTE_LOGGER_H
#define SRSLTE_LOGGER_H

#include "buffer_pool.h"
#include <memory>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace srslte {

enum {
  LTE_PHY_PUSCH_Decoding_Result = 0xB139,
  LTE_PHY_PUCCH_Decoding_Result = 0xB13C, 
  LTE_MAC_DL_Transport_Block    = 0xB063,

  LTE_MAC_DL_Stats      = 0xF001,
  LTE_MAC_UL_Stats      = 0xF002,
  LTE_GTPU_Buffer_Stats = 0xF003,
} MiMessageType; 

  // LTE_RLC_UL_Stats      = 0xB097;
  // LTE_RLC_DL_Stats      = 0xB087;
  // LTE_PDCP_UL_Data_PDU  = 0xB0B3;
  // LTE_PDCP_DL_Data_PDU  = 0xB0A3;

const std::unordered_map<std::string, MiMessageType> MsgTypeToName ( {
  {"LTE_PHY_PUSCH_Decoding_Result", LTE_PHY_PUSCH_Decoding_Result},
  {"LTE_PHY_PUCCH_Decoding_Result", LTE_PHY_PUCCH_Decoding_Result},
  {"LTE_MAC_DL_Transport_Block", LTE_MAC_DL_Transport_Block},
  {"LTE_MAC_DL_Stats", LTE_MAC_DL_Stats},
  {"LTE_MAC_UL_Stats", LTE_MAC_UL_Stats},
  {"LTE_GTPU_Buffer_Stats", LTE_GTPU_Buffer_Stats},
} );

class logger
{
public:
  const static uint32_t preallocated_log_str_size = 1024;
  std::unordered_set<MiMessageType> supported_msg_types; //mi-log

  logger() : pool(16 * 1024) {}
  virtual ~logger() = default;

  class log_str
  {
  public:
    log_str(const char* msg_ = nullptr, uint32_t size_ = 0)
    {
      size = size_ ? size_ : preallocated_log_str_size;
      msg  = new char[size];
      if (msg_) {
        strncpy(msg, msg_, size);
      } else {
        msg[0] = '\0';
      }
    }
    log_str(const log_str&) = delete;
    log_str& operator=(const log_str&) = delete;
    ~log_str() { delete[] msg; }
    void     reset() { msg[0] = '\0'; }
    char*    str() { return msg; }
    uint32_t get_buffer_size() { return size; }
#ifdef SRSLTE_BUFFER_POOL_LOG_ENABLED
    char debug_name[SRSLTE_BUFFER_POOL_LOG_NAME_LEN] = {};
#endif

  private:
    uint32_t size;
    char*    msg;
  };

  typedef buffer_pool<log_str> log_str_pool_t;

  class log_str_deleter
  {
  public:
    explicit log_str_deleter(log_str_pool_t* pool_ = nullptr) : pool(pool_) {}
    void operator()(log_str* buf)
    {
      if (buf) {
        if (pool) {
          buf->reset();
          pool->deallocate(buf);
        } else {
          delete buf;
        }
      }
    }

  private:
    log_str_pool_t* pool;
  };
  typedef std::unique_ptr<log_str, log_str_deleter> unique_log_str_t;

  void log_char(const char* msg) { log(unique_log_str_t(new log_str(msg), log_str_deleter())); }

  virtual void log(unique_log_str_t msg) = 0;

  virtual void log_mi(unique_log_str_t msg) = 0; //mi-log

  log_str_pool_t&  get_pool() { return pool; }
  unique_log_str_t allocate_unique_log_str()
  {
    return unique_log_str_t(pool.allocate(), logger::log_str_deleter(&pool));
  }

  bool is_supported_type(MiMessageType t) { return supported_msg_types.find(MiMessageType) != supported_msg_types.end(); }

private:
  log_str_pool_t pool;
};

} // namespace srslte

#endif // SRSLTE_LOGGER_H
