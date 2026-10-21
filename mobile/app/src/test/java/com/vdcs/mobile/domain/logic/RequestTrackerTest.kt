package com.vdcs.mobile.domain.logic

import com.vdcs.mobile.domain.model.FanLevel
import com.vdcs.mobile.domain.model.RequestState
import com.vdcs.mobile.domain.model.ResultReason
import com.vdcs.mobile.domain.model.TrackedRequest
import com.vdcs.mobile.domain.model.UserRequest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test

class RequestTrackerTest {
    private val s1 = 0x1111L
    private val s2 = 0x2222L
    private val deadlines = ResultDeadlines(firstResponseMs = 1_000, settingMs = 2_000, doorMs = 3_000, climateMs = 5_000)
    private val roomy = 1_000
    private val tracker = RequestTracker(deadlines, roomy)

    @Test fun `ID 는 1부터 단조 증가 - 0 없음`() {
        val a = tracker.create(UserRequest.Door(true), s1, 0)
        val b = tracker.create(UserRequest.Door(false), s1, 0)
        assertEquals(1L, a.requestId)
        assertEquals(2L, b.requestId)
        assertEquals(RequestState.SENT, a.state)
    }

    @Test fun `Result 는 REQUEST_SESSION 까지 맞아야 연결된다`() {
        val r = tracker.create(UserRequest.Door(true), s1, 0)
        assertNull(tracker.onResult(s2, r.requestId, RequestState.DONE, null, true, 10))
        assertEquals(RequestState.SENT, tracker.snapshot().single().state)
        assertEquals(RequestState.DONE, tracker.onResult(s1, r.requestId, RequestState.DONE, null, true, 10)!!.state)
    }

    @Test fun `CANCELLED 포함 종결 뒤 늦은 ACCEPTED·IN_PROGRESS 는 무시`() {
        for (final in RequestTracker.FINAL) {
            val t = RequestTracker(deadlines, roomy)
            val r = t.create(UserRequest.Door(true), s1, 0)
            t.onResult(s1, r.requestId, final, null, true, 5)
            assertNull(t.onResult(s1, r.requestId, RequestState.IN_PROGRESS, null, true, 6))
            assertNull(t.onResult(s1, r.requestId, RequestState.ACCEPTED, null, true, 7))
            assertEquals(final, t.snapshot().single().state)
            assertEquals(5L, t.snapshot().single().updatedAtMs)
        }
    }

    @Test fun `종결 뒤 다른 종결값은 상태를 바꾸지 않고 결과 불일치로 표시 - REQ-005`() {
        for (final in RequestTracker.FINAL) {
            for (other in RequestTracker.FINAL - final) {
                val t = RequestTracker(deadlines, roomy)
                val r = t.create(UserRequest.Door(true), s1, 0)
                t.onResult(s1, r.requestId, final, null, confirmed = false, nowMs = 5)
                assertFalse(t.snapshot().single().resultMismatch)
                val flagged = t.onResult(s1, r.requestId, other, null, confirmed = true, nowMs = 6)!!
                assertEquals(final, flagged.state)
                assertFalse(flagged.confirmed)
                assertTrue(flagged.resultMismatch)
                assertNull(t.onResult(s1, r.requestId, other, null, confirmed = true, nowMs = 7))
                assertEquals(flagged, t.snapshot().single())
            }
        }
    }

    @Test fun `같은 종결 상태의 UNCONFIRMED 에서 CONFIRMED 승격은 받는다 - 강등은 무시`() {
        for (final in RequestTracker.FINAL) {
            val t = RequestTracker(deadlines, roomy)
            val r = t.create(UserRequest.Door(true), s1, 0)
            t.onResult(s1, r.requestId, final, ResultReason.NONE, confirmed = false, nowMs = 5)
            val promoted = t.onResult(s1, r.requestId, final, ResultReason.NONE, confirmed = true, nowMs = 8)!!
            assertEquals(final, promoted.state)
            assertTrue(promoted.confirmed)
            assertEquals(8L, promoted.updatedAtMs)
            assertNull(t.onResult(s1, r.requestId, final, ResultReason.NONE, confirmed = false, nowMs = 9))
            assertNull(t.onResult(s1, r.requestId, final, ResultReason.NONE, confirmed = true, nowMs = 10))
            assertTrue(t.snapshot().single().confirmed)
            assertEquals(8L, t.snapshot().single().updatedAtMs)
        }
    }

