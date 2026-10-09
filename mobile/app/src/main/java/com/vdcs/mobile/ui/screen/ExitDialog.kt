package com.vdcs.mobile.ui.screen

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.res.stringResource
import com.vdcs.mobile.R
import com.vdcs.mobile.ui.theme.Spacing

@Composable
fun ExitDialog(warningBanner: @Composable () -> Unit, onConfirm: () -> Unit, onDismiss: () -> Unit) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(R.string.exit_title)) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(Spacing.l)) {
                warningBanner()
                Text(stringResource(R.string.exit_notice), style = MaterialTheme.typography.bodyMedium)
            }
        },
        confirmButton = { TextButton(onClick = onConfirm) { Text(stringResource(R.string.exit_confirm)) } },
        dismissButton = { TextButton(onClick = onDismiss) { Text(stringResource(R.string.exit_cancel)) } },
    )
}
