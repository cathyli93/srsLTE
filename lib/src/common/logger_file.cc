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

#define LOG_BUFFER_SIZE 1024 * 32

#include "srslte/common/logger_file.h"

using namespace std;

namespace srslte {

logger_file::logger_file() : logfile(NULL), is_running(false), cur_length(0), max_length(0), thread("LOGGER_FILE"), mi_logfile(NULL)
{
  pthread_mutex_init(&mutex, NULL);
  pthread_cond_init(&not_empty, NULL);
}

logger_file::~logger_file()
{
  stop();
  pthread_mutex_destroy(&mutex);
  pthread_cond_destroy(&not_empty);
}

void logger_file::init(std::string file, int max_length_, std::string mi_file, std::string mi_msg_types)
{
  if (is_running) {
    fprintf(stderr, "Error: logger thread is already running.\n");
    return;
  }
  pthread_mutex_lock(&mutex);
  max_length = (int64_t)max_length_ * 1024;
  name_idx   = 0;
  filename   = file;
  logfile    = fopen(filename.c_str(), "w");
  if (logfile == NULL) {
    printf("Error: could not create log file, no messages will be logged!\n");
  }

  // mi-log
  mi_filename = mi_file;
  if (mi_file != "") {
    mi_logfile = fopen(mi_filename.c_str(), "w");
    if (mi_logfile == NULL) {
      printf("Error: could not create MI log file, no messages will be logged!\n");
    }
  }

  if (mi_msg_types != "") {
    std::string to_split = mi_msg_types[mi_msg_types.length() - 1] != ',' ? mi_msg_types + "," : mi_msg_types;
    std::string delimiter = ",";
    size_t pos = 0;
    std::string token;
    while ((pos = to_split.find(delimiter)) != std::string::npos) {
      token = to_split.substr(0, pos);
      if (MsgTypeToName.find(token) == MsgTypeToName.end()) {
        printf("Error: unsupported MI message type: %s!\n", token.c_str());
        continue;
      }
      // MiMessageType msg_type = MsgTypeToName[token];
      supported_msg_types.insert(MsgTypeToName.find(token)->second);
      printf("Token=%s, id=0x%x\n", token, MsgTypeToName.find(token)->second);
      to_split.erase(0, pos + delimiter.length());
    }
    for (auto it=supported_msg_types.begin(); it!=supported_msg_types.end(); it++) {
      printf("Inserted type=0x%x\n", *it);
    }
  }

  is_running = true;
  start(-2);
  pthread_mutex_unlock(&mutex);
}

void logger_file::stop()
{
  if (is_running) {
    logger::log_char("Closing log\n");
    pthread_mutex_lock(&mutex);
    is_running = false;
    pthread_cond_signal(&not_empty); // wakeup thread and let it terminate
    pthread_mutex_unlock(&mutex);
    wait_thread_finish();
    pthread_mutex_lock(&mutex);
    flush();
    if (logfile) {
      fclose(logfile);
      logfile = NULL;
    }
    // mi-log
    if (mi_logfile) {
      fclose(mi_logfile);
      mi_logfile = NULL;
    } // mi-log end
    pthread_mutex_unlock(&mutex);
  } else {
    pthread_mutex_lock(&mutex);
    flush(); // flush even if thread isn't running anymore
    pthread_mutex_unlock(&mutex);
  }
}

void logger_file::log(unique_log_str_t msg)
{
  pthread_mutex_lock(&mutex);
  buffer.push_back(std::move(msg));
  pthread_cond_signal(&not_empty);
  pthread_mutex_unlock(&mutex);
}

void logger_file::log_mi(unique_log_str_t msg)
{
  pthread_mutex_lock(&mutex);
  mi_buffer.push_back(std::move(msg));
  pthread_cond_signal(&not_empty);
  pthread_mutex_unlock(&mutex);
}

void logger_file::run_thread()
{
  while (is_running) {
    pthread_mutex_lock(&mutex);
    while (buffer.empty() && mi_buffer.empty()) {
      pthread_cond_wait(&not_empty, &mutex);
      if (!is_running) {
        pthread_mutex_unlock(&mutex);
        return; // Thread done. Messages in buffer will be handled in flush.
      }
    }

    if (!buffer.empty()) {
      unique_log_str_t s = std::move(buffer.front());

      int n = 0;
      if (logfile) {
        n = fprintf(logfile, "%s", s->str());
      }
      buffer.pop_front();

      if (n > 0) {
        cur_length += (int64_t)n;
        if (cur_length >= max_length && max_length > 0) {
          fclose(logfile);
          name_idx++;
          char numstr[21]; // enough to hold all numbers up to 64-bits
          sprintf(numstr, ".%d", name_idx);
          string newfilename = filename + numstr;
          logfile            = fopen(newfilename.c_str(), "w");
          if (logfile == NULL) {
            printf("Error: could not create log file, no messages will be logged!\n");
          }
          cur_length = 0;
        }
      }
    }

    if (!mi_buffer.empty()) {
      unique_log_str_t s = std::move(mi_buffer.front());

      int n = 0;
      if (mi_logfile) {
        n = fprintf(mi_logfile, "%s", s->str());
      }
      mi_buffer.pop_front();
    }
    pthread_mutex_unlock(&mutex);
  }
}

void logger_file::flush()
{
  std::deque<unique_log_str_t>::iterator it;
  for (it = buffer.begin(); it != buffer.end(); it++) {
    unique_log_str_t s = std::move(*it);
    if (logfile) {
      fprintf(logfile, "%s", s->str());
    }
  }
  buffer.clear();

  for (it = mi_buffer.begin(); it != mi_buffer.end(); it++) {
    unique_log_str_t s = std::move(*it);
    if (mi_logfile) {
      fprintf(mi_logfile, "%s", s->str());
    }
  }
  mi_buffer.clear();
}

bool logger_file::is_supported_type(MiMessageType t)
{
  return mi_filename != "" && supported_msg_types.find(t) != supported_msg_types.end();
}

} // namespace srslte
