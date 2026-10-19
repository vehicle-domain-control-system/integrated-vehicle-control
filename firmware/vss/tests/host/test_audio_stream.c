/* Audio stream service on the real transmit driver and the HAL stand-in.
 * Half-buffer boundaries are raised every 1024 frames at 48 kHz (21.33 ms);
 * the timer tick follows the same time line at 800 Hz. Each cue is also
 * decoded here directly with minimp3 to know how many halves of PCM it
 * yields, which fixes the boundaries at which STARTED and COMPLETED must be
 * reported. */

#include "test_util.h"
#include "hal_mock.h"

#include "../../src/service/audio_stream/audio_stream.h"
#include "../../src/service/audio_stream/decoder/Mp3PcmProvider.h"
#include "../../src/service/audio_stream/decoder/minimp3.h"
#include "../../src/service/asset/audio_asset.h"
#include "../../src/service/asset/vss_cue_table.h"
#include "../../src/driver/audio_tx/audio_tx.h"
#include "../../src/infra/vss_time.h"

#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

#define HALF_FRAMES   AUDIO_TX_HALF_FRAMES
#define MAX_ASSET     20000u
#define MAX_FACTS     64u
#define TOKEN_BASE    100u

/* ---- simulated time line --------------------------------------------- */

static uint32_t s_boundary;          /* boundaries raised since Start */
static StreamFact s_facts[MAX_FACTS];
static uint32_t s_factBoundary[MAX_FACTS];
static uint32_t s_factCount;
static double s_serviceWorstUs;

/* Tick of boundary k: k halves of 1024/48000 s at 800 ticks/s = k * 256/15. */
static uint32_t BoundaryTick(uint32_t k)
{
    return (uint32_t)(((uint64_t)k * 256u) / 15u);
}

static uint64_t BoundaryMs(uint32_t k)
{
    return VssTime_TicksToMs(BoundaryTick(k));
}

static double NowUs(void)
{
#if defined(_WIN32)
    LARGE_INTEGER f;
    LARGE_INTEGER c;

    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1e6 / (double)f.QuadPart;
#else
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
#endif
}

static void Collect(void)
{
    StreamFact f[4];
    size_t n;
    size_t i;

    while ((n = AudioStream_Advance(f, 4u)) > 0u)
    {
        for (i = 0u; i < n; ++i)
        {
            if (s_factCount < MAX_FACTS)
            {
                s_facts[s_factCount] = f[i];
                s_factBoundary[s_factCount] = s_boundary;
                s_factCount++;
            }
        }
    }
}

static void Service(void)
{
    double t0 = NowUs();
    double dt;

    AudioStream_Service();
    dt = NowUs() - t0;
    if (dt > s_serviceWorstUs)
    {
        s_serviceWorstUs = dt;
    }
}

/* Raises the next boundary and runs one service pass. */
static void Boundary(void)
{
    s_boundary++;
    HalMock_SetTicks(BoundaryTick(s_boundary));
    HalMock_Boundary();
    Service();
}

static void BoundaryAndCollect(void)
{
    Boundary();
    Collect();
}

static void StartOutput(void)
{
    HalMock_Reset();
    AudioTx_ResetForTest();
    AudioAsset_OverrideForTest(0u, NULL, 0u);
    AudioAsset_Init();
    CHECK(AudioTx_Init());
    CHECK(AudioTx_Start());
    AudioStream_Init();
    s_boundary = 0u;
    s_factCount = 0u;
}

static const int16_t *RingHalf(uint8_t half)
{
    return (const int16_t *)&HalMock_RingWords()[HAL_AUDIO_PRIME_WORDS + (uint32_t)half * HAL_AUDIO_HALF_WORDS];
}

static bool HalfIsSilent(uint8_t half)
{
    const int16_t *h = RingHalf(half);
    uint32_t i;

    for (i = 0u; i < HAL_AUDIO_HALF_WORDS; ++i)
    {
        if (h[i] != 0)
        {
            return false;
        }
    }
    return true;
}

/* Half written by the service at boundary k (k >= 1): A is freed at odd
 * boundaries, B at even ones. */
static uint8_t HalfWrittenAt(uint32_t k)
{
    return (uint8_t)(((k & 1u) != 0u) ? 0u : 1u);
}

/* ---- reference decode ------------------------------------------------ */

