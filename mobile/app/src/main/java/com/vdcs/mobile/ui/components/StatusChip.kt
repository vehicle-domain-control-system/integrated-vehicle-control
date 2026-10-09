package com.vdcs.mobile.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import com.vdcs.mobile.ui.rules.ChipTone
import com.vdcs.mobile.ui.rules.StatusChip
import com.vdcs.mobile.ui.theme.Sizes
import com.vdcs.mobile.ui.theme.Spacing
import com.vdcs.mobile.ui.theme.ok

private class ToneStyle(val dot: Color, val text: Color, val hollow: Boolean)

@Composable
private fun toneStyle(tone: ChipTone): ToneStyle {
    val c = MaterialTheme.colorScheme
    return when (tone) {
        ChipTone.NORMAL -> ToneStyle(c.ok, c.onSurface, hollow = false)
        ChipTone.ATTENTION -> ToneStyle(c.tertiary, c.tertiary, hollow = false)
        ChipTone.EMERGENCY -> ToneStyle(c.error, c.error, hollow = false)
        ChipTone.STALE -> ToneStyle(c.tertiary, c.tertiary, hollow = true)
        ChipTone.UNKNOWN -> ToneStyle(c.outline, c.onSurfaceVariant, hollow = true)
    }
}

@Composable
fun StatusDot(color: Color, modifier: Modifier = Modifier, hollow: Boolean = false) {
    val shaped = modifier.size(Sizes.statusDot)
    Box(if (hollow) shaped.border(Sizes.border, color, CircleShape) else shaped.background(color, CircleShape))
}

@Composable
fun ToneDot(tone: ChipTone, modifier: Modifier = Modifier) {
    val style = toneStyle(tone)
    StatusDot(style.dot, modifier, hollow = style.hollow)
}

@Composable
fun toneTextColor(tone: ChipTone): Color = toneStyle(tone).text

@Composable
fun StatusChipView(chip: StatusChip, modifier: Modifier = Modifier) {
    Row(modifier, verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(Spacing.s)) {
        ToneDot(chip.tone)
        Text(chip.text, style = MaterialTheme.typography.labelLarge, color = toneTextColor(chip.tone))
    }
}

@Composable
fun StatusPin(
    tone: ChipTone,
    label: String,
    description: String,
    modifier: Modifier = Modifier,
) {
    Surface(modifier.semantics { contentDescription = description }, shape = pinShape(tone), color = MaterialTheme.colorScheme.surface) {
        PinContent(toneStyle(tone), label)
    }
}

@Composable
private fun pinShape(tone: ChipTone): Shape = if (tone == ChipTone.UNKNOWN) MaterialTheme.shapes.small else CircleShape

@Composable
private fun PinContent(style: ToneStyle, label: String) {
    Row(
        Modifier.padding(Spacing.s).clearAndSetSemantics {},
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(Spacing.xs),
    ) {
        StatusDot(style.dot, hollow = style.hollow)
        Text(label, Modifier.padding(end = Spacing.xxs), style = MaterialTheme.typography.labelMedium, color = style.text)
    }
}