    @Test fun `UNCONFIRMED 는 FAILED 로 바꾸지 않는다 - 만료·세션 변경을 거쳐도`() {
        val inProgress = tracker.create(UserRequest.Door(true), s1, 0)
        val done = tracker.create(UserRequest.Door(false), s1, 0)
        val u = tracker.onResult(s1, inProgress.requestId, RequestState.IN_PROGRESS, null, confirmed = false, nowMs = 5)!!
        assertEquals(RequestState.IN_PROGRESS, u.state)
        assertFalse(u.confirmed)
        tracker.onResult(s1, done.requestId, RequestState.DONE, ResultReason.NONE, confirmed = false, nowMs = 5)

        assertEquals(emptyList<Any>(), tracker.expireToUnknown(2_000))
        var byId = tracker.snapshot().associateBy { it.requestId }
        assertEquals(RequestState.IN_PROGRESS, byId.getValue(inProgress.requestId).state)
        assertEquals(RequestState.DONE, byId.getValue(done.requestId).state)

        tracker.resetForNewSession(s2, 11_000)
        byId = tracker.snapshot().associateBy { it.requestId }
        assertEquals(RequestState.UNKNOWN, byId.getValue(inProgress.requestId).state)
        assertFalse(byId.getValue(inProgress.requestId).confirmed)
        assertEquals(RequestState.DONE, byId.getValue(done.requestId).state)
        assertFalse(byId.getValue(done.requestId).confirmed)
        assertTrue(tracker.snapshot().none { it.state == RequestState.FAILED })
    }

    @Test fun `수용 뒤 최종 결과 기한을 넘기면 UNKNOWN - 실패로 단정하지 않는다 (PERF-003)`() {
        val inProgress = tracker.create(UserRequest.Door(true), s1, 0)
        tracker.onResult(s1, inProgress.requestId, RequestState.IN_PROGRESS, null, confirmed = false, nowMs = 5)
        val expired = tracker.expireToUnknown(3_005).single()
        assertEquals(RequestState.UNKNOWN, expired.state)
        assertFalse(expired.confirmed)
        assertTrue(tracker.snapshot().none { it.state == RequestState.FAILED })
    }

    @Test fun `1초 무결과면 UNKNOWN - 그 뒤 온 Result 는 반영`() {
        val r = tracker.create(UserRequest.Door(true), s1, 0)
        assertEquals(emptyList<Any>(), tracker.expireToUnknown(999))
        val expired = tracker.expireToUnknown(1_000)
        assertEquals(RequestState.UNKNOWN, expired.single().state)
        assertEquals(emptyList<Any>(), tracker.expireToUnknown(5_000))
        val late = tracker.onResult(s1, r.requestId, RequestState.DONE, ResultReason.NONE, true, 3_000)!!
        assertEquals(RequestState.DONE, late.state)
    }

    @Test fun `같은 세션의 UNKNOWN 재요청은 같은 ID`() {
        val r = tracker.create(UserRequest.Door(true), s1, 0)
        tracker.expireToUnknown(1_000)
        val again = tracker.resend(s1, r.requestId, currentSessionId = s1, nowMs = 2_000)!!
        assertEquals(r.requestId, again.requestId)
        assertEquals(RequestState.SENT, again.state)
        assertEquals(1, tracker.snapshot().size)
    }

    @Test fun `세션이 바뀐 뒤 재요청은 새 번호공간의 새 ID - 예전 요청은 UNKNOWN 으로 남는다`() {
        val r = tracker.create(UserRequest.Door(true), s1, 0)
        tracker.create(UserRequest.Door(false), s1, 0)
        tracker.resetForNewSession(s2, 500)
        val again = tracker.resend(s1, r.requestId, currentSessionId = s2, nowMs = 600)!!
        assertEquals(s2, again.sessionId)
        assertEquals(1L, again.requestId)
        assertEquals(RequestState.SENT, again.state)
        assertEquals(UserRequest.Door(true), again.request)

        val old = tracker.snapshot().single { it.sessionId == s1 && it.requestId == r.requestId }
        assertEquals(RequestState.UNKNOWN, old.state)
        assertEquals(3, tracker.snapshot().size)
    }

