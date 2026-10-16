#include "Mp3PcmProvider.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
static unsigned int groups;
#define CHECK(expr) do { checks++; if (!(expr)) { \
    (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    exit(1); } } while (0)

typedef struct {
    uint16_t before[8];
    uint16_t pcm[MP3_PCM_PROVIDER_MAX_RENDER_FRAMES * 2U];
    uint16_t after[8];
} GuardedHalf;

typedef struct {
    uint32_t frames;
    uint32_t hash;
    uint32_t tail;
    uint32_t nonzero;
} DecodeResult;

static void fill_guard(GuardedHalf *half)
{
    size_t i;
    for (i = 0U; i < 8U; ++i) {
        half->before[i] = UINT16_C(0xa539);
        half->after[i] = UINT16_C(0x5ac6);
    }
    for (i = 0U; i < MP3_PCM_PROVIDER_MAX_RENDER_FRAMES * 2U; ++i)
        half->pcm[i] = UINT16_C(0x6666);
}

static void check_guard(const GuardedHalf *half)
{
    size_t i;
    for (i = 0U; i < 8U; ++i) {
        CHECK(half->before[i] == UINT16_C(0xa539));
        CHECK(half->after[i] == UINT16_C(0x5ac6));
    }
}

static void asset_reader(void)
{
    AudioAsset_IdType id;
    uint32_t size;
    uint32_t got;
    uint8_t bytes[18];
    AudioAsset_Init();
    for (id = 0U; id < AUDIO_ASSET_COUNT; ++id) {
        CHECK(AudioAsset_GetSize(id, &size) == AUDIO_ASSET_STATUS_OK);
        CHECK(size > 32U);
        (void)memset(bytes, 0xaa, sizeof(bytes));
        CHECK(AudioAsset_Read(id, 0U, &bytes[1], 16U, &got) == AUDIO_ASSET_STATUS_OK);
        CHECK(got == 16U);
        CHECK(bytes[0] == 0xaaU && bytes[17] == 0xaaU);
        if (id <= AUDIO_ASSET_ID_WARNING_WAV) {
            CHECK(memcmp(&bytes[1], "RIFF", 4U) == 0);
            CHECK(memcmp(&bytes[9], "WAVE", 4U) == 0);
        }
        CHECK(AudioAsset_Read(id, size - 3U, bytes, 4U, &got) == AUDIO_ASSET_STATUS_OUT_OF_RANGE);
        CHECK(got == 3U);
        CHECK(AudioAsset_Read(id, size, NULL, 0U, &got) == AUDIO_ASSET_STATUS_OK);
        CHECK(got == 0U);
        CHECK(AudioAsset_Read(id, size + 1U, bytes, 1U, &got) == AUDIO_ASSET_STATUS_OUT_OF_RANGE);
        CHECK(got == 0U);
    }
    size = 99U;
    CHECK(AudioAsset_GetSize(255U, &size) == AUDIO_ASSET_STATUS_INVALID_ID);
    CHECK(size == 0U);
    CHECK(AudioAsset_GetSize(0U, NULL) == AUDIO_ASSET_STATUS_INVALID_ARG);
    CHECK(AudioAsset_Read(255U, 0U, bytes, 1U, &got) == AUDIO_ASSET_STATUS_INVALID_ID);
    CHECK(got == 0U);
    CHECK(AudioAsset_Read(0U, 0U, NULL, 1U, &got) == AUDIO_ASSET_STATUS_INVALID_ARG);
    CHECK(AudioAsset_Read(0U, 0U, bytes, 1U, NULL) == AUDIO_ASSET_STATUS_INVALID_ARG);
    groups++;
}

static DecodeResult decode(AudioAsset_IdType id, uint32_t requested)
{
    GuardedHalf half;
    DecodeResult result = {0U, UINT32_C(2166136261), 0U, 0U};
    uint32_t renders;
    boolean finished = FALSE;
    Mp3PcmProvider_SelectAsset(id, FALSE);
    Mp3PcmProvider_Reset();
    CHECK(g_mp3ProviderValid == TRUE);
    CHECK(g_mp3LoopEnabled == FALSE);
    CHECK(Mp3PcmProvider_GetLastRenderValidFrames() == 0U);
    for (renders = 0U; renders < 100000U; ++renders) {
        uint32_t i;
        uint32_t valid;
        uint32_t callsBefore = g_mp3DecodeCallCount;
        fill_guard(&half);
        Mp3PcmProvider_RenderStereo16(half.pcm, requested);
        valid = Mp3PcmProvider_GetLastRenderValidFrames();
        check_guard(&half);
        CHECK(valid <= requested);
        CHECK(g_mp3DecodeCallCount - callsBefore <= 66U);
        CHECK(g_mp3ProviderValid == TRUE);
        CHECK(g_mp3DecodeErrorCount == 0U);
        CHECK(g_mp3LoopCount == 0U);
        CHECK(g_mp3ActiveAssetId == id);
        for (i = 0U; i < valid * 2U; ++i) {
            const uint16_t sample = half.pcm[i];
            result.hash ^= (uint32_t)(sample & 255U);
            result.hash *= UINT32_C(16777619);
            result.hash ^= (uint32_t)(sample >> 8);
            result.hash *= UINT32_C(16777619);
            if (sample != 0U) result.nonzero++;
        }
        for (i = valid * 2U; i < requested * 2U; ++i) CHECK(half.pcm[i] == 0U);
        for (i = requested * 2U; i < MP3_PCM_PROVIDER_MAX_RENDER_FRAMES * 2U; ++i)
            CHECK(half.pcm[i] == UINT16_C(0x6666));
        result.frames += valid;
        if (valid < requested) {
            CHECK(g_mp3ProviderEof == TRUE);
            result.tail = valid;
            finished = TRUE;
            break;
        }
    }
    CHECK(finished == TRUE);
    CHECK(result.frames > 0U && result.nonzero > 0U);
    CHECK(g_mp3SampleRateHz == 48000U);
    CHECK(g_mp3Channels == 1U || g_mp3Channels == 2U);
    CHECK(g_mp3Layer == 3U);
    CHECK(sizeof(half.pcm[0]) == 2U);
    fill_guard(&half);
    Mp3PcmProvider_RenderStereo16(half.pcm, requested);
    CHECK(Mp3PcmProvider_GetLastRenderValidFrames() == 0U);
    CHECK(g_mp3LoopCount == 0U && g_mp3ProviderEof == TRUE);
    check_guard(&half);
    for (renders = 0U; renders < requested * 2U; ++renders) CHECK(half.pcm[renders] == 0U);
    return result;
}

