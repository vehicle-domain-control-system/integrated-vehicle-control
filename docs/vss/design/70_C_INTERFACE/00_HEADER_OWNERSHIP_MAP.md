# Header Ownership / Typed C Interface — R6

> 2026-10-08 · 최신 승인 기준 R5-C1 독립 PASS · **LOGICAL R6 CONTRACT / 후보 pseudo prototype**

> 위 기준·아래 R6 자체 검수/중단 표현은 **R6 작업 당시 이력**이다. R6는 인계의 독립 PASS로 승인됐고 최신 작업은 [R7 통합 검수](../../../../R7_RESULT_SUMMARY.md)다. 이 문서의 선언·133 타입·18 후보·TBD-R6-01~08 본문은 보존하며 실제 Header/ABI/물리 충분성을 새로 확정하지 않았다.

[Document Map](../00_OVERVIEW/01_DOCUMENT_MAP.md) · [R3 전수 판정](../40_DATA/00_DATA_OVERVIEW.md#type-decisions) · [18 Core의 R3 대응](../40_DATA/00_DATA_OVERVIEW.md#function-data-map) · [R5-C1 Flow](../60_PSEUDOCODE/00_PSEUDOCODE_OVERVIEW.md) · [결과·자체 검수](../../../../R6_RESULT_SUMMARY.md)

R6는 선언의 책임·가시성·typed 입력/결과·참조 보호와 include 방향을 정리한다. 기존 5 Layer / 10 Module / 18 Core, R3의 53 struct / 292 field / 42 enum / 38 의미 alias와 32 provisional 판정, R4 Contract 5종, R5-C1 Flow 및 Governance v1.1은 보존한다. 이 문서는 실제 C/H 파일, vendor ABI, 물리 Binding, Build/보드 PASS가 아니다.

<a id="provenance"></a>
## 1. 근거 수준과 기존 Header 재사용

| 표시 | 의미 |
| --- | --- |
| CONFIRMED FROM PROVIDED SOURCE | 제공된 `.h`의 실제 선언 또는 `.c`의 실제 정의/사용만 확인. 현재 R3 타입이 그 파일에 구현됐다는 뜻은 아님 |
| PROVISIONAL REUSE CANDIDATE | 기존 경계의 재사용 방향. `.h` 미제공·R3 typed 선언 미존재·transitive include를 확인 못 한 범위 포함 |
| LOGICAL R6 CONTRACT | 승인된 의미를 지킬 선언 owner/visibility/입력·출력·보호 의무. 물리 파일 실재/ABI와 구별 |
| B2-R / HW TBD | 최신 전체 Header/생성 설정·RTD/메모리/원시각/물리 충분조건이 필요한 실물 결합 |

인계의 `reference/BRINGUP_SOURCE_PROVENANCE.md`를 먼저 읽고 일부 source를 읽기 전용으로 대조했다. 결과 ZIP은 이 참고 source를 중복 배포하지 않는다. 원본 경로·행·SHA는 결과 Summary에서 확인한다.

| 확인한 기존 선언/정의 | 확인 범위 | R6에서의 한계·사용 결정 |
| --- | --- | --- |
| `AudioPlayer.h`: SetAppHooks, SelectSource, Begin/StopPlaybackRequest, RenderNextBuffer, PrimeBuffers, OnBufferComplete/Start | CONFIRMED FROM PROVIDED SOURCE. 직접 include는 Std_Types.h / AudioPcmProvider.h | STREAM 기존 경계 재사용 후보. 후자의 Header 내용은 미제공. 현재 uint/boolean·sourceId·debug global은 최종 R3 타입/원귀속/시각/수명 계약이 아님 |
| `AudioDmaTransport.h`: PrimePlayerBuffers, InitDmaPath, InitSaiPath, Start, Service, readiness/error 관측 | CONFIRMED FROM PROVIDED SOURCE. 직접 include는 Std_Types.h, 공개 인자에 RTD type 없음 | TX 재사용 우선. 기존 Started/HasError flag를 actual/NO_START/termination/안전 반환으로 승격하지 않음 |
| `AudioDmaTransport.c`: AudioPlayer_DmaTxIrqCallback / SaiTxCallback / Sai0IrqHandler | CONFIRMED FROM PROVIDED SOURCE인 정의. vendor include와 메모리/TCD는 이 구현 쪽에 있음 | 해당 이름의 prefix가 Player여도 transport/BSP callback 경계다. Header 외부 노출/registration key·실제 ABI는 B2-R / HW TBD |
| `VssAudioSession.c`: Init, SubmitRequest, Process, GetState, GetActiveRequest, private StartRequest | 정의와 Port 호출 확인. VssAudioSession.h 본문은 없음 | 기존 Session 경계 재사용 후보. single pending·current flag를 승인된 Occurrence/Attempt/late 계약으로 고정하지 않음 |
| `VssAudioArbitrator.h`, `AudioAsset.h`, `Mp3PcmProvider.h`, `SoundAsset.h` | `.c` include/호출 + 이전 Binding의 symbol 근거. 실제 `.h` 내용 미제공 | PROVISIONAL REUSE CANDIDATE. 실물 Header 확인·R3 선언 확인으로 기입하지 않음 |
| `AudioCodec.h`, `AudioClock.h`, `AudioAppControl.h`, `AudioPcmProvider.h` | 제공 `.c/.h`의 include/사용만 확인; Header 본문 없음 | 기존 제어/app/provider 위치 후보. transitive vendor 노출/상태 쓰기/최종 ABI는 미확인 |
| INPUT / STORE / Structured HEALTH / VSS_STATUS | 승인 문서가 논리 경계를 요구함. 최종 Network·저장·Health Header는 없음 | LOGICAL R6 CONTRACT. 별도 새 Module/Header/framework를 먼저 만들지 않음 |

<a id="header-candidates"></a>
## 2. 선언 경계 후보와 가시성

`H-*`는 아래 표를 가리키는 문서 label이며 Header 파일명·새 타입·추가 Module 목록이 아니다. 같은 기존 물리 파일이 여러 논리 owner를 담을 수 있어도 writer 책임은 분리한다. 모든 새 typed 선언의 status는 LOGICAL R6 CONTRACT이며, 기존 Header 재사용은 아래 provenance 범위다.

| label / 논리 책임 | 기존 선언 위치 후보·재사용 판단 | 확인 근거와 한계 | 후보 수준 |
| --- | --- | --- | --- |
| H-FLOW / FLOW | 현재 main/AudioAppControl 실행 경계. 별도 FLOW Header 필요성 미확인 | main.c의 main / AudioAppControl.c의 private ProcessVss | PROVISIONAL REUSE CANDIDATE |
| H-INPUT / INPUT | 기존 Node Communication/TEST 경계의 의미 입력 선언 위치; 파일명 미확인 | 제공 source 없음 | LOGICAL R6 CONTRACT |
| H-STORE / STORE | 기존 Arbitrator/Session의 private 저장·수용 경계 우선; 별도 RequestStore Header 선생성 금지 | 기존 Binding + VssAudioSession.c. Arbitrator 본문/두 Header 미제공 | PROVISIONAL REUSE CANDIDATE |
| H-POLICY / POLICY | VssAudioArbitrator.h 재사용 후보; 중앙 정책/불변 계획의 선언만 분리해서 소비 | AudioAppControl.c의 include/호출 + 기존 Binding; .h/.c 본문 미제공 | PROVISIONAL REUSE CANDIDATE |
| H-PB / PLAYBACK | VssAudioSession.h 재사용 후보; Context는 private | VssAudioSession.c의 정의/Port 사용. .h 미제공 | PROVISIONAL REUSE CANDIDATE |
| H-ASSET / ASSET | AudioAsset.h / SoundAsset.h 기존 제한 읽기·descriptor 선언 재사용 후보 | Mp3PcmProvider.c / AudioAppControl.c의 include/호출; 두 .h와 ASSET .c 미제공 | PROVISIONAL REUSE CANDIDATE |
| H-STREAM / AUDIO STREAM | AudioPlayer.h 재사용 우선. Mp3PcmProvider.h는 provider 내부 후보 | AudioPlayer.h/.c 실물; Mp3PcmProvider.c만 제공 | PROVISIONAL REUSE CANDIDATE |
| H-TX / AUDIO TX | AudioDmaTransport.h의 vendor 중립 경계 재사용 우선 | AudioDmaTransport.h/.c 실물. R3 타입/후보 Core 선언은 현재 없음 | PROVISIONAL REUSE CANDIDATE |
| H-CONTROL / AUDIO CONTROL | AudioCodec.h / AudioClock.h의 기존 제어 경계 재사용 후보 | main.c의 include/호출만; 두 .h/.c 미제공 | PROVISIONAL REUSE CANDIDATE |
| H-HAL / HAL/BSP Boundary | 기존 BSP/transport 내부 선언 위치. generated callback 외부 symbol은 Binding 내부 | AudioDmaTransport.c의 3 callback/IRQ 정의. 등록 record/후속 raw 경계는 미구현 | LOGICAL R6 CONTRACT |
| H-TIME / Runtime Boundary | 기존 timebase의 중립 시간 선언 위치; 파일명 미확인 | main.c의 AudioClock_GetRefTicks는 존재 사용 근거만 | LOGICAL R6 CONTRACT |
| H-READ / 각 stamp owner / FLOW 연결 | 기존 중립 읽기 선언 위치; 실제 Header 미확인 | R3 OwnerStamp/ReadBasis; 새로운 revision manager 없음 | LOGICAL R6 CONTRACT |
| H-HEALTH / HEALTH | 기존 진단/보고 연결의 제한·복구·상태 선언 위치; 파일명 미확인 | Structured HEALTH/Node Communication source 미제공 | LOGICAL R6 CONTRACT |

Public은 **기존 허용 Caller가 사용하는 VSS 경계**라는 뜻이며 외부 사용자 SDK를 확장하는 뜻이 아니다. Internal은 해당 구현 협업자에게만 보이는 보호된 타입/관측이다. Private는 owner 원본·내부 처리로 제한한다. HAL/Runtime은 Layer Boundary를 유지한다. FillBuffer, decoder 임시, callback lookup/기록 보존 처리는 private/internal이며 새 public API로 승격하지 않는다.

<a id="type-map"></a>
## 3. R3 타입 → 의미 owner / producer·consumer / 선언 경계

아래 53개 struct는 R3 원문의 필드·owner를 그대로 따른다. Header label은 선언 hosting 후보이며 의미 writer 이전이 아니다. 42 enum과 38 alias는 다음 표에서 전수 연결한다. cross-owner 자료를 큰 state/command/result로 재포장하지 않는다. 보호 상세는 [수명 표](#lifetime), complete type/forward declaration은 [include 규칙](#include-direction)을 따른다.

| R3 struct / 선언 role | 의미 owner / producer → consumer | visibility / Header 후보 / 사용권 |
| --- | --- | --- |
| [VssTimeEvidence](../40_DATA/10_INPUT_DATA.md#vsstimeevidence) / D-TIME | Runtime Boundary / 기존 Runtime 시간 도구 → FLOW/INPUT/STORE/PLAYBACK/AUDIO STREAM/DRIVER/HEALTH | Public typed 경계; H-TIME; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssInputMeta](../40_DATA/10_INPUT_DATA.md#vssinputmeta) / D-INPUT | INPUT의 검증된 의미 Meta. 수신 원본 생성 책임은 기존 Node Communication/TEST다. / Node Communication/TEST 원본 → INPUT 검증 → INPUT/STORE | Public typed 경계; H-INPUT; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssInputEvidence](../40_DATA/10_INPUT_DATA.md#vssinputevidence) / D-INPUT | INPUT 경계의 해석 의미. 원본 자료 writer는 기존 통신/TEST다. / 기존 Node Communication 또는 격리 TEST → Input_Process | Public typed 경계; H-INPUT; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssInputContext](../40_DATA/10_INPUT_DATA.md#vssinputcontext) / D-INPUT | INPUT 단일 writer / Input_Process → Input_Process / FLOW·HEALTH의 읽기 관측 | Private 원본 / Internal const 관측; H-INPUT; owner만 mutable; foreign write 금지 |
| [VssOneShotEvent](../40_DATA/10_INPUT_DATA.md#vssoneshotevent) / D-INPUT | INPUT의 검증 사건. 수용 이후 occurrence 원본은 STORE다. / Input_Process → Store_ApplyInput | Public typed 경계; H-INPUT; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssStateUpdate](../40_DATA/10_INPUT_DATA.md#vssstateupdate) / D-INPUT | INPUT 검증 의미 / STORE 적용 상태 / Input_Process → Store_ApplyInput | Public typed 경계; H-INPUT; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssQualityChange](../40_DATA/10_INPUT_DATA.md#vssqualitychange) / D-INPUT | INPUT의 품질 검증 / Input_Process → Store_ApplyInput / FLOW·HEALTH 읽기 | Public typed 경계; H-INPUT; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssInputApplication](../40_DATA/10_INPUT_DATA.md#vssinputapplication) / D-INPUT | INPUT / Input_Process → FLOW | Public typed 경계; H-INPUT; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssFactTime](../40_DATA/10_INPUT_DATA.md#vssfacttime) / D-TIME | 해당 사실을 포착한 producer의 불변 근거 / 원래 의미 producer / HAL 포착 / 실제 관측 주체 → INPUT/DRIVER/AUDIO STREAM/PLAYBACK/HEALTH | Public typed 경계; H-TIME; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssOccurrenceRecord](../40_DATA/20_STORE_DATA.md#vssoccurrencerecord) / D-STORE | STORE 단일 writer / Store_ApplyInput / Store_ApplyPlaybackFact / Store_AdvanceDeadlines → STORE / FLOW의 읽기 경계 → POLICY/PLAYBACK/HEALTH | Private 원본; H-STORE; owner만 mutable; foreign write 금지 |
| [VssStatefulState](../40_DATA/20_STORE_DATA.md#vssstatefulstate) / D-STORE | STORE 단일 writer / Store_ApplyInput / Store_AdvanceDeadlines → STORE / FLOW → POLICY/PLAYBACK/HEALTH | Private 원본; H-STORE; owner만 mutable; foreign write 금지 |
| [VssRearGateState](../40_DATA/20_STORE_DATA.md#vssreargatestate) / D-STORE | STORE / Store_ApplyInput → STORE 후보 결합 / POLICY·PLAYBACK 읽기 | Private 원본; H-STORE; owner만 mutable; foreign write 금지 |
| [VssOwnerStamp](../40_DATA/30_SELECTION_DATA.md#vssownerstamp) / D-READ | 각 원본 owner의 읽기 표지 / INPUT/STORE/PLAYBACK/AUDIO STREAM/HEALTH의 반영 완료 읽기 경계 → FLOW의 관측 수집/일관성 검토 | Public typed 경계; H-READ; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssReadBasis](../40_DATA/30_SELECTION_DATA.md#vssreadbasis) / D-READ | FLOW의 임시 수집 근거. stamp 원본은 각 owner다. / 각 owner stamp + Runtime → FLOW 수집 → POLICY/PLAYBACK/HEALTH | Public typed 경계; H-READ; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssCandidateObservation](../40_DATA/30_SELECTION_DATA.md#vsscandidateobservation) / D-STORE | STORE 파생 읽기 / STORE Core 반영 후 기존 읽기 관측 → FLOW → POLICY/PLAYBACK / HEALTH 보고 | Public typed 경계; H-STORE; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssCueDescriptor](../40_DATA/30_SELECTION_DATA.md#vsscuedescriptor) / D-PLAN | POLICY 읽기 전용 중앙 정책 / 기존 의미→전체 패턴 정책 → Select_Choose / PLAYBACK 정상 cue 진행 | Public typed 경계; H-POLICY; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssPlanDescriptor](../40_DATA/30_SELECTION_DATA.md#vssplandescriptor) / D-PLAN | POLICY / 중앙 읽기 전용 정책 / Select_Choose 연결 → FLOW → PLAYBACK → AUDIO STREAM의 의미 준비 | Public typed 경계; H-POLICY; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssSelectionDecision](../40_DATA/30_SELECTION_DATA.md#vssselectiondecision) / D-PLAN | POLICY / Select_Choose → FLOW → Playback_RequestTransition | Public typed 경계; H-POLICY; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssSessionContext](../40_DATA/40_PLAYBACK_DATA.md#vsssessioncontext) / D-PB | PLAYBACK 단일 writer / Playback_RequestTransition/Playback_Advance → PLAYBACK / FLOW·HEALTH 읽기 projection | Private 원본; H-PB; owner만 mutable; foreign write 금지 |
| [VssAttemptContext](../40_DATA/40_PLAYBACK_DATA.md#vssattemptcontext) / D-PB | PLAYBACK 단일 writer / Playback_RequestTransition/Playback_Advance → PLAYBACK / AUDIO STREAM 요청 연결 / 읽기 관측 | Private 원본; H-PB; owner만 mutable; foreign write 금지 |
| [VssPlaybackFact](../40_DATA/40_PLAYBACK_DATA.md#vssplaybackfact) / D-PB | PLAYBACK의 판정 사실 / 적용 이력 writer는 STORE / Playback_Advance → Store_ApplyPlaybackFact | Public typed 경계; H-PB; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssPlaybackObservation](../40_DATA/40_PLAYBACK_DATA.md#vssplaybackobservation) / D-PB | PLAYBACK 파생 읽기 / Playback_Advance 반영 후 기존 읽기 경계 → FLOW → Store_AdvanceDeadlines / POLICY / HEALTH | Public typed 경계; H-PB; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssAssetDescriptor](../40_DATA/50_AUDIO_STREAM_DATA.md#vssassetdescriptor) / D-ASSET | ASSET / 현재 read-only 이미지/배치 근거 → ASSET → Asset_Read / AUDIO STREAM / FLOW의 읽기 가용 근거 | Public typed 경계; H-ASSET; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssAssetReadRequest](../40_DATA/50_AUDIO_STREAM_DATA.md#vssassetreadrequest) / D-ASSET | AUDIO STREAM 요구 / ASSET 검증 / AudioStream_Prepare 또는 AudioStream_Advance 내부 생산 → Asset_Read | Public typed 경계; H-ASSET; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssAssetSpan](../40_DATA/50_AUDIO_STREAM_DATA.md#vssassetspan) / D-ASSET | ASSET의 제공 자료/참조 원본 / Asset_Read → AUDIO STREAM provider | Public typed 경계; H-ASSET; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssAudioPreparation](../40_DATA/50_AUDIO_STREAM_DATA.md#vssaudiopreparation) / D-STREAM | PLAYBACK의 요구 의미 / AUDIO STREAM의 수용 문맥 / Playback_RequestTransition 또는 Playback_Advance → AudioStream_Prepare | Public typed 경계; H-STREAM; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssPlaybackControlCommand](../40_DATA/50_AUDIO_STREAM_DATA.md#vssplaybackcontrolcommand) / D-STREAM | PLAYBACK 제어 의미 / Playback_RequestTransition/Playback_Advance → AudioStream_RequestControl | Public typed 경계; H-STREAM; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssProviderContext](../40_DATA/50_AUDIO_STREAM_DATA.md#vssprovidercontext) / D-STREAM | AUDIO STREAM 단일 writer / AudioStream_Prepare/AudioStream_RequestControl/AudioStream_Advance → AUDIO STREAM 내부 생산/정리 | Private 원본; H-STREAM; owner만 mutable; foreign write 금지 |
| [VssAudioRequestResult](../40_DATA/50_AUDIO_STREAM_DATA.md#vssaudiorequestresult) / D-STREAM | AUDIO STREAM / AudioStream_Prepare/RequestControl/Advance → PLAYBACK | Public typed 경계; H-STREAM; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssAudioOutputEvidence](../40_DATA/50_AUDIO_STREAM_DATA.md#vssaudiooutputevidence) / D-STREAM | AUDIO STREAM의 Backend 근거. transport 원본 producer는 AUDIO TX다. / AudioStream_Advance → Playback_Advance | Public typed 경계; H-STREAM; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssBackendCleanupObservation](../40_DATA/50_AUDIO_STREAM_DATA.md#vssbackendcleanupobservation) / D-STREAM | AUDIO STREAM 파생 읽기 / AudioStream_Advance → PLAYBACK / FLOW의 복구 안전 정리 조율 | Public typed 경계; H-STREAM; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssPcmCycleKey](../40_DATA/50_AUDIO_STREAM_DATA.md#vsspcmcyclekey) / D-PCM | AUDIO STREAM의 회차 생성 / AudioStream_Prepare/AudioStream_Advance 내부 생산 → AUDIO TX/HAL → AUDIO STREAM 반환 대조 | Public typed 경계; H-STREAM; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssPcmBufferState](../40_DATA/50_AUDIO_STREAM_DATA.md#vsspcmbufferstate) / D-PCM | AUDIO STREAM 단일 writer / AudioStream_Prepare/AudioStream_Advance 내부 PCM 생산 → AUDIO STREAM / AudioTx_Request의 보호된 인계 | Private 원본; H-STREAM; owner만 mutable; foreign write 금지 |
| [VssPcmHandoffCommand](../40_DATA/60_AUDIO_TX_DATA.md#vsspcmhandoffcommand) / D-PCM | AUDIO STREAM 인계 의미 / AudioStream_Prepare/AudioStream_Advance 내부 PCM 생산 → AudioTx_Request | Public typed 경계; H-STREAM; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssTxControlCommand](../40_DATA/60_AUDIO_TX_DATA.md#vsstxcontrolcommand) / D-TX | AUDIO STREAM 제어 의미 / AudioStream_RequestControl 또는 AudioStream_Advance 내부 진행 → AudioTx_Request | Public typed 경계; H-TX; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssTxOperation](../40_DATA/60_AUDIO_TX_DATA.md#vsstxoperation) / D-TX | AUDIO TX 단일 writer / AudioTx_Request/AudioTx_Advance → AUDIO TX / HAL 등록 연결 | Private 원본; H-TX; owner만 mutable; foreign write 금지 |
| [VssTxRequestResult](../40_DATA/60_AUDIO_TX_DATA.md#vsstxrequestresult) / D-TX | AUDIO TX / AudioTx_Request/AudioTx_Advance → AUDIO STREAM | Public typed 경계; H-TX; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssObservationCoverage](../40_DATA/60_AUDIO_TX_DATA.md#vssobservationcoverage) / D-TX | 관측 producer의 원래 근거 / HAL 포착 / AUDIO TX 범위 검증 → AUDIO TX/AUDIO STREAM/PLAYBACK | Public typed 경계; H-TX; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssTxOutputEvidence](../40_DATA/60_AUDIO_TX_DATA.md#vsstxoutputevidence) / D-TX | AUDIO TX / AudioTx_Advance의 실제 근거 적용 → AudioStream_Advance | Public typed 경계; H-TX; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssPcmConsumptionEvidence](../40_DATA/60_AUDIO_TX_DATA.md#vsspcmconsumptionevidence) / D-TX | AUDIO TX / AudioTx_Advance → AudioStream_Advance | Public typed 경계; H-TX; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssPcmReturnEvidence](../40_DATA/60_AUDIO_TX_DATA.md#vsspcmreturnevidence) / D-TX | AUDIO TX / AudioTx_Request의 명확한 무접근 거부 또는 AudioTx_Advance → AudioStream_Advance / 즉시 요청 결과 적용 | Public typed 경계; H-TX; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssHalTxRegistration](../40_DATA/60_AUDIO_TX_DATA.md#vsshaltxregistration) / D-HAL | HAL/BSP Layer의 HAL Boundary / HAL의 기존 귀속 보호 경계. AUDIO TX가 원래 의미를 제공한다. → AudioHAL_Callback / AudioTx_Advance | Internal 보호 기록; H-HAL; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssHalDeviceRegistration](../40_DATA/60_AUDIO_TX_DATA.md#vsshaldeviceregistration) / D-HAL | HAL Boundary / HAL의 기존 device 제어 등록 경계 → AudioHAL_Callback / AudioControl_Service | Internal 보호 기록; H-HAL; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssRawObservation](../40_DATA/60_AUDIO_TX_DATA.md#vssrawobservation) / D-HAL | HAL Boundary raw 포착 / vendor/실제 관측 → AudioHAL_Callback → AudioTx_Advance 또는 AudioControl_Service | Internal 보호 기록; H-HAL; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssDeviceControlIntent](../40_DATA/60_AUDIO_TX_DATA.md#vssdevicecontrolintent) / D-CONTROL | AUDIO STREAM 요구 의미 / AUDIO CONTROL의 실제 제어 / AUDIO STREAM Prepare/RequestControl/Advance → AudioControl_Service | Public typed 경계; H-CONTROL; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssDeviceReadinessState](../40_DATA/60_AUDIO_TX_DATA.md#vssdevicereadinessstate) / D-CONTROL | AUDIO CONTROL 단일 writer / AudioControl_Service → AUDIO STREAM의 준비·제한/HEALTH의 진단 관측 | Private 원본 / Internal const 관측; H-CONTROL; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssDeviceControlResult](../40_DATA/60_AUDIO_TX_DATA.md#vssdevicecontrolresult) / D-CONTROL | AUDIO CONTROL / AudioControl_Service → AUDIO STREAM | Public typed 경계; H-CONTROL; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssDiagnosticEvidence](../40_DATA/70_DIAGNOSTIC_DATA.md#vssdiagnosticevidence) / D-DIAG | 원래 발견 주체의 불변 사실. Fault 분류/제한 writer는 HEALTH다. / INPUT/ASSET/PLAYBACK/AUDIO STREAM/AUDIO TX/AUDIO CONTROL/HAL의 해당 실패 경계 → FLOW → Health_Evaluate | Public typed 경계; H-HEALTH; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssFaultState](../40_DATA/70_DIAGNOSTIC_DATA.md#vssfaultstate) / D-HEALTH | HEALTH 단일 writer / Health_Evaluate → FLOW 실행 제한 / Health_BuildStatus | Private 원본 / Internal const 관측; H-HEALTH; owner만 mutable; foreign write 금지 |
| [VssRecoveryPermission](../40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoverypermission) / D-RECOVERY | HEALTH의 허용 writer. FLOW는 요구 연결만 한다. / Health_Evaluate → Flow_Process → AudioStream_RequestControl → 실제 Backend 수행 경계 | Public typed 경계; H-HEALTH; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssRecoveryPerformedEvidence](../40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoveryperformedevidence) / D-RECOVERY | 실제 Backend 수행 주체 / AudioStream_Advance / AudioControl_Service의 실제 수행 근거 → FLOW → Health_Evaluate | Public typed 경계; H-HEALTH; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssRecoveryVerificationEvidence](../40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoveryverificationevidence) / D-RECOVERY | 실제 Backend 효과 검증 주체 / AudioStream_Advance / AudioControl_Service의 별도 검증 → FLOW → Health_Evaluate | Public typed 경계; H-HEALTH; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |
| [VssStatusSnapshot](../40_DATA/70_DIAGNOSTIC_DATA.md#vssstatussnapshot) / D-STATUS | HEALTH 보고 책임 / Health_BuildStatus → FLOW → INPUT의 기존 통신 연결 → Node Communication | Public typed 경계; H-HEALTH; 제출/공개 뒤 immutable; 동기 borrow와 실제 async 참조 보호 분리 |

InputContext/FaultState/DeviceReadinessState의 const 관측은 R3가 지정한 읽기 소비자가 필요한 필드만 읽는 경계다. writer 원본 전체를 상위에 공유하거나 새 Snapshot/Context 복제 타입을 만들지 않는다. R6 후보의 IN/OUT 관측은 평가 수명에서 안정된 읽기 값/const projection이며, mutable 원본 주소를 후속 Result/Status에 장기 보관하지 않는다. 실제 불변 protection 방식은 수명 표와 TBD-R6-03을 따른다.

| R3 enum | 원 owner / producer → consumer | 선언 role / Header 후보 / 가시성 |
| --- | --- | --- |
| [VssProcessingOpportunity](../40_DATA/10_INPUT_DATA.md#vssprocessingopportunity) | Runtime Boundary의 알림 의미 / FLOW의 조율 / 기존 실행 진입·Runtime 알림 → FLOW | L-FLOW / H-FLOW / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssFlowProgress](../40_DATA/10_INPUT_DATA.md#vssflowprogress) | FLOW / Flow_Process → 기존 실행 진입 | L-FLOW / H-FLOW / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssContinuity](../40_DATA/10_INPUT_DATA.md#vsscontinuity) | Runtime/INPUT의 해당 근거 / Runtime 또는 INPUT 검증 → 관련 시간·출처 소비자 | L-TIME / H-TIME / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssInputQuality](../40_DATA/10_INPUT_DATA.md#vssinputquality) | INPUT 검증 / STORE 현재 품질 / INPUT → STORE/HEALTH 읽기 소비자 | L-INPUT / H-INPUT / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssInputSignal](../40_DATA/10_INPUT_DATA.md#vssinputsignal) | INPUT의 의미 변환 / 제품/TEST 변환 → STORE | L-INPUT / H-INPUT / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssInputValue](../40_DATA/10_INPUT_DATA.md#vssinputvalue) | INPUT 의미 변환 / 제품/TEST 변환 → INPUT/STORE | L-INPUT / H-INPUT / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssInputValidation](../40_DATA/10_INPUT_DATA.md#vssinputvalidation) | INPUT / Input_Process → FLOW | L-INPUT / H-INPUT / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssStoreAdmission](../40_DATA/20_STORE_DATA.md#vssstoreadmission) | STORE. NOT_SUBMITTED 표기는 INPUT의 미호출 표지다. / Store_ApplyInput / INPUT 미호출 → INPUT/FLOW | L-STORE / H-STORE / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssOccurrenceState](../40_DATA/20_STORE_DATA.md#vssoccurrencestate) | STORE / Store_ApplyInput/Store_ApplyPlaybackFact/Store_AdvanceDeadlines → POLICY/PLAYBACK/HEALTH 읽기 | L-STORE / H-STORE / Private; 값·숫자·폭 추가 없음 |
| [VssTrackingDisposition](../40_DATA/20_STORE_DATA.md#vsstrackingdisposition) | STORE / STORE Core → STORE | L-STORE / H-STORE / Private; 값·숫자·폭 추가 없음 |
| [VssFactApplication](../40_DATA/20_STORE_DATA.md#vssfactapplication) | STORE / Store_ApplyPlaybackFact → PLAYBACK | L-STORE / H-STORE / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssStoreProgress](../40_DATA/20_STORE_DATA.md#vssstoreprogress) | STORE / Store_AdvanceDeadlines → FLOW | L-STORE / H-STORE / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssReadValidity](../40_DATA/30_SELECTION_DATA.md#vssreadvalidity) | FLOW의 수집 판단 / Flow_Process → POLICY/PLAYBACK/HEALTH | L-READ / H-READ / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssCandidateValidity](../40_DATA/30_SELECTION_DATA.md#vsscandidatevalidity) | STORE 파생 읽기 / Store_ApplyInput/Store_AdvanceDeadlines 이후 읽기 경계 → POLICY/PLAYBACK | L-STORE / H-STORE / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssPlaybackMode](../40_DATA/30_SELECTION_DATA.md#vssplaybackmode) | POLICY 의미 / STORE 후보 분류 / 중앙 정책·STORE 의미 → POLICY/PLAYBACK | L-POLICY / H-POLICY / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssSelectionAction](../40_DATA/30_SELECTION_DATA.md#vssselectionaction) | POLICY / Select_Choose → FLOW/PLAYBACK | L-POLICY / H-POLICY / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssPlaybackPhase](../40_DATA/40_PLAYBACK_DATA.md#vssplaybackphase) | PLAYBACK / Playback_RequestTransition/Playback_Advance → PLAYBACK·읽기 관측 소비자 | L-PB / H-PB / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssStartKnowledge](../40_DATA/40_PLAYBACK_DATA.md#vssstartknowledge) | PLAYBACK / Playback_Advance → PLAYBACK/STORE·HEALTH 읽기 | L-PB / H-PB / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssPlaybackFactKind](../40_DATA/40_PLAYBACK_DATA.md#vssplaybackfactkind) | PLAYBACK의 전체 판정 / Playback_Advance → Store_ApplyPlaybackFact | L-PB / H-PB / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssRequestDisposition](../40_DATA/40_PLAYBACK_DATA.md#vssrequestdisposition) | 해당 요구를 적용하는 callee owner / Playback_RequestTransition 또는 AUDIO STREAM 준비/제어 수용 → caller | L-PB / H-PB / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssAssetReadDisposition](../40_DATA/50_AUDIO_STREAM_DATA.md#vssassetreaddisposition) | ASSET / Asset_Read → AUDIO STREAM | L-ASSET / H-ASSET / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssAudioControlAction](../40_DATA/50_AUDIO_STREAM_DATA.md#vssaudiocontrolaction) | PLAYBACK / Playback_RequestTransition/Playback_Advance → AudioStream_RequestControl | L-PCM / H-STREAM / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssProviderPhase](../40_DATA/50_AUDIO_STREAM_DATA.md#vssproviderphase) | AUDIO STREAM / AudioStream_Prepare/RequestControl/Advance → AUDIO STREAM | L-PCM / H-STREAM / Private; 값·숫자·폭 추가 없음 |
| [VssAudioRequestStage](../40_DATA/50_AUDIO_STREAM_DATA.md#vssaudiorequeststage) | AUDIO STREAM / Prepare/RequestControl/Advance → PLAYBACK | L-PCM / H-STREAM / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssAudioOutputKind](../40_DATA/50_AUDIO_STREAM_DATA.md#vssaudiooutputkind) | AUDIO STREAM의 Backend 근거 결합 / AudioStream_Advance → PLAYBACK | L-PCM / H-STREAM / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssPcmBufferId](../40_DATA/50_AUDIO_STREAM_DATA.md#vsspcmbufferid) | AUDIO STREAM의 PCM 의미 / 현재 Buffer 정의 → AUDIO STREAM/AUDIO TX/HAL | L-PCM / H-STREAM / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssPcmUsage](../40_DATA/50_AUDIO_STREAM_DATA.md#vsspcmusage) | AUDIO STREAM / Prepare/RequestControl/Advance 내부 PCM 처리 → AUDIO STREAM | L-PCM / H-STREAM / Private; 값·숫자·폭 추가 없음 |
| [VssOutputScope](../40_DATA/60_AUDIO_TX_DATA.md#vssoutputscope) | 원래 요청을 만드는 PLAYBACK/AUDIO STREAM와 하위 범위 적용 owner / AUDIO STREAM → AUDIO TX → AUDIO TX/AUDIO STREAM/PLAYBACK | L-TX / H-TX / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssTxControlAction](../40_DATA/60_AUDIO_TX_DATA.md#vsstxcontrolaction) | AUDIO STREAM / AudioStream_RequestControl / 내부 구간 진행 → AudioTx_Request | L-TX / H-TX / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssTxRequestState](../40_DATA/60_AUDIO_TX_DATA.md#vsstxrequeststate) | AUDIO TX / AudioTx_Request/AudioTx_Advance → AUDIO STREAM | L-TX / H-TX / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssCoverageValidity](../40_DATA/60_AUDIO_TX_DATA.md#vsscoveragevalidity) | 해당 관측 producer / HAL/AUDIO TX/실제 검증 주체 → DRIVER/AUDIO STREAM/PLAYBACK/HEALTH | L-TX / H-TX / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssTxOutputFactKind](../40_DATA/60_AUDIO_TX_DATA.md#vsstxoutputfactkind) | AUDIO TX의 범위 판정 / AudioTx_Advance → AUDIO STREAM | L-TX / H-TX / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssDeviceKind](../40_DATA/60_AUDIO_TX_DATA.md#vssdevicekind) | AUDIO CONTROL의 장치 의미 / 현재 장치 요구/구성 → AUDIO CONTROL/HAL/AUDIO STREAM | L-TX / H-CONTROL / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssDeviceAction](../40_DATA/60_AUDIO_TX_DATA.md#vssdeviceaction) | AUDIO STREAM 요구 / AUDIO CONTROL 적용 / AUDIO STREAM → AudioControl_Service | L-TX / H-CONTROL / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssDeviceReadyState](../40_DATA/60_AUDIO_TX_DATA.md#vssdevicereadystate) | AUDIO CONTROL / AudioControl_Service → AUDIO STREAM | L-TX / H-CONTROL / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssFaultKind](../40_DATA/70_DIAGNOSTIC_DATA.md#vssfaultkind) | HEALTH / Health_Evaluate → FLOW/보고 소비자 | L-HEALTH / H-HEALTH / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssFaultRecordState](../40_DATA/70_DIAGNOSTIC_DATA.md#vssfaultrecordstate) | HEALTH / Health_Evaluate → FLOW/Health_BuildStatus | L-HEALTH / H-HEALTH / Private; 값·숫자·폭 추가 없음 |
| [VssRecoveryEffect](../40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoveryeffect) | 실제 Backend 수행/검증 주체 / AUDIO STREAM/AUDIO CONTROL 실제 처리 → FLOW/HEALTH | L-HEALTH / H-HEALTH / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssHealthChange](../40_DATA/70_DIAGNOSTIC_DATA.md#vsshealthchange) | HEALTH / Health_Evaluate → FLOW | L-HEALTH / H-HEALTH / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssStatusBuildResult](../40_DATA/70_DIAGNOSTIC_DATA.md#vssstatusbuildresult) | HEALTH 보고 책임 / Health_BuildStatus → FLOW | L-HEALTH / H-HEALTH / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssReportedPlaybackState](../40_DATA/70_DIAGNOSTIC_DATA.md#vssreportedplaybackstate) | HEALTH 파생 보고 / Health_BuildStatus → FLOW → INPUT → Node Communication | L-HEALTH / H-HEALTH / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |
| [VssServiceLevel](../40_DATA/70_DIAGNOSTIC_DATA.md#vssservicelevel) | HEALTH 파생 보고 / Health_BuildStatus → 외부 보고 소비자 | L-HEALTH / H-HEALTH / Public/Internal 필요한 경계만; 값·숫자·폭 추가 없음 |

| R3 의미 alias 그룹 | 생성·의미 책임 / 소비와 선언 위치 | 보호·미정 |
| --- | --- | --- |
| [VssTimeDomainType](../40_DATA/10_INPUT_DATA.md#vsstimedomaintype), [VssEpochType](../40_DATA/10_INPUT_DATA.md#vssepochtype) | Runtime domain/epoch → 모든 시간 소비자; H-TIME의 독립 중립 leaf 선언 | 불변 원문맥 scalar, 0/새 now/최신 key로 없음 합성 금지. 실제 C primitive/encoding/wrap·complete typedef 위치 TBD-R6-01/02 |
| [VssOriginType](../40_DATA/10_INPUT_DATA.md#vssorigintype), [VssSourceKeyType](../40_DATA/10_INPUT_DATA.md#vsssourcekeytype), [VssGenerationType](../40_DATA/10_INPUT_DATA.md#vssgenerationtype), [VssOrderingType](../40_DATA/10_INPUT_DATA.md#vssorderingtype) | INPUT의 원출처/generation/ordering → STORE·판단·보고; 원본 갱신 금지; H-INPUT의 독립 중립 leaf 선언 | 불변 원문맥 scalar, 0/새 now/최신 key로 없음 합성 금지. 실제 C primitive/encoding/wrap·complete typedef 위치 TBD-R6-01/02 |
| [VssOccurrenceKeyType](../40_DATA/20_STORE_DATA.md#vssoccurrencekeytype), [VssStatefulKeyType](../40_DATA/20_STORE_DATA.md#vssstatefulkeytype), [VssCandidateKeyType](../40_DATA/30_SELECTION_DATA.md#vsscandidatekeytype) | STORE 발생/Stateful/후보 key → POLICY/PB/보고; 서로 같은 identity 아님; H-STORE의 독립 중립 leaf 선언 | 불변 원문맥 scalar, 0/새 now/최신 key로 없음 합성 금지. 실제 C primitive/encoding/wrap·complete typedef 위치 TBD-R6-01/02 |
| [VssRevisionType](../40_DATA/10_INPUT_DATA.md#vssrevisiontype), [VssOwnerKeyType](../40_DATA/30_SELECTION_DATA.md#vssownerkeytype), [VssFactWatermarkType](../40_DATA/30_SELECTION_DATA.md#vssfactwatermarktype) | revision은 해당 writer, owner/watermark는 원 반영 주체 → FLOW/판단/보고; H-READ의 독립 중립 leaf 선언 | 불변 원문맥 scalar, 0/새 now/최신 key로 없음 합성 금지. 실제 C primitive/encoding/wrap·complete typedef 위치 TBD-R6-01/02 |
| [VssUseLimitType](../40_DATA/30_SELECTION_DATA.md#vssuselimittype), [VssPlanKeyType](../40_DATA/30_SELECTION_DATA.md#vssplankeytype), [VssRepeatRuleType](../40_DATA/30_SELECTION_DATA.md#vssrepeatruletype), [VssPlanPositionType](../40_DATA/30_SELECTION_DATA.md#vssplanpositiontype) | POLICY 전체 계획/위치·제한/반복 → PB/STREAM; 새 정책값 없음; H-POLICY의 독립 중립 leaf 선언 | 불변 원문맥 scalar, 0/새 now/최신 key로 없음 합성 금지. 실제 C primitive/encoding/wrap·complete typedef 위치 TBD-R6-01/02 |
| [VssSessionKeyType](../40_DATA/40_PLAYBACK_DATA.md#vsssessionkeytype), [VssAttemptKeyType](../40_DATA/40_PLAYBACK_DATA.md#vssattemptkeytype), [VssFactKeyType](../40_DATA/40_PLAYBACK_DATA.md#vssfactkeytype) | PLAYBACK Session/Attempt/fact → STORE/STREAM/TX/HAL 원귀속; H-PB의 독립 중립 leaf 선언 | 불변 원문맥 scalar, 0/새 now/최신 key로 없음 합성 금지. 실제 C primitive/encoding/wrap·complete typedef 위치 TBD-R6-01/02 |
| [VssAssetKeyType](../40_DATA/50_AUDIO_STREAM_DATA.md#vssassetkeytype), [VssImageKeyType](../40_DATA/50_AUDIO_STREAM_DATA.md#vssimagekeytype), [VssAssetMetadataType](../40_DATA/50_AUDIO_STREAM_DATA.md#vssassetmetadatatype), [VssAssetSpanKeyType](../40_DATA/50_AUDIO_STREAM_DATA.md#vssassetspankeytype) | 기존 image/Asset table 및 ASSET → 정책/STREAM; 의미 음원 key와 주소 구별; H-ASSET의 독립 중립 leaf 선언 | 불변 원문맥 scalar, 0/새 now/최신 key로 없음 합성 금지. 실제 C primitive/encoding/wrap·complete typedef 위치 TBD-R6-01/02 |
| [VssPcmFormatType](../40_DATA/50_AUDIO_STREAM_DATA.md#vsspcmformattype), [VssPcmCycleType](../40_DATA/50_AUDIO_STREAM_DATA.md#vsspcmcycletype) | STREAM 검증 format/PCM 회차 → TX/HAL·반환 대조; H-STREAM의 독립 중립 leaf 선언 | 불변 원문맥 scalar, 0/새 now/최신 key로 없음 합성 금지. 실제 C primitive/encoding/wrap·complete typedef 위치 TBD-R6-01/02 |
| [VssOperationKeyType](../40_DATA/60_AUDIO_TX_DATA.md#vssoperationkeytype) | 각 실제 요청 owner의 operation → 원 소비자/HAL·복구 인과 연결; H-TX의 독립 중립 leaf 선언 | 불변 원문맥 scalar, 0/새 now/최신 key로 없음 합성 금지. 실제 C primitive/encoding/wrap·complete typedef 위치 TBD-R6-01/02 |
| [VssObservationBoundaryType](../40_DATA/60_AUDIO_TX_DATA.md#vssobservationboundarytype), [VssBindingKeyType](../40_DATA/60_AUDIO_TX_DATA.md#vssbindingkeytype), [VssRegistrationKeyType](../40_DATA/60_AUDIO_TX_DATA.md#vssregistrationkeytype) | 실제 관측/HAL의 boundary/binding/등록 key → TX/CONTROL·상위 보호 근거; H-HAL의 독립 중립 leaf 선언 | 불변 원문맥 scalar, 0/새 now/최신 key로 없음 합성 금지. 실제 C primitive/encoding/wrap·complete typedef 위치 TBD-R6-01/02 |
| [VssConfigurationKeyType](../40_DATA/60_AUDIO_TX_DATA.md#vssconfigurationkeytype) | 해당 구성 owner/실제 구성 근거 → CONTROL/Backend/HEALTH; H-CONTROL의 독립 중립 leaf 선언 | 불변 원문맥 scalar, 0/새 now/최신 key로 없음 합성 금지. 실제 C primitive/encoding/wrap·complete typedef 위치 TBD-R6-01/02 |
| [VssDiagnosticStageType](../40_DATA/70_DIAGNOSTIC_DATA.md#vssdiagnosticstagetype), [VssCauseType](../40_DATA/70_DIAGNOSTIC_DATA.md#vsscausetype), [VssTargetKeyType](../40_DATA/70_DIAGNOSTIC_DATA.md#vsstargetkeytype), [VssFaultInstanceKeyType](../40_DATA/70_DIAGNOSTIC_DATA.md#vssfaultinstancekeytype), [VssRestrictionType](../40_DATA/70_DIAGNOSTIC_DATA.md#vssrestrictiontype), [VssRecoveryKeyType](../40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoverykeytype), [VssRecoveryActionType](../40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoveryactiontype), [VssAvailabilityValueType](../40_DATA/70_DIAGNOSTIC_DATA.md#vssavailabilityvaluetype) | stage/cause는 발견 producer, target은 원대상; Fault/허용/제한/보고는 HEALTH → 실행/검증/보고 소비자; H-HEALTH의 독립 중립 leaf 선언 | 불변 원문맥 scalar, 0/새 now/최신 key로 없음 합성 금지. 실제 C primitive/encoding/wrap·complete typedef 위치 TBD-R6-01/02 |

Semantic*와 PcmSamplesRef / CompressedBytesRef / RawFactValue는 [R3 표기](../40_DATA/00_DATA_OVERVIEW.md#pseudo-conventions)를 따른다. 실제 primitive/vendor payload 타입을 이 문서에서 만들지 않는다. VssAssetKeyType·VssConfigurationKeyType·VssOperationKeyType 같은 교차 key의 생성 책임은 R3 해당 producer이며 Header label 하나의 owner에게 일괄 이전되지 않는다. 진단 발견 사실도 HEALTH가 처음 만든 것으로 바꾸지 않는다.

<a id="include-direction"></a>
## 4. include 방향 — 선언 의존과 실행 호출을 분리

아래 adjacency는 **후보 선언 의존**이며 새 파일 목록이 아니다. L은 owner별 최소 scalar/enum 선언, D는 이미 존재하는 R3 typed 자료, A는 Core 경계 선언이다. L들을 하나의 공통 God Header에 모으지 않는다. 각 owner의 기존 Header/중립 선언 위치를 먼저 확인하고, 여러 role의 물리 co-hosting은 아래 DAG를 실제로 보존하는 경우만 가능하다. 논리 DAG의 무순환 검사는 물리 Header의 transitive closure 확인을 대체하지 않는다.

| role | 허용하는 선언 의존 대상 |
| --- | --- |
| L-TIME | 없음 — 기존 중립 scalar/enum/primitive 선언만 |
| L-INPUT | 없음 — 기존 중립 scalar/enum/primitive 선언만 |
| L-STORE | 없음 — 기존 중립 scalar/enum/primitive 선언만 |
| L-POLICY | 없음 — 기존 중립 scalar/enum/primitive 선언만 |
| L-PB | 없음 — 기존 중립 scalar/enum/primitive 선언만 |
| L-ASSET | 없음 — 기존 중립 scalar/enum/primitive 선언만 |
| L-PCM | 없음 — 기존 중립 scalar/enum/primitive 선언만 |
| L-TX | 없음 — 기존 중립 scalar/enum/primitive 선언만 |
| L-HEALTH | 없음 — 기존 중립 scalar/enum/primitive 선언만 |
| L-READ | 없음 — 기존 중립 scalar/enum/primitive 선언만 |
| L-FLOW | 없음 — 기존 중립 scalar/enum/primitive 선언만 |
| D-TIME | L-TIME |
| D-READ | D-TIME, L-READ |
| D-INPUT | D-TIME, L-INPUT, L-STORE |
| D-PB | D-TIME, D-READ, L-PB, L-STORE, L-POLICY |
| D-STORE | D-INPUT, D-PB, D-READ, L-STORE, L-POLICY |
| D-PLAN | L-POLICY, L-ASSET, L-INPUT |
| D-PCM | L-PB, L-PCM |
| D-TX | D-TIME, D-PCM, L-TX, D-INPUT |
| D-DIAG | D-TIME, L-READ, L-TX, L-HEALTH |
| D-RECOVERY | D-TIME, L-READ, L-TX, L-HEALTH |
| D-ASSET | L-ASSET, L-PB |
| D-STREAM | L-PB, L-PCM, L-POLICY, L-TX, D-TX, D-PLAN, D-INPUT, D-RECOVERY |
| D-CONTROL | L-TX, L-READ, D-RECOVERY |
| D-HAL | D-TX, L-TX, D-TIME |
| D-HEALTH | D-DIAG, D-RECOVERY, D-READ, L-HEALTH |
| D-STATUS | D-READ, L-HEALTH |
| A-FLOW | L-FLOW, D-TIME |
| A-INPUT | D-INPUT, D-TIME, D-DIAG |
| A-STORE | D-INPUT, D-PB, D-STORE |
| A-POLICY | D-STORE, D-PLAN, D-PB, D-HEALTH, D-ASSET, D-CONTROL, D-STREAM |
| A-PB | D-PLAN, D-PB, D-STREAM, D-HEALTH, D-ASSET, D-STORE, D-CONTROL, D-READ, D-DIAG |
| A-STREAM | D-STREAM, D-RECOVERY, D-READ, D-STORE, D-PB, D-CONTROL, D-HEALTH, D-DIAG |
| A-TX | D-TX, D-TIME, D-DIAG |
| A-ASSET | D-ASSET, D-DIAG |
| A-CONTROL | D-CONTROL, D-RECOVERY, D-DIAG, D-TIME |
| A-HAL | D-HAL |
| A-HEALTH | D-HEALTH, D-STATUS, D-INPUT, D-STORE, D-PB, D-CONTROL, D-STREAM, D-ASSET |

L-INPUT/STORE/POLICY/PB/ASSET/PCM/TX/HEALTH/READ/FLOW의 타입은 §3의 owner별 선언이다. L-PB는 Session/Attempt/fact key와 기존 playback scalar, L-PCM은 A/B·회차/format·STREAM scalar, L-TX는 operation/config/binding/등록/scope·TX/CONTROL/HAL scalar를 뜻한다. L-TIME은 time/epoch/continuity다. 이 분류는 타입명/소유권이나 물리 파일 수를 늘리지 않는다. Private Context의 추가 내부 참조는 그 owner 구현에서 완전한 정의를 읽되 public Header로 끌어올리지 않는다.

- API Header끼리 실행 호출 때문에 상호 include하지 않는다. FLOW 구현→INPUT/STORE/POLICY/PB/STREAM/HEALTH, INPUT 구현→STORE, PB 구현→STREAM/STORE, STREAM 구현→ASSET/TX/CONTROL, TX/CONTROL 구현→HAL, HAL→최소 Runtime 도구의 기존 호출 방향을 유지한다. POLICY의 실행 역호출, STREAM→PB/HEALTH writer 호출, HAL의 의미 Core 호출은 없다.
- `VssInputApplication.storeAdmission`에는 L-STORE의 완전한 enum 선언만 필요하며 STORE API 전체를 INPUT Header에 넣지 않는다. STORE의 입력 타입 의존 D-INPUT과 cycle을 만들지 않는다.
- PCM key/handoff의 의미 owner는 STREAM이지만 **TX가 소비하는 중립 인계 선언**은 H-TX 재사용 경계에 둘 후보를 우선한다. AudioDmaTransport.h가 AudioPlayer.h의 전체 provider/API를 include하게 하지 않는다. AudioPlayer.h는 필요한 중립 PCM/TX 선언을 소비할 수 있고 transport `.c`→Player 호출은 Header cycle이 아니다. 물리 버퍼 allocation이 transport에 있어도 PCM 내용·usage writer는 STREAM이다.
- Plan/Cue의 Asset key는 중립 scalar만 소비하고 Asset 전체 API를 include하지 않는다. Session/Attempt/Occurrence·time/revision/permission/target key를 하위에서 쓰려면 기존 중립 선언 위치를 확인한다. **VssAudioSession.h/HEALTH API 전체를 TX/HAL에 include해서 우회하지 않는다.** 실제 중립 Header 위치는 TBD-R6-01이며 file-per-key/새 God Header를 선생성하지 않는다.
- 포인터만 사용하는 named struct는 forward declaration 후보가 될 수 있다. R3의 anonymous `typedef struct { ... } T`를 그대로 불투명 forward declaration할 수 있다고 기입하지 않는다. 실제 named-tag 표현을 쓰려면 B2-R에서 동일 필드/owner/type identity의 정의와 대조해야 한다. 값으로 포함하는 struct·enum·alias에는 완전한 정의가 필요하다. 특히 InputMeta/FactTime/OwnerStamp/PcmCycleKey/ObservationCoverage/Status의 basis를 포인터로 바꿔 cycle을 숨기지 않는다.
- SDK/vendor의 Sai_Ip/Dma_Ip/Lpi2c/TCD/채널/메모리 layout type은 HAL/BSP·Driver 구현 내부에 둔다. 상위 후보 prototype에는 중립 R3 자료만 있다. 제공 Header의 Std_Types/AudioPcmProvider transitive 내용, 실제 neutral typedef와 co-hosting include closure는 TBD-R6-01/02/07이다. 확인 전 실제 Header compile PASS를 주장하지 않는다.

<a id="prototype-notation"></a>
## 5. 후보 prototype 표기와 반환·자료 전달

아래 fenced text는 **pseudo C 표기**다. 실제 `.h` 선언/overload/새 public symbol을 등록하지 않는다. 기존 logical Core 이름으로 읽으며 B2-R에서는 기존 symbol/private 처리에 최소 대응한다.

| 표기 | C 경계 후보 의미 / 유효성 |
| --- | --- |
| VALUE T | 이미 존재하는 작은 scalar의 동기 값 전달. 실제 폭/enum 숫자는 미정 |
| IN const T * / T[count] | 호출/평가 동안 안정된 read-only borrow. 범위는 `(const T *items, SemanticCount count)` 후보이며 count=0은 없음, count>0은 유효 저장·범위 필수. count를 type마다 독립 전달 |
| OPTIONAL IN const T * | R3/R5가 허용한 없음만 NULL로 표현. 없음에 fake identity/정상값을 채우지 않음 |
| ONE_OF T1 * / T2 * ... | 각 typed 포인터를 **별도 인자**로 전달하는 후보. 정확히 한 개 non-NULL. C overload/union/void payload 아님. 0개/복수는 효력 전 신규 거부·원 보호 유지. 각 lane 권한을 독립 검사 |
| OUT T[capacity -> written] | 각각 `(T *out, SemanticCount capacity, SemanticCount *written)` 후보. written은 이 호출에서 제공한 유효 원사실 수이며 성공 bool이 아님. 단일 optional 결과는 0/1. 전달 후 참조 보호 조건까지 충족된 자료만 유효 |
| void / count=0 | 통합 성공/완료 의미 없음. 후속 필요성은 기존 FLOW progress·owner 보호 문맥·관측/진단으로 유지 |

OUT은 값 필드의 caller 출력 공간 전달을 우선 검토하되 pointer 포함 자료의 **얕은 복사만으로** 보호 완료를 주장하지 않는다. 후속에 쓰는 plan/Meta/bytes/samples/transportFacts/diagnosis/fact/Status 목록·stamps는 독립 보호해야 한다. 물리 복사/고정 borrow/retained 참조의 선택은 TBD-R6-03. 강제 deep-copy 전체 상태나 owner 공동 mutable storage를 요구하지 않는다.

독립 사실의 capacity/written은 각각 다르며 overflow·보호 공간 부족은 기존 사실 삭제/완료 ack가 아니다. 아직 전달/반영하지 못한 사실은 producer가 보호하고 기존 Advance/후속 기회로 이어간다. 중요 사실을 보호 못 했다면 해당 coverage=LOST/진단을 관측 가능하게 남겨 충분성 판정을 차단한다. 확정 결과를 유효 출력 공간 없이 생성·소비한 것으로 보이지 않게 한다. 실제 bounded 저장·전달 완료/consumer 반영 보호·직렬화는 TBD-R6-03/04다.

R5 CALL은 logical 인자만 보이는 표기다. 아래 후보에서 펼친 관측/시간/OUT 범위는 그 CALL의 `최신 조건`, `typed 사실`, callee 기존 보호 문맥에 해당한다. 없는 근거를 만들어 추가 인자로 채우지 않으며, owner의 내부 resource/permission 재검사는 외부 bool 하나로 대체하지 않는다. 최초 호출과 후속 callback의 결과는 **같은 typed 종류여도 원사실·시점별로 독립 보호**한다.

<a id="core-map"></a>
## 6. 18 Core — R2 → R3 typed → R5 실제 CALL → R6 후보

각 행의 원 R2 링크에는 불변 provisional 전체 원형이 있다. 아래 R2 열은 그 원형을 축약해도 입력/반환 타입을 전부 표시한다. 후보 원형은 이어지는 18개 절에 있다. R2의 SPLIT/INLINE wrapper를 실제 C 타입으로 복원하지 않는다.

| Core / 기존 R2 provisional | R3 입력·반환의 선언 | R5 실제 CALL 위치 / entry | R6 후보·Header |
| --- | --- | --- | --- |
| [Flow_Process](../30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process) / `VssProcessingOpportunity, VssTimeEvidence → VssFlowProgress` | [VssFlowProgress](../40_DATA/10_INPUT_DATA.md#vssflowprogress), [VssProcessingOpportunity](../40_DATA/10_INPUT_DATA.md#vssprocessingopportunity), [VssTimeEvidence](../40_DATA/10_INPUT_DATA.md#vsstimeevidence) | [20 Flow entry](../60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#flow-process) / 기존 app 진입 | [Flow_Process 후보](#r6-flow-process); H-FLOW |
| [Input_Process](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process) / `VssInputEvidence, VssTimeEvidence → VssInputApplication` | [VssInputApplication](../40_DATA/10_INPUT_DATA.md#vssinputapplication), [VssInputEvidence](../40_DATA/10_INPUT_DATA.md#vssinputevidence), [VssTimeEvidence](../40_DATA/10_INPUT_DATA.md#vsstimeevidence), [VssDiagnosticEvidence](../40_DATA/70_DIAGNOSTIC_DATA.md#vssdiagnosticevidence) | [20_INPUT_TO_SELECTION:flow-process](../60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#flow-process) | [Input_Process 후보](#r6-input-process); H-INPUT |
| [Store_ApplyInput](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyinput) / `VssValidatedInput → VssStoreAdmission` | [VssStoreAdmission](../40_DATA/20_STORE_DATA.md#vssstoreadmission), [VssOneShotEvent](../40_DATA/10_INPUT_DATA.md#vssoneshotevent), [VssStateUpdate](../40_DATA/10_INPUT_DATA.md#vssstateupdate), [VssQualityChange](../40_DATA/10_INPUT_DATA.md#vssqualitychange) | [20_INPUT_TO_SELECTION:input-process](../60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#input-process) | [Store_ApplyInput 후보](#r6-store-applyinput); H-STORE |
| [Store_ApplyPlaybackFact](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact) / `VssPlaybackFact → VssFactApplication` | [VssFactApplication](../40_DATA/20_STORE_DATA.md#vssfactapplication), [VssPlaybackFact](../40_DATA/40_PLAYBACK_DATA.md#vssplaybackfact) | [30_PLAYBACK_FLOW:playback-advance](../60_PSEUDOCODE/30_PLAYBACK_FLOW.md#playback-advance); [50_STOP_LATE_UNCERTAIN_FLOW:fact-notification](../60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#fact-notification) | [Store_ApplyPlaybackFact 후보](#r6-store-applyplaybackfact); H-STORE |
| [Store_AdvanceDeadlines](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines) / `VssTimeEvidence, VssPlaybackObservations → VssStoreProgress` | [VssStoreProgress](../40_DATA/20_STORE_DATA.md#vssstoreprogress), [VssTimeEvidence](../40_DATA/10_INPUT_DATA.md#vsstimeevidence), [VssPlaybackObservation](../40_DATA/40_PLAYBACK_DATA.md#vssplaybackobservation) | [20_INPUT_TO_SELECTION:flow-process](../60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#flow-process) | [Store_AdvanceDeadlines 후보](#r6-store-advancedeadlines); H-STORE |
| [Select_Choose](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose) / `VssSelectionView → VssSelectionDecision` | [VssSelectionDecision](../40_DATA/30_SELECTION_DATA.md#vssselectiondecision), [VssCandidateObservation](../40_DATA/30_SELECTION_DATA.md#vsscandidateobservation), [VssPlaybackObservation](../40_DATA/40_PLAYBACK_DATA.md#vssplaybackobservation), [VssFaultState](../40_DATA/70_DIAGNOSTIC_DATA.md#vssfaultstate), [VssAssetDescriptor](../40_DATA/50_AUDIO_STREAM_DATA.md#vssassetdescriptor), [VssDeviceReadinessState](../40_DATA/60_AUDIO_TX_DATA.md#vssdevicereadinessstate), [VssBackendCleanupObservation](../40_DATA/50_AUDIO_STREAM_DATA.md#vssbackendcleanupobservation), [VssReadBasis](../40_DATA/30_SELECTION_DATA.md#vssreadbasis) | [20_INPUT_TO_SELECTION:flow-process](../60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#flow-process); [60_FAULT_RECOVERY_FLOW:recovery-flow](../60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#recovery-flow) | [Select_Choose 후보](#r6-select-choose); H-POLICY |
| [Playback_RequestTransition](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition) / `VssPlaybackIntent, VssExecutionConditions → VssRequestDisposition` | [VssRequestDisposition](../40_DATA/40_PLAYBACK_DATA.md#vssrequestdisposition), [VssSelectionDecision](../40_DATA/30_SELECTION_DATA.md#vssselectiondecision), [VssSessionKeyType](../40_DATA/40_PLAYBACK_DATA.md#vsssessionkeytype), [VssCauseType](../40_DATA/70_DIAGNOSTIC_DATA.md#vsscausetype), [VssCandidateObservation](../40_DATA/30_SELECTION_DATA.md#vsscandidateobservation), [VssFaultState](../40_DATA/70_DIAGNOSTIC_DATA.md#vssfaultstate), [VssAssetDescriptor](../40_DATA/50_AUDIO_STREAM_DATA.md#vssassetdescriptor), [VssDeviceReadinessState](../40_DATA/60_AUDIO_TX_DATA.md#vssdevicereadinessstate), [VssBackendCleanupObservation](../40_DATA/50_AUDIO_STREAM_DATA.md#vssbackendcleanupobservation), [VssReadBasis](../40_DATA/30_SELECTION_DATA.md#vssreadbasis) | [20_INPUT_TO_SELECTION:flow-process](../60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#flow-process); [60_FAULT_RECOVERY_FLOW:recovery-flow](../60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#recovery-flow) | [Playback_RequestTransition 후보](#r6-playback-requesttransition); H-PB |
| [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) / `VssAudioBackendResult, VssExecutionConditions, VssTimeEvidence → VssPlaybackProgress` | [VssAudioRequestResult](../40_DATA/50_AUDIO_STREAM_DATA.md#vssaudiorequestresult), [VssAudioOutputEvidence](../40_DATA/50_AUDIO_STREAM_DATA.md#vssaudiooutputevidence), [VssBackendCleanupObservation](../40_DATA/50_AUDIO_STREAM_DATA.md#vssbackendcleanupobservation), [VssCandidateObservation](../40_DATA/30_SELECTION_DATA.md#vsscandidateobservation), [VssFaultState](../40_DATA/70_DIAGNOSTIC_DATA.md#vssfaultstate), [VssAssetDescriptor](../40_DATA/50_AUDIO_STREAM_DATA.md#vssassetdescriptor), [VssDeviceReadinessState](../40_DATA/60_AUDIO_TX_DATA.md#vssdevicereadinessstate), [VssReadBasis](../40_DATA/30_SELECTION_DATA.md#vssreadbasis), [VssTimeEvidence](../40_DATA/10_INPUT_DATA.md#vsstimeevidence), [VssPlaybackObservation](../40_DATA/40_PLAYBACK_DATA.md#vssplaybackobservation), [VssPlaybackFact](../40_DATA/40_PLAYBACK_DATA.md#vssplaybackfact), [VssFactApplication](../40_DATA/20_STORE_DATA.md#vssfactapplication), [VssDiagnosticEvidence](../40_DATA/70_DIAGNOSTIC_DATA.md#vssdiagnosticevidence) | [20_INPUT_TO_SELECTION:flow-process](../60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#flow-process) | [Playback_Advance 후보](#r6-playback-advance); H-PB |
| [Asset_Read](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#asset-read) / `VssAssetAccess → VssAssetReadResult` | [VssAssetReadDisposition](../40_DATA/50_AUDIO_STREAM_DATA.md#vssassetreaddisposition), [VssAssetReadRequest](../40_DATA/50_AUDIO_STREAM_DATA.md#vssassetreadrequest), [VssAssetSpanKeyType](../40_DATA/50_AUDIO_STREAM_DATA.md#vssassetspankeytype), [VssAssetSpan](../40_DATA/50_AUDIO_STREAM_DATA.md#vssassetspan), [VssDiagnosticEvidence](../40_DATA/70_DIAGNOSTIC_DATA.md#vssdiagnosticevidence) | [40_AUDIO_PREPARE_TX_FLOW:audiostream-prepare](../60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#audiostream-prepare); [40_AUDIO_PREPARE_TX_FLOW:fill-buffer](../60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#fill-buffer) | [Asset_Read 후보](#r6-asset-read); H-ASSET |
| [AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) / `VssAudioPreparation → VssAudioBackendResult` | [VssRequestDisposition](../40_DATA/40_PLAYBACK_DATA.md#vssrequestdisposition), [VssAudioPreparation](../40_DATA/50_AUDIO_STREAM_DATA.md#vssaudiopreparation), [VssAudioRequestResult](../40_DATA/50_AUDIO_STREAM_DATA.md#vssaudiorequestresult), [VssDiagnosticEvidence](../40_DATA/70_DIAGNOSTIC_DATA.md#vssdiagnosticevidence) | [30_PLAYBACK_FLOW:playback-requesttransition](../60_PSEUDOCODE/30_PLAYBACK_FLOW.md#playback-requesttransition); [30_PLAYBACK_FLOW:playback-advance](../60_PSEUDOCODE/30_PLAYBACK_FLOW.md#playback-advance) | [AudioStream_Prepare 후보](#r6-audiostream-prepare); H-STREAM |
| [AudioStream_RequestControl](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol) / `VssAudioControlIntent, VssExecutionConditions → VssAudioBackendResult` | [VssRequestDisposition](../40_DATA/40_PLAYBACK_DATA.md#vssrequestdisposition), [VssPlaybackControlCommand](../40_DATA/50_AUDIO_STREAM_DATA.md#vssplaybackcontrolcommand), [VssRecoveryPermission](../40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoverypermission), [VssPlaybackObservation](../40_DATA/40_PLAYBACK_DATA.md#vssplaybackobservation), [VssCandidateObservation](../40_DATA/30_SELECTION_DATA.md#vsscandidateobservation), [VssFaultState](../40_DATA/70_DIAGNOSTIC_DATA.md#vssfaultstate), [VssDeviceReadinessState](../40_DATA/60_AUDIO_TX_DATA.md#vssdevicereadinessstate), [VssBackendCleanupObservation](../40_DATA/50_AUDIO_STREAM_DATA.md#vssbackendcleanupobservation), [VssReadBasis](../40_DATA/30_SELECTION_DATA.md#vssreadbasis), [VssAudioRequestResult](../40_DATA/50_AUDIO_STREAM_DATA.md#vssaudiorequestresult), [VssDiagnosticEvidence](../40_DATA/70_DIAGNOSTIC_DATA.md#vssdiagnosticevidence) | [30_PLAYBACK_FLOW:playback-requesttransition](../60_PSEUDOCODE/30_PLAYBACK_FLOW.md#playback-requesttransition); [30_PLAYBACK_FLOW:playback-advance](../60_PSEUDOCODE/30_PLAYBACK_FLOW.md#playback-advance); [60_FAULT_RECOVERY_FLOW:recovery-flow](../60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#recovery-flow) | [AudioStream_RequestControl 후보](#r6-audiostream-requestcontrol); H-STREAM |
| [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) / `VssTimeEvidence → VssAudioBackendResult` | [VssTimeEvidence](../40_DATA/10_INPUT_DATA.md#vsstimeevidence), [VssAudioRequestResult](../40_DATA/50_AUDIO_STREAM_DATA.md#vssaudiorequestresult), [VssAudioOutputEvidence](../40_DATA/50_AUDIO_STREAM_DATA.md#vssaudiooutputevidence), [VssBackendCleanupObservation](../40_DATA/50_AUDIO_STREAM_DATA.md#vssbackendcleanupobservation), [VssDeviceReadinessState](../40_DATA/60_AUDIO_TX_DATA.md#vssdevicereadinessstate), [VssDiagnosticEvidence](../40_DATA/70_DIAGNOSTIC_DATA.md#vssdiagnosticevidence), [VssRecoveryPerformedEvidence](../40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoveryperformedevidence), [VssRecoveryVerificationEvidence](../40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoveryverificationevidence) | [10_STARTUP_FLOW:flow-startup](../60_PSEUDOCODE/10_STARTUP_FLOW.md#flow-startup); [20_INPUT_TO_SELECTION:flow-process](../60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#flow-process) | [AudioStream_Advance 후보](#r6-audiostream-advance); H-STREAM |
| [AudioTx_Request](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-request) / `VssPcmTransferIntent → VssTxProgress` | [VssTxRequestState](../40_DATA/60_AUDIO_TX_DATA.md#vsstxrequeststate), [VssPcmHandoffCommand](../40_DATA/60_AUDIO_TX_DATA.md#vsspcmhandoffcommand), [VssTxControlCommand](../40_DATA/60_AUDIO_TX_DATA.md#vsstxcontrolcommand), [VssOperationKeyType](../40_DATA/60_AUDIO_TX_DATA.md#vssoperationkeytype), [VssTimeEvidence](../40_DATA/10_INPUT_DATA.md#vsstimeevidence), [VssTxRequestResult](../40_DATA/60_AUDIO_TX_DATA.md#vsstxrequestresult), [VssPcmReturnEvidence](../40_DATA/60_AUDIO_TX_DATA.md#vsspcmreturnevidence), [VssDiagnosticEvidence](../40_DATA/70_DIAGNOSTIC_DATA.md#vssdiagnosticevidence) | [40_AUDIO_PREPARE_TX_FLOW:fill-buffer](../60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#fill-buffer); [40_AUDIO_PREPARE_TX_FLOW:audiostream-requestcontrol](../60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#audiostream-requestcontrol); [50_STOP_LATE_UNCERTAIN_FLOW:stream-stop](../60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#stream-stop) | [AudioTx_Request 후보](#r6-audiotx-request); H-TX |
| [AudioTx_Advance](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance) / `VssTimeEvidence → VssTxProgress` | [VssTimeEvidence](../40_DATA/10_INPUT_DATA.md#vsstimeevidence), [VssTxRequestResult](../40_DATA/60_AUDIO_TX_DATA.md#vsstxrequestresult), [VssTxOutputEvidence](../40_DATA/60_AUDIO_TX_DATA.md#vsstxoutputevidence), [VssPcmConsumptionEvidence](../40_DATA/60_AUDIO_TX_DATA.md#vsspcmconsumptionevidence), [VssPcmReturnEvidence](../40_DATA/60_AUDIO_TX_DATA.md#vsspcmreturnevidence), [VssDiagnosticEvidence](../40_DATA/70_DIAGNOSTIC_DATA.md#vssdiagnosticevidence) | [40_AUDIO_PREPARE_TX_FLOW:audiostream-advance](../60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#audiostream-advance) | [AudioTx_Advance 후보](#r6-audiotx-advance); H-TX |
| [AudioControl_Service](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service) / `VssDeviceControlIntent, VssTimeEvidence → VssDeviceReadiness` | [VssDeviceControlIntent](../40_DATA/60_AUDIO_TX_DATA.md#vssdevicecontrolintent), [VssTimeEvidence](../40_DATA/10_INPUT_DATA.md#vsstimeevidence), [VssDeviceReadinessState](../40_DATA/60_AUDIO_TX_DATA.md#vssdevicereadinessstate), [VssDeviceControlResult](../40_DATA/60_AUDIO_TX_DATA.md#vssdevicecontrolresult), [VssDiagnosticEvidence](../40_DATA/70_DIAGNOSTIC_DATA.md#vssdiagnosticevidence), [VssRecoveryPerformedEvidence](../40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoveryperformedevidence), [VssRecoveryVerificationEvidence](../40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoveryverificationevidence) | [40_AUDIO_PREPARE_TX_FLOW:audiostream-prepare](../60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#audiostream-prepare); [40_AUDIO_PREPARE_TX_FLOW:audiostream-advance](../60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#audiostream-advance); [60_FAULT_RECOVERY_FLOW:stream-recovery](../60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#stream-recovery) | [AudioControl_Service 후보](#r6-audiocontrol-service); H-CONTROL |
| [AudioHAL_Callback](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiohal-callback) / `VssHalRegistration, VssRawObservation → void` | [VssHalTxRegistration](../40_DATA/60_AUDIO_TX_DATA.md#vsshaltxregistration), [VssHalDeviceRegistration](../40_DATA/60_AUDIO_TX_DATA.md#vsshaldeviceregistration), [VssRawObservation](../40_DATA/60_AUDIO_TX_DATA.md#vssrawobservation) | [40 HAL entry](../60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#audiohal-callback) / vendor·IRQ entry; CALL 아님 | [AudioHAL_Callback 후보](#r6-audiohal-callback); H-HAL |
| [Health_Evaluate](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate) / `VssDiagnosticObservations → VssHealthAssessment` | [VssHealthChange](../40_DATA/70_DIAGNOSTIC_DATA.md#vsshealthchange), [VssDiagnosticEvidence](../40_DATA/70_DIAGNOSTIC_DATA.md#vssdiagnosticevidence), [VssRecoveryPerformedEvidence](../40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoveryperformedevidence), [VssRecoveryVerificationEvidence](../40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoveryverificationevidence), [VssPlaybackObservation](../40_DATA/40_PLAYBACK_DATA.md#vssplaybackobservation), [VssDeviceReadinessState](../40_DATA/60_AUDIO_TX_DATA.md#vssdevicereadinessstate), [VssBackendCleanupObservation](../40_DATA/50_AUDIO_STREAM_DATA.md#vssbackendcleanupobservation), [VssReadBasis](../40_DATA/30_SELECTION_DATA.md#vssreadbasis), [VssFaultState](../40_DATA/70_DIAGNOSTIC_DATA.md#vssfaultstate), [VssRecoveryPermission](../40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoverypermission) | [10_STARTUP_FLOW:flow-startup](../60_PSEUDOCODE/10_STARTUP_FLOW.md#flow-startup); [20_INPUT_TO_SELECTION:flow-process](../60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#flow-process); [60_FAULT_RECOVERY_FLOW:recovery-flow](../60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#recovery-flow) | [Health_Evaluate 후보](#r6-health-evaluate); H-HEALTH |
| [Health_BuildStatus](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus) / `VssStatusObservations → VssStatusSnapshot` | [VssStatusBuildResult](../40_DATA/70_DIAGNOSTIC_DATA.md#vssstatusbuildresult), [VssInputContext](../40_DATA/10_INPUT_DATA.md#vssinputcontext), [VssCandidateObservation](../40_DATA/30_SELECTION_DATA.md#vsscandidateobservation), [VssPlaybackObservation](../40_DATA/40_PLAYBACK_DATA.md#vssplaybackobservation), [VssAssetDescriptor](../40_DATA/50_AUDIO_STREAM_DATA.md#vssassetdescriptor), [VssDeviceReadinessState](../40_DATA/60_AUDIO_TX_DATA.md#vssdevicereadinessstate), [VssBackendCleanupObservation](../40_DATA/50_AUDIO_STREAM_DATA.md#vssbackendcleanupobservation), [VssReadBasis](../40_DATA/30_SELECTION_DATA.md#vssreadbasis), [VssStatusSnapshot](../40_DATA/70_DIAGNOSTIC_DATA.md#vssstatussnapshot) | [10_STARTUP_FLOW:flow-startup](../60_PSEUDOCODE/10_STARTUP_FLOW.md#flow-startup); [20_INPUT_TO_SELECTION:flow-process](../60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#flow-process) | [Health_BuildStatus 후보](#r6-health-buildstatus); H-HEALTH |

<a id="r6-flow-process"></a>
### Flow_Process — LOGICAL R6 CONTRACT

```text
VssFlowProgress Flow_Process(
    VALUE VssProcessingOpportunity opportunity,
    IN const VssTimeEvidence *time);
```

opportunity는 작은 enum 값, time은 동기 borrow. 도착 입력은 기존 INPUT 경계, 하위 사실은 STREAM 경계에서 읽는다. 반환은 FLOW의 조율/후속 필요성이며 모든 출력 성공이 아니다. public 후보는 기존 app 진입 연결 안에만 있다.

<a id="r6-input-process"></a>
### Input_Process — LOGICAL R6 CONTRACT

```text
VssInputApplication Input_Process(
    OPTIONAL IN const VssInputEvidence *input,
    IN const VssTimeEvidence *time,
    OUT VssDiagnosticEvidence diagnostics[capacity -> written]);
```

input=NULL은 RX 없는 품질/시간 처리다. 반환 세 필드의 값 의미를 유지하고 STORE 거부에도 contextApplied를 되돌리지 않는다. 전달 Meta의 optional validUntil 및 내부 참조까지 보호한다. diagnostics는 원 INPUT 발견 근거만.

<a id="r6-store-applyinput"></a>
### Store_ApplyInput — LOGICAL R6 CONTRACT

```text
VssStoreAdmission Store_ApplyInput(
    ONE_OF const VssOneShotEvent *event,
           const VssStateUpdate *state,
           const VssQualityChange *quality);
```

정확히 한 typed 포인터만 존재한다. null 3개/복수 입력은 신규 적용 거부이며 기존 ledger/Stateful/Rear와 INPUT 문맥을 보존한다. 품질 변화로 EVENT/CLEAR를 합성하지 않는다. 별도 ValidatedInput 타입/union/3개 public Core 없음.

<a id="r6-store-applyplaybackfact"></a>
### Store_ApplyPlaybackFact — LOGICAL R6 CONTRACT

```text
VssFactApplication Store_ApplyPlaybackFact(
    IN const VssPlaybackFact *fact);
```

fact 하나의 적용 결과만 반환한다. NOT_APPLIED이면 PB 보호 의무가 남고, 거짓 ALREADY_APPLIED를 ack로 만들지 않는다. STORE는 필요한 정규화 fact를 자체 ledger 수명으로 보호한다.

<a id="r6-store-advancedeadlines"></a>
### Store_AdvanceDeadlines — LOGICAL R6 CONTRACT

```text
VssStoreProgress Store_AdvanceDeadlines(
    IN const VssTimeEvidence *time,
    IN const VssPlaybackObservation *playback);
```

singular PlaybackObservation을 쓰며 notificationPending/stamp/startKnowledge를 먼저 확인한다. 부족한 pb/time은 RECONFIRM_REQUIRED, RX 없음도 진행 가능하다. pending/unknown/uncertain을 age만으로 Expired로 바꾸지 않는다.

<a id="r6-select-choose"></a>
### Select_Choose — LOGICAL R6 CONTRACT

```text
VssSelectionDecision Select_Choose(
    IN const VssCandidateObservation candidates[count],
    IN const VssPlaybackObservation *playback,
    IN const VssFaultState restrictions[count],
    IN const VssAssetDescriptor assets[count],
    IN const VssDeviceReadinessState devices[count],
    IN const VssBackendCleanupObservation cleanup[count],
    IN const VssReadBasis *basis);
```

SelectionView/ExecutionConditions는 위 기존 typed borrow들로 펼친다. 후보/plan/basis는 평가 수명만 유효하다. 부족/변화면 RECOLLECT/WAIT를 유지하며 장치 동작이나 다른 owner 원본 변경 없음. 관측이 여러 개면 각각의 count와 owner stamp를 확인한다.

<a id="r6-playback-requesttransition"></a>
### Playback_RequestTransition — LOGICAL R6 CONTRACT

```text
VssRequestDisposition Playback_RequestTransition(
    ONE_OF const VssSelectionDecision *decision,
           const VssSessionKeyType *terminationSession,
    OPTIONAL IN const VssCauseType *terminationReason,
    IN const VssCandidateObservation candidates[count],
    IN const VssFaultState restrictions[count],
    IN const VssAssetDescriptor assets[count],
    IN const VssDeviceReadinessState devices[count],
    IN const VssBackendCleanupObservation cleanup[count],
    IN const VssReadBasis *conditionsBasis);
```

decision과 terminationSession 중 정확히 하나다. 종료 경로의 terminationReason은 기존 VssCauseType으로 제한된 의미 사유를 전달하며, 종류별 action/숫자 확정은 아니다. 원 Session이 없거나 사유 연결이 부족하면 확인/거부하고 가짜 Session 없음. 현재 조건 상실은 decision을 받은 경우에도 PB 자체 최신 조건 검사로 종료할 수 있다. latest 조건과 PB 자기 권한/보호 자원은 별도 확인한다.

<a id="r6-playback-advance"></a>
### Playback_Advance — LOGICAL R6 CONTRACT

```text
void Playback_Advance(
    IN const VssAudioRequestResult requests[count],
    IN const VssAudioOutputEvidence outputs[count],
    IN const VssBackendCleanupObservation cleanup[count],
    IN const VssCandidateObservation candidates[count],
    IN const VssFaultState restrictions[count],
    IN const VssAssetDescriptor assets[count],
    IN const VssDeviceReadinessState devices[count],
    IN const VssReadBasis *conditionsBasis,
    IN const VssTimeEvidence *time,
    OUT VssPlaybackObservation observation[capacity -> written],
    OUT VssPlaybackFact facts[capacity -> written],
    OUT VssFactApplication applications[capacity -> written],
    OUT VssDiagnosticEvidence diagnostics[capacity -> written]);
```

요청/출력/정리 입력 count=0도 시간·정책 후속을 허용한다. facts와 applications는 같은 원 fact 순서·동일 written 수로 대응하고, 실제 적용을 시도한 fact에만 APPLIED/ALREADY_APPLIED/NOT_APPLIED를 붙인다. 미반영 fact는 독립 보존한다. 관측 없는 귀속 실패는 observation written=0/진단/후속이며 정상 Session을 만들지 않는다.

<a id="r6-asset-read"></a>
### Asset_Read — LOGICAL R6 CONTRACT

```text
VssAssetReadDisposition Asset_Read(
    ONE_OF const VssAssetReadRequest *request,
           const VssAssetSpanKeyType *referenceEnd,
    VALUE SemanticBool readerReferencesEnded,
    OUT VssAssetSpan span[capacity -> written],
    OUT VssDiagnosticEvidence diagnostics[capacity -> written]);
```

read 또는 referenceEnd 중 정확히 하나다. READ에서는 readerReferencesEnded를 사용하지 않는다. RELEASE의 true는 실제 STREAM reader/대기 참조 종료 근거여야 하고 ASSET이 자기 span 참조도 대조한다. false/미확인은 STILL_PROTECTED. SPAN_GRANTED에서만 span written=1, 실패/END/RELEASE는 0. bytes는 반환된 인자보다 오래 보호한다.

<a id="r6-audiostream-prepare"></a>
### AudioStream_Prepare — LOGICAL R6 CONTRACT

```text
VssRequestDisposition AudioStream_Prepare(
    IN const VssAudioPreparation *preparation,
    OUT VssAudioRequestResult requests[capacity -> written],
    OUT VssDiagnosticEvidence diagnostics[capacity -> written]);
```

VssRequestDisposition은 기존 R3가 STREAM 수용에도 허용한 작은 enum을 재사용한다. ADOPTED/KEPT/WAIT/REJECTED는 prepared/actual과 독립이다. 적법한 preparation 귀속이 있을 때만 요청 단계 결과를 생성한다. 수용 전/중/후 동시 단계 사실은 requests의 독립 원기록이며 PREPARE_FAILED 뒤에도 옛 자원 보호가 남는다.

<a id="r6-audiostream-requestcontrol"></a>
### AudioStream_RequestControl — LOGICAL R6 CONTRACT

```text
VssRequestDisposition AudioStream_RequestControl(
    ONE_OF const VssPlaybackControlCommand *playback,
           const VssRecoveryPermission *recovery,
    IN const VssPlaybackObservation *currentPlayback,
    IN const VssCandidateObservation candidates[count],
    IN const VssFaultState restrictions[count],
    IN const VssDeviceReadinessState devices[count],
    IN const VssBackendCleanupObservation cleanup[count],
    IN const VssReadBasis *conditionsBasis,
    OUT VssAudioRequestResult requests[capacity -> written],
    OUT VssDiagnosticEvidence diagnostics[capacity -> written]);
```

정확히 한 입력. 재생 결과는 실제 playback.session/attempt에만 생성하고 Recovery에는 requests written=0이다. 동기 disposition은 수행/검증 성공이 아니며 복구 후속은 Advance의 두 Evidence다. 최신 permission/안전 관측을 부수효력 직전에 재검사한다. 상세는 typed-stream 절.

<a id="r6-audiostream-advance"></a>
### AudioStream_Advance — LOGICAL R6 CONTRACT

```text
void AudioStream_Advance(
    IN const VssTimeEvidence *time,
    OUT VssAudioRequestResult requests[capacity -> written],
    OUT VssAudioOutputEvidence outputs[capacity -> written],
    OUT VssBackendCleanupObservation cleanup[capacity -> written],
    OUT VssDeviceReadinessState devices[capacity -> written],
    OUT VssDiagnosticEvidence diagnostics[capacity -> written],
    OUT VssRecoveryPerformedEvidence performed[capacity -> written],
    OUT VssRecoveryVerificationEvidence verified[capacity -> written]);
```

time만 외부 입력이며 prepare/control/recovery와 TX/CONTROL 보호 사실은 callee 내부 연결에서 받는다. Session 없는 초기 device 관측도 기존 타입으로 내보낸다. devices는 CONTROL이 반영한 값/const 관측으로서 STREAM의 readiness writer 승격이 아니다. 각 결과군이 동시에 존재하며 transportFacts의 실제 참조도 PB 소비까지 보호한다.

<a id="r6-audiotx-request"></a>
### AudioTx_Request — LOGICAL R6 CONTRACT

```text
VssTxRequestState AudioTx_Request(
    ONE_OF const VssPcmHandoffCommand *pcm,
           const VssTxControlCommand *control,
    OPTIONAL IN const VssOperationKeyType *continuingOperation,
    IN const VssOperationKeyType targetOperations[count],
    IN const VssTimeEvidence *time,
    OUT VssTxRequestResult requests[capacity -> written],
    OUT VssPcmReturnEvidence returned[capacity -> written],
    OUT VssDiagnosticEvidence diagnostics[capacity -> written]);
```

requestState scalar는 해당 새 요구/기존 작업의 요청 단계만이다. operation 생성 전 실제 새 무효력 거부는 REJECTED_WITH_NO_ACCESS와 requests/returned written=0으로 표현하고 진단을 남긴다. 생성 뒤에만 원 operation/attempt의 typed result를 만든다. 명확한 무접근 반환 Evidence도 실제 원 operation/key가 있어야 한다. 부분 효력은 EFFECT_UNCERTAIN/보호 유지. 상세는 typed-tx 및 stop-scope 절.

<a id="r6-audiotx-advance"></a>
### AudioTx_Advance — LOGICAL R6 CONTRACT

```text
void AudioTx_Advance(
    IN const VssTimeEvidence *time,
    OUT VssTxRequestResult requests[capacity -> written],
    OUT VssTxOutputEvidence outputs[capacity -> written],
    OUT VssPcmConsumptionEvidence consumed[capacity -> written],
    OUT VssPcmReturnEvidence returned[capacity -> written],
    OUT VssDiagnosticEvidence diagnostics[capacity -> written]);
```

HAL의 보호 raw와 원 operation은 내부 입력이며 현재 Attempt로 보충하지 않는다. 같은 기회의 request/output/consumption/return/diagnostic을 모두 독립 typed 범위로 보존한다. void는 완료/성공 의미가 없고 count=0도 보호 해제를 뜻하지 않는다.

<a id="r6-audiocontrol-service"></a>
### AudioControl_Service — LOGICAL R6 CONTRACT

```text
void AudioControl_Service(
    OPTIONAL IN const VssDeviceControlIntent *intent,
    IN const VssTimeEvidence *time,
    OUT VssDeviceReadinessState devices[capacity -> written],
    OUT VssDeviceControlResult results[capacity -> written],
    OUT VssDiagnosticEvidence diagnostics[capacity -> written],
    OUT VssRecoveryPerformedEvidence performed[capacity -> written],
    OUT VssRecoveryVerificationEvidence verified[capacity -> written]);
```

intent=NULL이면 이미 수용한 장치 제어만 진행한다. device/config/operation 없는 새 요구 실패에는 result를 만들지 않으며 기존 ready/제어는 보호한다. 현재 확인 가능한 장치만 devices에 내보내고 미확인 장치를 READY/FAILED로 채우지 않는다. startup/Recovery 모두 가짜 Session 없음.

<a id="r6-audiohal-callback"></a>
### AudioHAL_Callback — LOGICAL R6 CONTRACT

```text
void AudioHAL_Callback(
    OPTIONAL IN const VssHalTxRegistration *originalTx,
    OPTIONAL IN const VssHalDeviceRegistration *originalDevice,
    IN const VssRawObservation *observation);
```

originalTx/originalDevice는 실제 검증된 원 보호 기록이며 동시에 둘을 현재 key로 조합하지 않는다. 둘 다 없을 수 있고 원 raw 귀속 부족/손실을 그대로 포착한다. persistent raw의 key가 실제 확인되어도 개별 operation/회차는 별도 대응 검증이다. callback의 실제 vendor prototype/ISR adapter는 미정이며 이 중립 논리 원형을 generated ABI로 선언하지 않는다.

<a id="r6-health-evaluate"></a>
### Health_Evaluate — LOGICAL R6 CONTRACT

```text
VssHealthChange Health_Evaluate(
    IN const VssDiagnosticEvidence diagnostics[count],
    IN const VssRecoveryPerformedEvidence performed[count],
    IN const VssRecoveryVerificationEvidence verified[count],
    IN const VssPlaybackObservation *playback,
    IN const VssDeviceReadinessState devices[count],
    IN const VssBackendCleanupObservation cleanup[count],
    IN const VssReadBasis *basis,
    OUT VssFaultState faults[capacity -> written],
    OUT VssRecoveryPermission permissions[capacity -> written]);
```

수행/검증 각 count=0은 아직 결과 없음이다. 한쪽만 있으면 원 fact를 보호하고 현재 제한을 해제하지 않는다. 현재 Fault/permission 원본은 HEALTH만 변경한다. OUT faults는 보호된 읽기 값이며 diagnosis가 가리키는 원사실도 불변 보호한다. 제한 변화 enum은 HW 성공이 아니다.

<a id="r6-health-buildstatus"></a>
### Health_BuildStatus — LOGICAL R6 CONTRACT

```text
VssStatusBuildResult Health_BuildStatus(
    IN const VssInputContext inputs[count],
    IN const VssCandidateObservation candidates[count],
    IN const VssPlaybackObservation *playback,
    IN const VssAssetDescriptor assets[count],
    IN const VssDeviceReadinessState devices[count],
    IN const VssBackendCleanupObservation cleanup[count],
    OPTIONAL IN const SemanticBool *inputAcceptingBasis,
    OPTIONAL IN const SemanticBool *storeTrackingBasis,
    IN const VssReadBasis *basis,
    OUT VssStatusSnapshot snapshot[capacity -> written]);
```

HEALTH의 current/recent Fault 원본은 내부 read-only 보고 근거다. inputs/faults에 getter 숨은 적용 없음. inputAcceptingBasis는 INPUT 검증/출처·시간 준비, storeTrackingBasis는 STORE 저장/replay 추적 준비의 기존 INLINE scalar 관측이며 각각 stamp와 연결한다. NULL=미확인, false=확인된 제한으로 구분하고 fake true/새 manager 없음. CREATED에서만 snapshot written=1; REEVALUATE는 0. Snapshot의 목록/basis는 송신 소비 종료까지 별도 불변 보호한다.

후보의 작은 VssRequestDisposition을 STREAM 수용에 재사용하는 근거는 [R3 disposition](../40_DATA/40_PLAYBACK_DATA.md#vssrequestdisposition)의 producer/consumer다. 이는 새 return enum이 아니며 실제 준비/제어 결과와 합치지 않는다. VssCauseType은 기존 의미 사유 alias를 재사용한 후보로서 종료 사유의 실제 encoding/정책 대응은 TBD다. R5-C1 STOP 원 operation 연결에 필요한 `targetOperations` 및 동일 작업 후속의 `continuingOperation`은 기존 VssOperationKeyType scalar 전달이며 새로운 operation/segment 타입·ID 생성 API가 아니다.

<a id="typed-stream"></a>
## 7. AudioStream_RequestControl — 재생 제어와 Session 없는 복구

| typed lane | 인과·권한 / 동기 수용 | 후속·보호 / 금지 |
| --- | --- | --- |
| Playback START | PLAYBACK 원 Session/Attempt·prepared·현재 조건. 원 firstStartMeta는 STREAM의 보호된 preparation에서 전달 | VssAudioRequestResult는 원 귀속이 유효할 때만. 수용/제출/활성화와 actual 분리. 임시 playback 포인터 저장 금지 |
| Playback STOP | 원 Session/Attempt 연결 후 그 Session 전체 관련 미래 생산/제출을 하위보다 먼저 차단 | 아래 STOP scope 규칙. 하위 실패에도 productionAllowed=false 유지. currentAttempt 한 개로 전체 정리를 제한하지 않음 |
| Recovery | HEALTH→FLOW의 6필드 permission/faultInstance/faultRevision/target/configuration/action, 현재 허용·old retirement/실제 자원 안전 재확인 | Session/Attempt 불필요. requests written=0, 실제 수행/별도 검증은 각각 Advance Evidence. 수용·performed 성공만으로 HEALTH clear 없음 |

두 typed 포인터 방식은 기존 Core 수를 유지하고 C overload/큰 conditional Command/새 wrapper 없이 lane을 구별하는 **후보**다. 이 작은 배타 입력 인자와 lane별 사전 검사로 충분한지 B2-R 실제 기존 symbol/호출 관례와 대조한다. 이를 이유로 타입별 public API를 미리 늘리지 않는다. 실제 잠긴 owner/자원 조건은 해당 callee가 검사한다. 최신 허용이 철회되거나 구성이 바뀌면 새 수행을 보류하고 옛 결과는 원정리에만 격리한다.

<a id="typed-tx"></a>
## 8. AudioTx_Request — PCM 참조, 주소 없는 제어, 조기 반환

| typed lane / 조건 | 입력·보호 | 요청 return / 독립 후속 |
| --- | --- | --- |
| PCM 인계 | VssPcmHandoffCommand의 key/samples/firstValidFrame/validFrameCount/format/lastForSegment. >0인 유효 음향 범위, 안정 samples, 호출 전 HANDOFF_PENDING | 원 operation 생성 뒤 TxRequestResult. 인계 수용≠START 활성화. 소비/반환/출력은 별도 typed evidence |
| TX START/STOP | **기존 4필드** VssTxControlCommand: attempt/action/scope/firstStartMeta. PCM 포인터 필드 없음. operation.handoff=NULL | 실제 scope/원 operation을 대조. STOP/정상 후속 START에는 firstStartMeta=NULL. START 수용·장치 활성화·새 actual은 독립 |
| operation 생성 전 새 거부 | invalid typed 선택/범위, PCM 0, 신규 보호 공간·first gate 부족; 하위 요청/새 retain이 전혀 없음을 확인한 범위 | scalar REJECTED_WITH_NO_ACCESS, requests/returned written=0. fake operation/Attempt나 기존 작업 반환을 만들지 않음. 무효력 자체도 미확인인 옛 작업은 별도 보호 유지 |
| operation 생성 뒤 실패/부분 효력 | 원 operation/Attempt/target/PCM 회차·등록을 이미 보호 | EFFECT_UNCERTAIN/CLEANUP_PENDING + 원 typed result/진단. 무접근 증거 없이 pending/retain rollback 금지 |
| 명확한 무접근 반환 | 실제 원 operation/key에 대한 기존/미래 접근 종료 근거 | VssPcmReturnEvidence는 실제 귀속·noFutureAccess·basis가 있을 때만. 현재 CPU 쓰기 안전은 STREAM의 별도 확인 |

새 PCM/START는 `targetOperations` count=0이다. STOP은 §9의 확인된 원 대상 operation key 범위만 전달한다. `continuingOperation=NULL`은 아직 효력 전인 **새 원 요구**에만 해당하며 이미 제출된 작업을 새 요구로 재발행하는 표기가 아니다. 후속에는 TX가 반환한 실제 operation key를 전달해 동일 원 command/target/scope를 대조한다. 아직 key 전달 여부·수용 상태가 불명확하면 원 요구 보호/조회 후속을 유지하고 새 HW 동작을 발행하지 않는다. 동일 요청의 보호·결과 상관과 멱등 적용 수단은 TBD-R6-04이며, Attempt/action/scope tuple이 같다는 이유만으로 다른 SEGMENT/새 요청을 같은 operation으로 취급하지 않는다.

실제 PCM samples의 const 접근은 `PcmSamplesRef`의 중립 의미로 보존한다. 후속 구현의 쓰기 가능한 `uint16 *`를 상위에 노출해 CPU/HW 공유 쓰기 권한을 주지 않는다. 제공 Player의 pointer/Buffer ID만으로 원 cycle/접근 종료/현재 안전을 입증할 수 없다.

<a id="stop-scope"></a>
## 9. R5-C1 STOP — 원 대상 연결과 전체 종료

STREAM은 STOP 호출 **전에** 자신이 보호한 원 prepare/control/handoff와 공개 TX 결과·출력 관측을 연결해 각 대상의 원 Attempt, 기존 SEGMENT/ATTEMPT scope, 관련 실제 operation key 범위를 확보한다. targetOperations는 그 기존 연결을 타입 있는 scalar 범위로 전달하는 후보다. TX 원본 state/descriptor를 STREAM이 직접 수정하지 않는다.

scope enum 자체는 segment identity가 아니다. 같은 Attempt의 두 SEGMENT에 동일 enum 값이 있어도 각 원 operation/계획 구간·잔류와의 실제 대응이 필요하다. TX는 대상 key마다 자기 원 operation.attempt/scope와 요청 범위를 확인한다. 실제 구간/활성·대기 descriptor/FIFO/frame 잔류를 덮는 대응이 부족하면 **STOP binding 미확정**으로 보호·차단·진단/후속을 유지한다. 새 SegmentKey·Context·Queue·ledger를 만들거나 현재 key로 보충하지 않는다. 관련 descriptor/FIFO의 물리 scope 연결·제어 효과는 TBD-R6-05/07이다.

| 경계 사례 | 개별 정리 / 전체 판정 |
| --- | --- |
| 같은 Attempt의 SEGMENT X만 종료, Y의 FIFO/frame 잔류 | X의 target/근거만 적용. Y 보호·후속 정리. ATTEMPT/whole 종료 불가 |
| 원 Session의 Attempt A·B 중 A만 STOP 성공 | A의 결과를 B에 붙이지 않음. B의 미래 생산/제출도 Session STOP 의도로 먼저 차단하며 실제 종료 근거를 별도로 확보 |
| 한 SEGMENT에 복수 operation/대기 descriptor, 일부만 종료 | 범위의 모든 해당 operation/잔류가 실제 근거에 덮일 때까지 scope 종료 미확정. 목록에서 빠졌다는 이유로 종료 처리 금지 |
| STOP accepted이나 partial/timeout/결과 미확인 | 요청 단계만. 원 retain·생산 차단·후속 유지. safe return/NO_START/termination 합성 없음 |
| scope/target key/실제 대응 부족 또는 bounded 기회·보호 공간 한계 | 임의 STOP target·scope 확대 없이 원 차단·보호·진단 유지, Advance 후속. 부분 목록 성공≠전체 종료 |
| whole STOP 요청으로 새 control operation만 종료 | 그 control의 완료와 대상 옛 출력 operation들의 종료를 구별. 실제 target 전체를 덮는 fence 없으면 whole 판정 없음 |

Backend 출력 확정은 TX의 **각 원 범위** 과거 무출력·미래 차단/잔류 출력 종료와 STREAM의 그 범위 미래 PCM 차단을 결합한다. PLAYBACK retirement는 **전체 관련 Attempt/구간을 빠짐없이 덮는** 충분한 Backend 근거, 전체 start/cue/repeat 권한 차단, late 원귀속 보호 및 새 owner의 실제 자원 안전을 추가한다. [R5-C1 STOP/whole](../60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#playback-stop-late)과 [Async 충분조건](../50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#confirmed-outcomes)을 따른다. 정상 SEGMENT 끝·EOF·PCM 반환·mute는 whole final이 아니다.

<a id="first-actual-time"></a>
## 10. 실제 출력 시각 판정 — 세 갈래와 안전 정리

인터페이스는 VssInputMeta의 원발생/domain/epoch/continuity와 VssFactTime의 원 actual earliest/latest/domain/epoch, TxOutputEvidence의 원 operation/Attempt/scope/coverage와 실제 activation 인과를 보존한다. R6는 별도 판정 enum/bool/필드를 만들지 않는다. PLAYBACK 내부에서 다음 세 경로로 적용한다.

| 원 사실과 원래 권한의 근거 | R5-C1 판정 / 경계 동작 |
| --- | --- |
| actual 충분 + 비교 가능 + 가능한 구간 전체가 원발생 이후, `[잠정] age < 2초` 및 원래 권한 안 | 적법 확인. 최초 Started를 한 번 통지. 종료 진행 중이면 Started/보고만 적용, STOPPING/ABORTING 유지 |
| 충분한 실제 출력 + 비교/권한 근거로 구간 전체가 기한·권한 밖 (`=2초`도 미허용) | 확정 위반. 실제 출력/원 coverage·위반 진단·원 정리 보호. 적법 Started 없음 |
| 시간/activation/당시 권한 비교 불가·coverage 부족/손실·2초 경계 걸침 | 적법 Started와 확정 위반을 모두 보류. 실제 확인된 물리 fact/충분 coverage는 그대로 보호하며 부족한 비교·권한 범위를 별도 진단 |

actual의 충분성 또는 적법성 미확인은 전체 후속 권한 차단·원 STOP·안전 정리로 연결한다. 기존 적법 Started는 지우지 않고, 부족만으로 uncertain final/NO_START/출력 종료/retirement를 만들지 않는다. `occurredAt` 대신 `capturedAt`/처리 now를 쓰지 않는다. 이미 적법 Started된 같은 Session의 정상 후속 cue/반복에는 최초 age gate를 재적용하지 않는다. USE_LIMIT 관계/불명확 최종 조건은 미정이다. [R5-C1 원문](../60_PSEUDOCODE/30_PLAYBACK_FLOW.md#first-actual-gate) · [R4 시간 계약](../50_CONTRACTS/30_TIMING_FRESHNESS.md#first-start).

<a id="lifetime"></a>
## 11. C 호출·포인터와 실제 자료 수명

| 경계 / mutable writer | 호출 중 borrow/copy 후보 | async 보호·회수 기준 / 미확인 시 제한 |
| --- | --- | --- |
| INPUT evidence·Meta → STORE / INPUT·STORE 각각 | const 동기 borrow, 필요한 원 identity/source/generation/order/time는 owner 보호 | validUntil 같은 내부 pointer도 보호. INPUT 적용과 STORE 수용을 분리. 시간/순서 비교 부족이면 정상 사건/후보 합성 없음 |
| 후보/plan/basis → POLICY/PB / STORE·POLICY·각 stamp owner | POLICY는 평가기간 const borrow, PB 채택은 필요한 불변 계획·원 Meta를 Session 수명으로 보호 | mutable 후보/stamp 주소를 장기 Result에 넣지 않음. 수집 뒤 관련 변경이면 재수집. wrapper 전체 복제 금지 |
| Preparation/PlaybackControl/RecoveryPermission → STREAM / 각 의미 producer·STREAM 수용 | 인자 주소는 호출 수명만. 필요한 필드와 plan/Meta/permission을 자기 문맥으로 보호 | START 효력/즉시 callback 전에 보호. Recovery의 현재 revision/target/config/action 재검사. Session 없는 permission 유지 |
| ASSET Span / ASSET 제공·STREAM 실제 reader | span의 값/bytes 중립 참조. ASSET의 실제 압축 자료 보호 | decoder/read consumer·대기 작업의 실제 참조 종료까지. EOF/retirement/새 read로 기존 bytes 해제 금지 |
| PCM Handoff / STREAM 내용·usage, TX 장치 보호 | command 필드/key·samples를 호출 전 HANDOFF_PENDING으로 보호, TX는 필요한 불변 귀속/참조 확보 | **정확히 A/B 2개**. 같은 Attempt/A 또는 B/cycle의 충분 반환과 현재 쓰기 안전까지 내용 변경 금지. 인자 포인터 종료≠samples 종료 |
| TX Control / STREAM 의도, TX operation | 주소 없는 4필드 및 별도 원 target key 범위 보존 | 모든 원 작업/잔류의 효과 확인 전 보호. STOP 실패/반환에 producer 권한 rollback 없음 |
| HAL TX/device 기록 / HAL Boundary | 효력 전 불변 원 operation/Attempt/PCM 또는 device/config/binding 보호 | HW·대기·처리 중 SW·late 대응 참조가 모두 끝난 뒤만 기록 회수. IRQ disable/PCM 반환/Session retirement만으로 폐기 금지 |
| HAL raw → TX/CONTROL / HAL 포착 | callback/ISR의 const raw를 짧게 포착·보호, 필요한 원등록/당시 binding 참조 보존 | 즉시·늦음·순서 역전·overflow도 원귀속. 알림은 사실 아님. 부족/손실을 observable하게 남기고 confirmed 생성 차단 |
| 요청/출력/소비/반환/진단 → 각 consumer / 원 producer | 각 typed 사실이 함께 존재. OUT은 유효 count만 전달, 단일 마지막 슬롯 없음 | 소비자 반영/독립 보호 추적 완료까지. partial/late는 원 cleanup/diagnostic. new A cycle/current Attempt로 해제·재귀속 없음 |
| PB Fact → STORE / PB 판정·STORE 적용 | 원 fact 값/근거 보존, APPLIED/ALREADY_APPLIED만 그 통지 의무 완료 | ledger/replay는 STORE 수명, raw HAL 전체는 별도 수명. STARTED/final 동시에 보호; NOT_APPLIED는 pending 의무 |
| device readiness / CONTROL | 구성별 적용 완료 const 값/관측. 옛 control 결과는 원 config/operation | startup UNASSESSED 정상. 옛 성공으로 새 READY 생성 없음. 실제 control 참조 종료는 readiness 무효화와 별개 |
| Recovery 수행·별도 검증 → HEALTH / 실제 Backend | 각 permission/target/configuration/operation·발생/검증 시각을 불변 보호 | 수행/검증 pair와 현재 instance/revision/config를 대조한 HEALTH만 해당 제한 해제. 미확인·옛 결과는 원 제한/추적 유지 |
| Status Snapshot → FLOW→INPUT→통신 / HEALTH 파생 | CREATED에서만 snapshot/불변 Fault 목록·ReadBasis/stamps 보호 | 복사 완료 또는 실제 송신 consumer 참조 종료까지. mutable 원본 pointer 저장 금지. REEVALUATE에서는 새 정상 Snapshot 없음 |

source consumed ≠ 옛 회차 safe return ≠ **현재 CPU 쓰기 안전** ≠ output termination ≠ whole retirement다. 반환 Evidence가 옛 A/회차1이면 새 A/회차2/B를 열지 않는다. 옛 내용 접근 종료라도 차기 DMA 방문 전 실제 bounded 쓰기를 끝낼 수 있는지 현재 다시 확인한다. validFrameCount=0은 새 sound handoff 없음이며 지속 물리 무음/전송 정지를 뜻하지 않는다. 제공 코드의 startup prime 영역은 정상 사운드용 제3 PCM 버퍼로 승격하지 않으며 실제 길이/배치 채택은 하지 않는다. [R3 A/B](../40_DATA/50_AUDIO_STREAM_DATA.md#pcm) · [R4 쓰기 안전](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#safe-write).

HAL callback은 raw/원 귀속·사실 시각·포착 시각·coverage/loss와 최소 acknowledge/후속 알림에 한정한다. decode/refill/PCM 대량 복사/동적 할당/정책·복구/상위 Core 호출은 하지 않는다. 원 vendor 지속 등록 수명은 개별 VssHalTxRegistration/TxOperation/Attempt와 다르다. raw 지속 key만으로 원 Attempt/cycle을 추정하지 않는다. [R4 지속 등록](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#continuous-stream) · [R5 HAL](../60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#audiohal-callback).

Occurrence/Session/Attempt를 합치지 않는다. uncertain final 뒤 또는 retired 결과는 원정리/진단에만 적용하며 정상 Pending/Started/Completed·새 Session/ready/Fault를 부활시키지 않는다. 독립 보호된 옛 기록이 남은 채 새 A/B 회차를 사용할 조건과 실제 자원 위험 종료는 각각 [Ownership](../50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#references-and-release)을 따른다.

<a id="tbd-register"></a>
## 12. 남은 issue/TBD — 결정한 논리 계약과 필요한 실물 근거

| ID / 상태 | 결정한 계약 / 안전 제한 | 필요한 확인 근거 / 담당 Gate |
| --- | --- | --- |
| TBD-R6-01 / 물리 Header 결핍 | §2 후보 재사용·§3 writer/visibility·§4 DAG 유지. 실제 neutral leaf 위치/alias/enum complete 선언이 확인되기 전 임의 Header·include/ABI 확정 없음 | 최신 Session/Arbitrator/Asset/SoundAsset/Provider/Codec/Clock/Node Communication와 project 중립 Header 전부, 실제 transitive closure. R7는 문서 issue 확인만, 실물 채택 B2-R |
| TBD-R6-02 / C ABI 미정 | typed pointers/OUT·배타 입력이 후보. R3 필드/owner/type identity 유지, fake primitive/폭/enum 숫자/tag/layout/packing 없음 | 기존 coding/ABI 관례·complete typedef/named-tag·실제 public/private symbols. B2-R; 필요한 실제 신규 경계가 드러나면 근거·승인 범위를 별도 제시 |
| TBD-R6-03 / 복사·retain·전달 보호 | 인자 수명과 참조 자료 수명 분리. 임시/얕은 pointer로 후속 자료 보호를 대신하지 않음 | plan/Meta/span/samples/transportFacts/fact/diagnosis/Status/stamps 실제 저장·복사/borrow·consumer 반영/송신 종료·bounded 예산. B2-R |
| TBD-R6-04 / 멱등 요청·bounded 사실 보존 | continuingOperation은 실제 반환 key만. 중복/후속 미확인은 새 HW 요구 재발행 금지; 결과 손실은 LOST/제한. alias 없는 원 key 보호 | 기존 요청 보호·key 전달/재호출 상관·원기록 lookup/consumer 적용 완료·기록 회수/wrap/reboot/직렬화와 예산. B2-R, 새 manager/Queue 설계 없음 |
| TBD-R6-05 / STOP target와 물리 scope 상관 | 기존 원 operation key 범위+Attempt/scope를 명시. 같은 SEGMENT enum으로 구간 identity/whole을 추정하지 않음. 불명확이면 차단·retain·후속 | R5-C1 원 plan/요청/결과 연결을 실제 key·descriptor/active/대기/FIFO/frame 범위에 연결하는 근거, 제어 operation과 대상 작업의 결과 대응. B2-R |
| TBD-R6-06 / 원 시각·권한 비교 | 세 갈래 판정. 실제 fact·충분 coverage와 비교 근거 부족을 구별. 현재 now/새 epoch로 원 발생 갱신 금지 | 원 input source/generation/order/시간 변환·Runtime epoch/wrap/reset, actual earliest/latest/activation·당시 권한 범위 입증. B2-R/정책 근거 |
| TBD-R6-07 / HAL·DMA·출력/접근 물리 충분성 | vendor ABI/지속 raw→operation/cycle·no-start/termination/현재 쓰기 안전 미입증은 confirmed 생성 차단 | 최신 generated/RTD/IRQ/원 callback signature·등록/descriptor/abort/drain·관측 coverage, A/B actual mapping·cache/barrier/alignment·차기 방문/WCET/IRQ 지연. B2-R 및 보드 검증 |
| TBD-R6-08 / Startup·Recovery·정책/보고 | Session 없는 진행; Permission→수행→별도 검증→HEALTH 현재 제한 해제. 미평가/부분 관측으로 정상 보고 합성 없음 | 기존 초기 control entry/순서, exact cause→기존 Fault 5종/Action·timeout/retry/불명확 final·Hold/USE_LIMIT·최초 <2초 제품값·Status reduction/초기 availability/accepting source 계약. 기존 정책 결정/B2-R |

R2 provisional 원형과 R3 SPLIT/INLINE, R5 logical CALL의 표기 차이는 이 문서에서 typed 전달로 해석했다. 승인된 상위 의미를 수정해야 하는 충돌은 확인하지 않았다. 원 C symbol/경계 후보가 typed 의미를 충족하지 못하는 gap은 위 TBD로 공개했으며 R2~R5를 고쳐 사실을 맞추지 않았다.

<a id="scope-gate"></a>
## 13. 과설계·과압축 점검과 중단 Gate

새 Core·Module·상태/필드·전송 PCM·Fault·Header 파일·범용 Command/Result union·getter/setter 계층·Framework·Task/Queue/Mutex를 만들지 않았다. 기존 symbol/private 처리 재사용 우선, FillBuffer/decoder/callback 등록 보호는 기존 내부 지위다. 배타 typed 입력·독립 결과군·원 STOP key·시간/권한 불확실성·참조 회수 조건을 생략하지 않았다. API 수와 typed 선언 위치는 실제 source 결합에서 중복 wrapper 없이 최소화해야 한다.

Governance v1.1과 R2~R5-C1 원문, Layer/Module/Trace/Binding·참고 source는 불변이다. 기존 49 Trace의 End-to-End 검수는 R7 작업이며 여기서 수행하지 않았다. WINDOW **[구현 보류 — 설계 유지]**, Fault 5종, PCM A/B 2개, 모든 비동기 충분조건/TBD를 유지한다.

**R6 작성 당시 이력:** 본 문서의 자체 정합성 결과는 `R6 SELF-CHECK PASS`였고 당시 독립 검수 결과는 대기 상태였으며 [Summary](../../../../R6_RESULT_SUMMARY.md)의 실제 검사 결과를 기준으로 일반 채팅에 결과 ZIP을 넘긴 뒤 중단했다. **현재 R6는 독립 PASS다.** R6 작업 당시 R7·B2-R·GATE-C·실제 C/H 구현·Build/Flash·보드 테스트는 진행하지 않았다. 최신 Stage/게이트는 README를 따른다.
