package com.vdcs.mobile.ui.rules

import com.vdcs.mobile.domain.model.Availability
import com.vdcs.mobile.domain.model.ConnectionState
import com.vdcs.mobile.domain.model.FanLevel
import com.vdcs.mobile.domain.model.FunctionStatus
import com.vdcs.mobile.domain.model.Quality
import com.vdcs.mobile.domain.model.ReadState
import com.vdcs.mobile.domain.model.RequestState
import com.vdcs.mobile.domain.model.ResultReason
import com.vdcs.mobile.domain.model.Rgb
import com.vdcs.mobile.domain.model.Severity
import com.vdcs.mobile.domain.model.TrackedRequest
import com.vdcs.mobile.domain.model.UserRequest
import com.vdcs.mobile.domain.model.VehicleFunction
import com.vdcs.mobile.domain.model.VehicleSnapshot
import com.vdcs.mobile.domain.model.Warning
import com.vdcs.mobile.domain.model.WarningRecord
import com.vdcs.mobile.domain.model.WarningType
import com.vdcs.mobile.ui.VehicleUiState
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class ControlRulesTest {
    private val now = 50_000L
    private val notAuthenticated = ConnectionState.entries.filter { it != ConnectionState.AUTHENTICATED }
    private val unavailable = FunctionStatus(Availability.UNAVAILABLE, ResultReason.DOOR_OPEN, now, ageMs = 0)
    private val available = FunctionStatus(Availability.AVAILABLE, ResultReason.NONE, now, ageMs = 0)

    @Test fun `읽음 - 인증 전이면 사유, 인증 뒤에는 null`() {
        notAuthenticated.forEach { assertNotNull(it.name, ControlRules.ackBlockReason(it)) }
        assertNull(ControlRules.ackBlockReason(ConnectionState.AUTHENTICATED))
    }

    @Test fun `모두 읽음 - 현재 경고 ACK 가 있으면 읽음 사유를 따르고, 보관 이력 앱 확인만이면 연결과 무관 (D21f)`() {
        val record = WarningRecord(1, WarningType.REAR, 1, Severity.CAUTION, false, ReadState.UNREAD, 0, 0)
        val warning = Warning(WarningType.REAR, true, Severity.CAUTION, Quality.OK, 2, 1, ReadState.UNREAD, 0, 0)
        val archivedOnly = AckTargets(emptyList(), listOf(record, record.copy(occurrenceId = 0)))
        val withCurrent = AckTargets(listOf(warning), listOf(record))
        notAuthenticated.forEach {
            assertNull(it.name, ControlRules.ackAllBlockReason(it, archivedOnly))
            assertEquals(it.name, ControlRules.ackBlockReason(it), ControlRules.ackAllBlockReason(it, withCurrent))
        }
        assertNull(ControlRules.ackAllBlockReason(ConnectionState.AUTHENTICATED, withCurrent))
    }

    @Test fun `다시 보내기 - 인증 전 문구는 기존 그대로`() {
        notAuthenticated.forEach {
            assertEquals("차량 인증 완료 후 다시 보낼 수 있습니다", ControlRules.resendBlockReason(it, available))
        }
    }

    @Test fun `다시 보내기도 기능 가용성으로 막는다 - D9`() {
        assertEquals("사용 불가 — 도어가 열려 있음", ControlRules.resendBlockReason(ConnectionState.AUTHENTICATED, unavailable))
        val stale = available.copy(quality = Quality.STALE)
        assertEquals(ControlRules.AVAILABILITY_STALE, ControlRules.resendBlockReason(ConnectionState.AUTHENTICATED, stale))
        assertNull(ControlRules.resendBlockReason(ConnectionState.AUTHENTICATED, available))
        assertNull(ControlRules.resendBlockReason(ConnectionState.AUTHENTICATED, null))
    }

    @Test fun `인증 뒤 가용성 사유는 제어와 다시 보내기가 같다`() {
        val cases = listOf(
            null,
            available,
            available.copy(availability = Availability.LIMITED),
            available.copy(availability = Availability.UNKNOWN),
            unavailable,
            available.copy(quality = Quality.INVALID),
        )
        cases.forEach {
            assertEquals(
                ControlRules.controlBlockReason(ConnectionState.AUTHENTICATED, null, it),
                ControlRules.resendBlockReason(ConnectionState.AUTHENTICATED, it),
            )
        }
    }

    @Test fun `요청 기능 매핑 - REQUEST_KIND 묶음`() {
        assertEquals(VehicleFunction.DOOR, UserRequest.Door(lock = true).function)
        assertEquals(VehicleFunction.CLIMATE, UserRequest.TargetTemperature(22.0).function)
        assertEquals(VehicleFunction.CLIMATE, UserRequest.ClimateAuto(true).function)
        assertEquals(VehicleFunction.CLIMATE, UserRequest.Fan(FanLevel.LOW).function)
        assertEquals(VehicleFunction.INTERIOR_LIGHT, UserRequest.LightEnabled(true).function)
        assertEquals(VehicleFunction.INTERIOR_LIGHT, UserRequest.LightBrightness(50).function)
        assertEquals(VehicleFunction.INTERIOR_LIGHT, UserRequest.LightColor(Rgb(1, 2, 3)).function)
        assertEquals(VehicleFunction.DIGITAL_KEY, UserRequest.ProximityUnlock(true).function)
    }

    @Test fun `UiState 는 요청의 기능 가용성으로 다시 보내기 사유를 낸다`() {
        val state = VehicleUiState(
            connection = ConnectionState.AUTHENTICATED,
            snapshot = VehicleSnapshot(availability = mapOf(VehicleFunction.DOOR to unavailable)),
        )
        fun unknown(r: UserRequest) = TrackedRequest(1, 9, r, RequestState.UNKNOWN, null, confirmed = false, sentAtMs = now)
        assertEquals("사용 불가 — 도어가 열려 있음", state.resendBlockReason(unknown(UserRequest.Door(lock = false))))
        assertNull(state.resendBlockReason(unknown(UserRequest.ClimateAuto(true))))
        assertEquals("사용 불가 — 도어가 열려 있음", state.controlBlockReason(VehicleFunction.DOOR))
        assertNull(state.ackBlockReason)
    }

    @Test fun `연결 시작 - 끊김에서만 열리고 그 밖엔 연결 상태 라벨이 사유`() {
        assertNull(ControlRules.connectBlockReason(ConnectionState.DISCONNECTED))
        assertEquals("연결 중", ControlRules.connectBlockReason(ConnectionState.CONNECTING))
        assertEquals("연결됨 · 인증 대기", ControlRules.connectBlockReason(ConnectionState.CONNECTED))
        assertEquals("인증됨", ControlRules.connectBlockReason(ConnectionState.AUTHENTICATED))
    }

    @Test fun `연결 해제 - 끊김에서만 막히고 연결 중·인증 대기·인증됨에서는 열린다`() {
        assertEquals(ControlRules.DISCONNECT_BLOCKED, ControlRules.disconnectBlockReason(ConnectionState.DISCONNECTED))
        listOf(ConnectionState.CONNECTING, ConnectionState.CONNECTED, ConnectionState.AUTHENTICATED)
            .forEach { assertNull(it.name, ControlRules.disconnectBlockReason(it)) }
    }
}
