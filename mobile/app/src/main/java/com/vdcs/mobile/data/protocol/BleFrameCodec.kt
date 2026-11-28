package com.vdcs.mobile.data.protocol

import com.vdcs.mobile.data.ble.GattSpec

class BleFrame(
    val messageType: Int,
    val transferId: Int,
    val totalPayloadLength: Int,
    val fragmentIndex: Int,
    val fragmentCount: Int,
    val data: ByteArray,
)

class BleFrameCodec {
    private var nextTransferId = 1

    fun encode(messageType: Int, payload: ByteArray, negotiatedMtu: Int): List<ByteArray> {
        val dataMax = GattSpec.fragmentDataMax(negotiatedMtu)
        require(dataMax > 0) { "MTU $negotiatedMtu 로는 헤더도 못 싣는다" }
        val count = maxOf(1, (payload.size + dataMax - 1) / dataMax)
        require(count <= 255) { "FRAGMENT_COUNT 는 u8 — ${payload.size} B 는 MTU $negotiatedMtu 에 안 들어간다" }
        val transferId = issueTransferId()
        return (0 until count).map { index ->
            val from = index * dataMax
            val chunk = payload.copyOfRange(from, minOf(payload.size, from + dataMax))
            ByteWriter(GattSpec.HEADER_SIZE + chunk.size)
                .u8(0, GattSpec.PROTOCOL_VERSION)
                .u8(1, messageType)
                .u16(2, transferId)
                .u16(4, payload.size)
                .u8(6, index)
                .u8(7, count)
                .bytes(GattSpec.HEADER_SIZE, chunk)
                .toByteArray()
        }
    }

    fun parseFrame(raw: ByteArray): BleFrame? {
        if (raw.size < GattSpec.HEADER_SIZE) return null
        val r = ByteReader(raw)
        if (r.u8(0) != GattSpec.PROTOCOL_VERSION) return null
        val transferId = r.u16(2)
        val total = r.u16(4)
        val index = r.u8(6)
        val count = r.u8(7)
        if (transferId == 0 || count == 0 || index >= count) return null
        val data = raw.copyOfRange(GattSpec.HEADER_SIZE, raw.size)
        if (data.size > total) return null
        return BleFrame(r.u8(1), transferId, total, index, count, data)
    }

    fun resetTransferId() {
        nextTransferId = 1
    }

    private fun issueTransferId(): Int {
        val id = nextTransferId
        nextTransferId = if (id == 0xFFFF) 1 else id + 1
        return id
    }
}

class Reassembler(
    private val timeoutMs: Long,
) {
    private class Pending(val head: BleFrame, val startedAtMs: Long) {
        val parts = arrayOfNulls<ByteArray>(head.fragmentCount)
    }

    private var pending: Pending? = null

    fun accept(frame: BleFrame, nowMs: Long): Pair<Int, ByteArray>? {
        evictExpired(nowMs)
        var current = pending
        if (current != null && current.head.transferId != frame.transferId) {
            current = null
        }
        if (current == null) {
            current = Pending(frame, nowMs)
            pending = current
        } else if (!sameMessage(current.head, frame)) {
            pending = null
            return null
        }
        current.parts[frame.fragmentIndex] = frame.data
        if (current.parts.any { it == null }) return null

        pending = null
        val payload = current.parts.fold(ByteArray(0)) { acc, part -> acc + part!! }
        if (payload.size != frame.totalPayloadLength) return null
        val expected = MessageType.PAYLOAD_LENGTH[frame.messageType]
        if (expected != null && expected != payload.size) return null
        return frame.messageType to payload
    }

    fun evictExpired(nowMs: Long) {
        val current = pending ?: return
        if (nowMs - current.startedAtMs > timeoutMs) pending = null
    }

    fun clear() {
        pending = null
    }

    private fun sameMessage(a: BleFrame, b: BleFrame): Boolean =
        a.messageType == b.messageType &&
            a.totalPayloadLength == b.totalPayloadLength &&
            a.fragmentCount == b.fragmentCount
}
