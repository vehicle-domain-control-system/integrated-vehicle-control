package com.vdcs.mobile.ui.rules

object ClimateRules {
    val TARGET_RANGE_C: ClosedFloatingPointRange<Double> = 17.0..27.0

    fun stepTarget(current: Double, steps: Int): Double =
        ValueFormat.stepTarget(current, steps).coerceIn(TARGET_RANGE_C)

    fun targetStepBlockReason(current: Double, steps: Int): String? {
        val min = TARGET_RANGE_C.start
        val max = TARGET_RANGE_C.endInclusive
        return when {
            steps < 0 && current <= min -> "최저 ${ValueFormat.celsius(min)} — 더 내릴 수 없습니다"
            steps > 0 && current >= max -> "최고 ${ValueFormat.celsius(max)} — 더 올릴 수 없습니다"
            else -> null
        }
    }

    fun ringFraction(target: Double): Float {
        val min = TARGET_RANGE_C.start
        val span = TARGET_RANGE_C.endInclusive - min
        return ((target - min) / span).coerceIn(0.0, 1.0).toFloat()
    }
}
