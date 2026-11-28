package com.vdcs.mobile.domain.logic

import com.vdcs.mobile.domain.model.RequestAttempt
import com.vdcs.mobile.domain.model.RequestState
import com.vdcs.mobile.domain.model.ResultReason
import com.vdcs.mobile.domain.model.TrackedRequest
import com.vdcs.mobile.domain.model.UserRequest

class RequestTracker internal constructor(
    private val deadlines: ResultDeadlines,
    private val recentLimit: Int,
    firstRequestId: Long,
) {
    constructor(deadlines: ResultDeadlines, recentLimit: Int) : this(deadlines, recentLimit, 1L)

    private data class Key(val sessionId: Long, val requestId: Long)

    private val requests = LinkedHashMap<Key, TrackedRequest>()
    private var nextId = firstRequestId

    private var counterSessionId = 0L

    fun create(request: UserRequest, sessionId: Long, nowMs: Long): TrackedRequest {
        bindCounter(sessionId)
        while (nextId <= MAX_U32 && Key(sessionId, nextId) in requests) nextId++
        if (nextId > MAX_U32) throw IdSpaceExhausted()
        val tracked = TrackedRequest(
            sessionId = sessionId, requestId = nextId++, request = request,
            state = RequestState.SENT, reason = null, confirmed = false, sentAtMs = nowMs,
        )
        requests[Key(sessionId, tracked.requestId)] = tracked
        return tracked
    }

    private fun bindCounter(sessionId: Long) {
        if (counterSessionId == sessionId) return
        if (counterSessionId != 0L) nextId = 1
        counterSessionId = sessionId
    }

    fun resend(sessionId: Long, requestId: Long, currentSessionId: Long, nowMs: Long): TrackedRequest? {
        val old = requests[Key(sessionId, requestId)] ?: return null
        if (old.state != RequestState.UNKNOWN) return null
        if (old.sessionId != currentSessionId) return create(old.request, currentSessionId, nowMs)
        val again = old.copy(
            state = RequestState.SENT, reason = null,
            attempts = old.attempts + RequestAttempt(sentAtMs = nowMs), updatedAtMs = nowMs,
        )
        requests[Key(sessionId, requestId)] = again
        return again
    }

    fun withdraw(sessionId: Long, requestId: Long) {
        requests.remove(Key(sessionId, requestId))
    }

    fun revertToUnknown(sessionId: Long, requestId: Long, nowMs: Long) {
        val key = Key(sessionId, requestId)
        val current = requests[key] ?: return
        if (current.state == RequestState.SENT && current.attempts.size > 1) {
            requests[key] = current.copy(state = RequestState.UNKNOWN, attempts = current.attempts.dropLast(1), updatedAtMs = nowMs)
        }
    }

    fun onResult(
        requestSession: Long,
        requestId: Long,
        state: RequestState,
        reason: ResultReason?,
        confirmed: Boolean,
        nowMs: Long,
    ): TrackedRequest? {
        val key = Key(requestSession, requestId)
        val current = requests[key] ?: return null
        val updated = when {
            state in FINAL -> applyFinal(current, state, reason, confirmed, nowMs)
            else -> applyProgress(current, state, reason, confirmed, nowMs)
        } ?: return null
        requests[key] = updated
        prune()
        return updated
    }

    private fun applyFinal(
        current: TrackedRequest,
        state: RequestState,
        reason: ResultReason?,
        confirmed: Boolean,
        nowMs: Long,
    ): TrackedRequest? = when {
        current.state in FINAL && state == current.state ->
            if (confirmed && !current.confirmed) current.copy(confirmed = true, updatedAtMs = nowMs) else null
        current.state in FINAL || (state == RequestState.REJECTED && current.executionObserved) ->
            if (current.resultMismatch) null else current.copy(resultMismatch = true, updatedAtMs = nowMs)
        else -> current.copy(state = state, reason = reason, confirmed = confirmed, updatedAtMs = nowMs)
    }

    private fun applyProgress(
        current: TrackedRequest,
        state: RequestState,
        reason: ResultReason?,
        confirmed: Boolean,
        nowMs: Long,
    ): TrackedRequest? {
        if (current.state in FINAL) return null
        val attempt = current.attempt.advance(state, nowMs) ?: return null
        val attempts = current.attempts.dropLast(1) + attempt
        return if (current.state == RequestState.UNKNOWN) {
            current.copy(attempts = attempts, updatedAtMs = nowMs)
        } else {
            current.copy(state = state, reason = reason, confirmed = confirmed, attempts = attempts, updatedAtMs = nowMs)
        }
    }

    private fun RequestAttempt.advance(state: RequestState, nowMs: Long): RequestAttempt? {
        if (progress != null && state.ordinal <= progress.ordinal) return null
        return copy(progress = state, acceptedAtMs = acceptedAtMs ?: nowMs)
    }

    fun expireToUnknown(nowMs: Long): List<TrackedRequest> {
        val expired = requests.values.filter { it.isOverdue(nowMs) }
        expired.forEach {
            requests[Key(it.sessionId, it.requestId)] = it.copy(state = RequestState.UNKNOWN, updatedAtMs = nowMs)
        }
        val result = expired.map { requests.getValue(Key(it.sessionId, it.requestId)) }
        prune()
        return result
    }

    private fun TrackedRequest.isOverdue(nowMs: Long): Boolean = when (state) {
        RequestState.SENT -> nowMs - sentAtMs >= deadlines.firstResponseMs
        RequestState.ACCEPTED, RequestState.IN_PROGRESS ->
            attempt.acceptedAtMs.let { it != null && nowMs - it >= deadlines.finalFor(request) }
        else -> false
    }

    fun onLinkLost(nowMs: Long) {
        markInFlightUnknown(nowMs)
        prune()
    }

    fun resetForNewSession(newSessionId: Long, nowMs: Long) {
        markInFlightUnknown(nowMs)
        bindCounter(newSessionId)
        prune()
    }

    fun unresolved(): List<TrackedRequest> = requests.values.filter { it.state == RequestState.UNKNOWN }

    private fun markInFlightUnknown(nowMs: Long) {
        requests.replaceAll { _, r ->
            if (r.state in IN_FLIGHT) r.copy(state = RequestState.UNKNOWN, updatedAtMs = nowMs) else r
        }
    }

    fun clear() {
        requests.clear()
    }

    private fun prune() {
        val settled = requests.entries.filter { it.value.state !in IN_FLIGHT }.sortedBy { it.value.updatedAtMs }
        val excess = settled.size - recentLimit
        if (excess <= 0) return
        settled.take(excess).forEach { requests.remove(it.key) }
    }

    fun snapshot(): List<TrackedRequest> = requests.values.toList()

    class IdSpaceExhausted : IllegalStateException("REQUEST_ID 번호공간 소진 — 새 APP_INSTANCE_ID 필요")

    companion object {
        const val MAX_U32 = 0xFFFF_FFFFL
        val FINAL = setOf(RequestState.DONE, RequestState.REJECTED, RequestState.CANCELLED, RequestState.FAILED)

        val IN_FLIGHT = setOf(RequestState.SENT, RequestState.ACCEPTED, RequestState.IN_PROGRESS)
    }
}
