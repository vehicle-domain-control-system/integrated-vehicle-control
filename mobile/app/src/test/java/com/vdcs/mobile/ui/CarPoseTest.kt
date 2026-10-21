package com.vdcs.mobile.ui

import com.vdcs.mobile.domain.model.DoorIntegrity
import com.vdcs.mobile.domain.model.DoorState
import com.vdcs.mobile.domain.model.FanLevel
import com.vdcs.mobile.domain.model.LightApplied
import com.vdcs.mobile.domain.model.LightState
import com.vdcs.mobile.domain.model.LightType
import com.vdcs.mobile.domain.model.LockState
import com.vdcs.mobile.domain.model.OpenState
import com.vdcs.mobile.domain.model.Qualified
import com.vdcs.mobile.domain.model.Quality
import com.vdcs.mobile.domain.model.Rgb
import com.vdcs.mobile.domain.model.UserSettingsState
import com.vdcs.mobile.domain.model.VehicleSnapshot
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class CarPoseTest {
    private val eps = 1e-6f

    private fun <T> q(v: T?, quality: Quality = Quality.OK) = Qualified(v, quality, 1_000L)

    private fun door(
        open: OpenState?,
        quality: Quality = Quality.OK,
        integrity: DoorIntegrity = DoorIntegrity.NORMAL,
        integrityQ: Quality = Quality.OK,
    ) = VehicleSnapshot(
        door = DoorState(
            lock = q(LockState.UNLOCKED),
            open = q(open, quality),
            integrity = q(integrity, integrityQ),
        ),
    )

    private fun light(
        type: LightType? = LightType.NORMAL,
        brightness: Int? = 80,
        rgb: Rgb? = Rgb(255, 128, 0),
        typeQ: Quality = Quality.OK,
        brightnessQ: Quality = Quality.OK,
        rgbQ: Quality = Quality.OK,
        feedback: Boolean? = false,
    ) = LightState(
        activeType = q(type, typeQ),
        applied = q(LightApplied.APPLIED),
        brightnessPercent = q(brightness, brightnessQ),
        rgb = q(rgb, rgbQ),
        physicalFeedbackSupported = q(feedback),
    )

    private fun settings(enabled: Boolean?, quality: Quality = Quality.OK) = UserSettingsState(
        targetTemperatureC = q(22.0),
        climateAuto = q(true),
        fanLevel = q(FanLevel.LOW),
        lightEnabled = q(enabled, quality),
        lightLevelPercent = q(80),
        lightRgb = q(Rgb(255, 255, 255)),
    )

    private fun assertAllClosed(p: DoorPose) =
        CarDoor.entries.forEach { assertEquals("${it.nodeName} 는 0°", 0f, p.angleOf(it), eps) }

    private fun assertNoOutput(l: LightPose) {
        assertNull(l.rgb)
        assertNull(l.brightnessPercent)
        assertEquals(0f, l.intensity, eps)
        assertFalse(l.on)
    }

    @Test fun `노드 이름은 glb 계약 그대로`() {
        assertEquals(listOf("Door_FL", "Door_FR", "Door_RL", "Door_RR"), CarDoor.entries.map { it.nodeName })
    }

    @Test fun `좌측 도어는 음수, 우측 도어는 양수가 열림`() {
        assertEquals(-60f, CarDoor.FL.fullOpenAngleDeg, eps)
        assertEquals(-60f, CarDoor.RL.fullOpenAngleDeg, eps)
        assertEquals(60f, CarDoor.FR.fullOpenAngleDeg, eps)
        assertEquals(60f, CarDoor.RR.fullOpenAngleDeg, eps)
    }

    @Test fun `OPEN OK 면 운전석 앞문만 음수 60도로 열린다`() {
        val p = CarPose.doorPose(door(OpenState.OPEN))
        assertEquals(-60f, p.angleOf(CarDoor.FL), eps)
        assertTrue(p.angleOf(CarDoor.FL) < 0f)
        assertEquals(0f, p.angleOf(CarDoor.FR), eps)
        assertEquals(0f, p.angleOf(CarDoor.RL), eps)
        assertEquals(0f, p.angleOf(CarDoor.RR), eps)
        assertEquals(DoorDisplay.OPEN, p.display)
        assertFalse(p.stale)
        assertEquals(4, p.anglesDeg.size)
    }

    @Test fun `OPEN 이어도 4짝을 다 열지 않는다`() {
        val p = CarPose.doorPose(door(OpenState.OPEN))
        assertEquals(1, p.anglesDeg.values.count { it != 0f })
    }

    @Test fun `OPEN STALE 은 값을 쓰고 stale 표시`() {
        val p = CarPose.doorPose(door(OpenState.OPEN, Quality.STALE))
        assertEquals(-60f, p.angleOf(CarDoor.FL), eps)
        assertTrue(p.stale)
        assertFalse(p.unknown)
    }

    @Test fun `CLOSED 면 모두 0도`() {
        val p = CarPose.doorPose(door(OpenState.CLOSED))
        assertAllClosed(p)
        assertEquals(DoorDisplay.CLOSED, p.display)
        assertFalse(p.stale)
    }

    @Test fun `CLOSED STALE 은 0도 + stale`() {
        val p = CarPose.doorPose(door(OpenState.CLOSED, Quality.STALE))
        assertAllClosed(p)
        assertTrue(p.stale)
    }

    @Test fun `UNKNOWN 값은 0도 + 모름`() {
        val p = CarPose.doorPose(door(OpenState.UNKNOWN))
        assertAllClosed(p)
        assertTrue(p.unknown)
    }

    @Test fun `INVALID 는 OPEN 값이어도 열지 않고 모름`() {
        val p = CarPose.doorPose(door(OpenState.OPEN, Quality.INVALID))
        assertAllClosed(p)
        assertTrue(p.unknown)
        assertFalse(p.stale)
    }

    @Test fun `NO_DATA 는 0도 + 모름`() {
        val p = CarPose.doorPose(door(OpenState.OPEN, Quality.NO_DATA))
        assertAllClosed(p)
        assertTrue(p.unknown)
    }

    @Test fun `값이 null 이면 모름`() {
        assertTrue(CarPose.doorPose(door(null)).unknown)
    }

    @Test fun `도어 미수신과 스냅샷 없음은 모름`() {
        assertTrue(CarPose.doorPose(VehicleSnapshot()).unknown)
        assertTrue(CarPose.doorPose(null).unknown)
        assertAllClosed(CarPose.doorPose(null))
    }

    @Test fun `INCONSISTENT 면 OPEN 이어도 열지 않고 모름`() {
        val p = CarPose.doorPose(door(OpenState.OPEN, integrity = DoorIntegrity.INCONSISTENT))
        assertAllClosed(p)
        assertTrue(p.unknown)
    }

    @Test fun `UNTRUSTED 면 CLOSED 여도 모름`() {
        val p = CarPose.doorPose(door(OpenState.CLOSED, integrity = DoorIntegrity.UNTRUSTED))
        assertAllClosed(p)
        assertTrue(p.unknown)
    }

    @Test fun `integrityQuality INVALID 나 NO_DATA 면 모름`() {
        listOf(Quality.INVALID, Quality.NO_DATA).forEach { iq ->
            val p = CarPose.doorPose(door(OpenState.OPEN, integrityQ = iq))
            assertAllClosed(p)
            assertTrue("$iq 는 모름", p.unknown)
        }
    }

    @Test fun `integrityQuality STALE 은 값을 쓰고 stale 에 합친다`() {
        val p = CarPose.doorPose(door(OpenState.OPEN, integrityQ = Quality.STALE))
        assertEquals(-60f, p.angleOf(CarDoor.FL), eps)
        assertFalse(p.unknown)
        assertTrue(p.stale)
    }

    @Test fun `OK 면 차량이 보고한 rgb 와 밝기를 그대로 쓴다`() {
        val l = CarPose.lightPose(VehicleSnapshot(light = light()))
        assertEquals(LightDisplay.ON, l.display)
        assertEquals(Rgb(255, 128, 0), l.rgb)
        assertEquals(80, l.brightnessPercent)
        assertEquals(0.8f, l.intensity, eps)
        assertFalse(l.unknown)
        assertFalse(l.stale)
        assertNull(l.alert)
    }

    @Test fun `실제 점등 확인은 차량이 지원한다고 보고한 경우만`() {
        assertTrue(CarPose.lightPose(VehicleSnapshot(light = light(feedback = true))).physicallyConfirmed)
        assertFalse(CarPose.lightPose(VehicleSnapshot(light = light(feedback = false))).physicallyConfirmed)
        assertFalse("모름은 확인 아님", CarPose.lightPose(VehicleSnapshot(light = light(feedback = null))).physicallyConfirmed)
    }

    @Test fun `사용자 설정이 false 여도 보고된 밝기가 0 보다 크면 켜짐`() {
        val l = CarPose.lightPose(VehicleSnapshot(light = light(), settings = settings(false)))
        assertEquals(LightDisplay.ON, l.display)
        assertEquals(80, l.brightnessPercent)
    }

    @Test fun `사용자 설정이 true 여도 보고된 밝기 0 이면 꺼짐`() {
        val l = CarPose.lightPose(VehicleSnapshot(light = light(brightness = 0), settings = settings(true)))
        assertEquals(LightDisplay.OFF, l.display)
        assertNoOutput(l)
        assertFalse(l.unknown)
    }

    @Test fun `사용자 설정의 품질은 판정과 stale 에 영향이 없다`() {
        listOf(Quality.STALE, Quality.INVALID, Quality.NO_DATA).forEach { sq ->
            val l = CarPose.lightPose(VehicleSnapshot(light = light(), settings = settings(true, sq)))
            assertEquals("$sq", LightDisplay.ON, l.display)
            assertFalse("$sq", l.stale)
        }
    }

    @Test fun `GOODBYE 출력도 보고된 대로 켜짐`() {
        val l = CarPose.lightPose(
            VehicleSnapshot(light = light(LightType.GOODBYE, rgb = Rgb(0, 0, 255)), settings = settings(false)),
        )
        assertEquals(LightDisplay.ON, l.display)
        assertEquals(Rgb(0, 0, 255), l.rgb)
        assertNull(l.alert)
    }

    @Test fun `LightState 의 STALE 은 값을 쓰고 stale 표시`() {
        listOf(
            light(typeQ = Quality.STALE),
            light(brightnessQ = Quality.STALE),
            light(rgbQ = Quality.STALE),
        ).forEach { ls ->
            val l = CarPose.lightPose(VehicleSnapshot(light = ls))
            assertTrue(l.on)
            assertEquals(Rgb(255, 128, 0), l.rgb)
            assertTrue(l.stale)
            assertFalse(l.unknown)
        }
    }

    @Test fun `꺼짐도 STALE 이면 stale`() {
        val l = CarPose.lightPose(VehicleSnapshot(light = light(brightness = 0, brightnessQ = Quality.STALE)))
        assertEquals(LightDisplay.OFF, l.display)
        assertTrue(l.stale)
    }

    @Test fun `rgb INVALID 는 모름 - 색을 내지 않는다`() {
        val l = CarPose.lightPose(VehicleSnapshot(light = light(rgbQ = Quality.INVALID)))
        assertEquals(LightDisplay.UNKNOWN, l.display)
        assertNoOutput(l)
    }

    @Test fun `밝기 NO_DATA 는 모름`() {
        val l = CarPose.lightPose(VehicleSnapshot(light = light(brightnessQ = Quality.NO_DATA)))
        assertTrue(l.unknown)
        assertNoOutput(l)
    }

    @Test fun `activeType INVALID 는 모름`() {
        assertTrue(CarPose.lightPose(VehicleSnapshot(light = light(typeQ = Quality.INVALID))).unknown)
    }

    @Test fun `실내등 값이 null 이면 모름`() {
        assertTrue(CarPose.lightPose(VehicleSnapshot(light = light(rgb = null))).unknown)
        assertTrue(CarPose.lightPose(VehicleSnapshot(light = light(brightness = null))).unknown)
        assertTrue(CarPose.lightPose(VehicleSnapshot(light = light(type = null))).unknown)
    }

    @Test fun `LightState 미수신과 스냅샷 없음은 모름 - 설정이 있어도`() {
        assertTrue(CarPose.lightPose(VehicleSnapshot(settings = settings(true))).unknown)
        assertTrue(CarPose.lightPose(null).unknown)
        assertNoOutput(CarPose.lightPose(null))
    }

    @Test fun `WARNING 은 차량이 보고한 rgb 그대로 + alert 표시`() {
        val l = CarPose.lightPose(VehicleSnapshot(light = light(LightType.WARNING, rgb = Rgb(10, 20, 30))))
        assertEquals(Rgb(10, 20, 30), l.rgb)
        assertEquals(LightType.WARNING, l.alert)
        assertTrue(l.on)
    }

    @Test fun `FAULT 는 보고된 출력대로 + alert`() {
        val l = CarPose.lightPose(VehicleSnapshot(light = light(LightType.FAULT)))
        assertTrue(l.on)
        assertEquals(Rgb(255, 128, 0), l.rgb)
        assertEquals(LightType.FAULT, l.alert)
    }

    @Test fun `WARNING 이라도 rgb 를 모르면 색을 지어내지 않는다`() {
        val l = CarPose.lightPose(VehicleSnapshot(light = light(LightType.WARNING, rgbQ = Quality.NO_DATA)))
        assertTrue(l.unknown)
        assertNoOutput(l)
    }

    @Test fun `범위 밖 밝기는 자른다 — 색 채널 자르기는 렌더러 몫`() {
        val l = CarPose.lightPose(VehicleSnapshot(light = light(brightness = 150, rgb = Rgb(300, -5, 255))))
        assertEquals(100, l.brightnessPercent)
        assertEquals(1f, l.intensity, eps)
    }

    @Test fun `from 은 도어와 실내등을 함께 낸다`() {
        val p = CarPose.from(door(OpenState.OPEN).copy(light = light()))
        assertEquals(DoorDisplay.OPEN, p.door.display)
        assertEquals(-60f, p.door.angleOf(CarDoor.FL), eps)
        assertTrue(p.light.on)
    }
}
