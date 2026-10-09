# SERVICE Layer

> R1 Layer 설계 유지 · R1.1 한국어 표현 교정 · 2026-10-07

[Layer Overview](00_LAYER_OVERVIEW.md)

## 1. 목적

차량 의미와 실행 정책을 HW 연결 세부에서 분리해 VSS의 상태·재생·진단 판단을 관리한다.

## 2. 역할

SERVICE는 의미 입력·상태·정책과 MP3/PCM 준비를 맡는다. APP에는 중립 요청/관측/결과를 제공하고 vendor handle·실제 채널/RTD 설정은 하위에 둔다.

## 3. 책임

- 입력 검증과 발생/현재 후보를 관리한다.
- read-only 선택과 단일 전체 재생 정책을 진행한다.
- 저장 MP3 공급·CPU decode·PCM 준비와 Backend 결과 결합을 진행한다.
- 진단/복구 판단과 외부 보고를 파생한다.

## 4. 책임이 아닌 것

물리 통신 framing/Network TX lifecycle, vendor/IRQ binding, 보드 구성, 장치가 버퍼에 접근할 수 있는 동안의 사용권 보호(retain)에 관한 원본 상태는 이 Layer의 책임이 아니다. HEALTH는 HW 복구나 Network 송신을 직접 수행하지 않는다.

## 5. 포함 Module / 실제 내부 책임

INPUT / STORE / POLICY / PLAYBACK / AUDIO STREAM / ASSET / HEALTH의 7개 Module이다. 각 원본 상태의 owner는 [Module Overview](../20_MODULES/00_MODULE_OVERVIEW.md)에 연결하며 INPUT~HEALTH의 책임 ID(C02~C12)를 C file·Task에 1:1 배치하지 않는다.

## 6. 입력 / 출력

| 구분 | 의미 |
| --- | --- |
| 입력 | 값+Meta/TEST·시간 근거, APP 처리/실행 요구, DRIVER의 원래 출력 시도·버퍼 사용 회차·구성 장치 근거 |
| 출력 | 수용·후보 관측·판단/계획·재생 사실·보고와 DRIVER에 대한 중립 PCM/구성 제어 요청 |

## 7. 허용 의존 방향

| Module | 허용된 주요 의존 |
| --- | --- |
| INPUT | STORE / 필요한 RUNTIME, 기존 Node Communication 의미 경계 |
| STORE / POLICY / ASSET / HEALTH | 필요한 RUNTIME. 관측/판단 전달은 FLOW가 조율 |
| PLAYBACK | AUDIO STREAM / STORE / 필요한 RUNTIME |
| AUDIO STREAM | ASSET / AUDIO TX / AUDIO CONTROL / 필요한 RUNTIME |

ASSET의 Platform 참조는 이미지/배치의 build-time 근거다. runtime Flash 호출을 추가하지 않는다.

## 8. 금지 의존 방향

SERVICE → RTD/SAI register/vendor handle 직접 접근을 금지한다. 다른 Module의 authoritative 상태를 직접 쓰지 않는다. POLICY → PLAYBACK 직접 실행 호출, HEALTH → HW 복구/Network 송신 직접 호출을 만들지 않는다.

## 9. Layer 수준 Contract

차량 의미·전체 정책과 장치 근거의 범위를 구별한다. Audio 요청은 DRIVER 중립 경계로 전달하고 결과는 해당 상태의 owner가 반영한다. Session/Attempt·PCM·Fault의 구체 계약은 해당 Module과 기존 공통 원문 위치에서 확인한다.

## 10. 대표 처리 흐름

입력/제어는 INPUT → STORE 후보 → POLICY 판단 → PLAYBACK이며 관측·판단 전달은 FLOW가 조율한다. 출력은 PLAYBACK → AUDIO STREAM → ASSET/DRIVER이고 장치에서 관측한 원본 사실은 Backend 결과를 거쳐 전체 재생 사실로 반영된다. 반영된 관측에서 HEALTH가 보고를 파생한다.

## 11. Module / 하위 상세 링크

[INPUT](../20_MODULES/20_INPUT_MODULE.md) · [STORE](../20_MODULES/21_STORE_MODULE.md) · [POLICY](../20_MODULES/22_POLICY_MODULE.md) · [PLAYBACK](../20_MODULES/23_PLAYBACK_MODULE.md) · [AUDIO STREAM](../20_MODULES/24_AUDIO_STREAM_MODULE.md) · [ASSET](../20_MODULES/25_ASSET_MODULE.md) · [HEALTH](../20_MODULES/26_HEALTH_MODULE.md)

## 12. 관련 공통 Contract

[공통 계약의 현재 원문 위치](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md)

## 13. TBD / Deferred

Function 상세/prototype은 R2, 데이터 필드/타입은 R3, 공통 계약 통합은 R4 후속이다. 미정 [정책값](../90_BINDING/91_IMPLEMENTATION_TBD.md#policy)과 [입력 표현](../90_BINDING/91_IMPLEMENTATION_TBD.md#identity)은 유지한다. WINDOW [구현 보류 — 설계 유지]는 INPUT의 제품/TEST 경계를 따른다.