static uint8_t s_assetBytes[MAX_ASSET];
static mp3dec_t s_refDecoder;
static mp3d_sample_t s_refPcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
static int16_t s_refFirst[HALF_FRAMES];

typedef struct {
    uint32_t frames;        /* PCM frames (samples per channel) in the asset */
    uint32_t mp3Frames;
    bool     allMono48k;
} RefDecode;

static uint32_t LoadAsset(AudioAsset_IdType id)
{
    uint32_t size = 0u;
    uint32_t got = 0u;

    CHECK(AudioAsset_GetSize(id, &size) == AUDIO_ASSET_STATUS_OK);
    CHECK(size > 0u && size <= MAX_ASSET);
    if (size > MAX_ASSET)
    {
        return 0u;
    }
    CHECK(AudioAsset_Read(id, 0u, s_assetBytes, size, &got) == AUDIO_ASSET_STATUS_OK);
    CHECK_EQ_U(got, size);
    return size;
}

static RefDecode ReferenceDecode(const uint8_t *data, uint32_t size)
{
    RefDecode r;
    uint32_t offset = 0u;
    mp3dec_frame_info_t info;
    int samples;
    int i;

    r.frames = 0u;
    r.mp3Frames = 0u;
    r.allMono48k = true;
    memset(s_refFirst, 0, sizeof(s_refFirst));
    mp3dec_init(&s_refDecoder);
    while (offset < size)
    {
        memset(&info, 0, sizeof(info));
        samples = mp3dec_decode_frame(&s_refDecoder, data + offset, (int)(size - offset), s_refPcm, &info);
        if (info.frame_bytes <= 0)
        {
            break;
        }
        offset += (uint32_t)info.frame_bytes;
        if (samples > 0)
        {
            r.mp3Frames++;
            if ((info.hz != 48000) || (info.channels != 1) || (info.layer != 3))
            {
                r.allMono48k = false;
            }
            for (i = 0; i < samples; ++i)
            {
                if (r.frames + (uint32_t)i < HALF_FRAMES)
                {
                    s_refFirst[r.frames + (uint32_t)i] = s_refPcm[i];
                }
            }
            r.frames += (uint32_t)samples;
        }
    }
    return r;
}

/* ---- cases ------------------------------------------------------------ */

static const char *const s_names[AUDIO_ASSET_COUNT] = {
    "WELCOME", "GOODBYE", "LOCK", "UNLOCK", "LOCK_ERROR",
    "ANTI_PINCH", "OCCUPANT", "REAR_CAUTION", "REAR_EMERGENCY"
};

static uint32_t FindFact(StreamFactKind kind)
{
    uint32_t i;

    for (i = 0u; i < s_factCount; ++i)
    {
        if (s_facts[i].kind == kind)
        {
            return i;
        }
    }
    return MAX_FACTS;
}

/* Every cue: 48 kHz mono, STARTED at the boundary where its first PCM half
 * begins, COMPLETED at the boundary where its last PCM half ends. */
