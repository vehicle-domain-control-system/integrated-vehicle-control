package com.vdcs.mobile.data

import com.vdcs.mobile.data.protocol.VehicleMessage
import com.vdcs.mobile.domain.model.Availability
import com.vdcs.mobile.domain.model.AutoUnlockResult
import com.vdcs.mobile.domain.model.CabinEnvironment
import com.vdcs.mobile.domain.model.ClimateState
import com.vdcs.mobile.domain.model.DigitalKeyState
import com.vdcs.mobile.domain.model.DoorIntegrity
import com.vdcs.mobile.domain.model.DoorState
import com.vdcs.mobile.domain.model.EcuHealth
import com.vdcs.mobile.domain.model.FanLevel
import com.vdcs.mobile.domain.model.FaultCategory
import com.vdcs.mobile.domain.model.FunctionStatus
import com.vdcs.mobile.domain.model.HeatRemoval
import com.vdcs.mobile.domain.model.LightApplied
import com.vdcs.mobile.domain.model.LightState
import com.vdcs.mobile.domain.model.LightType
import com.vdcs.mobile.domain.model.LockState
import com.vdcs.mobile.domain.model.OccupantState
import com.vdcs.mobile.domain.model.OpenState
import com.vdcs.mobile.domain.model.Qualified
import com.vdcs.mobile.domain.model.Quality
import com.vdcs.mobile.domain.model.ReadState
import com.vdcs.mobile.domain.model.RearProximity
import com.vdcs.mobile.domain.model.RearState
import com.vdcs.mobile.domain.model.RequestState
import com.vdcs.mobile.domain.model.ResultReason
import com.vdcs.mobile.domain.model.Rgb
import com.vdcs.mobile.domain.model.Severity
import com.vdcs.mobile.domain.model.TempDirection
import com.vdcs.mobile.domain.model.UserSettingsState
import com.vdcs.mobile.domain.model.VehicleFunction
import com.vdcs.mobile.domain.model.VehicleSnapshot
import com.vdcs.mobile.domain.model.Warning
import com.vdcs.mobile.domain.model.WarningType

internal object VehicleMessageMapper {
    fun apply(s: VehicleSnapshot, m: VehicleMessage, now: Long): VehicleSnapshot = when (m) {
        is VehicleMessage.DoorStateMsg -> s.copy(door = door(m, now))
        is VehicleMessage.ClimateStateMsg -> s.copy(climate = climate(m, now))
        is VehicleMessage.LightStateMsg -> s.copy(light = light(m, now))
        is VehicleMessage.UserSettings -> s.copy(settings = settings(m, now))
        is VehicleMessage.CisEnvironment -> s.copy(environment = environment(m, now))
        is VehicleMessage.CisOccupant -> s.copy(occupant = occupant(m, now))
        is VehicleMessage.CisRear -> s.copy(rear = rear(m, now))
        is VehicleMessage.Availability -> s.copy(
            availability = m.entries.mapNotNull { (id, a) ->
                VehicleFunction.fromRaw(id)?.let {
                    it to FunctionStatus(availability(a.availability), ResultReason.fromRaw(a.reason), now, ageMs = m.sourceAgeMs.toLong())
                }
            }.toMap(),
        )
        is VehicleMessage.DigitalStatus -> s.copy(
            digitalKey = DigitalKeyState(
                proximityUnlockEnabled = qualified(
                    when (m.settingState) { 0 -> false; 1 -> true; else -> null }, Quality.OK, now, m.sourceAgeMs,
                ),
                lastAutoUnlock = s.digitalKey?.lastAutoUnlock,
            ),
            availability = s.availability + (
                VehicleFunction.DIGITAL_KEY to
                    FunctionStatus(availability(m.availability), ResultReason.fromRaw(m.reason), now, ageMs = m.sourceAgeMs.toLong())
                ),
        )
        is VehicleMessage.DigitalResult -> s.copy(
            digitalKey = DigitalKeyState(
                proximityUnlockEnabled = s.digitalKey?.proximityUnlockEnabled ?: Qualified(null, Quality.NO_DATA, now),
                lastAutoUnlock = Qualified(autoUnlock(m.result), if (m.result == NO_RESULT) Quality.NO_DATA else Quality.OK, now, m.ageMs.toLong()),
            ),
        )
        is VehicleMessage.WarningMsg -> {
            val w = warning(m, now)
            if (w == null) s else s.copy(warnings = (s.warnings.filter { it.type != w.type } + w).sortedBy { it.type.raw })
        }
        is VehicleMessage.BcmStatus -> s.withEcu(
            EcuHealth(
                "BCM", ecuState(m.ecuState), bits(m.faultMask, BCM_FAULTS), m.recovering, now,
                faultCategory = faultCategory(m.faultCategory), faultCode = m.faultCode,
                affectedFunctions = BCM_AFFECTED.filterIndexed { i, _ -> m.affectedFunction and (1 shl i) != 0 }.toSet(),
                ageMs = m.sourceAgeMs.toLong(),
            ),
        )
        is VehicleMessage.CisStatus -> s.withEcu(
            EcuHealth(
                "CIS", listOf("STARTUP", "READY", "ACTIVE", "FAULT").getOrElse(m.cisState) { "UNKNOWN" },
                bits(m.faultMask, CIS_FAULTS), m.recovering, now,
                faultCategory = null, faultCode = null, affectedFunctions = emptySet(), ageMs = m.sourceAgeMs.toLong(),
            ),
        )
        is VehicleMessage.VssStatus -> s.withEcu(
            EcuHealth(
                "VSS", listOf("STARTUP", "READY", "PLAYING", "FAULT").getOrElse(m.vssState) { "UNKNOWN" },
                bits(m.faultMask, VSS_FAULTS), false, now,
                faultCategory = null, faultCode = null, affectedFunctions = emptySet(), ageMs = m.sourceAgeMs.toLong(),
            ),
        )
        is VehicleMessage.WindowStateMsg -> s.copy(window = WindowMapper.state(m, now))
        is VehicleMessage.WindowFaultMsg -> s.copy(windowFault = WindowMapper.fault(m, now))
        is VehicleMessage.Result, is VehicleMessage.QueryEnd, is VehicleMessage.Raw -> s
    }

