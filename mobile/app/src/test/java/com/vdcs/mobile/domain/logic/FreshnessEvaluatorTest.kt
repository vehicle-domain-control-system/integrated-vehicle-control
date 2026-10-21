package com.vdcs.mobile.domain.logic

import com.vdcs.mobile.domain.model.Quality
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class FreshnessEvaluatorTest {
    private val f = FreshnessEvaluator()

    @Test fun `OK 도 수신 중단 기준을 넘기면 STALE`() {
        assertEquals(Quality.OK, f.evaluate(Quality.OK, receivedAtMs = 0, sourceAgeMs = 0, limitMs = 600, nowMs = 600))
        assertEquals(Quality.STALE, f.evaluate(Quality.OK, 0, 0, 600, 601))
    }

    @Test fun `INVALID·NO_DATA·STALE 는 그대로 - 올려 주지 않는다`() {
        for (q in listOf(Quality.INVALID, Quality.NO_DATA, Quality.STALE)) {
            assertEquals(q, f.evaluate(q, 0, 0, 600, 0))
            assertEquals(q, f.evaluate(q, 0, 0, 600, 10_000))
        }
    }

    @Test fun `수신 중단 판정`() {
        assertFalse(f.isReceptionLost(1_000, 0, 3_000, 4_000))
        assertTrue(f.isReceptionLost(1_000, 0, 3_000, 4_001))
    }

    @Test fun `SEM-007 - 차량이 보고한 원본 경과를 App 수신 경과에 더해 판정`() {
        assertEquals(Quality.OK, f.evaluate(Quality.OK, receivedAtMs = 0, sourceAgeMs = 400, limitMs = 600, nowMs = 200))
        assertEquals(Quality.STALE, f.evaluate(Quality.OK, receivedAtMs = 0, sourceAgeMs = 400, limitMs = 600, nowMs = 201))
        assertEquals(Quality.STALE, f.evaluate(Quality.OK, receivedAtMs = 0, sourceAgeMs = 700, limitMs = 600, nowMs = 0))
        assertTrue(f.isReceptionLost(1_000, 2_500, 3_000, 1_501))
    }
}
