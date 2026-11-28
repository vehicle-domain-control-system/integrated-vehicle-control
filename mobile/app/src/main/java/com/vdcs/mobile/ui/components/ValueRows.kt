package com.vdcs.mobile.ui.components

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchColors
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import com.vdcs.mobile.ui.rules.ValueText
import com.vdcs.mobile.ui.theme.Spacing

@Composable
private fun valueColor(value: ValueText, normal: Color): Color = when {
    !value.shown -> MaterialTheme.colorScheme.outline
    value.alert -> MaterialTheme.colorScheme.error
    else -> normal
}

@Composable
private fun noteColor(value: ValueText): Color =
    if (value.shown) MaterialTheme.colorScheme.tertiary else MaterialTheme.colorScheme.outline

private fun ValueText.withNote(): String = text + (note?.let { " · $it" } ?: "")

@Composable
fun ValueRow(label: String, value: ValueText, modifier: Modifier = Modifier) {
    Row(modifier.fillMaxWidth(), verticalAlignment = Alignment.Top) {
        Text(label, Modifier.weight(1f), style = MaterialTheme.typography.bodyMedium)
        Column(Modifier.weight(VALUE_WEIGHT), horizontalAlignment = Alignment.End) {
            Text(
                value.text,
                style = MaterialTheme.typography.bodyLarge,
                color = valueColor(value, MaterialTheme.colorScheme.onSurface),
                textAlign = TextAlign.End,
            )
            value.note?.let {
                Text(it, style = MaterialTheme.typography.labelSmall, color = noteColor(value), textAlign = TextAlign.End)
            }
        }
    }
}

@Composable
fun ValueLine(value: ValueText, modifier: Modifier = Modifier) {
    Text(
        value.withNote(),
        modifier = modifier,
        style = MaterialTheme.typography.bodySmall,
        color = valueColor(value, MaterialTheme.colorScheme.onSurfaceVariant),
    )
}

@Composable
fun ValueTile(label: String, value: ValueText, modifier: Modifier = Modifier) {
    Surface(modifier, shape = MaterialTheme.shapes.medium, color = MaterialTheme.colorScheme.surfaceContainer) {
        Column(Modifier.padding(Spacing.l), verticalArrangement = Arrangement.spacedBy(Spacing.xs)) {
            Text(label, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            Text(
                value.text,
                style = MaterialTheme.typography.titleSmall,
                fontWeight = FontWeight.SemiBold,
                color = valueColor(value, MaterialTheme.colorScheme.onSurface),
            )
            value.note?.let { Text(it, style = MaterialTheme.typography.labelSmall, color = noteColor(value)) }
        }
    }
}

@Composable
fun TextRow(label: String, text: String, modifier: Modifier = Modifier) =
    ValueRow(label, ValueText(text, null, shown = true), modifier)

@Composable
fun ToggleRow(
    label: String,
    value: ValueText,
    reported: Boolean?,
    blockReason: String?,
    modifier: Modifier = Modifier,
    colors: SwitchColors = SwitchDefaults.colors(),
    onChange: (Boolean) -> Unit,
) {
    Row(modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f)) {
            Text(label, style = MaterialTheme.typography.bodyMedium)
            Text(
                value.withNote(),
                style = MaterialTheme.typography.labelSmall,
                color = valueColor(value, MaterialTheme.colorScheme.onSurfaceVariant),
            )
        }
        Switch(checked = reported == true, onCheckedChange = onChange, enabled = blockReason == null, colors = colors)
    }
}

@Composable
fun BlockNote(reason: String?, modifier: Modifier = Modifier) {
    if (reason == null) return
    Text(reason, modifier, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.error)
}

@Composable
fun HintText(text: String, modifier: Modifier = Modifier) {
    Text(text, modifier, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
}

private const val VALUE_WEIGHT = 1.4f
