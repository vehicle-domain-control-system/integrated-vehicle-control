package com.vdcs.mobile.ui.screen.carview

import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.colorspace.ColorSpaces
import io.github.sceneview.math.Position

internal object CarSceneTuning {
    const val IBL_INTENSITY = 30_000f

    const val GROUND_SHADOW_WIDTH_M = 2.6f
    const val GROUND_SHADOW_LENGTH_M = 5.6f
    const val GROUND_SHADOW_ALPHA = 0.75f
    const val GROUND_SHADOW_CORE = 0.45f
    const val GROUND_SHADOW_TEXTURE_PX = 128

    const val LAMP_EMISSIVE_GAIN = 4f

    const val LAMP_MAX_LUMENS = 95_000f
    const val LAMP_FALLOFF_M = 1.4f
    val LAMP_POSITION = Position(0f, 1.25f, -0.34f)

    const val CLIMATE_MAX_LUMENS = 95_000f
    const val CLIMATE_FALLOFF_M = 1.2f
    val CLIMATE_POSITION = Position(0f, 0.95f, 0.55f)

    const val STALE_DIM = 0.4f

    const val OCCLUDED_ALPHA = 0.35f

    const val PROJECTION_EPSILON_PX = 0.5f
}

internal fun Color.toLinearRgb(): FloatArray = convert(ColorSpaces.LinearSrgb).let { floatArrayOf(it.red, it.green, it.blue) }
