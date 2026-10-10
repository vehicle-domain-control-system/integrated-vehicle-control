package com.vdcs.mobile.ui.components

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.FilledIconButton
import androidx.compose.material3.FilledTonalIconButton
import androidx.compose.material3.IconButtonColors
import androidx.compose.material3.IconButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import com.vdcs.mobile.ui.theme.Sizes
import com.vdcs.mobile.ui.theme.Spacing

@Composable
fun RoundButton(
    description: String,
    enabled: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    size: Dp = Sizes.roundControl,
    on: Boolean = false,
    onColors: IconButtonColors = IconButtonDefaults.filledIconButtonColors(),
    content: @Composable () -> Unit,
) {
    val m = modifier.size(size).semantics { contentDescription = description }
    if (on) {
        FilledIconButton(onClick, m, enabled = enabled, shape = CircleShape, colors = onColors, content = content)
    } else {
        FilledTonalIconButton(onClick, m, enabled = enabled, shape = CircleShape, content = content)
    }
}

@Composable
fun CircleControl(
    icon: ImageVector,
    label: String,
    description: String,
    blockReason: String?,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    on: Boolean = false,
    onColors: IconButtonColors = IconButtonDefaults.filledIconButtonColors(),
) {
    Column(
        modifier.width(Sizes.roundControlColumn),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(Spacing.m),
    ) {
        RoundButton(description, enabled = blockReason == null, onClick = onClick, on = on, onColors = onColors) {
            Icon(icon, contentDescription = null, Modifier.size(Sizes.roundControlIcon))
        }
        Text(
            label,
            Modifier.clearAndSetSemantics {},
            style = MaterialTheme.typography.labelMedium,
            textAlign = TextAlign.Center,
            maxLines = 1,
        )
    }
}
