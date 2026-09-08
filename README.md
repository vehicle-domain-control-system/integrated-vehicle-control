# Vehicle Domain Control System

차량 도메인 제어 시스템 프로젝트의 신규 Repository Baseline이다.

## 운영 방향

- 본 Repository는 기존 프로젝트 Repository를 그대로 이관하지 않고 **새 기준으로 처음부터 구성**한다.
- 기존 Repository와 기존 문서는 **참고 자료(Reference Only)** 로 사용한다.
- 새 Repository에 반영되는 내용은 현재 팀이 다시 검토한 뒤 Issue / Pull Request를 통해 반영한다.
- `main` 브랜치는 검토가 끝난 결과만 유지한다.
- 기본 협업 흐름은 `Issue → Branch → Commit → Pull Request → Review → Merge`를 사용한다.

## 초기 문서 구조

```text
.
├── .github/
│   ├── ISSUE_TEMPLATE/
│   └── pull_request_template.md
├── docs/
│   ├── requirements/
│   ├── architecture/
│   ├── network/
│   ├── vss/
│   └── process/
├── firmware/
├── hardware/
├── tools/
├── CONTRIBUTING.md
└── README.md
```

## 현재 우선 관리 영역

초기 프로젝트 재정비 과정에서 다음 영역을 우선 관리한다.

- SR
- Communication / Network
- VSS

다른 기능 영역과 담당자는 팀 논의 후 확정한다.

## 문서 위치

- SR: `docs/requirements/SR.md`
- Architecture: `docs/architecture/`
- Network / Protocol: `docs/network/`
- VSS: `docs/vss/`
- GitHub 협업 규칙: `docs/process/GITHUB_WORKFLOW.md`
- Baseline 운영 원칙: `docs/process/BASELINE_POLICY.md`

## 상태

- Repository Baseline: `v0.1`
- SR: Draft / 작성 전
- Architecture: Draft / 작성 전
- Network: Draft / 작성 전
- VSS: Draft / 작성 전
