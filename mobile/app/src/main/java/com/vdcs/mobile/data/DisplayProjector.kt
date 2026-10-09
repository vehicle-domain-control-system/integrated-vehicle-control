package com.vdcs.mobile.data

import com.vdcs.mobile.domain.logic.FreshnessEvaluator
import com.vdcs.mobile.domain.model.Qualified
import com.vdcs.mobile.domain.model.Quality
import com.vdcs.mobile.domain.model.VehicleSnapshot

internal class DisplayProjector(
    private val freshness: FreshnessEvaluator,
    private val fastStaleMs: Long,
    private val slowStaleMs: Long,
) {
    private var staleBefore = 0L

    fun markPreviousContext(now: Long) {
        staleBefore = now
    }

    fun project(s: VehicleSnapshot, now: Long): VehicleSnapshot {
        fun <T> Qualified<T>.fast() = degrade(this, fastStaleMs, now)
        fun <T> Qualified<T>.slow() = degrade(this, slowStaleMs, now)
        return s.copy(
            door = s.door?.let { it.copy(lock = it.lock.fast(), open = it.open.fast(), integrity = it.integrity.fast()) },
            climate = s.climate?.let {
                it.copy(
                    fanCommanded = it.fanCommanded.fast(), fanMeasured = it.fanMeasured.fast(),
                    tempDirection = it.tempDirection.fast(), outputPercent = it.outputPercent.fast(),
                    heatRemoval = it.heatRemoval.fast(),
                )
            },
            light = s.light?.let {
                it.copy(
                    activeType = it.activeType.fast(), applied = it.applied.fast(),
                    brightnessPercent = it.brightnessPercent.fast(), rgb = it.rgb.fast(),
                    physicalFeedbackSupported = it.physicalFeedbackSupported.fast(),
                )
            },
            digitalKey = s.digitalKey?.let { it.copy(proximityUnlockEnabled = it.proximityUnlockEnabled.slow()) },
            settings = s.settings?.let {
                it.copy(
                    targetTemperatureC = it.targetTemperatureC.slow(), climateAuto = it.climateAuto.slow(),
                    fanLevel = it.fanLevel.slow(), lightEnabled = it.lightEnabled.slow(),
                    lightLevelPercent = it.lightLevelPercent.slow(), lightRgb = it.lightRgb.slow(),
                )
            },
            environment = s.environment?.let {
                it.copy(
                    temperatureC = it.temperatureC.fast(), humidityPercent = it.humidityPercent.fast(),
                    illuminanceLux = it.illuminanceLux.fast(),
                )
            },
            occupant = s.occupant?.let { it.copy(present = it.present.fast(), count = it.count.fast()) },
            rear = s.rear?.let { it.copy(distanceCm = it.distanceCm.fast(), status = it.status.fast()) },
            window = s.window?.let {
                it.copy(
                    motion = it.motion.fast(), ecuState = it.ecuState.fast(),
                    positionClosedPercent = it.positionClosedPercent.fast(), fullyOpen = it.fullyOpen.fast(),
                    fullyClosed = it.fullyClosed.fast(), antiPinch = it.antiPinch.fast(), reversing = it.reversing.fast(),
                )
            },
            windowFault = s.windowFault?.let { it.copy(category = it.category.slow(), code = it.code.slow(), status = it.status.slow()) },
            warnings = s.warnings.map { it.copy(quality = slowQuality(it.quality, it.receivedAtMs, it.ageMs, now)) },
            availability = s.availability.mapValues { (_, f) -> f.copy(quality = slowQuality(f.quality, f.receivedAtMs, f.ageMs, now)) },
            ecus = s.ecus.map { it.copy(quality = slowQuality(it.quality, it.receivedAtMs, it.ageMs, now)) },
        )
    }

    private fun <T> degrade(q: Qualified<T>, limitMs: Long, now: Long): Qualified<T> {
        val quality = if (q.quality == Quality.OK && q.receivedAtMs < staleBefore) Quality.STALE
        else freshness.evaluate(q.quality, q.receivedAtMs, q.ageMs, limitMs, now)
        return if (quality == q.quality) q else q.copy(quality = quality)
    }

    private fun slowQuality(q: Quality, receivedAtMs: Long, ageMs: Long, now: Long): Quality =
        if (q == Quality.OK &&
            (receivedAtMs < staleBefore || freshness.isReceptionLost(receivedAtMs, ageMs, slowStaleMs, now))
        ) Quality.STALE else q
}
