package com.vdcs.mobile.domain.logic

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ReconnectPolicyTest {
    private val p = ReconnectPolicy(maxAttempts = 5, windowMs = 60_000)

    @Test fun `LINK_LOST 만 재시도`() {
        assertTrue(p.shouldRetry(DisconnectCause.LINK_LOST, 1, 0))
        assertFalse(p.shouldRetry(DisconnectCause.USER, 1, 0))
        assertFalse(p.shouldRetry(DisconnectCause.AUTH_FAILED, 1, 0))
    }

    @Test fun `횟수·시간 한도`() {
        assertTrue(p.shouldRetry(DisconnectCause.LINK_LOST, 5, 59_999))
        assertFalse(p.shouldRetry(DisconnectCause.LINK_LOST, 6, 0))
        assertFalse(p.shouldRetry(DisconnectCause.LINK_LOST, 2, 60_000))
    }

    @Test fun `지수 백오프 상한 10초`() {
        assertEquals(listOf(1_000L, 2_000L, 4_000L, 8_000L, 10_000L, 10_000L), (1..6).map(p::nextDelayMs))
    }

    @Test fun `CON-002 - 비활성 화면이면 한도와 무관하게 멈춤, 활성이면 한도 안에서 재시도`() {
        assertEquals(ReconnectDecision.Stop(ReconnectStop.INACTIVE), p.decide(1, 0, foreground = false))
        assertEquals(ReconnectDecision.Stop(ReconnectStop.INACTIVE), p.decide(6, 0, foreground = false))
        assertEquals(ReconnectDecision.Retry(2_000), p.decide(2, 0, foreground = true))
        assertEquals(ReconnectDecision.Stop(ReconnectStop.LIMIT_REACHED), p.decide(6, 0, foreground = true))
        assertEquals(ReconnectDecision.Stop(ReconnectStop.LIMIT_REACHED), p.decide(2, 60_000, foreground = true))
    }
}
