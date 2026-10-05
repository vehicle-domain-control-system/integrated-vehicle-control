# Gateway_Types 품질 타입 수정 설명 v0.2

이번 Proximity 구현에서 SysRS와의 표현 정합성을 위해
`Gateway_DataQuality_t`에 다음 값을 추가했다.

```c
GATEWAY_DATA_QUALITY_STALE
```

기존:

```text
UNKNOWN
VALID
INVALID
NO_DATA
```

수정:

```text
UNKNOWN
VALID
INVALID
NO_DATA
STALE
```

이유:

SysRS는 `STALE`, `INVALID`, `NO_DATA`를 서로 다른 정보 품질 상태로 구분하며,
특히 Digital Key 새 접근 판단에서 `STALE`을 유효한 비근접(`FAR`) 근거로
사용하지 않도록 요구한다.

따라서 Proximity expiry를 `NO_DATA`로 뭉개지 않고:

```text
state   = UNKNOWN
quality = STALE
```

로 표현한다.

중요:

`STALE`은 `FAR`가 아니다.

또한 STALE 판정 시 마지막 실제 관측 시각을 갱신하지 않으므로
원본 Age가 재전송/재평가 때문에 초기화되지 않는다.
