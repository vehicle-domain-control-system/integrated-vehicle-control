package com.vdcs.mobile.ui.screen

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import com.vdcs.mobile.R
import com.vdcs.mobile.domain.model.DigitalKeyState
import com.vdcs.mobile.domain.model.LightState
import com.vdcs.mobile.domain.model.UserRequest
import com.vdcs.mobile.domain.model.UserSettingsState
import com.vdcs.mobile.ui.components.BlockNote
import com.vdcs.mobile.ui.components.HintText
import com.vdcs.mobile.ui.components.PanelCard
import com.vdcs.mobile.ui.components.ToggleRow
import com.vdcs.mobile.ui.components.ValueLine
import com.vdcs.mobile.ui.components.ValueRow
import com.vdcs.mobile.ui.rules.LightRules
import com.vdcs.mobile.ui.rules.ValueFormat
import com.vdcs.mobile.ui.rules.VehicleValueRules
import com.vdcs.mobile.ui.rules.displayableValue
import com.vdcs.mobile.ui.rules.label
import com.vdcs.mobile.ui.theme.Spacing
import com.vdcs.mobile.ui.theme.interiorLight
import com.vdcs.mobile.ui.theme.onInteriorLight
import kotlin.math.roundToInt

@Composable
fun LightKeyTab(
    settings: UserSettingsState?,
    light: LightState?,
    digitalKey: DigitalKeyState?,
    nowMs: () -> Long,
    lightBlockReason: String?,
    keyBlockReason: String?,
    contentPadding: PaddingValues,
    onSend: (UserRequest) -> Unit,
    modifier: Modifier = Modifier,
) {
    val level = rememberDraft(settings?.lightLevelPercent.displayableValue()?.toFloat(), 0f)
    TabColumn(contentPadding, modifier) {
        item(key = "lightEnabled") { LightEnabledRow(settings, nowMs(), lightBlockReason, onSend) }
        item(key = "glow") {
            val feedback = ValueFormat.labeled(stringResource(R.string.light_physical_feedback), VehicleValueRules.physicalFeedbackText(light, nowMs()))
            LightGlowView(LightRules.glow(light), LightRules.glowDescription(light, nowMs()), feedback)
        }
        item(key = "light") {
            LightSettingsSection(
                settings, nowMs(), lightBlockReason, onSend,
                requestedLevel = level.value,
                onLevelChange = { level.edit(it) },
                onLevelCommit = { onSend(UserRequest.LightBrightness(level.commit().roundToInt().coerceIn(0, 100))) },
            )
        }
        item(key = "lightApplied") { ValueLine(LightRules.appliedText(light, nowMs())) }
        item(key = "digitalKey") { DigitalKeySection(digitalKey, nowMs(), keyBlockReason, onSend) }
    }
}

@Composable
fun LightEnabledRow(settings: UserSettingsState?, nowMs: Long, blockReason: String?, onSend: (UserRequest) -> Unit, modifier: Modifier = Modifier) {
    val c = MaterialTheme.colorScheme
    ToggleRow(
        stringResource(R.string.light_enabled),
        ValueFormat.value(settings?.lightEnabled, nowMs, ValueFormat::onOff),
        settings?.lightEnabled.displayableValue(),
        blockReason,
        modifier,
        colors = SwitchDefaults.colors(checkedTrackColor = c.interiorLight, checkedThumbColor = c.onInteriorLight),
    ) { onSend(UserRequest.LightEnabled(it)) }
}

@Composable
fun LightSettingsSection(
    settings: UserSettingsState?,
    nowMs: Long,
    blockReason: String?,
    onSend: (UserRequest) -> Unit,
    requestedLevel: Float,
    onLevelChange: (Float) -> Unit,
    onLevelCommit: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val enabled = blockReason == null
    PanelCard(modifier, verticalArrangement = Arrangement.spacedBy(Spacing.l)) {
        Column(verticalArrangement = Arrangement.spacedBy(Spacing.xs)) {
            NameValueLine(stringResource(R.string.light_level), ValueFormat.percent(requestedLevel.roundToInt()))
            Slider(
                requestedLevel, onLevelChange, enabled = enabled, valueRange = 0f..100f, onValueChangeFinished = onLevelCommit,
                colors = SliderDefaults.colors(activeTrackColor = MaterialTheme.colorScheme.interiorLight),
            )
            ValueLine(ValueFormat.labeled(stringResource(R.string.value_reported), ValueFormat.value(settings?.lightLevelPercent, nowMs, ValueFormat::percent)))
        }
        Column(verticalArrangement = Arrangement.spacedBy(Spacing.m)) {
            NameValueLine(stringResource(R.string.light_color), VehicleValueRules.LIGHT_COLOR_NOTE)
            LightColorDots(settings?.lightRgb.displayableValue(), enabled, onPick = { onSend(UserRequest.LightColor(it)) })
            ValueLine(ValueFormat.labeled(stringResource(R.string.value_reported), ValueFormat.value(settings?.lightRgb, nowMs) { it.label }))
        }
        BlockNote(blockReason)
    }
}

@Composable
private fun NameValueLine(name: String, value: String) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Text(name, Modifier.weight(1f), style = MaterialTheme.typography.titleSmall)
        HintText(value)
    }
}

@Composable
fun DigitalKeySection(
    key: DigitalKeyState?,
    nowMs: Long,
    blockReason: String?,
    onSend: (UserRequest) -> Unit,
    modifier: Modifier = Modifier,
) {
    PanelCard(modifier) {
        ToggleRow(
            stringResource(R.string.key_proximity_unlock),
            ValueFormat.value(key?.proximityUnlockEnabled, nowMs, ValueFormat::onOff),
            key?.proximityUnlockEnabled.displayableValue(),
            blockReason,
        ) { onSend(UserRequest.ProximityUnlock(it)) }
        ValueRow(stringResource(R.string.key_last_auto_unlock), VehicleValueRules.lastAutoUnlockText(key, nowMs))
        BlockNote(blockReason)
    }
}
