package com.vdcs.mobile.ui.screen

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.size
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import com.vdcs.mobile.R
import com.vdcs.mobile.domain.model.CabinEnvironment
import com.vdcs.mobile.domain.model.ClimateState
import com.vdcs.mobile.domain.model.FanLevel
import com.vdcs.mobile.domain.model.UserRequest
import com.vdcs.mobile.domain.model.UserSettingsState
import com.vdcs.mobile.ui.components.BlockNote
import com.vdcs.mobile.ui.components.HintText
import com.vdcs.mobile.ui.components.PanelCard
import com.vdcs.mobile.ui.components.RoundButton
import com.vdcs.mobile.ui.components.SegmentedControl
import com.vdcs.mobile.ui.components.ToggleRow
import com.vdcs.mobile.ui.components.ValueLine
import com.vdcs.mobile.ui.components.ValueTile
import com.vdcs.mobile.ui.rules.ClimateRules
import com.vdcs.mobile.ui.rules.HomeRules
import com.vdcs.mobile.ui.rules.ValueFormat
import com.vdcs.mobile.ui.rules.ValueText
import com.vdcs.mobile.ui.rules.VehicleValueRules
import com.vdcs.mobile.ui.rules.displayableValue
import com.vdcs.mobile.ui.rules.label
import com.vdcs.mobile.ui.theme.Sizes
import com.vdcs.mobile.ui.theme.Spacing

@Composable
fun ClimateTab(
    settings: UserSettingsState?,
    climate: ClimateState?,
    environment: CabinEnvironment?,
    nowMs: () -> Long,
    blockReason: String?,
    contentPadding: PaddingValues,
    onSend: (UserRequest) -> Unit,
    modifier: Modifier = Modifier,
) {
    val target = rememberDraft(settings?.targetTemperatureC.displayableValue(), DEFAULT_TARGET_C)
    TabColumn(contentPadding, modifier) {
        item(key = "cabin") { HintText(HomeRules.cabinTemperatureText(environment, nowMs())) }
        item(key = "target") {
            ClimateTargetSection(
                target.value,
                ValueFormat.labeled(stringResource(R.string.value_reported), ValueFormat.value(settings?.targetTemperatureC, nowMs(), ValueFormat::celsius)),
                blockReason,
                onStep = { target.edit(ClimateRules.stepTarget(target.value, it)) },
                onApply = { onSend(UserRequest.TargetTemperature(target.commit())) },
            )
        }
        item(key = "mode") { ClimateModeSection(settings, nowMs(), blockReason, onSend) }
        item(key = "output") { ClimateOutputSection(climate, nowMs()) }
    }
}

@Composable
fun ClimateTargetSection(
    requested: Double,
    reported: ValueText,
    blockReason: String?,
    onStep: (Int) -> Unit,
    onApply: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val downReason = blockReason ?: ClimateRules.targetStepBlockReason(requested, -1)
    val upReason = blockReason ?: ClimateRules.targetStepBlockReason(requested, +1)
    Column(modifier.fillMaxWidth(), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(Spacing.m)) {
        Box(Modifier.size(Sizes.targetRing), contentAlignment = Alignment.Center) {
            TargetRing(ClimateRules.ringFraction(requested))
            TargetReadout(requested, reported)
        }
        Row(horizontalArrangement = Arrangement.spacedBy(Spacing.xxl), verticalAlignment = Alignment.CenterVertically) {
            StepButton(stringResource(R.string.climate_step_down_symbol), stringResource(R.string.climate_step_down), downReason) { onStep(-1) }
            Button(onClick = onApply, enabled = blockReason == null) { Text(stringResource(R.string.climate_apply)) }
            StepButton(stringResource(R.string.climate_step_up_symbol), stringResource(R.string.climate_step_up), upReason) { onStep(+1) }
        }
        listOfNotNull(downReason, upReason).distinct().forEach { BlockNote(it) }
    }
}

