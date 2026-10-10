package com.vdcs.mobile.ui.screen.carview

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.colorResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign
import com.vdcs.mobile.R
import com.vdcs.mobile.ui.CarPose
import com.vdcs.mobile.ui.DoorDisplay
import com.vdcs.mobile.ui.DoorPose
import com.vdcs.mobile.ui.LightDisplay
import com.vdcs.mobile.ui.LightPose
import com.vdcs.mobile.ui.rules.ValueFormat
import com.vdcs.mobile.ui.rules.label
import com.vdcs.mobile.ui.theme.Sizes
import com.vdcs.mobile.ui.theme.Spacing

internal object CarPoseText {
    const val DOOR = "도어"
    const val LIGHT = "실내등"
    const val DOOR_OPEN = "열림 (어느 도어인지는 차량 미보고)"
    const val DOOR_CLOSED = "모두 닫힘"
    const val UNKNOWN = ValueFormat.UNTRUSTED_TEXT
    const val LIGHT_OFF = "꺼짐"
    const val STALE = "마지막 수신값"
    const val LIGHT_UNCONFIRMED = "지시 반영 (점등 미확인)"

    fun door(d: DoorPose): String {
        val base = when (d.display) {
            DoorDisplay.OPEN -> DOOR_OPEN
            DoorDisplay.CLOSED -> DOOR_CLOSED
            DoorDisplay.UNKNOWN -> UNKNOWN
        }
        return if (d.stale) "$base · $STALE" else base
    }

    fun light(l: LightPose): String {
        val base = when (l.display) {
            LightDisplay.ON -> buildString {
                append("켜짐 · 밝기 ${l.brightnessPercent ?: 0}%")
                l.rgb?.let { append(" · ${it.label}") }
                if (!l.physicallyConfirmed) append(" · $LIGHT_UNCONFIRMED")
            }
            LightDisplay.OFF -> LIGHT_OFF
            LightDisplay.UNKNOWN -> UNKNOWN
        }
        val alert = l.alert?.let { " · ${it.label}" }.orEmpty()
        return base + alert + if (l.stale) " · $STALE" else ""
    }

    fun doorLine(d: DoorPose): String = "$DOOR: ${door(d)}"
    fun lightLine(l: LightPose): String = "$LIGHT: ${light(l)}"

    private fun badges(pose: CarPose): List<BadgeFact> = doorFacts(pose.door) + lightFacts(pose.light)

    private fun doorFacts(d: DoorPose): List<BadgeFact> = buildList {
        when (d.display) {
            DoorDisplay.OPEN -> add(BadgeFact(DOOR, DOOR_OPEN))
            DoorDisplay.UNKNOWN -> add(BadgeFact(DOOR, UNKNOWN))
            DoorDisplay.CLOSED -> Unit
        }
        if (d.stale) add(BadgeFact(DOOR, STALE))
    }

    private fun lightFacts(l: LightPose): List<BadgeFact> = buildList {
        if (l.unknown) add(BadgeFact(LIGHT, UNKNOWN))
        if (l.on && !l.physicallyConfirmed) add(BadgeFact(LIGHT, LIGHT_UNCONFIRMED))
        l.alert?.let { add(BadgeFact(LIGHT, it.label)) }
        if (l.stale) add(BadgeFact(LIGHT, STALE))
    }

    fun badgeText(pose: CarPose): String? = badges(pose)
        .groupBy(BadgeFact::state, BadgeFact::subject)
        .map { (state, subjects) -> "${subjects.joinToString("·")} $state" }
        .takeIf { it.isNotEmpty() }
        ?.joinToString(" · ")
}

private data class BadgeFact(val subject: String, val state: String)

private const val BADGE_BG_ALPHA = 0.55f

@Composable
internal fun CarPoseBadge(pose: CarPose, modifier: Modifier = Modifier) {
    val text = CarPoseText.badgeText(pose) ?: return
    val scrim = colorResource(R.color.scene_badge_scrim).copy(alpha = BADGE_BG_ALPHA)
    Text(
        text,
        modifier
            .background(scrim, CircleShape)
            .padding(horizontal = Spacing.l, vertical = Sizes.overlayBadgePaddingV),
        color = colorResource(R.color.scene_badge_text),
        style = MaterialTheme.typography.labelMedium,
        textAlign = TextAlign.Center,
    )
}

@Composable
internal fun CarPoseTextSummary(pose: CarPose, reason: String, modifier: Modifier = Modifier) {
    Column(
        modifier.fillMaxSize().padding(Spacing.xl),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(Spacing.m, Alignment.CenterVertically),
    ) {
        Text(stringResource(R.string.car_summary_title), style = MaterialTheme.typography.titleMedium)
        Text(CarPoseText.doorLine(pose.door), style = MaterialTheme.typography.bodyMedium)
        Text(CarPoseText.lightLine(pose.light), style = MaterialTheme.typography.bodyMedium)
        Text(reason, style = MaterialTheme.typography.bodySmall)
    }
}
