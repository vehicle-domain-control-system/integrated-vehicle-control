package com.vdcs.mobile.ui.screen

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBars
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Notifications
import androidx.compose.material3.BadgedBox
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import com.vdcs.mobile.R
import com.vdcs.mobile.domain.model.ConnectionState
import com.vdcs.mobile.ui.VehicleUiState
import com.vdcs.mobile.ui.components.HintText
import com.vdcs.mobile.ui.components.RoundButton
import com.vdcs.mobile.ui.components.StatusDot
import com.vdcs.mobile.ui.components.ValueLine
import com.vdcs.mobile.ui.rules.VehicleValueRules
import com.vdcs.mobile.ui.rules.WarningRules
import com.vdcs.mobile.ui.rules.label
import com.vdcs.mobile.ui.theme.Sizes
import com.vdcs.mobile.ui.theme.Spacing
import com.vdcs.mobile.ui.theme.ok

@Composable
fun FixedHeader(title: String, state: VehicleUiState, nowMs: () -> Long, onOpenAlerts: () -> Unit, modifier: Modifier = Modifier) {
    Surface(modifier, color = MaterialTheme.colorScheme.background) {
        Column(
            Modifier.windowInsetsPadding(WindowInsets.statusBars).padding(horizontal = Spacing.xl, vertical = Spacing.l),
            verticalArrangement = Arrangement.spacedBy(Spacing.m),
        ) {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(Spacing.xxs)) {
                    Text(title, style = MaterialTheme.typography.titleLarge)
                    LinkStatus(state.connection, state.snapshot.lastReceivedAtMs, state.warningOrigin, nowMs)
                }
                AlertsButton(WarningRules.unreadText(state.unreadWarningCount), onOpenAlerts)
            }
            ActiveWarningBanner(state.snapshot.warnings, state.warningOrigin)
        }
    }
}

@Composable
private fun LinkStatus(connection: ConnectionState, lastReceivedAtMs: Long?, origin: String?, nowMs: () -> Long) {
    val separator = stringResource(R.string.header_separator)
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(Spacing.s)) {
        StatusDot(connectionColor(connection))
        HintText(connection.label)
        HintText(separator)
        ValueLine(VehicleValueRules.lastReceivedText(lastReceivedAtMs, nowMs()))
        origin?.let {
            HintText(separator)
            Text(it, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.tertiary)
        }
    }
}

@Composable
private fun connectionColor(connection: ConnectionState): Color = when (connection) {
    ConnectionState.AUTHENTICATED -> MaterialTheme.colorScheme.ok
    ConnectionState.CONNECTING, ConnectionState.CONNECTED -> MaterialTheme.colorScheme.tertiary
    ConnectionState.DISCONNECTED -> MaterialTheme.colorScheme.outline
}

@Composable
private fun AlertsButton(unreadText: String?, onClick: () -> Unit) {
    RoundButton(unreadText ?: stringResource(R.string.open_alerts), enabled = true, onClick = onClick, size = Sizes.headerButton) {
        BadgedBox(badge = { if (unreadText != null) StatusDot(MaterialTheme.colorScheme.tertiary) }) {
            Icon(Icons.Outlined.Notifications, contentDescription = null)
        }
    }
}
