package com.vdcs.mobile.ui.screen

import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import com.vdcs.mobile.domain.model.Warning
import com.vdcs.mobile.domain.model.WarningRecord
import com.vdcs.mobile.ui.VehicleUiState
import com.vdcs.mobile.ui.rules.AckTargets
import com.vdcs.mobile.ui.rules.ControlRules
import com.vdcs.mobile.ui.rules.WarningRules

@Composable
fun AlertsTab(
    state: VehicleUiState,
    nowMs: () -> Long,
    contentPadding: PaddingValues,
    onAck: (Warning) -> Unit,
    onAckAll: (AckTargets) -> Unit,
    onConfirmArchived: (WarningRecord) -> Unit,
    modifier: Modifier = Modifier,
) {
    var clearedExpanded by rememberSaveable { mutableStateOf(false) }
    val warnings = state.snapshot.warnings
    val groups = WarningRules.group(warnings)
    val targets = WarningRules.ackAllTargets(warnings, state.warningHistory, state.domainBootId)
    TabColumn(contentPadding, modifier) {
        item(key = "summary") {
            WarningsSummary(
                WarningRules.sectionTitle(state.warningOrigin),
                WarningRules.summaryText(warnings, state.connection),
                targets,
                ControlRules.ackAllBlockReason(state.connection, targets),
                onAckAll,
            )
        }
        if (groups.open.isNotEmpty()) {
            item(key = "open") { OpenWarningsSection(groups.open, state.warningOrigin, nowMs(), state.ackBlockReason, onAck) }
        }
        item(key = "past") {
            PastWarningsSection(
                PastWarnings(groups.cleared, WarningRules.archived(state.warningHistory, warnings), targets.archived),
                WarningRules.historySyncText(state.connection),
                nowMs(),
                state.ackBlockReason,
                clearedExpanded = clearedExpanded,
                onClearedExpandedChange = { clearedExpanded = it },
                onAck = onAck,
                onConfirmArchived = onConfirmArchived,
            )
        }
    }
}
