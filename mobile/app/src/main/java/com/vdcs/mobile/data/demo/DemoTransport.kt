package com.vdcs.mobile.data.demo

import com.vdcs.mobile.data.ble.AttError
import com.vdcs.mobile.data.ble.BleTransport
import com.vdcs.mobile.data.ble.GattSpec
import com.vdcs.mobile.data.ble.LinkState
import com.vdcs.mobile.data.ble.WriteResult
import com.vdcs.mobile.data.confinedDispatcher
import com.vdcs.mobile.data.protocol.BleFrameCodec
import com.vdcs.mobile.data.protocol.ByteReader
import com.vdcs.mobile.data.protocol.MessageType
import com.vdcs.mobile.data.protocol.Reassembler
import com.vdcs.mobile.domain.logic.DisconnectCause
import com.vdcs.mobile.domain.model.Severity
import com.vdcs.mobile.domain.model.WarningType
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import kotlin.random.Random

class DemoTransport(
    private val scope: CoroutineScope,
    private val clock: () -> Long,
    override val mtu: Int = GattSpec.PREFERRED_MTU,
    random: Random = Random.Default,
) : BleTransport {
    private val confined = scope.confinedDispatcher()

    private val _link = MutableStateFlow<LinkState>(LinkState.Disconnected(null))
    override val link: StateFlow<LinkState> = _link.asStateFlow()

    private val frames = MutableSharedFlow<ByteArray>(extraBufferCapacity = 1024)
    override val incomingFrames: Flow<ByteArray> = frames.asSharedFlow()

    private val gateway = MutableSharedFlow<ByteArray>(extraBufferCapacity = 16, onBufferOverflow = BufferOverflow.DROP_OLDEST)
    override val gatewayStatus: Flow<ByteArray> = gateway.asSharedFlow()

    private val txOrder = Mutex()
    private val downCodec = BleFrameCodec()
    private val upCodec = BleFrameCodec()
    private val upReassembler = Reassembler(REASSEMBLY_TIMEOUT_MS)

    private val esp = DemoGateway(random)
    private val vehicle = DemoVehicle()

    private sealed interface DemoLink {
        data object Down : DemoLink
        class Connecting : DemoLink
        class Up(val periodic: List<Job>) : DemoLink
    }

    private var demoLink: DemoLink = DemoLink.Down
    private val linked: Boolean get() = demoLink is DemoLink.Up

    override suspend fun connect() = withContext(confined) {
        if (demoLink != DemoLink.Down) return@withContext
        val attempt = DemoLink.Connecting()
        demoLink = attempt
        _link.value = LinkState.Connecting(1, "데모 차량 연결 중")
        delay(CONNECT_DELAY_MS)
        if (demoLink !== attempt) return@withContext
        upReassembler.clear()
        esp.queryBusy = false
        demoLink = DemoLink.Up(
            listOf(
                scope.launch { every(FAST_PERIOD_MS) { vehicle.fastMessages(it) } },
                scope.launch { every(SLOW_PERIOD_MS) { vehicle.slowMessages(it) } },
            ),
        )
        _link.value = LinkState.Linked
    }

    override suspend fun disconnect() = drop(forgetApp = false)

    override suspend fun forget() = drop(forgetApp = true)

    override var foreground: Boolean = true
        private set

    override suspend fun setForeground(foreground: Boolean) = withContext(confined) {
        this@DemoTransport.foreground = foreground
    }

    override suspend fun readGatewayStatus(): ByteArray? = withContext(confined) { if (linked) esp.statusBytes() else null }

    override suspend fun writeAppState(payload: ByteArray): WriteResult = withContext(confined) {
        if (!linked) return@withContext WriteResult.NotConnected
        if (payload.size != 5) return@withContext WriteResult.Rejected(AttError.PAYLOAD_LENGTH_INVALID)
        val r = ByteReader(payload)
        val instance = r.u32(1)
        if (r.u8(0) > 1 || instance == 0L) return@withContext WriteResult.Rejected(AttError.VALUE_INVALID)
        esp.onAppInstance(instance)?.let { gateway.tryEmit(it) }
        WriteResult.Ok
    }

    override suspend fun writeAppCommand(frame: ByteArray): WriteResult = withContext(confined) {
        if (!linked) return@withContext WriteResult.NotConnected
        if (frame.isNotEmpty() && frame[0].toInt() != GattSpec.PROTOCOL_VERSION) {
            return@withContext WriteResult.Rejected(AttError.BLE_VERSION_UNSUPPORTED)
        }
        val f = upCodec.parseFrame(frame) ?: return@withContext WriteResult.Rejected(AttError.FRAGMENT_INVALID)
        if (f.messageType !in MessageType.M_REQUEST..MessageType.M_WARNING_ACK) {
            return@withContext WriteResult.Rejected(AttError.MESSAGE_TYPE_UNSUPPORTED)
        }
        val (type, payload) = upReassembler.accept(f, clock())
            ?: return@withContext if (f.fragmentIndex == f.fragmentCount - 1) WriteResult.Rejected(AttError.PAYLOAD_LENGTH_INVALID)
            else WriteResult.Ok
        when (type) {
            MessageType.M_REQUEST -> onRequest(payload)
            MessageType.M_QUERY -> onQuery(payload)
            else -> onWarningAck(payload)
        }
    }

    suspend fun openDoor() = changeState { vehicle.setDoorOpen(true, it) }

    suspend fun closeDoor() = changeState { vehicle.setDoorOpen(false, it) }

    suspend fun setCabinTemperature(celsius: Double) = changeState { vehicle.setCabinTemperature(celsius, it) }

    suspend fun raiseWarning(type: WarningType, severity: Severity = Severity.CAUTION) =
        changeState { vehicle.raiseWarning(type.raw, severity.raw, it) }

    suspend fun clearWarning(type: WarningType) = changeState { vehicle.clearWarning(type.raw, it) }

    suspend fun clearAllWarnings() = changeState { vehicle.clearAllWarnings(it) }

    private fun onRequest(payload: ByteArray): WriteResult {
        val r = ByteReader(payload)
        val requestId = r.u32(0)
        val action = vehicle.parseAction(r.u8(4), r.u8(5), payload.copyOfRange(6, 12))
        if (requestId == 0L || action == null) return WriteResult.Rejected(AttError.VALUE_INVALID)
        if (!esp.registered) return WriteResult.Rejected(AttError.NOT_REGISTERED)
        if (!esp.sessionReady) return WriteResult.Rejected(AttError.SESSION_NOT_READY)
        val session = esp.sessionId
        val known = vehicle.knownResult(session, requestId)
        if (known == null) {
            vehicle.remember(session, requestId, action)
            scope.launch { execute(session, requestId, action) }
        } else {
            scope.launch {
                val again = if (known.result == DemoVehicle.RESULT_NONE || esp.sessionId != session) emptyList()
                else listOf(vehicle.resultMessage(esp.context(), known))
                transmit(again)
            }
        }
        return WriteResult.Ok
    }

    private suspend fun execute(session: Long, requestId: Long, action: DemoVehicle.Action) {
        delay(ACCEPT_DELAY_MS)
        if (vehicle.rejects(action)) {
            transmit(settle(session, requestId, DemoVehicle.RESULT_REJECTED, DemoVehicle.REASON_DOOR_OPEN))
            return
        }
        transmit(settle(session, requestId, DemoVehicle.RESULT_ACCEPTED, DemoVehicle.REASON_NONE))
        delay(EXECUTE_DELAY_MS)
        if (!linked || esp.sessionId != session) return
        val alreadyThere = vehicle.apply(action)
        val reason = if (alreadyThere) DemoVehicle.REASON_ALREADY_AT_TARGET else DemoVehicle.REASON_NONE
        transmit(settle(session, requestId, DemoVehicle.RESULT_DONE, reason) + vehicle.affected(action, esp.context()))
    }

    private fun settle(session: Long, requestId: Long, result: Int, reason: Int): List<Downlink> {
        val entry = vehicle.settle(session, requestId, result, reason) ?: return emptyList()
        if (!linked || esp.sessionId != session) return emptyList()
        return listOf(vehicle.resultMessage(esp.context(), entry))
    }

    private fun onQuery(payload: ByteArray): WriteResult {
        val r = ByteReader(payload)
        val queryId = r.u32(0)
        val scopeRaw = r.u8(4)
        val requestSession = r.u64(6)
        val requestId = r.u32(14)
        if (queryId == 0L || scopeRaw > DemoVehicle.SCOPE_ALL || r.u8(5) != 0) return WriteResult.Rejected(AttError.VALUE_INVALID)
        if (!esp.registered) return WriteResult.Rejected(AttError.NOT_REGISTERED)
        if (esp.queryBusy) return WriteResult.Rejected(AttError.QUERY_BUSY)
        if (esp.sessionId == 0L) return WriteResult.Rejected(AttError.SESSION_NOT_READY)
        esp.queryBusy = true
        val session = esp.sessionId
        scope.launch {
            delay(QUERY_DELAY_MS)
            if (!linked || esp.sessionId != session) return@launch
            esp.queryBusy = false
            val c = esp.context(queryId)
            val (items, status) = vehicle.queryItems(c, scopeRaw, requestSession, requestId)
            val becameReady = scopeRaw == DemoVehicle.SCOPE_ALL && !esp.sessionReady
            if (becameReady) esp.sessionReady = true
            val readyStatus = if (becameReady) esp.statusBytes() else null
            transmit(items + (MessageType.M_QUERY_END to DemoDownlink.queryEnd(c, scopeRaw, status, items.size)))
            readyStatus?.let { gateway.tryEmit(it) }
        }
        return WriteResult.Ok
    }

    private fun onWarningAck(payload: ByteArray): WriteResult {
        val r = ByteReader(payload)
        if (!esp.registered) return WriteResult.Rejected(AttError.NOT_REGISTERED)
        val update = vehicle.acknowledge(r.u8(0), r.u32(1), bootMatches = r.u32(5) == esp.domainBootId, c = esp.context())
        if (update.isNotEmpty()) scope.launch { transmit(update) }
        return WriteResult.Ok
    }

    private suspend fun every(periodMs: Long, build: (DemoDownlink.Context) -> List<Downlink>) {
        while (true) {
            delay(periodMs)
            transmit(if (linked && esp.sessionId != 0L) build(esp.context()) else emptyList())
        }
    }

    private suspend fun changeState(block: (DemoDownlink.Context) -> List<Downlink>) = withContext(confined) {
        val messages = block(esp.context())
        transmit(if (linked && esp.sessionId != 0L) messages else emptyList())
    }

    private suspend fun transmit(messages: List<Downlink>) {
        if (messages.isEmpty()) return
        txOrder.withLock {
            for ((type, payload) in messages) {
                for (f in downCodec.encode(type, payload, mtu)) frames.emit(f)
            }
        }
    }

    private suspend fun drop(forgetApp: Boolean) = withContext(confined) {
        (demoLink as? DemoLink.Up)?.periodic?.forEach { it.cancel() }
        demoLink = DemoLink.Down
        upReassembler.clear()
        esp.onLinkDown(forgetApp)
        _link.value = LinkState.Disconnected(DisconnectCause.USER)
    }

    private companion object {
        const val CONNECT_DELAY_MS = 300L
        const val ACCEPT_DELAY_MS = 50L
        const val EXECUTE_DELAY_MS = 300L
        const val QUERY_DELAY_MS = 20L
        const val FAST_PERIOD_MS = 200L
        const val SLOW_PERIOD_MS = 1_000L
        const val REASSEMBLY_TIMEOUT_MS = 1_000L
    }
}
