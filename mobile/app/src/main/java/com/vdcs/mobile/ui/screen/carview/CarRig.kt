package com.vdcs.mobile.ui.screen.carview

import com.google.android.filament.MaterialInstance
import com.vdcs.mobile.ui.CarDoor
import com.vdcs.mobile.ui.CarPose
import com.vdcs.mobile.ui.DoorPose
import com.vdcs.mobile.ui.LightPose
import com.vdcs.mobile.ui.theme.SceneColors
import dev.romainguy.kotlin.math.Quaternion
import io.github.sceneview.node.ModelNode
import io.github.sceneview.node.RenderableNode
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.sign
import kotlin.math.sin

internal class CarRig(model: ModelNode, colors: SceneColors) {
    private val doors: Map<CarDoor, RenderableNode> = CarDoor.entries.mapNotNull { door ->
        model.renderableNodes.firstOrNull { it.name == door.nodeName }?.let { door to it }
    }.toMap()
    private val lightMaterials: List<MaterialInstance> =
        model.renderableNodes.firstOrNull { it.name == INTERIOR_LIGHT_NODE }?.materialInstances.orEmpty()

    private val doorPaint = DoorHighlightPaint(model.engine, doors, colors.doorHighlight)

    private val currentDeg = FloatArray(CarDoor.entries.size)
    private var doorTarget: DoorPose? = null
    private var appliedLight: LightPose? = null
    private var lastFrameNanos = 0L

    fun setTarget(pose: CarPose) {
        doorTarget = pose.door
        doorPaint.show(pose.door)
        if (pose.light != appliedLight) {
            appliedLight = pose.light
            applyLight(LightPaint.of(pose.light))
        }
    }

    fun stepDoors(frameTimeNanos: Long) {
        val dt = if (lastFrameNanos == 0L) 0f else ((frameTimeNanos - lastFrameNanos) / 1e9f).coerceIn(0f, MAX_FRAME_DT_S)
        lastFrameNanos = frameTimeNanos
        val target = doorTarget ?: return
        val maxStep = CarPose.DOOR_OPEN_DEG * dt / DOOR_ANIM_S
        for ((door, node) in doors) {
            val i = door.ordinal
            val goal = target.angleOf(door)
            val cur = currentDeg[i]
            if (cur == goal) continue
            val diff = goal - cur
            val next = if (abs(diff) <= maxStep) goal else cur + sign(diff) * maxStep
            currentDeg[i] = next
            node.quaternion = yRotation(next)
        }
    }

    fun release() = doorPaint.release()

    private fun applyLight(paint: LightPaint) {
        lightMaterials.forEach { mi ->
            val material = mi.material
            if (material.hasParameter(BASE_COLOR_PARAM)) {
                mi.setParameter(BASE_COLOR_PARAM, paint.base[0], paint.base[1], paint.base[2], 1f)
            }
            if (material.hasParameter(EMISSIVE_PARAM)) {
                mi.setParameter(EMISSIVE_PARAM, paint.emissive[0], paint.emissive[1], paint.emissive[2])
            }
        }
    }

    private fun yRotation(deg: Float): Quaternion {
        val half = Math.toRadians(deg.toDouble()).toFloat() / 2f
        return Quaternion(0f, sin(half), 0f, cos(half))
    }

    private companion object {
        const val INTERIOR_LIGHT_NODE = "InteriorLight"
        const val BASE_COLOR_PARAM = "baseColorFactor"
        const val EMISSIVE_PARAM = "emissiveFactor"
        const val DOOR_ANIM_S = 0.3f
        const val MAX_FRAME_DT_S = 0.1f
    }
}
