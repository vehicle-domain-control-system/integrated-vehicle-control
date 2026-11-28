package com.vdcs.mobile.ui.screen.carview

internal object CalloutPlacement {
    fun stack(anchorY: List<Float>, heights: List<Int>, gap: Float, maxY: Float): List<Float> {
        val order = anchorY.indices.sortedBy { anchorY[it] }
        val top = FloatArray(anchorY.size)
        var cursor = 0f
        order.forEach { i ->
            top[i] = maxOf(anchorY[i] - heights[i] / 2f, cursor)
            cursor = top[i] + heights[i] + gap
        }
        var limit = maxY
        order.reversed().forEach { i ->
            top[i] = minOf(top[i], limit - heights[i])
            limit = top[i] - gap
        }
        return top.map { maxOf(it, 0f) }
    }
}
