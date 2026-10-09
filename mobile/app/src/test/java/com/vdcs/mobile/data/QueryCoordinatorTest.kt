package com.vdcs.mobile.data

import com.vdcs.mobile.data.QueryCoordinator.Pending
import com.vdcs.mobile.data.QueryCoordinator.Purpose
import com.vdcs.mobile.domain.repository.QueryScope
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class QueryCoordinatorTest {
    private val q = QueryCoordinator(timeoutMs = 3_000, busyRetryMs = 400)
    private val user = Pending(QueryScope.REQUEST_RESULT, Purpose.USER)

    @Test fun `한 번에 하나 - 진행 중이면 다음을 시작하지 않는다`() {
        q.enqueue(user)
        q.enqueue(Pending(QueryScope.WARNINGS, Purpose.USER))
        val first = q.startNext(0)!!
        assertNull(q.startNext(0))
        assertEquals(Purpose.USER, q.onEnd(first.id, status = 0, itemCount = 0, contextOk = true)!!.pending.purpose)
        val second = q.startNext(0)!!
        assertNotEquals(first.id, second.id)
        assertEquals(QueryScope.WARNINGS, second.pending.scope)
    }

    @Test fun `완결 - 문맥·QUERY_ID·수신 수·STATUS 0·1·2, 3 은 미완결`() {
        for (status in 0..3) {
            q.enqueue(user)
            val s = q.startNext(0)!!
            q.onResponse(s.id, null)
            q.onResponse(s.id, null)
            val done = q.onEnd(s.id, status, itemCount = 2, contextOk = true)!!
            assertEquals("STATUS $status", status <= 2, done.complete)
        }
    }

    private fun answer(id: Long, items: Iterable<Int>): Int {
        var n = 0
        for (item in items) { q.onResponse(id, item); n++ }
        return n
    }

    @Test fun `현재 상태 - 필수 14종이 다 오면 완결`() {
        q.enqueue(Pending(QueryScope.CURRENT_STATE, Purpose.USER))
        val s = q.startNext(0)!!
        val n = answer(s.id, QueryCoordinator.STATE_ITEMS)
        assertEquals(14, n)
        assertTrue(q.onEnd(s.id, 0, itemCount = n, contextOk = true)!!.complete)
    }

    @Test fun `현재 상태 - 개수는 맞아도 한 종류가 중복되고 하나가 빠지면 미완결`() {
        q.enqueue(Pending(QueryScope.CURRENT_STATE, Purpose.USER))
        val s = q.startNext(0)!!
        val items = QueryCoordinator.STATE_ITEMS.drop(1) + QueryCoordinator.STATE_ITEMS.last()
        val n = answer(s.id, items)
        assertFalse(q.onEnd(s.id, 0, itemCount = n, contextOk = true)!!.complete)
    }

    @Test fun `현재 상태 - STATUS 2(일부 확인 불가)도 목록이 다 오면 완결`() {
        q.enqueue(Pending(QueryScope.CURRENT_STATE, Purpose.USER))
        val s = q.startNext(0)!!
        val n = answer(s.id, QueryCoordinator.STATE_ITEMS)
        assertTrue(q.onEnd(s.id, 2, itemCount = n, contextOk = true)!!.complete)
    }

    @Test fun `경고 - 8개가 와도 같은 Type 중복으로 Type 8 이 빠지면 미완결`() {
        q.enqueue(Pending(QueryScope.WARNINGS, Purpose.USER))
        val s = q.startNext(0)!!
        val types = (1..7).toList() + 7
        val n = answer(s.id, types.map { QueryCoordinator.WARNING_ITEMS.toList()[it - 1] })
        assertEquals(8, n)
        assertFalse(q.onEnd(s.id, 0, itemCount = n, contextOk = true)!!.complete)
    }

    @Test fun `전체 - 상태 14 + 경고 8 + 결과 몇 건이면 완결`() {
        q.enqueue(Pending(QueryScope.ALL, Purpose.SYNC))
        val s = q.startNext(0)!!
        var n = answer(s.id, QueryCoordinator.STATE_ITEMS + QueryCoordinator.WARNING_ITEMS)
        q.onResponse(s.id, null)
        n++
        assertTrue(q.onEnd(s.id, 0, itemCount = n, contextOk = true)!!.complete)
    }

    @Test fun `다른 QUERY_ID·unsolicited(0) 응답은 세지 않는다`() {
        q.enqueue(user)
        val s = q.startNext(0)!!
        q.onResponse(0, null)
        q.onResponse(s.id + 1, null)
        assertFalse(q.onEnd(s.id, 0, itemCount = 1, contextOk = true)!!.complete)
    }

    @Test fun `문맥이 다르면 개수가 맞아도 미완결`() {
        q.enqueue(user)
        val s = q.startNext(0)!!
        assertFalse(q.onEnd(s.id, 0, itemCount = 0, contextOk = false)!!.complete)
    }

    @Test fun `진행 중이 아닌 QUERY_END 는 무시`() {
        assertNull(q.onEnd(1, 0, 0, contextOk = true))
        q.enqueue(user)
        val s = q.startNext(0)!!
        assertNull(q.onEnd(s.id + 5, 0, 0, contextOk = true))
        assertTrue(q.onEnd(s.id, 0, 0, contextOk = true)!!.complete)
    }

    @Test fun `대기 한도를 넘기면 미완결로 끝나고 늦은 끝은 무시`() {
        q.enqueue(user)
        val s = q.startNext(1_000)!!
        assertNull(q.expire(4_000))
        val timedOut = q.expire(4_001)!!
        assertFalse(timedOut.complete)
        assertNull(q.onEnd(s.id, 0, 0, contextOk = true))
    }

    @Test fun `초기 동기화는 맨 앞에 하나만`() {
        q.enqueue(user)
        q.enqueueSyncIfAbsent()
        q.enqueueSyncIfAbsent()
        val s = q.startNext(0)!!
        assertEquals(Purpose.SYNC, s.pending.purpose)
        assertEquals(QueryScope.ALL, s.pending.scope)
        q.enqueueSyncIfAbsent()
        q.onEnd(s.id, 0, 0, contextOk = true)
        assertEquals(Purpose.USER, q.startNext(0)!!.pending.purpose)
    }

    @Test fun `ATT 0x87 QUERY_BUSY - 같은 Query 를 맨 앞에 되돌리고 잠시 뒤 다시`() {
        q.enqueue(user)
        val s = q.startNext(0)!!
        q.onSendFailed(s, busy = true, now = 100)
        assertNull(q.startNext(499))
        val again = q.startNext(500)!!
        assertEquals(s.pending, again.pending)
    }

    @Test fun `그 밖의 쓰기 거절은 그 Query 를 버린다`() {
        q.enqueue(user)
        val s = q.startNext(0)!!
        q.onSendFailed(s, busy = false, now = 0)
        assertNull(q.startNext(0))
    }

    @Test fun `끊김·문맥 변경 - 진행·대기·재시도 대기 모두 폐기`() {
        q.enqueue(user)
        q.enqueue(user)
        val s = q.startNext(0)!!
        q.onSendFailed(s, busy = true, now = 0)
        q.clear()
        assertNull(q.startNext(0))
        q.enqueue(user)
        assertTrue(q.startNext(0) != null)
    }

    @Test fun `FOLLOW_UP 은 REQUEST_SESSION·REQUEST_ID 를 싣는다`() {
        q.enqueue(Pending(QueryScope.REQUEST_RESULT, Purpose.FOLLOW_UP, requestSession = 9, requestId = 4))
        val s = q.startNext(0)!!
        assertEquals(9L, s.pending.requestSession)
        assertEquals(4L, s.pending.requestId)
    }
}
