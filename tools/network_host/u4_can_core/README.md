# CAN 공통 Host Core

C11에서 `CIS_REAR`와 `DOMAIN_PERMISSION` 두 CAN FD 프로파일을 처리하는 private 라이브러리와 결정적인 Host Mock이다. 기존 I1 Codec을 직접 링크한다. UART, Node의 공개 API, 기능 판단 및 실제 MCU backend는 포함하지 않는다.

```powershell
python tools/network_host/u4_can_core/build_and_test.py --compiler gcc --output _network_local/evidence/u4c/my_run
```

매 실행에 새 출력 폴더를 지정한다. GCC/Clang 계열에 C11과 모든 경고를 오류로 처리하는 옵션을 적용한다. 실행 파일, 명령, 실제 stdout/stderr 및 사례별 입력·기대값·관측값은 출력 폴더에 남긴다. `--sanitize`는 설치된 도구가 지원할 때 address/undefined 검사를 추가한다. 테스트 없이 라이브러리를 빌드할 때 `CAN_CORE_TRACE`를 정의하지 않는다.

## API와 소유권

전체 타입과 정확한 16개 함수 원형은 [can_core_private.h](can_core_private.h)에 있다. Message Service 5개, Codec 2개, Transport 5개, LinkPort 4개다. 각 instance와 배열은 caller가 zero-init하고 독립적으로 제공한다. backing의 실제 원소 수와 수명을 정직하게 지정하고 배열·config·instance·출력 영역이 겹치지 않게 한다. runtime 동적 할당과 전역 상태는 없다.

초기화 순서는 Port → Transport → Service → 수신 source Bind다. Port와 Transport는 일대일 생애를 가진다. 새로운 token issuer를 기존 generation에 붙이거나 serial을 초기화해 재사용하지 않는다. Service를 같은 송신 BOOT 생애에서 재생성하여 송신 순번을 초기화하지 않는다. 필요하면 기존 owner가 새 생애를 명시하고 하위 storage/read/callback 수명 종료를 먼저 증명해야 한다.

owner 한 곳에서 `CanTransport_Pump(now,budget)` 다음 `CanSvc_Pump(now,budget)`를 호출한다. Service budget은 3 이상이며 두 current key를 공정하게 시도한다. 미수용 pending만 교체한다. Port accepted 이후에는 Service가 선택한 TX 순번을 commit하고 T/P의 서로 다른 frame 사본은 불변이다. `CanSvc_Submit`의 수용은 Service의 안정된 복사, Port accepted는 전송요청 수명 인수, terminal COMPLETE는 local controller의 종결이다. peer 수신이나 기능 성공의 증거로 사용하지 않는다.

Codec은 IDE0/FDF1/BRS1/RTR0, 두 ID, DLC11, 20바이트, 사용19바이트와 zero padding을 검사한다. struct 크기는 Wire 길이가 아니다. 정상 열화·unknown·sentinel 값은 그대로 보존한다. Codec 실패 시 OUT을 변경하지 않으며 실제 I1 오류와 I1 호출 여부를 반환한다. 같은 객체를 IN/OUT에 겹쳐 사용하지 않는다.

## 오류와 복구

확인된 NO_START만 TX reservation을 rollback한다. 시작 가능성을 배제할 수 없으면 Port가 owned bytes/token을 수용하고 UNKNOWN으로 별도 통지한다. UNKNOWN은 confirmed fail이 아니며 늦은 COMPLETE로 승격하지 않는다. 슬롯별 terminal cell은 RX ring과 분리해 보존하고 한 번만 소비한다. RX full은 기존 records를 유지하며 newest drop과 포화 counter를 기록한다.

UNKNOWN 뒤 Port bytes는 backend가 읽을 가능성이 있는 동안 고정한다. reset은 admission을 중지하고 미확정 accepted를 UNKNOWN으로 인수한 뒤 generation을 한 번 fence한다. 이미 확정된 terminal은 유지한다. backend가 모든 read reference와 old callback을 해제한 실제 flush 근거를 제공해야 storage를 회수하고 준비 상태로 돌아간다. generation/serial 최대값에서는 wrap하지 않고 새 수용을 중지한다. accepted 프레임의 자동 재전송은 없다.

`CanHostBackend`의 admit/flush 두 hook은 Host binding용이다. Mock은 Port 사본의 주소를 보관하고, flush에서 실제 reference를 모두 제거한 뒤에만 quiescence를 반환한다. capture helper는 bounded frame/event 복사만 수행한다. 실제 ISR의 atomic publish/acquire, IRQ·RTOS·SDK 동기화는 별도 backend의 책임이며 이 단일 owner Mock이 증명하지 않는다.

Clock은 valid·같은 epoch·외부 finite horizon 근거가 있을 때만 u32 경과를 계산한다. owner 관측 간격과 원 AGE·accepted 수명을 구별하고 AGE는 u16로 포화한다. 같은 UPDATE의 재제출/재수신으로 누적 AGE를 낮추지 않는다. 불명확한 경과는 UNCERTAIN 또는 송신 보류로 남긴다. 인코드 이후 측정하지 못한 전송 구간 때문에 AGE는 하한이며, 기능 fresh/ALLOW 판단에 사용하지 않는다.

diagnostic sink는 정상 반환하고 borrowed view를 저장하려면 전체 복사한다. callback 동안 Svc/T/P의 상태 API 재진입을 거부한다. `CanSvc_OnDecoded`는 private 입력이며 같은 Codec 결과와 RX record의 쌍만 전달한다. 구현은 기존 encoder로 원 bytes와의 대응도 검사해 잘못 결합된 입력을 거부한다. 실제 기능 callback이나 제품 입력으로 공개하지 않는다.

두 메시지의 raw 단위·품질·원 식별자를 보존하며 C2 단위/enum 변환, Permission producer/coherence/원기한, MCU capability·실시간성은 합의 근거가 필요하다. 제공 용량·watchdog·source horizon은 Host 시험 프로파일이다. WINDOW runtime과 기능 ALLOW/DONE/CLEAR 생성은 지원하지 않는다.
