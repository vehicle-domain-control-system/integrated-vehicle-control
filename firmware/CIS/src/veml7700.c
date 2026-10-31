#include "veml7700.h"
#include "regs_s32k144.h"
#include "timebase.h"

#define VEML_ADDR           0x10u
#define REG_ALS_CONF        0x00u
#define REG_ALS             0x04u
/* ALS_CONF: gain 1/8 (bits 12:11 = 10), integration time 100 ms (bits 9:6 = 0000),
 * persistence 1, interrupt off, power on (bit 0 = 0) */
#define ALS_CONF_VALUE      0x1000u

#define SDA_PIN             2u
#define SCL_PIN             3u
#define I2C_TIMEOUT_US      5000u

/* 100 kHz from the 8 MHz functional clock (prescaler 1):
 * low 44 + high 36 + synchronisation ~ 83 cycles = 10.4 us per SCL period */
#define MCCR0_100KHZ        ((0x10u << 24) | (0x28u << 16) | (0x23u << 8) | 0x2Bu)

typedef enum { I2C_OK = 0, I2C_NACK, I2C_TIMEOUT, I2C_BUS_ERROR } i2c_result_t;

/* Diagnostics for the debugger */
volatile uint32_t g_veml_ok;
volatile uint32_t g_veml_nack;
volatile uint32_t g_veml_timeout;
volatile uint32_t g_veml_bus_error;
volatile uint32_t g_veml_raw;
volatile uint32_t g_veml_conf;          /* configuration read back from the sensor */
volatile uint32_t g_veml_present;       /* 1 = the sensor answered at init */
volatile uint32_t g_i2c_sda_idle;       /* line levels before the bus is started: both 1 */
volatile uint32_t g_i2c_scl_idle;       /* when the module is wired (its pull-ups pull them up) */

static uint32_t s_last_read_ms;
static bool     s_need_config = true;

static bool timed_out(uint16_t t0, uint16_t limit_us)
{
    return (uint16_t)(timebase_cnt() - t0) > limit_us;
}

static void delay_us(uint16_t us)
{
    uint16_t t0 = timebase_cnt();
    while ((uint16_t)(timebase_cnt() - t0) < us) { }
}

static uint32_t line_levels(void)
{
    return PTA_PDIR;
}

/* If a slave holds SDA low (interrupted transfer), clock SCL up to 9 times and
 * finish with a STOP so the bus is free again. The lines are open-drain: a
 * line is driven low by making the pin an output (low) and released by
 * making it an input. */
static void bus_clear(void)
{
    uint32_t sda = 1u << SDA_PIN;
    uint32_t scl = 1u << SCL_PIN;
    int i;

    PORTA_PCR(SDA_PIN) = PCR_MUX(1) | PCR_PE | PCR_PS_PULLUP;
    PORTA_PCR(SCL_PIN) = PCR_MUX(1) | PCR_PE | PCR_PS_PULLUP;
    PTA_PDDR &= ~(sda | scl);
    delay_us(20);

    g_i2c_sda_idle = (line_levels() & sda) ? 1u : 0u;
    g_i2c_scl_idle = (line_levels() & scl) ? 1u : 0u;

    for (i = 0; i < 9 && !(line_levels() & sda); i++) {
        PTA_PCOR = scl;  PTA_PDDR |= scl;  delay_us(5);     /* SCL low */
        PTA_PDDR &= ~scl;                  delay_us(5);     /* SCL released (high) */
    }
    /* STOP: SDA low -> SCL high -> SDA released */
    PTA_PCOR = sda;  PTA_PDDR |= sda;  delay_us(5);
    PTA_PDDR &= ~scl;                  delay_us(5);
    PTA_PDDR &= ~sda;                  delay_us(5);
}

static void lpi2c_setup(void)
{
    PCC_PORTA |= PCC_CGC;
    PORTA_PCR(SDA_PIN) = PCR_MUX(3);                 /* LPI2C0_SDA */
    PORTA_PCR(SCL_PIN) = PCR_MUX(3);                 /* LPI2C0_SCL */

    PCC_LPI2C0 &= ~PCC_CGC;
    PCC_LPI2C0 = PCC_PCS(1) | PCC_CGC;               /* functional clock: SOSCDIV2 (8 MHz) */

    LPI2C0_MCR = LPI2C_MCR_RST;                      /* software reset */
    LPI2C0_MCR = 0u;
    LPI2C0_MCFGR1 = 0u;                              /* prescaler 1 */
    LPI2C0_MCCR0 = MCCR0_100KHZ;
    LPI2C0_MFCR = 0u;
    LPI2C0_MCR = LPI2C_MCR_MEN;
}

static void prepare(void)
{
    LPI2C0_MCR |= LPI2C_MCR_RTF | LPI2C_MCR_RRF;     /* empty both FIFOs */
    LPI2C0_MSR = LPI2C_MSR_CLEARABLE;                /* clear old flags (write 1) */
}

