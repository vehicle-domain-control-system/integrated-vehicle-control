package com.vdcs.mobile.ui

import com.vdcs.mobile.domain.model.ClimateState
import com.vdcs.mobile.domain.model.RearProximity
import com.vdcs.mobile.domain.model.RearState
import com.vdcs.mobile.domain.model.TempDirection
import com.vdcs.mobile.domain.model.VehicleSnapshot
import com.vdcs.mobile.domain.model.Warning
import com.vdcs.mobile.domain.model.WarningType
import com.vdcs.mobile.ui.rules.ChipTone
import com.vdcs.mobile.ui.rules.HomeRules
import com.vdcs.mobile.ui.rules.VehicleValueRules
import com.vdcs.mobile.ui.rules.WarningRules
import com.vdcs.mobile.ui.rules.displayableValue
import com.vdcs.mobile.ui.rules.isTrusted
import com.vdcs.mobile.ui.rules.label

data class CarVector(val x: Float, val y: Float, val z: Float)

enum class CarAnchor(val point: CarVector, val outward: CarVector?) {
    REAR(CarVector(0f, 0.75f, -2.38f), CarVector(0f, 0f, -1f)),

    CABIN(CarVector(0f, 1.5f, -0.45f), null),

    DRIVER_WINDOW(CarVector(0.8f, 1.12f, 0.45f), CarVector(1f, 0f, 0f)),

    DRIVER_DOOR(CarVector(0.92f, 0.7f, 0.4f), CarVector(1f, 0f, 0f)),
}

sealed interface CarPin {
    val label: String
    val tone: ChipTone
    val description: String
}

data class WarningPin(
    val type: WarningType,
    val anchor: CarAnchor,
    override val label: String,
    override val tone: ChipTone,
    override val description: String,
) : CarPin

data class DistancePin(override val label: String, override val tone: ChipTone, override val description: String) : CarPin

data class CabinGlow(val direction: TempDirection, val strength: Float, val stale: Boolean)

data class CarStatus(
    val markers: List<WarningPin>,
    val cabinGlow: CabinGlow?,
    val rear: DistancePin?,
) {
    val pinsByAnchor: Map<CarAnchor, List<CarPin>> = buildMap {
        markers.forEach { put(it.anchor, get(it.anchor).orEmpty() + it) }
        rear?.let { put(CarAnchor.REAR, get(CarAnchor.REAR).orEmpty() + it) }
    }

    companion object {
        val NONE = CarStatus(emptyList(), null, null)

        fun from(snapshot: VehicleSnapshot, origin: String?, nowMs: Long): CarStatus = CarStatus(
            markers = markers(snapshot.warnings, origin),
            cabinGlow = cabinGlow(snapshot.climate),
            rear = snapshot.rear?.takeUnless(::isQuietRear)?.let { distancePin(it, nowMs) },
        )

        fun anchorOf(type: WarningType): CarAnchor? = when (type) {
            WarningType.REAR -> CarAnchor.REAR
            WarningType.OCCUPANT_REMAINING -> CarAnchor.CABIN
            WarningType.PINCH, WarningType.WINDOW_FAULT -> CarAnchor.DRIVER_WINDOW
            WarningType.DOOR_OPEN_AFTER_EXIT -> CarAnchor.DRIVER_DOOR
            WarningType.BCM_FAULT, WarningType.CIS_FAULT, WarningType.VSS_FAULT -> null
        }

        fun markers(warnings: List<Warning>, origin: String?): List<WarningPin> =
            WarningRules.bannerWarnings(warnings).mapNotNull { w ->
                anchorOf(w.type)?.let {
                    WarningPin(w.type, it, w.type.label, WarningRules.activeTone(w), WarningRules.bannerText(w, origin))
                }
            }

        fun cabinGlow(climate: ClimateState?): CabinGlow? {
            climate ?: return null
            val direction = climate.tempDirection.displayableValue() ?: return null
            val output = climate.outputPercent.displayableValue() ?: return null
            if (direction == TempDirection.IDLE || output <= 0) return null
            val stale = !climate.tempDirection.quality.isTrusted || !climate.outputPercent.quality.isTrusted
            return CabinGlow(direction, output.coerceAtMost(PERCENT_MAX) / PERCENT_MAX.toFloat(), stale)
        }

        private fun isQuietRear(rear: RearState): Boolean =
            rear.status.quality.isTrusted &&
                (rear.status.value == RearProximity.NO_OBJECT || rear.status.value == RearProximity.INACTIVE)

        fun distancePin(rear: RearState, nowMs: Long): DistancePin {
            val value = VehicleValueRules.rearText(rear, nowMs)
            val description = listOfNotNull(VehicleValueRules.REAR_DISTANCE_NAME, value.text, value.note).joinToString(" · ")
            return DistancePin(value.text, HomeRules.tone(value), description)
        }

        private const val PERCENT_MAX = 100
    }
}
