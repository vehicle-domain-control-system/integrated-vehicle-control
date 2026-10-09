package com.vdcs.mobile.data.demo

import com.vdcs.mobile.data.protocol.ByteReader
import com.vdcs.mobile.data.protocol.MessageType

internal typealias Downlink = Pair<Int, ByteArray>

internal class DemoVehicle {
    private var doorLocked = true
    private var doorOpen = false
    private var targetTemperatureRaw = 2200
    private var climateAuto = false
    private var fanLevel = 0
    private var cabinTemperatureRaw = 2450
    private var lightEnabled = true
    private var lightLevel = 60
    private var lightRgb = DemoDownlink.Rgb(255, 255, 255)
    private var proximityUnlock = false

    private class WarningEntry(var occurrenceId: Long = 0, var severity: Int = 0, var active: Boolean = false, var read: Boolean = false)

    private val warnings = (1..WARNING_TYPES).associateWith { WarningEntry() }
    private var nextOccurrenceId = 1L

    class ResultEntry(val requestSession: Long, val requestId: Long, var result: Int, var reason: Int, val target: Int, val commandId: Long)

    private val results = LinkedHashMap<Pair<Long, Long>, ResultEntry>()
    private var nextCommandId = 1L
    private var sequence = 1L

    fun setDoorOpen(open: Boolean, c: DemoDownlink.Context): List<Downlink> {
        doorOpen = open
        return listOf(doorMessage(c))
    }

    fun setCabinTemperature(celsius: Double, c: DemoDownlink.Context): List<Downlink> {
        cabinTemperatureRaw = Math.round(celsius * 100).toInt()
        return listOf(environmentMessage(c), climateMessage(c))
    }

    fun raiseWarning(type: Int, severity: Int, c: DemoDownlink.Context): List<Downlink> {
        val w = warnings.getValue(type)
        w.occurrenceId = nextOccurrenceId++
        w.severity = severity
        w.active = true
        w.read = false
        return listOf(warningMessage(c, type))
    }

    fun clearWarning(type: Int, c: DemoDownlink.Context): List<Downlink> {
        warnings.getValue(type).active = false
        return listOf(warningMessage(c, type))
    }

    fun clearAllWarnings(c: DemoDownlink.Context): List<Downlink> =
        warnings.filterValues { it.active }.keys.flatMap { clearWarning(it, c) }

    fun acknowledge(type: Int, occurrenceId: Long, bootMatches: Boolean, c: DemoDownlink.Context): List<Downlink> {
        val w = warnings[type]
        if (w == null || !bootMatches || occurrenceId == 0L || w.occurrenceId != occurrenceId) return emptyList()
        w.read = true
        return listOf(warningMessage(c, type))
    }

    sealed class Action(val target: Int) {
        data class Door(val lock: Boolean) : Action(TARGET_BCM)
        data class TargetTemperature(val raw: Int) : Action(TARGET_DOMAIN)
        data class ClimateAuto(val on: Boolean) : Action(TARGET_DOMAIN)
        data class Fan(val level: Int) : Action(TARGET_BCM)
        data class LightEnabled(val on: Boolean) : Action(TARGET_BCM)
        data class LightLevel(val percent: Int) : Action(TARGET_BCM)
        data class LightColor(val rgb: DemoDownlink.Rgb) : Action(TARGET_BCM)
        data class ProximityUnlock(val on: Boolean) : Action(TARGET_DOMAIN)
    }

    fun parseAction(kind: Int, op: Int, arg: ByteArray): Action? {
        val a = ByteReader(arg)
        fun onOff() = when (a.u8(0)) { 0 -> false; 1 -> true; else -> null }
        return when (kind to op) {
            1 to 0 -> Action.Door(lock = true)
            1 to 1 -> Action.Door(lock = false)
            2 to 1 -> Action.TargetTemperature(a.i16(0))
            2 to 2 -> onOff()?.let { Action.ClimateAuto(it) }
            2 to 3 -> a.u8(0).takeIf { it in 0..3 }?.let { Action.Fan(it) }
            3 to 1 -> onOff()?.let { Action.LightEnabled(it) }
            3 to 2 -> a.u8(0).takeIf { it in 0..100 }?.let { Action.LightLevel(it) }
            3 to 3 -> Action.LightColor(DemoDownlink.Rgb(a.u8(0), a.u8(1), a.u8(2)))
            4 to 1 -> onOff()?.let { Action.ProximityUnlock(it) }
            else -> null
        }
    }

