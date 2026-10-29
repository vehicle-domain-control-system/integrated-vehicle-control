#ifndef BCM_PORT_BENCH144_H
#define BCM_PORT_BENCH144_H

/* BcmPort_* (bcm/bcm_port.h) for the S32K144 BCM bench: bcm_port_rtd.c without the
 * SysTick, the watchdog and the boot counter (the bench clock net_board_time144.c owns
 * SysTick and BOOT_ID). Call after Ftm_Pwm_Ip_Init(), before Net_Init(). */
void BcmPortBench_Init(void);

#endif
