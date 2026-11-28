package com.vdcs.mobile.ui.screen

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import com.vdcs.mobile.R
import com.vdcs.mobile.domain.model.ReadState
import com.vdcs.mobile.domain.model.Warning
import com.vdcs.mobile.domain.model.WarningRecord
import com.vdcs.mobile.ui.components.BlockNote
import com.vdcs.mobile.ui.components.HintText
import com.vdcs.mobile.ui.components.ListDivider
import com.vdcs.mobile.ui.components.ListSection
import com.vdcs.mobile.ui.components.ToneDot
import com.vdcs.mobile.ui.rules.ChipTone
import com.vdcs.mobile.ui.rules.WarningRules
import com.vdcs.mobile.ui.rules.label
import com.vdcs.mobile.ui.theme.Spacing

class PastWarnings(val cleared: List<Warning>, val archived: List<WarningRecord>, val confirmable: List<WarningRecord>)

@Composable
fun PastWarningsSection(
    past: PastWarnings,
    syncText: String?,
    nowMs: Long,
    ackBlockReason: String?,
    clearedExpanded: Boolean,
    onClearedExpandedChange: (Boolean) -> Unit,
    onAck: (Warning) -> Unit,
    onConfirmArchived: (WarningRecord) -> Unit,
    modifier: Modifier = Modifier,
) {
    ListSection(stringResource(R.string.alerts_past_title), modifier) {
        syncText?.let { HintText(it) }
        if (past.cleared.isNotEmpty()) {
            TextButton(onClick = { onClearedExpandedChange(!clearedExpanded) }) {
                Text(WarningRules.clearedSummaryText(past.cleared.size, clearedExpanded))
            }
        }
        if (clearedExpanded) ClearedRows(past.cleared, nowMs, ackBlockReason, onAck)
        WarningRules.historyEmptyText(past.archived)?.let { HintText(it) }
        ArchivedRows(past.archived, past.confirmable, nowMs, onConfirmArchived)
    }
}

@Composable
private fun ArchivedRows(archived: List<WarningRecord>, confirmable: List<WarningRecord>, nowMs: Long, onConfirm: (WarningRecord) -> Unit) {
    archived.forEachIndexed { i, r ->
        if (i > 0) ListDivider()
        PastRow(WarningRules.severityTone(r.severity), r.type.label, WarningRules.recordMetaText(r, nowMs)) {
            if (r in confirmable) {
                OutlinedButton(onClick = { onConfirm(r) }) { Text(stringResource(R.string.warning_confirm_archived)) }
            } else {
                ReadText(WarningRules.recordReadText(r), unread = r.unread)
            }
        }
    }
}

@Composable
private fun ClearedRows(cleared: List<Warning>, nowMs: Long, ackBlockReason: String?, onAck: (Warning) -> Unit) {
    cleared.forEach { w ->
        PastRow(WarningRules.severityTone(w.severity), w.type.label, WarningRules.clearedMetaText(w, nowMs)) {
            if (WarningRules.isAcknowledgeable(w)) {
                OutlinedButton(onClick = { onAck(w) }, enabled = ackBlockReason == null) { Text(stringResource(R.string.warning_ack)) }
            } else {
                ReadText(WarningRules.readShortText(w.read), unread = w.read == ReadState.UNREAD)
            }
        }
        ListDivider()
    }
    if (cleared.any { WarningRules.isAcknowledgeable(it) }) BlockNote(ackBlockReason)
}

@Composable
private fun PastRow(tone: ChipTone, name: String, meta: String, trailing: @Composable () -> Unit) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(Spacing.l)) {
        ToneDot(tone)
        Column(Modifier.weight(1f)) {
            Text(name, style = MaterialTheme.typography.bodyMedium, fontWeight = FontWeight.SemiBold)
            HintText(meta)
        }
        trailing()
    }
}

@Composable
private fun ReadText(text: String, unread: Boolean) {
    val color = if (unread) MaterialTheme.colorScheme.tertiary else MaterialTheme.colorScheme.outline
    Text(text, style = MaterialTheme.typography.labelMedium, color = color)
}