static void test_each_cue_plays_to_completion(void)
{
    AudioAsset_IdType id;

    for (id = 0u; id < AUDIO_ASSET_COUNT; ++id)
    {
        RefDecode ref;
        uint32_t size;
        uint32_t halves;
        uint32_t token = TOKEN_BASE + id;
        uint32_t i;
        uint32_t s;
        uint32_t c;
        const int16_t *first;
        bool firstMatches = true;

        StartOutput();
        size = LoadAsset(id);
        ref = ReferenceDecode(s_assetBytes, size);
        CHECK(ref.allMono48k);
        CHECK(ref.frames > 0u);
        halves = (ref.frames + HALF_FRAMES - 1u) / HALF_FRAMES;

        CHECK(AudioStream_Prepare(token, id, false) == STREAM_OK);
        CHECK(!AudioStream_IsIdle());
        CHECK(AudioStream_RequestControl(token, true) == STREAM_OK);

        /* The first refill holds the first decoded samples on both channels. */
        BoundaryAndCollect();
        first = RingHalf(HalfWrittenAt(1u));
        for (i = 0u; i < HALF_FRAMES; ++i)
        {
            if ((first[2u * i] != s_refFirst[i]) || (first[2u * i + 1u] != s_refFirst[i]))
            {
                firstMatches = false;
            }
        }
        CHECK(firstMatches);

        for (i = 0u; i < halves + 4u; ++i)
        {
            BoundaryAndCollect();
        }

        CHECK_EQ_U(s_factCount, 2u);
        s = FindFact(STREAM_FACT_STARTED);
        c = FindFact(STREAM_FACT_COMPLETED);
        CHECK(s < MAX_FACTS && c < MAX_FACTS && s < c);
        if (s < MAX_FACTS && c < MAX_FACTS)
        {
            /* First PCM written at boundary 1 plays from boundary 2; the last
             * PCM half, written at boundary 'halves', ends at halves + 2. */
            CHECK_EQ_U(s_factBoundary[s], 2u);
            CHECK_EQ_U(s_facts[s].atMs, BoundaryMs(2u));
            CHECK_EQ_U(s_facts[s].token, token);
            CHECK_EQ_U(s_factBoundary[c], halves + 2u);
            CHECK_EQ_U(s_facts[c].atMs, BoundaryMs(halves + 2u));
            CHECK_EQ_U(s_facts[c].token, token);
            CHECK(s_facts[c].everStarted);
        }
        CHECK(AudioStream_IsIdle());
        CHECK_EQ_U(Mp3PcmProvider_DecodeErrorCount(), 0u);
        CHECK_EQ_U(Mp3PcmProvider_LastSampleRateHz(), 48000u);
        CHECK_EQ_U(Mp3PcmProvider_LastChannels(), 1u);
        CHECK_EQ_U(AudioTx_FaultMask(), 0u);
        CHECK(HalfIsSilent(0u) && HalfIsSilent(1u));
        printf("cue %u %-14s %5u bytes %3u mp3 frames %6u pcm frames %3u halves  STARTED %llu ms  COMPLETED %llu ms\n",
               (unsigned)id, s_names[id], (unsigned)size, (unsigned)ref.mp3Frames, (unsigned)ref.frames,
               (unsigned)halves, (unsigned long long)BoundaryMs(2u),
               (unsigned long long)BoundaryMs(halves + 2u));
    }
}

static uint32_t BoundariesUntil(StreamFactKind kind, uint32_t limit);

/* Byte offset after the first mp3Frames frames of the loaded asset. */
static uint32_t PrefixBytes(uint32_t size, uint32_t mp3Frames)
{
    uint32_t offset = 0u;
    uint32_t n = 0u;
    mp3dec_frame_info_t info;

    mp3dec_init(&s_refDecoder);
    while (offset < size && n < mp3Frames)
    {
        memset(&info, 0, sizeof(info));
        (void)mp3dec_decode_frame(&s_refDecoder, s_assetBytes + offset, (int)(size - offset), s_refPcm, &info);
        if (info.frame_bytes <= 0)
        {
            break;
        }
        offset += (uint32_t)info.frame_bytes;
        n++;
    }
    return offset;
}

static uint8_t s_cutAsset[MAX_ASSET];

/* Eight frames give 9216 PCM frames, exactly nine halves: the last sample
 * fills the end of a half. With bytes after the last frame (a lone frame
 * header, which lets the decoder still accept the last frame) the end is
 * found only in the next render, which then holds no PCM; the half already
 * playing is the last. Either way COMPLETED comes when the ninth PCM half
 * ends. */
static void test_end_on_half_boundary(void)
{
    static const uint32_t trailing[2] = { 0u, 4u };
    uint32_t t;

    for (t = 0u; t < 2u; ++t)
    {
        RefDecode ref;
        uint32_t size;
        uint32_t cut;
        uint32_t c;

        StartOutput();
        size = LoadAsset(AUDIO_ASSET_ID_LOCK_ERROR);
        cut = PrefixBytes(size, 8u);
        memcpy(s_cutAsset, s_assetBytes, cut);
        memcpy(&s_cutAsset[cut], s_assetBytes, trailing[t]);
        ref = ReferenceDecode(s_cutAsset, cut + trailing[t]);
        CHECK_EQ_U(ref.frames, 9u * HALF_FRAMES);
        AudioAsset_OverrideForTest(AUDIO_ASSET_ID_LOCK_ERROR, s_cutAsset, cut + trailing[t]);

        CHECK(AudioStream_Prepare(30u + t, AUDIO_ASSET_ID_LOCK_ERROR, false) == STREAM_OK);
        CHECK(AudioStream_RequestControl(30u + t, true) == STREAM_OK);
        CHECK(BoundariesUntil(STREAM_FACT_COMPLETED, 20u) == 11u);
        c = FindFact(STREAM_FACT_COMPLETED);
        CHECK(c < MAX_FACTS);
        if (c < MAX_FACTS)
        {
            CHECK_EQ_U(s_facts[c].atMs, BoundaryMs(11u));
        }
        CHECK_EQ_U(s_factCount, 2u);
        CHECK_EQ_U(Mp3PcmProvider_DecodeErrorCount(), 0u);
        CHECK(AudioStream_IsIdle());
        AudioAsset_OverrideForTest(0u, NULL, 0u);
    }
}