@Composable
private fun TargetRing(fraction: Float) {
    val track = MaterialTheme.colorScheme.surfaceVariant
    val fill = MaterialTheme.colorScheme.primary
    Canvas(Modifier.size(Sizes.targetRing)) {
        val stroke = Stroke(Sizes.targetRingStroke.toPx(), cap = StrokeCap.Round)
        val inset = stroke.width / 2
        val arcSize = size.copy(width = size.width - stroke.width, height = size.height - stroke.width)
        val topLeft = Offset(inset, inset)
        drawArc(track, RING_START_DEG, RING_SWEEP_DEG, useCenter = false, topLeft = topLeft, size = arcSize, style = stroke)
        if (fraction > 0f) {
            drawArc(fill, RING_START_DEG, RING_SWEEP_DEG * fraction, useCenter = false, topLeft = topLeft, size = arcSize, style = stroke)
        }
    }
}

@Composable
private fun TargetReadout(requested: Double, reported: ValueText) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        HintText(stringResource(R.string.climate_target_title))
        Text(ValueFormat.celsius(requested), style = MaterialTheme.typography.displayMedium, fontWeight = FontWeight.SemiBold)
        ValueLine(reported)
    }
}

@Composable
private fun StepButton(symbol: String, description: String, blockReason: String?, onClick: () -> Unit) {
    RoundButton(description, blockReason == null, onClick, size = Sizes.stepButton) {
        Text(symbol, style = MaterialTheme.typography.headlineSmall)
    }
}

@Composable
fun ClimateModeSection(
    settings: UserSettingsState?,
    nowMs: Long,
    blockReason: String?,
    onSend: (UserRequest) -> Unit,
    modifier: Modifier = Modifier,
) {
    PanelCard(modifier, verticalArrangement = Arrangement.spacedBy(Spacing.l)) {
        val auto = settings?.climateAuto
        ToggleRow(stringResource(R.string.climate_auto), ValueFormat.value(auto, nowMs, ValueFormat::onOff), auto.displayableValue(), blockReason) {
            onSend(UserRequest.ClimateAuto(it))
        }
        Text(stringResource(R.string.climate_fan), style = MaterialTheme.typography.titleSmall)
        SegmentedControl(REQUESTABLE_FAN, settings?.fanLevel.displayableValue(), { it.label }, enabled = blockReason == null, onSelect = {
            onSend(UserRequest.Fan(it))
        })
        ValueLine(ValueFormat.labeled(stringResource(R.string.value_reported), ValueFormat.value(settings?.fanLevel, nowMs) { it.label }))
        BlockNote(blockReason)
    }
}

@Composable
fun ClimateOutputSection(climate: ClimateState?, nowMs: Long, modifier: Modifier = Modifier) {
    Column(modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(Spacing.m)) {
        Row(horizontalArrangement = Arrangement.spacedBy(Spacing.m)) {
            val tile = Modifier.weight(1f)
            ValueTile(stringResource(R.string.climate_temp_direction), ValueFormat.value(climate?.tempDirection, nowMs) { it.label }, tile)
            ValueTile(stringResource(R.string.climate_output), ValueFormat.value(climate?.outputPercent, nowMs, ValueFormat::percent), tile)
            ValueTile(stringResource(R.string.climate_heat_removal), VehicleValueRules.heatRemovalText(climate, nowMs), tile)
        }
        Row(horizontalArrangement = Arrangement.spacedBy(Spacing.l)) {
            ValueLine(ValueFormat.labeled(stringResource(R.string.climate_fan_commanded), ValueFormat.value(climate?.fanCommanded, nowMs) { it.label }))
            ValueLine(ValueFormat.labeled(stringResource(R.string.climate_fan_measured), ValueFormat.value(climate?.fanMeasured, nowMs) { it.label }))
        }
    }
}

private val REQUESTABLE_FAN = listOf(FanLevel.OFF, FanLevel.LOW, FanLevel.MEDIUM, FanLevel.HIGH)

private const val DEFAULT_TARGET_C = 22.0

private const val RING_START_DEG = 135f
private const val RING_SWEEP_DEG = 270f
