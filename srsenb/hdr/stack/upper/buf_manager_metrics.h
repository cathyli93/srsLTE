
#ifndef SRSENB_BUFM_METRICS_H
#define SRSENB_BUFM_METRICS_H

namespace srsenb {

// Buffer manager metrics per user

struct buf_manager_metrics_t {
  uint16_t rnti;
  int      rx_sdus;
  int      drop_sdus;
  int      tx_sdus;
  int      rx_brate;
  int      drop_brate;
  int      tx_brate;
  uint32_t buffer_sdus;
  uint32_t buffer_bytes;
};

} // namespace srsenb

#endif // SRSENB_BUFM_METRICS_H