static void test_cue_table_rows(void)
{
    AudioAsset_IdType a = 0xFFu;
    bool seen[AUDIO_ASSET_COUNT];
    size_t i;
    size_t j;

    memset(seen, 0, sizeof(seen));
    for (i = 0u; i < AUDIO_ASSET_CUE_ROWS; ++i)
    {
        const AudioAsset_CueRow *row = AudioAsset_CueAt(i);

        CHECK(row != NULL);
        if (row == NULL)
        {
            continue;
        }
        CHECK(row->asset < AUDIO_ASSET_COUNT);
        if (row->asset < AUDIO_ASSET_COUNT)
        {
            seen[row->asset] = true;
        }
        for (j = 0u; j < i; ++j)
        {
            CHECK(!(AudioAsset_CueAt(j)->signal == row->signal && AudioAsset_CueAt(j)->value == row->value));
        }
    }
    for (i = 0u; i < AUDIO_ASSET_COUNT; ++i)
    {
        CHECK(seen[i]);
    }
    CHECK(AudioAsset_CueAt(AUDIO_ASSET_CUE_ROWS) == NULL);

    CHECK(AudioAsset_FindCue(AUDIO_ASSET_CUE_SIGNAL_WELCOME, AUDIO_ASSET_CUE_VALUE_EVENT, &a));
    CHECK_EQ_U(a, AUDIO_ASSET_ID_WELCOME);
    CHECK(AudioAsset_FindCue(AUDIO_ASSET_CUE_SIGNAL_LOCK_COMPLETE, AUDIO_ASSET_CUE_VALUE_EVENT, &a));
    CHECK_EQ_U(a, AUDIO_ASSET_ID_LOCK);
    CHECK(AudioAsset_FindCue(AUDIO_ASSET_CUE_SIGNAL_ANTI_PINCH, AUDIO_ASSET_CUE_VALUE_ACTIVE, &a));
    CHECK_EQ_U(a, AUDIO_ASSET_ID_ANTI_PINCH);
    CHECK(AudioAsset_FindCue(AUDIO_ASSET_CUE_SIGNAL_OCCUPANT_HAZARD, AUDIO_ASSET_CUE_VALUE_ACTIVE, &a));
    CHECK_EQ_U(a, AUDIO_ASSET_ID_OCCUPANT);
    CHECK(AudioAsset_FindCue(AUDIO_ASSET_CUE_SIGNAL_REAR_RISK, AUDIO_ASSET_CUE_VALUE_CAUTION, &a));
    CHECK_EQ_U(a, AUDIO_ASSET_ID_REAR_CAUTION);
    CHECK(AudioAsset_FindCue(AUDIO_ASSET_CUE_SIGNAL_REAR_RISK, AUDIO_ASSET_CUE_VALUE_EMERGENCY, &a));
    CHECK_EQ_U(a, AUDIO_ASSET_ID_REAR_EMERGENCY);
    /* Clearing values and the activation lane have no sound. */
    CHECK(!AudioAsset_FindCue(AUDIO_ASSET_CUE_SIGNAL_REAR_RISK, 3u, &a));
    CHECK(!AudioAsset_FindCue(AUDIO_ASSET_CUE_SIGNAL_ANTI_PINCH, AUDIO_ASSET_CUE_VALUE_EVENT, &a));
    CHECK(!AudioAsset_FindCue(8u, 6u, &a));
}

/* Runs boundaries until a fact of this kind appears; returns how many
 * boundaries it took (limit + 1 when it did not). */
static uint32_t BoundariesUntil(StreamFactKind kind, uint32_t limit)
{
    uint32_t n;

    for (n = 1u; n <= limit; ++n)
    {
        BoundaryAndCollect();
        if (FindFact(kind) < MAX_FACTS)
        {
            return n;
        }
    }
    return limit + 1u;
}

