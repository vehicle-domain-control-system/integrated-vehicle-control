package com.vdcs.mobile.data

import com.vdcs.mobile.data.VehicleMessageMapper.faultCategory
import com.vdcs.mobile.data.VehicleMessageMapper.onOff
import com.vdcs.mobile.data.VehicleMessageMapper.qualified
import com.vdcs.mobile.data.protocol.VehicleMessage
import com.vdcs.mobile.domain.model.AntiPinchStatus
import com.vdcs.mobile.domain.model.Quality
import com.vdcs.mobile.domain.model.WindowEcuState
import com.vdcs.mobile.domain.model.WindowFault
import com.vdcs.mobile.domain.model.WindowFaultStatus
import com.vdcs.mobile.domain.model.WindowMotion
import com.vdcs.mobile.domain.model.WindowState

internal object WindowMapper {
    fun state(m: VehicleMessage.WindowStateMsg, now: Long): WindowState {
        val quality = Quality.fromRaw(m.quality)
        return WindowState(
            motion = qualified(WindowMotion.entries.getOrNull(m.motion), quality, now, m.sourceAgeMs),
            ecuState = qualified(WindowEcuState.entries.getOrNull(m.ecuState), quality, now, m.sourceAgeMs),
            positionClosedPercent = qualified(m.position.takeIf { it in 0..100 }, quality, now, m.sourceAgeMs),
            fullyOpen = qualified(onOff(m.fullyOpen), quality, now, m.sourceAgeMs),
            fullyClosed = qualified(onOff(m.fullyClosed), quality, now, m.sourceAgeMs),
            antiPinch = qualified(AntiPinchStatus.entries.getOrNull(m.antiPinch), quality, now, m.sourceAgeMs),
            reversing = qualified(onOff(m.reverse), quality, now, m.sourceAgeMs),
        )
    }

    fun fault(m: VehicleMessage.WindowFaultMsg, now: Long) = WindowFault(
        category = qualified(faultCategory(m.category), Quality.OK, now, m.sourceAgeMs),
        code = qualified(m.code, Quality.OK, now, m.sourceAgeMs),
        status = qualified(WindowFaultStatus.entries.getOrNull(m.status), Quality.OK, now, m.sourceAgeMs),
        occurrenceId = m.occurrenceId,
    )
}
