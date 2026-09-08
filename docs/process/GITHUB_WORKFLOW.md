# GitHub 협업 가이드 초안 v0.1

> **목적**  
> 본 문서는 Git과 GitHub 사용 경험이 많지 않은 팀원도 동일한 방식으로 작업할 수 있도록,
> 프로젝트의 기본 협업 규칙과 작업 흐름을 간단하게 정리한 초안이다.
>
> 이번 프로젝트는 기존 SR을 기반으로 요구사항을 **추가(ADD) / 수정(UPDATE) / 삭제(DELETE)** 하며 발전시키는 형태이므로,
> 단순히 결과 파일만 관리하는 것이 아니라 **누가, 왜, 무엇을 변경했는지 추적할 수 있는 구조**를 만드는 것을 목표로 한다.

---

## 1. 기본 운영 원칙

프로젝트에서는 아래 규칙만 우선 공통으로 적용한다.

1. `main` 브랜치에서 직접 작업하지 않는다.
2. 작업을 시작하기 전에 GitHub Issue를 생성한다.
3. 기본적으로 `1 Issue = 1 Branch = 1 Pull Request` 구조를 사용한다.
4. SR 변경 시 `ADD / UPDATE / DELETE` 중 하나를 명시한다.
5. Pull Request는 최소 1명의 리뷰 후 Merge한다.
6. 서로 관계없는 작업은 하나의 Pull Request에 섞지 않는다.
7. Merge 전에는 변경 파일과 `git diff`를 한 번 확인한다.

전체 흐름은 다음과 같다.

```text
Issue 생성
   ↓
Branch 생성
   ↓
작업
   ↓
Commit
   ↓
Push
   ↓
Pull Request 생성
   ↓
Review
   ↓
Merge
   ↓
Issue Close
```

---

# 2. Issue 사용 기준

Issue는 단순한 할 일 목록이 아니라,

- 무엇을 변경하는지
- 왜 변경하는지
- 어떤 부분에 영향이 있는지

를 남기는 기록으로 사용한다.

## 2.1 Issue 제목 기본 형식

### SR 변경

```text
[SR][ADD] 기능명
[SR][UPDATE] 기능명
[SR][DELETE] 기능명
```

예시:

```text
[SR][ADD] 잔류 탑승자 경고 요구사항 추가
[SR][UPDATE] 파워윈도우 안티핀치 요구사항 보완
[SR][DELETE] 자동 하이빔 요구사항 삭제
```

### 기능 개발

```text
[FEAT] 기능명
```

예시:

```text
[FEAT] CAN 파워윈도우 상태 수신 구현
```

### 오류 수정

```text
[FIX] 오류 내용
```

예시:

```text
[FIX] UART 수신 타임아웃 처리 오류 수정
```

---

# 3. SR 변경 Issue 기본 양식

```markdown
## 작업 종류

- [ ] ADD
- [ ] UPDATE
- [ ] DELETE

## 대상

파일:

기능:

## 현재 내용

현재 요구사항 또는 현재 상태를 작성합니다.

## 변경 내용

추가 / 수정 / 삭제할 내용을 작성합니다.

## 변경 이유

변경이 필요한 이유를 작성합니다.

예:
- 팀 회의 결정
- 멘토 피드백
- 구현 가능성 검토
- 요구사항 충돌
- 프로젝트 범위 변경

## 영향 범위

- [ ] SR
- [ ] SysRS
- [ ] Network
- [ ] Hardware
- [ ] Software
- [ ] Test
- [ ] 영향 없음
- [ ] 추가 확인 필요

## 완료 조건

- [ ] 대상 내용 수정
- [ ] 관련 문서 영향 확인
- [ ] 팀원 리뷰 완료
```

---

# 4. SR 변경 Issue 작성 예시

### Issue 제목

```text
[SR][UPDATE] 파워윈도우 안티핀치 요구사항 보완
```

### Issue 본문

