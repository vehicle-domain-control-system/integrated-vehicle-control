package com.vdcs.mobile.ui.screen

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.selection.selectableGroup
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.semantics
import com.vdcs.mobile.domain.model.Rgb
import com.vdcs.mobile.ui.components.ValueLine
import com.vdcs.mobile.ui.rules.LIGHT_PRESETS
import com.vdcs.mobile.ui.rules.LightGlow
import com.vdcs.mobile.ui.rules.ValueText
import com.vdcs.mobile.ui.theme.Sizes
import com.vdcs.mobile.ui.theme.Spacing

private fun Rgb.toColor(): Color = Color(r, g, b)

@Composable
fun LightGlowView(glow: LightGlow?, description: String, feedback: ValueText, modifier: Modifier = Modifier) {
    Column(modifier.fillMaxWidth(), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(Spacing.m)) {
        Box(
            Modifier.size(Sizes.lightGlow)
                .background(MaterialTheme.colorScheme.surfaceVariant, CircleShape)
                .semantics { contentDescription = description },
            contentAlignment = Alignment.Center,
        ) {
            if (glow == null) {
                Box(Modifier.size(Sizes.lightGlowCore).background(MaterialTheme.colorScheme.outlineVariant, CircleShape))
            } else {
                LitCore(glow)
            }
        }
        ValueLine(feedback)
    }
}

@Composable
private fun LitCore(glow: LightGlow) {
    val light = glow.rgb.toColor()
    val alpha = MIN_GLOW_ALPHA + (1f - MIN_GLOW_ALPHA) * glow.level
    Box(Modifier.fillMaxSize().background(Brush.radialGradient(listOf(light.copy(alpha = alpha * HALO_ALPHA), Color.Transparent)), CircleShape))
    Box(Modifier.size(Sizes.lightGlowCore).background(light.copy(alpha = alpha), CircleShape))
}

@Composable
fun LightColorDots(chosen: Rgb?, enabled: Boolean, onPick: (Rgb) -> Unit, modifier: Modifier = Modifier) {
    Row(modifier.fillMaxWidth().selectableGroup(), horizontalArrangement = Arrangement.SpaceBetween) {
        LIGHT_PRESETS.forEach { (name, rgb) ->
            ColorDot(name, rgb, selected = chosen == rgb, enabled = enabled) { onPick(rgb) }
        }
    }
}

@Composable
private fun ColorDot(name: String, rgb: Rgb, selected: Boolean, enabled: Boolean, onClick: () -> Unit) {
    val dot = Modifier.size(Sizes.swatch)
        .selectable(selected, enabled = enabled, role = Role.RadioButton, onClick = onClick)
        .semantics { contentDescription = name }
    val ringed = if (selected) dot.border(Sizes.selectedRing, MaterialTheme.colorScheme.primary, CircleShape).padding(Spacing.s) else dot
    Box(ringed.background(rgb.toColor(), CircleShape))
}

private const val MIN_GLOW_ALPHA = 0.35f

private const val HALO_ALPHA = 0.3f
