package com.vdcs.mobile.data.protocol

import com.vdcs.mobile.data.ble.GattSpec

data class GatewayStatus(
    val registrationState: Int,
    val domainLinkUp: Boolean,
    val sessionReady: Boolean,
    val deviceContextId: Long,
    val sessionId: Long,
    val domainBootId: Long,
    val domainState: Int,
) {
    val registered: Boolean get() = registrationState == REGISTERED

    companion object {
        const val LENGTH = 21
        const val NOT_REGISTERED = 0
        const val REGISTERED = 1
    }
}

object GattPayloads {
    fun decodeGatewayStatus(raw: ByteArray): GatewayStatus? {
        if (raw.size != GatewayStatus.LENGTH) return null
        val r = ByteReader(raw)
        if (r.u8(0) != GattSpec.PROTOCOL_VERSION) return null
        return GatewayStatus(
            registrationState = r.u8(1),
            domainLinkUp = r.u8(2) == 1,
            sessionReady = r.u8(3) == 1,
            deviceContextId = r.u32(4),
            sessionId = r.u64(8),
            domainBootId = r.u32(16),
            domainState = r.u8(20),
        )
    }

    fun encodeAppState(active: Boolean, appInstanceId: Long): ByteArray {
        require(appInstanceId in 1..0xFFFF_FFFFL) { "APP_INSTANCE_ID 는 0 이 아닌 u32" }
        return ByteWriter(5).u8(0, if (active) 1 else 0).u32(1, appInstanceId).toByteArray()
    }
}
