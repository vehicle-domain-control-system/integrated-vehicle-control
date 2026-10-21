package com.vdcs.mobile.ui.components

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.selection.selectableGroup
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.font.FontWeight
import com.vdcs.mobile.ui.theme.Sizes
import com.vdcs.mobile.ui.theme.Spacing

@Composable
fun <T> SegmentedControl(
    options: List<T>,
    selected: T?,
    label: (T) -> String,
    enabled: Boolean,
    onSelect: (T) -> Unit,
    modifier: Modifier = Modifier,
) {
    Surface(modifier.fillMaxWidth(), shape = MaterialTheme.shapes.medium, color = MaterialTheme.colorScheme.background) {
        Row(
            Modifier.padding(Spacing.xs).selectableGroup(),
            horizontalArrangement = Arrangement.spacedBy(Spacing.s),
        ) {
            options.forEach { option ->
                Segment(label(option), option == selected, enabled, { onSelect(option) }, Modifier.weight(1f))
            }
        }
    }
}

@Composable
private fun Segment(text: String, selected: Boolean, enabled: Boolean, onClick: () -> Unit, modifier: Modifier) {
    val c = MaterialTheme.colorScheme
    Surface(
        modifier.height(Sizes.segmentHeight).selectable(selected, enabled = enabled, role = Role.RadioButton, onClick = onClick),
        shape = MaterialTheme.shapes.small,
        color = if (selected) c.primary else c.background,
        contentColor = if (selected) c.onPrimary else c.onSurfaceVariant,
    ) {
        Box(contentAlignment = Alignment.Center) {
            Text(text, style = MaterialTheme.typography.labelLarge, fontWeight = if (selected) FontWeight.Bold else null)
        }
    }
}
