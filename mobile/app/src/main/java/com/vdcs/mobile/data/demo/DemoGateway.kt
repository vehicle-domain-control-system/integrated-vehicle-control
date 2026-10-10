package com.vdcs.mobile.data.demo

import com.vdcs.mobile.data.ble.GattSpec
import com.vdcs.mobile.data.protocol.ByteWriter
import com.vdcs.mobile.data.protocol.GatewayStatus
import kotlin.random.Random

internal class DemoGateway(private val random: Random) {
    val registered = true
    private val deviceContextId = 7L
    val domainBootId = nonZeroU32()

    var sessionId = 0L
        private set
    var sessionReady = false
    var queryBusy = false
    private var appInstanceId = 0L

    fun onAppInstance(instance: Long): ByteArray? {
        if (instance == appInstanceId) return null
        appInstanceId = instance
        sessionId = nonZeroU64()
        sessionReady = false
        queryBusy = false
        return statusBytes()
    }

    fun onLinkDown(forgetApp: Boolean) {
        queryBusy = false
        sessionReady = false
        if (forgetApp) appInstanceId = 0
    }

    fun context(queryId: Long = 0) = DemoDownlink.Context(deviceContextId, sessionId, domainBootId, queryId)

    fun statusBytes(): ByteArray = ByteWriter(GatewayStatus.LENGTH)
        .u8(0, GattSpec.PROTOCOL_VERSION)
        .u8(1, if (registered) GatewayStatus.REGISTERED else GatewayStatus.NOT_REGISTERED)
        .u8(2, 1)
        .u8(3, if (sessionReady) 1 else 0)
        .u32(4, deviceContextId)
        .u64(8, sessionId)
        .u32(16, domainBootId)
        .u8(20, 1)
        .toByteArray()

    private fun nonZeroU32(): Long {
        var v: Long
        do { v = random.nextLong() and 0xFFFF_FFFFL } while (v == 0L)
        return v
    }

    private fun nonZeroU64(): Long {
        var v: Long
        do { v = random.nextLong() } while (v == 0L || v == sessionId)
        return v
    }
}
