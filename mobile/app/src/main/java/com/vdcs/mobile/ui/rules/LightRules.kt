package com.vdcs.mobile.ui.rules

import com.vdcs.mobile.domain.model.LightState
import com.vdcs.mobile.domain.model.Rgb

data class LightGlow(val rgb: Rgb, val level: Float)

object LightRules {
    private const val GLOW_OFF_TEXT = "점등 보고 없음"
    private const val APPLIED_NAME = "현재 적용"
    private const val GLOW_NAME = "현재 점등"

    fun glow(light: LightState?): LightGlow? {
        val rgb = light?.rgb.displayableValue() ?: return null
        val percent = light?.brightnessPercent.displayableValue() ?: return null
        if (percent <= 0) return null
        return LightGlow(rgb, percent.coerceAtMost(FULL_PERCENT) / FULL_PERCENT.toFloat())
    }

    fun glowDescription(light: LightState?, nowMs: Long): String {
        if (glow(light) == null) return "$GLOW_NAME · $GLOW_OFF_TEXT"
        val color = ValueFormat.value(light?.rgb, nowMs) { it.label }
        val level = ValueFormat.value(light?.brightnessPercent, nowMs, ValueFormat::percent)
        return listOfNotNull(GLOW_NAME, color.text, level.text, color.note ?: level.note).joinToString(" · ")
    }

    fun appliedText(light: LightState?, nowMs: Long): ValueText {
        val type = ValueFormat.value(light?.activeType, nowMs) { it.label }
        val applied = ValueFormat.value(light?.applied, nowMs) { it.label }
        return ValueText(
            "$APPLIED_NAME ${type.text} · ${applied.text}",
            note = type.note ?: applied.note,
            shown = type.shown && applied.shown,
        )
    }

    private const val FULL_PERCENT = 100
}
