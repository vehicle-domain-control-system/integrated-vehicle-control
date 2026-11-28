package com.vdcs.mobile.ui.rules

import com.vdcs.mobile.domain.model.Qualified
import com.vdcs.mobile.domain.model.Quality
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class QualityRulesTest {
    @Test fun `보여 줄 수 있는 품질은 OK·STALE 뿐`() {
        assertTrue(Quality.OK.isDisplayable)
        assertTrue(Quality.STALE.isDisplayable)
        assertFalse(Quality.INVALID.isDisplayable)
        assertFalse(Quality.NO_DATA.isDisplayable)
    }

    @Test fun `믿을 수 있는 품질은 OK 뿐`() {
        assertEquals(listOf(Quality.OK), Quality.entries.filter { it.isTrusted })
    }

    @Test fun `믿을 수 있으면 반드시 보여 줄 수 있다`() {
        Quality.entries.filter { it.isTrusted }.forEach { assertTrue(it.isDisplayable) }
    }

    @Test fun `worst 는 덜 믿을 쪽 - 순서 OK STALE INVALID NO_DATA, 교환 법칙`() {
        val order = listOf(Quality.OK, Quality.STALE, Quality.INVALID, Quality.NO_DATA)
        for ((i, a) in order.withIndex()) {
            for ((j, b) in order.withIndex()) {
                val expected = order[maxOf(i, j)]
                assertEquals("$a·$b", expected, worst(a, b))
                assertEquals("$b·$a", expected, worst(b, a))
            }
        }
    }

    @Test fun `displayableValue - 미수신·INVALID·NO_DATA 는 null`() {
        assertNull((null as Qualified<Int>?).displayableValue())
        assertEquals(3, Qualified(3, Quality.OK, 0).displayableValue())
        assertEquals(3, Qualified(3, Quality.STALE, 0).displayableValue())
        assertNull(Qualified(3, Quality.INVALID, 0).displayableValue())
        assertNull(Qualified(3, Quality.NO_DATA, 0).displayableValue())
        assertNull(Qualified<Int>(null, Quality.OK, 0).displayableValue())
    }

    @Test fun `값 칸의 shown 은 displayableValue 와 같은 기준`() {
        for (q in Quality.entries) {
            val v = Qualified(7, q, 1_000)
            assertEquals(q.name, v.displayableValue() != null, ValueFormat.value(v, 2_000) { "$it" }.shown)
        }
    }
}
