package com.vdcs.mobile.ui.rules

import com.vdcs.mobile.domain.logic.WarningHistory
import com.vdcs.mobile.domain.model.ConnectionState
import com.vdcs.mobile.domain.model.ReadState
import com.vdcs.mobile.domain.model.Severity
import com.vdcs.mobile.domain.model.Warning
import com.vdcs.mobile.domain.model.WarningRecord
import com.vdcs.mobile.session.AppMode

enum class WarningDisplay { ACTIVE, UNCERTAIN, CLEARED }

data class WarningGroups(val open: List<Warning>, val cleared: List<Warning>)

data class AckTargets(val current: List<Warning>, val archived: List<WarningRecord>) {
    val size: Int get() = current.size + archived.size
}

enum class WarningSummaryKind { NOT_SYNCED, NO_REPORT, NONE_ACTIVE }

object WarningRules {
    const val READ_IS_NOT_CLEAR = "읽음은 확인 표시일 뿐, 경고를 해제하지 않습니다"

    const val SIMULATED_TEXT = "모의(시연)"

    const val NOT_SYNCED_TEXT = "차량과 동기화 전 — 현재 경고를 아직 확인하지 못했습니다 ('경고 없음' 아님)"
    const val NO_REPORT_TEXT = "차량이 보낸 경고 보고 없음 ('경고 없음' 으로 단정하지 않음)"
    const val NONE_ACTIVE_TEXT = "발생 중인 경고 없음"

    const val HISTORY_EMPTY_TEXT = "보관된 미확인 이력 없음"
    const val HISTORY_NOT_SYNCED_TEXT = "동기화 전 — 확인 상태는 연결 후 차량 보고로 갱신됩니다"

    fun display(w: Warning): WarningDisplay = when {
        w.active -> WarningDisplay.ACTIVE
        !w.quality.isTrusted -> WarningDisplay.UNCERTAIN
        else -> WarningDisplay.CLEARED
    }

    fun statusText(w: Warning): String = when (display(w)) {
        WarningDisplay.ACTIVE -> if (w.quality.isTrusted) "발생 중" else "발생 중 · 상태 ${ValueFormat.UNTRUSTED_TEXT}"
        WarningDisplay.UNCERTAIN -> ValueFormat.UNTRUSTED_TEXT
        WarningDisplay.CLEARED -> "해제됨"
    }

    fun activeTone(w: Warning): ChipTone = if (w.quality.isTrusted) severityTone(w.severity) else ChipTone.UNKNOWN

    fun severityTone(severity: Severity): ChipTone =
        if (severity == Severity.EMERGENCY) ChipTone.EMERGENCY else ChipTone.ATTENTION

    fun sort(list: List<Warning>): List<Warning> = list.sortedWith(
        compareBy<Warning> { display(it).ordinal }
            .thenByDescending { it.severity.raw }
            .thenBy { if (it.read == ReadState.UNREAD) 0 else 1 }
            .thenByDescending { it.receivedAtMs },
    )

    fun isAcknowledgeable(w: Warning): Boolean = w.read == ReadState.UNREAD && w.occurrenceId != NO_OCCURRENCE_ID

    fun acknowledgeable(list: List<Warning>): List<Warning> = list.filter(::isAcknowledgeable)

    private const val NO_OCCURRENCE_ID = 0L

    fun ackAllTargets(current: List<Warning>, history: List<WarningRecord>, currentDomainBootId: Long?): AckTargets =
        AckTargets(acknowledgeable(current), WarningHistory.confirmableInApp(history, currentDomainBootId))

    fun group(list: List<Warning>): WarningGroups {
        val (cleared, open) = sort(list).partition { display(it) == WarningDisplay.CLEARED }
        return WarningGroups(open, cleared)
    }

    fun bannerWarnings(list: List<Warning>): List<Warning> = sort(list).filter { display(it) == WarningDisplay.ACTIVE }

    fun originLabel(mode: AppMode): String? = when (mode) {
        AppMode.DEMO -> SIMULATED_TEXT
        AppMode.REAL -> null
    }

    fun headlineText(w: Warning): String = "[${w.severity.label}] ${w.type.label}"

    fun bannerText(w: Warning, origin: String?): String =
        listOfNotNull(headlineText(w), statusText(w), origin).joinToString(" · ")

    fun summaryText(warnings: List<Warning>, connection: ConnectionState): String? = when (summaryKind(warnings, connection)) {
        WarningSummaryKind.NOT_SYNCED -> NOT_SYNCED_TEXT
        WarningSummaryKind.NO_REPORT -> NO_REPORT_TEXT
        WarningSummaryKind.NONE_ACTIVE -> NONE_ACTIVE_TEXT
        null -> null
    }

    fun summaryKind(warnings: List<Warning>, connection: ConnectionState): WarningSummaryKind? = when {
        !synced(connection) -> WarningSummaryKind.NOT_SYNCED
        warnings.isEmpty() -> WarningSummaryKind.NO_REPORT
        warnings.all { display(it) == WarningDisplay.CLEARED } -> WarningSummaryKind.NONE_ACTIVE
        else -> null
    }

    fun sectionTitle(origin: String?): String = listOfNotNull("경고", origin).joinToString(" · ")

    fun historySyncText(connection: ConnectionState): String? = if (synced(connection)) null else HISTORY_NOT_SYNCED_TEXT

    private fun synced(connection: ConnectionState): Boolean = connection == ConnectionState.AUTHENTICATED

    fun clearedSummaryText(count: Int, expanded: Boolean): String =
        "해제된 경고 ${count}건 · ${if (expanded) "접기" else "펼치기"}"

    fun archived(history: List<WarningRecord>, current: List<Warning>): List<WarningRecord> {
        val shown = current.map { Triple(it.domainBootId, it.type, it.occurrenceId) }.toSet()
        return history.filter { Triple(it.domainBootId, it.type, it.occurrenceId) !in shown }
    }

    fun historyEmptyText(archived: List<WarningRecord>): String? = if (archived.none { it.unread }) HISTORY_EMPTY_TEXT else null

    fun unreadCount(history: List<WarningRecord>): Int = history.count { it.unread }

    fun unreadText(count: Int): String? = if (count > 0) "미확인 경고 ${count}건" else null

    fun unreadShortText(count: Int): String? = if (count > 0) "미확인 $count" else null

    fun cardDetailText(w: Warning, origin: String?): String =
        listOfNotNull(statusText(w), w.read.label, origin).joinToString(" · ")

    fun readShortText(read: ReadState): String = when (read) {
        ReadState.UNREAD -> "미확인"
        ReadState.READ -> "확인함"
    }

    const val APP_CONFIRMED_TEXT = "확인함 (앱)"

    fun recordReadText(r: WarningRecord): String =
        if (r.read == ReadState.UNREAD && r.confirmedInApp) APP_CONFIRMED_TEXT else readShortText(r.read)

    fun clearedMetaText(w: Warning, nowMs: Long): String =
        "${w.severity.label} · ${statusText(w)} · ${ValueFormat.receivedText(w.receivedAtMs, nowMs)}"

    fun recordMetaText(r: WarningRecord, nowMs: Long): String {
        val state = if (r.active) "마지막 보고 발생 중" else "해제됨"
        return "${r.severity.label} · $state · ${ValueFormat.ago(r.firstReceivedAtMs, nowMs)} 발생"
    }
}
