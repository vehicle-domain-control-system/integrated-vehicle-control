/* Clock synthesizer and codec control against the HAL stand-in. The expected
 * I2C transfers (address, bytes, STOP) and waits are those of the verified
 * board bring-up, in the same order. Read-modify-write steps are checked with
 * read values that make every mask visible. */

#include "test_util.h"
#include "hal_mock.h"

#include "../../src/driver/audio_control/audio_control.h"
#include "../../src/infra/vss_config.h"

#define CS 0x4Fu
#define CODEC 0x0Au

static uint32_t s_pos;

static void ExpectBegin(void)
{
    s_pos = 0u;
}

static void ExpectWrite(uint8_t addr, uint8_t len, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3,
                        bool stop, int line)
{
    const HalMockLog *log = HalMock_Log();
    const HalMockOp *op;
    uint8_t want[4];
    uint8_t i;
    bool ok;

    want[0] = b0; want[1] = b1; want[2] = b2; want[3] = b3;
    if (s_pos >= log->count)
    {
        ++g_testFailed;
        printf("FAIL line %d: transfer %u missing (expected write)\n", line, (unsigned)s_pos);
        return;
    }
    op = &log->ops[s_pos++];
    ok = (op->kind == HAL_MOCK_I2C_WRITE) && (op->addr == addr) && (op->len == len) &&
         (op->sendStop == stop);
    for (i = 0u; ok && i < len; ++i)
    {
        ok = (op->bytes[i] == want[i]);
    }
    if (ok)
    {
        ++g_testPassed;
    }
    else
    {
        ++g_testFailed;
        printf("FAIL line %d: op %u kind=%d addr=0x%02X len=%u stop=%d bytes=%02X %02X %02X %02X\n",
               line, (unsigned)(s_pos - 1u), (int)op->kind, op->addr, op->len, (int)op->sendStop,
               op->bytes[0], op->bytes[1], op->bytes[2], op->bytes[3]);
    }
}

static void ExpectRead(uint8_t addr, uint8_t len, bool stop, int line)
{
    const HalMockLog *log = HalMock_Log();
    const HalMockOp *op;

    if (s_pos >= log->count)
    {
        ++g_testFailed;
        printf("FAIL line %d: transfer %u missing (expected read)\n", line, (unsigned)s_pos);
        return;
    }
    op = &log->ops[s_pos++];
    if ((op->kind == HAL_MOCK_I2C_READ) && (op->addr == addr) && (op->len == len) &&
        (op->sendStop == stop))
    {
        ++g_testPassed;
    }
    else
    {
        ++g_testFailed;
        printf("FAIL line %d: op %u kind=%d addr=0x%02X len=%u stop=%d (expected read)\n",
               line, (unsigned)(s_pos - 1u), (int)op->kind, op->addr, op->len, (int)op->sendStop);
    }
}

static void ExpectWait(uint32_t ticks, int line)
{
    const HalMockLog *log = HalMock_Log();
    const HalMockOp *op;

    if (s_pos >= log->count)
    {
        ++g_testFailed;
        printf("FAIL line %d: op %u missing (expected wait)\n", line, (unsigned)s_pos);
        return;
    }
    op = &log->ops[s_pos++];
    if ((op->kind == HAL_MOCK_WAIT) && (op->ticks == ticks))
    {
        ++g_testPassed;
    }
    else
    {
        ++g_testFailed;
        printf("FAIL line %d: op %u kind=%d ticks=%u (expected wait %u)\n",
               line, (unsigned)(s_pos - 1u), (int)op->kind, (unsigned)op->ticks, (unsigned)ticks);
    }
}

static void ExpectEnd(int line)
{
    const HalMockLog *log = HalMock_Log();

    if ((log->count == s_pos) && !log->overflow)
    {
        ++g_testPassed;
    }
    else
    {
        ++g_testFailed;
        printf("FAIL line %d: %u ops recorded, %u expected\n", line, (unsigned)log->count,
               (unsigned)s_pos);
    }
}