    fun knownResult(session: Long, requestId: Long): ResultEntry? = results[session to requestId]

    fun remember(session: Long, requestId: Long, action: Action) {
        val commandId = if (action.target == TARGET_BCM) nextCommandId++ else 0
        results[session to requestId] = ResultEntry(session, requestId, RESULT_NONE, REASON_NONE, action.target, commandId)
        while (results.size > RECENT_RESULTS) results.remove(results.keys.first())
    }

    fun settle(session: Long, requestId: Long, result: Int, reason: Int): ResultEntry? =
        results[session to requestId]?.also {
            it.result = result
            it.reason = reason
        }

    fun rejects(action: Action): Boolean = action is Action.Door && action.lock && doorOpen

    fun apply(action: Action): Boolean = when (action) {
        is Action.Door -> (doorLocked == action.lock).also { doorLocked = action.lock }
        is Action.TargetTemperature -> (targetTemperatureRaw == action.raw).also { targetTemperatureRaw = action.raw }
        is Action.ClimateAuto -> (climateAuto == action.on).also { climateAuto = action.on }
        is Action.Fan -> (fanLevel == action.level).also { fanLevel = action.level }
        is Action.LightEnabled -> (lightEnabled == action.on).also { lightEnabled = action.on }
        is Action.LightLevel -> (lightLevel == action.percent).also { lightLevel = action.percent }
        is Action.LightColor -> (lightRgb == action.rgb).also { lightRgb = action.rgb }
        is Action.ProximityUnlock -> (proximityUnlock == action.on).also { proximityUnlock = action.on }
    }

    fun affected(action: Action, c: DemoDownlink.Context): List<Downlink> = when (action) {
        is Action.Door -> listOf(doorMessage(c))
        is Action.TargetTemperature, is Action.ClimateAuto, is Action.Fan -> listOf(settingsMessage(c), climateMessage(c))
        is Action.LightEnabled, is Action.LightLevel, is Action.LightColor -> listOf(settingsMessage(c), lightMessage(c))
        is Action.ProximityUnlock -> listOf(digitalStatusMessage(c))
    }

    fun resultMessage(c: DemoDownlink.Context, e: ResultEntry): Downlink = MessageType.M_RESULT to DemoDownlink.result(
        c, e.requestSession, e.requestId, e.result, e.reason,
        confirmed = e.result == RESULT_DONE || e.result == RESULT_REJECTED,
        commandTarget = e.target, commandId = e.commandId, ageMs = 0,
    )

    fun queryItems(c: DemoDownlink.Context, scope: Int, requestSession: Long, requestId: Long): Pair<List<Downlink>, Int> =
        when (scope) {
            SCOPE_CURRENT -> currentState(c) to STATUS_DONE
            SCOPE_REQUEST_RESULT -> {
                val found = if (requestId != 0L) listOfNotNull(results[requestSession to requestId]) else results.values.toList()
                found.filter { it.result != RESULT_NONE }.map { resultMessage(c, it) }
                    .let { it to if (it.isEmpty()) STATUS_NO_REQUEST else STATUS_DONE }
            }
            SCOPE_WARNINGS -> allWarnings(c) to STATUS_DONE
            else -> (currentState(c) + settledResults(c) + allWarnings(c)) to STATUS_DONE
        }

    private fun settledResults(c: DemoDownlink.Context) =
        results.values.filter { it.result != RESULT_NONE }.map { resultMessage(c, it) }

    private fun currentState(c: DemoDownlink.Context): List<Downlink> =
        fastMessages(c) + listOf(
            MessageType.M_BCM_STATUS to DemoDownlink.bcmStatus(c, next(), next()),
            MessageType.M_CIS_STATUS to DemoDownlink.cisStatus(c, next(), next()),
            MessageType.M_WINDOW_STATE to DemoDownlink.windowState(c, next()),
            MessageType.M_WINDOW_FAULT to DemoDownlink.windowFault(c, next()),
            MessageType.M_VSS_STATUS to DemoDownlink.vssStatus(c, next(), next()),
            settingsMessage(c),
            availabilityMessage(c),
            digitalStatusMessage(c),
        )

    private fun allWarnings(c: DemoDownlink.Context) = warnings.keys.map { warningMessage(c, it) }

