# ASSET Module — 읽기 전용 MP3 공급

> R1 설계 의미 유지 · R1.1 한국어 표현 교정 · 2026-10-07

[Module Overview](00_MODULE_OVERVIEW.md) · [상위 Layer](../10_LAYERS/20_SERVICE_LAYER.md)

## 1. 목적

read-only Firmware image의 저장 음원을 확인하고 consumer에 bounded compressed MP3 bytes를 공급한다.

## 2. 역할

ASSET(C10)은 Internal PFlash의 Asset ID/metadata·위치/길이·읽기 범위와 가용 근거를 관리한다.

## 3. 책임

- Asset lookup/metadata와 요청 범위·overflow를 확인한다.
- 요청 범위의 유효 bytes·읽기 결과를 제공한다.
- 실제 consumer 참조가 종료될 때까지 read span/staging을 보존한다.

## 4. 책임이 아닌 것

decoder 입력 소비 위치·PCM 생성·출력 owner를 변경하지 않는다. 런타임 Flash erase/program·파일시스템·외부 Storage Driver는 이 저장 경로의 책임이 아니다. decode corrupt/unsupported/provider 실패의 발견 owner는 AUDIO STREAM이다.

## 5. 소유 상태 / 데이터

read-only image/build의 descriptor/metadata·읽기 범위·가용 근거와 consumer에 제공한 span/staging 참조 관계를 ASSET이 소유한다. decoder 진행 상태와 별도로 관리한다.

## 6. 입력

AUDIO STREAM의 Asset·offset·요구 길이와 읽기/참조 진행 요구를 받는다. FLOW는 기존 공개 가용 근거를 관측한다. 이미지/배치 정보는 BSP의 build-time 근거다.

## 7. 출력

확인된 metadata/가용 범위, bounded compressed bytes·유효 길이/읽기 결과, lookup/metadata/bounds/PFlash read 단계의 실패 근거를 제공한다.

## 8. 사용하는 / 제공하는 주요 Core Function — R2

[Asset_Read](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#asset-read)

현재 Core는 [Asset_Read](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#asset-read)다. metadata/범위 읽기와 제공 자료의 실제 참조 수명을 같은 ASSET 경계에 둔다. 별도 lookup/read/release wrapper나 Storage Driver는 만들지 않는다.

## 9. 의존 Module

AUDIO STREAM → ASSET → read-only image / 필요한 RUNTIME이다. FLOW의 관측은 기존 공개 의미 경계를 사용한다. Platform 참조는 build-time 이미지/배치 근거이며 runtime Flash 호출 의존이 아니다.

## 10. 상태 전이 또는 주요 lifecycle

Asset lookup/metadata·범위 overflow 확인 → 요청 구간 읽기 → consumer 참조 종료까지 span/staging 보존의 흐름이다. lookup 성공이 아직 읽지 않은 전체 MP3 decode 성공의 증명은 아니다. direct Flash/staging의 실제 방식은 TBD다.

Descriptor는 해당 image/build 문맥이 유효한 동안 사용하고 span/staging은 consumer 참조 종료까지 유지한다.

## 11. Module-local Contract

lookup/read 성공은 아직 읽지 않은 전체 MP3 decode 성공을 증명하지 않는다. ASSET 발견 원인은 lookup/metadata/bounds/PFlash read이며 decode 실패는 [AUDIO STREAM](24_AUDIO_STREAM_MODULE.md)의 오류 발견 주체와 단계를 보존한다. Session retirement만으로 소비 중 bytes를 회수/overwrite하지 않는다.

## 12. 대표 오류 / 예외 처리 원칙

Asset 누락/metadata·범위 부적합/읽기 오류는 확인한 범위의 실패 근거로 전달한다. 다른 의미의 음원으로 대체하지 않는다.

## 13. 관련 Function 문서

[Asset_Read](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#asset-read) · [기존 후보의 R2 판정](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md#candidate-decisions) · [이전 명칭의 의미 연결](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md#merged).

## 14. 관련 Data 문서

[Asset Descriptor / Read Span](../40_DATA/50_AUDIO_STREAM_DATA.md#asset)

## 15. 관련 공통 Contract

[Ownership / Lifetime](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [Fault](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault)

공통 원문의 위치를 연결한다. 여러 Module에 동일한 규칙의 통합·중복 제거는 R4에서 수행한다.

## 16. TBD / Deferred

실제 image·Asset table·읽기 방식과 decoder format은 [Asset TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#asset), Flash budget은 [메모리 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#memory)를 확인한다. direct Flash/staging 방식·field/layout·Header 소유는 이번에 확정하지 않는다.
