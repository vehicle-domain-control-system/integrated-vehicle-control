package com.vdcs.mobile.data.protocol

fun hex(s: String): ByteArray =
    s.split(' ', '\n').filter { it.isNotBlank() }.map { it.toInt(16).toByte() }.toByteArray()

fun ByteArray.toHex(): String = joinToString(" ") { "%02X".format(it) }

fun downstream(
    type: Int,
    deviceContextId: Long = 7,
    sessionId: Long = 0x1122334455667788,
    domainBootId: Long = 3,
    queryId: Long = 0,
    body: ByteWriter.() -> Unit = {},
): ByteArray {
    val w = ByteWriter(MessageType.PAYLOAD_LENGTH.getValue(type))
        .u32(0, deviceContextId).u64(4, sessionId).u32(12, domainBootId).u32(16, queryId)
    w.body()
    return w.toByteArray()
}
