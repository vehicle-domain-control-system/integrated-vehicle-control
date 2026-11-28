package com.vdcs.mobile.data.demo

import com.vdcs.mobile.data.protocol.ByteWriter
import com.vdcs.mobile.data.protocol.MessageType

internal object DemoDownlink {
    data class Context(val deviceContextId: Long, val sessionId: Long, val domainBootId: Long, val queryId: Long)

    data class Rgb(val r: Int, val g: Int, val b: Int)

    const val SOURCE_BOOT = 1L

    private fun start(type: Int, c: Context): ByteWriter =
        ByteWriter(MessageType.PAYLOAD_LENGTH.getValue(type))
            .u32(0, c.deviceContextId).u64(4, c.sessionId).u32(12, c.domainBootId).u32(16, c.queryId)

    fun result(
        c: Context, requestSession: Long, requestId: Long, result: Int, reason: Int,
        confirmed: Boolean, commandTarget: Int, commandId: Long, ageMs: Int,
    ): ByteArray = start(MessageType.M_RESULT, c)
        .u64(20, requestSession).u32(28, requestId).u8(32, result).u16(33, reason)
        .u8(35, if (confirmed) 1 else 0).u8(36, commandTarget).u32(37, commandId)
        .u8(41, ORIGIN_MOBILE_MANUAL).u16(42, ageMs)
        .toByteArray()

    fun warning(
        c: Context, type: Int, occurrenceId: Long, severity: Int, active: Boolean, read: Boolean, sequence: Long,
    ): ByteArray = start(MessageType.M_WARNING, c)
        .u8(20, type).u32(21, occurrenceId).u8(25, severity).u8(26, if (active) 1 else 0).u8(27, if (read) 1 else 0)
        .u8(28, QUALITY_OK).u8(29, REASON_NONE).u32(30, SOURCE_BOOT).u32(34, sequence).u16(38, 0)
        .toByteArray()

    fun availability(c: Context, entries: List<Pair<Int, Int>>, sequence: Long): ByteArray {
        require(entries.size == 9)
        val w = start(MessageType.M_AVAILABILITY, c)
        entries.forEachIndexed { i, (availability, reason) ->
            val base = 20 + i * 4
            w.u8(base, i + 1).u8(base + 1, availability).u16(base + 2, reason)
        }
        return w.u32(56, sequence).u16(60, 0).toByteArray()
    }

    fun digitalStatus(c: Context, settingOn: Boolean, sequence: Long): ByteArray =
        start(MessageType.M_DIGITAL_STATUS, c)
            .u8(20, if (settingOn) 1 else 0).u8(21, AVAILABLE).u16(22, REASON_NONE).u32(24, sequence).u16(28, 0)
            .toByteArray()

    fun userSettings(
        c: Context, targetTemperatureRaw: Int, climateAuto: Boolean, fanLevel: Int,
        lightEnabled: Boolean, lightLevel: Int, rgb: Rgb, sequence: Long,
    ): ByteArray = start(MessageType.M_USER_SETTINGS, c)
        .i16(20, targetTemperatureRaw).u8(22, if (climateAuto) 1 else 0).u8(23, fanLevel)
        .u8(24, if (lightEnabled) 1 else 0).u8(25, lightLevel)
        .u8(26, rgb.r).u8(27, rgb.g).u8(28, rgb.b)
        .u8(29, QUALITY_OK).u32(30, sequence).u16(34, 0)
        .toByteArray()

    fun doorState(c: Context, locked: Boolean, open: Boolean, txSequence: Long, sequence: Long): ByteArray =
        start(MessageType.M_BCM_DOOR_STATE, c)
            .u32(20, SOURCE_BOOT).u32(24, txSequence)
            .u8(28, if (locked) 0 else 1).u8(29, if (open) 1 else 0)
            .u8(30, if (locked && open) 1 else 0)
            .u8(31, QUALITY_OK).u8(32, QUALITY_OK).u8(33, QUALITY_OK)
            .u32(34, sequence).u16(38, 0)
            .toByteArray()

    fun climateState(
        c: Context, fanCommand: Int, fanMeasured: Int, thermalDirection: Int, thermalOutput: Int,
        txSequence: Long, sequence: Long,
    ): ByteArray = start(MessageType.M_BCM_CLIMATE_STATE, c)
        .u32(20, SOURCE_BOOT).u32(24, txSequence)
        .u8(28, fanCommand).u8(29, fanMeasured).u8(30, thermalDirection).u8(31, thermalOutput).u8(32, 0)
        .u8(33, QUALITY_OK).u8(34, QUALITY_OK).u8(35, QUALITY_OK).u8(36, QUALITY_OK).u8(37, QUALITY_OK)
        .u32(38, sequence).u16(42, 0)
        .toByteArray()

