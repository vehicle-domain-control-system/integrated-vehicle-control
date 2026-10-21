package com.vdcs.mobile.domain.logic

import com.vdcs.mobile.domain.logic.SessionEvent.AppInstanceRenewed
import com.vdcs.mobile.domain.logic.SessionEvent.AppStateWritten
import com.vdcs.mobile.domain.logic.SessionEvent.AuthRejected
import com.vdcs.mobile.domain.logic.SessionEvent.ConnectRequested
import com.vdcs.mobile.domain.logic.SessionEvent.GatewayUpdated
import com.vdcs.mobile.domain.logic.SessionEvent.LinkConnecting
import com.vdcs.mobile.domain.logic.SessionEvent.LinkLost
import com.vdcs.mobile.domain.logic.SessionEvent.LinkUp
import com.vdcs.mobile.domain.logic.SessionEvent.SyncCompleted
import com.vdcs.mobile.domain.logic.SessionEvent.SyncFailed
import com.vdcs.mobile.domain.logic.SessionPhase.ContextWait
import com.vdcs.mobile.domain.logic.SessionPhase.Disconnected
import com.vdcs.mobile.domain.logic.SessionPhase.Linking
import com.vdcs.mobile.domain.logic.SessionPhase.QuerySync
import com.vdcs.mobile.domain.logic.SessionPhase.Ready
import com.vdcs.mobile.domain.logic.SessionPhase.SessionReadyWait
import com.vdcs.mobile.domain.logic.SessionPhase.Subscribed
import com.vdcs.mobile.domain.model.ConnectionState
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test

class SessionStateMachineTest {
    private val ctx = ContextKey(7, 10, 3)
    private val none = SessionFacts(context = null, registered = false, domainLinkUp = false, sessionReady = false)
    private val context = SessionFacts(context = ctx, registered = true, domainLinkUp = true, sessionReady = false)
    private val ready = context.copy(sessionReady = true)
    private val gw = GatewayUpdated(contextChanged = false, notRegistered = false)

    private fun SessionState.on(e: SessionEvent, f: SessionFacts = none) = SessionStateMachine.next(this, e, f)
    private fun at(p: SessionPhase, auth: String? = null, synced: ContextKey? = null) = SessionState(p, auth, synced)

    private fun readyState(f: SessionFacts = ready) =
        SessionState().on(LinkUp).on(AppStateWritten(true), f).on(SyncCompleted, f)

    @Test fun `§46 순서 - Linking, Subscribed, ContextWait, QuerySync, SessionReadyWait, Ready`() {
        var s = SessionState()
        assertEquals(ConnectionState.DISCONNECTED, s.connectionState)
        assertNull(s.detail)

        s = s.on(LinkConnecting("연결 중", 1))
        assertEquals(Linking("연결 중", 1), s.phase)
        assertEquals(ConnectionState.CONNECTING, s.connectionState)

        s = s.on(LinkUp)
        assertEquals(Subscribed(appStateFailed = false), s.phase)
        assertEquals(ConnectionState.CONNECTED, s.connectionState)

        s = s.on(AppStateWritten(ok = true), none)
        assertEquals(ContextWait(ContextGap.CONTEXT), s.phase)

        s = s.on(gw, context)
        assertEquals(QuerySync(), s.phase)
        assertEquals("초기 상태 동기화 중", s.detail)

        s = s.on(SyncCompleted, context)
        assertEquals(SessionReadyWait, s.phase)
        assertEquals("차량 세션 준비 중", s.detail)
        assertFalse(s.canIssueRequests)

        s = s.on(gw, ready)
        assertEquals(Ready, s.phase)
        assertEquals(ConnectionState.AUTHENTICATED, s.connectionState)
        assertNull(s.detail)
        assertTrue(s.canIssueRequests)
    }