```markdown
## 작업 종류

- [ ] ADD
- [x] UPDATE
- [ ] DELETE

## 대상

파일:
docs/requirements/SR.md

기능:
Power Window / Anti-Pinch

## 현재 내용

파워윈도우는 장애물이 감지되면 반전해야 한다.

## 변경 내용

파워윈도우는 끼임이 발생할 경우 창문을 열림 방향으로 전환하여
탑승자의 상해를 방지해야 한다.

관련 법규 요구사항을 만족해야 한다.

## 변경 이유

기존 요구사항에서는 끼임 발생 시 최종 동작과
안전 요구사항이 충분히 표현되지 않았다.

멘토링 내용을 반영하여 요구 기능을 보다 명확하게 작성한다.

## 영향 범위

- [x] SR
- [x] SysRS
- [ ] Network
- [x] Hardware
- [x] Software
- [x] Test

## 완료 조건

- [x] SR 수정
- [ ] SysRS 영향 확인
- [ ] 팀원 리뷰 완료
```

---

# 5. SR 작성 시 기본 원칙

SR은 기본적으로 **무엇을 해야 하는가(What)** 를 표현한다.

구체적인 구현 방법인 **어떻게 구현하는가(How)** 는
SysRS 또는 이후 설계 단계에서 다룬다.

예를 들어 다음과 같은 문장은 SR에서 가급적 피한다.

```text
전류 센서를 ADC로 읽어 특정 Threshold 이상이면
PWM Duty를 반전한다.
```

위 문장은 ADC, Threshold, PWM 등 구현 방법을 포함하고 있다.

SR에서는 다음과 같이 시스템이 제공해야 하는 기능에 집중한다.

```text
파워윈도우는 끼임이 발생한 경우
탑승자의 상해를 방지하기 위해 열림 방향으로 전환해야 한다.
```

> GitHub Issue 번호와 SR/SysRS 요구사항 ID는 서로 다른 개념이다.  
> Issue 번호는 GitHub에서 작업 변경 이력을 관리하기 위한 번호로 사용한다.

---

# 6. Branch Convention

브랜치 종류는 초기에는 복잡하게 늘리지 않고 다음 세 가지를 기본으로 한다.

```text
docs/<issue번호>-<간단한내용>
feat/<issue번호>-<간단한내용>
fix/<issue번호>-<간단한내용>
```

예시:

```text
docs/12-sr-antipinch-update
feat/23-can-window-status
fix/31-uart-timeout
```

| Prefix | 용도 |
|---|---|
| `docs` | SR, SysRS, 설계 문서, README 등 문서 변경 |
| `feat` | 새로운 기능 구현 |
| `fix` | 오류 또는 버그 수정 |

추후 필요할 경우 다음 항목을 추가할 수 있다.

```text
test/
refactor/
chore/
```

하지만 프로젝트 초반에는 불필요하게 종류를 늘리지 않는다.

---

# 7. Commit Convention

Commit 메시지는 아래 세 가지를 기본으로 사용한다.

```text
docs: 내용
feat: 내용
fix: 내용
```

예시:

```text
docs: 파워윈도우 안티핀치 SR 수정
feat: CAN 파워윈도우 상태 수신 기능 추가
fix: UART 수신 타임아웃 처리 수정
```

## Commit 작성 기준

좋은 예:

```text
feat: CAN 수신 메시지 파싱 기능 추가
```

피하는 예:

```text
수정
작업함
최종
진짜최종
test
```

Commit 메시지만 보더라도 변경 목적을 대략 이해할 수 있도록 작성한다.

---

# 8. Pull Request 기본 양식

```markdown
## 관련 Issue

Closes #

## 변경 내용

이번 PR에서 변경한 내용을 간단히 작성합니다.

## 변경 파일

-
-

## 확인 사항

- [ ] Build 확인
- [ ] 문서 내용 확인
- [ ] 관련 요구사항 영향 확인
- [ ] 불필요한 파일이 포함되지 않았음

## Reviewer 확인 요청

특히 확인이 필요한 부분을 작성합니다.

없으면 "없음"으로 작성합니다.
```

문서만 수정한 경우 Build 항목은 확인하지 않아도 된다.

예:

```text
- [ ] Build 확인
- [x] 문서 내용 확인
```

---

# 9. Pull Request 작성 예시

### PR 제목

```text
[SR] 파워윈도우 안티핀치 요구사항 보완
```

### PR 본문