static void test_stop_while_playing(void)
{
    uint32_t n;
    uint32_t i;
    uint32_t f;

    StartOutput();
    CHECK(AudioStream_Prepare(7u, AUDIO_ASSET_ID_OCCUPANT, false) == STREAM_OK);
    CHECK(AudioStream_RequestControl(7u, true) == STREAM_OK);
    for (i = 0u; i < 10u; ++i)
    {
        BoundaryAndCollect();
    }
    CHECK(FindFact(STREAM_FACT_STARTED) < MAX_FACTS);

    CHECK(AudioStream_RequestControl(7u, false) == STREAM_OK);
    n = BoundariesUntil(STREAM_FACT_STOPPED, 4u);
    CHECK(n <= 2u);
    f = FindFact(STREAM_FACT_STOPPED);
    if (f < MAX_FACTS)
    {
        CHECK(s_facts[f].everStarted);
        CHECK_EQ_U(s_facts[f].token, 7u);
        CHECK_EQ_U(s_facts[f].atMs, BoundaryMs(s_factBoundary[f]));
        /* At that boundary the playing half holds no session PCM. */
        CHECK(HalfIsSilent(HalfWrittenAt(s_factBoundary[f] - 1u)));
    }
    CHECK(AudioStream_IsIdle());
    CHECK(FindFact(STREAM_FACT_COMPLETED) == MAX_FACTS);
    BoundaryAndCollect();
    BoundaryAndCollect();
    CHECK(HalfIsSilent(0u) && HalfIsSilent(1u));
    CHECK_EQ_U(s_factCount, 2u);
}

/* STOP after the first PCM was written but before it played: STARTED is
 * still reported when it plays, then STOPPED one boundary later. */
static void test_stop_before_first_pcm_plays(void)
{
    uint32_t s;
    uint32_t t;

    StartOutput();
    CHECK(AudioStream_Prepare(8u, AUDIO_ASSET_ID_LOCK, false) == STREAM_OK);
    CHECK(AudioStream_RequestControl(8u, true) == STREAM_OK);
    BoundaryAndCollect();
    CHECK_EQ_U(s_factCount, 0u);
    CHECK(AudioStream_RequestControl(8u, false) == STREAM_OK);
    CHECK(BoundariesUntil(STREAM_FACT_STOPPED, 4u) <= 2u);
    s = FindFact(STREAM_FACT_STARTED);
    t = FindFact(STREAM_FACT_STOPPED);
    CHECK(s < MAX_FACTS && t < MAX_FACTS);
    if (s < MAX_FACTS && t < MAX_FACTS)
    {
        CHECK_EQ_U(s_factBoundary[s], 2u);
        CHECK_EQ_U(s_factBoundary[t], 3u);
        CHECK(s_facts[t].everStarted);
    }
}

/* No PCM reached the output: STOPPED at once, never started. */
static void test_stop_before_any_pcm(void)
{
    uint32_t i;

    /* Prepared, not started. */
    StartOutput();
    BoundaryAndCollect();
    CHECK(AudioStream_Prepare(9u, AUDIO_ASSET_ID_WELCOME, false) == STREAM_OK);
    CHECK(AudioStream_RequestControl(9u, false) == STREAM_OK);
    Collect();
    CHECK_EQ_U(s_factCount, 1u);
    CHECK(s_facts[0].kind == STREAM_FACT_STOPPED);
    CHECK(!s_facts[0].everStarted);
    CHECK_EQ_U(s_facts[0].token, 9u);
    CHECK_EQ_U(s_facts[0].atMs, BoundaryMs(1u));
    CHECK(AudioStream_IsIdle());

    /* Started, stopped before the first refill. */
    CHECK(AudioStream_Prepare(10u, AUDIO_ASSET_ID_WELCOME, false) == STREAM_OK);
    CHECK(AudioStream_RequestControl(10u, true) == STREAM_OK);
    CHECK(AudioStream_RequestControl(10u, false) == STREAM_OK);
    Collect();
    CHECK_EQ_U(s_factCount, 2u);
    CHECK(s_facts[1].kind == STREAM_FACT_STOPPED);
    CHECK(!s_facts[1].everStarted);
    CHECK(AudioStream_IsIdle());
    for (i = 0u; i < 4u; ++i)
    {
        BoundaryAndCollect();
    }
    CHECK_EQ_U(s_factCount, 2u);
    CHECK(HalfIsSilent(0u) && HalfIsSilent(1u));
}