    @Test fun `같은 세션으로 reset 하면 ID 가 이어진다`() {
        val a = tracker.create(UserRequest.Door(true), s1, 0)
        val b = tracker.create(UserRequest.Door(false), s1, 0)
        tracker.resetForNewSession(s1, 100)
        val c = tracker.create(UserRequest.ClimateAuto(true), s1, 200)
        assertEquals(3L, c.requestId)
        assertEquals(3, tracker.snapshot().size)
        val byId = tracker.snapshot().associateBy { it.requestId }
        assertEquals(UserRequest.Door(true), byId.getValue(a.requestId).request)
        assertEquals(RequestState.UNKNOWN, byId.getValue(a.requestId).state)
        assertEquals(RequestState.UNKNOWN, byId.getValue(b.requestId).state)
        tracker.onResult(s1, a.requestId, RequestState.DONE, null, true, 300)
        assertEquals(RequestState.SENT, tracker.snapshot().single { it.requestId == c.requestId }.state)
    }

    @Test fun `세션이 바뀌면 1부터, 같은 세션 reset 을 여러 번 해도 이어진다`() {
        tracker.create(UserRequest.Door(true), s1, 0)
        tracker.resetForNewSession(s1, 1)
        tracker.resetForNewSession(s1, 2)
        assertEquals(2L, tracker.create(UserRequest.Door(true), s1, 3).requestId)
        tracker.resetForNewSession(s2, 4)
        assertEquals(1L, tracker.create(UserRequest.Door(true), s2, 5).requestId)
        tracker.resetForNewSession(s2, 6)
        assertEquals(2L, tracker.create(UserRequest.Door(true), s2, 7).requestId)
    }

    @Test fun `이미 추적 중인 (세션, ID) 와 겹치면 덮어쓰지 않고 다음 ID 로 건너뛴다`() {
        val a = tracker.create(UserRequest.Door(true), s1, 0)
        val b = tracker.create(UserRequest.Door(false), s1, 0)
        tracker.resetForNewSession(s2, 10)
        tracker.create(UserRequest.Fan(FanLevel.LOW), s2, 11)
        tracker.resetForNewSession(s1, 20)
        val c = tracker.create(UserRequest.ClimateAuto(false), s1, 21)
        assertEquals(3L, c.requestId)
        assertEquals(4, tracker.snapshot().size)
        val s1ById = tracker.snapshot().filter { it.sessionId == s1 }.associateBy { it.requestId }
        assertEquals(UserRequest.Door(true), s1ById.getValue(a.requestId).request)
        assertEquals(UserRequest.Door(false), s1ById.getValue(b.requestId).request)
    }

    @Test fun `u32 경계 - 0xFFFFFFFF 까지 발급 후 IdSpaceExhausted`() {
        val t = RequestTracker(deadlines, roomy, firstRequestId = RequestTracker.MAX_U32 - 1)
        assertEquals(0xFFFF_FFFEL, t.create(UserRequest.Door(true), s1, 0).requestId)
        assertEquals(0xFFFF_FFFFL, t.create(UserRequest.Door(true), s1, 0).requestId)
        try {
            t.create(UserRequest.Door(true), s1, 0)
            fail("IdSpaceExhausted 가 나야 한다")
        } catch (_: RequestTracker.IdSpaceExhausted) {
        }
        assertEquals(2, t.snapshot().size)
        t.resetForNewSession(s1, 1)
        try {
            t.create(UserRequest.Door(true), s1, 2)
            fail("IdSpaceExhausted 가 나야 한다")
        } catch (_: RequestTracker.IdSpaceExhausted) {
        }
        t.resetForNewSession(s2, 3)
        assertEquals(1L, t.create(UserRequest.Door(true), s2, 4).requestId)
    }

    @Test fun `같은 ID 재요청 뒤 늦은 Result 는 그 요청에 반영된다`() {
        val r = tracker.create(UserRequest.Door(true), s1, 0)
        tracker.expireToUnknown(1_000)
        val again = tracker.resend(s1, r.requestId, currentSessionId = s1, nowMs = 2_000)!!
        assertEquals(r.requestId, again.requestId)
        val late = tracker.onResult(s1, r.requestId, RequestState.DONE, ResultReason.NONE, true, 2_100)!!
        assertEquals(RequestState.DONE, late.state)
        assertEquals(1, tracker.snapshot().size)
        assertEquals(emptyList<Any>(), tracker.expireToUnknown(10_000))
        assertEquals(RequestState.DONE, tracker.snapshot().single().state)
    }