/* Clock synthesizer writes and codec writes in one-line form. */
#define CS_W2(r, v)        ExpectWrite(CS, 2u, (r), (v), 0u, 0u, true, __LINE__)
#define CS_SELECT(r)       ExpectWrite(CS, 1u, (r), 0u, 0u, 0u, false, __LINE__)
#define CS_R1()            ExpectRead(CS, 1u, true, __LINE__)
#define CODEC_W(r, v)      ExpectWrite(CODEC, 4u, (uint8_t)((r) >> 8), (uint8_t)(r), \
                                       (uint8_t)((v) >> 8), (uint8_t)(v), true, __LINE__)
#define CODEC_SELECT(r)    ExpectWrite(CODEC, 2u, (uint8_t)((r) >> 8), (uint8_t)(r), 0u, 0u, true, __LINE__)
#define CODEC_R2()         ExpectRead(CODEC, 2u, true, __LINE__)
#define WAIT(t)            ExpectWait((t), __LINE__)

static void Queue1(uint8_t a)
{
    HalMock_QueueRead(&a, 1u);
}

static void Queue2(uint16_t v)
{
    uint8_t b[2];

    b[0] = (uint8_t)(v >> 8);
    b[1] = (uint8_t)v;
    HalMock_QueueRead(b, 2u);
}

static void ExpectCs2100Configuration(void)
{
    CS_W2(0x16u, 0x10u);   /* FUNC_CFG1 */
    CS_W2(0x17u, 0x00u);   /* FUNC_CFG2 */
    CS_W2(0x06u, 0x07u);   /* RATIO_0 */
    CS_W2(0x07u, 0x80u);   /* RATIO_1 */
    CS_W2(0x08u, 0x00u);   /* RATIO_2 */
    CS_W2(0x09u, 0x00u);   /* RATIO_3 */
    CS_W2(0x03u, 0x01u);   /* DEVICE_CFG1 */
    CS_W2(0x05u, 0x01u);   /* GLOBAL_CFG */
}

static void ExpectCs2100Full(void)
{
    CS_SELECT(0x01u); CS_R1();      /* DEVICE_ID */
    ExpectCs2100Configuration();
    WAIT(500u);
    CS_SELECT(0x02u); CS_R1();      /* DEVICE_CTRL, value discarded */
    WAIT(40u);
    CS_SELECT(0x02u); CS_R1();      /* DEVICE_CTRL, lock check */
}

/* Read data for a full successful bring-up, in transfer order. */
static void QueueLockedBringUp(void)
{
    Queue1(0x6Cu);          /* DEVICE_ID */
    Queue1(0x80u);          /* DEVICE_CTRL first read (ignored) */
    Queue1(0x00u);          /* DEVICE_CTRL: unlock bit clear */
    Queue2(0xA011u);        /* CHIP_ID: part 0xA0 */
    Queue2(0x0040u);        /* DIG_POWER */
    Queue2(0x7060u);        /* ANA_POWER */
    Queue2(0x0200u);        /* ADCDAC_CTRL */
    Queue2(0x0145u);        /* ANA_CTRL */
}

static void ExpectCodecProbeAndConfigure(void)
{
    CODEC_SELECT(0x0000u); CODEC_R2();          /* CHIP_ID */
    CODEC_W(0x0004u, 0x0008u);                  /* CLK_CTRL */
    CODEC_W(0x0006u, 0x0130u);                  /* I2S_CTRL */
    CODEC_W(0x000Au, 0x0010u);                  /* SSS_CTRL */
    CODEC_SELECT(0x0002u); CODEC_R2();
    CODEC_W(0x0002u, 0x0061u);                  /* DIG_POWER |= 0x0021 */
    CODEC_SELECT(0x0030u); CODEC_R2();
    CODEC_W(0x0030u, 0x70F8u);                  /* ANA_POWER |= 0x40B8 */
    WAIT(320u);
    CODEC_W(0x0010u, 0x3C3Cu);                  /* DAC_VOL */
    CODEC_W(0x0022u, 0x5050u);                  /* ANA_HP_CTRL */
    CODEC_SELECT(0x000Eu); CODEC_R2();
    CODEC_W(0x000Eu, 0x020Cu);                  /* ADCDAC_CTRL: DAC muted */
    CODEC_SELECT(0x0024u); CODEC_R2();
    CODEC_W(0x0024u, 0x0135u);                  /* ANA_CTRL: (v & ~0x0070) | 0x0030 */
}

