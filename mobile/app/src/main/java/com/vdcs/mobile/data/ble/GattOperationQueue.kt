package com.vdcs.mobile.data.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothProfile
import android.content.Context
import android.os.Build
import android.util.Log
import com.vdcs.mobile.domain.logic.BondSecurity
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.TimeoutCancellationException
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withTimeout
import kotlinx.coroutines.withTimeoutOrNull
import java.util.UUID

@SuppressLint("MissingPermission")
internal class GattOperationQueue(
    private val appContext: Context,
    private val scope: CoroutineScope,
    private val connectTimeoutMs: Long,
    private val opTimeoutMs: Long,
    private val isLinked: () -> Boolean,
    private val onNotify: (UUID, ByteArray) -> Unit,
    private val onDisconnected: (status: Int) -> Unit,
) {
    class OpResult(val status: Int, val value: ByteArray?)

    private class PendingOp(val kind: GattOpKind) {
        val done = CompletableDeferred<OpResult?>()
    }

    class Op internal constructor(private val pending: CompletableDeferred<OpResult?>) {
        fun fail(status: Int) {
            pending.complete(OpResult(status, null))
        }
    }

    private val ops = Mutex()
    private var gatt: BluetoothGatt? = null
    private var pending: PendingOp? = null
    private var connectWaiter: CompletableDeferred<Boolean>? = null

    var mtu: Int = GattSpec.MIN_MTU
        private set

    val isOpen: Boolean get() = gatt != null

    var lastDisconnectStatus: Int? = null
        private set

    suspend fun open(device: BluetoothDevice) {
        if (gatt != null) close()
        lastDisconnectStatus = null
        val connected = CompletableDeferred<Boolean>()
        connectWaiter = connected
        try {
            val g = device.connectGatt(appContext, false, callback, BluetoothDevice.TRANSPORT_LE)
                ?: throw LinkFailure("연결 시작 실패")
            gatt = g
            val ok = withTimeoutOrNull(connectTimeoutMs) { connected.await() } ?: throw LinkFailure("연결 시간 초과")
            if (!ok || gatt !== g) throw LinkFailure("연결 실패")
        } finally {
            if (connectWaiter === connected) connectWaiter = null
        }
    }

    fun hasService(): Boolean = gatt?.getService(GattSpec.SERVICE) != null

    suspend fun op(kind: GattOpKind, start: (BluetoothGatt, Op) -> Unit): OpResult? =
        opResult(kind, start)?.takeIf { it.status == BluetoothGatt.GATT_SUCCESS }

    private suspend fun opResult(kind: GattOpKind, start: (BluetoothGatt, Op) -> Unit): OpResult? = ops.withLock {
        val g = gatt ?: return@withLock null
        val p = PendingOp(kind)
        pending = p
        try {
            start(g, Op(p.done))
            withTimeout(opTimeoutMs) { p.done.await() }
        } catch (e: TimeoutCancellationException) {
            Log.w(BLE_LOG_TAG, "GATT 작업 시간 초과: $kind")
            null
        } finally {
            pending = null
        }
    }

    suspend fun subscribe(uuid: UUID) {
        val g = gatt ?: throw LinkFailure("연결 끊김")
        val ch = characteristic(g, uuid) ?: throw LinkFailure("특성 없음 $uuid")
        if (!g.setCharacteristicNotification(ch, true)) throw LinkFailure("알림 설정 실패")
        val cccd = ch.getDescriptor(GattSpec.CCCD) ?: throw LinkFailure("CCCD 없음")
        val value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
        val r = opResult(GattOpKind.DescriptorWrite(uuid)) { gg, op ->
            val started = if (Build.VERSION.SDK_INT >= 33) {
                gg.writeDescriptor(cccd, value) == BluetoothGatt.GATT_SUCCESS
            } else {
                @Suppress("DEPRECATION")
                cccd.value = value
                @Suppress("DEPRECATION")
                gg.writeDescriptor(cccd)
            }
            if (!started) op.fail(AttError.LOCAL_START_FAILED)
        }
        when {
            r == null -> throw LinkFailure("알림 구독 실패")
            BondSecurity.isAttSecurityFailure(r.status) -> throw BondMismatch("알림 구독 거절 (status ${r.status})")
            r.status != BluetoothGatt.GATT_SUCCESS -> throw LinkFailure("알림 구독 실패 (status ${r.status})")
        }
    }

    suspend fun read(uuid: UUID): ByteArray? = op(GattOpKind.Read(uuid)) { g, op ->
        val ch = characteristic(g, uuid)
        if (ch == null || !g.readCharacteristic(ch)) op.fail(AttError.LOCAL_START_FAILED)
    }?.value

    suspend fun write(uuid: UUID, value: ByteArray): WriteResult = ops.withLock {
        val g = gatt ?: return@withLock WriteResult.NotConnected
        val ch = characteristic(g, uuid) ?: return@withLock WriteResult.Rejected(AttError.LOCAL_START_FAILED)
        val p = PendingOp(GattOpKind.Write(uuid))
        pending = p
        try {
            val started = try {
                startWrite(g, ch, value)
            } catch (e: SecurityException) {
                Log.w(BLE_LOG_TAG, "Write 권한 없음", e)
                false
            }
            if (!started) return@withLock WriteResult.Rejected(AttError.LOCAL_START_FAILED)
            val r = withTimeoutOrNull(opTimeoutMs) { p.done.await() }
            when {
                r != null -> if (r.status == BluetoothGatt.GATT_SUCCESS) WriteResult.Ok else WriteResult.Rejected(r.status)
                p.done.isCompleted -> WriteResult.NotConnected
                isLinked() && gatt === g -> {
                    Log.w(BLE_LOG_TAG, "Write 응답 시간 초과: $uuid")
                    WriteResult.Rejected(AttError.LOCAL_TIMEOUT)
                }
                else -> WriteResult.NotConnected
            }
        } finally {
            pending = null
        }
    }

    private fun startWrite(g: BluetoothGatt, ch: BluetoothGattCharacteristic, value: ByteArray): Boolean =
        if (Build.VERSION.SDK_INT >= 33) {
            g.writeCharacteristic(ch, value, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) == BluetoothGatt.GATT_SUCCESS
        } else {
            @Suppress("DEPRECATION")
            ch.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
            @Suppress("DEPRECATION")
            ch.value = value
            @Suppress("DEPRECATION")
            g.writeCharacteristic(ch)
        }

    fun close() {
        failPending()
        val g = gatt
        gatt = null
        g?.let { runCatching { it.disconnect(); it.close() } }
        mtu = GattSpec.MIN_MTU
    }

    private fun failPending() {
        pending?.done?.complete(null)
        connectWaiter?.complete(false)
    }

    private fun characteristic(g: BluetoothGatt, uuid: UUID): BluetoothGattCharacteristic? =
        g.getService(GattSpec.SERVICE)?.getCharacteristic(uuid)

    private fun hop(g: BluetoothGatt, block: () -> Unit) {
        if (!scope.isActive) {
            runCatching { g.close() }
            return
        }
        scope.launch {
            if (gatt !== g) runCatching { g.close() } else block()
        }
    }

    private fun complete(kind: GattOpKind, status: Int, value: ByteArray? = null) {
        val p = pending
        if (p == null || p.kind != kind) {
            Log.w(BLE_LOG_TAG, "짝 없는 GATT 콜백 무시: $kind (진행 중 ${p?.kind})")
            return
        }
        p.done.complete(OpResult(status, value))
    }

    private val callback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) = hop(g) {
            when (newState) {
                BluetoothProfile.STATE_CONNECTED ->
                    if (status == BluetoothGatt.GATT_SUCCESS) {
                        connectWaiter?.complete(true)
                    } else {
                        close()
                    }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    lastDisconnectStatus = status
                    close()
                    onDisconnected(status)
                }
            }
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) = hop(g) { complete(GattOpKind.Discover, status) }

        override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) = hop(g) {
            if (status == BluetoothGatt.GATT_SUCCESS) this@GattOperationQueue.mtu = mtu
            complete(GattOpKind.Mtu, status)
        }

        override fun onDescriptorWrite(g: BluetoothGatt, d: BluetoothGattDescriptor, status: Int) =
            hop(g) { complete(GattOpKind.DescriptorWrite(d.characteristic.uuid), status) }

        override fun onCharacteristicWrite(g: BluetoothGatt, c: BluetoothGattCharacteristic, status: Int) =
            hop(g) { complete(GattOpKind.Write(c.uuid), status) }

        override fun onCharacteristicRead(g: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray, status: Int) =
            hop(g) { complete(GattOpKind.Read(c.uuid), status, value) }

        @Deprecated("API 32 이하 경로")
        @Suppress("DEPRECATION")
        override fun onCharacteristicRead(g: BluetoothGatt, c: BluetoothGattCharacteristic, status: Int) {
            if (Build.VERSION.SDK_INT >= 33) return
            val value = c.value?.copyOf()
            val uuid = c.uuid
            hop(g) { complete(GattOpKind.Read(uuid), status, value) }
        }

        override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray) =
            hop(g) { onNotify(c.uuid, value) }

        @Deprecated("API 32 이하 경로")
        @Suppress("DEPRECATION")
        override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic) {
            if (Build.VERSION.SDK_INT >= 33) return
            val value = c.value.copyOf()
            hop(g) { onNotify(c.uuid, value) }
        }
    }
}

internal sealed interface GattOpKind {
    data object Discover : GattOpKind
    data object Mtu : GattOpKind
    data class DescriptorWrite(val characteristic: UUID) : GattOpKind
    data class Write(val characteristic: UUID) : GattOpKind
    data class Read(val characteristic: UUID) : GattOpKind
}

internal class LinkFailure(message: String) : Exception(message)

internal class BondMismatch(message: String) : Exception(message)