    @Test fun `READY 는 등록·링크·SESSION_READY·초기 Query 완결이 모두 필요`() {
        val beforeSync = SessionState().on(LinkUp).on(AppStateWritten(true), ready)
        assertFalse(beforeSync.canIssueRequests)
        val r = beforeSync.on(SyncCompleted, ready)
        assertTrue(r.canIssueRequests)
        assertFalse(r.on(gw, ready.copy(sessionReady = false)).canIssueRequests)
        assertFalse(r.on(gw, ready.copy(registered = false)).canIssueRequests)
        assertFalse(r.on(gw, ready.copy(domainLinkUp = false)).canIssueRequests)
        assertFalse(r.on(gw, ready.copy(context = null)).canIssueRequests)
    }

    @Test fun `문맥(SESSION_ID·DOMAIN_BOOT_ID·DEVICE_CONTEXT_ID)이 바뀌면 다시 초기 Query`() {
        for (changed in listOf(ctx.copy(sessionId = 11), ctx.copy(domainBootId = 4), ctx.copy(deviceContextId = 8))) {
            val s = readyState().on(GatewayUpdated(contextChanged = true, notRegistered = false), ready.copy(context = changed))
            assertEquals(QuerySync(), s.phase)
            assertFalse(s.canIssueRequests)
            assertEquals(ConnectionState.CONNECTED, s.connectionState)
        }
        assertEquals(Ready, readyState().on(gw, ready).phase)
    }

    @Test fun `링크가 끊기면 같은 SESSION_ID 로 다시 이어져도 초기 Query 부터`() {
        val relinked = readyState().on(LinkLost(DisconnectCause.LINK_LOST, null)).on(LinkUp).on(AppStateWritten(true), ready)
        assertEquals(QuerySync(), relinked.phase)
        assertNull(relinked.syncedContext)
    }

    @Test fun `새 APP_INSTANCE_ID - 새 문맥이 올 때까지 막고, 오면 초기 Query 부터`() {
        val pending = readyState().on(AppInstanceRenewed, none)
        assertEquals(ContextWait(ContextGap.CONTEXT), pending.phase)
        assertFalse(pending.canIssueRequests)
        val newCtx = ready.copy(context = ctx.copy(sessionId = 12))
        val fresh = pending.on(GatewayUpdated(contextChanged = true, notRegistered = false), newCtx)
        assertEquals(QuerySync(), fresh.phase)
        assertTrue(fresh.on(SyncCompleted, newCtx).canIssueRequests)
        assertEquals(Subscribed(true), at(Subscribed(true)).on(AppInstanceRenewed, none).phase)
    }

    @Test fun `App State 가 받아들여지기 전에는 Gateway 문맥이 와도 진행하지 않고 조회도 못 한다`() {
        val s = at(Subscribed(false)).on(gw, ready)
        assertEquals(Subscribed(false), s.phase)
        assertFalse(s.canQuery)
        assertNull(s.pendingSync)
    }

    @Test fun `App State 실패는 어느 링크 단계에서든 Subscribed(실패) - 성공하면 사실대로 복귀`() {
        for (p in listOf(ContextWait(ContextGap.CONTEXT), QuerySync(), SessionReadyWait, Ready)) {
            val failed = at(p, synced = ctx).on(AppStateWritten(ok = false), ready)
            assertEquals(Subscribed(appStateFailed = true), failed.phase)
            assertEquals(SessionPhase.APP_STATE_FAILED_TEXT, failed.detail)
        }
        assertEquals(Ready, at(Subscribed(true), synced = ctx).on(AppStateWritten(ok = true), ready).phase)
        assertEquals(QuerySync(), at(Subscribed(true)).on(AppStateWritten(ok = true), context).phase)
        assertEquals(QuerySync(2, 500, "x"), at(QuerySync(2, 500, "x")).on(AppStateWritten(ok = true), context).phase)
    }

