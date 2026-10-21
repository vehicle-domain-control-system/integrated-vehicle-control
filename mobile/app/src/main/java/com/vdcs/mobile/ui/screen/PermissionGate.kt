package com.vdcs.mobile.ui.screen

import android.Manifest
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.provider.Settings
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.annotation.StringRes
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.core.content.ContextCompat
import androidx.lifecycle.compose.LifecycleResumeEffect
import com.vdcs.mobile.R
import com.vdcs.mobile.ui.theme.Spacing

fun requiredBlePermissions(): Array<String> =
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
    } else {
        arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
    }

private fun allGranted(context: Context, permissions: Array<String>): Boolean =
    permissions.all { ContextCompat.checkSelfPermission(context, it) == PackageManager.PERMISSION_GRANTED }

@Composable
fun BlePermissionGate(required: Boolean, onUseDemo: () -> Unit, modifier: Modifier = Modifier, content: @Composable () -> Unit) {
    val context = LocalContext.current
    val permissions = remember { requiredBlePermissions() }
    var granted by remember { mutableStateOf(allGranted(context, permissions)) }
    var denied by rememberSaveable { mutableStateOf(false) }
    val launcher = rememberLauncherForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
        granted = allGranted(context, permissions)
        denied = !granted
    }
    LifecycleResumeEffect(Unit) {
        granted = allGranted(context, permissions)
        onPauseOrDispose { }
    }
    if (!required || granted) {
        content()
    } else {
        PermissionRequest(
            denied = denied,
            onRequest = { launcher.launch(permissions) },
            onOpenSettings = { openAppSettings(context) },
            onUseDemo = onUseDemo,
            modifier = modifier,
        )
    }
}

@Composable
private fun PermissionRequest(
    denied: Boolean,
    onRequest: () -> Unit,
    onOpenSettings: () -> Unit,
    onUseDemo: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(Spacing.xxl),
        verticalArrangement = Arrangement.spacedBy(Spacing.xl),
    ) {
        Text(stringResource(R.string.permission_title), style = MaterialTheme.typography.headlineSmall)
        Text(stringResource(permissionReasonRes()), style = MaterialTheme.typography.bodyMedium)
        Button(onClick = onRequest, modifier = Modifier.fillMaxWidth()) { Text(stringResource(R.string.permission_request)) }
        if (denied) {
            Text(stringResource(R.string.permission_denied), style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.error)
            OutlinedButton(onClick = onOpenSettings, modifier = Modifier.fillMaxWidth()) { Text(stringResource(R.string.permission_open_settings)) }
        }
        TextButton(onClick = onUseDemo, modifier = Modifier.fillMaxWidth()) { Text(stringResource(R.string.permission_use_demo)) }
    }
}

@StringRes
private fun permissionReasonRes(): Int =
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) R.string.permission_reason_nearby else R.string.permission_reason_location

fun openAppSettings(context: Context) {
    val intent = Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.fromParts("package", context.packageName, null))
        .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
    context.startActivity(intent)
}

fun openBluetoothSettings(context: Context) {
    context.startActivity(Intent(Settings.ACTION_BLUETOOTH_SETTINGS).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
}
