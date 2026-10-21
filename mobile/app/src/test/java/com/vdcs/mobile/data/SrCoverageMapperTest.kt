package com.vdcs.mobile.data

import com.vdcs.mobile.data.protocol.MessageCodec
import com.vdcs.mobile.data.protocol.MessageType
import com.vdcs.mobile.data.protocol.ByteWriter
import com.vdcs.mobile.data.protocol.downstream
import com.vdcs.mobile.domain.model.AntiPinchStatus
import com.vdcs.mobile.domain.model.FaultCategory
import com.vdcs.mobile.domain.model.HeatRemoval
import com.vdcs.mobile.domain.model.Quality
import com.vdcs.mobile.domain.model.VehicleFunction
import com.vdcs.mobile.domain.model.VehicleSnapshot
import com.vdcs.mobile.domain.model.WindowEcuState
import com.vdcs.mobile.domain.model.WindowFaultStatus
import com.vdcs.mobile.domain.model.WindowMotion
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class SrCoverageMapperTest {
    private val codec = MessageCodec()

    private fun map(type: Int, body: ByteWriter.() -> Unit): VehicleSnapshot =
        VehicleMessageMapper.apply(VehicleSnapshot(), codec.decode(type, downstream(type, body = body))!!, now = 100)

    @Test fun `HEAT_REMOVAL - 0 정상, 1 과열 차단, 255 측정 불가는 값 없음`() {
        fun heat(raw: Int, quality: Int = 0) = map(MessageType.M_BCM_CLIMATE_STATE) { u8(32, raw).u8(37, quality) }.climate!!.heatRemoval
        assertEquals(HeatRemoval.NORMAL, heat(0).value)
        assertEquals(HeatRemoval.OVERHEAT_CUTOFF, heat(1).value)
        assertNull(heat(255).value)
        assertEquals(Quality.NO_DATA, heat(255).quality)
        assertNull(heat(1, quality = Quality.INVALID.raw).value)
        assertEquals(Quality.INVALID, heat(1, quality = Quality.INVALID.raw).quality)
    }

    @Test fun `조명 물리 점등 확인 지원 여부 - 0 미지원, 1 지원, 그 밖은 미확인`() {
        fun cap(raw: Int) = map(MessageType.M_BCM_LIGHT_STATE) { u8(34, raw) }.light!!.physicalFeedbackSupported
        assertEquals(false, cap(0).value)
        assertEquals(true, cap(1).value)
        assertNull(cap(7).value)
        assertEquals(Quality.NO_DATA, cap(7).quality)
    }

    @Test fun `BCM 고장 분류·코드·영향 기능 - CIS 는 분류 미확인`() {
        val bcm = map(MessageType.M_BCM_STATUS) {
            u8(28, 2).u16(29, 0b100).u8(31, 1).u16(32, 0x1234).u8(34, 0b101).u16(40, 250)
        }.ecus.single()
        assertEquals(FaultCategory.SENSOR, bcm.faultCategory)
        assertEquals(0x1234, bcm.faultCode)
        assertEquals(setOf(VehicleFunction.DOOR, VehicleFunction.INTERIOR_LIGHT), bcm.affectedFunctions)
        assertEquals(250L, bcm.ageMs)
        assertEquals(FaultCategory.COMM, map(MessageType.M_BCM_STATUS) { u8(31, 3) }.ecus.single().faultCategory)
        assertNull(map(MessageType.M_BCM_STATUS) { u8(31, 9) }.ecus.single().faultCategory)
        val cis = map(MessageType.M_CIS_STATUS) {}.ecus.single()
        assertNull(cis.faultCategory)
        assertNull(cis.faultCode)
    }

    @Test fun `WINDOW_STATE 디코딩 - 255 는 값 없음, 품질 공유`() {
        val w = map(MessageType.M_WINDOW_STATE) {
            u8(28, 3).u8(29, 1).u8(30, 40).u8(31, 0).u8(32, 0).u8(33, 255).u8(34, 1).u8(35, 1).u16(40, 120)
        }.window!!
        assertEquals(WindowMotion.ANTIPINCH_REVERSING, w.motion.value)
        assertEquals(WindowEcuState.READY, w.ecuState.value)
        assertEquals(40, w.positionClosedPercent.value)
        assertEquals(false, w.fullyOpen.value)
        assertNull(w.fullyClosed.value)
        assertEquals(Quality.NO_DATA, w.fullyClosed.quality)
        assertEquals(AntiPinchStatus.ACTIVE, w.antiPinch.value)
        assertEquals(true, w.reversing.value)
        assertEquals(120L, w.motion.ageMs)

        val unknown = map(MessageType.M_WINDOW_STATE) { u8(28, 255).u8(30, 255).u8(31, Quality.NO_DATA.raw) }.window!!
        assertNull(unknown.motion.value)
        assertNull(unknown.positionClosedPercent.value)
        assertEquals(Quality.NO_DATA, unknown.motion.quality)
    }

    @Test fun `WINDOW_FAULT 디코딩 - 분류·코드·상태·발생`() {
        val f = map(MessageType.M_WINDOW_FAULT) { u8(28, 2).u16(29, 0x0A0B).u8(31, 2).u32(32, 77) }.windowFault!!
        assertEquals(FaultCategory.FUNCTION, f.category.value)
        assertEquals(0x0A0B, f.code.value)
        assertEquals(WindowFaultStatus.RECOVERING, f.status.value)
        assertEquals(77L, f.occurrenceId)
        val reserved = map(MessageType.M_WINDOW_FAULT) { u8(28, 8).u8(31, 9) }.windowFault!!
        assertNull(reserved.category.value)
        assertEquals(Quality.NO_DATA, reserved.status.quality)
    }
}
