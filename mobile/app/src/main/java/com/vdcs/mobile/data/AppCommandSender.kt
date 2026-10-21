package com.vdcs.mobile.data

import com.vdcs.mobile.data.ble.BleTransport
import com.vdcs.mobile.data.ble.WriteResult
import com.vdcs.mobile.data.protocol.BleFrameCodec
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock

internal class AppCommandSender(
    private val transport: BleTransport,
    private val frameCodec: BleFrameCodec,
) {
    private val messageOrder = Mutex()

    suspend fun send(type: Int, payload: ByteArray): WriteResult = messageOrder.withLock {
        for (frame in frameCodec.encode(type, payload, transport.mtu)) {
            val r = transport.writeAppCommand(frame)
            if (r != WriteResult.Ok) return@withLock r
        }
        WriteResult.Ok
    }
}

internal fun WriteResult.describe(): String = when (this) {
    WriteResult.Ok -> "성공"
    WriteResult.NotConnected -> "차량과 연결되어 있지 않습니다"
    is WriteResult.Rejected -> reason
}
