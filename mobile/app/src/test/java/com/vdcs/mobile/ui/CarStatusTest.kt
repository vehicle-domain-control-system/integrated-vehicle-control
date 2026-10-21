package com.vdcs.mobile.ui

import com.vdcs.mobile.domain.model.ClimateState
import com.vdcs.mobile.domain.model.DoorIntegrity
import com.vdcs.mobile.domain.model.DoorState
import com.vdcs.mobile.domain.model.FanLevel
import com.vdcs.mobile.domain.model.HeatRemoval
import com.vdcs.mobile.domain.model.LockState
import com.vdcs.mobile.domain.model.OpenState
import com.vdcs.mobile.domain.model.Qualified
import com.vdcs.mobile.domain.model.Quality
import com.vdcs.mobile.domain.model.ReadState
import com.vdcs.mobile.domain.model.RearProximity
import com.vdcs.mobile.domain.model.RearState
import com.vdcs.mobile.domain.model.Severity
import com.vdcs.mobile.domain.model.TempDirection
import com.vdcs.mobile.domain.model.VehicleSnapshot
import com.vdcs.mobile.domain.model.Warning
import com.vdcs.mobile.domain.model.WarningType
import com.vdcs.mobile.ui.rules.ChipTone
import com.vdcs.mobile.ui.rules.HomeRules
import com.vdcs.mobile.ui.rules.ValueFormat
import com.vdcs.mobile.ui.rules.VehicleValueRules
import com.vdcs.mobile.ui.rules.WarningRules
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class CarStatusTest {
    private val eps = 1e-6f
    private val sim = WarningRules.SIMULATED_TEXT
    private val now = 6_000L

    private fun <T> q(v: T?, quality: Quality = Quality.OK) = Qualified(v, quality, 1_000L)

    private fun warning(
        type: WarningType,
        active: Boolean = true,
        severity: Severity = Severity.CAUTION,
        quality: Quality = Quality.OK,
    ) = Warning(type, active, severity, quality, occurrenceId = 1, domainBootId = 1, read = ReadState.UNREAD, receivedAtMs = 1_000L, ageMs = 0)

    private fun climate(direction: TempDirection?, output: Int?, dirQ: Quality = Quality.OK, outQ: Quality = Quality.OK) = ClimateState(
        fanCommanded = q(FanLevel.LOW),
        fanMeasured = q(FanLevel.LOW),
        tempDirection = q(direction, dirQ),
        outputPercent = q(output, outQ),
        heatRemoval = q(HeatRemoval.NORMAL),
    )

    private fun rear(proximity: RearProximity?, cm: Double? = 85.0, statusQ: Quality = Quality.OK, distQ: Quality = Quality.OK) =
        RearState(distanceCm = q(cm, distQ), status = q(proximity, statusQ))

    private fun markersOf(vararg w: Warning, origin: String? = null) = CarStatus.markers(w.toList(), origin)

    @Test fun `위치가 있는 경고는 정해진 앵커로`() {
        assertEquals(CarAnchor.REAR, CarStatus.anchorOf(WarningType.REAR))
        assertEquals(CarAnchor.CABIN, CarStatus.anchorOf(WarningType.OCCUPANT_REMAINING))
        assertEquals(CarAnchor.DRIVER_WINDOW, CarStatus.anchorOf(WarningType.PINCH))
        assertEquals(CarAnchor.DRIVER_WINDOW, CarStatus.anchorOf(WarningType.WINDOW_FAULT))
        assertEquals(CarAnchor.DRIVER_DOOR, CarStatus.anchorOf(WarningType.DOOR_OPEN_AFTER_EXIT))
    }

    @Test fun `위치 없는 ECU 고장은 마커 없음`() {
        listOf(WarningType.BCM_FAULT, WarningType.CIS_FAULT, WarningType.VSS_FAULT).forEach { assertNull(CarStatus.anchorOf(it)) }
        val m = markersOf(warning(WarningType.BCM_FAULT), warning(WarningType.CIS_FAULT), warning(WarningType.VSS_FAULT))
        assertTrue(m.isEmpty())
    }

    @Test fun `앵커 좌표는 glb 규약 - 후면은 뒤, 운전석 쪽은 +X`() {
        assertTrue(CarAnchor.REAR.point.z < 0f)
        assertEquals(-1f, CarAnchor.REAR.outward!!.z, eps)
        assertTrue(CarAnchor.DRIVER_DOOR.point.x > 0f)
        assertTrue(CarAnchor.DRIVER_WINDOW.point.y > CarAnchor.DRIVER_DOOR.point.y)
        assertNull(CarAnchor.CABIN.outward)
    }

    @Test fun `해제·확인 불가 CLEAR 경고는 마커를 만들지 않는다`() {
        val m = markersOf(warning(WarningType.REAR, active = false), warning(WarningType.PINCH, active = false, quality = Quality.NO_DATA))
        assertTrue(m.isEmpty())
    }

    @Test fun `핀 색 - 믿을 수 있으면 등급 색, STALE·INVALID·NO_DATA 는 확인 불가`() {
        val m = markersOf(
            warning(WarningType.REAR, severity = Severity.EMERGENCY),
            warning(WarningType.PINCH, quality = Quality.STALE),
            warning(WarningType.OCCUPANT_REMAINING, quality = Quality.INVALID),
            warning(WarningType.DOOR_OPEN_AFTER_EXIT, quality = Quality.NO_DATA),
        ).associateBy { it.type }
        assertEquals(ChipTone.EMERGENCY, m.getValue(WarningType.REAR).tone)
        assertEquals(ChipTone.UNKNOWN, m.getValue(WarningType.PINCH).tone)
        assertEquals(ChipTone.UNKNOWN, m.getValue(WarningType.OCCUPANT_REMAINING).tone)
        assertEquals(ChipTone.UNKNOWN, m.getValue(WarningType.DOOR_OPEN_AFTER_EXIT).tone)
        assertEquals(ChipTone.ATTENTION, markersOf(warning(WarningType.REAR)).single().tone)
    }

    @Test fun `핀 색은 경고 카드 상태 문구와 같은 판정 - 확인 불가 문구면 확인 불가 색`() {
        Quality.entries.forEach { quality ->
            val w = warning(WarningType.PINCH, quality = quality)
            val untrusted = WarningRules.statusText(w).contains(ValueFormat.UNTRUSTED_TEXT)
            assertEquals(quality.name, untrusted, markersOf(w).single().tone == ChipTone.UNKNOWN)
        }
    }

    @Test fun `핀 이름은 경고 종류만, 낭독 문구는 배너와 같은 글 - 최신 아님도 배너처럼 상태 확인 불가`() {
        val ok = markersOf(warning(WarningType.REAR, severity = Severity.EMERGENCY)).single()
        assertEquals("후방 물체 접근", ok.label)
        val staleWarning = warning(WarningType.PINCH, quality = Quality.STALE)
        val stale = markersOf(staleWarning, origin = sim).single()
        assertEquals("창문 끼임 감지", stale.label)
        assertEquals(WarningRules.bannerText(staleWarning, sim), stale.description)
        assertEquals("[주의] 창문 끼임 감지 · 발생 중 · 상태 확인 불가 · $sim", stale.description)
        val unknown = markersOf(warning(WarningType.OCCUPANT_REMAINING, quality = Quality.NO_DATA)).single()
        assertEquals("차 안에 탑승자 남음", unknown.label)
        assertEquals("[주의] 차 안에 탑승자 남음 · 발생 중 · 상태 확인 불가", unknown.description)
    }

    @Test fun `실차는 모의 표기 없음`() {
        assertFalse(markersOf(warning(WarningType.REAR)).single().description.contains(sim))
    }

    @Test fun `핀은 배너와 같은 경고만 - 위치 없는 것만 빠진다`() {
        val warnings = listOf(warning(WarningType.REAR), warning(WarningType.BCM_FAULT), warning(WarningType.PINCH, active = false))
        val bannered = WarningRules.bannerWarnings(warnings).map { it.type }.filter { CarStatus.anchorOf(it) != null }
        assertEquals(bannered, CarStatus.markers(warnings, null).map { it.type })
    }

    @Test fun `같은 위치 경고는 긴급 우선, 후방 거리는 경고 뒤`() {
        val status = CarStatus(
            markers = markersOf(warning(WarningType.WINDOW_FAULT), warning(WarningType.PINCH, severity = Severity.EMERGENCY)),
            cabinGlow = null,
            rear = null,
        )
        assertEquals(listOf(WarningType.PINCH, WarningType.WINDOW_FAULT), status.markers.map { it.type })
        val withRear = CarStatus.from(VehicleSnapshot(warnings = listOf(warning(WarningType.REAR)), rear = rear(RearProximity.VALID_DISTANCE)), null, now)
        val rearPins = withRear.pinsByAnchor.getValue(CarAnchor.REAR)
        assertEquals(listOf("후방 물체 접근", "85 cm"), rearPins.map { it.label })
        assertTrue(rearPins.last() is DistancePin)
    }

    @Test fun `냉방·난방은 보고 출력에 비례한 세기`() {
        val cool = CarStatus.cabinGlow(climate(TempDirection.COOL, 40))!!
        assertEquals(TempDirection.COOL, cool.direction)
        assertEquals(0.4f, cool.strength, eps)
        assertFalse(cool.stale)
        assertEquals(1f, CarStatus.cabinGlow(climate(TempDirection.HEAT, 100))!!.strength, eps)
    }

    @Test fun `대기·출력 0·미수신이면 빛 없음`() {
        assertNull(CarStatus.cabinGlow(climate(TempDirection.IDLE, 0)))
        assertNull(CarStatus.cabinGlow(climate(TempDirection.COOL, 0)))
        assertNull(CarStatus.cabinGlow(null))
    }

    @Test fun `방향·출력 중 하나라도 확인 불가면 빛 없음`() {
        assertNull(CarStatus.cabinGlow(climate(TempDirection.COOL, 50, dirQ = Quality.INVALID)))
        assertNull(CarStatus.cabinGlow(climate(TempDirection.HEAT, 50, outQ = Quality.NO_DATA)))
        assertNull(CarStatus.cabinGlow(climate(null, 50)))
    }

    @Test fun `STALE 이면 흐리게 표시할 빛`() {
        assertTrue(CarStatus.cabinGlow(climate(TempDirection.HEAT, 60, dirQ = Quality.STALE))!!.stale)
        assertTrue(CarStatus.cabinGlow(climate(TempDirection.HEAT, 60, outQ = Quality.STALE))!!.stale)
    }

    @Test fun `출력이 100 을 넘으면 1 로 묶는다`() {
        assertEquals(1f, CarStatus.cabinGlow(climate(TempDirection.COOL, 140))!!.strength, eps)
    }

    @Test fun `후방 감지 꺼짐(전원 꺼짐)이면 3D 핀을 띄우지 않는다`() {
        assertNull(CarStatus.from(VehicleSnapshot(rear = rear(RearProximity.INACTIVE, cm = null)), null, now).rear)
        assertNotNull(CarStatus.from(VehicleSnapshot(rear = rear(RearProximity.FAULT)), null, now).rear)
    }

    @Test fun `후방 미수신이면 표시 없음`() {
        assertNull(CarStatus.from(VehicleSnapshot(), null, now).rear)
    }

    private fun pin(r: RearState) = CarStatus.distancePin(r, now)

    @Test fun `후방 핀은 차량 값 화면 후방 거리 한 칸과 같은 글자`() {
        val cases = listOf(
            rear(RearProximity.VALID_DISTANCE),
            rear(RearProximity.VALID_DISTANCE, statusQ = Quality.STALE),
            rear(RearProximity.VALID_DISTANCE, distQ = Quality.INVALID),
            rear(RearProximity.VALID_DISTANCE, statusQ = Quality.NO_DATA),
            rear(RearProximity.NO_OBJECT),
            rear(RearProximity.FAULT),
            rear(RearProximity.UNAVAILABLE),
        )
        cases.forEach { r ->
            val value = VehicleValueRules.rearText(r, now)
            assertEquals(value.text, pin(r).label)
            assertEquals(HomeRules.tone(value), pin(r).tone)
        }
    }

    @Test fun `유효 거리 - 값만 짧게, 품질(거리·상태 중 나쁜 쪽)은 핀 색`() {
        assertEquals(DistancePin("85 cm", ChipTone.NORMAL, "후방 거리 · 85 cm"), pin(rear(RearProximity.VALID_DISTANCE)))
        val staleStatus = pin(rear(RearProximity.VALID_DISTANCE, statusQ = Quality.STALE))
        assertEquals("85 cm", staleStatus.label)
        assertEquals(ChipTone.STALE, staleStatus.tone)
        val staleDistance = pin(rear(RearProximity.VALID_DISTANCE, distQ = Quality.STALE))
        assertEquals(ChipTone.STALE, staleDistance.tone)
        assertEquals("후방 거리 · 85 cm · 5초 전 수신 · 최신 아님", staleDistance.description)
    }

    @Test fun `거리·상태를 보여 줄 수 없으면 수치 없이 값 화면과 같은 문구 - 무효는 확인 불가, 없음은 —`() {
        val invalidDistance = pin(rear(RearProximity.VALID_DISTANCE, distQ = Quality.INVALID))
        assertEquals(ValueFormat.UNTRUSTED_TEXT, invalidDistance.label)
        assertEquals(ChipTone.UNKNOWN, invalidDistance.tone)
        val noStatus = pin(rear(RearProximity.VALID_DISTANCE, statusQ = Quality.NO_DATA))
        assertEquals(ValueFormat.NO_DATA_TEXT, noStatus.label)
        assertEquals(ChipTone.UNKNOWN, noStatus.tone)
        assertFalse(noStatus.label.contains("85"))
    }

    @Test fun `물체 없음은 상태 품질, 고장·사용 불가는 값이 아니라 확인 불가 색`() {
        assertEquals(DistancePin("감지 물체 없음", ChipTone.NORMAL, "후방 거리 · 감지 물체 없음"), pin(rear(RearProximity.NO_OBJECT)))
        assertEquals(ChipTone.UNKNOWN, pin(rear(RearProximity.FAULT)).tone)
        assertEquals("고장", pin(rear(RearProximity.FAULT)).label)
        assertEquals(ChipTone.UNKNOWN, pin(rear(RearProximity.UNAVAILABLE)).tone)
        assertEquals("사용 불가", pin(rear(RearProximity.UNAVAILABLE)).label)
    }

    private fun doorPose(open: OpenState, quality: Quality = Quality.OK, integrity: DoorIntegrity = DoorIntegrity.NORMAL) =
        CarPose.doorPose(VehicleSnapshot(door = DoorState(q(LockState.UNLOCKED), q(open, quality), q(integrity))))

    @Test fun `열림이면 운전석 앞문만 강조 - 어느 도어인지 미보고`() {
        assertEquals(listOf(CarDoor.FL), doorPose(OpenState.OPEN).openDoors)
        assertTrue(doorPose(OpenState.OPEN, Quality.STALE).let { it.openDoors == listOf(CarDoor.FL) && it.stale })
    }

    @Test fun `닫힘·모름·불일치는 강조 없음`() {
        assertTrue(doorPose(OpenState.CLOSED).openDoors.isEmpty())
        assertTrue(doorPose(OpenState.OPEN, Quality.INVALID).openDoors.isEmpty())
        assertTrue(doorPose(OpenState.OPEN, integrity = DoorIntegrity.INCONSISTENT).openDoors.isEmpty())
    }

    @Test fun `빈 스냅샷은 아무것도 그리지 않는다`() {
        assertEquals(CarStatus.NONE, CarStatus.from(VehicleSnapshot(), null, now))
        assertTrue(CarStatus.NONE.pinsByAnchor.isEmpty())
    }

    @Test fun `믿을 수 있는 감지 물체 없음이면 후방 핀을 띄우지 않는다`() {
        assertNull(CarStatus.from(VehicleSnapshot(rear = rear(RearProximity.NO_OBJECT)), null, now).rear)
    }

    @Test fun `없음이 아닌 후방 상태는 핀으로 남는다 — 최신 아님·측정 불가·고장·거리`() {
        listOf(
            rear(RearProximity.NO_OBJECT, statusQ = Quality.STALE),
            rear(RearProximity.UNAVAILABLE),
            rear(RearProximity.FAULT),
            rear(RearProximity.VALID_DISTANCE),
        ).forEach { r -> assertNotNull("$r", CarStatus.from(VehicleSnapshot(rear = r), null, now).rear) }
    }
}
