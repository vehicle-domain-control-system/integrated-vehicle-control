package com.vdcs.mobile.data.protocol

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class GattPayloadsTest {
    @Test fun `Gateway Status 21 B - 헤더 없이 바로 필드`() {
        val raw = ByteWriter(21).u8(0, 1).u8(1, 1).u8(2, 1).u8(3, 1)
            .u32(4, 7).u64(8, 0x1122334455667788).u32(16, 3).u8(20, 1).toByteArray()
        val s = GattPayloads.decodeGatewayStatus(raw)!!
        assertTrue(s.registered)
        assertTrue(s.domainLinkUp && s.sessionReady)
        assertEquals(7L, s.deviceContextId)
        assertEquals(0x1122334455667788, s.sessionId)
        assertEquals(3L, s.domainBootId)
        assertEquals(1, s.domainState)
    }

    @Test fun `Gateway Status 길이·버전이 다르면 폐기`() {
        assertNull(GattPayloads.decodeGatewayStatus(ByteArray(20)))
        assertNull(GattPayloads.decodeGatewayStatus(ByteArray(21)))
        assertNull(GattPayloads.decodeGatewayStatus(ByteArray(29) { 1 }))
    }

    @Test fun `App State 5 B - 헤더 없음`() {
        assertEquals("01 78 56 34 12", GattPayloads.encodeAppState(true, 0x12345678).toHex())
        assertEquals("00 01 00 00 00", GattPayloads.encodeAppState(false, 1).toHex())
    }

    @Test(expected = IllegalArgumentException::class)
    fun `APP_INSTANCE_ID 0 금지`() {
        GattPayloads.encodeAppState(true, 0)
    }

    @Test fun `최소 지원 MTU 에서 Gateway Status 21 B 가 Notify 한 번에 실린다 (D23)`() {
        assertTrue(com.vdcs.mobile.data.ble.GattSpec.MIN_SUPPORTED_MTU - 3 >= GatewayStatus.LENGTH)
        assertTrue(com.vdcs.mobile.data.ble.GattSpec.MIN_MTU - 3 < GatewayStatus.LENGTH)
    }
}
