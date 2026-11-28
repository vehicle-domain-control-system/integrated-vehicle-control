package com.vdcs.mobile.ui.screen

import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.Stable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.listSaver
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import kotlinx.coroutines.delay

internal const val EDIT_IDLE_MS = 5_000L

internal data class Draft<T : Any>(
    val value: T,
    val editedAtMs: Long?,
    val lastReported: T? = null,
    val sent: Boolean = false,
) {
    fun isEditing(nowMs: Long): Boolean = editedAtMs != null && nowMs - editedAtMs < EDIT_IDLE_MS

    fun idleRemainingMs(nowMs: Long): Long =
        if (editedAtMs == null) 0 else (editedAtMs + EDIT_IDLE_MS - nowMs).coerceAtLeast(0)

    fun edit(v: T, nowMs: Long): Draft<T> = copy(value = v, editedAtMs = nowMs, sent = false)

    fun committed(nowMs: Long): Draft<T> = copy(editedAtMs = nowMs, sent = true)

    fun synced(reported: T?, nowMs: Long): Draft<T> = when {
        sent && reported != null && reported != lastReported -> Draft(reported, editedAtMs = null, lastReported = reported)
        isEditing(nowMs) -> copy(lastReported = reported)
        reported == null -> copy(editedAtMs = null, lastReported = null, sent = false)
        else -> Draft(reported, editedAtMs = null, lastReported = reported)
    }

    companion object {
        fun <T : Any> start(reported: T?, fallback: T): Draft<T> =
            Draft(reported ?: fallback, editedAtMs = null, lastReported = reported)
    }
}

@Stable
class DraftState<T : Any> internal constructor(initial: Draft<T>, private val clock: () -> Long) {
    private var draft by mutableStateOf(initial)

    val value: T get() = draft.value

    internal val editedAtMs: Long? get() = draft.editedAtMs

    fun edit(v: T) {
        draft = draft.edit(v, clock())
    }

    fun commit(): T {
        draft = draft.committed(clock())
        return draft.value
    }

    internal fun idleRemainingMs(): Long = draft.idleRemainingMs(clock())

    internal fun sync(reported: T?) {
        draft = draft.synced(reported, clock())
    }

    internal companion object {
        @Suppress("UNCHECKED_CAST")
        fun <T : Any> saver(clock: () -> Long) = listSaver<DraftState<T>, Any?>(
            save = { listOf(it.draft.value, it.draft.editedAtMs, it.draft.lastReported, it.draft.sent) },
            restore = { DraftState(Draft(it[0] as T, it[1] as Long?, it[2] as T?, it[3] as Boolean), clock) },
        )
    }
}

@Composable
fun <T : Any> rememberDraft(reported: T?, fallback: T): DraftState<T> {
    val clock: () -> Long = System::currentTimeMillis
    val state = rememberSaveable(saver = DraftState.saver(clock)) { DraftState(Draft.start(reported, fallback), clock) }
    LaunchedEffect(state, reported, state.editedAtMs) {
        state.sync(reported)
        val remaining = state.idleRemainingMs()
        if (remaining > 0) {
            delay(remaining)
            state.sync(reported)
        }
    }
    return state
}
