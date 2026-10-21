package com.vdcs.mobile.ui.screen.carview

import com.vdcs.mobile.domain.model.LightType
import com.vdcs.mobile.domain.model.Rgb
import com.vdcs.mobile.ui.CarDoor
import com.vdcs.mobile.ui.CarPose
import com.vdcs.mobile.ui.DoorDisplay
import com.vdcs.mobile.ui.DoorPose
import com.vdcs.mobile.ui.LightDisplay
import com.vdcs.mobile.ui.LightPose
import com.vdcs.mobile.ui.rules.label
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class CarPoseTextTest {
    private fun on(confirmed: Boolean) =
        LightPose(LightDisplay.ON, Rgb(255, 0, 0), brightnessPercent = 60, stale = false, physicallyConfirmed = confirmed)

    @Test fun `켜진 실내등은 색을 이름으로 말한다`() {
        assertEquals("켜짐 · 밝기 60% · 빨강", CarPoseText.light(on(confirmed = true)))
    }

    @Test fun `실제 점등 확인이 없으면 지시 반영임을 덧붙인다`() {
        assertTrue(CarPoseText.light(on(confirmed = false)).endsWith(CarPoseText.LIGHT_UNCONFIRMED))
    }

    @Test fun `꺼짐에는 점등 미확인을 붙이지 않는다`() {
        val off = LightPose(LightDisplay.OFF, rgb = null, brightnessPercent = null, stale = false)
        assertFalse(CarPoseText.light(off).contains(CarPoseText.LIGHT_UNCONFIRMED))
    }

    private fun door(display: DoorDisplay, stale: Boolean = false) =
        DoorPose(display, CarDoor.entries.associateWith { 0f }, stale)
    private val lightOff = LightPose(LightDisplay.OFF, rgb = null, brightnessPercent = null, stale = false)
    private val unknownLight = LightPose(LightDisplay.UNKNOWN, rgb = null, brightnessPercent = null, stale = false)

    @Test fun `배지 - 같은 상태는 대상을 묶어 한 번만 말한다`() {
        assertEquals(
            "${CarPoseText.DOOR}·${CarPoseText.LIGHT} ${CarPoseText.UNKNOWN}",
            CarPoseText.badgeText(CarPose(door(DoorDisplay.UNKNOWN), unknownLight)),
        )
        assertEquals(
            "${CarPoseText.DOOR}·${CarPoseText.LIGHT} ${CarPoseText.STALE}",
            CarPoseText.badgeText(CarPose(door(DoorDisplay.CLOSED, stale = true), lightOff.copy(stale = true))),
        )
    }

    @Test fun `배지 - 상태가 다르면 묶지 않아 품질 구분이 남는다`() {
        val text = CarPoseText.badgeText(CarPose(door(DoorDisplay.UNKNOWN), on(confirmed = false).copy(stale = true)))
        assertEquals(
            "${CarPoseText.DOOR} ${CarPoseText.UNKNOWN} · ${CarPoseText.LIGHT} ${CarPoseText.LIGHT_UNCONFIRMED} · ${CarPoseText.LIGHT} ${CarPoseText.STALE}",
            text,
        )
        val alert = CarPoseText.badgeText(CarPose(door(DoorDisplay.OPEN), lightOff.copy(alert = LightType.WARNING)))
        assertEquals("${CarPoseText.DOOR} ${CarPoseText.DOOR_OPEN} · ${CarPoseText.LIGHT} ${LightType.WARNING.label}", alert)
    }

    @Test fun `배지 - 그림이 다 보여 주면 말하지 않는다`() {
        assertEquals(null, CarPoseText.badgeText(CarPose(door(DoorDisplay.CLOSED), lightOff)))
        assertEquals(null, CarPoseText.badgeText(CarPose(door(DoorDisplay.CLOSED), on(confirmed = true))))
    }
}
