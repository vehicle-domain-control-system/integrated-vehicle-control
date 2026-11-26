# U3 C11 Host RAW 진단 Adapter

G2 기준 `489b2099fa183a35d4a84a5a32574532f8481b27`에서 두 CAN 후보
`CIS_REAR(0x0A0)`와 `DOMAIN_PERMISSION(0x080)`만 진단한다.
`WIRE_PROPOSED`이며 Host 실행 결과는 ECU 수신·차량 실행·팀 계약 채택을
증명하지 않는다. G3 독립 검수는 별도다.

저장소 루트의 PowerShell에서 실행한다. Python 표준 라이브러리와 C11 GCC
또는 GCC 호환 Clang이 필요하다. `--output`은 기존 자료를 보존하기 위해
매번 새 디렉터리를 지정한다. 결과와 실행 파일은 로컬 무시 경로에 저장한다.

```powershell
python tools/network_host/u3_raw_diag/build_and_test.py --compiler gcc --output _network_local/evidence/u3_repeat_01
```

스크립트는 두 번 별도 빌드·실행하고 stdout 일치, 독립 정적 벡터의
Python LE 검산, 기존 I1 CAN golden 9개 encode/decode, 실제 전처리 의존성을
확인한다. 명령·버전·종료코드·stderr·case별 결과·소스 SHA는 출력 디렉터리의
`results.json`, `commands.json`, `build_and_test_logs.txt`에 보존한다.
MinGW는 표준 `snprintf`/inttypes 출력을 위해 `__USE_MINGW_ANSI_STDIO=1`을
컴파일 명령에만 설정한다. 다른 컴파일러·sanitizer의 실행은 추정하지 않는다.

## 원본과 최소 경계

I1 원본 `NETWORK_I1_PROVISIONAL_HOST_RESULT.zip` SHA256:
`4b65228c4fbd3f9ca909d9f069e58b5fbbd3dfa9e276e94e6349a17e3dbdc2ab`.
[I1 출처](i1_provenance.json)에 명시한 `i1_host_codec.c/.h` 두 파일을
바이트 그대로 고정했다. 원본 한 파일에 포함된 기존 UART 함수도 그대로
보존하지만 U3 Adapter는 CAN 함수 네 개만 호출한다. `i1_m_request`는 추가하지
않았다. [기존 CAN 벡터](i1_can_vectors.json)는 원본 fixture에서 두 메시지의
9개 객체를 변경 없이 선택했다. [U3 독립 입력](independent_vectors.json)의
기대 바이트는 시험 대상 Codec으로 생성하지 않는다.

| 경계 | 구현·근거 |
|---|---|
| `u3_raw_decode` | 표준 ID/IDE/FDF/BRS/RTR/DLC/20B 검사 → 실제 I1 decode → raw 20B와 전체 typed 필드·로컬 RX 시각을 bounded 사본으로 반환 |
| `u3_raw_encode` | 진단 필드만 실제 I1 encoder에 연결; CAN 송신·Permission 생성·TX admission 없음 |
| `u3_raw_format` | 원 raw와 typed 필드 일치 확인 후 bounded 출력; 원 식별자·품질·상태·AGE·flag·pad 보존 |
| 오류 | unsupported ID, profile, 길이/DLC/padding, 예약 Boot, 불법 코드, 인자/출력 용량을 구분; 실패 시 출력 사본 보존·written=0 |

근거는 기존 `docs/network/interface/03_COMMUNICATION/10_CAN_FD.md`,
`04_COMMON/10_DATA_AND_ID_RULES.md`, `04_COMMON/30_ENUM_WIRE_MAPPING.md`와
승인된 `docs/network/sw_detail/flows/`의 두 흐름이다. 공식 API와 U2 문서는
변경하지 않는다. 패키지의 타입/함수는 Host 내부 경계다.

## 원 의미와 미결 보존

- CIS 거리는 `rear_distance_deci_cm`의 **원 u16**로 출력한다. mm/cm 변환,
  센서 물리 범위, C2 enum binding은 `UNRESOLVED_CONTRACT`다. `65535`는
  sentinel이다. NO_OBJECT·UNAVAILABLE·FAULT·RECOVERING·INACTIVE와 원
  validity/reason을 구분하며 CLEAR를 만들지 않는다.
- Permission은 power·operation의 raw 값과 독립 품질을 표시한다. raw 1/OK도
  기능 ALLOW 승인이 아니며 raw 0/NO_DATA도 확인된 DENY가 아니다. 원 producer,
  coherence, 원 사용 기한과 `150 ms SOURCE_LIMIT_NOT_AGREED`는 OPEN이다.
- AGE를 재계산하거나 0으로 초기화하지 않는다. 65535는 포화된 하한이며 fresh
  증거가 아니다. BOOT/TX와 UPDATE는 별도 필드다. RX 시각은 호출자가 관측한
  **개별 수신 사실**일 뿐 원 관측 시각이 아니다. 단일 frame pure 진단에는
  관측 기한·liveness 타이머·replay cache가 없으므로 중복 TX가 원 시간이나
  사용 기한을 다시 시작할 수 없다. Boot admission/순서/wrap 정책은 U4D다.
- 코드 범위 검사는 기존 I1과 동일하다. 범위 안 필드 조합의 producer·coherence,
  원 AGE 진실성·발신 노드 신뢰성·센서 정상·제품 채택은 평가하지 않는다.
- heap·queue·router·callback·RTOS·SDK/HAL·보드·UART Adapter·WINDOW 구현은
  추가하지 않는다. 입력/출력은 겹치지 않아야 하며 호출자가 읽을 수 있는
  실제 길이를 제공한다. format에는 성공적으로 decode한 사본을 전달한다.

T01~T17은 정상·열화·지원 범위·ID/flag/DLC/길이/pad·전체 raw 코드·예약
Boot·sentinel·AGE 포화·동일 UPDATE/새 TX·중복 TX·literal wrap·NULL·출력
용량·소유권을 검사한다. T18은 두 번 재현, T19는 실제 I1 연결/회귀,
T20은 컴파일 의존성 검사다. Host 검수 후 U4D는 별도 사용자 지시가 필요하다.
