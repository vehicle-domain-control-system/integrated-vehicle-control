package com.vdcs.mobile.ui.rules

import com.vdcs.mobile.domain.model.CabinEnvironment
import com.vdcs.mobile.domain.model.ConnectionState
import com.vdcs.mobile.domain.model.DoorIntegrity
import com.vdcs.mobile.domain.model.DoorState
import com.vdcs.mobile.domain.model.LockState
import com.vdcs.mobile.domain.model.OpenState
import com.vdcs.mobile.domain.model.Qualified
import com.vdcs.mobile.domain.model.Quality
import com.vdcs.mobile.domain.model.ReadState
import com.vdcs.mobile.domain.model.Severity
import com.vdcs.mobile.domain.model.Warning
import com.vdcs.mobile.domain.model.WarningType
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class HomeRulesTest {
    private val now = 100_000L
    private fun <T> q(v: T?, quality: Quality = Quality.OK) = Qualified(v, quality, now - 5_000)
    private fun door(lock: Qualified<LockState>, open: Qualified<OpenState> = q(OpenState.CLOSED)) =
        DoorState(lock, open, q(DoorIntegrity.NORMAL))
    private fun env(t: Qualified<Double>) = CabinEnvironment(t, q(40.0), q(100L))
    private fun warning(active: Boolean, quality: Quality = Quality.OK, severity: Severity = Severity.CAUTION) =
        Warning(WarningType.REAR, active, severity, quality, occurrenceId = 1, domainBootId = 1, read = ReadState.UNREAD, receivedAtMs = now, ageMs = 0)

    @Test fun `OK 값은 문장 그대로 - 잠겨 있음 · 도어 모두 닫힘 · 실내 온도`() {
        assertEquals(
            StatusSentence("잠겨 있음", "${HomeRules.DOOR_CLOSED_TEXT} · 실내 24.5°C"),
            HomeRules.statusSentence(door(q(LockState.LOCKED)), env(q(24.5)), now),
        )
        assertEquals("잠금 해제됨", HomeRules.lockText(door(q(LockState.UNLOCKED)), now))
    }

    @Test fun `STALE 값은 최신 아님을 글자로 말한다`() {
        assertEquals("잠겨 있음 (${HomeRules.STALE_SUFFIX})", HomeRules.lockText(door(q(LockState.LOCKED, Quality.STALE)), now))
        assertEquals("실내 21.5°C (${HomeRules.STALE_SUFFIX})", HomeRules.cabinTemperatureText(env(q(21.5, Quality.STALE)), now))
    }

    @Test fun `INVALID 는 확인 불가 - 값을 단정하지 않는다`() {
        assertEquals("잠금 ${ValueFormat.UNTRUSTED_TEXT}", HomeRules.lockText(door(q(LockState.LOCKED, Quality.INVALID)), now))
        val temp = HomeRules.cabinTemperatureText(env(q(21.5, Quality.INVALID)), now)
        assertEquals("실내 온도 ${ValueFormat.UNTRUSTED_TEXT}", temp)
        assertFalse(temp.contains("21.5"))
    }

    @Test fun `NO_DATA·미수신은 값 없음 표기 - 잠김·닫힘으로 단정하지 않는다`() {
        assertEquals("잠금 ${ValueFormat.NO_DATA_TEXT}", HomeRules.lockText(null, now))
        assertEquals("도어 ${ValueFormat.NO_DATA_TEXT}", HomeRules.doorOpenText(door(q(LockState.LOCKED), q(null, Quality.NO_DATA)), now))
        assertEquals("실내 온도 ${ValueFormat.NO_DATA_TEXT}", HomeRules.cabinTemperatureText(null, now))
    }

    @Test fun `한 번도 받지 못했으면 값 칸 대신 수신 전이라는 상황을 말한다`() {
        val s = HomeRules.statusSentence(null, null, now)
        assertEquals(StatusSentence(HomeRules.NOT_RECEIVED_HEADLINE, HomeRules.NOT_RECEIVED_DETAIL), s)
        assertFalse(s.headline.startsWith("잠금"))
    }

    @Test fun `일부라도 받았으면 칸마다 품질 문구 - 받지 못한 칸은 값 없음 표기`() {
        assertEquals(
            StatusSentence("잠겨 있음", "${HomeRules.DOOR_CLOSED_TEXT} · 실내 온도 ${ValueFormat.NO_DATA_TEXT}"),
            HomeRules.statusSentence(door(q(LockState.LOCKED)), null, now),
        )
        assertEquals(
            StatusSentence("잠금 ${ValueFormat.NO_DATA_TEXT}", "도어 ${ValueFormat.NO_DATA_TEXT} · 실내 24.5°C"),
            HomeRules.statusSentence(null, env(q(24.5)), now),
        )
    }

    @Test fun `차량이 모른다고 보고한 값(UNKNOWN)은 OK 품질이어도 확인 불가`() {
        assertEquals("잠금 ${ValueFormat.UNTRUSTED_TEXT}", HomeRules.lockText(door(q(LockState.UNKNOWN)), now))
        assertEquals("도어 ${ValueFormat.UNTRUSTED_TEXT}", HomeRules.doorOpenText(door(q(LockState.LOCKED), q(OpenState.UNKNOWN)), now))
    }

    @Test fun `도어 열림은 어느 도어인지 말하지 않는다 - 최신 아니면 그것도 붙는다`() {
        assertEquals(HomeRules.DOOR_OPEN_TEXT, HomeRules.doorOpenText(door(q(LockState.UNLOCKED), q(OpenState.OPEN)), now))
        assertEquals(
            "${HomeRules.DOOR_OPEN_TEXT} (${HomeRules.STALE_SUFFIX})",
            HomeRules.doorOpenText(door(q(LockState.UNLOCKED), q(OpenState.OPEN, Quality.STALE)), now),
        )
    }

    @Test fun `어떤 문장도 믿을 수 없는 값을 정상 문장으로 말하지 않는다`() {
        val trusted = HomeRules.statusSentence(door(q(LockState.LOCKED), q(OpenState.CLOSED)), env(q(21.5)), now)
        for (quality in listOf(Quality.STALE, Quality.INVALID, Quality.NO_DATA)) {
            val s = HomeRules.statusSentence(door(q(LockState.LOCKED, quality), q(OpenState.CLOSED, quality)), env(q(21.5, quality)), now)
            assertTrue("$quality → $s", s.headline != trusted.headline)
            val marked = s.detail.split(" · ")
            assertEquals(2, marked.size)
            marked.forEach {
                assertTrue("$quality → $it", it.contains(HomeRules.STALE_SUFFIX) || it.contains(ValueFormat.UNTRUSTED_TEXT) || it.contains(ValueFormat.NO_DATA_TEXT))
            }
        }
    }

    @Test fun `값 한 칸 색 규칙 - 칩과 같은 판정`() {
        assertEquals(ChipTone.UNKNOWN, HomeRules.tone(ValueFormat.value(q(1.0, Quality.INVALID), now) { "$it" }))
        assertEquals(ChipTone.UNKNOWN, HomeRules.tone(ValueFormat.NO_DATA_VALUE))
        assertEquals(ChipTone.STALE, HomeRules.tone(ValueFormat.value(q(1.0, Quality.STALE), now) { "$it" }))
        assertEquals(ChipTone.NORMAL, HomeRules.tone(ValueFormat.value(q(1.0), now) { "$it" }))
        assertEquals(ChipTone.UNKNOWN, HomeRules.tone(ValueFormat.value(q<Double>(null, Quality.NO_DATA), now) { "$it" }))
    }

    @Test fun `경고 요약 - 발생 중이면 주의, 건수와 모의 표기`() {
        val s = HomeRules.warningSummary(listOf(warning(true), warning(false)), ConnectionState.AUTHENTICATED, WarningRules.SIMULATED_TEXT)
        assertEquals(ChipTone.ATTENTION, s.tone)
        assertTrue(s.text.startsWith("발생 중 경고 1건"))
        assertTrue(s.text.endsWith(WarningRules.SIMULATED_TEXT))
    }

    @Test fun `경고 요약 - 동기화 전·보고 없음은 '없음' 으로 단정하지 않는다`() {
        assertEquals(StatusChip(WarningRules.NOT_SYNCED_TEXT, ChipTone.UNKNOWN), HomeRules.warningSummary(emptyList(), ConnectionState.CONNECTED, null))
        assertEquals(StatusChip(WarningRules.NO_REPORT_TEXT, ChipTone.UNKNOWN), HomeRules.warningSummary(emptyList(), ConnectionState.AUTHENTICATED, null))
        assertEquals(
            StatusChip(WarningRules.NONE_ACTIVE_TEXT, ChipTone.NORMAL),
            HomeRules.warningSummary(listOf(warning(false)), ConnectionState.AUTHENTICATED, null),
        )
    }

    @Test fun `경고 요약 - 확인 불가 경고만 남으면 확인 불가 건수`() {
        val s = HomeRules.warningSummary(listOf(warning(false, Quality.STALE), warning(false)), ConnectionState.AUTHENTICATED, null)
        assertEquals(ChipTone.UNKNOWN, s.tone)
        assertTrue(s.text.contains("1건"))
    }

    @Test fun `빠른 제어 - 켜짐 보고일 때만 끄기, 꺼짐·모름이면 켜기`() {
        assertFalse(HomeRules.toggleRequest(true))
        assertTrue(HomeRules.toggleRequest(false))
        assertTrue(HomeRules.toggleRequest(null))
        assertEquals("조명 끄기", HomeRules.toggleLabel("조명", true))
        assertEquals("조명 켜기", HomeRules.toggleLabel("조명", null))
    }

    @Test fun `빠른 제어 채움 - 믿을 수 있는 보고가 켜짐·잠김일 때만`() {
        assertTrue(HomeRules.filled(q(true), true))
        assertTrue(HomeRules.filled(q(LockState.LOCKED), LockState.LOCKED))
        assertFalse(HomeRules.filled(q(false), true))
        assertFalse(HomeRules.filled(q(LockState.UNLOCKED), LockState.LOCKED))
        listOf(Quality.STALE, Quality.INVALID, Quality.NO_DATA).forEach { quality ->
            assertFalse(quality.name, HomeRules.filled(q(true, quality), true))
            assertFalse(quality.name, HomeRules.filled(q(LockState.LOCKED, quality), LockState.LOCKED))
        }
        assertFalse(HomeRules.filled<Boolean>(null, true))
    }

    @Test fun `빠른 제어 낭독 - 요청 · 현재 상태 · 최신 아님 표기`() {
        assertEquals("공조 켜기 · 현재 꺼짐", HomeRules.controlDescription("공조 켜기", ValueFormat.value(q(false), now, ValueFormat::onOff)))
        val stale = HomeRules.controlDescription("실내등 켜기", ValueFormat.value(q(true, Quality.STALE), now, ValueFormat::onOff))
        assertTrue(stale, stale.startsWith("실내등 켜기 · 현재 켜짐 · "))
        assertTrue(stale, stale.endsWith(HomeRules.STALE_SUFFIX))
        assertEquals(
            "잠금 · 현재 ${ValueFormat.UNTRUSTED_TEXT} · 값 무효",
            HomeRules.controlDescription("잠금", ValueFormat.value(q(LockState.LOCKED, Quality.INVALID), now) { it.label }),
        )
    }

    @Test fun `켜기·끄기 요청은 믿을 수 있는 값으로 - 최신 아닌 켜짐은 모름처럼 켜기`() {
        assertFalse(HomeRules.toggleRequest(q(true).trustedValue()))
        assertTrue(HomeRules.toggleRequest(q(true, Quality.STALE).trustedValue()))
    }

    @Test fun `연결 CTA - 끊김이면 연결 버튼, 연결·인증 대기면 막히고 상태 문구, 인증되면 없음`() {
        assertEquals(ConnectCta(null), HomeRules.connectCta(ConnectionState.DISCONNECTED))
        assertEquals(HomeRules.CONNECT_TEXT, HomeRules.connectCta(ConnectionState.DISCONNECTED)?.text)
        listOf(ConnectionState.CONNECTING, ConnectionState.CONNECTED).forEach { c ->
            assertEquals(c.name, ConnectCta(c.label), HomeRules.connectCta(c))
            assertEquals(c.name, c.label, HomeRules.connectCta(c)?.text)
        }
        assertEquals(null, HomeRules.connectCta(ConnectionState.AUTHENTICATED))
    }

    @Test fun `연결 CTA 는 연결 영역의 '연결' 버튼과 같은 차단 규칙을 쓴다`() {
        ConnectionState.entries.forEach { c ->
            HomeRules.connectCta(c)?.let { assertEquals(c.name, ControlRules.connectBlockReason(c), it.blockReason) }
        }
    }
}
