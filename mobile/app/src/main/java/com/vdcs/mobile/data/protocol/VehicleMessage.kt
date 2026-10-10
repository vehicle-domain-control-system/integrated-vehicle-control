package com.vdcs.mobile.data.protocol

data class DownstreamContext(
    val deviceContextId: Long,
    val sessionId: Long,
    val domainBootId: Long,
    val queryId: Long,
)

data class RawRgb(val r: Int, val g: Int, val b: Int)

data class FunctionAvailability(val availability: Int, val reason: Int)

data class Measurement(val raw: Long, val valid: Boolean, val reason: Int, val ageMs: Int)

sealed interface VehicleMessage {
    val context: DownstreamContext?

    data class Result(
        override val context: DownstreamContext,
        val requestSession: Long,
        val requestId: Long,
        val result: Int,
        val reason: Int,
        val confirmed: Boolean,
        val commandTarget: Int,
        val commandId: Long,
        val origin: Int,
        val ageMs: Int,
    ) : VehicleMessage

    data class WarningMsg(
        override val context: DownstreamContext,
        val warningType: Int,
        val occurrenceId: Long,
        val severity: Int,
        val active: Boolean,
        val read: Boolean,
        val quality: Int,
        val qualityReason: Int,
        val sourceAgeMs: Int,
    ) : VehicleMessage

    data class Availability(
        override val context: DownstreamContext,
        val entries: Map<Int, FunctionAvailability>,
        val sourceAgeMs: Int,
    ) : VehicleMessage

    data class DigitalStatus(
        override val context: DownstreamContext,
        val settingState: Int,
        val availability: Int,
        val reason: Int,
        val sourceAgeMs: Int,
    ) : VehicleMessage

    data class DigitalResult(
        override val context: DownstreamContext,
        val autoOccurrenceId: Long,
        val relatedDoorCommandId: Long,
        val origin: Int,
        val result: Int,
        val reason: Int,
        val confirmed: Boolean,
        val ageMs: Int,
    ) : VehicleMessage

    data class UserSettings(
        override val context: DownstreamContext,
        val targetTemperatureRaw: Int,
        val climateAuto: Int,
        val fanLevel: Int,
        val lightEnabled: Int,
        val lightLevel: Int,
        val rgb: RawRgb,
        val quality: Int,
        val sourceAgeMs: Int,
    ) : VehicleMessage

    data class DoorStateMsg(
        override val context: DownstreamContext,
        val lock: Int,
        val open: Int,
        val composite: Int,
        val lockQuality: Int,
        val openQuality: Int,
        val compositeQuality: Int,
        val sourceAgeMs: Int,
    ) : VehicleMessage

    data class ClimateStateMsg(
        override val context: DownstreamContext,
        val fanCommand: Int,
        val fanMeasured: Int,
        val thermalDirection: Int,
        val thermalOutput: Int,
        val heatRemoval: Int,
        val fanCommandQuality: Int,
        val fanMeasuredQuality: Int,
        val thermalDirectionQuality: Int,
        val thermalOutputQuality: Int,
        val heatRemovalQuality: Int,
        val sourceAgeMs: Int,
    ) : VehicleMessage

    data class LightStateMsg(
        override val context: DownstreamContext,
        val lightType: Int,
        val level: Int,
        val rgb: RawRgb,
        val applyResult: Int,
        val physicalFeedback: Int,
        val typeQuality: Int,
        val levelQuality: Int,
        val colorQuality: Int,
        val applyQuality: Int,
        val sourceAgeMs: Int,
    ) : VehicleMessage

    data class BcmStatus(
        override val context: DownstreamContext,
        val ecuState: Int,
        val faultMask: Int,
        val faultCategory: Int,
        val faultCode: Int,
        val affectedFunction: Int,
        val recovering: Boolean,
        val sourceAgeMs: Int,
    ) : VehicleMessage

    data class CisEnvironment(
        override val context: DownstreamContext,
        val temperature: Measurement,
        val humidity: Measurement,
        val illuminance: Measurement,
    ) : VehicleMessage

    data class CisOccupant(
        override val context: DownstreamContext,
        val presence: Int,
        val presenceValid: Boolean,
        val presenceReason: Int,
        val count: Int,
        val countValid: Boolean,
        val countReason: Int,
        val visionAgeMs: Int,
    ) : VehicleMessage

    data class CisRear(
        override val context: DownstreamContext,
        val distanceRaw: Int,
        val valid: Boolean,
        val qualityReason: Int,
        val proximityStatus: Int,
        val sourceAgeMs: Int,
    ) : VehicleMessage

    data class CisStatus(
        override val context: DownstreamContext,
        val cisState: Int,
        val faultMask: Int,
        val recovering: Boolean,
        val sourceAgeMs: Int,
    ) : VehicleMessage

    data class VssStatus(
        override val context: DownstreamContext,
        val vssState: Int,
        val availability: Int,
        val faultActive: Boolean,
        val faultMask: Int,
        val sourceAgeMs: Int,
    ) : VehicleMessage

    data class WindowStateMsg(
        override val context: DownstreamContext,
        val motion: Int,
        val ecuState: Int,
        val position: Int,
        val quality: Int,
        val fullyOpen: Int,
        val fullyClosed: Int,
        val antiPinch: Int,
        val reverse: Int,
        val sourceAgeMs: Int,
    ) : VehicleMessage

    data class WindowFaultMsg(
        override val context: DownstreamContext,
        val category: Int,
        val code: Int,
        val status: Int,
        val occurrenceId: Long,
        val sourceAgeMs: Int,
    ) : VehicleMessage

    data class QueryEnd(
        override val context: DownstreamContext,
        val scope: Int,
        val status: Int,
        val itemCount: Int,
    ) : VehicleMessage

    class Raw(
        val messageType: Int,
        val payload: ByteArray,
        override val context: DownstreamContext? = null,
    ) : VehicleMessage
}
