package com.vdcs.mobile.data.protocol

import com.vdcs.mobile.domain.model.FanLevel
import com.vdcs.mobile.domain.model.Quality
import com.vdcs.mobile.domain.model.ReadState
import com.vdcs.mobile.domain.model.Rgb
import com.vdcs.mobile.domain.model.Severity
import com.vdcs.mobile.domain.model.UserRequest
import com.vdcs.mobile.domain.model.Warning
import com.vdcs.mobile.domain.model.WarningType
import com.vdcs.mobile.domain.repository.QueryScope
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class MessageCodecTest {
    private val codec = MessageCodec()

    @Test fun `#56 §50 Door Unlock REQUEST_ID 52 바이트와 일치`() {
        assertEquals(
            "34 00 00 00 01 01 00 00 00 00 00 00",
            codec.encodeRequest(52, UserRequest.Door(lock = false)).toHex(),
        )
    }

    @Test fun `목표 온도는 0_01도 단위 i16`() {
        assertEquals("01 00 00 00 02 01 CA 08 00 00 00 00", codec.encodeRequest(1, UserRequest.TargetTemperature(22.5)).toHex())
        assertEquals("0C FE", codec.encodeRequest(1, UserRequest.TargetTemperature(-5.0)).copyOfRange(6, 8).toHex())
    }

    @Test fun `조명·팬·디지털키 OP 매핑`() {
        assertEquals("03 01 01", codec.encodeRequest(1, UserRequest.LightEnabled(true)).copyOfRange(4, 7).toHex())
        assertEquals("03 02 50", codec.encodeRequest(1, UserRequest.LightBrightness(80)).copyOfRange(4, 7).toHex())
        assertEquals("03 03 FF 80 00", codec.encodeRequest(1, UserRequest.LightColor(Rgb(255, 128, 0))).copyOfRange(4, 9).toHex())
        assertEquals("02 03 03", codec.encodeRequest(1, UserRequest.Fan(FanLevel.HIGH)).copyOfRange(4, 7).toHex())
        assertEquals("02 02 00", codec.encodeRequest(1, UserRequest.ClimateAuto(false)).copyOfRange(4, 7).toHex())
        assertEquals("04 01 01", codec.encodeRequest(1, UserRequest.ProximityUnlock(true)).copyOfRange(4, 7).toHex())
    }

    @Test(expected = IllegalArgumentException::class)
    fun `목표 온도가 i16 범위를 넘으면 거부 - -32768 은 무효값`() {
        codec.encodeRequest(1, UserRequest.TargetTemperature(-327.68))
    }

    @Test(expected = IllegalArgumentException::class)
    fun `REQUEST_ID 0 은 예약`() {
        codec.encodeRequest(0, UserRequest.Door(true))
    }

    @Test(expected = IllegalArgumentException::class)
    fun `팬 UNKNOWN 은 요청에 쓰지 않는다`() {
        codec.encodeRequest(1, UserRequest.Fan(FanLevel.UNKNOWN))
    }

    @Test fun `M_QUERY 20 B - 요청 결과 조회`() {
        val q = codec.encodeQuery(5, QueryScope.REQUEST_RESULT, requestSession = 0x0102030405060708, targetRequestId = 52)
        assertEquals("05 00 00 00 01 00 08 07 06 05 04 03 02 01 34 00 00 00 00 00", q.toHex())
    }

    @Test fun `M_WARNING_ACK 9 B - TYPE OCCURRENCE BOOT 순`() {
        val w = Warning(WarningType.DOOR_OPEN_AFTER_EXIT, true, Severity.CAUTION, Quality.OK, 0x10, 3, ReadState.UNREAD, 0, ageMs = 0)
        assertEquals("08 10 00 00 00 03 00 00 00", codec.encodeWarningAck(w).toHex())
    }

    @Test fun `M_RESULT 디코딩 - REQUEST_SESSION·ID·결과·사유`() {
        val p = downstream(MessageType.M_RESULT) {
            u64(20, 0x1122334455667788).u32(28, 52).u8(32, 2).u16(33, 9).u8(35, 1).u8(36, 1).u32(37, 77).u8(41, 1).u16(42, 120)
        }
        val m = codec.decode(MessageType.M_RESULT, p) as VehicleMessage.Result
        assertEquals(DownstreamContext(7, 0x1122334455667788, 3, 0), m.context)
        assertEquals(52L, m.requestId)
        assertEquals(2, m.result)
        assertEquals(9, m.reason)
        assertTrue(m.confirmed)
        assertEquals(120, m.ageMs)
    }

    @Test fun `M_BCM_DOOR_STATE 디코딩`() {
        val p = downstream(MessageType.M_BCM_DOOR_STATE) { u8(28, 1).u8(29, 0).u8(30, 0).u8(31, 0).u8(32, 1).u8(33, 0).u16(38, 40) }
        val m = codec.decode(MessageType.M_BCM_DOOR_STATE, p) as VehicleMessage.DoorStateMsg
        assertEquals(1, m.lock)
        assertEquals(0, m.open)
        assertEquals(1, m.openQuality)
        assertEquals(40, m.sourceAgeMs)
    }

    @Test fun `M_CIS_ENVIRONMENT - 무효 온도 -32768 보존`() {
        val p = downstream(MessageType.M_CIS_ENVIRONMENT) { i16(28, -32768).u8(36, 0).u8(37, 3).u16(38, 4550).u8(46, 1).u32(48, 0xFFFFFFFFL) }
        val m = codec.decode(MessageType.M_CIS_ENVIRONMENT, p) as VehicleMessage.CisEnvironment
        assertEquals(-32768L, m.temperature.raw)
        assertEquals(false, m.temperature.valid)
        assertEquals(4550L, m.humidity.raw)
        assertEquals(0xFFFFFFFFL, m.illuminance.raw)
    }

    @Test fun `M_WARNING 디코딩 - Type 8 도 버리지 않는다`() {
        val p = downstream(MessageType.M_WARNING) { u8(20, 8).u32(21, 99).u8(25, 1).u8(26, 1).u8(27, 0).u8(28, 0) }
        val m = codec.decode(MessageType.M_WARNING, p) as VehicleMessage.WarningMsg
        assertEquals(8, m.warningType)
        assertEquals(99L, m.occurrenceId)
        assertTrue(m.active)
    }

    @Test fun `M_QUERY_END 디코딩`() {
        val p = downstream(MessageType.M_QUERY_END, queryId = 5) { u8(20, 3).u8(21, 0).u16(22, 31) }
        val m = codec.decode(MessageType.M_QUERY_END, p) as VehicleMessage.QueryEnd
        assertEquals(5L, m.context.queryId)
        assertEquals(31, m.itemCount)
    }

    @Test fun `M_AVAILABILITY 9개 기능을 FUNCTION_ID 로 묶는다`() {
        val p = downstream(MessageType.M_AVAILABILITY) {
            for (i in 0 until 9) u8(20 + i * 4, i + 1).u8(21 + i * 4, if (i == 0) 2 else 0).u16(22 + i * 4, if (i == 0) 4 else 0)
        }
        val m = codec.decode(MessageType.M_AVAILABILITY, p) as VehicleMessage.Availability
        assertEquals(FunctionAvailability(2, 4), m.entries[1])
        assertEquals(9, m.entries.size)
    }

    @Test fun `길이가 다르면 폐기`() {
        assertNull(codec.decode(MessageType.M_RESULT, ByteArray(43)))
    }

    @Test fun `모르는 Type 은 Raw 로 보존 - WINDOW 는 이제 디코딩한다 (D15)`() {
        assertTrue(codec.decode(MessageType.M_WINDOW_STATE, ByteArray(42)) is VehicleMessage.WindowStateMsg)
        assertTrue(codec.decode(MessageType.M_WINDOW_FAULT, ByteArray(42)) is VehicleMessage.WindowFaultMsg)
        assertTrue(codec.decode(0x7E, ByteArray(3)) is VehicleMessage.Raw)
    }

    @Test fun `M_DIGITAL_STATUS 디코딩 - SETTING_STATE 는 원시값 그대로(0·1 밖도 보존)`() {
        val on = codec.decode(MessageType.M_DIGITAL_STATUS, downstream(MessageType.M_DIGITAL_STATUS) { u8(20, 1).u8(21, 0).u16(22, 0).u16(28, 15) })
            as VehicleMessage.DigitalStatus
        assertEquals(1, on.settingState)
        assertEquals(15, on.sourceAgeMs)
        val odd = codec.decode(MessageType.M_DIGITAL_STATUS, downstream(MessageType.M_DIGITAL_STATUS) { u8(20, 7) })
            as VehicleMessage.DigitalStatus
        assertEquals(7, odd.settingState)
    }
}