    fun requestState(raw: Int): RequestState? = when (raw) {
        0 -> RequestState.ACCEPTED
        1 -> RequestState.IN_PROGRESS
        2 -> RequestState.DONE
        3 -> RequestState.REJECTED
        4 -> RequestState.CANCELLED
        5 -> RequestState.FAILED
        else -> null
    }

    private fun q(raw: Int) = Quality.fromRaw(raw)

    fun faultCategory(raw: Int): FaultCategory? = FaultCategory.entries.getOrNull(raw)

    fun <T> qualified(value: T?, quality: Quality, now: Long, age: Int): Qualified<T> {
        val shown = if (quality == Quality.INVALID || quality == Quality.NO_DATA) null else value
        return Qualified(shown, if (shown == null && quality == Quality.OK) Quality.NO_DATA else quality, now, age.toLong())
    }

    private fun door(m: VehicleMessage.DoorStateMsg, now: Long) = DoorState(
        lock = qualified(when (m.lock) { 0 -> LockState.LOCKED; 1 -> LockState.UNLOCKED; else -> null }, q(m.lockQuality), now, m.sourceAgeMs),
        open = qualified(when (m.open) { 0 -> OpenState.CLOSED; 1 -> OpenState.OPEN; else -> null }, q(m.openQuality), now, m.sourceAgeMs),
        integrity = qualified(
            when (m.composite) { 0 -> DoorIntegrity.NORMAL; 1 -> DoorIntegrity.INCONSISTENT; else -> DoorIntegrity.UNTRUSTED },
            q(m.compositeQuality), now, m.sourceAgeMs,
        ),
    )

    private fun fan(raw: Int): FanLevel? = when (raw) {
        0 -> FanLevel.OFF; 1 -> FanLevel.LOW; 2 -> FanLevel.MEDIUM; 3 -> FanLevel.HIGH
        255 -> FanLevel.UNKNOWN; else -> null
    }

    private fun climate(m: VehicleMessage.ClimateStateMsg, now: Long) = ClimateState(
        fanCommanded = qualified(fan(m.fanCommand), q(m.fanCommandQuality), now, m.sourceAgeMs),
        fanMeasured = qualified(fan(m.fanMeasured), q(m.fanMeasuredQuality), now, m.sourceAgeMs),
        tempDirection = qualified(
            when (m.thermalDirection) { 0 -> TempDirection.IDLE; 1 -> TempDirection.COOL; 2 -> TempDirection.HEAT; else -> null },
            q(m.thermalDirectionQuality), now, m.sourceAgeMs,
        ),
        outputPercent = qualified(m.thermalOutput.takeIf { it in 0..100 }, q(m.thermalOutputQuality), now, m.sourceAgeMs),
        heatRemoval = qualified(HeatRemoval.entries.getOrNull(m.heatRemoval), q(m.heatRemovalQuality), now, m.sourceAgeMs),
    )

    private fun light(m: VehicleMessage.LightStateMsg, now: Long) = LightState(
        activeType = qualified(LightType.entries.getOrNull(m.lightType), q(m.typeQuality), now, m.sourceAgeMs),
        applied = qualified(
            when (m.applyResult) { 0 -> LightApplied.APPLIED; 1 -> LightApplied.APPLY_FAILED; else -> LightApplied.UNKNOWN },
            q(m.applyQuality), now, m.sourceAgeMs,
        ),
        brightnessPercent = qualified(m.level.takeIf { it in 0..100 }, q(m.levelQuality), now, m.sourceAgeMs),
        rgb = qualified(Rgb(m.rgb.r, m.rgb.g, m.rgb.b), q(m.colorQuality), now, m.sourceAgeMs),
        physicalFeedbackSupported = qualified(onOff(m.physicalFeedback), Quality.OK, now, m.sourceAgeMs),
    )

    fun onOff(raw: Int): Boolean? = when (raw) { 0 -> false; 1 -> true; else -> null }

