package com.vdcs.mobile.data

import com.vdcs.mobile.data.ble.AttError
import com.vdcs.mobile.data.ble.BleTransport
import com.vdcs.mobile.data.ble.WriteResult
import com.vdcs.mobile.data.protocol.BleFrameCodec
import com.vdcs.mobile.data.protocol.MessageCodec
import com.vdcs.mobile.data.protocol.MessageType
import com.vdcs.mobile.data.protocol.Reassembler
import com.vdcs.mobile.domain.logic.FreshnessEvaluator
import com.vdcs.mobile.domain.logic.RequestTracker
import com.vdcs.mobile.domain.logic.SessionContext
import com.vdcs.mobile.domain.logic.SessionEvent
import com.vdcs.mobile.domain.logic.SessionPhase
import com.vdcs.mobile.domain.logic.WarningHistory
import com.vdcs.mobile.domain.model.ConnectionState
import com.vdcs.mobile.domain.model.TrackedRequest
import com.vdcs.mobile.domain.model.UserRequest
import com.vdcs.mobile.domain.model.VehicleSnapshot
import com.vdcs.mobile.domain.model.Warning
import com.vdcs.mobile.domain.model.WarningRecord
import com.vdcs.mobile.domain.repository.ConnectionController
import com.vdcs.mobile.domain.repository.InvalidRequest
import com.vdcs.mobile.domain.repository.PermissionMissing
import com.vdcs.mobile.domain.repository.QueryScope
import com.vdcs.mobile.domain.repository.RequestBlocked
import com.vdcs.mobile.domain.repository.RequestNotSent
import com.vdcs.mobile.domain.repository.VehicleRepository
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class VehicleRepositoryImpl(
    private val transport: BleTransport,
    frameCodec: BleFrameCodec,
    reassembler: Reassembler,
    private val messageCodec: MessageCodec,
    private val requestTracker: RequestTracker,
    freshness: FreshnessEvaluator,
    private val session: SessionContext,
    warningHistoryStorage: WarningHistoryStorage,
    config: VdcsConfig,
    private val scope: CoroutineScope,
    private val clock: () -> Long,
) : VehicleRepository, ConnectionController {
    private val confined = scope.confinedDispatcher()

    private val _notices = MutableSharedFlow<String>(extraBufferCapacity = 16)
    override val notices: SharedFlow<String> = _notices

    private val sender = AppCommandSender(transport, frameCodec)
    private val store = VehicleStateStore(
        requestTracker = requestTracker,
        projector = DisplayProjector(freshness, config.fastStaleMs, config.slowStaleMs),
        history = WarningHistory(config.warningHistoryLimit),
        historyStorage = warningHistoryStorage,
    )
    private val queries = QueryCoordinator(config.queryTimeoutMs, config.queryBusyRetryMs)
    private val orchestrator = SessionOrchestrator(
        transport = transport,
        session = session,
        requestTracker = requestTracker,
        pipeline = ReceivePipeline(frameCodec, reassembler, messageCodec),
        queries = queries,
        store = store,
        sender = sender,
        heartbeat = AppStateHeartbeat(transport, session, config.heartbeatMs, clock),
        messageCodec = messageCodec,
        config = config,
        clock = clock,
        notify = { _notices.tryEmit(it) },
    )

    override val connectionState: StateFlow<ConnectionState> = orchestrator.connectionState
    override val registered: StateFlow<Boolean> = orchestrator.registered
    override val domainBootId: StateFlow<Long?> = orchestrator.domainBootId
    override val connectionDetail: StateFlow<String?> = orchestrator.connectionDetail
    override val vehicleState: StateFlow<VehicleSnapshot> = store.vehicle
    override val requests: StateFlow<List<TrackedRequest>> = store.requests
    override val warningHistory: StateFlow<List<WarningRecord>> = store.warningHistory

    init {
        session.newAppInstance()
        orchestrator.start(scope)
    }

    override suspend fun connect() = withContext(confined) {
        orchestrator.dispatch(SessionEvent.ConnectRequested)
        permissionGuard { transport.connect() }
    }

    override suspend fun disconnect() = withContext(confined) {
        if (orchestrator.isLinked) orchestrator.writeAppState(active = false)
        permissionGuard { transport.disconnect() }
    }

    override suspend fun unregister() = withContext(confined) {
        if (orchestrator.isLinked) orchestrator.writeAppState(active = false)
        permissionGuard { transport.forget() }
        orchestrator.forgetRegistration()
        _notices.tryEmit("등록을 해제했습니다. 휴대폰 블루투스 설정에서 차량 기기도 삭제해 주세요.")
        Unit
    }

    override fun setForeground(foreground: Boolean) {
        scope.launch { orchestrator.setForeground(foreground) }
    }

    override suspend fun send(request: UserRequest): Long = withContext(confined) {
        requireReady()
        try {
            messageCodec.validate(request)
        } catch (e: IllegalArgumentException) {
            throw InvalidRequest(e.message ?: "범위를 확인하세요")
        }
        val tracked = try {
            requestTracker.create(request, session.sessionId, clock())
        } catch (e: RequestTracker.IdSpaceExhausted) {
            orchestrator.renewAppInstance()
            throw RequestBlocked("요청 번호를 새로 시작하는 중 — 잠시 후 다시 시도")
        }
        store.publish(clock())
        transmit(tracked, resentSameId = false)
        tracked.requestId
    }

    override suspend fun resend(request: TrackedRequest) = withContext(confined) {
        requireReady()
        val again = requestTracker.resend(request.sessionId, request.requestId, session.sessionId, clock())
            ?: throw RequestBlocked("미확인 요청만 다시 보낼 수 있습니다")
        store.publish(clock())
        transmit(again, resentSameId = again.requestId == request.requestId && again.sessionId == request.sessionId)
    }

    override suspend fun acknowledgeWarning(warning: Warning) = withContext(confined) {
        if (!orchestrator.isLinked) throw RequestBlocked(SessionPhase.NOT_CONNECTED_TEXT)
        val r = sender.send(MessageType.M_WARNING_ACK, messageCodec.encodeWarningAck(warning))
        if (r != WriteResult.Ok) _notices.tryEmit("읽음 처리 실패: ${r.describe()}")
        Unit
    }

    override suspend fun confirmArchivedWarnings(records: List<WarningRecord>) = withContext(confined) {
        store.confirmArchivedWarnings(records, session.currentDomainBootId, clock())
    }

    override suspend fun query(scope: QueryScope) = withContext(confined) {
        queries.enqueue(QueryCoordinator.Pending(scope, QueryCoordinator.Purpose.USER))
        orchestrator.pumpQueries()
    }

    private fun requireReady() {
        val s = orchestrator.state.value
        if (!s.canIssueRequests) throw RequestBlocked(s.blockReason)
    }

    private suspend fun transmit(t: TrackedRequest, resentSameId: Boolean) {
        val r = sender.send(MessageType.M_REQUEST, messageCodec.encodeRequest(t.requestId, t.request))
        if (r == WriteResult.Ok) return
        undo(t, resentSameId)
        if (r is WriteResult.Rejected && r.status == AttError.NOT_REGISTERED) orchestrator.rejectAuth()
        throw RequestNotSent(r.describe())
    }

    private fun undo(t: TrackedRequest, resentSameId: Boolean) {
        if (resentSameId) requestTracker.revertToUnknown(t.sessionId, t.requestId, clock())
        else requestTracker.withdraw(t.sessionId, t.requestId)
        store.publish(clock())
    }

    private inline fun <T> permissionGuard(block: () -> T): T =
        try {
            block()
        } catch (e: SecurityException) {
            throw PermissionMissing("블루투스 권한이 없습니다")
        }
}
