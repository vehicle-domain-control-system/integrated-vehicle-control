package com.vdcs.mobile.ui

import com.vdcs.mobile.domain.model.DoorIntegrity
import com.vdcs.mobile.domain.model.LightState
import com.vdcs.mobile.domain.model.LightType
import com.vdcs.mobile.domain.model.Rgb
import com.vdcs.mobile.domain.model.OpenState
import com.vdcs.mobile.domain.model.VehicleSnapshot
import com.vdcs.mobile.ui.rules.displayableValue
import com.vdcs.mobile.ui.rules.isTrusted

enum class CarDoor(val nodeName: String, val fullOpenAngleDeg: Float) {
    FL("Door_FL", -CarPose.DOOR_OPEN_DEG),
    FR("Door_FR", +CarPose.DOOR_OPEN_DEG),
    RL("Door_RL", -CarPose.DOOR_OPEN_DEG),
    RR("Door_RR", +CarPose.DOOR_OPEN_DEG),
}

enum class DoorDisplay { OPEN, CLOSED, UNKNOWN }

data class DoorPose(
    val display: DoorDisplay,
    val anglesDeg: Map<CarDoor, Float>,
    val stale: Boolean,
) {
    val unknown: Boolean get() = display == DoorDisplay.UNKNOWN
    fun angleOf(door: CarDoor): Float = anglesDeg[door] ?: 0f

    val openDoors: List<CarDoor> get() = CarDoor.entries.filter { angleOf(it) != 0f }
}

enum class LightDisplay { ON, OFF, UNKNOWN }

data class LightPose(
    val display: LightDisplay,
    val rgb: Rgb?,
    val brightnessPercent: Int?,
    val stale: Boolean,
    val alert: LightType? = null,
    val physicallyConfirmed: Boolean = false,
) {
    val on: Boolean get() = display == LightDisplay.ON
    val unknown: Boolean get() = display == LightDisplay.UNKNOWN

    val intensity: Float get() = (brightnessPercent ?: 0) / 100f
}

data class CarPose(
    val door: DoorPose,
    val light: LightPose,
) {
    companion object {
        const val DOOR_OPEN_DEG = 60f

        val SYMBOLIC_OPEN_DOOR = CarDoor.FL

        private val CLOSED_ANGLES: Map<CarDoor, Float> = CarDoor.entries.associateWith { 0f }
        private val OPEN_ANGLES: Map<CarDoor, Float> =
            CLOSED_ANGLES + (SYMBOLIC_OPEN_DOOR to SYMBOLIC_OPEN_DOOR.fullOpenAngleDeg)
        private val UNKNOWN_DOOR = DoorPose(DoorDisplay.UNKNOWN, CLOSED_ANGLES, stale = false)
        private val UNKNOWN_LIGHT = LightPose(LightDisplay.UNKNOWN, rgb = null, brightnessPercent = null, stale = false)

        fun from(snapshot: VehicleSnapshot?): CarPose = CarPose(
            door = doorPose(snapshot),
            light = lightPose(snapshot),
        )

        fun doorPose(snapshot: VehicleSnapshot?): DoorPose {
            val door = snapshot?.door ?: return UNKNOWN_DOOR
            if (door.integrity.displayableValue() != DoorIntegrity.NORMAL) return UNKNOWN_DOOR
            val stale = !door.open.quality.isTrusted || !door.integrity.quality.isTrusted
            return when (door.open.displayableValue()) {
                OpenState.OPEN -> DoorPose(DoorDisplay.OPEN, OPEN_ANGLES, stale)
                OpenState.CLOSED -> DoorPose(DoorDisplay.CLOSED, CLOSED_ANGLES, stale)
                OpenState.UNKNOWN, null -> UNKNOWN_DOOR
            }
        }

        fun lightPose(snapshot: VehicleSnapshot?): LightPose {
            val light = snapshot?.light ?: return UNKNOWN_LIGHT
            val type = light.activeType.displayableValue() ?: return UNKNOWN_LIGHT
            val brightness = light.brightnessPercent.displayableValue() ?: return UNKNOWN_LIGHT
            val rgb = light.rgb.displayableValue() ?: return UNKNOWN_LIGHT

            val stale = listOf(light.activeType, light.brightnessPercent, light.rgb).any { !it.quality.isTrusted }
            val alert = type.takeIf { it == LightType.WARNING || it == LightType.FAULT }
            val confirmed = light.physicalFeedbackSupported.displayableValue() == true
            return if (brightness > 0) {
                LightPose(
                    display = LightDisplay.ON,
                    rgb = rgb,
                    brightnessPercent = brightness.coerceAtMost(100),
                    stale = stale,
                    alert = alert,
                    physicallyConfirmed = confirmed,
                )
            } else {
                LightPose(LightDisplay.OFF, rgb = null, brightnessPercent = null, stale = stale, alert = alert, physicallyConfirmed = confirmed)
            }
        }

    }
}
