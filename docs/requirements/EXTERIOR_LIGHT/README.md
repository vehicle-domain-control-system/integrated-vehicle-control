# EXTERIOR_LIGHT

외부 조명 실행 기능의 기능 범위, 시스템 요구사항 및 상위 Domain 연계 기준을 관리한다.

담당 기능:

- 확정된 의미 기반 외부 조명 명령 처리
- 대표 외부 조명 채널의 켜짐, 꺼짐 및 선택적 출력 수준 실행
- 출력 상태와 명령 결과 제공
- 지원되는 하드웨어 범위의 출력 피드백 감시
- 통신·출력 구동·초기화 고장 처리

현재 기준은 저전압 데모 1채널이며, 별도 Function ECU 또는 BCM 확장 여부는 아키텍처 검토 후 확정한다. 기존 BCM 기준의 Ambient Lighting은 실내 조명이므로 본 모듈과 구분한다.

## 문서

- [`01_EXTERIOR_LIGHT_SR_FUNCTIONAL_BASELINE_DRAFT.md`](01_EXTERIOR_LIGHT_SR_FUNCTIONAL_BASELINE_DRAFT.md): 상위 시스템 관점의 기능 범위와 요구
- [`02_EXTERIOR_LIGHT_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md`](02_EXTERIOR_LIGHT_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md): 외부 조명 실행 기능의 기능·성능·안전·진단 요구

후속 작성 대상:

- `03_EXTERIOR_LIGHT_LOGICAL_INTERFACE_NEEDS_DRAFT.md`
- `04_EXTERIOR_LIGHT_SR_SYSRS_TRACE_NAVIGATION_DRAFT.md`

자동 점등 임계값과 차량 수준 우선순위는 Domain 책임으로 유지하며, 네트워크·전기·법규 상세는 후속 검토에서 확정한다.
