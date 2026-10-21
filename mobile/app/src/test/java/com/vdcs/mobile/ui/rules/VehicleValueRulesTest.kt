package com.vdcs.mobile.ui.rules

import com.vdcs.mobile.domain.model.AutoUnlockResult
import com.vdcs.mobile.domain.model.DigitalKeyState
import com.vdcs.mobile.domain.model.ClimateState
import com.vdcs.mobile.domain.model.EcuHealth
import com.vdcs.mobile.domain.model.FanLevel
import com.vdcs.mobile.domain.model.FaultCategory
import com.vdcs.mobile.domain.model.HeatRemoval
import com.vdcs.mobile.domain.model.LightApplied
import com.vdcs.mobile.domain.model.LightState
import com.vdcs.mobile.domain.model.LightType
import com.vdcs.mobile.domain.model.Qualified
import com.vdcs.mobile.domain.model.Quality
import com.vdcs.mobile.domain.model.RearProximity
import com.vdcs.mobile.domain.model.RearState
import com.vdcs.mobile.domain.model.Rgb
import com.vdcs.mobile.domain.model.TempDirection
import com.vdcs.mobile.domain.model.VehicleFunction
import com.vdcs.mobile.domain.model.WindowFault
import com.vdcs.mobile.domain.model.WindowFaultStatus
import com.vdcs.mobile.domain.model.WindowState
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class VehicleValueRulesTest {
    private val now = 100_000L
    private fun ecu(q: Quality, faults: List<String> = listOf("SENSOR")) =
        EcuHealth("BCM", "ACTIVE", faults, recovering = false, receivedAtMs = now - 2_000, quality = q,
            faultCategory = null, faultCode = null, affectedFunctions = emptySet(), ageMs = 0)

    @Test fun `고장 강조색은 보여 줄 수 있는 고장에만 - INVALID 는 흐리게`() {
        val ok = VehicleValueRules.ecuFaultsText(ecu(Quality.OK))
        assertTrue(ok.shown && ok.alert)
        val stale = VehicleValueRules.ecuFaultsText(ecu(Quality.STALE))
        assertTrue(stale.shown && stale.alert)
        for (q in listOf(Quality.INVALID, Quality.NO_DATA)) {
            val v = VehicleValueRules.ecuFaultsText(ecu(q))
            assertFalse(q.name, v.shown)
            assertEquals("고장 여부 ${ValueFormat.UNTRUSTED_TEXT}", v.text)
        }
        val none = VehicleValueRules.ecuFaultsText(ecu(Quality.OK, faults = emptyList()))
        assertEquals("고장 없음", none.text)
        assertFalse(none.alert)
    }

    @Test fun `ECU 수신 시각은 한 번만 - STALE 은 상태 note 가 싣는다`() {
        val stale = ecu(Quality.STALE)
        assertTrue(VehicleValueRules.ecuStatusText(stale, now).note!!.contains("2초 전 수신"))
        assertNull(VehicleValueRules.ecuReceivedText(stale, now))
        for (q in listOf(Quality.OK, Quality.INVALID, Quality.NO_DATA)) {
            assertEquals(q.name, "2초 전 수신", VehicleValueRules.ecuReceivedText(ecu(q), now))
            assertFalse(q.name, VehicleValueRules.ecuStatusText(ecu(q), now).note.orEmpty().contains("수신 ·"))
        }
    }

    @Test fun `자동 해제 기록이 없으면 기록 없음, 있으면 값 규칙`() {
        assertEquals("기록 없음", VehicleValueRules.lastAutoUnlockText(null, now).text)
        val noRecord = DigitalKeyState(Qualified(true, Quality.OK, now), lastAutoUnlock = null)
        assertEquals("기록 없음", VehicleValueRules.lastAutoUnlockText(noRecord, now).text)
        val invalid = noRecord.copy(lastAutoUnlock = Qualified(AutoUnlockResult.DONE, Quality.INVALID, now))
        assertEquals(ValueFormat.UNTRUSTED_TEXT, VehicleValueRules.lastAutoUnlockText(invalid, now).text)
        val done = noRecord.copy(lastAutoUnlock = Qualified(AutoUnlockResult.DONE, Quality.OK, now))
        assertEquals("자동 해제됨", VehicleValueRules.lastAutoUnlockText(done, now).text)
    }

    private fun rear(distanceQ: Quality, status: RearProximity, statusQ: Quality) =
        RearState(Qualified(120.0, distanceQ, now - 3_000), Qualified(status, statusQ, now - 3_000))

    @Test fun `거리 수치 품질은 거리·상태 중 나쁜 쪽`() {
        for (dq in Quality.entries) {
            for (sq in listOf(Quality.OK, Quality.STALE)) {
                val v = VehicleValueRules.rearText(rear(dq, RearProximity.VALID_DISTANCE, sq), now)
                val expected = ValueFormat.value(Qualified(120.0, worst(dq, sq), now - 3_000), now, ValueFormat::centimeters)
                assertEquals("$dq·$sq", expected, v)
            }
        }
    }

    @Test fun `상태를 보여 줄 수 없으면 거리도 물체 없음도 단정 안 함`() {
        for (sq in listOf(Quality.INVALID, Quality.NO_DATA)) {
            for (p in RearProximity.entries) {
                assertFalse("$sq·$p", VehicleValueRules.rearText(rear(Quality.OK, p, sq), now).shown)
            }
        }
    }

    @Test fun `사용 불가·고장·복구 중·감지 꺼짐·모름은 상태 이름만, STALE 이면 수신 시각`() {
        for (p in listOf(RearProximity.UNAVAILABLE, RearProximity.FAULT, RearProximity.RECOVERING, RearProximity.INACTIVE, RearProximity.UNKNOWN)) {
            val ok = VehicleValueRules.rearText(rear(Quality.OK, p, Quality.OK), now)
            assertEquals(p.label, ok.text)
            assertFalse(ok.shown)
            assertNull(ok.note)
            assertNotNull(VehicleValueRules.rearText(rear(Quality.OK, p, Quality.STALE), now).note)
        }
        val noObjectStale = VehicleValueRules.rearText(rear(Quality.OK, RearProximity.NO_OBJECT, Quality.STALE), now)
        assertEquals("감지 물체 없음", noObjectStale.text)
        assertTrue(noObjectStale.note!!.contains("3초 전 수신"))
        assertEquals(ValueFormat.NO_DATA_VALUE, VehicleValueRules.rearText(null, now))
    }

    private fun <T> ok(v: T?, q: Quality = Quality.OK) = Qualified(v, q, now - 2_000)

    private fun climate(heat: Qualified<HeatRemoval>) = ClimateState(
        ok(FanLevel.LOW), ok(FanLevel.LOW), ok(TempDirection.COOL), ok(40), heatRemoval = heat,
    )

    private fun light(feedback: Qualified<Boolean>) = LightState(
        ok(LightType.NORMAL), ok(LightApplied.APPLIED), ok(80), ok(Rgb(255, 255, 255)), physicalFeedbackSupported = feedback,
    )

    @Test fun `과열 차단은 안전 정책 미적용 사유로 강조 - 확인 불가면 정상이라 하지 않는다`() {
        val cut = VehicleValueRules.heatRemovalText(climate(ok(HeatRemoval.OVERHEAT_CUTOFF)), now)
        assertTrue(cut.shown && cut.alert)
        assertTrue(cut.text.contains("안전 정책"))
        val normal = VehicleValueRules.heatRemovalText(climate(ok(HeatRemoval.NORMAL)), now)
        assertFalse(normal.alert)
        val unknown = VehicleValueRules.heatRemovalText(climate(ok(null, Quality.NO_DATA)), now)
        assertFalse(unknown.shown || unknown.alert)
        assertFalse(VehicleValueRules.heatRemovalText(null, now).shown)
    }

    @Test fun `조명 실제 점등 확인 세 상태 - 지원·미지원은 보고대로, 모르면 미지원이라 단정 안 함`() {
        val unsupported = VehicleValueRules.physicalFeedbackText(light(ok(false)), now)
        assertEquals(VehicleValueRules.PHYSICAL_FEEDBACK_UNSUPPORTED, unsupported.text)
        assertTrue(unsupported.shown)
        val supported = VehicleValueRules.physicalFeedbackText(light(ok(true)), now)
        assertEquals(VehicleValueRules.PHYSICAL_FEEDBACK_SUPPORTED, supported.text)
        assertTrue(supported.shown)
        val unknown = VehicleValueRules.physicalFeedbackText(light(ok(null, Quality.NO_DATA)), now)
        assertFalse(unknown.shown)
        assertNotEquals(VehicleValueRules.PHYSICAL_FEEDBACK_UNSUPPORTED, unknown.text)
        val unreceived = VehicleValueRules.physicalFeedbackText(null, now)
        assertFalse(unreceived.shown)
        assertNotEquals(VehicleValueRules.PHYSICAL_FEEDBACK_UNSUPPORTED, unreceived.text)
    }

    private fun bcm(category: FaultCategory?, q: Quality = Quality.OK, code: Int? = 12, affected: Set<VehicleFunction> = emptySet()) =
        ecu(q).copy(faultCategory = category, faultCode = code, affectedFunctions = affected)

    @Test fun `ECU 오류 분류 - 보고 안 하는 ECU 는 행 없음, 품질 낮으면 단정 안 함`() {
        assertNull(VehicleValueRules.ecuCategoryText(bcm(null)))
        val sensor = VehicleValueRules.ecuCategoryText(bcm(FaultCategory.SENSOR))!!
        assertEquals("센서 오류", sensor.text)
        assertTrue(sensor.alert)
        assertFalse(VehicleValueRules.ecuCategoryText(bcm(FaultCategory.NONE))!!.alert)
        assertEquals("통신 오류 (최신 아님)", VehicleValueRules.ecuCategoryText(bcm(FaultCategory.COMM, Quality.STALE))!!.text)
        val invalid = VehicleValueRules.ecuCategoryText(bcm(FaultCategory.FUNCTION, Quality.INVALID))!!
        assertFalse(invalid.shown)
    }

    @Test fun `ECU 고장 세부·대응 행동은 분류가 오류를 말할 때만`() {
        val detail = VehicleValueRules.ecuFaultDetailText(
            bcm(FaultCategory.FUNCTION, affected = setOf(VehicleFunction.INTERIOR_LIGHT, VehicleFunction.DOOR)),
        )
        assertEquals("코드 12 · 영향: 도어, 실내 조명", detail!!.text)
        assertNull(VehicleValueRules.ecuFaultDetailText(bcm(FaultCategory.NONE)))
        assertNull(VehicleValueRules.ecuFaultDetailText(bcm(FaultCategory.SENSOR, Quality.NO_DATA)))
        assertNull(VehicleValueRules.ecuFaultDetailText(bcm(FaultCategory.SENSOR, code = null)))

        for (c in listOf(FaultCategory.SENSOR, FaultCategory.FUNCTION, FaultCategory.COMM)) {
            assertNotNull(c.name, VehicleValueRules.ecuActionText(bcm(c)))
        }
        assertEquals(3, listOf(FaultCategory.SENSOR, FaultCategory.FUNCTION, FaultCategory.COMM).map { VehicleValueRules.faultActionText(it) }.toSet().size)
        assertNull(VehicleValueRules.ecuActionText(bcm(FaultCategory.NONE)))
        assertNull(VehicleValueRules.ecuActionText(bcm(null)))
        assertNull(VehicleValueRules.ecuActionText(bcm(FaultCategory.SENSOR, Quality.INVALID)))
    }

    @Test fun `차량 전체 마지막 수신 - 받은 적 없으면 수신 없음`() {
        val none = VehicleValueRules.lastReceivedText(null, now)
        assertEquals(VehicleValueRules.NOT_RECEIVED_TEXT, none.text)
        assertFalse(none.shown)
        assertEquals("3초 전 수신", VehicleValueRules.lastReceivedText(now - 3_000, now).text)
    }

    @Test fun `ECU 이름은 사용자 말 + 약어, 모르는 이름은 그대로`() {
        assertEquals("차체 제어 장치 (BCM)", VehicleValueRules.ecuName("BCM"))
        assertEquals("경고음 장치 (VSS)", VehicleValueRules.ecuName("VSS"))
        assertEquals("XYZ", VehicleValueRules.ecuName("XYZ"))
    }

    @Test fun `PROXIMITY_STATUS 원시값 - 5 는 감지 꺼짐, 표에 없는 값은 모름`() {
        assertEquals(RearProximity.INACTIVE, RearProximity.fromRaw(5))
        assertEquals("감지 꺼짐", RearProximity.INACTIVE.label)
        for ((raw, p) in listOf(0 to RearProximity.VALID_DISTANCE, 1 to RearProximity.NO_OBJECT, 4 to RearProximity.RECOVERING)) {
            assertEquals(p, RearProximity.fromRaw(raw))
        }
        for (raw in listOf(-1, 6, 255)) assertEquals(RearProximity.UNKNOWN, RearProximity.fromRaw(raw))
    }
}