    private fun settings(m: VehicleMessage.UserSettings, now: Long): UserSettingsState {
        val quality = q(m.quality)
        return UserSettingsState(
            targetTemperatureC = qualified(m.targetTemperatureRaw.takeIf { it != I16_INVALID }?.let { it / 100.0 }, quality, now, m.sourceAgeMs),
            climateAuto = qualified(onOff(m.climateAuto), quality, now, m.sourceAgeMs),
            fanLevel = qualified(fan(m.fanLevel)?.takeIf { it != FanLevel.UNKNOWN }, quality, now, m.sourceAgeMs),
            lightEnabled = qualified(onOff(m.lightEnabled), quality, now, m.sourceAgeMs),
            lightLevelPercent = qualified(m.lightLevel.takeIf { it in 0..100 }, quality, now, m.sourceAgeMs),
            lightRgb = qualified(Rgb(m.rgb.r, m.rgb.g, m.rgb.b), quality, now, m.sourceAgeMs),
        )
    }

    private fun <T> measured(value: T?, valid: Boolean, now: Long, age: Int): Qualified<T> =
        qualified(value, if (!valid) Quality.INVALID else if (value == null) Quality.NO_DATA else Quality.OK, now, age)

    private fun environment(m: VehicleMessage.CisEnvironment, now: Long) = CabinEnvironment(
        temperatureC = measured(m.temperature.raw.takeIf { it != I16_INVALID.toLong() }?.let { it / 100.0 }, m.temperature.valid, now, m.temperature.ageMs),
        humidityPercent = measured(m.humidity.raw.takeIf { it != U16_INVALID.toLong() }?.let { it / 100.0 }, m.humidity.valid, now, m.humidity.ageMs),
        illuminanceLux = measured(m.illuminance.raw.takeIf { it != U32_INVALID }, m.illuminance.valid, now, m.illuminance.ageMs),
    )

    private fun occupant(m: VehicleMessage.CisOccupant, now: Long) = OccupantState(
        present = measured(when (m.presence) { 0 -> false; 1 -> true; else -> null }, m.presenceValid, now, m.visionAgeMs),
        count = measured(m.count.takeIf { it in 0..5 }, m.countValid, now, m.visionAgeMs),
    )

    private fun rear(m: VehicleMessage.CisRear, now: Long) = RearState(
        distanceCm = measured(m.distanceRaw.takeIf { it != U16_INVALID }?.let { it / 10.0 }, m.valid, now, m.sourceAgeMs),
        status = rearStatus(RearProximity.fromRaw(m.proximityStatus), m.valid, now, m.sourceAgeMs),
    )

    private fun rearStatus(status: RearProximity, valid: Boolean, now: Long, ageMs: Int) =
        qualified(status, if (valid || status == RearProximity.INACTIVE) Quality.OK else Quality.INVALID, now, ageMs)

    private fun availability(raw: Int) = when (raw) {
        0 -> Availability.AVAILABLE; 1 -> Availability.LIMITED; 2 -> Availability.UNAVAILABLE; else -> Availability.UNKNOWN
    }

    private fun autoUnlock(raw: Int) = when (raw) {
        2 -> AutoUnlockResult.DONE; 3 -> AutoUnlockResult.REJECTED; 5 -> AutoUnlockResult.FAILED; else -> AutoUnlockResult.UNKNOWN
    }

    fun warning(m: VehicleMessage.WarningMsg, now: Long): Warning? {
        val type = WarningType.fromRaw(m.warningType) ?: return null
        val severity = Severity.fromRaw(m.severity) ?: return null
        return Warning(
            type = type, active = m.active, severity = severity, quality = q(m.quality),
            occurrenceId = m.occurrenceId, domainBootId = m.context.domainBootId,
            read = if (m.read) ReadState.READ else ReadState.UNREAD, receivedAtMs = now, ageMs = m.sourceAgeMs.toLong(),
        )
    }

    private fun ecuState(raw: Int) = listOf("INIT", "READY", "DEGRADED", "FAULT").getOrElse(raw) { "UNKNOWN" }

    private fun bits(mask: Int, names: List<String>) = names.filterIndexed { i, _ -> mask and (1 shl i) != 0 }

    private fun VehicleSnapshot.withEcu(e: EcuHealth) = copy(ecus = (ecus.filter { it.name != e.name } + e).sortedBy { it.name })

    private val BCM_AFFECTED = listOf(VehicleFunction.DOOR, VehicleFunction.CLIMATE, VehicleFunction.INTERIOR_LIGHT)

    private const val NO_RESULT = 255
    private const val I16_INVALID = -32768
    private const val U16_INVALID = 65535
    private const val U32_INVALID = 0xFFFF_FFFFL

    private val BCM_FAULTS = listOf(
        "도어 센서", "팬 센서", "온도 센서", "과열", "팬", "잠금 구동기", "조명 적용", "통신 시간 초과",
    )
    private val CIS_FAULTS = listOf(
        "초기화 실패", "비전", "온도 센서", "습도 센서", "조도 센서", "근접 센서", "통신", "데이터 무효", "범위 초과",
    )
    private val VSS_FAULTS = listOf("음원 없음", "재생 시작 실패", "오디오 출력", "재생 상태", "초기화 실패")
}