static void actual_mp3_assets(void)
{
    AudioAsset_IdType id;
    for (id = AUDIO_ASSET_ID_MP3_TEST; id < AUDIO_ASSET_COUNT; ++id) {
        const DecodeResult whole = decode(id, 1024U);
        const DecodeResult odd = decode(id, 1009U);
        CHECK(whole.frames == odd.frames);
        CHECK(whole.hash == odd.hash);
        CHECK(whole.nonzero == odd.nonzero);
        CHECK(odd.tail > 0U && odd.tail < 1009U);
        (void)printf("asset=%u decoded_frames=%lu pcm_fnv1a=%08lx tail1024=%lu tail1009=%lu\n",
            (unsigned int)id, (unsigned long)whole.frames, (unsigned long)whole.hash,
            (unsigned long)whole.tail, (unsigned long)odd.tail);
        groups++;
    }
}

static void invalid_and_bounds(void)
{
    GuardedHalf half;
    uint32_t i;
    uint32_t errors;
    Mp3PcmProvider_SelectAsset(255U, FALSE);
    Mp3PcmProvider_Reset();
    fill_guard(&half);
    Mp3PcmProvider_RenderStereo16(half.pcm, 1024U);
    CHECK(g_mp3ProviderValid == FALSE);
    CHECK(g_mp3ActiveAssetId == 255U);
    CHECK(Mp3PcmProvider_GetLastRenderValidFrames() == 0U);
    CHECK(g_mp3DecodeCallCount == 0U);
    CHECK(g_mp3LoopCount == 0U);
    check_guard(&half);
    for (i = 0U; i < 2048U; ++i) CHECK(half.pcm[i] == 0U);
    Mp3PcmProvider_SelectAsset(AUDIO_ASSET_ID_WELCOME_WAV, FALSE);
    Mp3PcmProvider_Reset();
    Mp3PcmProvider_RenderStereo16(half.pcm, 1024U);
    CHECK(g_mp3ProviderValid == FALSE);
    CHECK(Mp3PcmProvider_GetLastRenderValidFrames() == 0U);
    CHECK(g_mp3DecodeCallCount <= 64U);
    CHECK(g_mp3DecodeErrorCount > 0U);
    CHECK(g_mp3ActiveAssetId == AUDIO_ASSET_ID_WELCOME_WAV);
    CHECK(g_mp3LoopCount == 0U);
    Mp3PcmProvider_SelectAsset(AUDIO_ASSET_ID_MP3_TEST, FALSE);
    Mp3PcmProvider_Reset();
    Mp3PcmProvider_RenderStereo16(half.pcm, 1U);
    CHECK(Mp3PcmProvider_GetLastRenderValidFrames() == 1U);
    errors = g_mp3DecodeErrorCount;
    Mp3PcmProvider_RenderStereo16(NULL, 1U);
    CHECK(Mp3PcmProvider_GetLastRenderValidFrames() == 0U);
    CHECK(g_mp3DecodeErrorCount == errors + 1U);
    CHECK(Mp3PcmProvider_WasLastRenderTerminal() == FALSE);
    fill_guard(&half);
    Mp3PcmProvider_RenderStereo16(half.pcm, 1025U);
    CHECK(Mp3PcmProvider_GetLastRenderValidFrames() == 0U);
    CHECK(g_mp3DecodeErrorCount == errors + 2U);
    check_guard(&half);
    for (i = 0U; i < 2048U; ++i) CHECK(half.pcm[i] == UINT16_C(0x6666));
    Mp3PcmProvider_RenderStereo16(half.pcm, 0U);
    CHECK(Mp3PcmProvider_GetLastRenderValidFrames() == 0U);
    CHECK(g_mp3DecodeErrorCount == errors + 2U);
    Mp3PcmProvider_Reset();
    CHECK(Mp3PcmProvider_GetLastRenderValidFrames() == 0U);
    groups++;
}

int main(void)
{
    asset_reader();
    actual_mp3_assets();
    invalid_and_bounds();
    (void)printf("PASS audio decode %u groups, %u checks; HOST_ONLY physical/target NOT_RUN\n",
        groups, checks);
    return 0;
}
