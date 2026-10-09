package com.vdcs.mobile.ui.screen

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import com.vdcs.mobile.R
import com.vdcs.mobile.ui.theme.Spacing
import com.vdcs.mobile.ui.theme.ok
import com.vdcs.mobile.ui.components.ListDivider
import com.vdcs.mobile.ui.components.ListSection
import com.vdcs.mobile.ui.components.ValueRow
import com.vdcs.mobile.ui.components.ValueLine
import com.vdcs.mobile.ui.components.BlockNote
import com.vdcs.mobile.ui.components.HintText
import com.vdcs.mobile.domain.model.EcuHealth
import com.vdcs.mobile.domain.model.FunctionStatus
import com.vdcs.mobile.domain.model.TrackedRequest
import com.vdcs.mobile.domain.model.VehicleFunction
import com.vdcs.mobile.ui.rules.ControlRules
import com.vdcs.mobile.ui.rules.RequestRules
import com.vdcs.mobile.ui.rules.RequestTone
import com.vdcs.mobile.ui.rules.ValueFormat
import com.vdcs.mobile.ui.rules.VehicleValueRules
import com.vdcs.mobile.ui.rules.label

private val LISTED_FUNCTIONS = VehicleFunction.entries.filter { it != VehicleFunction.WINDOW }

@Composable
fun AvailabilitySection(availability: Map<VehicleFunction, FunctionStatus>, nowMs: Long, modifier: Modifier = Modifier) {
    ListSection(stringResource(R.string.availability_title), modifier) {
        LISTED_FUNCTIONS.forEach { fn ->
            ValueRow(fn.label, ControlRules.availabilityText(availability[fn], nowMs))
        }
    }
}

@Composable
fun EcuSection(ecus: List<EcuHealth>, nowMs: Long, modifier: Modifier = Modifier) {
    ListSection(stringResource(R.string.ecu_title), modifier) {
        if (ecus.isEmpty()) HintText(stringResource(R.string.ecu_empty))
        ecus.forEachIndexed { i, ecu ->
            if (i > 0) ListDivider()
            Column(verticalArrangement = Arrangement.spacedBy(Spacing.xxs)) {
                ValueRow(VehicleValueRules.ecuName(ecu.name), VehicleValueRules.ecuStatusText(ecu, nowMs))
                ValueLine(VehicleValueRules.ecuFaultsText(ecu))
                VehicleValueRules.ecuCategoryText(ecu)?.let { ValueRow(stringResource(R.string.ecu_fault_category), it) }
                VehicleValueRules.ecuFaultDetailText(ecu)?.let { ValueLine(it) }
                VehicleValueRules.ecuActionText(ecu)?.let { HintText(it) }
                VehicleValueRules.ecuReceivedText(ecu, nowMs)?.let {
                    Text(it, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.outline)
                }
            }
        }
    }
}

@Composable
fun RequestsSection(
    requests: List<TrackedRequest>,
    nowMs: Long,
    resendBlockReason: (TrackedRequest) -> String?,
    onResend: (TrackedRequest) -> Unit,
    modifier: Modifier = Modifier,
) {
    val list = requests.sortedByDescending { it.sentAtMs }
    ListSection(stringResource(R.string.requests_title), modifier) {
        if (list.isEmpty()) HintText(stringResource(R.string.requests_empty))
        list.forEachIndexed { i, t ->
            if (i > 0) ListDivider()
            RequestItem(t, nowMs, resendBlockReason, onResend)
        }
        if (list.any { RequestRules.isResendable(it) }) {
            HintText(stringResource(R.string.requests_resend_note))
        }
    }
}

@Composable
private fun RequestItem(
    t: TrackedRequest,
    nowMs: Long,
    resendBlockReason: (TrackedRequest) -> String?,
    onResend: (TrackedRequest) -> Unit,
) {
    val status = RequestRules.status(t)
    Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(Spacing.xxs)) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(Spacing.m)) {
            Column(Modifier.weight(1f)) {
                Text(t.request.label, style = MaterialTheme.typography.bodyMedium)
                HintText(requestMetaText(t, nowMs))
            }
            Text(status.label, style = MaterialTheme.typography.labelLarge, color = toneColor(status.tone))
        }
        status.detail?.let { HintText(it) }
        RequestRules.progressText(t)?.let { HintText(it) }
        RequestRules.mismatchText(t)?.let {
            Text(it, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.error)
        }
        if (RequestRules.isResendable(t)) ResendButton(resendBlockReason(t)) { onResend(t) }
    }
}

@Composable
private fun requestMetaText(t: TrackedRequest, nowMs: Long): String = listOfNotNull(
    stringResource(R.string.requests_sent_ago, ValueFormat.ago(t.sentAtMs, nowMs)),
    RequestRules.confirmationText(t),
    "#${t.requestId}",
).joinToString(" · ")

@Composable
private fun ResendButton(blockReason: String?, onClick: () -> Unit) {
    OutlinedButton(onClick = onClick, enabled = blockReason == null) { Text(stringResource(R.string.requests_resend)) }
    BlockNote(blockReason)
}

@Composable
private fun toneColor(tone: RequestTone): Color = when (tone) {
    RequestTone.SUCCESS -> MaterialTheme.colorScheme.ok
    RequestTone.FAILURE -> MaterialTheme.colorScheme.error
    RequestTone.UNCONFIRMED -> MaterialTheme.colorScheme.tertiary
    RequestTone.PENDING -> MaterialTheme.colorScheme.secondary
    RequestTone.NEUTRAL -> MaterialTheme.colorScheme.onSurfaceVariant
}
