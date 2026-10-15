# VSS SW 상세 가이드 — 처음 읽기

> GUIDE · 설계 정본: `VSS@0641b2c2268cf47450aeb50302638a45a7a3277f`.

VSS는 Domain이 확정한 사건·경고를 검증하고, 유효한 후보에서 하나의 음향 Session을 진행하며 상태·고장을 보고합니다. 여기서는 **VSS → Layer → Module → 함수 설명·의사코드**를 따라 읽을 수 있습니다.

추가 탐색: [외부 입력·Status·18 Core 호출/반환 지도](02_INTERFACE_MAP.md) · [기존 6 Flow·49 Trace 시나리오 지도](03_SCENARIO_MAP.md).

실제 ABI·양단 채택·HW는 미확정이며, 외부 미결15와 P2 HOLD를 유지합니다.

## 1분 안에 찾기

1. [VSS 지도](01_VSS_MAP.md)에서 담당 Layer를 선택합니다.
2. Layer의 Module 링크를 열어 상태 owner와 입력·출력 경계를 확인합니다.
3. Module의 함수 표에서 설명·의사코드 링크를 엽니다. 전체 18 Core의 36개 문서는 [문서쌍 색인](01_VSS_MAP.md#18-core의-문서쌍)에서 확인할 수 있습니다.
4. 함수 설명의 입력·반환/OUT·예외·수명을 읽고, 짝을 이룬 의사코드와 **동결 원문 링크**를 대조합니다.

자주 찾는 곳: [STORE — 중복·발생 이력](02_SERVICE/STORE/00_MODULE_OVERVIEW.md), [AUDIO TX — PCM·DMA 근거](03_DRIVER/AUDIO_TX/00_MODULE_OVERVIEW.md), [HEALTH — 상태 보고](02_SERVICE/HEALTH/00_MODULE_OVERVIEW.md), [HAL callback Boundary](04_HAL_BSP/00_LAYER_OVERVIEW.md).

## 문서 작성 기준

설명과 의사코드는 별도 파일쌍입니다. Module 표의 두 링크로 진입하고, 각 파일의 설명 ↔ 의사코드 ↔ Module 상대 링크를 따라 읽습니다.

| 문서 | 읽을 내용 | 단일 근거 |
|---|---|---|
| 함수 설명 | 한 문장 목적, 논리 owner·Caller/Callee, 입력·반환/OUT 의미, 변경 상태, 정상·예외·시간·참조 수명, 미정과 출처, 독립 SW 후보 | [R2 함수 색인](../design/30_FUNCTIONS/00_FUNCTION_OVERVIEW.md), [R3 자료](../design/40_DATA/00_DATA_OVERVIEW.md) |
| 함수 의사코드 | R6 논리 원형을 바탕으로 C 형태와 한국어 주석, 검사·분기·반복·종료·return 및 사후조건, 부족한 근거와 잠정값의 한계 | [R6 논리 Interface](../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md), [R5-C1 흐름](../design/60_PSEUDOCODE/00_PSEUDOCODE_OVERVIEW.md) |

실제 C ABI·enum 폭·API 바인딩은 미정입니다. R3 자료나 [R4 공통 계약](../design/50_CONTRACTS/00_CONTRACT_OVERVIEW.md)을 새 정본으로 복사하지 않고, 필요한 필드·의미만 설명하고 원문으로 연결합니다. 근거 없는 helper·Task·Queue·Manager를 추가하지 않습니다.

## 표시를 읽는 방법

`GUIDE`는 탐색 설명, `FROZEN_SEMANTIC`은 기존 승인 논리, `PROVISIONAL_TESTABLE`은 근거 있는 잠정 시험 후보, `PROPOSED_IF`는 미합의 외부 제안입니다. `BINDING_TBD`·`HW_UNVERIFIED`는 실제 연결·물리 근거 미확정, `TEST_NOT_RUN`은 시험 미실행을 뜻합니다. 이 표시는 문서의 확인 수준이며 새 제품 enum이 아닙니다.

독립적인 중복·품질/Hold·읽기 범위·late/Unknown·Status 판단은 후속 SW/Host 후보로 유지합니다. 외부 IF·물리 안전 Gate는 종속된 제품 연결에 적용합니다. 현재 가이드는 구현·Host 시험·Build·보드 성공을 승인하지 않습니다.

<a id="현재-기준과-다음-작성"></a>
## 현재 기준과 문서 범위

유효 문서 기준은 Master v1.1(원 기술 REQ-01~27 + 편집 REQ-28~35)이며, 기술 의미는 [동결 설계](../design/00_OVERVIEW/00_VSS_DESIGN_OVERVIEW.md)를 따릅니다. 설계 기준 `VSS@0641b2c2268cf47450aeb50302638a45a7a3277f`는 [SHA 고정 GitHub 원문](https://github.com/vehicle-domain-control-system/integrated-vehicle-control/blob/0641b2c2268cf47450aeb50302638a45a7a3277f/docs/vss/design/30_FUNCTIONS/00_FUNCTION_OVERVIEW.md)과 같으며, 이 편집의 게시 원본 `VSS@2702d6adec768627794dc07b2b221a63405fd79f`와 구분합니다.

상세 가이드를 따로 배포할 때는 `sw_detail/`과 동결 `design/` 54개 문서의 상대 경로를 함께 유지해야 합니다. 외부 참조 미결15건은 해소되지 않았습니다.
