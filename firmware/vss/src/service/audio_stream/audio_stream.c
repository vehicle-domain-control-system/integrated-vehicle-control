#include "audio_stream.h"
#include "decoder/Mp3PcmProvider.h"
#include "../../driver/audio_tx/audio_tx.h"
#include "../../infra/vss_time.h"

#include <stddef.h>

typedef enum {
    STREAM_PHASE_IDLE = 0,
    STREAM_PHASE_PREPARED,
    STREAM_PHASE_PLAYING
} StreamPhase;

/* What was handed to the output in each half, so that boundaries can be
 * translated into facts once the DMA reaches or leaves the half. */
typedef struct {
    uint32_t token;
    bool hasPcm;
    bool first;     /* the session's first PCM */
    bool last;      /* a one-shot asset's last PCM */
} StreamHalfMeta;

static StreamHalfMeta s_half[2];

static StreamPhase s_phase;
static uint32_t s_token;
static bool s_loop;
static bool s_wroteAny;        /* session PCM was handed to the output */
static bool s_everStarted;     /* STARTED was reported */
static bool s_decodeDone;      /* nothing left to decode */
static bool s_stopRequested;
static bool s_decodeFailed;

/* Latched until reset: the transmit driver failed or refused a refill. */
static bool s_outputFailed;
static uint32_t s_outputFaultMask;

static StreamFact s_facts[AUDIO_STREAM_FACT_CAPACITY];
static size_t s_factHead;
static size_t s_factCount;

/* Decoded PCM for one half, written to the ring in one copy. */
static int16_t s_stage[AUDIO_TX_HALF_WORDS];

/* 64-bit extension of the 32-bit boundary tick. */
static uint32_t s_lastTick;
static uint64_t s_ticks;
static uint64_t s_lastBoundaryMs;

static uint64_t AudioStream_TickToMs(uint32_t tick)
{
    uint32_t ahead = tick - s_lastTick;

    if (ahead < 0x80000000UL)
    {
        s_ticks += ahead;
        s_lastTick = tick;
        return VssTime_TicksToMs(s_ticks);
    }
    /* An older tick (a fault event repeats the last boundary's tick). */
    return VssTime_TicksToMs(s_ticks - (uint32_t)(s_lastTick - tick));
}

static void AudioStream_ClearHalves(void)
{
    uint8_t i;

    for (i = 0U; i < 2U; ++i)
    {
        s_half[i].token = 0U;
        s_half[i].hasPcm = false;
        s_half[i].first = false;
        s_half[i].last = false;
    }
}

static void AudioStream_EndSession(void)
{
    s_phase = STREAM_PHASE_IDLE;
    s_loop = false;
    s_wroteAny = false;
    s_everStarted = false;
    s_decodeDone = false;
    s_stopRequested = false;
    s_decodeFailed = false;
    AudioStream_ClearHalves();
}

static void AudioStream_Store(StreamFactKind kind, uint64_t atMs, uint32_t faultMask)
{
    StreamFact *f = &s_facts[(s_factHead + s_factCount) % AUDIO_STREAM_FACT_CAPACITY];

    f->kind = kind;
    f->token = s_token;
    f->atMs = atMs;
    f->everStarted = s_everStarted;
    f->faultMask = faultMask;
    s_factCount++;
}

/* Reports a fact of the current session. When the collector has fallen
 * behind, the reserved slot reports the overflow instead and the session
 * ends here. Returns false in that case. */
static bool AudioStream_Report(StreamFactKind kind, uint64_t atMs, uint32_t faultMask)
{
    if (s_factCount < (AUDIO_STREAM_FACT_CAPACITY - 1U))
    {
        AudioStream_Store(kind, atMs, faultMask);
        return true;
    }
    if (s_factCount == (AUDIO_STREAM_FACT_CAPACITY - 1U))
    {
        AudioStream_Store(STREAM_FACT_FAULT, atMs, AUDIO_STREAM_FAULT_FACT_QUEUE);
    }
    AudioStream_EndSession();
    return false;
}

/* The output is gone for good: report FAULT for a live session and refuse
 * every later session. */