static void test_loop_runs_until_stop(void)
{
    RefDecode ref;
    uint32_t size;
    uint32_t halves;
    uint32_t i;
    uint32_t f;

    StartOutput();
    size = LoadAsset(AUDIO_ASSET_ID_LOCK);
    ref = ReferenceDecode(s_assetBytes, size);
    halves = (ref.frames + HALF_FRAMES - 1u) / HALF_FRAMES;

    CHECK(AudioStream_Prepare(11u, AUDIO_ASSET_ID_LOCK, true) == STREAM_OK);
    CHECK(AudioStream_RequestControl(11u, true) == STREAM_OK);
    for (i = 0u; i < 5u * halves; ++i)
    {
        BoundaryAndCollect();
    }
    CHECK_EQ_U(s_factCount, 1u);
    CHECK(s_facts[0].kind == STREAM_FACT_STARTED);
    CHECK(!AudioStream_IsIdle());
    CHECK_EQ_U(Mp3PcmProvider_DecodeErrorCount(), 0u);

    CHECK(AudioStream_RequestControl(11u, false) == STREAM_OK);
    CHECK(BoundariesUntil(STREAM_FACT_STOPPED, 4u) <= 2u);
    f = FindFact(STREAM_FACT_STOPPED);
    CHECK(f < MAX_FACTS && s_facts[f].everStarted);
    CHECK(FindFact(STREAM_FACT_COMPLETED) == MAX_FACTS);
    CHECK(AudioStream_IsIdle());
}

/* MPEG-1 Layer III frames at 44.1 kHz, 128 kbit/s, mono, no audio data:
 * well-formed but at the wrong sample rate for the output. */
#define BAD_FRAME_BYTES 417u
#define BAD_FRAMES      12u
static uint8_t s_badAsset[MAX_ASSET + BAD_FRAME_BYTES * BAD_FRAMES];

static uint32_t BuildBadFrames(uint8_t *dst)
{
    uint32_t i;

    memset(dst, 0, BAD_FRAME_BYTES * BAD_FRAMES);
    for (i = 0u; i < BAD_FRAMES; ++i)
    {
        dst[i * BAD_FRAME_BYTES + 0u] = 0xFFu;
        dst[i * BAD_FRAME_BYTES + 1u] = 0xFBu;
        dst[i * BAD_FRAME_BYTES + 2u] = 0x90u;
        dst[i * BAD_FRAME_BYTES + 3u] = 0xC0u;
    }
    return BAD_FRAME_BYTES * BAD_FRAMES;
}

static void test_decode_error_before_start(void)
{
    uint32_t size = BuildBadFrames(s_badAsset);
    uint32_t f;
    RefDecode ref = ReferenceDecode(s_badAsset, size);

    /* The reference decoder accepts the frames: the rate check refuses them. */
    CHECK(ref.mp3Frames > 0u);
    CHECK(!ref.allMono48k);

    StartOutput();
    AudioAsset_OverrideForTest(AUDIO_ASSET_ID_WELCOME, s_badAsset, size);
    CHECK(AudioStream_Prepare(12u, AUDIO_ASSET_ID_WELCOME, false) == STREAM_OK);
    CHECK(AudioStream_RequestControl(12u, true) == STREAM_OK);
    CHECK(BoundariesUntil(STREAM_FACT_FAULT, 4u) <= 2u);
    f = FindFact(STREAM_FACT_FAULT);
    CHECK(f < MAX_FACTS);
    if (f < MAX_FACTS)
    {
        CHECK(!s_facts[f].everStarted);
        CHECK_EQ_U(s_facts[f].faultMask, AUDIO_STREAM_FAULT_DECODE);
        CHECK_EQ_U(s_facts[f].token, 12u);
    }
    CHECK_EQ_U(s_factCount, 1u);
    CHECK(Mp3PcmProvider_DecodeErrorCount() > 0u);
    CHECK(AudioStream_IsIdle());
    CHECK(HalfIsSilent(0u) && HalfIsSilent(1u));
    CHECK_EQ_U(AudioTx_FaultMask(), 0u);

    /* A decode failure ends the session only; the next one plays. */
    AudioAsset_OverrideForTest(0u, NULL, 0u);
    CHECK(AudioStream_Prepare(13u, AUDIO_ASSET_ID_LOCK, false) == STREAM_OK);
    CHECK(AudioStream_RequestControl(13u, true) == STREAM_OK);
    CHECK(BoundariesUntil(STREAM_FACT_COMPLETED, 60u) <= 60u);
}

