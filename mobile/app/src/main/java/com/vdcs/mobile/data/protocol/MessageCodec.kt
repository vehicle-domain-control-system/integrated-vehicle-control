package com.vdcs.mobile.data.protocol

import com.vdcs.mobile.domain.model.FanLevel
import com.vdcs.mobile.domain.model.UserRequest
import com.vdcs.mobile.domain.model.Warning
import com.vdcs.mobile.domain.repository.QueryScope
import kotlin.math.roundToInt

class MessageCodec {
    fun validate(request: UserRequest) {
        when (request) {
            is UserRequest.TargetTemperature ->
                require(temperatureRaw(request) in -32767..32767) { "목표 온도 ${request.celsius}°C 는 i16 0.01°C 범위 밖" }
            is UserRequest.Fan -> require(request.level != FanLevel.UNKNOWN) { "UNKNOWN 은 상태/측정 전용 (#56 §7.7)" }
            is UserRequest.LightBrightness -> require(request.percent in 0..100) { "밝기는 0~100%" }
            is UserRequest.Door, is UserRequest.ClimateAuto, is UserRequest.LightEnabled,
            is UserRequest.LightColor, is UserRequest.ProximityUnlock -> Unit
        }
    }

    fun encodeRequest(requestId: Long, request: UserRequest): ByteArray {
        require(requestId in 1..0xFFFF_FFFFL) { "REQUEST_ID 는 0 이 아닌 u32" }
        validate(request)
        val w = ByteWriter(12).u32(0, requestId)
        when (request) {
            is UserRequest.Door -> w.u8(4, KIND_DOOR).u8(5, if (request.lock) 0 else 1)
            is UserRequest.TargetTemperature -> w.u8(4, KIND_CLIMATE).u8(5, 1).i16(6, temperatureRaw(request))
            is UserRequest.ClimateAuto -> w.u8(4, KIND_CLIMATE).u8(5, 2).u8(6, request.enabled.toU8())
            is UserRequest.Fan -> w.u8(4, KIND_CLIMATE).u8(5, 3).u8(6, request.level.ordinal)
            is UserRequest.LightEnabled -> w.u8(4, KIND_LIGHT).u8(5, 1).u8(6, request.enabled.toU8())
            is UserRequest.LightBrightness -> w.u8(4, KIND_LIGHT).u8(5, 2).u8(6, request.percent)
            is UserRequest.LightColor ->
                w.u8(4, KIND_LIGHT).u8(5, 3).u8(6, request.rgb.r).u8(7, request.rgb.g).u8(8, request.rgb.b)
            is UserRequest.ProximityUnlock -> w.u8(4, KIND_DIGITAL_KEY).u8(5, 1).u8(6, request.enabled.toU8())
        }
        return w.toByteArray()
    }

    private fun temperatureRaw(r: UserRequest.TargetTemperature): Int = (r.celsius * 100).roundToInt()

    fun encodeQuery(
        queryId: Long,
        scope: QueryScope,
        requestSession: Long = 0,
        targetRequestId: Long = 0,
    ): ByteArray {
        require(queryId in 1..0xFFFF_FFFFL) { "QUERY_ID 는 0 이 아닌 u32" }
        return ByteWriter(20)
            .u32(0, queryId)
            .u8(4, scope.raw)
            .u8(5, 0)
            .u64(6, requestSession)
            .u32(14, targetRequestId)
            .u16(18, 0)
            .toByteArray()
    }

    fun encodeWarningAck(warning: Warning): ByteArray =
        ByteWriter(9)
            .u8(0, warning.type.raw)
            .u32(1, warning.occurrenceId)
            .u32(5, warning.domainBootId)
            .toByteArray()