    @Test fun `UNKNOWN 이 아니면 재요청 대상 아님`() {
        val r = tracker.create(UserRequest.Door(true), s1, 0)
        assertNull(tracker.resend(s1, r.requestId, s1, 10))
    }

    @Test fun `새 세션 - 진행 중은 UNKNOWN, 종결은 그대로`() {
        val a = tracker.create(UserRequest.Door(true), s1, 0)
        val b = tracker.create(UserRequest.Door(false), s1, 0)
        tracker.onResult(s1, b.requestId, RequestState.DONE, null, true, 1)
        tracker.resetForNewSession(s2, 10)
        val byId = tracker.snapshot().associateBy { it.requestId }
        assertEquals(RequestState.UNKNOWN, byId.getValue(a.requestId).state)
        assertEquals(RequestState.DONE, byId.getValue(b.requestId).state)
    }

    @Test fun `송신 실패 요청은 추적에서 뺀다`() {
        val r = tracker.create(UserRequest.Door(true), s1, 0)
        tracker.withdraw(s1, r.requestId)
        assertEquals(0, tracker.snapshot().size)
    }

    @Test fun `같은 ID 재요청 송신 실패면 지우지 않고 UNKNOWN 으로 되돌린다`() {
        val r = tracker.create(UserRequest.Door(true), s1, 0)
        tracker.expireToUnknown(1_000)
        tracker.resend(s1, r.requestId, s1, 2_000)
        tracker.revertToUnknown(s1, r.requestId, 2_001)
        val back = tracker.snapshot().single()
        assertEquals(r.requestId, back.requestId)
        assertEquals(RequestState.UNKNOWN, back.state)
    }

    @Test fun `최종 기한은 최초 수용 관측부터 - 중간 응답으로 연장하지 않는다`() {
        val r = tracker.create(UserRequest.Door(true), s1, 0)
        tracker.onResult(s1, r.requestId, RequestState.ACCEPTED, null, true, 500)
        tracker.onResult(s1, r.requestId, RequestState.IN_PROGRESS, null, true, 2_500)
        assertEquals(500L, tracker.snapshot().single().acceptedAtMs)
        assertEquals(emptyList<Any>(), tracker.expireToUnknown(3_499))
        assertEquals(RequestState.UNKNOWN, tracker.expireToUnknown(3_500).single().state)
    }

    @Test fun `기능별 최종 기한 - 설정 2 s, 도어 3 s, 공조 Fan 5 s`() {
        val setting = tracker.create(UserRequest.TargetTemperature(22.0), s1, 0)
        val door = tracker.create(UserRequest.Door(true), s1, 0)
        val fan = tracker.create(UserRequest.Fan(FanLevel.HIGH), s1, 0)
        listOf(setting, door, fan).forEach { tracker.onResult(s1, it.requestId, RequestState.ACCEPTED, null, true, 0) }
        assertEquals(listOf(setting.requestId), tracker.expireToUnknown(2_000).map { it.requestId })
        assertEquals(listOf(door.requestId), tracker.expireToUnknown(3_000).map { it.requestId })
        assertEquals(emptyList<Any>(), tracker.expireToUnknown(4_999))
        assertEquals(listOf(fan.requestId), tracker.expireToUnknown(5_000).map { it.requestId })
    }

    @Test fun `링크 단절 - SENT·ACCEPTED·IN_PROGRESS 는 UNKNOWN, 종결은 그대로 (§40_1)`() {
        val sent = tracker.create(UserRequest.Door(true), s1, 0)
        val accepted = tracker.create(UserRequest.Door(false), s1, 0)
        val running = tracker.create(UserRequest.ClimateAuto(true), s1, 0)
        val done = tracker.create(UserRequest.LightEnabled(true), s1, 0)
        tracker.onResult(s1, accepted.requestId, RequestState.ACCEPTED, null, true, 1)
        tracker.onResult(s1, running.requestId, RequestState.IN_PROGRESS, null, true, 1)
        tracker.onResult(s1, done.requestId, RequestState.DONE, null, true, 1)
        tracker.onLinkLost(10)
        val byId = tracker.snapshot().associate { it.requestId to it.state }
        assertEquals(RequestState.UNKNOWN, byId[sent.requestId])
        assertEquals(RequestState.UNKNOWN, byId[accepted.requestId])
        assertEquals(RequestState.UNKNOWN, byId[running.requestId])
        assertEquals(RequestState.DONE, byId[done.requestId])
        assertEquals(listOf(sent, accepted, running).map { it.requestId }, tracker.unresolved().map { it.requestId })
    }

