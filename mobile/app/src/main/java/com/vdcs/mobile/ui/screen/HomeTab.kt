package com.vdcs.mobile.ui.screen

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowRight
import androidx.compose.material.icons.filled.Bluetooth
import androidx.compose.material.icons.filled.Lightbulb
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.LockOpen
import androidx.compose.material.icons.filled.Thermostat
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButtonColors
import androidx.compose.material3.IconButtonDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import com.vdcs.mobile.R
import com.vdcs.mobile.domain.model.LockState
import com.vdcs.mobile.domain.model.Qualified
import com.vdcs.mobile.domain.model.UserRequest
import com.vdcs.mobile.domain.model.VehicleFunction
import com.vdcs.mobile.session.AppMode
import com.vdcs.mobile.ui.VehicleUiState
import com.vdcs.mobile.ui.components.BlockNote
import com.vdcs.mobile.ui.components.CircleControl
import com.vdcs.mobile.ui.components.StatusChipView
import com.vdcs.mobile.ui.rules.ConnectCta
import com.vdcs.mobile.ui.rules.HomeRules
import com.vdcs.mobile.ui.rules.HomeSection
import com.vdcs.mobile.ui.rules.StatusChip
import com.vdcs.mobile.ui.rules.StatusSentence
import com.vdcs.mobile.ui.rules.TabRules
import com.vdcs.mobile.ui.rules.ValueFormat
import com.vdcs.mobile.ui.rules.WarningRules
import com.vdcs.mobile.ui.rules.label
import com.vdcs.mobile.ui.rules.trustedValue
import com.vdcs.mobile.ui.theme.Sizes
import com.vdcs.mobile.ui.theme.Spacing
import com.vdcs.mobile.ui.theme.interiorLight
import com.vdcs.mobile.ui.theme.onInteriorLight

class ConnectionActions(
    val onSwitchMode: (AppMode) -> Unit,
    val onConnect: () -> Unit,
    val onDisconnect: () -> Unit,
    val onUnregister: () -> Unit,
)

@Composable
fun HomeTab(
    state: VehicleUiState,
    nowMs: () -> Long,
    contentPadding: PaddingValues,
    onSend: (UserRequest) -> Unit,
    onOpenAlerts: () -> Unit,
    connectionActions: ConnectionActions,
    modifier: Modifier = Modifier,
) {
    var confirmUnregister by rememberSaveable { mutableStateOf(false) }
    val context = LocalContext.current
    val unregister = {
        confirmUnregister = false
        connectionActions.onUnregister()
        if (state.mode == AppMode.REAL) openBluetoothSettings(context)
    }
    val connectionPanel: @Composable () -> Unit = {
        ConnectionPanel(
            state.mode, state.connection, state.registered, state.connectionDetail, connectionActions,
            confirmingUnregister = confirmUnregister,
            onConfirmUnregisterChange = { confirmUnregister = it },
            onUnregisterConfirmed = unregister,
            warningBanner = { ActiveWarningBanner(state.snapshot.warnings, state.warningOrigin) },
        )
    }
    val cta = HomeRules.connectCta(state.connection)
    val connectCta: @Composable () -> Unit = { cta?.let { ConnectCtaButton(it, connectionActions.onConnect) } }
    TabColumn(contentPadding, modifier) {
        items(TabRules.homeSections(showConnectCta = cta != null), key = { it.name }) { section ->
            HomeItem(section, state, nowMs(), onSend, onOpenAlerts, connectCta, connectionPanel)
        }
    }
}

@Composable
private fun HomeItem(
    section: HomeSection,
    state: VehicleUiState,
    nowMs: Long,
    onSend: (UserRequest) -> Unit,
    onOpenAlerts: () -> Unit,
    connectCta: @Composable () -> Unit,
    connection: @Composable () -> Unit,
) {
    val s = state.snapshot
    when (section) {
        HomeSection.CAR_VIEW -> CarViewSlot(s, state.warningOrigin, nowMs)
        HomeSection.STATUS_SENTENCE -> StatusSentenceView(HomeRules.statusSentence(s.door, s.environment, nowMs))
        HomeSection.CONNECT_CTA -> connectCta()
        HomeSection.QUICK_CONTROLS -> QuickControls(
            QuickReports(s.door?.lock, s.settings?.climateAuto, s.settings?.lightEnabled, nowMs),
            QuickBlockReasons(
                door = state.controlBlockReason(VehicleFunction.DOOR),
                climate = state.controlBlockReason(VehicleFunction.CLIMATE),
                light = state.controlBlockReason(VehicleFunction.INTERIOR_LIGHT),
            ),
            onSend,
        )
        HomeSection.WARNING_LINE -> WarningLineCard(
            HomeRules.warningSummary(s.warnings, state.connection, state.warningOrigin),
            WarningRules.unreadShortText(state.unreadWarningCount),
            onOpenAlerts,
        )
        HomeSection.CONNECTION -> connection()
    }
}