static void AudioStream_FailOutput(uint32_t faultMask, uint64_t atMs)
{
    s_outputFailed = true;
    s_outputFaultMask = faultMask;
    if (s_phase != STREAM_PHASE_IDLE)
    {
        (void)AudioStream_Report(STREAM_FACT_FAULT, atMs, faultMask);
    }
    AudioStream_EndSession();
}

static bool AudioStream_HoldsSessionPcm(uint8_t half)
{
    return s_half[half].hasPcm && (s_half[half].token == s_token);
}

/* The DMA finished 'ended' and began 'playing' at atMs. */
static void AudioStream_OnBoundary(uint8_t ended, uint8_t playing, uint64_t atMs)
{
    if (s_phase != STREAM_PHASE_PLAYING)
    {
        return;
    }
    if (AudioStream_HoldsSessionPcm(ended) && s_half[ended].last)
    {
        if (AudioStream_Report(STREAM_FACT_COMPLETED, atMs, 0U))
        {
            AudioStream_EndSession();
        }
        return;
    }
    if (AudioStream_HoldsSessionPcm(playing) && s_half[playing].first)
    {
        s_everStarted = true;
        if (!AudioStream_Report(STREAM_FACT_STARTED, atMs, 0U))
        {
            return;
        }
    }
    if ((s_stopRequested || s_decodeFailed) && !AudioStream_HoldsSessionPcm(playing))
    {
        if (AudioStream_Report(s_decodeFailed ? STREAM_FACT_FAULT : STREAM_FACT_STOPPED, atMs,
                               s_decodeFailed ? AUDIO_STREAM_FAULT_DECODE : 0U))
        {
            AudioStream_EndSession();
        }
    }
}

/* Decodes the next PCM for 'free' while the session runs. Returns the stage,
 * or NULL for silence. 'playing' is the half the DMA reads now. */
static const int16_t *AudioStream_Fill(uint8_t freeHalf, uint8_t playing)
{
    uint32_t errorsBefore;
    uint32_t valid;
    bool eof;

    /* Whatever this half held has finished playing. */
    s_half[freeHalf].token = 0U;
    s_half[freeHalf].hasPcm = false;
    s_half[freeHalf].first = false;
    s_half[freeHalf].last = false;

    if ((s_phase != STREAM_PHASE_PLAYING) || s_stopRequested || s_decodeFailed || s_decodeDone)
    {
        return NULL;
    }

    errorsBefore = Mp3PcmProvider_DecodeErrorCount();
    /* int16_t and uint16_t may alias each other. */
    Mp3PcmProvider_RenderStereo16((uint16_t *)s_stage, AUDIO_TX_HALF_FRAMES);
    valid = Mp3PcmProvider_LastRenderValidFrames();
    eof = Mp3PcmProvider_IsEof();

    if ((Mp3PcmProvider_DecodeErrorCount() != errorsBefore) || !Mp3PcmProvider_IsValid())
    {
        /* Partial PCM before the failure is dropped. */
        s_decodeFailed = true;
        return NULL;
    }
    if (valid == 0U)
    {
        /* The previous render ended exactly on the last frame (bytes after
         * it held no audio): the half playing now holds the last PCM. */
        s_decodeDone = true;
        if (eof && s_wroteAny && AudioStream_HoldsSessionPcm(playing))
        {
            s_half[playing].last = true;
        }
        else
        {
            s_decodeFailed = true;
        }
        return NULL;
    }

    s_half[freeHalf].token = s_token;
    s_half[freeHalf].hasPcm = true;
    s_half[freeHalf].first = !s_wroteAny;
    /* EOF without the terminal mark: bytes after the last frame were skipped
     * in this render; the PCM in it is still the last. */
    s_half[freeHalf].last = !s_loop && (Mp3PcmProvider_WasLastRenderTerminal() || eof);
    s_wroteAny = true;
    if (s_half[freeHalf].last)
    {
        s_decodeDone = true;
    }
    return s_stage;
}

