# 노드·링크와 적용 범위

문서에 기재된 역할을 기준으로 Network 책임을 나눈다. 보드명은 연결 설계의 기준이며 실제 배선·펌웨어 배포 상태를 증명하지 않는다.

| 노드 | 문서상 보드/역할 | Network 경계 | 외부 기능 책임 |
|---|---|---|---|
| N01 DOMAIN | S32K344 | CAN·UART-M 검증, C2 인계, 중계·조회 감독 | 공개 DomainIf 본체, Manager, 차량 정책·위험 판단 |
| N02 BCM | S32K144 | CAN 명령 수신과 상태·결과 송신 | 도어·공조·실내등 실행·로컬 보호 |
| N03 CIS | S32K144 | CAN·UART-C, 센서 관측 송신·Pi 자료 중계 | 환경·후방 측정, 원 관측 번호·품질 생산 |
| N04 VSS | DOMAIN과 별도 S32K344 | CAN 경고·사건 수신과 상태 송신 | 음향 중재·실제 출력 |
| N05 ESP32 | Gateway | UART-M와 BLE 의미 전달 경계 | ESP32 Network 구현, peer·Session·BLE 처리 |
| N06 Pi | 카메라 서비스 | UART-C 판정·상태·Permission 경계 | 영상 처리와 원 비전 판정 |
| N07 WINDOW | 예정 S32K144 | CAN 설계 보존, 실행·표시·감시 보류 | 이동·STOP·끼임 방지·실제 결과 |
| N08 MOBILE | 사용자 App | Gateway 외부 메시지 경계만 참조 | 요청·활성 근거·화면 표시 |

## 물리 통신과 논리 경로

| 링크 | 연결 | 방향과 의미 | 현재 상세도 |
|---|---|---|---|
| P01 CAN FD | DOMAIN·BCM·CIS·VSS·WINDOW 공유 버스 | DOMAIN 명령/허용·경고, 각 ECU 상태/결과 | [CAN22/Rx25 상세](04_CAN_FD_OVERVIEW.md); WINDOW는 기능 보류 |
| P02 UART-M | ESP32↔DOMAIN | 요청·문맥·조회·읽음 / 차량 상태·결과·경고 | [UART-M23/Rx23 상세](05_UART_M_OVERVIEW.md); VERSION2 후보·Window2 실행 HOLD |
| P03 UART-C | Pi↔CIS | 비전 판정·상태 / DOMAIN 원 허용의 제한 중계 | [UART-C3/Rx3 상세](06_UART_C_OVERVIEW.md); 양단 후보·품질/허용 HOLD |
| P04 BLE | MOBILE↔ESP32 | 사용자 요청 및 차량 표시 자료 | ESP32/MOBILE 외부 담당 경계 |

R01(DOMAIN↔BCM), R02(DOMAIN↔CIS), R03(DOMAIN↔VSS), R04(DOMAIN↔WINDOW)는 P01 위 논리 관계다. R05는 MOBILE–ESP32–DOMAIN의 BLE+UART-M, R06은 Pi–CIS–DOMAIN의 UART-C+CAN 경로다. I01~I07은 각각 DOMAIN·BCM·CIS·VSS·ESP32·Pi·WINDOW의 노드 내부 typed 경계다. 이17개 링크 식별자를17개 물리 선로로 해석하지 않는다.

## 노드별 Layer 적용

DOMAIN은 L01 원본 연결과 조회 감독, L02 C2 호출, L03~L05 CAN/UART-M 처리의 접점이다. CIS는 L01 원 관측·Pi 중계, L02 센서·Permission typed 경계, L03~L05 CAN/UART-C를 사용한다. BCM·VSS는 CAN 관련 L02~L05를 적용하며 기능 판단은 각 노드의 Application 담당 책임으로 남긴다. ESP32·Pi의 L02~L05는 상대 담당자 구현의 계약을 나타내며 S32K 공통 구현을 강제하지 않는다. WINDOW의 적용표는 재개 전까지 설계만 유지한다. MOBILE 내부에는 이 Layer 구성을 적용하지 않는다.

기존 대표 경로의 시작은 N03 후방 관측이고 끝은 N01의 공개 Rx 입력이다. CAN 22종은 [메시지별 지도](04_CAN_FD_OVERVIEW.md#messages)로 확장하되 각 양단의 미결을 유지한다. DOMAIN 위험 판단→VSS 경고의 CF02 형식은 A1 WARNING60으로 선택했으며 실제 DOMAIN producer/VSS receiver·시간·ABI 지원은 검증 대기다.

[Network](01_NETWORK_OVERVIEW.md) · [CIS_REAR 전체 흐름](flows/CIS_REAR_144_TO_344.md)

P02의 상세 경로는 [상향4](flows/UART_M_UPLINK.md), [하향17](flows/UART_M_DOWNLINK.md), [Query/private 감독](flows/UART_M_QUERY_AND_PROJECTION.md)으로 연결한다. P03 UART-C3/Rx3은 [양방향 상세](06_UART_C_OVERVIEW.md)로 연결한다. 공유 CAN의22/Rx25와 UART-M23/Rx23을 더해45/Rx48이며 UART-C3/Rx3을 더한 전체가48/Rx51이다. 두 UART의 version/registry는 독립이다.

## 노드 경계 상세로 이동

[8 Node·17 Link 전체 적용표](08_NODE_BOUNDARIES.md)에서 물리4·논리6·내부7의 연결을 확인한다. [DOMAIN](nodes/DOMAIN.md), [BCM](nodes/BCM.md), [CIS/Pi](nodes/CIS_PI.md), [VSS](nodes/VSS.md), [ESP32/MOBILE](nodes/ESP32_MOBILE.md), [WINDOW](nodes/WINDOW.md)는 실제 기존 선언·자료·호출·수명·정상/실패 경계를 설명한다. 이 연결로 Board 실행·양단 후보 채택·외부 HOLD를 확정하지 않는다.
