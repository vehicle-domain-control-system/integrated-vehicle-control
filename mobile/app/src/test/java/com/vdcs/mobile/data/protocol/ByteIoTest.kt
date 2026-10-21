package com.vdcs.mobile.data.protocol

import org.junit.Assert.assertEquals
import org.junit.Test

class ByteIoTest {
    @Test fun `u16 u32 u64 는 Little Endian 으로 왕복한다`() {
        val bytes = ByteWriter(14).u16(0, 0xBEEF).u32(2, 0xDEADBEEFL).u64(6, 0x1122334455667788).toByteArray()
        assertEquals("EF BE EF BE AD DE 88 77 66 55 44 33 22 11", bytes.toHex())
        val r = ByteReader(bytes)
        assertEquals(0xBEEF, r.u16(0))
        assertEquals(0xDEADBEEFL, r.u32(2))
        assertEquals(0x1122334455667788, r.u64(6))
    }

    @Test fun `i16 는 2의 보수 - 무효값 -32768 을 그대로 읽는다`() {
        val bytes = ByteWriter(4).i16(0, -1).i16(2, -32768).toByteArray()
        assertEquals("FF FF 00 80", bytes.toHex())
        assertEquals(-1, ByteReader(bytes).i16(0))
        assertEquals(-32768, ByteReader(bytes).i16(2))
    }

    @Test fun `u32 최댓값은 음수가 되지 않는다`() {
        assertEquals(0xFFFFFFFFL, ByteReader(hex("FF FF FF FF")).u32(0))
    }
}
