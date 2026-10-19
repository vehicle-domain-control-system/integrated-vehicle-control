#ifndef HAL_S32K344_CFG_H
#define HAL_S32K344_CFG_H

/* The only file that names configuration-tool generated symbols. The HAL
 * sources use the HAL_CFG_* aliases below, so a regenerated configuration
 * with different names needs changes here and nowhere else. */

#include "Clock_Ip.h"
#include "Siul2_Port_Ip.h"
#include "Siul2_Dio_Ip.h"
#include "IntCtrl_Ip.h"
#include "Lpi2c_Ip.h"
#include "Pit_Ip.h"
#include "CDD_Rm.h"
#include "Dma_Ip.h"
#include "Sai_Ip.h"

/* Clock tree */
#define HAL_CFG_CLOCK_CONFIG        (&Clock_Ip_aClockConfig[0])

/* Pin multiplexing */
#define HAL_CFG_PIN_COUNT           NUM_OF_CONFIGURED_PINS_PortContainer_0_VS_0
#define HAL_CFG_PIN_CONFIG          g_pin_mux_InitConfigArr_PortContainer_0_VS_0

/* Interrupt table (PIT0, DMA channel 0, SAI0) */
#define HAL_CFG_INTCTRL_CONFIG      (&intCtrlConfig)

/* I2C master: LPI2C0, fast mode */
#define HAL_CFG_LPI2C_INSTANCE      0U
#define HAL_CFG_LPI2C_MASTER_CONFIG (&I2c_Lpi2cMaster_HwChannel0_Channel0_VS_0)

/* Tick timer: PIT0 channel 0. The callback bound in the generated channel
 * configuration toggles the codec reference pin and counts ticks. */
#define HAL_CFG_PIT_INSTANCE        0U
#define HAL_CFG_PIT_CHANNEL         0U
#define HAL_CFG_PIT_INIT_CONFIG     (&PIT_0_InitConfig_PB_VS_0)
#define HAL_CFG_PIT_CHANNEL_CONFIG  PIT_0_CH_0
#define HAL_CFG_PIT_PERIOD_COUNT    50000U  /* 800 Hz tick */

/* Codec reference pin (PTA26) and user switch SW2 (PTC20, active high). The
 * pin numbers are bit positions inside the upper half-port (pins 16..31). */
#define HAL_CFG_REF_PORT            CS2100_REF_PORT
#define HAL_CFG_REF_PIN             CS2100_REF_PIN
#define HAL_CFG_SW2_PORT            PTC_H_HALF
#define HAL_CFG_SW2_PIN             4U

/* Resource manager, DMA and SAI. SAI0 TX is routed to DMA logic channel 0. */
#define HAL_CFG_RM_CONFIG           (&Rm_Config_VS_0)
#define HAL_CFG_DMA_INIT_CONFIG     (&Dma_Ip_Sa_xDmaInitPB_VS_0)
#define HAL_CFG_DMA_LOGIC_CH        SA_DMA_LOGIC_CH_0
#define HAL_CFG_SAI_INSTANCE        SAICONFIGURATION_1_VS_0_INSTANCE
#define HAL_CFG_SAI_CONFIG          (&Sai_Ip_Sa_xInitConfig_0_VS_0)

/* Callbacks the generated configuration binds by name. They are defined in
 * hal_audio_tx_s32k344.c (first three) and hal_timer_s32k344.c (last). */
void AudioPlayer_DmaTxIrqCallback(void);
void AudioPlayer_SaiTxCallback(Sai_Ip_ReportType event);
void AudioPlayer_Sai0IrqHandler(void);
void Cs2100Ref_Callback(uint8 channel);

#endif /* HAL_S32K344_CFG_H */
