package com.vdcs.mobile.data.protocol

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class BleFrameCodecTest {
    private val codec = BleFrameCodec()

    @Test fun `MTU 185 에서 12 B 요청은 조각 1개 - 헤더 8 B`() {
        val frames = codec.encode(MessageType.M_REQUEST, ByteArray(12) { it.toByte() }, 185)
        assertEquals(1, frames.size)
        assertEquals("01 10 01 00 0C 00 00 01", frames[0].copyOf(8).toHex())
        assertEquals(20, frames[0].size)
    }

    @Test fun `MTU 23 에서 조각 데이터는 12 B - 20 B M_QUERY 는 2조각`() {
        val payload = ByteArray(20) { (it + 1).toByte() }
        val frames = codec.encode(MessageType.M_QUERY, payload, 23)
        assertEquals(2, frames.size)
        assertEquals(20, frames[0].size)
        assertEquals(8 + 8, frames[1].size)
        frames.forEachIndexed { i, f ->
            assertEquals(i, f[6].toInt())
            assertEquals(2, f[7].toInt())
            assertEquals(frames[0][2], f[2])
        }
    }

    @Test fun `TRANSFER_ID 는 메시지마다 증가하고 65535 다음은 1`() {
        repeat(0xFFFE) { codec.encode(MessageType.M_REQUEST, ByteArray(12), 185) }
        val last = codec.encode(MessageType.M_REQUEST, ByteArray(12), 185)[0]
        assertEquals(0xFFFF, ByteReader(last).u16(2))
        val wrapped = codec.encode(MessageType.M_REQUEST, ByteArray(12), 185)[0]
        assertEquals(1, ByteReader(wrapped).u16(2))
    }

    @Test fun `#56 §5_2 단일 조각 예시 헤더를 파싱한다`() {
        val raw = hex("01 20 35 00 2C 00 00 01") + ByteArray(44)
        val frame = codec.parseFrame(raw)!!
        assertEquals(MessageType.M_RESULT, frame.messageType)
        assertEquals(0x35, frame.transferId)
        assertEquals(44, frame.totalPayloadLength)
        assertEquals(0, frame.fragmentIndex)
        assertEquals(1, frame.fragmentCount)
    }

    @Test fun `버전 불일치·TRANSFER_ID 0·INDEX 범위 초과는 폐기`() {
        assertNull(codec.parseFrame(hex("02 20 35 00 2C 00 00 01")))
        assertNull(codec.parseFrame(hex("01 20 00 00 2C 00 00 01")))
        assertNull(codec.parseFrame(hex("01 20 35 00 2C 00 01 01")))
        assertNull(codec.parseFrame(hex("01 20 35")))
    }

    @Test fun `분할한 것을 조립하면 원래 payload`() {
        val payload = ByteArray(44) { (it * 3).toByte() }
        val sender = BleFrameCodec()
        val reassembler = Reassembler(1000)
        val frames = sender.encode(MessageType.M_RESULT, payload, 23)
        var done: Pair<Int, ByteArray>? = null
        frames.forEach { done = reassembler.accept(codec.parseFrame(it)!!, 0) ?: done }
        assertEquals(MessageType.M_RESULT, done!!.first)
        assertArrayEquals(payload, done!!.second)
    }

    @Test fun `TOTAL_PAYLOAD_LENGTH 와 실제 조립 길이가 다르면 폐기`() {
        val raw = hex("01 20 05 00 2C 00 00 01") + ByteArray(43)
        assertNull(Reassembler(1000).accept(codec.parseFrame(raw)!!, 0))
    }

    @Test fun `Type 고정 길이와 다르면 조립 후 폐기`() {
        val frames = BleFrameCodec().encode(MessageType.M_RESULT, ByteArray(43), 185)
        assertNull(Reassembler(1000).accept(codec.parseFrame(frames[0])!!, 0))
    }

    @Test fun `첫 조각 기준 1000 ms 넘으면 미완성 조립 폐기`() {
        val frames = BleFrameCodec().encode(MessageType.M_RESULT, ByteArray(44), 23)
        val r = Reassembler(1000)
        r.accept(codec.parseFrame(frames[0])!!, 0)
        frames.drop(1).forEach { assertNull(r.accept(codec.parseFrame(it)!!, 1001)) }
    }

    @Test fun `TRANSFER_ID 가 바뀌면 이전 미완성은 버리고 새 메시지를 받는다`() {
        val sender = BleFrameCodec()
        val first = sender.encode(MessageType.M_RESULT, ByteArray(44), 23)
        val second = sender.encode(MessageType.M_QUERY_END, ByteArray(24) { 9 }, 185)
        val r = Reassembler(1000)
        r.accept(codec.parseFrame(first[0])!!, 0)
        val done = r.accept(codec.parseFrame(second[0])!!, 10)
        assertEquals(MessageType.M_QUERY_END, done!!.first)
        first.drop(1).forEach { assertNull(r.accept(codec.parseFrame(it)!!, 20)) }
    }
}
