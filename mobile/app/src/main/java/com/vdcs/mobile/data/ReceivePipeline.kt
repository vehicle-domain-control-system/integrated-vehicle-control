package com.vdcs.mobile.data

import com.vdcs.mobile.data.protocol.BleFrameCodec
import com.vdcs.mobile.data.protocol.MessageCodec
import com.vdcs.mobile.data.protocol.Reassembler
import com.vdcs.mobile.data.protocol.VehicleMessage

internal class ReceivePipeline(
    private val frameCodec: BleFrameCodec,
    private val reassembler: Reassembler,
    private val messageCodec: MessageCodec,
) {
    fun accept(bytes: ByteArray, now: Long): VehicleMessage? {
        val frame = frameCodec.parseFrame(bytes) ?: return null
        val (type, payload) = reassembler.accept(frame, now) ?: return null
        return messageCodec.decode(type, payload)
    }

    fun evictExpired(now: Long) = reassembler.evictExpired(now)

    fun clear() = reassembler.clear()
}
