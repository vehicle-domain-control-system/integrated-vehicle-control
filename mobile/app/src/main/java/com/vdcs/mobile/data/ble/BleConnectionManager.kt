package com.vdcs.mobile.data.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothManager
import android.content.Context
import android.util.Log
import com.vdcs.mobile.data.VdcsConfig
import com.vdcs.mobile.data.confinedDispatcher
import com.vdcs.mobile.domain.logic.DisconnectCause
import com.vdcs.mobile.domain.logic.ReconnectDecision
import com.vdcs.mobile.domain.logic.ReconnectPolicy
import com.vdcs.mobile.domain.logic.ReconnectStop
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.CoroutineStart
import kotlinx.coroutines.Job
import kotlinx.coroutines.cancelAndJoin
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.delay
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.util.UUID

@SuppressLint("MissingPermission")
class BleConnectionManager(
    context: Context,
    private val reconnectPolicy: ReconnectPolicy,
    private val scope: CoroutineScope,
    private val clock: () -> Long = System::currentTimeMillis,
    config: VdcsConfig = VdcsConfig(),
) : BleTransport {
    private val appContext = context.applicationContext
    private val adapter = appContext.getSystemService(BluetoothManager::class.java)?.adapter
    private val confined = scope.confinedDispatcher()

    private val _link = MutableStateFlow<LinkState>(LinkState.Disconnected(null))
    override val link: StateFlow<LinkState> = _link.asStateFlow()

    private val _incoming = MutableSharedFlow<ByteArray>(extraBufferCapacity = 256)
    override val incomingFrames: SharedFlow<ByteArray> = _incoming

    private val _gateway = MutableSharedFlow<ByteArray>(replay = 1, extraBufferCapacity = 8)
    override val gatewayStatus: SharedFlow<ByteArray> = _gateway

    private val gatt = GattOperationQueue(
        appContext = appContext,
        scope = scope,
        connectTimeoutMs = config.gattConnectTimeoutMs,
        opTimeoutMs = config.gattOpTimeoutMs,
        isLinked = { _link.value == LinkState.Linked },
        onNotify = ::dispatch,
        onDisconnected = ::onGattDisconnected,
    )
    private val procedure = BleLinkProcedure(
        appContext = appContext,
        adapter = adapter,
        scanner = BleScanner(adapter, config.scanTimeoutMs),
        bonding = BleBonding(appContext, config.bondTimeoutMs),
        gatt = gatt,
    )

    override val mtu: Int get() = gatt.mtu

    override var foreground: Boolean = true
        private set

    private var userClosed = false

    private var linkJob: Job? = null

    private var waitingJob: Job? = null

    private var teardown: CompletableDeferred<Unit>? = null

    private var reconnectWindowStart = 0L

    override suspend fun connect() = withContext(confined) {
        awaitTeardown()
        val job = startOrJoinable() ?: return@withContext
        job.start()
        job.join()
    }

    private suspend fun awaitTeardown() {
        while (true) {
            val t = teardown ?: return
            if (t.isCompleted) return
            t.await()
        }
    }

    private fun startOrJoinable(): Job? {
        var running = linkJob?.takeIf { !it.isCompleted }
        if (running != null && running === waitingJob) {
            running.cancel()
            waitingJob = null
            running = null
        }
        return when {
            running != null -> running
            _link.value == LinkState.Linked && gatt.isOpen -> null
            else -> {
                userClosed = false
                reconnectWindowStart = 0
                scope.launch(start = CoroutineStart.LAZY) { establish(attempt = 1) }.also { linkJob = it }
            }
        }
    }

    override suspend fun disconnect() = withContext(confined) {
        teardown?.takeIf { !it.isCompleted }?.let {
            it.await()
            return@withContext
        }
        val done = CompletableDeferred<Unit>().also { teardown = it }
        userClosed = true
        waitingJob = null
        val job = linkJob.also { linkJob = null }
        try {
            job?.cancelAndJoin()
        } finally {
            gatt.close()
            _link.value = LinkState.Disconnected(DisconnectCause.USER)
            done.complete(Unit)
        }
    }

    override suspend fun setForeground(foreground: Boolean) = withContext(confined) {
        this@BleConnectionManager.foreground = foreground
        if (foreground) resumeReconnect() else suspendReconnect()
    }

    private fun suspendReconnect() {
        val waiting = waitingJob ?: return
        waiting.cancel()
        waitingJob = null
        linkJob = null
        _link.value = LinkState.Disconnected(DisconnectCause.LINK_LOST, null, ReconnectStop.INACTIVE)
    }

    private fun resumeReconnect() {
        val stopped = _link.value as? LinkState.Disconnected ?: return
        if (stopped.reconnectStop != ReconnectStop.INACTIVE || userClosed) return
        reconnectWindowStart = 0
        linkJob = scope.launch { establish(attempt = 1) }
    }

    override suspend fun forget() {
        disconnect()
        procedure.forgetDevice()
    }

    override suspend fun readGatewayStatus(): ByteArray? = withContext(confined) {
        try {
            gatt.read(GattSpec.CHAR_GATEWAY_STATUS)
        } catch (e: SecurityException) {
            Log.w(BLE_LOG_TAG, "Gateway READ 권한 없음", e)
            null
        }
    }

    override suspend fun writeAppCommand(frame: ByteArray): WriteResult = write(GattSpec.CHAR_APP_COMMAND, frame)

    override suspend fun writeAppState(payload: ByteArray): WriteResult = write(GattSpec.CHAR_APP_STATE, payload)

    private suspend fun write(uuid: UUID, value: ByteArray): WriteResult = withContext(confined) {
        if (_link.value != LinkState.Linked) WriteResult.NotConnected else gatt.write(uuid, value)
    }

    private suspend fun checkAlive() {
        currentCoroutineContext().ensureActive()
        if (userClosed) throw CancellationException("사용자 해제")
    }

    private fun settleDisconnected(cause: DisconnectCause, detail: String) {
        if (!userClosed) _link.value = LinkState.Disconnected(cause, detail)
    }

    private suspend fun establish(attempt: Int) {
        if (userClosed) return
        val bt = adapter
        if (bt == null || !bt.isEnabled) {
            settleDisconnected(DisconnectCause.LINK_LOST, "블루투스가 꺼져 있음")
            return
        }
        try {
            val outcome = procedure.run(onStep = { _link.value = LinkState.Connecting(attempt, it) }, checkAlive = ::checkAlive)
            when (outcome) {
                LinkOutcome.Linked -> {
                    reconnectWindowStart = 0
                    _link.value = LinkState.Linked
                }
                is LinkOutcome.Settled -> settleDisconnected(outcome.cause, outcome.detail)
            }
        } catch (e: CancellationException) {
            gatt.close()
            settleDisconnected(DisconnectCause.LINK_LOST, "연결 취소")
            throw e
        } catch (e: LinkFailure) {
            Log.w(BLE_LOG_TAG, "링크 실패: ${e.message}")
            gatt.close()
            onLinkLost(e.message)
        } catch (e: SecurityException) {
            Log.w(BLE_LOG_TAG, "BLE 권한 없음", e)
            gatt.close()
            settleDisconnected(DisconnectCause.LINK_LOST, "블루투스 권한 필요")
        } catch (e: IllegalStateException) {
            Log.w(BLE_LOG_TAG, "BLE 어댑터 오류", e)
            gatt.close()
            settleDisconnected(DisconnectCause.LINK_LOST, "블루투스가 꺼져 있음")
        }
    }

    private fun onGattDisconnected(status: Int) {
        if (_link.value == LinkState.Linked) onLinkLost("연결 끊김 (status $status)")
    }

    private fun onLinkLost(detail: String?) {
        if (userClosed) return
        val now = clock()
        if (reconnectWindowStart == 0L) reconnectWindowStart = now
        val attempt = (_link.value as? LinkState.Connecting)?.attempt?.plus(1) ?: 1
        val delayMs = when (val decision = reconnectPolicy.decide(attempt, now - reconnectWindowStart, foreground)) {
            is ReconnectDecision.Stop -> {
                _link.value = LinkState.Disconnected(DisconnectCause.LINK_LOST, detail, decision.reason)
                return
            }
            is ReconnectDecision.Retry -> decision.delayMs
        }
        _link.value = LinkState.Connecting(attempt, "재연결 대기")
        scope.launch(start = CoroutineStart.LAZY) {
            delay(delayMs)
            if (waitingJob === coroutineContext[Job]) waitingJob = null
            establish(attempt)
        }.also {
            linkJob = it
            waitingJob = it
        }.start()
    }

    private fun dispatch(uuid: UUID, value: ByteArray) {
        when (uuid) {
            GattSpec.CHAR_VEHICLE_DATA -> _incoming.tryEmit(value)
            GattSpec.CHAR_GATEWAY_STATUS -> _gateway.tryEmit(value)
        }
    }
}
