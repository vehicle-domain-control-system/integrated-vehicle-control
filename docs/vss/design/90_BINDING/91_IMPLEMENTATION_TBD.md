# Implementation TBD

R2~R6의 논리 설계와 R7 뒤 B2-R의 실제 C Mapping에서 확인할 기존 값·근거를 한 곳에 모았다. Module의 TBD는 여기로 연결한다. [82 Data Model](../40_DATA/00_DATA_OVERVIEW.md)은 의미·owner·lifetime 기준이고 [80 API 후보](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md)는 논리 호출 경계다. 실제 C 표현·signature·자료 복사/참조와 저장 방식은 여기의 근거를 확인해 다음 Mapping에서 결정한다. 미확인 값은 임의로 채우지 않는다.

<a id="identity"></a>
## 1. 입력·identity·추적

| 확인 대상 | 필요한 근거 |
| --- | --- |
| 기존 Network 값+Meta·Generation/Ordering/원본 시간 전달 | current Node Communication 계약과 source. WINDOW 제품 연결 보류 상태 유지 |
| occurrence·Session·Attempt·operation·Buffer 회차/불변 callback 연결 | current callback signature·재전달 범위·참조 lifetime |
| 수용/이력/context 보호 용량과 안전 회수 | 기존 project 자원 예산·결과 통지/참조 종료·replay/시간 연속성 근거 |

<a id="execution"></a>
## 2. 실제 코드·실행·시간

| 확인 대상 | 필요한 근거 |
| --- | --- |
| C file/API/type·field명/width/layout·signature·public/private·상태 저장/참조 | 82의 owner·identity/lifetime과 최신 source·기존 모듈/API 관례. 의미 묶음/API 후보 수로 struct/file을 복제하지 않음 |
| 기존 실행 문맥·처리기회·직렬화·정적 자원 예산 | 현 Task/IRQ/Infra·producer/consumer·bounded 처리량. Task/priority·Queue/Mutex를 선결정하지 않음 |
| 공통 timebase·wrap/reset·timestamp 정밀도/coverage·기한 gate | 실제 time source와 lower start 관측, 원본 비교/연속성 |
| marker 포착 지점과 decode/refill/정리 시간 여유 | 원operation 자료·CPU WCET·보드 물리 출력 상관/측정 |

<a id="asset"></a>
## 3. Asset·decode

실제 Asset 파일/table·metadata와 image 배치, decoder library·MP3 bitrate/sample rate/channel·PCM format, 입력 direct Flash/staging 여부와 필요한 provider 작업 공간을 확인한다. 과거 bring-up의 format/library를 현 확정값으로 가져오지 않는다.

<a id="memory"></a>
## 4. Flash·SRAM·DMA 접근

| 확인 대상 | 필요한 근거 |
| --- | --- |
| Asset Flash budget | current linker/map에서 code·const·Boot/HSE/기타 reserved·정렬 여유를 제외. 칩 전체 Flash 용량을 예산으로 쓰지 않음 |
| SRAM/CPU budget | decoder state/scratch·필요 입력/decoded 작업 공간·PCM A/B·descriptor·기존 project 자원 합산 |
| A/B size/alignment/section·MPU/cache·DMA-visible 접근 | current map/MPU/cache·실제 DMA mode. PCM 전송 Buffer는 정확히 2개 유지 |
| channel/TCD/mode·인계/재접근·짧은 유효 tail | generated config/RTD source·해당 회차 접근 종료 근거 |

<a id="hardware"></a>
## 5. HW·RTD·출력 근거

current board/source/generated의 Clock/PinMux/IRQ·SAI/SGTL5000/CS2100/Reference 구성·순서/ratio·register와 지원 범위를 확인한다. 실제 RTD의 요청 수락/arm, partial error·abort postcondition, descriptor 재접근·FIFO/frame 정리, fresh actual start·과거 no-output coverage·전체 end 충분성을 확인한다. IRQ disable과 software 참조 종료는 구별한다.

기본 책임은 [TX DRIVER](../20_MODULES/30_AUDIO_TX_MODULE.md#evidence)와 [HAL/BSP](../10_LAYERS/details/40_HAL_BSP_BOUNDARY.md)를 따른다. 실제 API·물리 값 확정은 후속 `90_C_FILE_API_MAPPING.md`, 충분성/성능 입증은 구현·보드 확인에서 다룬다.

<a id="policy"></a>
## 6. 미정 정책과 후보값

Hold 상세값·미정 cue/반복/무음/출력 수준, start/정리 timeout·retry, exact cause→기존 Fault/DIA-017/Recovery Action·해제 조건·초기 Availability/전체 reduction은 기존 잠정/TBD를 유지한다. 수치가 없다고 새 정책/복구 framework를 만들지 않는다. One-shot의 기존 **[잠정] 실제 새 시작 age < 2초** 및 기존 PER-001~010의 후보 상태는 바꾸지 않는다.



## 문서 연결과 후속 범위

[Document Map](../00_OVERVIEW/01_DOCUMENT_MAP.md) · [C Interface 미확정](../70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md) · [기존 Mapping](90_C_FILE_API_MAPPING.md)

R0에서는 모든 기존 미정/잠정 범위를 유지했다. 논리 필드·pseudo type은 R3, Header ownership은 R6, 실제 RTD/HW/메모리 근거는 B2-R 및 구현·보드 확인에서 구분해 다룬다.
