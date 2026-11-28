package com.vdcs.mobile.ui.screen

import androidx.activity.compose.BackHandler
import androidx.activity.compose.LocalActivity
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListScope
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.AcUnit
import androidx.compose.material.icons.outlined.DirectionsCar
import androidx.compose.material.icons.outlined.Lightbulb
import androidx.compose.material.icons.outlined.MonitorHeart
import androidx.compose.material.icons.outlined.Notifications
import androidx.compose.material3.BadgedBox
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationBarItemDefaults
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.saveable.rememberSaveableStateHolder
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.lifecycle.repeatOnLifecycle
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.vdcs.mobile.domain.model.VehicleFunction
import com.vdcs.mobile.session.AppMode
import com.vdcs.mobile.ui.VehicleUiState
import com.vdcs.mobile.ui.VehicleViewModel
import com.vdcs.mobile.ui.components.StatusDot
import com.vdcs.mobile.ui.rules.AppTab
import com.vdcs.mobile.ui.rules.TabRules
import com.vdcs.mobile.ui.rules.WarningRules
import com.vdcs.mobile.ui.rules.label
import com.vdcs.mobile.ui.rules.title
import com.vdcs.mobile.ui.theme.Spacing

@Composable
fun VdcsApp(vm: VehicleViewModel, modifier: Modifier = Modifier) {
    val state by vm.uiState.collectAsStateWithLifecycle()
    val clock = vm.nowMs.collectAsStateWithLifecycle()
    val nowMs = { clock.value }
    val snackbar = remember { SnackbarHostState() }
    val lifecycleOwner = LocalLifecycleOwner.current
    LaunchedEffect(vm, lifecycleOwner) {
        lifecycleOwner.repeatOnLifecycle(Lifecycle.State.STARTED) { vm.messages.collect { snackbar.showSnackbar(it) } }
    }
    var tab by rememberSaveable { mutableStateOf(TabRules.START_TAB) }
    val tabStates = rememberSaveableStateHolder()
    var confirmExit by rememberSaveable { mutableStateOf(false) }
    val activity = LocalActivity.current
    BackHandler { if (tab != TabRules.START_TAB) tab = TabRules.START_TAB else confirmExit = true }
    if (confirmExit) {
        ExitDialog(
            warningBanner = { ActiveWarningBanner(state.snapshot.warnings, state.warningOrigin) },
            onConfirm = { activity?.finish() },
            onDismiss = { confirmExit = false },
        )
    }

    Scaffold(
        modifier = modifier,
        topBar = { FixedHeader(tab.title, state, nowMs, onOpenAlerts = { tab = AppTab.ALERTS }) },
        bottomBar = { TabBar(tab, WarningRules.unreadText(state.unreadWarningCount)) { tab = it } },
        snackbarHost = { SnackbarHost(snackbar) },
        containerColor = MaterialTheme.colorScheme.background,
    ) { padding ->
        BlePermissionGate(
            required = state.mode == AppMode.REAL,
            onUseDemo = { vm.switchMode(AppMode.DEMO) },
            modifier = Modifier.padding(padding),
        ) {
            tabStates.SaveableStateProvider(tab.name) { TabContent(tab, state, nowMs, vm, padding) { tab = it } }
        }
    }
}

@Composable
private fun TabContent(
    tab: AppTab,
    state: VehicleUiState,
    nowMs: () -> Long,
    vm: VehicleViewModel,
    padding: PaddingValues,
    onOpenTab: (AppTab) -> Unit,
) {
    val s = state.snapshot
    when (tab) {
        AppTab.HOME -> HomeTab(
            state, nowMs, padding, vm::send,
            onOpenAlerts = { onOpenTab(AppTab.ALERTS) },
            connectionActions = ConnectionActions(vm::switchMode, vm::connect, vm::disconnect, vm::unregister),
        )
        AppTab.CLIMATE ->
            ClimateTab(s.settings, s.climate, s.environment, nowMs, state.controlBlockReason(VehicleFunction.CLIMATE), padding, vm::send)
        AppTab.LIGHT_KEY -> LightKeyTab(
            s.settings, s.light, s.digitalKey, nowMs,
            state.controlBlockReason(VehicleFunction.INTERIOR_LIGHT), state.controlBlockReason(VehicleFunction.DIGITAL_KEY),
            padding, vm::send,
        )
        AppTab.ALERTS -> AlertsTab(state, nowMs, padding, vm::acknowledge, vm::acknowledgeAll, vm::confirmArchived)
        AppTab.VEHICLE -> VehicleTab(state, nowMs, padding, vm::resend, vm::demo)
    }
}

@Composable
fun TabColumn(contentPadding: PaddingValues, modifier: Modifier = Modifier, content: LazyListScope.() -> Unit) {
    LazyColumn(
        modifier = modifier.fillMaxSize(),
        contentPadding = PaddingValues(
            start = Spacing.xl,
            end = Spacing.xl,
            top = contentPadding.calculateTopPadding() + Spacing.l,
            bottom = contentPadding.calculateBottomPadding() + Spacing.xxl,
        ),
        verticalArrangement = Arrangement.spacedBy(Spacing.l),
        content = content,
    )
}

@Composable
private fun TabBar(selected: AppTab, alertsUnreadText: String?, onSelect: (AppTab) -> Unit) {
    val c = MaterialTheme.colorScheme
    val itemColors = NavigationBarItemDefaults.colors(
        selectedIconColor = c.onSurface,
        selectedTextColor = c.onSurface,
        indicatorColor = Color.Transparent,
        unselectedIconColor = c.outline,
        unselectedTextColor = c.outline,
    )
    Column {
        HorizontalDivider(color = c.outlineVariant)
        NavigationBar(containerColor = c.background) {
            AppTab.entries.forEach { t ->
                val isSelected = t == selected
                NavigationBarItem(
                    selected = isSelected,
                    onClick = { onSelect(t) },
                    icon = { TabIcon(t, alertsUnreadText.takeIf { t == AppTab.ALERTS }) },
                    label = { Text(t.label, maxLines = 1, fontWeight = if (isSelected) FontWeight.SemiBold else null) },
                    colors = itemColors,
                )
            }
        }
    }
}

@Composable
private fun TabIcon(tab: AppTab, unreadText: String?) {
    BadgedBox(badge = {
        unreadText?.let { StatusDot(MaterialTheme.colorScheme.tertiary, Modifier.semantics { contentDescription = it }) }
    }) {
        Icon(tab.icon, contentDescription = null)
    }
}

private val AppTab.icon: ImageVector
    get() = when (this) {
        AppTab.HOME -> Icons.Outlined.DirectionsCar
        AppTab.CLIMATE -> Icons.Outlined.AcUnit
        AppTab.LIGHT_KEY -> Icons.Outlined.Lightbulb
        AppTab.ALERTS -> Icons.Outlined.Notifications
        AppTab.VEHICLE -> Icons.Outlined.MonitorHeart
    }