static bool put(uint32_t word)
{
    uint16_t t0 = timebase_cnt();
    while (!(LPI2C0_MSR & LPI2C_MSR_TDF)) {
        if (timed_out(t0, I2C_TIMEOUT_US)) {
            return false;
        }
    }
    LPI2C0_MTDR = word;
    return true;
}

/* Waits for the STOP and evaluates the flags. */
static i2c_result_t finish(void)
{
    uint16_t t0 = timebase_cnt();
    uint32_t flags;
    i2c_result_t r = I2C_OK;

    while (!(LPI2C0_MSR & LPI2C_MSR_SDF)) {
        if (timed_out(t0, I2C_TIMEOUT_US)) {
            r = I2C_TIMEOUT;
            break;
        }
    }
    flags = LPI2C0_MSR;
    LPI2C0_MSR = flags & LPI2C_MSR_CLEARABLE;
    if (r == I2C_OK) {
        if (flags & LPI2C_MSR_NDF) {
            r = I2C_NACK;
        } else if (flags & (LPI2C_MSR_ALF | LPI2C_MSR_FEF | LPI2C_MSR_PLTF)) {
            r = I2C_BUS_ERROR;
        }
    }
    return r;
}

static i2c_result_t write16(uint8_t reg, uint16_t value)
{
    prepare();
    if (!put(LPI2C_CMD_START | (VEML_ADDR << 1)) ||            /* START + address, write */
        !put(LPI2C_CMD_TXD | reg) ||                           /* register (command code) */
        !put(LPI2C_CMD_TXD | (value & 0xFFu)) ||               /* low byte first */
        !put(LPI2C_CMD_TXD | (value >> 8)) ||
        !put(LPI2C_CMD_STOP)) {
        return I2C_TIMEOUT;
    }
    return finish();
}

static i2c_result_t read16(uint8_t reg, uint16_t *value)
{
    uint32_t d0;
    uint32_t d1;
    i2c_result_t r;

    prepare();
    if (!put(LPI2C_CMD_START | (VEML_ADDR << 1)) ||            /* START + address, write */
        !put(LPI2C_CMD_TXD | reg) ||                           /* register to read */
        !put(LPI2C_CMD_START | (VEML_ADDR << 1) | 1u) ||       /* repeated START, read */
        !put(LPI2C_CMD_RXD | 1u) ||                            /* receive 2 bytes */
        !put(LPI2C_CMD_STOP)) {
        return I2C_TIMEOUT;
    }
    r = finish();
    if (r != I2C_OK) {
        return r;
    }
    d0 = LPI2C0_MRDR;
    d1 = LPI2C0_MRDR;
    if ((d0 & LPI2C_MRDR_RXEMPTY) || (d1 & LPI2C_MRDR_RXEMPTY)) {
        return I2C_BUS_ERROR;
    }
    *value = (uint16_t)(((d1 & 0xFFu) << 8) | (d0 & 0xFFu));   /* low byte first */
    return I2C_OK;
}

static bool configure(void)
{
    uint16_t conf = 0;

    if (write16(REG_ALS_CONF, ALS_CONF_VALUE) != I2C_OK) {
        return false;
    }
    delay_us(3000);                                            /* sensor needs >= 2.5 ms */
    if (read16(REG_ALS_CONF, &conf) != I2C_OK) {
        return false;
    }
    g_veml_conf = conf;
    return conf == ALS_CONF_VALUE;
}

bool veml7700_init(void)
{
    bus_clear();
    lpi2c_setup();
    s_last_read_ms = timebase_now_ms();                        /* first reading after one period */
    s_need_config = !configure();
    g_veml_present = s_need_config ? 0u : 1u;
    return !s_need_config;
}

bool veml7700_step(lux_state_t *s, uint32_t now_ms, bool allowed)
{
    uint16_t raw = 0;
    i2c_result_t r;

    if (!allowed || (uint32_t)(now_ms - s_last_read_ms) < VEML7700_PERIOD_MS) {
        return false;
    }
    s_last_read_ms = now_ms;

    if (s_need_config) {
        if (!configure()) {
            g_veml_timeout++;
            lux_on_failure(s, now_ms);
            return true;
        }
        s_need_config = false;
        g_veml_present = 1u;
        return true;                       /* first reading in the next period */
    }

    r = read16(REG_ALS, &raw);
    if (r == I2C_OK) {
        g_veml_ok++;
        g_veml_raw = raw;
        lux_on_raw(s, raw, now_ms);
    } else {
        if (r == I2C_NACK) {
            g_veml_nack++;
        } else if (r == I2C_TIMEOUT) {
            g_veml_timeout++;
        } else {
            g_veml_bus_error++;
        }
        lux_on_failure(s, now_ms);
        /* Restart the peripheral and free the bus; the sensor is configured again. */
        bus_clear();
        lpi2c_setup();
        s_need_config = true;
    }
    return true;
}