/* The cue, then frames the output cannot play: STARTED, then FAULT once the
 * cue's PCM has left the output. */
static void test_decode_error_while_playing(void)
{
    uint32_t cue;
    uint32_t size;
    uint32_t s;
    uint32_t f;
    uint32_t i;

    StartOutput();
    cue = LoadAsset(AUDIO_ASSET_ID_LOCK);
    memcpy(s_badAsset, s_assetBytes, cue);
    size = cue + BuildBadFrames(&s_badAsset[cue]);
    AudioAsset_OverrideForTest(AUDIO_ASSET_ID_LOCK, s_badAsset, size);

    CHECK(AudioStream_Prepare(14u, AUDIO_ASSET_ID_LOCK, false) == STREAM_OK);
    CHECK(AudioStream_RequestControl(14u, true) == STREAM_OK);
    for (i = 0u; i < 60u && FindFact(STREAM_FACT_FAULT) == MAX_FACTS; ++i)
    {
        BoundaryAndCollect();
    }
    s = FindFact(STREAM_FACT_STARTED);
    f = FindFact(STREAM_FACT_FAULT);
    CHECK(s < MAX_FACTS && f < MAX_FACTS);
    CHECK(FindFact(STREAM_FACT_COMPLETED) == MAX_FACTS);
    if (f < MAX_FACTS)
    {
        CHECK(s_facts[f].everStarted);
        CHECK_EQ_U(s_facts[f].faultMask, AUDIO_STREAM_FAULT_DECODE);
        CHECK(HalfIsSilent(HalfWrittenAt(s_factBoundary[f] - 1u)));
    }
    CHECK(AudioStream_IsIdle());
    AudioAsset_OverrideForTest(0u, NULL, 0u);
}

/* A late refill latches a transmit fault: one FAULT fact, then nothing, and
 * no later session. */
static void test_transmit_fault_ends_output(void)
{
    uint32_t f;
    uint32_t i;

    StartOutput();
    CHECK(AudioStream_Prepare(15u, AUDIO_ASSET_ID_GOODBYE, false) == STREAM_OK);
    CHECK(AudioStream_RequestControl(15u, true) == STREAM_OK);
    for (i = 0u; i < 4u; ++i)
    {
        BoundaryAndCollect();
    }
    /* Two boundaries without a service pass: the half freed by the first
     * one is reached unwritten. */
    s_boundary++;
    HalMock_SetTicks(BoundaryTick(s_boundary));
    HalMock_Boundary();
    BoundaryAndCollect();
    f = FindFact(STREAM_FACT_FAULT);
    CHECK(f < MAX_FACTS);
    if (f < MAX_FACTS)
    {
        CHECK((s_facts[f].faultMask & AUDIO_TX_FAULT_LATE) != 0u);
        CHECK(s_facts[f].everStarted);
        CHECK_EQ_U(s_facts[f].token, 15u);
    }
    CHECK(AudioStream_IsIdle());
    for (i = 0u; i < 4u; ++i)
    {
        BoundaryAndCollect();
    }
    CHECK_EQ_U(s_factCount, 2u);
    CHECK(AudioStream_Prepare(16u, AUDIO_ASSET_ID_LOCK, false) == STREAM_REJECTED);
    CHECK(AudioStream_RequestControl(15u, false) == STREAM_REJECTED);
    CHECK_EQ_U(HalMock_AudioStops(), 1u);
}

/* No boundary for longer than the stall limit. */
static void test_stall_ends_output(void)
{
    uint32_t f;

    StartOutput();
    CHECK(AudioStream_Prepare(17u, AUDIO_ASSET_ID_LOCK, false) == STREAM_OK);
    CHECK(AudioStream_RequestControl(17u, true) == STREAM_OK);
    BoundaryAndCollect();
    BoundaryAndCollect();
    HalMock_SetTicks(BoundaryTick(s_boundary) + AUDIO_TX_STALL_TICKS + 1u);
    Service();
    Collect();
    f = FindFact(STREAM_FACT_FAULT);
    CHECK(f < MAX_FACTS);
    if (f < MAX_FACTS)
    {
        CHECK_EQ_U(s_facts[f].faultMask, AUDIO_TX_FAULT_STALL);
        CHECK_EQ_U(s_facts[f].atMs, BoundaryMs(s_boundary));
    }
    Service();
    Service();
    Collect();
    CHECK_EQ_U(s_factCount, 2u);
    CHECK(AudioStream_Prepare(18u, AUDIO_ASSET_ID_LOCK, false) == STREAM_REJECTED);

    /* Idle output: the fault is latched without a fact. */
    StartOutput();
    HalMock_SetTicks(AUDIO_TX_STALL_TICKS + 1u);
    Service();
    Collect();
    CHECK_EQ_U(s_factCount, 0u);
    CHECK(AudioStream_Prepare(19u, AUDIO_ASSET_ID_LOCK, false) == STREAM_REJECTED);
}

