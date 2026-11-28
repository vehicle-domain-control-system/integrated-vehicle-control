package com.vdcs.mobile.domain.repository

import com.vdcs.mobile.domain.model.TrackedRequest
import com.vdcs.mobile.domain.model.UserRequest
import com.vdcs.mobile.domain.model.VehicleSnapshot
import com.vdcs.mobile.domain.model.Warning
import com.vdcs.mobile.domain.model.WarningRecord
import kotlinx.coroutines.flow.Flow

interface VehicleRepository {
    val vehicleState: Flow<VehicleSnapshot>

    val requests: Flow<List<TrackedRequest>>

    val warningHistory: Flow<List<WarningRecord>>

    val notices: Flow<String>

    suspend fun send(request: UserRequest): Long

    suspend fun resend(request: TrackedRequest)

    suspend fun acknowledgeWarning(warning: Warning)

    suspend fun confirmArchivedWarnings(records: List<WarningRecord>)

    suspend fun query(scope: QueryScope)
}

enum class QueryScope(val raw: Int) {
    CURRENT_STATE(0),
    REQUEST_RESULT(1),
    WARNINGS(2),
    ALL(3),
}
