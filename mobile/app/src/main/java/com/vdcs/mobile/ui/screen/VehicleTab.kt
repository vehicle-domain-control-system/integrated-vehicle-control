package com.vdcs.mobile.ui.screen

import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.lazy.items
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import com.vdcs.mobile.R
import com.vdcs.mobile.domain.model.CabinEnvironment
import com.vdcs.mobile.domain.model.DoorState
import com.vdcs.mobile.domain.model.OccupantState
import com.vdcs.mobile.domain.model.RearState
import com.vdcs.mobile.domain.model.TrackedRequest
import com.vdcs.mobile.domain.model.WindowFault
import com.vdcs.mobile.domain.model.WindowState
import com.vdcs.mobile.session.DemoControls
import com.vdcs.mobile.ui.VehicleUiState
import com.vdcs.mobile.ui.components.HintText
import com.vdcs.mobile.ui.components.ListSection
import com.vdcs.mobile.ui.components.ValueLine
import com.vdcs.mobile.ui.components.ValueRow
import com.vdcs.mobile.ui.rules.TabRules
import com.vdcs.mobile.ui.rules.ValueFormat
import com.vdcs.mobile.ui.rules.ValueText
import com.vdcs.mobile.ui.rules.VehicleSection
import com.vdcs.mobile.ui.rules.VehicleValueRules
import com.vdcs.mobile.ui.rules.label

@Composable
fun VehicleTab(
    state: VehicleUiState,
    nowMs: () -> Long,
    contentPadding: PaddingValues,
    onResend: (TrackedRequest) -> Unit,
    onDemo: (suspend DemoControls.() -> Unit) -> Unit,
    modifier: Modifier = Modifier,
) {
    var demoInput by rememberSaveable(stateSaver = DemoInput.Saver) { mutableStateOf(DemoInput()) }
    val demoPanel: @Composable () -> Unit = { DemoPanel(demoInput, { demoInput = it }, onDemo) }
    TabColumn(contentPadding, modifier) {
        items(TabRules.vehicleSections(state.hasDemoControls), key = { it.name }) { section ->
            VehicleItem(section, state, nowMs(), onResend, demoPanel)
        }
    }
}

@Composable
private fun VehicleItem(
    section: VehicleSection,
    state: VehicleUiState,
    nowMs: Long,
    onResend: (TrackedRequest) -> Unit,
    demoPanel: @Composable () -> Unit,
) {
    val s = state.snapshot
    when (section) {
        VehicleSection.DOOR -> DoorSection(s.door, nowMs)
        VehicleSection.CABIN -> CabinSection(s.environment, s.occupant, s.rear, nowMs)
        VehicleSection.WINDOW -> WindowSection(s.window, s.windowFault, nowMs)
        VehicleSection.AVAILABILITY -> AvailabilitySection(s.availability, nowMs)
        VehicleSection.ECU -> EcuSection(s.ecus, nowMs)
        VehicleSection.REQUESTS -> RequestsSection(state.requests, nowMs, state::resendBlockReason, onResend)
        VehicleSection.DEMO_CONTROLS -> demoPanel()
    }
}

@Composable
fun DoorSection(door: DoorState?, nowMs: Long, modifier: Modifier = Modifier) {
    ListSection(stringResource(R.string.door_title), modifier) {
        ValueRow(stringResource(R.string.door_lock), ValueFormat.value(door?.lock, nowMs) { it.label })
        ValueRow(stringResource(R.string.door_open), ValueFormat.value(door?.open, nowMs) { it.label })
        ValueRow(stringResource(R.string.door_integrity), VehicleValueRules.integrityText(door, nowMs))
    }
}

@Composable
fun WindowSection(window: WindowState?, fault: WindowFault?, nowMs: Long, modifier: Modifier = Modifier) {
    ListSection(stringResource(R.string.window_title), modifier) {
        when {
            VehicleValueRules.windowNothingReceived(window, fault) ->
                ValueLine(ValueText(VehicleValueRules.WINDOW_NOT_RECEIVED_TEXT, null, shown = false))
            else -> {
                WindowStateRows(window, nowMs)
                WindowFaultRows(fault, nowMs)
            }
        }
    }
}

@Composable
private fun WindowStateRows(window: WindowState?, nowMs: Long) {
    if (window == null) {
        ValueRow(stringResource(R.string.window_state), VehicleValueRules.NOT_RECEIVED_VALUE)
        return
    }
    ValueRow(stringResource(R.string.window_motion), ValueFormat.value(window.motion, nowMs) { it.label })
    ValueRow(stringResource(R.string.window_ecu), ValueFormat.value(window.ecuState, nowMs) { it.label })
    ValueRow(stringResource(R.string.window_closed_percent), ValueFormat.value(window.positionClosedPercent, nowMs, ValueFormat::percent))
    ValueRow(stringResource(R.string.window_fully_open), ValueFormat.value(window.fullyOpen, nowMs, ValueFormat::yesNo))
    ValueRow(stringResource(R.string.window_fully_closed), ValueFormat.value(window.fullyClosed, nowMs, ValueFormat::yesNo))
    ValueRow(stringResource(R.string.window_anti_pinch), ValueFormat.value(window.antiPinch, nowMs) { it.label })
    ValueRow(stringResource(R.string.window_reversing), ValueFormat.value(window.reversing, nowMs, ValueFormat::yesNo))
}

@Composable
private fun WindowFaultRows(fault: WindowFault?, nowMs: Long) {
    if (fault == null) {
        ValueRow(stringResource(R.string.window_fault), VehicleValueRules.NOT_RECEIVED_VALUE)
        return
    }
    ValueRow(stringResource(R.string.window_fault_status), ValueFormat.value(fault.status, nowMs) { it.label })
    ValueRow(stringResource(R.string.window_fault_category), ValueFormat.value(fault.category, nowMs) { it.label })
    ValueRow(stringResource(R.string.window_fault_code), ValueFormat.value(fault.code, nowMs) { it.toString() })
    VehicleValueRules.windowFaultActionText(fault)?.let { HintText(it) }
}

@Composable
fun CabinSection(
    environment: CabinEnvironment?,
    occupant: OccupantState?,
    rear: RearState?,
    nowMs: Long,
    modifier: Modifier = Modifier,
) {
    ListSection(stringResource(R.string.cabin_title), modifier) {
        ValueRow(stringResource(R.string.cabin_temperature), ValueFormat.value(environment?.temperatureC, nowMs, ValueFormat::celsius))
        ValueRow(stringResource(R.string.cabin_humidity), ValueFormat.value(environment?.humidityPercent, nowMs, ValueFormat::humidity))
        ValueRow(stringResource(R.string.cabin_illuminance), ValueFormat.value(environment?.illuminanceLux, nowMs, ValueFormat::lux))
        ValueRow(stringResource(R.string.cabin_occupant), ValueFormat.value(occupant?.present, nowMs, ValueFormat::presence))
        ValueRow(stringResource(R.string.cabin_occupant_count), ValueFormat.value(occupant?.count, nowMs, ValueFormat::people))
        ValueRow(VehicleValueRules.REAR_DISTANCE_NAME, VehicleValueRules.rearText(rear, nowMs))
    }
}