static void test_session_guards(void)
{
    StartOutput();
    CHECK(AudioStream_IsIdle());
    CHECK(AudioStream_RequestControl(1u, true) == STREAM_REJECTED);
    CHECK(AudioStream_Prepare(1u, AUDIO_ASSET_COUNT, false) == STREAM_REJECTED);
    CHECK(AudioStream_IsIdle());
    CHECK(AudioStream_Prepare(1u, AUDIO_ASSET_ID_WELCOME, false) == STREAM_OK);
    CHECK(AudioStream_Prepare(2u, AUDIO_ASSET_ID_LOCK, false) == STREAM_BUSY);
    CHECK(AudioStream_RequestControl(2u, true) == STREAM_REJECTED);
    CHECK(AudioStream_RequestControl(1u, true) == STREAM_OK);
    BoundaryAndCollect();
    CHECK(AudioStream_RequestControl(1u, false) == STREAM_OK);
    CHECK(AudioStream_RequestControl(1u, true) == STREAM_REJECTED);

    /* An empty asset cannot be prepared. */
    StartOutput();
    AudioAsset_OverrideForTest(AUDIO_ASSET_ID_WELCOME, s_badAsset, 0u);
    CHECK(AudioStream_Prepare(3u, AUDIO_ASSET_ID_WELCOME, false) == STREAM_REJECTED);
    CHECK(AudioStream_IsIdle());
    AudioAsset_OverrideForTest(0u, NULL, 0u);
}

/* Facts left uncollected: the reserved slot reports the overflow. */
static void test_fact_overflow(void)
{
    uint32_t session;
    StreamFact f[AUDIO_STREAM_FACT_CAPACITY + 1u];
    size_t n;
    size_t i;
    uint32_t j;

    StartOutput();
    for (session = 0u; session < 4u; ++session)
    {
        CHECK(AudioStream_Prepare(20u + session, AUDIO_ASSET_ID_LOCK, false) == STREAM_OK);
        CHECK(AudioStream_RequestControl(20u + session, true) == STREAM_OK);
        for (j = 0u; j < 40u && !AudioStream_IsIdle(); ++j)
        {
            Boundary();
        }
        CHECK(AudioStream_IsIdle());
    }
    n = AudioStream_Advance(f, AUDIO_STREAM_FACT_CAPACITY + 1u);
    CHECK_EQ_U(n, AUDIO_STREAM_FACT_CAPACITY);
    for (i = 0u; i + 1u < n; ++i)
    {
        CHECK(f[i].kind == ((i % 2u) == 0u ? STREAM_FACT_STARTED : STREAM_FACT_COMPLETED));
    }
    CHECK(f[n - 1u].kind == STREAM_FACT_FAULT);
    CHECK_EQ_U(f[n - 1u].faultMask, AUDIO_STREAM_FAULT_FACT_QUEUE);
    CHECK_EQ_U(f[n - 1u].token, 23u);
    CHECK_EQ_U(AudioStream_Advance(f, 4u), 0u);
}

int main(void)
{
    test_cue_table_rows();
    test_each_cue_plays_to_completion();
    test_end_on_half_boundary();
    test_stop_while_playing();
    test_stop_before_first_pcm_plays();
    test_stop_before_any_pcm();
    test_loop_runs_until_stop();
    test_decode_error_before_start();
    test_decode_error_while_playing();
    test_transmit_fault_ends_output();
    test_stall_ends_output();
    test_session_guards();
    test_fact_overflow();
    printf("SERVICE_WORST_US %.1f (host, one AudioStream_Service call incl. decode and ring copy)\n",
           s_serviceWorstUs);
    TEST_FINISH();
}
