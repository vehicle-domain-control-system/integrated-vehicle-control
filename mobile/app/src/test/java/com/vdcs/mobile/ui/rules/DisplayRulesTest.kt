package com.vdcs.mobile.ui.rules

import com.vdcs.mobile.domain.model.Availability
import com.vdcs.mobile.domain.model.AutoUnlockResult
import com.vdcs.mobile.domain.model.ConnectionState
import com.vdcs.mobile.domain.model.DoorIntegrity
import com.vdcs.mobile.domain.model.DoorState
import com.vdcs.mobile.domain.model.LockState
import com.vdcs.mobile.domain.model.OpenState
import com.vdcs.mobile.domain.model.EcuHealth
import com.vdcs.mobile.domain.model.FunctionStatus
import com.vdcs.mobile.domain.model.Qualified
import com.vdcs.mobile.domain.model.Quality
import com.vdcs.mobile.domain.model.ReadState
import com.vdcs.mobile.domain.model.RearProximity
import com.vdcs.mobile.domain.model.RearState
import com.vdcs.mobile.domain.model.RequestState
import com.vdcs.mobile.domain.model.ResultReason
import com.vdcs.mobile.domain.model.Rgb
import com.vdcs.mobile.domain.model.Severity
import com.vdcs.mobile.domain.model.TrackedRequest
import com.vdcs.mobile.domain.model.UserRequest
import com.vdcs.mobile.domain.model.Warning
import com.vdcs.mobile.domain.model.WarningType
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class DisplayRulesTest {
    private val now = 100_000L
    private fun temp(q: Quality, at: Long = now) = Qualified(21.5, q, at)
    private fun fmt(v: Double) = ValueFormat.celsius(v)

    private fun doorWith(integrity: Qualified<DoorIntegrity>) =
        DoorState(Qualified(LockState.LOCKED, Quality.OK, now), Qualified(OpenState.CLOSED, Quality.OK, now), integrity)

    @Test fun `INVALID 는 수치 대신 확인 불가`() {
        val v = ValueFormat.value(temp(Quality.INVALID), now, ::fmt)
        assertEquals(ValueFormat.UNTRUSTED_TEXT, v.text)
        assertFalse(v.shown)
        assertFalse(v.text.contains("21"))
    }

    @Test fun `NO_DATA 와 미수신은 대시`() {
        val v = ValueFormat.value(temp(Quality.NO_DATA), now, ::fmt)
        assertEquals(ValueFormat.NO_DATA_TEXT, v.text)
        assertFalse(v.shown)
        val none = ValueFormat.value<Double>(null, now, ::fmt)
        assertEquals(ValueFormat.NO_DATA_TEXT, none.text)
        assertFalse(none.shown)
    }

    @Test fun `STALE 은 값과 마지막 수신 시각을 함께`() {
        val v = ValueFormat.value(temp(Quality.STALE, at = now - 7_000), now, ::fmt)
        assertEquals("21.5°C", v.text)
        assertTrue(v.shown)
        assertNotNull(v.note)
        assertTrue(v.note!!.contains("7초 전"))
    }

    @Test fun `OK 는 값만`() {
        val v = ValueFormat.value(temp(Quality.OK), now, ::fmt)
        assertEquals("21.5°C", v.text)
        assertNull(v.note)
    }

    @Test fun `경과 시간 표기`() {
        assertEquals("0초 전", ValueFormat.ago(now, now))
        assertEquals("59초 전", ValueFormat.ago(now - 59_000, now))
        assertEquals("2분 전", ValueFormat.ago(now - 125_000, now))
        assertEquals("1시간 전", ValueFormat.ago(0, 3_700_000))
        assertEquals("0초 전", ValueFormat.ago(now + 5_000, now))
    }

    @Test fun `목표 온도 스테퍼는 0_5 격자`() {
        assertEquals(22.5, ValueFormat.stepTarget(22.0, 1), 0.0)
        assertEquals(21.5, ValueFormat.stepTarget(22.0, -1), 0.0)
        assertEquals(22.5, ValueFormat.stepTarget(22.26, 0), 0.0)
    }

    private fun warning(
        type: WarningType,
        active: Boolean,
        severity: Severity = Severity.CAUTION,
        quality: Quality = Quality.OK,
        read: ReadState = ReadState.UNREAD,
        at: Long = now,
    ) = Warning(type, active, severity, quality, occurrenceId = 1, domainBootId = 1, read = read, receivedAtMs = at, ageMs = 0)

    @Test fun `CLEAR 여도 품질이 OK 가 아니면 확인 불가`() {
        for (q in listOf(Quality.STALE, Quality.INVALID, Quality.NO_DATA)) {
            val w = warning(WarningType.REAR, active = false, quality = q)
            assertEquals(WarningDisplay.UNCERTAIN, WarningRules.display(w))
            assertEquals(ValueFormat.UNTRUSTED_TEXT, WarningRules.statusText(w))
        }
        val ok = warning(WarningType.REAR, active = false)
        assertEquals("해제됨", WarningRules.statusText(ok))
    }

    @Test fun `활성 경고는 품질이 낮아도 감추지 않는다`() {
        val w = warning(WarningType.PINCH, active = true, quality = Quality.STALE)
        assertEquals(WarningDisplay.ACTIVE, WarningRules.display(w))
        assertTrue(WarningRules.statusText(w).startsWith("발생 중"))
    }

    @Test fun `경고 정렬 - 활성 우선, 그 안에서 EMERGENCY 우선`() {
        val cleared = warning(WarningType.REAR, active = false, severity = Severity.EMERGENCY)
        val uncertain = warning(WarningType.CIS_FAULT, active = false, quality = Quality.NO_DATA)
        val caution = warning(WarningType.OCCUPANT_REMAINING, active = true, severity = Severity.CAUTION)
        val emergency = warning(WarningType.DOOR_OPEN_AFTER_EXIT, active = true, severity = Severity.EMERGENCY, at = now - 60_000)
        val sorted = WarningRules.sort(listOf(cleared, caution, uncertain, emergency))
        assertEquals(listOf(emergency, caution, uncertain, cleared), sorted)
    }

    @Test fun `경고 8종 모두 한국어 이름 - Type 8 은 하차 후 도어 열림`() {
        assertEquals(8, WarningType.entries.size)
        WarningType.entries.forEach { assertTrue(it.label.isNotBlank()) }
        assertEquals("하차 후 도어 열림", WarningType.DOOR_OPEN_AFTER_EXIT.label)
    }

    @Test fun `읽음은 해제가 아니다`() {
        assertTrue(ReadState.READ.label.contains("해제 아님"))
        val readButActive = warning(WarningType.REAR, active = true, read = ReadState.READ)
        assertEquals(WarningDisplay.ACTIVE, WarningRules.display(readButActive))
    }

    private fun tracked(state: RequestState, reason: ResultReason? = null, confirmed: Boolean = false) =
        TrackedRequest(1, 7, UserRequest.Door(lock = false), state, reason, confirmed, sentAtMs = now)

    @Test fun `UNKNOWN 은 미확인 - 성공 실패로 단정하지 않는다`() {
        val s = RequestRules.status(tracked(RequestState.UNKNOWN))
        assertEquals("미확인", s.label)
        assertEquals(RequestTone.UNCONFIRMED, s.tone)
        assertTrue(RequestRules.isResendable(tracked(RequestState.UNKNOWN)))
        RequestState.entries.filter { it != RequestState.UNKNOWN }.forEach {
            assertFalse(RequestRules.isResendable(tracked(it)))
        }
    }

    @Test fun `DONE + ALREADY_AT_TARGET 은 오류가 아니라 완료 - 구동 없이`() {
        val s = RequestRules.status(tracked(RequestState.DONE, ResultReason.ALREADY_AT_TARGET))
        assertEquals("완료", s.label)
        assertEquals(RequestTone.SUCCESS, s.tone)
        assertEquals("이미 목표 상태 — 구동 없이 완료", s.detail)
    }

    @Test fun `REJECTED·FAILED 는 사유가 ALREADY_AT_TARGET 이어도 보고 그대로 - 자체 보정 금지`() {
        val rejected = RequestRules.status(tracked(RequestState.REJECTED, ResultReason.ALREADY_AT_TARGET))
        assertEquals("거부", rejected.label)
        assertEquals(RequestTone.FAILURE, rejected.tone)
        assertEquals(ResultReason.ALREADY_AT_TARGET.label, rejected.detail)
        val failed = RequestRules.status(tracked(RequestState.FAILED, ResultReason.ALREADY_AT_TARGET))
        assertEquals("실패", failed.label)
        assertEquals(RequestTone.FAILURE, failed.tone)
    }

    @Test fun `거부·실패는 사유 문구를 붙인다`() {
        val s = RequestRules.status(tracked(RequestState.REJECTED, ResultReason.DOOR_OPEN))
        assertEquals("거부", s.label)
        assertEquals(RequestTone.FAILURE, s.tone)
        assertEquals("도어가 열려 있음", s.detail)
    }

    @Test fun `미확정 결과는 실패가 아니다`() {
        val unconfirmed = tracked(RequestState.DONE, confirmed = false)
        assertEquals("차량 확인 전 (실패 아님)", RequestRules.confirmationText(unconfirmed))
        assertEquals("차량 확인됨", RequestRules.confirmationText(tracked(RequestState.DONE, confirmed = true)))
        assertNull(RequestRules.confirmationText(tracked(RequestState.SENT)))
    }

    @Test fun `근접 자동 해제는 사용자 요청 해제와 구분`() {
        val user = UserRequest.Door(lock = false).label
        val auto = AutoUnlockResult.DONE.label
        assertTrue(user.contains("사용자 요청"))
        assertTrue(auto.contains("자동"))
        assertNotEquals(user, auto)
    }

    @Test fun `사유 코드는 전부 한국어 문구`() {
        ResultReason.entries.forEach {
            val text = it.label
            assertTrue(text.isNotBlank())
            assertFalse("$it 문구에 영문 enum 이름이 그대로 나옴", text.contains(it.name))
        }
    }

    @Test fun `AUTHENTICATED 가 아니면 제어 막고 사유`() {
        for (c in ConnectionState.entries.filter { it != ConnectionState.AUTHENTICATED }) {
            val reason = ControlRules.controlBlockReason(c, "동기화 중", null)
            assertNotNull(reason)
            assertTrue(reason!!.contains("동기화 중"))
        }
        assertNull(ControlRules.controlBlockReason(ConnectionState.AUTHENTICATED, null, null))
    }

    @Test fun `UNAVAILABLE 기능은 버튼을 막고 사유 표시`() {
        val off = FunctionStatus(Availability.UNAVAILABLE, ResultReason.FAULT, now, ageMs = 0)
        assertEquals("사용 불가 — 고장", ControlRules.controlBlockReason(ConnectionState.AUTHENTICATED, null, off))
        val limited = FunctionStatus(Availability.LIMITED, ResultReason.STALE, now, ageMs = 0)
        assertNull(ControlRules.controlBlockReason(ConnectionState.AUTHENTICATED, null, limited))
    }

    @Test fun `AVAILABLE 이지만 품질이 OK 가 아니면 막힘 - 가용성 미확인`() {
        for (q in listOf(Quality.STALE, Quality.INVALID, Quality.NO_DATA)) {
            val s = FunctionStatus(Availability.AVAILABLE, ResultReason.NONE, now - 4_000, q, ageMs = 0)
            assertEquals(ControlRules.AVAILABILITY_STALE, ControlRules.controlBlockReason(ConnectionState.AUTHENTICATED, null, s))
            val text = ControlRules.availabilityText(s, now)
            assertEquals("가용성 미확인", text.text)
            assertFalse(text.shown)
            assertTrue(text.note!!.contains("4초 전 수신"))
        }
        val ok = FunctionStatus(Availability.AVAILABLE, ResultReason.NONE, now, Quality.OK, ageMs = 0)
        assertNull(ControlRules.controlBlockReason(ConnectionState.AUTHENTICATED, null, ok))
        assertEquals("사용 가능", ControlRules.availabilityText(ok, now).text)
    }

    @Test fun `availability UNKNOWN 도 막힘`() {
        val s = FunctionStatus(Availability.UNKNOWN, ResultReason.NONE, now, ageMs = 0)
        assertNotNull(ControlRules.controlBlockReason(ConnectionState.AUTHENTICATED, null, s))
        assertEquals("가용성 미확인", ControlRules.availabilityText(s, now).text)
    }

    @Test fun `ECU 상태 - STALE 은 최신 아님, NO_DATA 는 고장 없음으로 단정 안 함`() {
        val stale = EcuHealth("BCM", "ACTIVE", emptyList(), recovering = false, receivedAtMs = now - 5_000, quality = Quality.STALE,
            faultCategory = null, faultCode = null, affectedFunctions = emptySet(), ageMs = 0)
        val st = VehicleValueRules.ecuStatusText(stale, now)
        assertEquals("ACTIVE", st.text)
        assertTrue(st.note!!.contains("최신 아님"))
        assertTrue(VehicleValueRules.ecuFaultsText(stale).text.contains("최신 아님"))
        val none = stale.copy(quality = Quality.NO_DATA)
        assertFalse(VehicleValueRules.ecuStatusText(none, now).shown)
        assertFalse(VehicleValueRules.ecuFaultsText(none).text.contains("고장 없음"))
        assertEquals("고장 없음", VehicleValueRules.ecuFaultsText(stale.copy(quality = Quality.OK)).text)
    }

    @Test fun `도어 무결성 - 품질이 낮으면 정상으로 단정 안 함`() {
        val invalid = VehicleValueRules.integrityText(doorWith(Qualified(DoorIntegrity.NORMAL, Quality.INVALID, now)), now)
        assertEquals(ValueFormat.UNTRUSTED_TEXT, invalid.text)
        val stale = VehicleValueRules.integrityText(doorWith(Qualified(DoorIntegrity.NORMAL, Quality.STALE, now - 3_000)), now)
        assertEquals("정상", stale.text)
        assertTrue(stale.note!!.contains("최신 아님"))
        assertNull(VehicleValueRules.integrityText(doorWith(Qualified(DoorIntegrity.NORMAL, Quality.OK, now)), now).note)
    }

    @Test fun `후방 상태 품질 - 낮으면 물체 없음·수치 단정 안 함`() {
        val d = Qualified(120.0, Quality.OK, now)
        assertEquals("120 cm", VehicleValueRules.rearText(RearState(d, Qualified(RearProximity.VALID_DISTANCE, Quality.OK, now)), now).text)
        val invalid = VehicleValueRules.rearText(RearState(d, Qualified(RearProximity.NO_OBJECT, Quality.INVALID, now)), now)
        assertEquals(ValueFormat.UNTRUSTED_TEXT, invalid.text)
        val staleDistance = VehicleValueRules.rearText(RearState(d, Qualified(RearProximity.VALID_DISTANCE, Quality.STALE, now)), now)
        assertEquals("120 cm", staleDistance.text)
        assertNotNull(staleDistance.note)
        assertEquals("감지 물체 없음", VehicleValueRules.rearText(RearState(d, Qualified(RearProximity.NO_OBJECT, Quality.OK, now)), now).text)
    }

    @Test fun `편집 값 동기화는 shown 값만`() {
        assertEquals(50, Qualified(50, Quality.OK, now).displayableValue())
        assertEquals(50, Qualified(50, Quality.STALE, now).displayableValue())
        assertNull(Qualified(50, Quality.INVALID, now).displayableValue())
        assertNull(Qualified(50, Quality.NO_DATA, now).displayableValue())
    }

    @Test fun `송신 실패 메시지`() {
        assertTrue(ActionMessages.blockedMessage("차량 문맥 확인 중").contains("차량 문맥 확인 중"))
        assertTrue(ActionMessages.notSentMessage("다른 조회 진행 중").contains("전달되지 않았습니다"))
    }

    @Test fun `조명 색상은 프리셋 이름으로, 프리셋 밖 값은 코드와 함께`() {
        assertEquals("빨강", Rgb(255, 0, 0).label)
        assertEquals("전구색", Rgb(255, 180, 107).label)
        assertEquals("사용자 지정 (#123456)", Rgb(0x12, 0x34, 0x56).label)
        assertEquals("조명 색상 파랑", UserRequest.LightColor(Rgb(0, 0, 255)).label)
    }
}