static uint32_t I2cErrors(void)
{
    AudioControlStatus st;

    AudioControl_Read(&st);
    return st.i2cErrors;
}

static void test_bring_up_sequence(void)
{
    AudioControlStatus st;

    HalMock_Reset();
    QueueLockedBringUp();
    CHECK(AudioControl_Init());

    ExpectBegin();
    ExpectCs2100Full();
    ExpectCodecProbeAndConfigure();
    ExpectEnd(__LINE__);

    AudioControl_Read(&st);
    CHECK(st.clockLocked);
    CHECK(st.codecConfigured);
    CHECK(!st.unmuted);
    CHECK(!st.muteLatched);
    CHECK_EQ_U(st.i2cErrors, 0u);
}

static void test_unlocked_clock_leaves_codec_untouched(void)
{
    AudioControlStatus st;

    HalMock_Reset();
    Queue1(0x6Cu);
    Queue1(0x80u);
    Queue1(0x80u);          /* still unlocked */
    CHECK(!AudioControl_Init());

    ExpectBegin();
    ExpectCs2100Full();
    ExpectEnd(__LINE__);

    AudioControl_Read(&st);
    CHECK(!st.clockLocked);
    CHECK(!st.codecConfigured);

    /* Nothing to unmute. */
    HalMock_ClearLog();
    CHECK(!AudioControl_Unmute(50u));
    CHECK_EQ_U(HalMock_Log()->count, 0u);
}

static void test_id_read_failure_does_not_stop_configuration(void)
{
    uint32_t before;

    HalMock_Reset();
    before = I2cErrors();
    HalMock_FailI2cAt(0u);  /* DEVICE_ID register select */
    Queue1(0x80u);
    Queue1(0x00u);
    Queue2(0xA011u);
    Queue2(0x0040u);
    Queue2(0x7060u);
    Queue2(0x0200u);
    Queue2(0x0145u);
    CHECK(AudioControl_Init());

    ExpectBegin();
    CS_SELECT(0x01u);       /* failed: no read follows */
    ExpectCs2100Configuration();
    WAIT(500u);
    CS_SELECT(0x02u); CS_R1();
    WAIT(40u);
    CS_SELECT(0x02u); CS_R1();
    ExpectCodecProbeAndConfigure();
    ExpectEnd(__LINE__);
    CHECK_EQ_U(I2cErrors() - before, 1u);
}

static void test_configuration_write_failure_stops(void)
{
    uint32_t before;
    AudioControlStatus st;

    HalMock_Reset();
    before = I2cErrors();
    Queue1(0x6Cu);
    HalMock_FailI2cAt(5u);  /* ID select, ID read, 3 writes, then RATIO_1 */
    CHECK(!AudioControl_Init());

    ExpectBegin();
    CS_SELECT(0x01u); CS_R1();
    CS_W2(0x16u, 0x10u);
    CS_W2(0x17u, 0x00u);
    CS_W2(0x06u, 0x07u);
    CS_W2(0x07u, 0x80u);    /* failed */
    ExpectEnd(__LINE__);
    CHECK_EQ_U(I2cErrors() - before, 1u);
    AudioControl_Read(&st);
    CHECK(!st.clockLocked);
    CHECK(!st.codecConfigured);
}

static void test_wrong_codec_part_is_not_configured(void)
{
    AudioControlStatus st;

    HalMock_Reset();
    Queue1(0x6Cu);
    Queue1(0x80u);
    Queue1(0x00u);
    Queue2(0x0011u);        /* part 0x00 */
    CHECK(!AudioControl_Init());

    ExpectBegin();
    ExpectCs2100Full();
    CODEC_SELECT(0x0000u); CODEC_R2();
    ExpectEnd(__LINE__);
    AudioControl_Read(&st);
    CHECK(st.clockLocked);
    CHECK(!st.codecConfigured);
}

static void test_codec_configuration_failure(void)
{
    uint32_t before;

    HalMock_Reset();
    before = I2cErrors();
    QueueLockedBringUp();
    /* 14 clock transfers, CHIP_ID select+read, CLK_CTRL, then I2S_CTRL fails. */
    HalMock_FailI2cAt(17u);
    CHECK(!AudioControl_Init());

    ExpectBegin();
    ExpectCs2100Full();
    CODEC_SELECT(0x0000u); CODEC_R2();
    CODEC_W(0x0004u, 0x0008u);
    CODEC_W(0x0006u, 0x0130u);  /* failed */
    ExpectEnd(__LINE__);
    CHECK_EQ_U(I2cErrors() - before, 1u);
}

