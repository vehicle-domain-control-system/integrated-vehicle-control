# Original audio decode reference

This directory contains the supplied, working S32K344 audio project's real
internal-flash assets, AudioAsset reader, minimp3 decoder and MP3 provider.
`PROVENANCE.json` identifies the original ZIP, each original ZIP member and its
SHA-256, the current copy SHA-256, and whether the copy is byte-exact.
`provider_adaptation.patch` records the only changes to original source files.
The original archive and audited project were not edited.

Eight of ten copied files are byte-exact. Only `Mp3PcmProvider.c` and
`Mp3PcmProvider.h` differ:

- Expose `Mp3PcmProvider_GetLastRenderValidFrames()`, reset its value on reset
  and at call entry, and record decoded frames before zero-fill. This count is
  provider output evidence; it says nothing about DMA consumption or sound.
- Reject null output and requests above 1024 frames before writing. The caller
  must provide at least `2 * frameCount` writable uint16 elements. C cannot
  determine the actual allocation size of a non-null pointer.
- Reject non-MP3 asset IDs when resetting the MP3 provider. The original generic
  byte reader includes WAV IDs 0 through 2; scanning their bytes as MP3 can find
  false frame sync. Only the supplied MP3 IDs 3 through 7 are admitted here.
- Clear the per-render terminal flag even on rejected calls to avoid carrying
  a prior call's marker into a failed render result.

There is no new PCM transfer buffer: the original 4096-byte compressed window,
decoded-frame storage and existing minimp3 static scratch remain. The provider
is single-owner, non-reentrant. Original `Init()` behavior is retained for
traceability; the VSS adapter must explicitly select an admitted asset with
`SelectAsset(id, FALSE)` and call `Reset()` before rendering. Original defaults
are not authorization to start output. No automatic fallback, demo playlist,
loop, or automatic Welcome is introduced by the VSS integration.

`host_shim/Std_Types.h` is an isolated host typedef shim, guarded by
`VSS_AUDIO_REFERENCE_HOST`. It must not be installed or used as target RTD SDK
headers. The production integration must compile against the real target SDK.

## Reproduce the host test

From the repository root:

```text
python firmware/vss/audio_reference/run_host_decode.py --out firmware/vss/build/audio_decode
```

The runner records exact commands, stdout, stderr and return codes. It builds
the asset reader, provider and test with C99, `-Wall -Wextra -Werror -Wpedantic
-Wconversion -Wshadow -Wstrict-prototypes -Wmissing-prototypes` at O0 and O2.
Only the exact copied `Minimp3Port.c` implementation unit adds
`-Wno-conversion -Wno-sign-conversion`; the original decoder emits these
conversion diagnostics. The failing strict vendor probe is retained as an
expected, optional diagnostic. No vendor decoder source was altered to silence
warnings. The added provider statements and tests receive full strict checks.

The source list is `AudioAsset.c`, `Mp3PcmProvider.c`, `Minimp3Port.c` and
`../tests/test_audio_decode.c`; include directories are `host_shim` and this
directory, with `-DVSS_AUDIO_REFERENCE_HOST`. The embedded asset headers and
`minimp3.h` are included by those source files and must not be separate units.

Local GCC 6.3.0 O0 and O2 each passed 7 groups / 125,913 checks. The tests read
all 8 real assets, decode each of the 5 MP3 assets to EOF, compare frame counts
and PCM FNV-1a hashes for 1024-frame and 1009-frame rendering, verify the exact
partial-tail count and zero-fill, protect sentinels and check output beyond
the requested length remains untouched. Invalid asset IDs, WAV passed to MP3,
null/oversized output, reset and EOF cannot fall back or loop. Counts represent
decoder output, including any encoded delay/padding; no gapless or acoustic
duration claim is made. FNV hashes are regression observations, not authenticity
or cryptographic integrity checks; the source manifest uses SHA-256.

| MP3 ID | Decoded frames | PCM FNV-1a | Tail at 1024 | Tail at 1009 |
|---|---:|---|---:|---:|
| 3 | 39168 | 9ac2eb51 | 256 | 826 |
| 4 | 387072 | 8715c6f1 | 0 | 625 |
| 5 | 252288 | c93d4785 | 384 | 38 |
| 6 | 252288 | c159ca9d | 384 | 38 |
| 7 | 252288 | 7b0fc50d | 384 | 38 |

A 1024 tail of zero means the decoded length is an exact multiple and the next
render observed EOF. All assets still exercise a partial tail at 1009 frames.
The reference test does not prove target buildability, WCET, safe DMA write
windows, codec readiness, physical start/end, safe reuse or hardware shutdown.
Those remain separate P3 gates.

## License and provenance boundary

The complete original minimp3 header, including its public-domain/CC0 notice,
is preserved. It identifies `https://github.com/lieff/minimp3` and
`http://creativecommons.org/publicdomain/zero/1.0/` in the supplied source.
No internet version was substituted. The original `Minimp3Port.c` documents
the existing static decoder scratch change required by the original target's
stack budget; this copied `minimp3.h` already contains that change.

No separate license document for the project wrappers or embedded audio assets
was found in the supplied ZIP. They are retained as user-supplied project
reference material; this package does not assert new redistribution rights
or a license for those bytes. The manifest records this status per file.
