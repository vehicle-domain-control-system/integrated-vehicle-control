package com.vdcs.mobile.ui.screen

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import com.vdcs.mobile.R
import com.vdcs.mobile.domain.model.ConnectionState
import com.vdcs.mobile.session.AppMode
import com.vdcs.mobile.ui.components.HintText
import com.vdcs.mobile.ui.components.SectionCard
import com.vdcs.mobile.ui.components.TextRow
import com.vdcs.mobile.ui.rules.ControlRules
import com.vdcs.mobile.ui.theme.Spacing

@Composable
fun ConnectionPanel(
    mode: AppMode,
    connection: ConnectionState,
    registered: Boolean,
    detail: String?,
    actions: ConnectionActions,
    confirmingUnregister: Boolean,
    onConfirmUnregisterChange: (Boolean) -> Unit,
    onUnregisterConfirmed: () -> Unit,
    warningBanner: @Composable () -> Unit,
    modifier: Modifier = Modifier,
) {
    SectionCard(stringResource(R.string.connection_title), modifier) {
        ModeChips(mode, actions.onSwitchMode)
        TextRow(
            stringResource(R.string.connection_registration),
            stringResource(if (registered) R.string.connection_registered else R.string.connection_not_registered),
        )
        detail?.let { HintText(it) }
        Row(horizontalArrangement = Arrangement.spacedBy(Spacing.m)) {
            OutlinedButton(onClick = actions.onConnect, enabled = ControlRules.connectBlockReason(connection) == null) { Text(stringResource(R.string.connection_connect)) }
            OutlinedButton(onClick = actions.onDisconnect, enabled = ControlRules.disconnectBlockReason(connection) == null) { Text(stringResource(R.string.connection_disconnect)) }
            TextButton(onClick = { onConfirmUnregisterChange(true) }) { Text(stringResource(R.string.connection_unregister)) }
        }
    }
    if (confirmingUnregister) {
        UnregisterDialog(mode, warningBanner, onConfirm = onUnregisterConfirmed, onDismiss = { onConfirmUnregisterChange(false) })
    }
}

@Composable
private fun ModeChips(mode: AppMode, onSwitchMode: (AppMode) -> Unit) {
    Row(horizontalArrangement = Arrangement.spacedBy(Spacing.m)) {
        FilterChip(selected = mode == AppMode.REAL, onClick = { onSwitchMode(AppMode.REAL) }, label = { Text(stringResource(R.string.mode_real)) })
        FilterChip(selected = mode == AppMode.DEMO, onClick = { onSwitchMode(AppMode.DEMO) }, label = { Text(stringResource(R.string.mode_demo)) })
    }
}

@Composable
private fun UnregisterDialog(mode: AppMode, warningBanner: @Composable () -> Unit, onConfirm: () -> Unit, onDismiss: () -> Unit) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(R.string.unregister_title)) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(Spacing.l)) {
                warningBanner()
            }
        },
        confirmButton = {
            TextButton(onClick = onConfirm) {
                Text(stringResource(if (mode == AppMode.REAL) R.string.unregister_confirm_real else R.string.unregister_confirm_demo))
            }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text(stringResource(R.string.unregister_cancel)) } },
    )
}
