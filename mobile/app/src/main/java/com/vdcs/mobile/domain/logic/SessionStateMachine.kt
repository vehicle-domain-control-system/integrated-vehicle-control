package com.vdcs.mobile.domain.logic

import com.vdcs.mobile.domain.model.ConnectionState

sealed interface SessionPhase {
    data class Disconnected(
        val cause: DisconnectCause?,
        val linkDetail: String?,
        val reconnectStop: ReconnectStop? = null,
    ) : SessionPhase

    data class Linking(val step: String, val attempt: Int) : SessionPhase

    data class Subscribed(val appStateFailed: Boolean) : SessionPhase

    data class ContextWait(val waitingFor: ContextGap) : SessionPhase

    data class QuerySync(val attempt: Int = 1, val retryAt: Long = 0, val failure: String? = null) : SessionPhase

    data object SessionReadyWait : SessionPhase

    data object Ready : SessionPhase

    val isLinked: Boolean get() = this !is Disconnected && this !is Linking

    val connectionState: ConnectionState
        get() = when (this) {
            is Disconnected -> ConnectionState.DISCONNECTED
            is Linking -> ConnectionState.CONNECTING
            Ready -> ConnectionState.AUTHENTICATED
            else -> ConnectionState.CONNECTED
        }

    val detail: String?
        get() = when (this) {
            is Disconnected -> when (cause) {
                null -> null
                DisconnectCause.USER -> "연결 해제됨"
                DisconnectCause.AUTH_FAILED -> "등록 실패 — 휴대폰 블루투스 설정에서 차량 기기를 삭제한 뒤 다시 연결하세요"
                DisconnectCause.BOND_MISMATCH -> BOND_MISMATCH_TEXT
                DisconnectCause.LINK_LOST -> when (reconnectStop) {
                    ReconnectStop.LIMIT_REACHED -> withLinkDetail(RECONNECT_LIMIT_TEXT)
                    ReconnectStop.INACTIVE -> withLinkDetail(RECONNECT_INACTIVE_TEXT)
                    null -> linkDetail ?: "연결 끊김"
                }
            }
            is Linking -> step + if (attempt > 1) " (재시도 $attempt)" else ""
            is Subscribed -> if (appStateFailed) APP_STATE_FAILED_TEXT else CONTEXT_CHECK_TEXT
            is ContextWait -> when (waitingFor) {
                ContextGap.CONTEXT -> CONTEXT_CHECK_TEXT
                ContextGap.REGISTRATION -> NOT_REGISTERED_TEXT
                ContextGap.DOMAIN_LINK -> "차량 중앙 연결 대기 중"
            }
            is QuerySync -> failure ?: "초기 상태 동기화 중"
            SessionReadyWait -> "차량 세션 준비 중"
            Ready -> null
        }

    companion object {
        const val RECONNECT_LIMIT_TEXT = "재연결 한도 도달 — 다시 연결을 눌러 주세요"
        const val RECONNECT_INACTIVE_TEXT = "화면 비활성 — 자동 재연결 중지 (화면으로 돌아오면 다시 연결)"
        const val NOT_CONNECTED_TEXT = "차량과 연결되어 있지 않습니다"
        const val NOT_REGISTERED_TEXT = "등록되지 않은 단말입니다"
        const val BOND_MISMATCH_TEXT = "재등록 필요 — 휴대폰과 차량의 등록 정보가 맞지 않습니다. 휴대폰 블루투스 설정에서 차량 기기를 삭제한 뒤 다시 등록하세요"
        const val APP_STATE_FAILED_TEXT = "앱 상태 전송 실패 — 다시 시도 중"
        const val SYNC_INCOMPLETE_TEXT = "초기 상태 동기화 미완료 — 다시 시도 중"
        private const val CONTEXT_CHECK_TEXT = "차량 문맥 확인 중"
    }
}

private fun SessionPhase.Disconnected.withLinkDetail(head: String): String =
    linkDetail?.let { "$head ($it)" } ?: head

enum class ContextGap { CONTEXT, REGISTRATION, DOMAIN_LINK }

sealed interface SessionEvent {
    data object ConnectRequested : SessionEvent
    data class LinkConnecting(val step: String, val attempt: Int) : SessionEvent
    data class LinkLost(
        val cause: DisconnectCause?,
        val detail: String?,
        val reconnectStop: ReconnectStop? = null,
    ) : SessionEvent
    data object LinkUp : SessionEvent
    data class AppStateWritten(val ok: Boolean) : SessionEvent
    data class GatewayUpdated(val contextChanged: Boolean, val notRegistered: Boolean) : SessionEvent
    data object AppInstanceRenewed : SessionEvent
    data object SyncCompleted : SessionEvent
    data class SyncFailed(val reason: String, val retryAt: Long) : SessionEvent
    data class AuthRejected(val reason: String) : SessionEvent
}

