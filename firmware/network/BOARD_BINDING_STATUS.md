# 보드 binding 상태 — Network 골격

2026-10-17(Asia/Seoul) 기준으로 **S32K144와 S32K344의 Network HAL binding은 `NOT_CONFIGURED` / `BINDING_OPEN` 그대로다**. Host Mock은 따로 주입하는 시험 구현이다. Host 통과는 공통 C 동작만 확인한다. 실제 대상 빌드, Flash, CAN/UART 통신, 인터럽트 동시성, 전기 측정은 **NOT_RUN**이다.

## 플랫폼 경계

| 영역 | S32K144 | S32K344 | Host Mock |
|---|---|---|---|
| 드라이버와 SDK | 쓸 수 있는 로컬 Network 드라이버 프로젝트가 확인되지 않음 | RTD/PlatformSDK 7.0.1 오디오 증거를 찾음. Network 드라이버 binding은 확인되지 않음 | 표준 C. SDK 필요 없음 |
| 핀 / 트랜시버 / 버스 인스턴스 | BINDING_OPEN | 오디오 SAI/LPI2C/GPIO 핀 설정은 있음. CAN/UART 배정과 트랜시버는 BINDING_OPEN | 주입한 바이트와 전송 종류 |
| 시계 / 시각 | BINDING_OPEN | 오디오 PIT 기준 시계는 있음. Network tick 단위, wrap, 바이트 시각, STM 정책은 BINDING_OPEN | 시험이 제어하는 가짜 시계 |
| CAN nominal/data bitrate / UART baud | BINDING_OPEN | BINDING_OPEN | 물리 속도를 주장하지 않음 |
| IRQ / ISR / 동기화 | BINDING_OPEN | BINDING_OPEN | 결정적인 단일 소유자. 실제 ISR 증거 없음 |
| 접수 / 최종 / 해제 | 실제 장치 계약 미확인 | 실제 장치 계약 미확인 | 서로 독립적으로 주입·관찰하는 이벤트 |
| 대상 활성화 | NOT_CONFIGURED | NOT_CONFIGURED | Host 빌드/시험 대상. `_local_review/HOST_TEST_RESULTS.txt` 참고 |
| 하드웨어 검증 | NOT_RUN | NOT_RUN | 해당 없음 |

MCU 어댑터 골격은 `NOT_CONFIGURED`/`UNSUPPORTED`를 돌려줘야 하고, 지원하지 않는 대상 설정은 명시적으로 실패해야 한다. 핀, 주파수, baud rate, 트랜시버 활성화 순서, IRQ 등록, SDK API, 하드웨어 성공 결과를 지어내지 않는다. 공통 Layer/Module 소스는 이식형 C와 공통 Port 인터페이스만 쓰고, 플랫폼 내부는 `src/platform/` 아래에 둔다. 경계는 M08이 소유하며, 플랫폼 폴더가 여섯 번째 Layer를 만들지 않는다.

## 로컬에서 확인한 증거

저장소 HEAD는 `00a43414807e7a5b9c087d589a1183d1989e5b91`이다. 하드웨어 파일은 `hardware/README.md` 하나뿐이고, HEAD의 firmware에는 `firmware/README.md`만 있다. 두 파일의 해시, 그리고 관련된 읽기 전용 설계 소스는 `_local_review/BOARD_EVIDENCE.json`에 있다.

살펴본 외부 압축 파일은 `C:/Users/leejh/Downloads/S32K344_AUDIO_BRINGUP_STUDY_TRACKER_v1.4.zip`이다. 프로젝트 경로 앞부분은 `S32K344_AUDIO_BRINGUP_STUDY_TRACKER_v1.4/S32K344_WB_Audio_Bringup/`이다. 압축 안의 파일은 메모리에서 읽었고, SDK 파일은 이 저장소에 복사하지 않았다.

- `.settings/com.freescale.s32ds.cross.sdk.support.prefs`는 Debug_FLASH/Debug_RAM에 `PlatformSDK_S32K3_S32K344_M7_7.0.1_PATH`를 선언한다. `RTD/include/Clock_Ip.h`의 소프트웨어 버전은 **7.0.1**이고, 핀 설정 파일과 생성된 시계 파일도 같은 버전을 가리킨다.
- `.cproject`는 S32DS K3 ARM GNU 10.2 toolchain 변수와 외부 PlatformSDK include 루트를 가리킨다. 이것은 프로젝트 메타데이터일 뿐, 그 toolchain/SDK가 설치되어 있거나 이 Network 빌드에 링크된다는 증거가 아니다.
- `board/Siul2_Port_Ip_Cfg.c:108`부터 SAI0(PTB2/PTC12/PTB3/PTC13), LPI2C0(PTD13/PTD14), GPIO, ADC 배정이 나온다. 이것은 오디오 보드의 증거이고, Network CAN/UART 핀이나 트랜시버 배선의 근거가 되지 않는다.
- `src/AudioClock.c`는 CS2100 오디오 기준 시계에 PIT0/channel0과 LPI2C0을 쓴다. Network ms 시각, 시각 wrap 처리, UART 바이트별 시각, Network의 STM0 소유의 근거가 되지 않는다.
- 압축 안의 파일 이름에 FlexCAN, LPUART, Can_Ip, Uart_Ip가 들어간 것은 없다. 오디오 프로젝트가 있다는 것만으로는 Network CAN/UART 드라이버를 구현하거나 검증할 수 없다.
- `docs/vss/design/90_BINDING/90_C_FILE_API_MAPPING.md`와 `91_IMPLEMENTATION_TBD.md`는 실제 하위 매핑, 생성 소스, IRQ/DMA 완료·수명, Node Communication 증거를 명시적으로 열어 둔다. 두 문서의 과거 오디오 증거는 Network G-08을 닫지 못한다.