    @Test fun `CONTEXT_WAIT 사유 순서 - 문맥, 등록, 도메인 링크`() {
        assertEquals(ContextWait(ContextGap.CONTEXT), at(ContextWait(ContextGap.CONTEXT)).on(gw, none).phase)
        val unregistered = at(ContextWait(ContextGap.CONTEXT)).on(gw, context.copy(registered = false))
        assertEquals(ContextWait(ContextGap.REGISTRATION), unregistered.phase)
        assertEquals(SessionPhase.NOT_REGISTERED_TEXT, unregistered.detail)
        val linkDown = readyState().on(gw, ready.copy(domainLinkUp = false))
        assertEquals(ContextWait(ContextGap.DOMAIN_LINK), linkDown.phase)
        assertEquals("차량 중앙 연결 대기 중", linkDown.detail)
        assertFalse(linkDown.canQuery)
        assertEquals(Ready, linkDown.on(gw, ready).phase)
    }

    @Test fun `동기화 실패 - 시도 수·재시도 시각·사유를 남기고, 같은 문맥 Gateway 는 이어 간다`() {
        val failed = at(QuerySync()).on(SyncFailed(SessionPhase.SYNC_INCOMPLETE_TEXT, 2_000), context)
        assertEquals(QuerySync(2, 2_000, SessionPhase.SYNC_INCOMPLETE_TEXT), failed.phase)
        assertEquals(SessionPhase.SYNC_INCOMPLETE_TEXT, failed.detail)
        assertEquals(failed.phase, failed.on(gw, context).phase)
        assertEquals(QuerySync(), failed.on(GatewayUpdated(contextChanged = true, notRegistered = false), context).phase)
        assertSame(Ready, at(Ready).on(SyncFailed("x", 1), ready).phase)
    }

    @Test fun `조회 가능 단계 - QUERY_SYNC 이후만`() {
        assertTrue(at(QuerySync()).canQuery)
        assertTrue(at(SessionReadyWait).canQuery)
        assertTrue(at(Ready).canQuery)
        for (p in listOf(Disconnected(null, null), Linking("x", 1), Subscribed(false), Subscribed(true), ContextWait(ContextGap.DOMAIN_LINK))) {
            assertFalse("$p", at(p).canQuery)
        }
    }

    @Test fun `D13 - Gateway NOT_REGISTERED·ATT 0x86 은 overlay, REGISTERED 알림이 풀지 않는다`() {
        val viaGateway = readyState().on(GatewayUpdated(contextChanged = false, notRegistered = true), ready)
        assertTrue(viaGateway.blocked)
        assertEquals(SessionPhase.NOT_REGISTERED_TEXT, viaGateway.detail)
        val viaAtt = readyState().on(AuthRejected(SessionPhase.NOT_REGISTERED_TEXT), ready)
        assertTrue(viaAtt.blocked)
        assertEquals(ConnectionState.CONNECTED, viaAtt.connectionState)
        assertFalse(viaAtt.canIssueRequests)
        assertFalse(viaAtt.canQuery)

        val still = viaAtt.on(gw, ready)
        assertTrue(still.blocked)
        assertEquals(SessionPhase.NOT_REGISTERED_TEXT, still.detail)
    }

    @Test fun `overlay 아래 단계는 그대로 진행한다 - 동기화는 막고, 해제하면 그 단계가 드러난다`() {
        val blocked = SessionState().on(AuthRejected("미등록")).on(LinkUp).on(AppStateWritten(true), context)
        assertEquals(QuerySync(), blocked.phase)
        assertNull(blocked.pendingSync)
        assertEquals("미등록", blocked.detail)
        val cleared = blocked.on(ConnectRequested, context)
        assertFalse(cleared.blocked)
        assertEquals(QuerySync(), cleared.pendingSync)
    }