    @Test fun `UNKNOWN 뒤 늦은 ACCEPTED·IN_PROGRESS 는 UNKNOWN 유지 - 진행 정보만 보관, 최종 결과만 해소`() {
        val r = tracker.create(UserRequest.Door(true), s1, 0)
        tracker.expireToUnknown(1_000)
        val late = tracker.onResult(s1, r.requestId, RequestState.ACCEPTED, null, true, 1_200)!!
        assertEquals(RequestState.UNKNOWN, late.state)
        assertEquals(RequestState.ACCEPTED, late.progress)
        assertEquals(RequestState.UNKNOWN, tracker.onResult(s1, r.requestId, RequestState.IN_PROGRESS, null, true, 1_300)!!.state)
        assertEquals(RequestState.IN_PROGRESS, tracker.snapshot().single().progress)
        assertEquals(emptyList<Any>(), tracker.expireToUnknown(10_000))
        val resolved = tracker.onResult(s1, r.requestId, RequestState.DONE, ResultReason.NONE, true, 1_400)!!
        assertEquals(RequestState.DONE, resolved.state)
        assertFalse(resolved.resultMismatch)
    }

    @Test fun `확인된 IN_PROGRESS 뒤 REJECTED 는 순서 불일치 - 상태 유지, UNKNOWN 을 거쳐도 같다`() {
        val r = tracker.create(UserRequest.Door(true), s1, 0)
        tracker.onResult(s1, r.requestId, RequestState.IN_PROGRESS, null, true, 10)
        val direct = tracker.onResult(s1, r.requestId, RequestState.REJECTED, ResultReason.DOOR_OPEN, true, 20)!!
        assertEquals(RequestState.IN_PROGRESS, direct.state)
        assertTrue(direct.resultMismatch)

        val viaUnknown = tracker.create(UserRequest.Door(false), s1, 0)
        tracker.onResult(s1, viaUnknown.requestId, RequestState.IN_PROGRESS, null, true, 10)
        tracker.onLinkLost(20)
        val flagged = tracker.onResult(s1, viaUnknown.requestId, RequestState.REJECTED, null, true, 30)!!
        assertEquals(RequestState.UNKNOWN, flagged.state)
        assertTrue(flagged.resultMismatch)
        val acceptedOnly = tracker.create(UserRequest.Door(true), s1, 0)
        tracker.onResult(s1, acceptedOnly.requestId, RequestState.ACCEPTED, null, true, 10)
        val rejected = tracker.onResult(s1, acceptedOnly.requestId, RequestState.REJECTED, null, true, 20)!!
        assertEquals(RequestState.REJECTED, rejected.state)
        assertFalse(rejected.resultMismatch)
    }

    @Test fun `IN_PROGRESS 뒤 늦은 ACCEPTED 는 진행을 되돌리지 않는다`() {
        val r = tracker.create(UserRequest.Door(true), s1, 0)
        tracker.onResult(s1, r.requestId, RequestState.IN_PROGRESS, null, true, 10)
        assertNull(tracker.onResult(s1, r.requestId, RequestState.ACCEPTED, null, true, 20))
        assertEquals(RequestState.IN_PROGRESS, tracker.snapshot().single().state)
    }

    @Test fun `결과가 난 요청은 최근 한도만 남기고 진행 중은 한도와 무관하게 유지`() {
        val t = RequestTracker(deadlines, recentLimit = 2)
        val inFlight = (1..3).map { t.create(UserRequest.Door(true), s1, 0) }
        val settled = (1..4).map { t.create(UserRequest.LightEnabled(true), s1, 0) }
        settled.forEachIndexed { i, r -> t.onResult(s1, r.requestId, RequestState.DONE, null, true, 10L + i) }
        val kept = t.snapshot().map { it.requestId }
        assertEquals(inFlight.map { it.requestId } + settled.takeLast(2).map { it.requestId }, kept)
        t.onLinkLost(100)
        assertEquals(2, t.snapshot().size)
        assertTrue(t.snapshot().all { it.state == RequestState.UNKNOWN })
    }