static void BringUp(void)
{
    HalMock_Reset();
    QueueLockedBringUp();
    CHECK(AudioControl_Init());
    HalMock_ClearLog();
}

/* Read values 0x020C (ADCDAC_CTRL) and 0x0135 (ANA_CTRL) are queued first. */
static void ExpectUnmute(uint16_t hp)
{
    ExpectBegin();
    CODEC_SELECT(0x000Eu); CODEC_R2();
    CODEC_W(0x000Eu, 0x0200u);                  /* DAC unmute: clear 0x000C */
    CODEC_W(0x0022u, hp);                       /* headphone level */
    CODEC_SELECT(0x0024u); CODEC_R2();
    CODEC_W(0x0024u, 0x0125u);                  /* headphone unmute: clear 0x0010 */
    ExpectEnd(__LINE__);
}

static void test_unmute_sequence_and_level(void)
{
    AudioControlStatus st;

    BringUp();
    Queue2(0x020Cu);
    Queue2(0x0135u);
    CHECK(AudioControl_Unmute(50u));
    /* 50 %: 0x7F - ((49 * 79 + 49) / 99) = 0x58 */
    ExpectUnmute(0x5858u);
    AudioControl_Read(&st);
    CHECK(st.unmuted);

    BringUp();
    Queue2(0x020Cu);
    Queue2(0x0135u);
    CHECK(AudioControl_Unmute(10u));
    /* 10 %: 0x7F - ((9 * 79 + 49) / 99) = 0x78 */
    ExpectUnmute(0x7878u);
}

static void test_unmute_level_is_capped(void)
{
    BringUp();
    Queue2(0x020Cu);
    Queue2(0x0135u);
    CHECK(AudioControl_Unmute(90u));
    ExpectUnmute(0x5858u);
    CHECK_EQ_U(VSS_CFG_VOLUME_PERCENT_MAX, 50u);
}

static void test_unmute_i2c_failure(void)
{
    AudioControlStatus st;

    BringUp();
    HalMock_FailI2cAt(0u);
    CHECK(!AudioControl_Unmute(50u));
    AudioControl_Read(&st);
    CHECK(!st.unmuted);
}

/* Runs last: the mute latch holds until reset. */
static void test_mute_latch(void)
{
    AudioControlStatus st;

    BringUp();
    Queue2(0x020Cu);
    Queue2(0x0135u);
    CHECK(AudioControl_Unmute(50u));
    HalMock_ClearLog();

    Queue2(0x0125u);
    AudioControl_MuteLatched();
    ExpectBegin();
    CODEC_SELECT(0x0024u); CODEC_R2();
    CODEC_W(0x0024u, 0x0135u);                  /* headphone mute: set 0x0010 */
    CODEC_W(0x0022u, 0x7F7Fu);                  /* lowest level */
    ExpectEnd(__LINE__);

    AudioControl_Read(&st);
    CHECK(st.muteLatched);
    CHECK(!st.unmuted);

    HalMock_ClearLog();
    CHECK(!AudioControl_Unmute(50u));
    AudioControl_MuteLatched();
    CHECK_EQ_U(HalMock_Log()->count, 0u);

    /* A new bring-up does not release the latch. */
    BringUp();
    CHECK(!AudioControl_Unmute(50u));
    CHECK_EQ_U(HalMock_Log()->count, 0u);
    AudioControl_Read(&st);
    CHECK(st.muteLatched);
}

int main(void)
{
    test_bring_up_sequence();
    test_unlocked_clock_leaves_codec_untouched();
    test_id_read_failure_does_not_stop_configuration();
    test_configuration_write_failure_stops();
    test_wrong_codec_part_is_not_configured();
    test_codec_configuration_failure();
    test_unmute_sequence_and_level();
    test_unmute_level_is_capped();
    test_unmute_i2c_failure();
    test_mute_latch();
    TEST_FINISH();
}
