@file:OptIn(ExperimentalLayoutApi::class)

package com.vdcs.mobile.ui.screen

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.material3.Button
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.saveable.listSaver
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import com.vdcs.mobile.R
import com.vdcs.mobile.domain.model.Severity
import com.vdcs.mobile.domain.model.WarningType
import com.vdcs.mobile.session.DemoControls
import com.vdcs.mobile.session.DemoWarningPolicy
import com.vdcs.mobile.ui.components.HintText
import com.vdcs.mobile.ui.components.ListSection
import com.vdcs.mobile.ui.rules.ValueFormat
import com.vdcs.mobile.ui.rules.label
import com.vdcs.mobile.ui.theme.Spacing

data class DemoInput(
    val type: WarningType = WarningType.DOOR_OPEN_AFTER_EXIT,
    val severity: Severity = DemoWarningPolicy.choices(WarningType.DOOR_OPEN_AFTER_EXIT).first(),
    val cabinC: Double = DEFAULT_CABIN_C,
) {
    companion object {
        val Saver = listSaver<DemoInput, Any>(
            save = { listOf(it.type, it.severity, it.cabinC) },
            restore = { DemoInput(it[0] as WarningType, it[1] as Severity, it[2] as Double) },
        )
    }
}

@Composable
fun DemoPanel(
    input: DemoInput,
    onInputChange: (DemoInput) -> Unit,
    onDemo: (suspend DemoControls.() -> Unit) -> Unit,
    modifier: Modifier = Modifier,
) {
    ListSection(stringResource(R.string.demo_title), modifier) {
        Text(stringResource(R.string.demo_door), style = MaterialTheme.typography.bodyMedium)
        Row(horizontalArrangement = Arrangement.spacedBy(Spacing.m)) {
            OutlinedButton(onClick = { onDemo { openDoor() } }) { Text(stringResource(R.string.demo_door_open)) }
            OutlinedButton(onClick = { onDemo { closeDoor() } }) { Text(stringResource(R.string.demo_door_close)) }
        }
        DemoWarningControls(input, onInputChange, onDemo)
        DemoCabinControls(input, onInputChange, onDemo)
    }
}

@Composable
private fun DemoWarningControls(input: DemoInput, onInputChange: (DemoInput) -> Unit, onDemo: (suspend DemoControls.() -> Unit) -> Unit) {
    Text(stringResource(R.string.demo_warning_type), style = MaterialTheme.typography.bodyMedium)
    FlowRow(horizontalArrangement = Arrangement.spacedBy(Spacing.m)) {
        WarningType.entries.forEach { t ->
            FilterChip(
                selected = input.type == t,
                onClick = { onInputChange(input.copy(type = t, severity = DemoWarningPolicy.choices(t).first())) },
                label = { Text(stringResource(R.string.demo_warning_type_option, t.raw, t.label)) },
            )
        }
    }
    DemoSeverityChoice(input, onInputChange)
    Row(horizontalArrangement = Arrangement.spacedBy(Spacing.m)) {
        Button(onClick = { onDemo { raiseWarning(input.type, input.severity) } }) { Text(stringResource(R.string.demo_warning_raise)) }
        OutlinedButton(onClick = { onDemo { clearWarning(input.type) } }) { Text(stringResource(R.string.demo_warning_clear)) }
    }
    OutlinedButton(onClick = { onDemo { clearAllWarnings() } }) { Text(stringResource(R.string.demo_warning_clear_all)) }
}

@Composable
private fun DemoSeverityChoice(input: DemoInput, onInputChange: (DemoInput) -> Unit) {
    val choices = DemoWarningPolicy.choices(input.type)
    Text(stringResource(R.string.demo_severity), style = MaterialTheme.typography.bodyMedium)
    if (choices.size == 1) {
        HintText(choices.first().label)
        return
    }
    Row(horizontalArrangement = Arrangement.spacedBy(Spacing.m)) {
        choices.forEach { s ->
            FilterChip(selected = input.severity == s, onClick = { onInputChange(input.copy(severity = s)) }, label = { Text(s.label) })
        }
    }
}

@Composable
private fun DemoCabinControls(input: DemoInput, onInputChange: (DemoInput) -> Unit, onDemo: (suspend DemoControls.() -> Unit) -> Unit) {
    val step = { steps: Int -> onInputChange(input.copy(cabinC = ValueFormat.stepTarget(input.cabinC, steps))) }
    Text(stringResource(R.string.demo_cabin_temperature), style = MaterialTheme.typography.bodyMedium)
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(Spacing.m)) {
        OutlinedButton(onClick = { step(-1) }) { Text(stringResource(R.string.demo_step_down)) }
        Text(ValueFormat.celsius(input.cabinC), style = MaterialTheme.typography.titleMedium)
        OutlinedButton(onClick = { step(+1) }) { Text(stringResource(R.string.demo_step_up)) }
        Button(onClick = { onDemo { setCabinTemperature(input.cabinC) } }) { Text(stringResource(R.string.demo_apply)) }
    }
}

private const val DEFAULT_CABIN_C = 24.0