    fun lightState(c: Context, level: Int, rgb: Rgb, txSequence: Long, sequence: Long): ByteArray =
        start(MessageType.M_BCM_LIGHT_STATE, c)
            .u32(20, SOURCE_BOOT).u32(24, txSequence)
            .u8(28, LIGHT_NORMAL).u8(29, level).u8(30, rgb.r).u8(31, rgb.g).u8(32, rgb.b)
            .u8(33, 0).u8(34, 0)
            .u8(35, QUALITY_OK).u8(36, QUALITY_OK).u8(37, QUALITY_OK).u8(38, QUALITY_OK)
            .u32(39, sequence).u16(43, 0)
            .toByteArray()

    fun bcmStatus(c: Context, txSequence: Long, sequence: Long): ByteArray =
        start(MessageType.M_BCM_STATUS, c)
            .u32(20, SOURCE_BOOT).u32(24, txSequence)
            .u8(28, 1).u16(29, 0).u8(31, 0).u16(32, 0).u8(34, 0).u8(35, 0)
            .u32(36, sequence).u16(40, 0)
            .toByteArray()

    fun environment(
        c: Context, temperatureRaw: Int, humidityRaw: Int, illuminanceLux: Long,
        txSequence: Long, sequence: Long,
    ): ByteArray = start(MessageType.M_CIS_ENVIRONMENT, c)
        .u32(20, SOURCE_BOOT).u32(24, txSequence)
        .i16(28, temperatureRaw).u32(30, sequence).u16(34, 0).u8(36, VALID).u8(37, REASON_NONE)
        .u16(38, humidityRaw).u32(40, sequence).u16(44, 0).u8(46, VALID).u8(47, REASON_NONE)
        .u32(48, illuminanceLux).u32(52, sequence).u16(56, 0).u8(58, VALID).u8(59, REASON_NONE)
        .toByteArray()

    fun occupant(c: Context, present: Boolean, count: Int, txSequence: Long, sequence: Long): ByteArray =
        start(MessageType.M_CIS_OCCUPANT, c)
            .u32(20, SOURCE_BOOT).u32(24, txSequence).u32(28, SOURCE_BOOT).u32(32, sequence).u16(36, 0)
            .u8(38, if (present) 1 else 0).u8(39, VALID).u8(40, REASON_NONE)
            .u8(41, count).u8(42, VALID).u8(43, REASON_NONE).u8(44, 2)
            .toByteArray()

    fun rearNoObject(c: Context, txSequence: Long, sequence: Long): ByteArray =
        start(MessageType.M_CIS_REAR, c)
            .u32(20, SOURCE_BOOT).u32(24, txSequence).u32(28, sequence).u16(32, 0)
            .u16(34, 65535).u8(36, VALID).u8(37, REASON_NONE).u8(38, 1)
            .toByteArray()

    fun cisStatus(c: Context, txSequence: Long, sequence: Long): ByteArray =
        start(MessageType.M_CIS_STATUS, c)
            .u32(20, SOURCE_BOOT).u32(24, txSequence)
            .u8(28, 1).u8(29, 2).u8(30, 2).u8(31, 2).u8(32, 2).u8(33, 2).u8(34, 0).u8(35, 0)
            .u16(36, 0).u16(38, 0).u8(40, 0).u16(41, 0).u32(43, 0).u8(47, 0)
            .u32(48, sequence).u16(52, 0)
            .toByteArray()

    fun vssStatus(c: Context, txSequence: Long, sequence: Long): ByteArray =
        start(MessageType.M_VSS_STATUS, c)
            .u32(20, SOURCE_BOOT).u32(24, txSequence)
            .u8(28, 1).u8(29, 0).u8(30, 1).u8(31, 0).u8(32, 0)
            .u16(33, 0).u32(35, 0).u16(39, 0).u8(41, QUALITY_NO_DATA)
            .u32(42, sequence).u16(46, 0)
            .toByteArray()

    fun queryEnd(c: Context, scope: Int, status: Int, itemCount: Int): ByteArray =
        start(MessageType.M_QUERY_END, c).u8(20, scope).u8(21, status).u16(22, itemCount).toByteArray()

    private const val QUALITY_OK = 0
    private const val QUALITY_NO_DATA = 3
    private const val REASON_NONE = 0
    private const val VALID = 1
    private const val AVAILABLE = 0
    private const val LIGHT_NORMAL = 0
    private const val ORIGIN_MOBILE_MANUAL = 1
}
