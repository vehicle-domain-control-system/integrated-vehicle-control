package com.vdcs.mobile.ui

import com.vdcs.mobile.domain.model.ConnectionState
import com.vdcs.mobile.domain.model.TrackedRequest
import com.vdcs.mobile.domain.model.VehicleFunction
import com.vdcs.mobile.domain.model.VehicleSnapshot
import com.vdcs.mobile.domain.model.WarningRecord
import com.vdcs.mobile.session.AppMode
import com.vdcs.mobile.ui.rules.ControlRules
import com.vdcs.mobile.ui.rules.WarningRules
import com.vdcs.mobile.ui.rules.function

data class VehicleUiState(
    val mode: AppMode = AppMode.REAL,
    val hasDemoControls: Boolean = false,
    val connection: ConnectionState = ConnectionState.DISCONNECTED,
    val registered: Boolean = false,
    val connectionDetail: String? = null,
    val domainBootId: Long? = null,
    val snapshot: VehicleSnapshot = VehicleSnapshot(),
    val requests: List<TrackedRequest> = emptyList(),
    val warningHistory: List<WarningRecord> = emptyList(),
) {
    val warningOrigin: String?
        get() = WarningRules.originLabel(mode)

    val unreadWarningCount: Int
        get() = WarningRules.unreadCount(warningHistory)

    fun controlBlockReason(function: VehicleFunction): String? =
        ControlRules.controlBlockReason(connection, connectionDetail, snapshot.availability[function])

    val ackBlockReason: String?
        get() = ControlRules.ackBlockReason(connection)

    fun resendBlockReason(t: TrackedRequest): String? =
        ControlRules.resendBlockReason(connection, snapshot.availability[t.request.function])
}
