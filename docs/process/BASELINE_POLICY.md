# Repository Baseline Policy

## 1. 신규 Repository 원칙

본 Repository는 기존 프로젝트 Repository의 연장이 아니라 **새 프로젝트 Baseline**으로 시작한다.

기존 Repository의 다음 항목은 자동 승계하지 않는다.

- 기존 Branch
- 기존 Commit History
- 기존 Issue / Pull Request
- 기존 SR / SysRS
- 기존 코드
- 기존 Architecture / Network 정의

필요한 내용은 참고 자료로 검토한 뒤 현재 팀이 다시 승인하여 반영한다.

## 2. 기존 자료 사용 원칙

기존 자료는 `Reference Only`로 취급한다.

새 Repository에 가져올 때는 다음을 확인한다.

1. 현재 프로젝트 범위에 여전히 필요한가?
2. 현재 팀이 이해하고 리뷰할 수 있는가?
3. 다른 요구사항 또는 구조와 충돌하지 않는가?
4. 구현 가능성이 있는가?
5. 누가 담당하는가?

반영이 결정되면 일반 작업과 동일하게 Issue → Branch → PR → Review → Merge 절차를 따른다.

## 3. SR 관리

SR 변경은 다음 세 유형으로 관리한다.

```text
ADD
UPDATE
DELETE
```

변경 시 최소한 다음을 남긴다.

- 대상
- 현재 내용
- 변경 내용
- 변경 이유
- 영향 범위
- 완료 조건

## 4. main 기준

`main`은 현재 팀이 검토하고 승인한 Baseline만 포함한다.

초안 작성, 실험, 검토 중 작업은 각 Issue Branch에서 진행한다.

## 5. 초기 우선순위

프로젝트 재구성 초기에는 다음 순서로 Baseline을 만든다.

1. Repository 운영 규칙
2. SR
3. System Architecture
4. Communication / Network
5. VSS
6. 각 기능별 상세 설계 및 구현

필요 시 팀 논의에 따라 조정한다.