data class ContextKey(val deviceContextId: Long, val sessionId: Long, val domainBootId: Long)

data class SessionFacts(
    val context: ContextKey?,
    val registered: Boolean,
    val domainLinkUp: Boolean,
    val sessionReady: Boolean,
)

fun SessionContext.facts() = SessionFacts(
    context = if (hasContext()) ContextKey(deviceContextId, sessionId, domainBootId) else null,
    registered = registered,
    domainLinkUp = domainLinkUp,
    sessionReady = sessionReady,
)

data class SessionState(
    val phase: SessionPhase = SessionPhase.Disconnected(cause = null, linkDetail = null),
    val authFailure: String? = null,
    val syncedContext: ContextKey? = null,
) {
    val blocked: Boolean get() = phase.isLinked && authFailure != null

    val connectionState: ConnectionState get() = if (blocked) ConnectionState.CONNECTED else phase.connectionState

    val detail: String? get() = if (blocked) authFailure else phase.detail

    val canIssueRequests: Boolean get() = !blocked && phase == SessionPhase.Ready

    val canQuery: Boolean
        get() = !blocked && (phase is SessionPhase.QuerySync || phase == SessionPhase.SessionReadyWait || phase == SessionPhase.Ready)

    val pendingSync: SessionPhase.QuerySync? get() = if (blocked) null else phase as? SessionPhase.QuerySync

    val blockReason: String get() = detail ?: SessionPhase.NOT_CONNECTED_TEXT
}

object SessionStateMachine {
    fun next(state: SessionState, event: SessionEvent, facts: SessionFacts): SessionState {
        val phase = state.phase
        return when (event) {
            SessionEvent.ConnectRequested -> state.copy(authFailure = null)
            is SessionEvent.LinkConnecting ->
                state.copy(phase = SessionPhase.Linking(event.step, event.attempt), syncedContext = null)
            is SessionEvent.LinkLost ->
                state.copy(phase = SessionPhase.Disconnected(event.cause, event.detail, event.reconnectStop), syncedContext = null)
            SessionEvent.LinkUp -> state.copy(phase = SessionPhase.Subscribed(appStateFailed = false))
            is SessionEvent.AppStateWritten -> when {
                !phase.isLinked -> state
                !event.ok -> state.copy(phase = SessionPhase.Subscribed(appStateFailed = true))
                phase is SessionPhase.Subscribed -> state.progress(null, facts)
                else -> state
            }
            is SessionEvent.GatewayUpdated -> {
                val s = if (event.notRegistered) state.copy(authFailure = SessionPhase.NOT_REGISTERED_TEXT) else state
                when {
                    !phase.isLinked || phase is SessionPhase.Subscribed -> s
                    else -> s.progress(if (event.contextChanged) null else phase, facts)
                }
            }
            SessionEvent.AppInstanceRenewed -> state.copy(syncedContext = null).progressIfPastSubscribed(phase, facts)
            SessionEvent.SyncCompleted -> when {
                !phase.isLinked || facts.context == null -> state
                else -> state.copy(syncedContext = facts.context).progressIfPastSubscribed(null, facts)
            }
            is SessionEvent.SyncFailed -> when (phase) {
                is SessionPhase.QuerySync ->
                    state.copy(phase = SessionPhase.QuerySync(phase.attempt + 1, event.retryAt, event.reason))
                else -> state
            }
            is SessionEvent.AuthRejected -> state.copy(authFailure = event.reason)
        }
    }

    private fun SessionState.progressIfPastSubscribed(current: SessionPhase?, f: SessionFacts): SessionState =
        if (phase.isLinked && phase !is SessionPhase.Subscribed) progress(current, f) else this

    private fun SessionState.progress(current: SessionPhase?, f: SessionFacts): SessionState {
        val synced = f.context != null && f.context == syncedContext
        val next = when {
            f.context == null -> SessionPhase.ContextWait(ContextGap.CONTEXT)
            !f.registered -> SessionPhase.ContextWait(ContextGap.REGISTRATION)
            !f.domainLinkUp -> SessionPhase.ContextWait(ContextGap.DOMAIN_LINK)
            !synced -> current as? SessionPhase.QuerySync ?: SessionPhase.QuerySync()
            !f.sessionReady -> SessionPhase.SessionReadyWait
            else -> SessionPhase.Ready
        }
        return copy(phase = next)
    }
}
