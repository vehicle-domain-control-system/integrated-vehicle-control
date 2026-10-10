package com.vdcs.mobile.ui.screen.carview

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class CalloutPlacementTest {
    private val eps = 0.001f

    @Test fun `떨어진 앵커는 각자 높이 가운데에 놓인다`() {
        assertEquals(listOf(80f, 280f), CalloutPlacement.stack(listOf(100f, 300f), listOf(40, 40), 4f, 1000f))
    }

    @Test fun `가까운 앵커는 겹치지 않게 아래로 밀린다 — 입력 순서 유지`() {
        val tops = CalloutPlacement.stack(listOf(110f, 100f), listOf(40, 40), 4f, 1000f)
        assertEquals(80f, tops[1], eps)
        assertEquals(124f, tops[0], eps)
    }

    @Test fun `아래로 넘치면 위로 되밀고 서로 겹치지 않는다`() {
        val tops = CalloutPlacement.stack(listOf(190f, 195f, 199f), listOf(40, 40, 40), 4f, 200f)
        val sorted = tops.sorted()
        assertTrue(sorted.last() + 40 <= 200f + eps)
        sorted.zipWithNext().forEach { (a, b) -> assertTrue(b - a >= 44f - eps) }
    }
}