```markdown
## 관련 Issue

Closes #12

## 변경 내용

파워윈도우 안티핀치 SR을 보완했습니다.

기존 요구사항에서 명확하지 않았던
끼임 발생 시 최종 동작과 안전 요구사항을 추가했습니다.

## 변경 파일

- docs/requirements/SR.md

## 확인 사항

- [ ] Build 확인
- [x] 문서 내용 확인
- [x] 관련 요구사항 영향 확인
- [x] 불필요한 파일이 포함되지 않았음

## Reviewer 확인 요청

SR에 구현 방법이 포함되지 않았는지 확인 부탁드립니다.

관련 SysRS에 추가적인 영향 분석이 필요한지도 확인 부탁드립니다.
```

`Closes #12`와 같이 작성하면 PR이 Merge될 때 관련 Issue를 함께 종료할 수 있다.

---

# 10. 실제 작업 과정 예시

Issue `#12`의 파워윈도우 SR을 수정한다고 가정한다.

## Step 1. main 최신화

```bash
git switch main
git pull origin main
```

---

## Step 2. 작업 Branch 생성

```bash
git switch -c docs/12-sr-antipinch-update
```

현재 구조:

```text
main
 └── docs/12-sr-antipinch-update
```

---

## Step 3. 파일 수정

예:

```text
docs/requirements/SR.md
```

필요한 내용을 수정한다.

---

## Step 4. 변경 상태 확인

```bash
git status
```

그리고 실제 변경 내용을 확인한다.

```bash
git diff
```

`git diff`는 Commit 전에 반드시 한 번 확인하는 것을 권장한다.

---

## Step 5. 변경 파일 추가

```bash
git add docs/requirements/SR.md
```

---

## Step 6. Commit

```bash
git commit -m "docs: 파워윈도우 안티핀치 SR 수정"
```

---

## Step 7. 원격 Repository에 Push

```bash
git push -u origin docs/12-sr-antipinch-update
```

---

## Step 8. Pull Request 생성

GitHub에서 다음 방향으로 PR을 생성한다.

```text
docs/12-sr-antipinch-update
        ↓
       main
```

PR 본문에는 관련 Issue를 연결한다.

```text
Closes #12
```

---

## Step 9. 팀원 Review

SR 변경의 경우 Reviewer는 우선 아래 내용을 확인한다.

- 요구사항의 의미가 명확한가?
- 기존 요구사항과 충돌하지 않는가?
- 구현 방법이 SR에 과도하게 포함되어 있지 않은가?
- 다른 기능 또는 문서에 영향이 없는가?
- 삭제된 내용이 있다면 삭제 이유가 기록되어 있는가?

코드 변경의 경우에는 최소한 아래 내용을 확인한다.

- Build가 가능한가?
- 입력 → 처리 → 출력 흐름을 설명할 수 있는가?
- 불필요한 blocking 또는 무한 대기가 없는가?
- timeout / error 상황이 고려되어 있는가?
- 변경하지 않아도 되는 파일까지 함께 수정되지 않았는가?

---

## Step 10. Merge

Review가 완료되면 Merge한다.

초기 프로젝트에서는 `Squash and merge` 사용을 권장한다.

여러 개의 작은 Commit이 있더라도 `main`에는 하나의 작업 단위로 정리되어
전체 이력을 확인하기 쉬워진다.

---

## Step 11. 로컬 Branch 정리

```bash
git switch main
git pull origin main
git branch -d docs/12-sr-antipinch-update
```

작업이 완료된다.

---

# 11. 기능 개발도 동일한 흐름 사용

예를 들어 CAN 파워윈도우 상태 수신 기능을 구현한다고 가정한다.

### Issue

```text
#23
[FEAT] CAN 파워윈도우 상태 수신 구현
```

### Branch

```text
feat/23-can-window-status
```

### Commit

```text
feat: CAN 파워윈도우 상태 수신 추가
```

### Pull Request

```text
[CAN] 파워윈도우 상태 수신 구현
```

즉, 문서 작업과 코드 작업 모두 동일한 기본 흐름을 사용한다.

```text
Issue
 ↓
Branch
 ↓
Commit
 ↓
Pull Request
 ↓
Review
 ↓
Merge
```

---

# 12. 서로 다른 작업은 분리한다

다음과 같은 변경이 동시에 발생했다고 가정한다.

```text
파워윈도우 SR 수정
CAN 코드 수정
README 구조 정리
```

세 변경이 서로 직접적인 관계가 없다면 하나의 PR에 모두 넣지 않는다.

권장:

```text
Issue #12 → SR 수정 → PR #15

Issue #13 → CAN 코드 수정 → PR #16

Issue #14 → README 수정 → PR #17
```

