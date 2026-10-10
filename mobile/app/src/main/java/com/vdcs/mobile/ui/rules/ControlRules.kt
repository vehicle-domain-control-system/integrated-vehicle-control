package com.vdcs.mobile.ui.rules

import com.vdcs.mobile.domain.model.Availability
import com.vdcs.mobile.domain.model.ConnectionState
import com.vdcs.mobile.domain.model.FunctionStatus

object ControlRules {
    private const val AVAILABILITY_UNCONFIRMED = "가용성 미확인"
    const val AVAILABILITY_STALE = "$AVAILABILITY_UNCONFIRMED — 최신 아님"
    private const val AVAILABLE_TEXT = "사용 가능"

    private const val NO_AVAILABILITY_TEXT = "정보 없음"
    private val NO_AVAILABILITY_VALUE = ValueText(NO_AVAILABILITY_TEXT, null, shown = false)

    fun connectBlockReason(connection: ConnectionState): String? =
        if (connection == ConnectionState.DISCONNECTED) null else connection.label

    const val DISCONNECT_BLOCKED = "연결되어 있지 않아 해제할 것이 없습니다"

    fun disconnectBlockReason(connection: ConnectionState): String? =
        if (connection == ConnectionState.DISCONNECTED) DISCONNECT_BLOCKED else null

    fun controlBlockReason(connection: ConnectionState, connectionDetail: String?, function: FunctionStatus?): String? =
        if (connection != ConnectionState.AUTHENTICATED) {
            "차량 인증 완료 전에는 제어할 수 없습니다" + (connectionDetail?.let { " — $it" } ?: "")
        } else {
            availabilityBlockReason(function)
        }

    fun ackBlockReason(connection: ConnectionState): String? =
        if (connection != ConnectionState.AUTHENTICATED) "차량 인증 완료 후 읽음 처리할 수 있습니다" else null

    fun ackAllBlockReason(connection: ConnectionState, targets: AckTargets): String? =
        if (targets.current.isEmpty()) null else ackBlockReason(connection)

    fun resendBlockReason(connection: ConnectionState, function: FunctionStatus?): String? =
        if (connection != ConnectionState.AUTHENTICATED) {
            "차량 인증 완료 후 다시 보낼 수 있습니다"
        } else {
            availabilityBlockReason(function)
        }

    private fun availabilityBlockReason(function: FunctionStatus?): String? = when {
        function == null -> null
        !function.quality.isTrusted -> AVAILABILITY_STALE
        else -> when (function.availability) {
            Availability.AVAILABLE, Availability.LIMITED -> null
            Availability.UNKNOWN -> "$AVAILABILITY_UNCONFIRMED — 차량이 확인하지 못함"
            Availability.UNAVAILABLE -> unavailableText(function)
        }
    }

    fun availabilityText(s: FunctionStatus?, nowMs: Long): ValueText = when {
        s == null -> NO_AVAILABILITY_VALUE
        !s.quality.isTrusted -> ValueText(AVAILABILITY_UNCONFIRMED, "최신 아님 · ${ValueFormat.receivedText(s.receivedAtMs, nowMs)}", shown = false)
        else -> when (s.availability) {
            Availability.AVAILABLE -> ValueText(AVAILABLE_TEXT, null, shown = true)
            Availability.LIMITED -> ValueText(limitedText(s), null, shown = true)
            Availability.UNAVAILABLE -> ValueText(unavailableText(s), null, shown = true)
            Availability.UNKNOWN -> ValueText(AVAILABILITY_UNCONFIRMED, null, shown = false)
        }
    }

    private fun unavailableText(s: FunctionStatus): String = "사용 불가 — ${s.reason.label}"
    private fun limitedText(s: FunctionStatus): String = "제한됨 — ${s.reason.label}"
}
