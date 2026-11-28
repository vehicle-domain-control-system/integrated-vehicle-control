package com.vdcs.mobile.data

import com.vdcs.mobile.data.ble.AttError
import com.vdcs.mobile.data.ble.BleTransport
import com.vdcs.mobile.data.ble.LinkState
import com.vdcs.mobile.data.ble.WriteResult
import com.vdcs.mobile.data.protocol.GattPayloads
import com.vdcs.mobile.data.protocol.GatewayStatus
import com.vdcs.mobile.data.protocol.MessageCodec
import com.vdcs.mobile.data.protocol.MessageType
import com.vdcs.mobile.data.protocol.VehicleMessage
import com.vdcs.mobile.domain.logic.ContextChange
import com.vdcs.mobile.domain.logic.RequestTracker
import com.vdcs.mobile.domain.logic.SessionContext
import com.vdcs.mobile.domain.logic.SessionEvent
import com.vdcs.mobile.domain.logic.SessionPhase
import com.vdcs.mobile.domain.logic.SessionState
import com.vdcs.mobile.domain.logic.SessionStateMachine
import com.vdcs.mobile.domain.logic.facts
import com.vdcs.mobile.domain.model.ConnectionState
import com.vdcs.mobile.domain.model.TrackedRequest
import com.vdcs.mobile.domain.repository.QueryScope
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