    @Test fun `overlay 중 App State 실패는 기억된다 - connect 로 풀어도 App State 확인 없이 READY 가 되지 않는다`() {
        val blocked = readyState().on(AuthRejected("미등록"), ready).on(AppStateWritten(ok = false), ready)
        assertEquals(Subscribed(appStateFailed = true), blocked.phase)
        assertEquals("미등록", blocked.detail)
        val cleared = blocked.on(ConnectRequested, ready)
        assertEquals(Subscribed(appStateFailed = true), cleared.phase)
        assertFalse(cleared.canIssueRequests)
        assertEquals(Ready, cleared.on(AppStateWritten(ok = true), ready).phase)
    }

    @Test fun `AUTH_FAILED 는 끊겼다 다시 이어져도 남고 사용자 connect 만 푼다`() {
        val blocked = readyState().on(AuthRejected("등록 안 됨"), ready)
        val lost = blocked.on(LinkLost(DisconnectCause.LINK_LOST, "신호 약함"))
        assertFalse(lost.blocked)
        assertEquals("신호 약함", lost.detail)
        assertEquals("등록 안 됨", lost.on(LinkUp).detail)

        val cleared = blocked.on(ConnectRequested, ready)
        assertNull(cleared.authFailure)
        assertEquals(Ready, cleared.phase)
        assertTrue(cleared.canIssueRequests)
        val relinked = lost.on(ConnectRequested).on(LinkUp)
        assertEquals(Subscribed(false), relinked.phase)
        assertFalse(relinked.blocked)
    }

    @Test fun `끊김 사유 한 줄 - 원인별`() {
        assertEquals("연결 해제됨", at(Disconnected(DisconnectCause.USER, null)).detail)
        assertEquals("연결 끊김", at(Disconnected(DisconnectCause.LINK_LOST, null)).detail)
        assertTrue(at(Disconnected(DisconnectCause.AUTH_FAILED, null)).detail!!.startsWith("등록 실패"))
        assertTrue(at(Disconnected(DisconnectCause.BOND_MISMATCH, null)).detail!!.startsWith("재등록 필요"))
        assertEquals(SessionPhase.NOT_CONNECTED_TEXT, at(Disconnected(null, null)).blockReason)
        assertEquals("연결 중 (재시도 3)", at(Linking("연결 중", 3)).detail)
    }

    @Test fun `링크가 없을 때의 App State·Gateway·동기화 사건은 단계를 바꾸지 않는다`() {
        val off = at(Disconnected(DisconnectCause.USER, null))
        assertSame(off, off.on(AppStateWritten(ok = false)))
        assertEquals(off.phase, off.on(SyncCompleted, ready).phase)
        assertEquals(off.phase, off.on(GatewayUpdated(true, false), ready).phase)
        val relinked = off.on(GatewayUpdated(false, true)).on(LinkUp)
        assertTrue(relinked.blocked)
        assertEquals(SessionPhase.NOT_REGISTERED_TEXT, relinked.detail)
    }

    @Test fun `CON-002 - 재연결 한도·비활성 문구가 링크 사유에 덮이지 않는다`() {
        val limit = SessionState().on(LinkLost(DisconnectCause.LINK_LOST, "연결 끊김 (status 8)", ReconnectStop.LIMIT_REACHED))
        assertEquals("${SessionPhase.RECONNECT_LIMIT_TEXT} (연결 끊김 (status 8))", limit.detail)
        val limitNoDetail = SessionState().on(LinkLost(DisconnectCause.LINK_LOST, null, ReconnectStop.LIMIT_REACHED))
        assertEquals(SessionPhase.RECONNECT_LIMIT_TEXT, limitNoDetail.detail)
        val inactive = SessionState().on(LinkLost(DisconnectCause.LINK_LOST, null, ReconnectStop.INACTIVE))
        assertEquals(SessionPhase.RECONNECT_INACTIVE_TEXT, inactive.detail)
        assertEquals("신호 약함", SessionState().on(LinkLost(DisconnectCause.LINK_LOST, "신호 약함")).detail)
        assertEquals(ConnectionState.DISCONNECTED, limit.connectionState)
    }
}