    fun decode(messageType: Int, payload: ByteArray): VehicleMessage? {
        val expected = MessageType.PAYLOAD_LENGTH[messageType]
        if (expected != null && expected != payload.size) return null
        if (expected == null || messageType < MessageType.M_RESULT) return VehicleMessage.Raw(messageType, payload)
        val r = ByteReader(payload)
        val ctx = DownstreamContext(r.u32(0), r.u64(4), r.u32(12), r.u32(16))
        return when (messageType) {
            MessageType.M_RESULT -> VehicleMessage.Result(
                ctx,
                requestSession = r.u64(20), requestId = r.u32(28),
                result = r.u8(32), reason = r.u16(33), confirmed = r.u8(35) == 1,
                commandTarget = r.u8(36), commandId = r.u32(37), origin = r.u8(41), ageMs = r.u16(42),
            )
            MessageType.M_WARNING -> VehicleMessage.WarningMsg(
                ctx,
                warningType = r.u8(20), occurrenceId = r.u32(21), severity = r.u8(25),
                active = r.u8(26) == 1, read = r.u8(27) == 1,
                quality = r.u8(28), qualityReason = r.u8(29), sourceAgeMs = r.u16(38),
            )
            MessageType.M_AVAILABILITY -> VehicleMessage.Availability(
                ctx,
                entries = (0 until 9).associate { i ->
                    val base = 20 + i * 4
                    r.u8(base) to FunctionAvailability(r.u8(base + 1), r.u16(base + 2))
                },
                sourceAgeMs = r.u16(60),
            )
            MessageType.M_DIGITAL_STATUS -> VehicleMessage.DigitalStatus(
                ctx,
                settingState = r.u8(20), availability = r.u8(21), reason = r.u16(22), sourceAgeMs = r.u16(28),
            )
            MessageType.M_DIGITAL_RESULT -> VehicleMessage.DigitalResult(
                ctx,
                autoOccurrenceId = r.u32(20), relatedDoorCommandId = r.u32(24), origin = r.u8(28),
                result = r.u8(29), reason = r.u16(30), confirmed = r.u8(32) == 1, ageMs = r.u16(33),
            )
            MessageType.M_USER_SETTINGS -> VehicleMessage.UserSettings(
                ctx,
                targetTemperatureRaw = r.i16(20), climateAuto = r.u8(22), fanLevel = r.u8(23),
                lightEnabled = r.u8(24), lightLevel = r.u8(25),
                rgb = RawRgb(r.u8(26), r.u8(27), r.u8(28)), quality = r.u8(29), sourceAgeMs = r.u16(34),
            )
            MessageType.M_BCM_DOOR_STATE -> VehicleMessage.DoorStateMsg(
                ctx,
                lock = r.u8(28), open = r.u8(29), composite = r.u8(30),
                lockQuality = r.u8(31), openQuality = r.u8(32), compositeQuality = r.u8(33),
                sourceAgeMs = r.u16(38),
            )
            MessageType.M_BCM_CLIMATE_STATE -> VehicleMessage.ClimateStateMsg(
                ctx,
                fanCommand = r.u8(28), fanMeasured = r.u8(29), thermalDirection = r.u8(30),
                thermalOutput = r.u8(31), heatRemoval = r.u8(32),
                fanCommandQuality = r.u8(33), fanMeasuredQuality = r.u8(34),
                thermalDirectionQuality = r.u8(35), thermalOutputQuality = r.u8(36),
                heatRemovalQuality = r.u8(37), sourceAgeMs = r.u16(42),
            )
            MessageType.M_BCM_LIGHT_STATE -> VehicleMessage.LightStateMsg(
                ctx,
                lightType = r.u8(28), level = r.u8(29), rgb = RawRgb(r.u8(30), r.u8(31), r.u8(32)),
                applyResult = r.u8(33), physicalFeedback = r.u8(34),
                typeQuality = r.u8(35), levelQuality = r.u8(36), colorQuality = r.u8(37),
                applyQuality = r.u8(38), sourceAgeMs = r.u16(43),
            )
            MessageType.M_BCM_STATUS -> VehicleMessage.BcmStatus(
                ctx,
                ecuState = r.u8(28), faultMask = r.u16(29), faultCategory = r.u8(31),
                faultCode = r.u16(32), affectedFunction = r.u8(34), recovering = r.u8(35) == 1,
                sourceAgeMs = r.u16(40),
            )
            MessageType.M_CIS_ENVIRONMENT -> VehicleMessage.CisEnvironment(
                ctx,
                temperature = Measurement(r.i16(28).toLong(), r.u8(36) == 1, r.u8(37), r.u16(34)),
                humidity = Measurement(r.u16(38).toLong(), r.u8(46) == 1, r.u8(47), r.u16(44)),
                illuminance = Measurement(r.u32(48), r.u8(58) == 1, r.u8(59), r.u16(56)),
            )
            MessageType.M_CIS_OCCUPANT -> VehicleMessage.CisOccupant(
                ctx,
                presence = r.u8(38), presenceValid = r.u8(39) == 1, presenceReason = r.u8(40),
                count = r.u8(41), countValid = r.u8(42) == 1, countReason = r.u8(43),
                visionAgeMs = r.u16(36),
            )
            MessageType.M_CIS_REAR -> VehicleMessage.CisRear(
                ctx,
                distanceRaw = r.u16(34), valid = r.u8(36) == 1, qualityReason = r.u8(37),
                proximityStatus = r.u8(38), sourceAgeMs = r.u16(32),
            )
            MessageType.M_CIS_STATUS -> VehicleMessage.CisStatus(
                ctx,
                cisState = r.u8(28), faultMask = r.u16(36), recovering = r.u8(40) == 1, sourceAgeMs = r.u16(52),
            )
            MessageType.M_VSS_STATUS -> VehicleMessage.VssStatus(
                ctx,
                vssState = r.u8(28), availability = r.u8(29), faultActive = r.u8(31) == 1,
                faultMask = r.u8(32), sourceAgeMs = r.u16(46),
            )
            MessageType.M_QUERY_END -> VehicleMessage.QueryEnd(
                ctx,
                scope = r.u8(20), status = r.u8(21), itemCount = r.u16(22),
            )
            MessageType.M_WINDOW_STATE -> VehicleMessage.WindowStateMsg(
                ctx,
                motion = r.u8(28), ecuState = r.u8(29), position = r.u8(30), quality = r.u8(31),
                fullyOpen = r.u8(32), fullyClosed = r.u8(33), antiPinch = r.u8(34), reverse = r.u8(35),
                sourceAgeMs = r.u16(40),
            )
            MessageType.M_WINDOW_FAULT -> VehicleMessage.WindowFaultMsg(
                ctx,
                category = r.u8(28), code = r.u16(29), status = r.u8(31), occurrenceId = r.u32(32),
                sourceAgeMs = r.u16(40),
            )
            else -> VehicleMessage.Raw(messageType, payload, ctx)
        }
    }

    private fun Boolean.toU8() = if (this) 1 else 0

    private companion object {
        const val KIND_DOOR = 1
        const val KIND_CLIMATE = 2
        const val KIND_LIGHT = 3
        const val KIND_DIGITAL_KEY = 4
    }
}
