package com.vdcs.mobile.ui.rules

import com.vdcs.mobile.domain.model.CabinEnvironment
import com.vdcs.mobile.domain.model.ConnectionState
import com.vdcs.mobile.domain.model.DoorState
import com.vdcs.mobile.domain.model.LockState
import com.vdcs.mobile.domain.model.OpenState
import com.vdcs.mobile.domain.model.Qualified
import com.vdcs.mobile.domain.model.Warning

enum class ChipTone { NORMAL, ATTENTION, EMERGENCY, STALE, UNKNOWN }

data class StatusChip(val text: String, val tone: ChipTone)

data class ConnectCta(val blockReason: String?) {
    val text: String get() = blockReason ?: HomeRules.CONNECT_TEXT
}

data class StatusSentence(val headline: String, val detail: String)

object HomeRules {
    const val STALE_SUFFIX = "최신 아님"
    const val DOOR_OPEN_TEXT = "열린 도어 있음"
    const val DOOR_CLOSED_TEXT = "도어 모두 닫힘"
    const val NOT_RECEIVED_HEADLINE = "차량 상태 수신 전"
    const val NOT_RECEIVED_DETAIL = "잠금·도어·실내 온도는 수신 후 표시"

    private fun <T : Any> phrase(
        q: Qualified<T>?,
        nowMs: Long,
        name: String,
        format: (T) -> String,
        known: (T) -> Boolean = { true },
    ): String {
        val text = ValueFormat.value(q, nowMs, format)
        val v = q.displayableValue()
        if (v != null && !known(v)) return "$name ${ValueFormat.UNTRUSTED_TEXT}"
        return when (tone(text)) {
            ChipTone.UNKNOWN -> "$name ${text.text}"
            ChipTone.STALE -> "${text.text} ($STALE_SUFFIX)"
            else -> text.text
        }
    }

    fun tone(v: ValueText): ChipTone = when {
        !v.shown -> ChipTone.UNKNOWN
        v.note != null -> ChipTone.STALE
        else -> ChipTone.NORMAL
    }

    fun lockText(door: DoorState?, nowMs: Long): String =
        phrase(door?.lock, nowMs, "잠금", { if (it == LockState.LOCKED) "잠겨 있음" else it.label }, known = { it != LockState.UNKNOWN })

    fun doorOpenText(door: DoorState?, nowMs: Long): String =
        phrase(door?.open, nowMs, "도어", { if (it == OpenState.OPEN) DOOR_OPEN_TEXT else DOOR_CLOSED_TEXT }, known = { it != OpenState.UNKNOWN })

    fun cabinTemperatureText(environment: CabinEnvironment?, nowMs: Long): String =
        phrase(environment?.temperatureC, nowMs, "실내 온도", { "실내 ${ValueFormat.celsius(it)}" })

    fun statusSentence(door: DoorState?, environment: CabinEnvironment?, nowMs: Long): StatusSentence =
        if (door == null && environment == null) {
            StatusSentence(NOT_RECEIVED_HEADLINE, NOT_RECEIVED_DETAIL)
        } else {
            StatusSentence(
                headline = lockText(door, nowMs),
                detail = listOf(doorOpenText(door, nowMs), cabinTemperatureText(environment, nowMs)).joinToString(" · "),
            )
        }

    fun warningSummary(warnings: List<Warning>, connection: ConnectionState, origin: String?): StatusChip {
        val active = WarningRules.bannerWarnings(warnings)
        if (active.isNotEmpty()) {
            val text = listOfNotNull("발생 중 경고 ${active.size}건", WarningRules.headlineText(active.first()), origin)
            return StatusChip(text.joinToString(" · "), ChipTone.ATTENTION)
        }
        return when (WarningRules.summaryKind(warnings, connection)) {
            WarningSummaryKind.NOT_SYNCED -> StatusChip(WarningRules.NOT_SYNCED_TEXT, ChipTone.UNKNOWN)
            WarningSummaryKind.NO_REPORT -> StatusChip(WarningRules.NO_REPORT_TEXT, ChipTone.UNKNOWN)
            WarningSummaryKind.NONE_ACTIVE -> StatusChip(WarningRules.NONE_ACTIVE_TEXT, ChipTone.NORMAL)
            null -> StatusChip("상태 ${ValueFormat.UNTRUSTED_TEXT} 경고 ${uncertainCount(warnings)}건", ChipTone.UNKNOWN)
        }
    }

    private fun uncertainCount(warnings: List<Warning>): Int = warnings.count { WarningRules.display(it) == WarningDisplay.UNCERTAIN }

    const val CONNECT_TEXT = "차량 연결"

    fun connectCta(connection: ConnectionState): ConnectCta? =
        if (connection == ConnectionState.AUTHENTICATED) null else ConnectCta(ControlRules.connectBlockReason(connection))

    fun toggleRequest(reported: Boolean?): Boolean = reported != true

    fun toggleLabel(name: String, reported: Boolean?): String = if (toggleRequest(reported)) "$name 켜기" else "$name 끄기"

    fun <T> filled(q: Qualified<T>?, on: T): Boolean = q.trustedValue() == on

    fun controlDescription(action: String, state: ValueText): String =
        listOfNotNull(action, "현재 ${state.text}", state.note).joinToString(" · ")
}
