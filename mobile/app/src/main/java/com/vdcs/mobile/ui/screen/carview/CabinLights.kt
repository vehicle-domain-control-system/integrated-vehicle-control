package com.vdcs.mobile.ui.screen.carview

import com.google.android.filament.Engine
import com.google.android.filament.LightManager
import com.vdcs.mobile.domain.model.TempDirection
import com.vdcs.mobile.ui.CabinGlow
import com.vdcs.mobile.ui.LightPose
import com.vdcs.mobile.ui.theme.SceneColors
import io.github.sceneview.math.Color
import io.github.sceneview.math.Position
import io.github.sceneview.node.LightNode
import io.github.sceneview.node.Node

internal class CabinLights(engine: Engine, colors: SceneColors) {
    private val lamp = pointLight(engine, CarSceneTuning.LAMP_POSITION, CarSceneTuning.LAMP_FALLOFF_M)
    private val climate = pointLight(engine, CarSceneTuning.CLIMATE_POSITION, CarSceneTuning.CLIMATE_FALLOFF_M)
    private val coolRgb = colors.climateCool.toLinearRgb()
    private val heatRgb = colors.climateHeat.toLinearRgb()

    val nodes: List<Node> get() = listOf(lamp, climate)

    fun show(light: LightPose, glow: CabinGlow?) {
        lamp.color = rgb(LightPaint.of(light).base)
        lamp.intensity = CarSceneTuning.LAMP_MAX_LUMENS * LightPaint.lampLevel(light)

        if (glow == null) {
            climate.intensity = 0f
            return
        }
        climate.color = rgb(if (glow.direction == TempDirection.HEAT) heatRgb else coolRgb)
        val dim = if (glow.stale) CarSceneTuning.STALE_DIM else 1f
        climate.intensity = CarSceneTuning.CLIMATE_MAX_LUMENS * glow.strength * dim
    }

    fun destroy() {
        lamp.destroy()
        climate.destroy()
    }

    private fun rgb(c: FloatArray) = Color(c[0], c[1], c[2], 1f)

    private fun pointLight(engine: Engine, at: Position, falloffM: Float) = LightNode(engine, LightManager.Type.POINT) {
        position(at.x, at.y, at.z)
        falloff(falloffM)
        intensity(0f)
        castShadows(false)
    }
}
