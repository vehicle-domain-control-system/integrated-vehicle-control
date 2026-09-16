# WINDOW

Window ECU의 기능 범위, 시스템 요구사항 및 상위 Domain 연계 기준을 관리한다.

담당 기능:

- 로컬 스위치와 확정된 Domain 창문 명령 처리
- 열림, 닫힘, 정지 및 목표 위치 이동
- 위치·끝단 상태 제공
- 끼임 감지 시 로컬 정지와 안전 반전
- 명령 결과, 이벤트 및 고장 제공

## 문서

- [`01_WINDOW_SR_FUNCTIONAL_BASELINE_DRAFT.md`](01_WINDOW_SR_FUNCTIONAL_BASELINE_DRAFT.md): 상위 시스템 관점의 기능 범위와 요구
- [`02_WINDOW_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md`](02_WINDOW_SYSRS_FUNCTIONAL_BASELINE_DRAFT.md): Window ECU의 기능·성능·안전·진단 요구

후속 작성 대상:

- `03_WINDOW_LOGICAL_INTERFACE_NEEDS_DRAFT.md`
- `04_WINDOW_SR_SYSRS_TRACE_NAVIGATION_DRAFT.md`

구체적인 네트워크와 모터·센서·기구 수치는 인터페이스 및 Element/SW/HW 검토 이후 확정한다.
