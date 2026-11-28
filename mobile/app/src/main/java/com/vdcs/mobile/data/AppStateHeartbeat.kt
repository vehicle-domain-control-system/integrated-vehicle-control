package com.vdcs.mobile.data

import com.vdcs.mobile.data.ble.BleTransport
import com.vdcs.mobile.data.ble.WriteResult
import com.vdcs.mobile.data.protocol.GattPayloads
import com.vdcs.mobile.domain.logic.SessionContext

internal class AppStateHeartbeat(
    private val transport: BleTransport,
    private val session: SessionContext,
    private val heartbeatMs: Long,
    private val clock: () -> Long,
) {
    private val foreground: Boolean get() = transport.foreground

    private var lastActiveAt = NEVER
    private var lastAttemptAt = NEVER

    fun activeNow() {
        lastActiveAt = NEVER
    }

    fun due(now: Long, retrying: Boolean): Boolean? = when {
        retrying -> foreground.takeIf { now - lastAttemptAt >= heartbeatMs }
        foreground && (lastActiveAt == NEVER || now - lastActiveAt >= heartbeatMs) -> true
        else -> null
    }

    suspend fun write(active: Boolean): Boolean {
        val now = clock()
        if (active) lastActiveAt = now
        lastAttemptAt = now
        return transport.writeAppState(GattPayloads.encodeAppState(active, session.appInstanceId)) == WriteResult.Ok
    }

    private companion object {
        const val NEVER = Long.MIN_VALUE
    }
}
