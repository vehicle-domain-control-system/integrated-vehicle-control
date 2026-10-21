package com.vdcs.mobile.ui.rules

import com.vdcs.mobile.domain.repository.InvalidRequest
import com.vdcs.mobile.domain.repository.PermissionMissing
import com.vdcs.mobile.domain.repository.RequestBlocked
import com.vdcs.mobile.domain.repository.RequestNotSent

object ActionMessages {
    fun of(e: Exception): String = when (e) {
        is RequestBlocked -> blockedMessage(e.message)
        is RequestNotSent -> notSentMessage(e.message)
        is InvalidRequest -> "요청 값 오류 — ${e.message ?: "범위를 확인하세요"}"
        is PermissionMissing -> "블루투스 권한이 없습니다 — 설정에서 허용하세요"
        else -> "처리하지 못했습니다 — ${e.message ?: e.javaClass.simpleName}"
    }

    fun blockedMessage(reason: String?): String = "요청을 보낼 수 없습니다 — ${reason ?: "차량 준비 안 됨"}"

    fun notSentMessage(reason: String?): String =
        "차량에 전달되지 않았습니다 — ${reason ?: "전송 실패"} (요청 이력에 남기지 않음)"
}
