package com.vdcs.mobile.domain.model

enum class WarningType(val raw: Int) {
    REAR(1),
    OCCUPANT_REMAINING(2),
    PINCH(3),
    BCM_FAULT(4),
    CIS_FAULT(5),
    WINDOW_FAULT(6),
    VSS_FAULT(7),
    DOOR_OPEN_AFTER_EXIT(8);

    companion object {
        fun fromRaw(raw: Int): WarningType? = entries.firstOrNull { it.raw == raw }
    }
}

enum class Severity(val raw: Int) {
    INFO(0),
    CAUTION(1),
    EMERGENCY(2);

    companion object {
        fun fromRaw(raw: Int): Severity? = entries.firstOrNull { it.raw == raw }
    }
}

data class Warning(
    val type: WarningType,
    val active: Boolean,
    val severity: Severity,
    val quality: Quality,
    val occurrenceId: Long,
    val domainBootId: Long,
    val read: ReadState,
    val receivedAtMs: Long,
    val ageMs: Long,
)

data class WarningRecord(
    val domainBootId: Long,
    val type: WarningType,
    val occurrenceId: Long,
    val severity: Severity,
    val active: Boolean,
    val read: ReadState,
    val firstReceivedAtMs: Long,
    val updatedAtMs: Long,
    val confirmedInApp: Boolean = false,
) {
    val unread: Boolean get() = read == ReadState.UNREAD && !confirmedInApp
}
