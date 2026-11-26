# CIS_REAR — 기존 수직 흐름과 CAN 공통 설계의 연결

[Network](../../00_START_HERE.md) · [M03](../../modules/M03_TYPED_HANDOFF.md) · [M05](../../modules/M05_CAN_CODEC.md) · [노드별 흐름](../../flows/CAN_CIS_WINDOW_VSS.md#CIS_REAR)

`WIRE-P01-CIS_REAR` / `RX-P01-CIS_REAR-DOMAIN`. CIS→DOMAIN, 후보 ID0x0A0 / used19B / frame20B / DLC11 / B19 padding0. 기존 B11의 아래 여덟 파일이 이 경로의 단일 정본이며 같은 자료·함수 본체를 재정의하지 않는다.

| 책임 | 기존 정본 |
|---|---|
| NetCisRear/CisRearState_t/NetCanSource/NetCanRxMeta, 필드 전체와 단위 | [CIS_REAR_DATA](../../data/CIS_REAR_DATA.md) |
| 정확한 CisNet_PublishRear 원형·안정 사본·네 반환 | [TX Submit](../CIS_REAR_TX_SUBMIT.md) |
| NetRear_Dispatch와 원 관측 예약·Driver 제출 | [TX Dispatch](../CIS_REAR_TX_DISPATCH.md) |
| NetRear_Encode/Decode, 모든 offset·flags·길이·padding | [Codec](../CIS_REAR_CODEC.md) |
| NetRear_ReceiveFrame/Admit, 정상/중복·순서·원자 반영 | [RX Admission](../CIS_REAR_RX_ADMISSION.md) |
| NetRear_Deliver와 정확한 DomainIf_RxCisRear 호출 | [DOMAIN Ingress](../DOMAIN_CIS_REAR_INGRESS.md) |
| NetRear_ObservePort, 최종 결과·참조 해제·bus-off | [Port Recovery](../CAN_PORT_RECOVERY.md) |
| 양단 정상·경계·실패 전체 흐름 | [144→344 Flow](../../flows/CIS_REAR_144_TO_344.md) |

CAN 전체 경로가 메시지 종류를 판별하면 Rear 경로는 위 B11 함수로 이어진다. B12 `NetCanFrame`과 B11 `NetRearFrame`을 pointer cast하지 않는다. 통합 단계에서 필요하면 실제 ID/flags/DLC/20B/수신시각을 필드별로 보존하는 Network 사본이 필요하며 형식·참조 수명 검증 전 공통 프레임을 B11 구조체로 해석하지 않는다. 같은 수신에 B11 Admission과 B12 공통 Admission을 두 번 적용하지 않는다. Rear는 B11 한 경로에서 원본·순번·AGE를 관리하고 공통 문서는 동등한 경계 설명으로 연결한다.

유효 raw123은 정확히123mm다. raw65535는 실제 거리로 사용하지 않고 validity/reason/proximity_status를 함께 보존한다. NO_OBJECT와 FAULT/UNAVAILABLE은 서로 다르다. 같은 update_sequence의 의미 필드 비교는 거리·품질·status에 대한 것이며 SOURCE_AGE는 같은 소비시점으로 환산하는 별도 시간 근거다. 이 연결 설명은 B11 문장·판정의 변경이 아니다.

`CisNet_PublishRear`의 OK는 전체 로컬 사본, `NetRear_Decode`의 OK는 형식 변환, `DomainIf_RxCisRear`의 OK는 로컬 C2 호출이다. 서로를 Driver TX·Manager commit·VSS 완료로 확장하지 않는다. 원 CIS 세대/BOOT 대응, 품질 투영, 실제 시간원·Driver 수명·DOMAIN 소비 바인딩은 기존 HOLD다. [공통 전송 경계](../CAN_TRANSFER_CONTROL.md)와 [전체 CAN Flow](../../flows/CAN_NETWORK_PATHS.md)의 복구는 이 HOLD를 해제하지 않는다.
