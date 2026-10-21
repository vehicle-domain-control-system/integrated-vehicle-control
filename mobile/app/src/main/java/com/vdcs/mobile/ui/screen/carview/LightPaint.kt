package com.vdcs.mobile.ui.screen.carview

import com.vdcs.mobile.ui.LightDisplay
import com.vdcs.mobile.ui.LightPose
import kotlin.math.pow

internal class LightPaint private constructor(val base: FloatArray, val emissive: FloatArray) {
    companion object {
        private const val NEUTRAL_GREY_SRGB = 0.5f
        private val NEUTRAL_GREY_LINEAR = srgbToLinear(NEUTRAL_GREY_SRGB)
        private val NO_EMISSION = floatArrayOf(0f, 0f, 0f)
        private val NEUTRAL = LightPaint(floatArrayOf(NEUTRAL_GREY_LINEAR, NEUTRAL_GREY_LINEAR, NEUTRAL_GREY_LINEAR), NO_EMISSION)

        private const val SRGB_LINEAR_LIMIT = 0.04045f
        private const val SRGB_LINEAR_SLOPE = 12.92f
        private const val SRGB_OFFSET = 0.055f
        private const val SRGB_GAMMA = 2.4
        private const val SRGB_MAX = 255

        fun of(light: LightPose): LightPaint {
            val rgb = light.rgb
            if (light.display != LightDisplay.ON || rgb == null) return NEUTRAL
            val base = floatArrayOf(channel(rgb.r), channel(rgb.g), channel(rgb.b))
            val e = light.intensity * CarSceneTuning.LAMP_EMISSIVE_GAIN * staleFactor(light.stale)
            return LightPaint(base, floatArrayOf(base[0] * e, base[1] * e, base[2] * e))
        }

        fun lampLevel(light: LightPose): Float = if (light.on) light.intensity * staleFactor(light.stale) else 0f

        private fun staleFactor(stale: Boolean): Float = if (stale) CarSceneTuning.STALE_DIM else 1f

        private fun channel(v: Int): Float = srgbToLinear(v.coerceIn(0, SRGB_MAX) / SRGB_MAX.toFloat())

        private fun srgbToLinear(c: Float): Float =
            if (c <= SRGB_LINEAR_LIMIT) {
                c / SRGB_LINEAR_SLOPE
            } else {
                ((c + SRGB_OFFSET) / (1f + SRGB_OFFSET)).toDouble().pow(SRGB_GAMMA).toFloat()
            }
    }
}