이렇게 분리하면 이후 문제가 발생했을 때 어떤 변경에서 문제가 발생했는지 찾기 쉽다.

---

# 13. main Branch 관리

가능하면 `main`에는 직접 Push하지 않는다.

권장 설정:

```text
main Branch Protection

- Pull Request를 통해서만 Merge
- 최소 1명 Approval
- Review conversation 해결 후 Merge
- Force Push 금지
```

프로젝트 초반에는 CI, 복잡한 자동화, CODEOWNERS 등을 필수로 적용하지 않는다.

필요한 시점에 단계적으로 추가한다.

---

# 14. 권장 Repository 관리 파일

GitHub 협업 규칙은 Repository 안에 함께 보관하는 것을 권장한다.

예시:

```text
.github/
├── ISSUE_TEMPLATE/
│   ├── sr_change.md
│   ├── feature.md
│   └── bug.md
│
└── pull_request_template.md

docs/
└── process/
    └── GITHUB_WORKFLOW.md

CONTRIBUTING.md
```

역할:

| 파일 | 역할 |
|---|---|
| `CONTRIBUTING.md` | 프로젝트의 핵심 GitHub 협업 규칙 |
| `sr_change.md` | SR 추가/수정/삭제 Issue 양식 |
| `feature.md` | 기능 개발 Issue 양식 |
| `bug.md` | 오류 수정 Issue 양식 |
| `pull_request_template.md` | Pull Request 공통 양식 |
| `GITHUB_WORKFLOW.md` | Issue부터 Merge까지의 전체 사용 방법 |

---

# 15. 팀원이 최소한 기억해야 할 내용

처음부터 모든 Git 명령어나 GitHub 기능을 외울 필요는 없다.

우선 아래 흐름만 익히면 된다.

```text
1. Issue를 만든다.
2. main을 최신 상태로 만든다.
3. Issue용 Branch를 만든다.
4. 작업한다.
5. git status / git diff로 변경 내용을 확인한다.
6. Commit한다.
7. Push한다.
8. Pull Request를 만든다.
9. 팀원에게 Review를 받는다.
10. Merge한다.
```

그리고 프로젝트 진행 중 가장 중요하게 지켜야 하는 기준은 다음 세 가지다.

> **변경 이유를 남긴다.**  
> **관련 없는 작업을 한 번에 섞지 않는다.**  
> **main에는 Review된 결과만 반영한다.**

---

# 16. 프로젝트 진행 중 확장 가능한 항목

본 문서는 프로젝트 시작 단계의 기본 규칙을 정의한 초안이다.

프로젝트가 진행되면서 필요할 경우 다음 항목을 추가할 수 있다.

- Commit Convention 세분화
- Issue Label 규칙
- Milestone 관리
- CODEOWNERS
- 자동 Build / Test
- CI
- Release / Tag Convention
- 요구사항 Traceability 관리
- 변경 이력(Change Trace) 관리
- 문서 Version 관리

다만 프로젝트 시작 단계에서는 관리 규칙 자체가 개발보다 복잡해지지 않도록
필요한 항목부터 단계적으로 적용한다.

---

# Appendix A. 자주 사용하는 Git 명령어

```bash
# 현재 상태 확인
git status

# 변경 내용 확인
git diff

# main 이동
git switch main

# 최신 main 가져오기
git pull origin main

# 새 Branch 생성
git switch -c feat/23-example

# 특정 파일 Stage
git add <파일>

# Commit
git commit -m "feat: 기능 설명"

# Push
git push -u origin feat/23-example

# 작업 종료 후 main으로 복귀
git switch main

# Merge된 로컬 Branch 삭제
git branch -d feat/23-example
```

---

# Appendix B. 전체 흐름 요약

```text
┌──────────────────────┐
│      GitHub Issue    │
│ 무엇을 / 왜 변경?   │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│        Branch        │
│ 실제 변경 작업 분리 │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│        Commit        │
│ 변경 단위 기록      │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│     Pull Request     │
│ 변경 내용 / 영향    │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│        Review        │
│ 팀원 확인           │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│         main         │
│ 검토 완료 결과 반영 │
└──────────────────────┘
```

---

**문서 상태:** Draft  
**Version:** v0.1  
**용도:** 프로젝트 초기 GitHub 협업 규칙 논의 및 팀 공유
