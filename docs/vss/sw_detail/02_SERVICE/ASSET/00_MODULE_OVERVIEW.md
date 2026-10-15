# VSS / SERVICE / ASSET

[상위 Layer](../00_LAYER_OVERVIEW.md) · [VSS 지도](../../01_VSS_MAP.md) · [문서 양식·확인 수준](../../00_START_HERE.md)

> GUIDE · 설계 정본: `VSS@0641b2c2268cf47450aeb50302638a45a7a3277f`.

read-only Firmware image의 저장 MP3를 확인하고 consumer에 bounded compressed bytes를 공급합니다. lookup·metadata·범위/overflow와 읽기 근거를 다루며 CPU decode는 AUDIO_STREAM에 둡니다.

## 상태 owner와 입출력

ASSET은 image/build 문맥의 descriptor·metadata·읽기 범위/가용 근거와 제공한 span/staging의 consumer 참조 관계를 소유합니다. decoder 입력 소비 위치·PCM·출력 Session을 쓰지 않습니다.

| 구분 | 주요 의미 |
|---|---|
| 입력 | AUDIO_STREAM의 Asset ID·offset·길이·읽기/참조 요구; BSP의 build-time 이미지 배치 근거 |
| 출력 | 확인한 metadata·bounded bytes·유효 길이·읽기 결과와 lookup/metadata/bounds/read 실패 근거 |
| 의존 | AUDIO_STREAM → ASSET → read-only image 및 필요한 RUNTIME; runtime Flash erase/program 경로 없음 |

## 인접 관계와 비동기 경계

lookup/read 성공은 아직 읽지 않은 MP3의 decode 성공을 뜻하지 않습니다. span/staging은 실제 consumer 참조가 끝날 때까지 유지하므로 Session retirement만으로 회수하지 않습니다. 읽기 발견 오류와 STREAM의 decode 발견 오류를 구분합니다.

[AUDIO_STREAM의 decode·provider](../AUDIO_STREAM/00_MODULE_OVERVIEW.md) · [BSP build 근거](../../04_HAL_BSP/00_LAYER_OVERVIEW.md)

<a id="담당-core의-예정-문서쌍"></a>
## 담당 Core의 문서쌍

| Core | 함수 / 역할 | 상세 설명 | 한국어 의사코드 |
|---|---|---|---|
| CORE-09 | `Asset_Read` — 동기 범위 읽기 / consumer 참조 지속 | [상세 설명](functions/Asset_Read.md) | [한국어 의사코드](pseudocode/Asset_Read.md) |

CORE-09의 설명·의사코드는 위 링크로 왕복합니다. 원형·자료·참조 수명의 기술 정본은 아래 동결 원문입니다.

## 후속 진행 후보와 미정

bounded read·오류/부분 읽기·참조 유지의 독립 Host 후보를 보존합니다. 기존 dated AudioAsset 읽기 경계는 재사용 후보이며 제품 table/image·빠진 자산·Flash budget·direct Flash/staging 채택은 미확정입니다. 새 Storage Driver·AssetManager를 만들지 않습니다. CORE-09 문서쌍은 위 링크에서 읽습니다.

위 후보는 `TEST_NOT_RUN`이며 실제 코드/Host 실행은 별도 지시·범위·Gate가 필요합니다. 미해결 Q/R6·외부 IF/HW에 종속된 제품 결합과 P2는 계속 HOLD입니다. 문서 관계 보정의 승인 범위를 원 G1-B 원본 복구나 실제 제품 C1 완료로 확대하지 않습니다.

## 현재 상세 근거

- [기존 ASSET Module의 책임·lifecycle](../../../design/20_MODULES/25_ASSET_MODULE.md)
- [R2 담당 함수·Caller/Callee](../../../design/30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md)
- [R3 관련 자료·writer·수명](../../../design/40_DATA/50_AUDIO_STREAM_DATA.md)
- [관련 R4 공통 계약](../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md)

논리 원형과 실제 C ABI의 구분은 [공통 문서 작성 기준](../../00_START_HERE.md)에서 R6 원문으로 이어집니다. 이 페이지는 탐색·설명 보조이며 승인 기술 정본을 대체하지 않습니다.