    fun fastMessages(c: DemoDownlink.Context): List<Downlink> = listOf(
        doorMessage(c),
        climateMessage(c),
        lightMessage(c),
        environmentMessage(c),
        MessageType.M_CIS_OCCUPANT to DemoDownlink.occupant(c, present = false, count = 0, next(), next()),
        MessageType.M_CIS_REAR to DemoDownlink.rearNoObject(c, next(), next()),
    )

    fun slowMessages(c: DemoDownlink.Context): List<Downlink> = listOf(
        MessageType.M_BCM_STATUS to DemoDownlink.bcmStatus(c, next(), next()),
        MessageType.M_CIS_STATUS to DemoDownlink.cisStatus(c, next(), next()),
        MessageType.M_VSS_STATUS to DemoDownlink.vssStatus(c, next(), next()),
        availabilityMessage(c),
        digitalStatusMessage(c),
        settingsMessage(c),
    ) + allWarnings(c)

    private fun doorMessage(c: DemoDownlink.Context) =
        MessageType.M_BCM_DOOR_STATE to DemoDownlink.doorState(c, doorLocked, doorOpen, next(), next())

    private fun climateMessage(c: DemoDownlink.Context): Downlink {
        val diff = cabinTemperatureRaw - targetTemperatureRaw
        val direction = when {
            fanLevel == 0 -> THERMAL_IDLE
            diff > THERMAL_DEADBAND -> THERMAL_COOL
            diff < -THERMAL_DEADBAND -> THERMAL_HEAT
            else -> THERMAL_IDLE
        }
        val output = if (direction == THERMAL_IDLE) 0 else minOf(100, 20 * fanLevel + kotlin.math.abs(diff) / 10)
        return MessageType.M_BCM_CLIMATE_STATE to DemoDownlink.climateState(c, fanLevel, fanLevel, direction, output, next(), next())
    }

    private fun lightMessage(c: DemoDownlink.Context) =
        MessageType.M_BCM_LIGHT_STATE to DemoDownlink.lightState(c, if (lightEnabled) lightLevel else 0, lightRgb, next(), next())

    private fun environmentMessage(c: DemoDownlink.Context) =
        MessageType.M_CIS_ENVIRONMENT to DemoDownlink.environment(c, cabinTemperatureRaw, 4500, 320, next(), next())

    private fun settingsMessage(c: DemoDownlink.Context) = MessageType.M_USER_SETTINGS to
        DemoDownlink.userSettings(c, targetTemperatureRaw, climateAuto, fanLevel, lightEnabled, lightLevel, lightRgb, next())

    private fun availabilityMessage(c: DemoDownlink.Context) = MessageType.M_AVAILABILITY to DemoDownlink.availability(
        c,
        (1..9).map { id -> if (id == FUNCTION_WINDOW) UNAVAILABLE to REASON_NOT_READY else AVAILABLE to REASON_NONE },
        next(),
    )

    private fun digitalStatusMessage(c: DemoDownlink.Context) =
        MessageType.M_DIGITAL_STATUS to DemoDownlink.digitalStatus(c, proximityUnlock, next())

    private fun warningMessage(c: DemoDownlink.Context, type: Int): Downlink {
        val w = warnings.getValue(type)
        return MessageType.M_WARNING to DemoDownlink.warning(c, type, w.occurrenceId, w.severity, w.active, w.read, next())
    }

    private fun next(): Long = sequence.also { sequence = if (it >= 0xFFFF_FFFFL) 1 else it + 1 }

    companion object {
        const val SCOPE_ALL = 3
        const val RESULT_ACCEPTED = 0
        const val RESULT_DONE = 2
        const val RESULT_REJECTED = 3
        const val RESULT_NONE = 255
        const val REASON_NONE = 0
        const val REASON_DOOR_OPEN = 4
        const val REASON_ALREADY_AT_TARGET = 9

        private const val WARNING_TYPES = 8
        private const val SCOPE_CURRENT = 0
        private const val SCOPE_REQUEST_RESULT = 1
        private const val SCOPE_WARNINGS = 2
        private const val STATUS_DONE = 0
        private const val STATUS_NO_REQUEST = 1
        private const val REASON_NOT_READY = 17
        private const val RECENT_RESULTS = 8
        private const val TARGET_DOMAIN = 0
        private const val TARGET_BCM = 1
        private const val AVAILABLE = 0
        private const val UNAVAILABLE = 2
        private const val FUNCTION_WINDOW = 8
        private const val THERMAL_IDLE = 0
        private const val THERMAL_COOL = 1
        private const val THERMAL_HEAT = 2
        private const val THERMAL_DEADBAND = 50
    }
}
