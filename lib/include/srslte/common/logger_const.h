#ifndef SRSLTE_LOGGERCONST_H
#define SRSLTE_LOGGERCONST_H

#include <stdio.h>
#include <string>
#include <unordered_map>

namespace srslte {

typedef enum {
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
  // {"LTE_RLC_UL_Stats", LTE_RLC_UL_Stats},
  // {"LTE_RLC_DL_Stats", LTE_RLC_DL_Stats},
  // {"LTE_PDCP_UL_Data_PDU", LTE_PDCP_UL_Data_PDU},
  // {"LTE_PDCP_DL_Data_PDU", LTE_PDCP_DL_Data_PDU},
} );

} // namespace srslte

#endif // SRSLTE_LOGGERCONST_H