void AudioStream_Init(void)
{
    s_outputFailed = false;
    s_outputFaultMask = 0U;
    s_token = 0U;
    s_factHead = 0U;
    s_factCount = 0U;
    s_lastTick = 0U;
    s_ticks = 0U;
    s_lastBoundaryMs = 0U;
    AudioStream_EndSession();
    Mp3PcmProvider_Init();
}

StreamResult AudioStream_Prepare(uint32_t token, AudioAsset_IdType asset, bool loop)
{
    if (s_outputFailed)
    {
        return STREAM_REJECTED;
    }
    if (s_phase != STREAM_PHASE_IDLE)
    {
        return STREAM_BUSY;
    }
    if (asset >= AUDIO_ASSET_COUNT)
    {
        return STREAM_REJECTED;
    }
    Mp3PcmProvider_SelectAsset(asset, loop);
    Mp3PcmProvider_Reset();
    if (!Mp3PcmProvider_IsValid())
    {
        return STREAM_REJECTED;
    }
    AudioStream_EndSession();
    s_token = token;
    s_loop = loop;
    s_phase = STREAM_PHASE_PREPARED;
    return STREAM_OK;
}

StreamResult AudioStream_RequestControl(uint32_t token, bool start)
{
    if ((s_phase == STREAM_PHASE_IDLE) || (token != s_token))
    {
        return STREAM_REJECTED;
    }
    if (start)
    {
        if (s_stopRequested)
        {
            return STREAM_REJECTED;
        }
        s_phase = STREAM_PHASE_PLAYING;
        return STREAM_OK;
    }
    if (!s_wroteAny)
    {
        /* Nothing reached the output: the session ends now. */
        if (AudioStream_Report(STREAM_FACT_STOPPED, s_lastBoundaryMs, 0U))
        {
            AudioStream_EndSession();
        }
        return STREAM_OK;
    }
    s_stopRequested = true;
    return STREAM_OK;
}

void AudioStream_Service(void)
{
    AudioTxEvent ev;
    AudioTxResult result;
    const int16_t *pcm;
    uint32_t txFault;
    uint64_t atMs;
    uint8_t freeHalf;
    uint8_t playing;

    /* After a transmit fault the driver reports nothing but FAULT. */
    if (s_outputFailed || !AudioTx_Advance(&ev))
    {
        return;
    }
    atMs = AudioStream_TickToMs(ev.boundaryTick);
    if (ev.kind == AUDIO_TX_EVENT_FAULT)
    {
        AudioStream_FailOutput(ev.faultMask, atMs);
        return;
    }
    if (ev.kind != AUDIO_TX_EVENT_HALF_FREE)
    {
        return;
    }
    s_lastBoundaryMs = atMs;
    freeHalf = (uint8_t)(ev.freeHalf & 1U);
    playing = (uint8_t)(freeHalf ^ 1U);

    AudioStream_OnBoundary(freeHalf, playing, atMs);
    pcm = AudioStream_Fill(freeHalf, playing);

    /* Every free half is written, with silence when there is no PCM, so the
     * driver never replays a stale half. */
    result = AudioTx_Request(freeHalf, ev.generation, pcm);
    if (result != AUDIO_TX_OK)
    {
        /* LATE: the driver latched a fault while writing. REJECTED: the
         * half was not written, so the driver will find it stale at the
         * next boundary. Either way the output cannot continue. */
        txFault = AudioTx_FaultMask();
        AudioStream_FailOutput((txFault != 0U) ? txFault : AUDIO_STREAM_FAULT_REQUEST, atMs);
    }
}

size_t AudioStream_Advance(StreamFact *out, size_t capacity)
{
    size_t n = 0U;

    if (out == NULL)
    {
        return 0U;
    }
    while ((n < capacity) && (s_factCount > 0U))
    {
        out[n] = s_facts[s_factHead];
        s_factHead = (s_factHead + 1U) % AUDIO_STREAM_FACT_CAPACITY;
        s_factCount--;
        n++;
    }
    return n;
}

bool AudioStream_IsIdle(void)
{
    return s_phase == STREAM_PHASE_IDLE;
}

bool AudioStream_OutputFailed(void)
{
    return s_outputFailed || (AudioTx_FaultMask() != 0U);
}
