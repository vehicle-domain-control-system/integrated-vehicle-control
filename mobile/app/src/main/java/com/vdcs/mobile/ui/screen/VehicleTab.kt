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
