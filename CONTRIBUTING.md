# Contributing Guide

본 문서는 프로젝트에서 반드시 지켜야 할 최소 GitHub 협업 규칙만 정리한다.

상세 사용 방법은 `docs/process/GITHUB_WORKFLOW.md`를 참고한다.

## 기본 규칙

1. `main` 브랜치에서 직접 작업하지 않는다.
2. 작업 시작 전에 GitHub Issue를 생성한다.
3. 기본적으로 `1 Issue = 1 Branch = 1 Pull Request` 구조를 사용한다.
4. SR 변경은 `ADD / UPDATE / DELETE` 중 하나를 명시한다.
5. Pull Request는 최소 1명의 리뷰 후 Merge한다.
6. 서로 관계없는 변경은 하나의 Pull Request에 섞지 않는다.
7. Commit 전 `git status`, `git diff`를 확인한다.
8. Merge는 기본적으로 `Squash and merge`를 사용한다.

## Branch Convention

```text
docs/<issue번호>-<간단한내용>
feat/<issue번호>-<간단한내용>
fix/<issue번호>-<간단한내용>
```

예:

```text
docs/12-sr-antipinch-update
feat/23-can-window-status
fix/31-uart-timeout
```

## Commit Convention

```text
docs: 내용
feat: 내용
fix: 내용
```

## 기본 작업 흐름

```text
Issue
  ↓
Branch
  ↓
작업
  ↓
Commit
  ↓
Push
  ↓
Pull Request
  ↓
Review
  ↓
Merge
```

## SR 작성 원칙

SR은 시스템이 **무엇을 해야 하는가(What)** 를 표현한다.

구체적인 구현 방법인 **어떻게 구현하는가(How)** 는 SR에 과도하게 포함하지 않는다.

GitHub Issue 번호와 요구사항 문서 내부의 식별 체계는 별개의 개념으로 관리한다.
