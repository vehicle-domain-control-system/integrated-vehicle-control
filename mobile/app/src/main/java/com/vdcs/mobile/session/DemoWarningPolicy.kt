package com.vdcs.mobile.session

import com.vdcs.mobile.domain.model.Severity
import com.vdcs.mobile.domain.model.WarningType

object DemoWarningPolicy {
    fun choices(type: WarningType): List<Severity> = when (type) {
        WarningType.REAR -> listOf(Severity.CAUTION, Severity.EMERGENCY)
        WarningType.PINCH, WarningType.OCCUPANT_REMAINING -> listOf(Severity.EMERGENCY)
        WarningType.BCM_FAULT, WarningType.CIS_FAULT, WarningType.WINDOW_FAULT, WarningType.VSS_FAULT,
        WarningType.DOOR_OPEN_AFTER_EXIT -> listOf(Severity.CAUTION)
    }
}
