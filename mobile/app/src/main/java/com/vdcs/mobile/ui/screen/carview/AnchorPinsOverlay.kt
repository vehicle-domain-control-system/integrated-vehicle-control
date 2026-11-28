package com.vdcs.mobile.ui.screen.carview

import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.layout.Layout
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.Constraints
import com.vdcs.mobile.ui.CarAnchor
import com.vdcs.mobile.ui.CarPin
import com.vdcs.mobile.ui.components.StatusPin
import com.vdcs.mobile.ui.components.ToneDot
import com.vdcs.mobile.ui.components.toneTextColor
import com.vdcs.mobile.ui.theme.Sizes
import com.vdcs.mobile.ui.theme.Spacing
import kotlin.math.roundToInt

@Composable
internal fun AnchorPinsOverlay(
    pins: Map<CarAnchor, List<CarPin>>,
    layout: () -> AnchorLayout,
    modifier: Modifier = Modifier,
) {
    val callouts = remember(pins) { pins.flatMap { (anchor, list) -> list.map { anchor to it } } }
    val placed = remember { PlacedCallouts() }
    val lineColors = callouts.map { (_, pin) -> toneTextColor(pin.tone) }
    val edge = with(LocalDensity.current) { Spacing.m.toPx() }
    val gap = with(LocalDensity.current) { Spacing.xs.toPx() }
    val stroke = with(LocalDensity.current) { Sizes.border.toPx() }
    Layout(
        content = {
            callouts.forEach { (_, pin) -> StatusPin(pin.tone, pin.label, pin.description) }
            callouts.forEach { (_, pin) -> ToneDot(pin.tone) }
        },
        modifier = modifier.fillMaxSize().drawBehind {
            placed.lines.forEachIndexed { i, line ->
                line?.let { drawLine(lineColors[i].copy(alpha = it.alpha), it.from, it.to, stroke) }
            }
        },
    ) { measurables, constraints ->
        val loose = Constraints(maxWidth = constraints.maxWidth / 2, maxHeight = constraints.maxHeight)
        val labels = measurables.take(callouts.size).map { it.measure(loose) }
        val dots = measurables.drop(callouts.size).map { it.measure(Constraints()) }
        val spots = callouts.map { (anchor, _) -> layout().spotOf(anchor)?.takeIf { it.visibility != AnchorVisibility.HIDDEN } }
        val width = constraints.maxWidth.toFloat()
        val leftSide = spots.map { (it?.x ?: width) < width / 2 }
        val tops = FloatArray(callouts.size)
        listOf(true, false).forEach { side ->
            val idx = spots.indices.filter { spots[it] != null && leftSide[it] == side }
            CalloutPlacement.stack(idx.map { spots[it]!!.y }, idx.map { labels[it].height }, gap, constraints.maxHeight.toFloat())
                .forEachIndexed { k, top -> tops[idx[k]] = top }
        }
        placed.lines = callouts.indices.map { i ->
            val s = spots[i] ?: return@map null
            val boxX = if (leftSide[i]) edge else width - edge - labels[i].width
            val boxInner = if (leftSide[i]) boxX + labels[i].width else boxX
            CalloutLine(Offset(boxInner, tops[i] + labels[i].height / 2f), Offset(s.x, s.y), alphaOf(s))
        }
        layout(constraints.maxWidth, constraints.maxHeight) {
            callouts.indices.forEach { i ->
                val s = spots[i] ?: return@forEach
                val boxX = if (leftSide[i]) edge else width - edge - labels[i].width
                labels[i].placeWithLayer(boxX.roundToInt(), tops[i].roundToInt()) { alpha = alphaOf(s) }
                dots[i].placeWithLayer((s.x - dots[i].width / 2f).roundToInt(), (s.y - dots[i].height / 2f).roundToInt()) { alpha = alphaOf(s) }
            }
        }
    }
}

private fun alphaOf(s: AnchorSpot): Float = if (s.visibility == AnchorVisibility.OCCLUDED) CarSceneTuning.OCCLUDED_ALPHA else 1f

private class CalloutLine(val from: Offset, val to: Offset, val alpha: Float)

private class PlacedCallouts {
    var lines: List<CalloutLine?> by mutableStateOf(emptyList())
}
