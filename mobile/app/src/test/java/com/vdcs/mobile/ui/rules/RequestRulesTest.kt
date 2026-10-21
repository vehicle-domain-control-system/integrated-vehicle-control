package com.vdcs.mobile.ui.rules

import com.vdcs.mobile.domain.model.RequestAttempt
import com.vdcs.mobile.domain.model.RequestState
import com.vdcs.mobile.domain.model.ResultReason
import com.vdcs.mobile.domain.model.TrackedRequest
import com.vdcs.mobile.domain.model.UserRequest
import com.vdcs.mobile.domain.repository.InvalidRequest
import com.vdcs.mobile.domain.repository.PermissionMissing
import com.vdcs.mobile.domain.repository.RequestBlocked
import com.vdcs.mobile.domain.repository.RequestNotSent
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class RequestRulesTest {
    private fun tracked(state: RequestState, reason: ResultReason?) =
        TrackedRequest(1, 7, UserRequest.Door(lock = true), state, reason, confirmed = false, sentAtMs = 0)

    @Test fun `UNKNOWN 은 사유가 와도 알 수 없음 문구 - 사유로 성공 실패를 암시하지 않는다`() {
        val withReason = RequestRules.status(tracked(RequestState.UNKNOWN, ResultReason.FAULT))
        val without = RequestRules.status(tracked(RequestState.UNKNOWN, null))
        assertEquals(without, withReason)
        assertEquals(RequestTone.UNCONFIRMED, withReason.tone)
        assertTrue(withReason.detail!!.contains("알 수 없음"))
    }

    @Test fun `NONE 사유는 문구를 붙이지 않는다`() {
        assertEquals(null, RequestRules.status(tracked(RequestState.DONE, ResultReason.NONE)).detail)
        assertEquals(null, RequestRules.status(tracked(RequestState.SENT, null)).detail)
    }

    @Test fun `조작 실패 메시지는 도메인 예외별 문구`() {
        assertEquals(ActionMessages.blockedMessage("문맥 확인 중"), ActionMessages.of(RequestBlocked("문맥 확인 중")))
        assertEquals(ActionMessages.notSentMessage("ATT 0x87"), ActionMessages.of(RequestNotSent("ATT 0x87")))
        assertTrue(ActionMessages.of(InvalidRequest("밝기 범위 밖")).startsWith("요청 값 오류 — 밝기 범위 밖"))
        assertTrue(ActionMessages.of(PermissionMissing("x")).startsWith("블루투스 권한이 없습니다"))
        assertTrue(ActionMessages.of(IllegalStateException("뭔가")).startsWith("처리하지 못했습니다 — 뭔가"))
    }

    @Test fun `진행 응답은 상태와 다를 때만 - 미확인이어도 접수 응답은 받았음을 말한다`() {
        val unknownAfterAccept = tracked(RequestState.UNKNOWN, null).copy(attempts = listOf(RequestAttempt(0, progress = RequestState.ACCEPTED)))
        assertEquals("차량 진행 응답: 접수됨", RequestRules.progressText(unknownAfterAccept))
        val inProgress = tracked(RequestState.IN_PROGRESS, null).copy(attempts = listOf(RequestAttempt(0, progress = RequestState.IN_PROGRESS)))
        assertEquals(null, RequestRules.progressText(inProgress))
        assertEquals(null, RequestRules.progressText(tracked(RequestState.SENT, null)))
    }

    @Test fun `결과 불일치는 상태와 별도로 표시 - 상태 글자는 먼저 확인된 결과 그대로`() {
        val mismatch = tracked(RequestState.DONE, null).copy(resultMismatch = true)
        assertEquals(RequestRules.MISMATCH_TEXT, RequestRules.mismatchText(mismatch))
        assertEquals("완료", RequestRules.status(mismatch).label)
        assertEquals(null, RequestRules.mismatchText(tracked(RequestState.DONE, null)))
    }

    @Test fun `전송됨과 완료는 다른 문구 - 전송 완료를 수행 완료로 쓰지 않는다`() {
        val sent = RequestRules.status(tracked(RequestState.SENT, null))
        val done = RequestRules.status(tracked(RequestState.DONE, null))
        assertEquals(RequestTone.PENDING, sent.tone)
        assertEquals(RequestTone.SUCCESS, done.tone)
        assertTrue(sent.label != done.label)
    }
}
