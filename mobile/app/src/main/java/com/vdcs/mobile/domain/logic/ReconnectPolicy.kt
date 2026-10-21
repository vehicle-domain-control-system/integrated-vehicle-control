package com.vdcs.mobile.domain.logic

class ReconnectPolicy(
    private val maxAttempts: Int,
    private val windowMs: Long,
    private val baseDelayMs: Long = 1_000,
    private val maxDelayMs: Long = 10_000,
) {
    fun shouldRetry(cause: DisconnectCause, attempt: Int, elapsedMs: Long): Boolean =
        cause == DisconnectCause.LINK_LOST && attempt in 1..maxAttempts && elapsedMs < windowMs

    fun decide(attempt: Int, elapsedMs: Long, foreground: Boolean): ReconnectDecision = when {
        !foreground -> ReconnectDecision.Stop(ReconnectStop.INACTIVE)
        shouldRetry(DisconnectCause.LINK_LOST, attempt, elapsedMs) -> ReconnectDecision.Retry(nextDelayMs(attempt))
        else -> ReconnectDecision.Stop(ReconnectStop.LIMIT_REACHED)
    }

    fun nextDelayMs(attempt: Int): Long {
        val shift = (attempt - 1).coerceIn(0, 20)
        return minOf(maxDelayMs, baseDelayMs shl shift)
    }
}

enum class DisconnectCause {
    LINK_LOST,
    USER,
    AUTH_FAILED,
    BOND_MISMATCH,
}

enum class ReconnectStop {
    LIMIT_REACHED,
    INACTIVE,
}

sealed interface ReconnectDecision {
    data class Retry(val delayMs: Long) : ReconnectDecision
    data class Stop(val reason: ReconnectStop) : ReconnectDecision
}
