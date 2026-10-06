/*
 * S32K144 register access by address.
 *
 * Used instead of a device header so the code does not depend on the register
 * names of a particular header (RTD uses IP_WDOG, IP_PORTD, ...; the Cookbook
 * header uses WDOG, PORTD, ...).
 *
 * Offsets are from the S32K144 Reference Manual. Base addresses are from the
 * S32K144 memory map; compare with IP_xxx_BASE in the project's S32K144.h
 * if a value ever looks wrong.
 */
#ifndef REGS_S32K144_H
#define REGS_S32K144_H

#include <stdint.h>

#define REG32(addr)         (*(volatile uint32_t *)(addr))

/* WDOG */
#define WDOG_BASE           0x40052000u
#define WDOG_CS             REG32(WDOG_BASE + 0x0u)
#define WDOG_CNT            REG32(WDOG_BASE + 0x4u)
#define WDOG_TOVAL          REG32(WDOG_BASE + 0x8u)

/* SCG: system oscillator (8 MHz crystal on the EVB) */
#define SCG_BASE            0x40064000u
#define SCG_SOSCCSR         REG32(SCG_BASE + 0x100u)
#define SCG_SOSCDIV         REG32(SCG_BASE + 0x104u)
#define SCG_SOSCCFG         REG32(SCG_BASE + 0x108u)
#define SCG_SOSCCSR_SOSCEN  (1u << 0)
#define SCG_SOSCCSR_LK      (1u << 23)
#define SCG_SOSCCSR_SOSCVLD (1u << 24)

/* PCC: peripheral clock control (offset from the Reference Manual) */
#define PCC_BASE            0x40065000u
#define PCC_FTM0            REG32(PCC_BASE + 0x0E0u)
#define PCC_PORTA           REG32(PCC_BASE + 0x124u)
#define PCC_PORTD           REG32(PCC_BASE + 0x130u)
#define PCC_PORTE           REG32(PCC_BASE + 0x134u)
#define PCC_CGC             (1u << 30)           /* clock gate control */
#define PCC_PCS(n)          ((uint32_t)(n) << 24) /* peripheral clock source */

/* PORT */
#define PORTA_BASE          0x40049000u
#define PORTD_BASE          0x4004C000u
#define PORTE_BASE          0x4004D000u
#define PORTA_PCR(n)        REG32(PORTA_BASE + 4u * (n))
#define PORTD_PCR(n)        REG32(PORTD_BASE + 4u * (n))
#define PORTE_PCR(n)        REG32(PORTE_BASE + 4u * (n))
#define PCR_MUX(n)          ((uint32_t)(n) << 8)
#define PCR_PS_PULLUP       (1u << 0)            /* pull select: pull-up */
#define PCR_PE              (1u << 1)            /* pull enable */

/* GPIO */
#define PTA_BASE            0x400FF000u
#define PTD_BASE            0x400FF0C0u
#define PTE_BASE            0x400FF100u
#define PTA_PSOR            REG32(PTA_BASE + 0x04u)
#define PTA_PCOR            REG32(PTA_BASE + 0x08u)
#define PTA_PDIR            REG32(PTA_BASE + 0x10u)
#define PTA_PDDR            REG32(PTA_BASE + 0x14u)
#define PTD_PSOR            REG32(PTD_BASE + 0x04u)
#define PTD_PCOR            REG32(PTD_BASE + 0x08u)
#define PTD_PDDR            REG32(PTD_BASE + 0x14u)
#define PTE_PDIR            REG32(PTE_BASE + 0x10u)
#define PTE_PSOR            REG32(PTE_BASE + 0x04u)
#define PTE_PCOR            REG32(PTE_BASE + 0x08u)
#define PTE_PDDR            REG32(PTE_BASE + 0x14u)

/* FTM0 */
#define FTM0_BASE           0x40038000u
#define FTM0_SC             REG32(FTM0_BASE + 0x00u)
#define FTM0_CNT            REG32(FTM0_BASE + 0x04u)
#define FTM0_MOD            REG32(FTM0_BASE + 0x08u)
#define FTM0_C6SC           REG32(FTM0_BASE + 0x3Cu)   /* CnSC = 0x0C + 8n */
#define FTM0_C6V            REG32(FTM0_BASE + 0x40u)   /* CnV  = 0x10 + 8n */
#define FTM0_MODE           REG32(FTM0_BASE + 0x54u)
#define FTM_SC_TOF          (1u << 9)
#define FTM_MODE_WPDIS      (1u << 2)
#define FTM_CHSC_CHF        (1u << 7)

#endif /* REGS_S32K144_H */
