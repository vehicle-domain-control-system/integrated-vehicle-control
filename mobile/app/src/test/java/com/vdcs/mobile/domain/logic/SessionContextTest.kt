package com.vdcs.mobile.domain.logic

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class SessionContextTest {
    @Test fun `APP_INSTANCE_ID 는 0 이 아닌 u32`() {
        val values = ArrayDeque(listOf(0L, 0x1_0000_0000L, 0x1234L))
        val s = SessionContext { values.removeFirst() }
        s.newAppInstance()
        assertEquals(0x1234L, s.appInstanceId)
    }

    private fun ready(s: SessionContext, session: Long = 10, boot: Long = 3) =
        s.onGatewayStatus(true, true, true, 7, session, boot)

    @Test fun `Gateway 거울 - 등록·링크·SESSION_READY 를 그대로 반영`() {
        val s = SessionContext { 1 }
        assertEquals(ContextChange.FIRST, ready(s))
        assertTrue(s.registered && s.domainLinkUp && s.sessionReady && s.hasContext())
        s.onGatewayStatus(true, true, false, 7, 10, 3)
        assertFalse(s.sessionReady)
    }

    @Test fun `SESSION_ID·DOMAIN_BOOT_ID 변경은 CHANGED`() {
        val s = SessionContext { 1 }
        ready(s)
        assertEquals(ContextChange.SAME, ready(s))
        assertEquals(ContextChange.CHANGED, ready(s, boot = 4))
        assertEquals(ContextChange.CHANGED, ready(s, session = 11, boot = 4))
    }

    @Test fun `문맥 대조 - 현재가 미확인(0)이면 적용 안 함`() {
        val s = SessionContext { 1 }
        assertFalse(s.matches(0, 0, 0))
        ready(s)
        assertTrue(s.matches(7, 10, 3))
        assertFalse(s.matches(7, 10, 4))
        s.onGatewayStatus(true, true, false, 7, 10, 0)
        assertFalse(s.matches(7, 10, 0))
    }

    @Test fun `링크 끊김 - 세션 기억은 남기되 READY 해제`() {
        val s = SessionContext { 1 }
        ready(s)
        s.onLinkLost()
        assertFalse(s.sessionReady)
        assertFalse(s.domainLinkUp)
        assertNotEquals(0L, s.sessionId)
    }

    @Test fun `SESSION_ID 만 바뀌어도 CHANGED`() {
        val s = SessionContext { 1 }
        ready(s)
        assertEquals(ContextChange.CHANGED, ready(s, session = 11))
        assertTrue(s.matches(7, 11, 3))
    }

    @Test fun `DEVICE_CONTEXT_ID 변경도 CHANGED`() {
        val s = SessionContext { 1 }
        ready(s)
        assertEquals(ContextChange.CHANGED, s.onGatewayStatus(true, true, true, 8, 10, 3))
        assertTrue(s.matches(8, 10, 3))
        assertFalse(s.matches(7, 10, 3))
    }

    @Test fun `프로세스 시작 직후 newAppInstance 는 FIRST 흐름을 막지 않는다`() {
        val s = SessionContext { 0x55 }
        s.newAppInstance()
        assertFalse(s.contextPending)
        assertEquals(ContextChange.FIRST, ready(s))
        assertTrue(s.matches(7, 10, 3))
        assertTrue(s.hasContext())
    }

    @Test fun `newAppInstance 뒤에는 이전 문맥 하행·요청 발급 금지 - 새 Gateway 문맥이 와야 해제`() {
        val ids = ArrayDeque(listOf(0x11L, 0x22L))
        val s = SessionContext { ids.removeFirst() }
        s.newAppInstance()
        ready(s)
        assertTrue(s.hasContext())

        s.newAppInstance()
        assertEquals(0x22L, s.appInstanceId)
        assertTrue(s.contextPending)
        assertFalse(s.matches(7, 10, 3))
        assertFalse(s.sessionReady)
        assertFalse(s.hasContext())
        assertEquals(10L, s.sessionId)

        assertEquals(ContextChange.SAME, ready(s))
        assertTrue(s.contextPending)
        assertFalse(s.matches(7, 10, 3))
        assertFalse(s.sessionReady)
        assertFalse(s.hasContext())

        assertEquals(ContextChange.CHANGED, ready(s, session = 12))
        assertFalse(s.contextPending)
        assertTrue(s.matches(7, 12, 3))
        assertFalse(s.matches(7, 10, 3))
        assertTrue(s.hasContext())
    }
}