| 압축 파일 / 파일 | SHA256 |
|---|---|
| `C:/Users/leejh/Downloads/S32K344_AUDIO_BRINGUP_STUDY_TRACKER_v1.4.zip` | `27a5cae866bc3a3a187db570b2d18501bcada57fb0b41d1fe7086068a3849836` |
| `S32K344_AUDIO_BRINGUP_STUDY_TRACKER_v1.4/S32K344_WB_Audio_Bringup/.settings/com.freescale.s32ds.cross.sdk.support.prefs` | `83a09b67e14020bde7fc64d60978c011cf12e85bd3470d0f1dd389834ddfe283` |
| `S32K344_AUDIO_BRINGUP_STUDY_TRACKER_v1.4/S32K344_WB_Audio_Bringup/RTD/include/Clock_Ip.h` | `a7fa41dc1d9ebfe0947c8eb67918f6e1d87eabcb3d0a7d6681ebe5f33a388e89` |
| `S32K344_AUDIO_BRINGUP_STUDY_TRACKER_v1.4/S32K344_WB_Audio_Bringup/generate/src/Clock_Ip_Cfg.c` | `209c571b393c6eeaab76807b0e1dc278527f0bec1c5c0431d9ff3f914cc11fcc` |
| `S32K344_AUDIO_BRINGUP_STUDY_TRACKER_v1.4/S32K344_WB_Audio_Bringup/board/Siul2_Port_Ip_Cfg.c` | `8f4649a9c33593aaa9c85a873c809f9115f8e3c0e2608988af1ae77a4d9fc933` |
| `S32K344_AUDIO_BRINGUP_STUDY_TRACKER_v1.4/S32K344_WB_Audio_Bringup/src/AudioClock.c` | `d57afb181633ef628f1b66887fd93974f95fff69bc0e5c8d5ecf640001df91ed` |
| `S32K344_AUDIO_BRINGUP_STUDY_TRACKER_v1.4/S32K344_WB_Audio_Bringup/src/AudioClock.h` | `d9c1c066e6403904152d6951af2b99d97722cab5808a2887a85ca81c56ab087c` |
| `S32K344_AUDIO_BRINGUP_STUDY_TRACKER_v1.4/S32K344_WB_Audio_Bringup/.cproject` | `6641231574dbe31fbe78e27949cfbf072e1dd64e17efcf98906174e2088a831e` |

범위를 정한 파일 이름 검색에서 접근할 수 있는 S32K144 전용 Network SDK/핀 프로젝트는 찾지 못했다. 이것은 정한 범위 안의 검색 결과이며, 컴퓨터 어디에도 SDK가 없다는 주장이 아니다. 읽기 전용 검색의 한계는 `_local_review/EVIDENCE_REVIEW.md`에 적었다.

## 실제 binding 전에 필요한 증거

보드마다 승인된 보드 revision과 회로도, CAN/UART 인스턴스와 핀 mux, 트랜시버 제어, 생성된 주변장치 설정, 정확한 SDK/toolchain, 시계 단위·속도와 wrap 규칙, ISR에 안전한 소유·동기화, 프레임/바이트 시각 출처, 상한이 있는 큐 크기를 확인한다. 재시작 뒤에도 콜백 식별이 유지된다는 것과, 드라이버 접수·최종 송신 결과·참조 해제가 서로 독립된 의미라는 것을 보인다. 그다음 대상 컴파일/링크, 물리 loopback/버스 시험, 오류·복구 시험을 한다. 이 단계 중 어느 것도 Host 시험에서 추정하지 않는다.

Round4 v1.4는 G-08을 `NOT_RUN`으로, 더 넓은 제품 gate를 OPEN/HOLD로 둔다. ID 11개 / 위치 32개와 출처 구분은 `_local_review/EVIDENCE_REVIEW.md`를 본다. 기존 DOMAIN v0.5/Vehicle v0.2 문서 헤더와 따로 있는 v0.6/v0.3 후보 헤더는 보드 binding도 아니고, 승인된 Network ABI 연동도 아니다.
