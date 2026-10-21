package com.vdcs.mobile.ui.screen.carview

import androidx.compose.runtime.Immutable
import com.vdcs.mobile.ui.CarAnchor
import dev.romainguy.kotlin.math.dot
import io.github.sceneview.math.Position
import io.github.sceneview.node.CameraNode
import io.github.sceneview.utils.worldToScreen
import kotlin.math.abs
import com.google.android.filament.View as FilamentView

internal enum class AnchorVisibility { VISIBLE, OCCLUDED, HIDDEN }

@Immutable
internal data class AnchorSpot(val x: Float, val y: Float, val visibility: AnchorVisibility)

@Immutable
internal class AnchorLayout(private val spots: Map<CarAnchor, AnchorSpot>) {
    fun spotOf(anchor: CarAnchor): AnchorSpot? = spots[anchor]

    companion object {
        val NONE = AnchorLayout(emptyMap())
    }
}

internal class AnchorProjector {
    private val anchors = CarAnchor.entries
    private val positions = anchors.map { Position(it.point.x, it.point.y, it.point.z) }
    private var last: AnchorLayout = AnchorLayout.NONE

    fun project(view: FilamentView, camera: CameraNode): AnchorLayout? {
        val width = view.viewport.width
        val height = view.viewport.height
        if (width <= 0 || height <= 0) return null
        val eye = camera.worldPosition
        val forward = camera.forwardDirection
        val spots = anchors.indices.associate { i ->
            anchors[i] to spot(view, anchors[i], positions[i], eye, forward, camera.near, width, height)
        }
        if (anchors.all { same(last.spotOf(it), spots.getValue(it)) }) return null
        return AnchorLayout(spots).also { last = it }
    }

    private fun spot(
        view: FilamentView,
        anchor: CarAnchor,
        at: Position,
        eye: Position,
        forward: Position,
        near: Float,
        width: Int,
        height: Int,
    ): AnchorSpot {
        if (dot(at - eye, forward) <= near) return AnchorSpot(0f, 0f, AnchorVisibility.HIDDEN)
        val screen = view.worldToScreen(at)
        val onScreen = screen.x in 0f..width.toFloat() && screen.y in 0f..height.toFloat()
        val facesAway = anchor.outward?.let { dot(Position(it.x, it.y, it.z), eye - at) < 0f } ?: false
        val visibility = when {
            !onScreen -> AnchorVisibility.HIDDEN
            facesAway -> AnchorVisibility.OCCLUDED
            else -> AnchorVisibility.VISIBLE
        }
        return AnchorSpot(screen.x, screen.y, visibility)
    }

    private fun same(a: AnchorSpot?, b: AnchorSpot): Boolean =
        a != null && a.visibility == b.visibility &&
            abs(a.x - b.x) < CarSceneTuning.PROJECTION_EPSILON_PX && abs(a.y - b.y) < CarSceneTuning.PROJECTION_EPSILON_PX
}