internal class SessionOrchestrator(
    private val transport: BleTransport,
    private val session: SessionContext,
    private val requestTracker: RequestTracker,
    private val pipeline: ReceivePipeline,
    private val queries: QueryCoordinator,
    private val store: VehicleStateStore,
    private val sender: AppCommandSender,
    private val heartbeat: AppStateHeartbeat,
    private val messageCodec: MessageCodec,
    private val config: VdcsConfig,
    private val clock: () -> Long,
    private val notify: (String) -> Unit,
) {
    private var current = SessionState()

    private val _state = MutableStateFlow(current)
    val state: StateFlow<SessionState> = _state.asStateFlow()

    private val _connection = MutableStateFlow(current.connectionState)
    val connectionState: StateFlow<ConnectionState> = _connection.asStateFlow()

    private val _detail = MutableStateFlow(current.detail)
    val connectionDetail: StateFlow<String?> = _detail.asStateFlow()

    private val _registered = MutableStateFlow(session.registered)
    val registered: StateFlow<Boolean> = _registered.asStateFlow()
    private val _domainBootId = MutableStateFlow(session.currentDomainBootId)
    val domainBootId: StateFlow<Long?> = _domainBootId.asStateFlow()

    val isLinked: Boolean get() = current.phase.isLinked

    fun start(scope: CoroutineScope) {
        scope.launch { transport.link.collect { onLink(it) } }
        scope.launch { transport.gatewayStatus.collect { onGatewayBytes(it) } }
        scope.launch { transport.incomingFrames.collect { onFrame(it) } }
        scope.launch {
            while (isActive) {
                delay(config.tickMs)
                tick()
            }
        }
    }

    fun dispatch(event: SessionEvent) {
        current = SessionStateMachine.next(current, event, session.facts())
        publishSession()
    }

    private fun publishSession() {
        _state.value = current
        _connection.value = current.connectionState
        _detail.value = current.detail
        _registered.value = session.registered
        _domainBootId.value = session.currentDomainBootId
    }

    suspend fun setForeground(foreground: Boolean) {
        transport.setForeground(foreground)
        if (!isLinked) return
        if (foreground) heartbeat.activeNow() else writeAppState(active = false)
    }

    suspend fun writeAppState(active: Boolean) {
        dispatch(SessionEvent.AppStateWritten(heartbeat.write(active)))
    }

    fun renewAppInstance() {
        session.newAppInstance()
        heartbeat.activeNow()
        dispatch(SessionEvent.AppInstanceRenewed)
    }

    fun forgetRegistration() {
        session.onUnregistered()
        publishSession()
        queries.clear()
        store.forgetAll()
        store.publish(clock())
    }

    private suspend fun onLink(link: LinkState) {
        val now = clock()
        when (link) {
            LinkState.Linked -> {
                pipeline.clear()
                dispatch(SessionEvent.LinkUp)
                writeAppState(active = transport.foreground)
                transport.readGatewayStatus()?.let { onGatewayBytes(it) }
            }
            is LinkState.Connecting -> {
                linkDown(now)
                dispatch(SessionEvent.LinkConnecting(link.step, link.attempt))
            }
            is LinkState.Disconnected -> {
                linkDown(now)
                dispatch(SessionEvent.LinkLost(link.cause, link.detail, link.reconnectStop))
            }
        }
    }

    private fun linkDown(now: Long) {
        if (isLinked) store.markPreviousContext(now)
        pipeline.clear()
        queries.clear()
        requestTracker.onLinkLost(now)
        session.onLinkLost()
        store.publish(now)
    }

    private suspend fun onGatewayBytes(bytes: ByteArray) {
        val status = GattPayloads.decodeGatewayStatus(bytes) ?: return
        val now = clock()
        val change = session.onGatewayStatus(
            registered = status.registered,
            domainLinkUp = status.domainLinkUp,
            sessionReady = status.sessionReady,
            deviceContextId = status.deviceContextId,
            sessionId = status.sessionId,
            domainBootId = status.domainBootId,
        )
        val changed = change == ContextChange.CHANGED
        if (changed) {
            requestTracker.resetForNewSession(status.sessionId, now)
            queries.clear()
            pipeline.clear()
            store.markPreviousContext(now)
        }
        dispatch(SessionEvent.GatewayUpdated(changed, status.registrationState == GatewayStatus.NOT_REGISTERED))
        store.publish(now)
        maybeSync()
        pumpQueries()
    }

    private fun enqueueRecovery(unresolved: List<TrackedRequest>) {
        unresolved.takeLast(config.recoveryQueryMax).forEach(::enqueueResultQuery)
    }

    private fun enqueueResultQuery(t: TrackedRequest) {
        queries.enqueue(QueryCoordinator.Pending(QueryScope.REQUEST_RESULT, QueryCoordinator.Purpose.FOLLOW_UP, t.sessionId, t.requestId))
    }

    private suspend fun onFrame(bytes: ByteArray) {
        val message = pipeline.accept(bytes, clock()) ?: return
        if (message is VehicleMessage.QueryEnd) onQueryEnd(message) else onMessage(message)
    }

    private fun onMessage(m: VehicleMessage) {
        val ctx = m.context ?: return
        if (!session.matches(ctx.deviceContextId, ctx.sessionId, ctx.domainBootId)) return
        val now = clock()
        queries.onResponse(ctx.queryId, QueryCoordinator.itemOf(m))
        if (m is VehicleMessage.Result) store.applyResult(m, now) else store.apply(m, now)
        store.publish(now)
    }

    private suspend fun onQueryEnd(m: VehicleMessage.QueryEnd) {
        val ctx = m.context
        val contextOk = session.matches(ctx.deviceContextId, ctx.sessionId, ctx.domainBootId)
        queries.onEnd(ctx.queryId, m.status, m.itemCount, contextOk)?.let { finish(it, clock()) }
        maybeSync()
        pumpQueries()
    }

    private fun finish(f: QueryCoordinator.Finished, now: Long) {
        when (f.pending.purpose) {
            QueryCoordinator.Purpose.SYNC ->
                if (f.complete) {
                    dispatch(SessionEvent.SyncCompleted)
                    enqueueRecovery(requestTracker.unresolved())
                } else {
                    dispatch(SessionEvent.SyncFailed(SessionPhase.SYNC_INCOMPLETE_TEXT, now + config.syncRetryMs))
                }
            QueryCoordinator.Purpose.USER ->
                if (!f.complete) notify("조회가 완전하지 않습니다 — 일부 항목은 동기화되지 않았습니다")
            QueryCoordinator.Purpose.FOLLOW_UP -> Unit
        }
    }

    private suspend fun tick() {
        val now = clock()
        pipeline.evictExpired(now)
        requestTracker.expireToUnknown(now)
            .filter { it.sessionId == session.sessionId }
            .forEach(::enqueueResultQuery)
        queries.expire(now)?.let { finish(it, now) }
        store.publish(now)
        if (isLinked) {
            val retrying = (current.phase as? SessionPhase.Subscribed)?.appStateFailed == true
            heartbeat.due(now, retrying)?.let { active -> writeAppState(active) }
        }
        maybeSync()
        pumpQueries()
    }

    private fun maybeSync() {
        val sync = current.pendingSync ?: return
        if (clock() >= sync.retryAt) queries.enqueueSyncIfAbsent()
    }

    suspend fun pumpQueries() {
        if (!current.canQuery) return
        val started = queries.startNext(clock()) ?: return
        val p = started.pending
        val r = sender.send(MessageType.M_QUERY, messageCodec.encodeQuery(started.id, p.scope, p.requestSession, p.requestId))
        if (r == WriteResult.Ok) return
        val now = clock()
        val rejected = (r as? WriteResult.Rejected)?.status
        queries.onSendFailed(started, busy = rejected == AttError.QUERY_BUSY, now = now)
        when {
            rejected == AttError.QUERY_BUSY -> Unit
            rejected == AttError.NOT_REGISTERED -> rejectAuth()
            p.purpose == QueryCoordinator.Purpose.SYNC ->
                dispatch(SessionEvent.SyncFailed("동기화 요청 실패: ${r.describe()}", now + config.syncRetryMs))
        }
    }

    fun rejectAuth() = dispatch(SessionEvent.AuthRejected(SessionPhase.NOT_REGISTERED_TEXT))
}
