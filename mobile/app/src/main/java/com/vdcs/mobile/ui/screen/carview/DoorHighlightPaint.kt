package com.vdcs.mobile.ui.screen.carview

import android.util.Log
import androidx.compose.ui.graphics.Color
import com.google.android.filament.Engine
import com.google.android.filament.MaterialInstance
import com.vdcs.mobile.ui.CarDoor
import com.vdcs.mobile.ui.DoorPose
import io.github.sceneview.node.RenderableNode

internal class DoorHighlightPaint(private val engine: Engine, doors: Map<CarDoor, RenderableNode>, highlight: Color) {
    private class Slot(val node: RenderableNode, val primitive: Int, val original: MaterialInstance, val highlight: MaterialInstance)

    private val slots: Map<CarDoor, List<Slot>> = doors.mapValues { (door, node) -> paintSlots(door, node) }
    private val highlightRgb = highlight.toLinearRgb()
    private var applied: Pair<List<CarDoor>, Boolean>? = null

    fun show(door: DoorPose) {
        val target = door.openDoors to door.stale
        if (target == applied) return
        applied = target
        val dim = if (door.stale) CarSceneTuning.STALE_DIM else 1f
        slots.forEach { (d, list) ->
            val on = d in door.openDoors
            list.forEach { slot ->
                if (on) paint(slot.highlight, dim)
                slot.node.setMaterialInstanceAt(slot.primitive, if (on) slot.highlight else slot.original)
            }
        }
    }

    fun release() {
        slots.values.flatten().forEach { slot ->
            slot.node.setMaterialInstanceAt(slot.primitive, slot.original)
            engine.destroyMaterialInstance(slot.highlight)
        }
    }

    private fun paint(mi: MaterialInstance, dim: Float) {
        if (!mi.material.hasParameter(BASE_COLOR_PARAM)) return
        mi.setParameter(BASE_COLOR_PARAM, highlightRgb[0] * dim, highlightRgb[1] * dim, highlightRgb[2] * dim, 1f)
    }

    private fun paintSlots(door: CarDoor, node: RenderableNode): List<Slot> {
        val slots = node.materialInstances.withIndex()
            .filter { it.value.name == PAINT_MATERIAL }
            .map { (i, mi) -> Slot(node, i, mi, MaterialInstance.duplicate(mi, "${PAINT_MATERIAL}_${door.nodeName}")) }
        if (slots.isEmpty()) Log.w(LOG_TAG, "${door.nodeName} 에 $PAINT_MATERIAL 재질이 없어 강조색을 칠할 수 없음")
        return slots
    }

    private companion object {
        const val LOG_TAG = "VdcsCarView"
        const val PAINT_MATERIAL = "Paint"
        const val BASE_COLOR_PARAM = "baseColorFactor"
    }
}
