package com.vdcs.mobile.ui.screen

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import com.vdcs.mobile.R
import com.vdcs.mobile.domain.model.Severity
import com.vdcs.mobile.domain.model.Warning
import com.vdcs.mobile.ui.components.BlockNote
import com.vdcs.mobile.ui.components.HintText
import com.vdcs.mobile.ui.components.SectionHeading
import com.vdcs.mobile.ui.components.StatusChipView
import com.vdcs.mobile.ui.components.toneTextColor
import com.vdcs.mobile.ui.rules.AckTargets
import com.vdcs.mobile.ui.rules.StatusChip
import com.vdcs.mobile.ui.rules.ValueFormat
import com.vdcs.mobile.ui.rules.WarningDisplay
import com.vdcs.mobile.ui.rules.WarningRules
import com.vdcs.mobile.ui.rules.label
import com.vdcs.mobile.ui.theme.Spacing

@Composable
fun ActiveWarningBanner(warnings: List<Warning>, origin: String?, modifier: Modifier = Modifier) {
    val active = WarningRules.bannerWarnings(warnings)
    if (active.isEmpty()) return
    val colors = warningColors(active = true, emergency = active.any { it.severity == Severity.EMERGENCY })
    Surface(modifier.fillMaxWidth(), shape = MaterialTheme.shapes.large, color = colors.container, contentColor = colors.content) {
        Column(Modifier.padding(horizontal = Spacing.xl, vertical = Spacing.l), verticalArrangement = Arrangement.spacedBy(Spacing.s)) {
            active.forEach { StatusChipView(StatusChip(WarningRules.bannerText(it, origin), WarningRules.activeTone(it))) }
        }
    }
}

@Composable
fun WarningsSummary(
    title: String,
    summary: String?,
    targets: AckTargets,
    ackAllBlockReason: String?,
    onAckAll: (AckTargets) -> Unit,
    modifier: Modifier = Modifier,
) {
    Row(modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(Spacing.xxs)) {
            Text(title, style = MaterialTheme.typography.titleSmall)
            summary?.let { HintText(it) }
        }
        if (targets.size > 1) {
            Column(horizontalAlignment = Alignment.End) {
                OutlinedButton(onClick = { onAckAll(targets) }, enabled = ackAllBlockReason == null) {
                    Text(stringResource(R.string.warning_ack_all))
                }
                BlockNote(ackAllBlockReason)
            }
        }
    }
}

@Composable
fun OpenWarningsSection(open: List<Warning>, origin: String?, nowMs: Long, ackBlockReason: String?, onAck: (Warning) -> Unit, modifier: Modifier = Modifier) {
    Column(modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(Spacing.m)) {
        SectionHeading(stringResource(R.string.alerts_open_title))
        open.forEach { OpenWarningCard(it, origin, nowMs, ackBlockReason) { onAck(it) } }
    }
}

@Composable
private fun OpenWarningCard(w: Warning, origin: String?, nowMs: Long, ackBlockReason: String?, onAck: () -> Unit) {
    val colors = warningColors(active = WarningRules.display(w) == WarningDisplay.ACTIVE, emergency = w.severity == Severity.EMERGENCY)
    Surface(Modifier.fillMaxWidth(), shape = MaterialTheme.shapes.large, color = colors.container, contentColor = colors.content) {
        Column(Modifier.padding(Spacing.xl), verticalArrangement = Arrangement.spacedBy(Spacing.m)) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(Spacing.m)) {
                Text(w.type.label, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
                Text(w.severity.label, Modifier.weight(1f), style = MaterialTheme.typography.labelLarge, color = toneTextColor(WarningRules.activeTone(w)))
                Text(ValueFormat.receivedText(w.receivedAtMs, nowMs), style = MaterialTheme.typography.labelMedium)
            }
            Text(WarningRules.cardDetailText(w, origin), style = MaterialTheme.typography.bodyMedium)
            if (WarningRules.isAcknowledgeable(w)) {
                Button(onClick = onAck, enabled = ackBlockReason == null) { Text(stringResource(R.string.warning_ack)) }
                BlockNote(ackBlockReason)
            }
        }
    }
}

private class WarningColors(val container: Color, val content: Color)

@Composable
private fun warningColors(active: Boolean, emergency: Boolean): WarningColors {
    val c = MaterialTheme.colorScheme
    return when {
        active && emergency -> WarningColors(c.errorContainer, c.onErrorContainer)
        active -> WarningColors(c.tertiaryContainer, c.onTertiaryContainer)
        else -> WarningColors(c.surfaceVariant, c.onSurfaceVariant)
    }
}
