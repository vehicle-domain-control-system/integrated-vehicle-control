package com.vdcs.mobile.domain.model

sealed interface UserRequest {
    data class Door(val lock: Boolean) : UserRequest

    data class TargetTemperature(val celsius: Double) : UserRequest

    data class ClimateAuto(val enabled: Boolean) : UserRequest

    data class Fan(val level: FanLevel) : UserRequest

    data class LightEnabled(val enabled: Boolean) : UserRequest

    data class LightBrightness(val percent: Int) : UserRequest

    data class LightColor(val rgb: Rgb) : UserRequest

    data class ProximityUnlock(val enabled: Boolean) : UserRequest
}

data class TrackedRequest(
    val sessionId: Long,
    val requestId: Long,
    val request: UserRequest,
    val state: RequestState,
    val reason: ResultReason?,
    val confirmed: Boolean,
    val attempts: List<RequestAttempt>,
    val updatedAtMs: Long,

    val resultMismatch: Boolean = false,
) {
    constructor(
        sessionId: Long,
        requestId: Long,
        request: UserRequest,
        state: RequestState,
        reason: ResultReason?,
        confirmed: Boolean,
        sentAtMs: Long,
    ) : this(sessionId, requestId, request, state, reason, confirmed, listOf(RequestAttempt(sentAtMs)), sentAtMs)

    init {
        require(attempts.isNotEmpty()) { "시도가 하나 이상 있어야 한다" }
    }

    val attempt: RequestAttempt get() = attempts.last()

    val sentAtMs: Long get() = attempt.sentAtMs

    val acceptedAtMs: Long? get() = attempt.acceptedAtMs

    val progress: RequestState? get() = attempt.progress

    val executionObserved: Boolean get() = attempts.any { it.progress == RequestState.IN_PROGRESS }
}

data class RequestAttempt(
    val sentAtMs: Long,
    val acceptedAtMs: Long? = null,
    val progress: RequestState? = null,
)

enum class ResultReason(val raw: Int) {
    NONE(0),
    NOT_REGISTERED(1),
    LINK_LOST(2),
    SESSION_INVALID(3),
    DOOR_OPEN(4),
    STATE_UNTRUSTED(5),
    CMD_INVALID(6),
    NO_FEEDBACK(7),
    DRIVE_LIMIT_EXCEEDED(8),
    ALREADY_AT_TARGET(9),
    LOCAL_OVERRIDE(10),
    STOP_REQUESTED(11),
    SUPERSEDED(12),
    ANTIPINCH(13),
    FAULT(14),
    STALE(15),
    NO_DATA(16),
    NOT_READY(17),
    OUT_OF_RANGE(18),
    SOURCE_FAILURE(19),
    UNKNOWN_REQUEST(20),
    EXPIRED(21),
    ID_CONFLICT(22),
    POWER_NOT_ALLOWED(23),
    OVERHEAT(24),
    FAN_MISMATCH(25),

    UNSPECIFIED(-1);

    companion object {
        fun fromRaw(raw: Int): ResultReason = entries.firstOrNull { it.raw == raw } ?: UNSPECIFIED
    }
}
