package com.vdcs.mobile.domain.model

data class Qualified<T>(
    val value: T?,
    val quality: Quality,
    val receivedAtMs: Long,
    val ageMs: Long = 0,
)

data class DoorState(
    val lock: Qualified<LockState>,
    val open: Qualified<OpenState>,

    val integrity: Qualified<DoorIntegrity>,
)

enum class LockState { LOCKED, UNLOCKED, UNKNOWN }
enum class OpenState { CLOSED, OPEN, UNKNOWN }
enum class DoorIntegrity { NORMAL, INCONSISTENT, UNTRUSTED }

data class ClimateState(
    val fanCommanded: Qualified<FanLevel>,
    val fanMeasured: Qualified<FanLevel>,
    val tempDirection: Qualified<TempDirection>,
    val outputPercent: Qualified<Int>,

    val heatRemoval: Qualified<HeatRemoval>,
)

enum class FanLevel { OFF, LOW, MEDIUM, HIGH, UNKNOWN }
enum class TempDirection { COOL, HEAT, IDLE }

enum class HeatRemoval { NORMAL, OVERHEAT_CUTOFF }

data class LightState(
    val activeType: Qualified<LightType>,
    val applied: Qualified<LightApplied>,
    val brightnessPercent: Qualified<Int>,
    val rgb: Qualified<Rgb>,

    val physicalFeedbackSupported: Qualified<Boolean>,
)

enum class LightType { NORMAL, GOODBYE, WARNING, FAULT }
enum class LightApplied { APPLIED, APPLY_FAILED, UNKNOWN }
data class Rgb(val r: Int, val g: Int, val b: Int)

data class DigitalKeyState(
    val proximityUnlockEnabled: Qualified<Boolean>,
    val lastAutoUnlock: Qualified<AutoUnlockResult>?,
)

enum class AutoUnlockResult { DONE, REJECTED, FAILED, UNKNOWN }

data class UserSettingsState(
    val targetTemperatureC: Qualified<Double>,
    val climateAuto: Qualified<Boolean>,
    val fanLevel: Qualified<FanLevel>,
    val lightEnabled: Qualified<Boolean>,
    val lightLevelPercent: Qualified<Int>,
    val lightRgb: Qualified<Rgb>,
)

data class CabinEnvironment(
    val temperatureC: Qualified<Double>,
    val humidityPercent: Qualified<Double>,
    val illuminanceLux: Qualified<Long>,
)

data class OccupantState(
    val present: Qualified<Boolean>,
    val count: Qualified<Int>,
)

data class RearState(
    val distanceCm: Qualified<Double>,
    val status: Qualified<RearProximity>,
)

enum class RearProximity(val raw: Int) {
    VALID_DISTANCE(0), NO_OBJECT(1), UNAVAILABLE(2), FAULT(3), RECOVERING(4), INACTIVE(5), UNKNOWN(-1);

    companion object {
        fun fromRaw(raw: Int): RearProximity = entries.firstOrNull { it.raw == raw && it != UNKNOWN } ?: UNKNOWN
    }
}

enum class VehicleFunction(val raw: Int) {
    DOOR(1), CLIMATE(2), INTERIOR_LIGHT(3), DIGITAL_KEY(4), OCCUPANT(5),
    ENVIRONMENT(6), REAR_WARNING(7), WINDOW(8), VSS(9);

    companion object {
        fun fromRaw(raw: Int): VehicleFunction? = entries.firstOrNull { it.raw == raw }
    }
}

enum class Availability { AVAILABLE, LIMITED, UNAVAILABLE, UNKNOWN }

data class FunctionStatus(
    val availability: Availability,
    val reason: ResultReason,
    val receivedAtMs: Long,
    val quality: Quality = Quality.OK,
    val ageMs: Long,
)

enum class FaultCategory { NONE, SENSOR, FUNCTION, COMM }

data class EcuHealth(
    val name: String,
    val state: String,
    val faults: List<String>,
    val recovering: Boolean,
    val receivedAtMs: Long,
    val quality: Quality = Quality.OK,
    val faultCategory: FaultCategory?,
    val faultCode: Int?,
    val affectedFunctions: Set<VehicleFunction>,
    val ageMs: Long,
)

data class VehicleSnapshot(
    val door: DoorState? = null,
    val climate: ClimateState? = null,
    val light: LightState? = null,
    val digitalKey: DigitalKeyState? = null,
    val settings: UserSettingsState? = null,
    val environment: CabinEnvironment? = null,
    val occupant: OccupantState? = null,
    val rear: RearState? = null,
    val availability: Map<VehicleFunction, FunctionStatus> = emptyMap(),
    val ecus: List<EcuHealth> = emptyList(),
    val warnings: List<Warning> = emptyList(),
    val window: WindowState? = null,
    val windowFault: WindowFault? = null,

    val lastReceivedAtMs: Long? = null,
)