@Composable
fun StatusSentenceView(sentence: StatusSentence, modifier: Modifier = Modifier) {
    Column(modifier.fillMaxWidth(), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(Spacing.xs)) {
        Text(sentence.headline, style = MaterialTheme.typography.headlineMedium, fontWeight = FontWeight.Bold, textAlign = TextAlign.Center)
        Text(
            sentence.detail,
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            textAlign = TextAlign.Center,
        )
    }
}

@Composable
fun WarningLineCard(summary: StatusChip, unreadShortText: String?, onOpenAlerts: () -> Unit, modifier: Modifier = Modifier) {
    val c = MaterialTheme.colorScheme
    Card(
        onClick = onOpenAlerts,
        modifier = modifier.fillMaxWidth(),
        shape = MaterialTheme.shapes.large,
        colors = CardDefaults.cardColors(containerColor = c.surfaceContainer, contentColor = c.onSurface),
    ) {
        Row(Modifier.padding(horizontal = Spacing.xl, vertical = Spacing.l), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(Spacing.xs)) {
                Row(horizontalArrangement = Arrangement.spacedBy(Spacing.s)) {
                    Text(stringResource(R.string.home_warning_title), style = MaterialTheme.typography.titleSmall)
                    unreadShortText?.let { Text(it, style = MaterialTheme.typography.titleSmall, color = c.tertiary) }
                }
                StatusChipView(summary)
            }
            Icon(Icons.AutoMirrored.Filled.KeyboardArrowRight, contentDescription = stringResource(R.string.open_alerts), tint = c.outline)
        }
    }
}

@Composable
fun ConnectCtaButton(cta: ConnectCta, onConnect: () -> Unit, modifier: Modifier = Modifier) {
    Button(onClick = onConnect, enabled = cta.blockReason == null, modifier = modifier.fillMaxWidth()) {
        if (cta.blockReason != null) {
            CircularProgressIndicator(Modifier.size(Sizes.inlineProgress), strokeWidth = Sizes.inlineProgressStroke)
        } else {
            Icon(Icons.Filled.Bluetooth, contentDescription = null)
        }
        Spacer(Modifier.width(Spacing.m))
        Text(cta.text)
    }
}

class QuickReports(
    val lock: Qualified<LockState>?,
    val climateAuto: Qualified<Boolean>?,
    val lightEnabled: Qualified<Boolean>?,
    val nowMs: Long,
)

class QuickBlockReasons(val door: String?, val climate: String?, val light: String?) {
    val distinct: List<String> get() = listOfNotNull(door, climate, light).distinct()
}

@Composable
fun QuickControls(reports: QuickReports, blockReasons: QuickBlockReasons, onSend: (UserRequest) -> Unit, modifier: Modifier = Modifier) {
    val c = MaterialTheme.colorScheme
    val lockState = ValueFormat.value(reports.lock, reports.nowMs) { it.label }
    Column(modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(Spacing.m)) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly, verticalAlignment = Alignment.Top) {
            val lockName = stringResource(R.string.quick_lock)
            val unlockName = stringResource(R.string.quick_unlock)
            CircleControl(
                Icons.Filled.Lock, lockName, HomeRules.controlDescription(lockName, lockState), blockReasons.door,
                { onSend(UserRequest.Door(lock = true)) }, on = HomeRules.filled(reports.lock, LockState.LOCKED),
            )
            CircleControl(
                Icons.Filled.LockOpen, unlockName, HomeRules.controlDescription(unlockName, lockState), blockReasons.door,
                { onSend(UserRequest.Door(lock = false)) },
            )
            ToggleControl(Icons.Filled.Thermostat, stringResource(R.string.quick_climate), reports.climateAuto, reports.nowMs, blockReasons.climate) {
                onSend(UserRequest.ClimateAuto(it))
            }
            ToggleControl(
                Icons.Filled.Lightbulb, stringResource(R.string.quick_light), reports.lightEnabled, reports.nowMs, blockReasons.light,
                IconButtonDefaults.filledIconButtonColors(containerColor = c.interiorLight, contentColor = c.onInteriorLight),
            ) { onSend(UserRequest.LightEnabled(it)) }
        }
        blockReasons.distinct.forEach { BlockNote(it) }
    }
}

@Composable
private fun ToggleControl(
    icon: ImageVector,
    name: String,
    reported: Qualified<Boolean>?,
    nowMs: Long,
    blockReason: String?,
    onColors: IconButtonColors = IconButtonDefaults.filledIconButtonColors(),
    onRequest: (Boolean) -> Unit,
) {
    val value = reported.trustedValue()
    CircleControl(
        icon,
        name,
        HomeRules.controlDescription(HomeRules.toggleLabel(name, value), ValueFormat.value(reported, nowMs, ValueFormat::onOff)),
        blockReason,
        { onRequest(HomeRules.toggleRequest(value)) },
        on = HomeRules.filled(reported, true),
        onColors = onColors,
    )
}