    @Test fun `등록 해제 정리 - 요청을 모두 지우고 번호는 이어 간다`() {
        tracker.create(UserRequest.Door(true), s1, 0)
        val done = tracker.create(UserRequest.Door(false), s1, 0)
        tracker.onResult(s1, done.requestId, RequestState.DONE, null, true, 1)
        tracker.clear()
        assertTrue(tracker.snapshot().isEmpty())
        assertEquals(3L, tracker.create(UserRequest.Door(true), s1, 2).requestId)
    }

    private fun resentAfterProgress(t: RequestTracker): TrackedRequest {
        val r = t.create(UserRequest.Door(true), s1, 0)
        t.onResult(s1, r.requestId, RequestState.IN_PROGRESS, null, true, 100)
        assertEquals(RequestState.UNKNOWN, t.expireToUnknown(3_100).single().state)
        val again = t.resend(s1, r.requestId, s1, 4_000)!!
        assertEquals(r.requestId, again.requestId)
        assertEquals(2, again.attempts.size)
        assertNull(again.progress)
        return again
    }

    @Test fun `같은 ID 재요청 뒤 진행 응답은 반영 - ACCEPTED`() {
        val again = resentAfterProgress(tracker)
        val accepted = tracker.onResult(s1, again.requestId, RequestState.ACCEPTED, null, true, 4_100)!!
        assertEquals(RequestState.ACCEPTED, accepted.state)
        assertEquals(RequestState.ACCEPTED, accepted.progress)
        assertEquals(4_100L, accepted.acceptedAtMs)
        assertEquals(emptyList<Any>(), tracker.expireToUnknown(5_000))
    }

    @Test fun `같은 ID 재요청 뒤 진행 응답은 반영 - IN_PROGRESS`() {
        val again = resentAfterProgress(tracker)
        val running = tracker.onResult(s1, again.requestId, RequestState.IN_PROGRESS, null, true, 4_100)!!
        assertEquals(RequestState.IN_PROGRESS, running.state)
        assertEquals(RequestState.IN_PROGRESS, running.progress)
        assertEquals(emptyList<Any>(), tracker.expireToUnknown(5_000))
        assertNull(tracker.onResult(s1, again.requestId, RequestState.ACCEPTED, null, true, 4_200))
    }

    @Test fun `재요청 시도 기준 기한 - 최초 응답은 재송신부터, 최종은 새 시도의 첫 수용부터`() {
        val again = resentAfterProgress(tracker)
        assertEquals(4_000L, again.sentAtMs)
        assertEquals(emptyList<Any>(), tracker.expireToUnknown(4_999))
        assertEquals(RequestState.UNKNOWN, tracker.expireToUnknown(5_000).single().state)

        val t = RequestTracker(deadlines, roomy)
        val second = resentAfterProgress(t)
        t.onResult(s1, second.requestId, RequestState.ACCEPTED, null, true, 4_500)
        t.onResult(s1, second.requestId, RequestState.IN_PROGRESS, null, true, 6_000)
        assertEquals(emptyList<Any>(), t.expireToUnknown(7_499))
        assertEquals(RequestState.UNKNOWN, t.expireToUnknown(7_500).single().state)
    }

    @Test fun `이전 시도의 IN_PROGRESS 뒤 REJECTED 는 재요청을 거쳐도 순서 불일치`() {
        val again = resentAfterProgress(tracker)
        val flagged = tracker.onResult(s1, again.requestId, RequestState.REJECTED, ResultReason.DOOR_OPEN, true, 4_100)!!
        assertEquals(RequestState.SENT, flagged.state)
        assertTrue(flagged.resultMismatch)
        assertTrue(flagged.executionObserved)
    }

    @Test fun `재요청 송신 실패면 그 시도를 거둔다 - 이전 시도의 이력은 남는다`() {
        val again = resentAfterProgress(tracker)
        tracker.revertToUnknown(s1, again.requestId, 4_001)
        val back = tracker.snapshot().single()
        assertEquals(RequestState.UNKNOWN, back.state)
        assertEquals(1, back.attempts.size)
        assertEquals(RequestState.IN_PROGRESS, back.progress)
    }
}
