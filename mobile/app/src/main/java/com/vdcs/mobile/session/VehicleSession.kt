package com.vdcs.mobile.session

import com.vdcs.mobile.domain.model.Severity
import com.vdcs.mobile.domain.model.WarningType
import com.vdcs.mobile.domain.repository.ConnectionController
import com.vdcs.mobile.domain.repository.VehicleRepository
import kotlinx.coroutines.flow.StateFlow

enum class AppMode { REAL, DEMO }

class VehicleSession(
    val mode: AppMode,
    val repository: VehicleRepository,
    val connection: ConnectionController,
    val demo: DemoControls?,
)

interface SessionSource {
    val session: StateFlow<VehicleSession>

    fun switchMode(mode: AppMode)
}

interface DemoControls {
    suspend fun openDoor()
    suspend fun closeDoor()
    suspend fun raiseWarning(type: WarningType, severity: Severity)
    suspend fun clearWarning(type: WarningType)
    suspend fun clearAllWarnings()
    suspend fun setCabinTemperature(celsius: Double)
}
