package com.vdcs.mobile.ui.rules

import com.vdcs.mobile.domain.model.ConnectionState
import com.vdcs.mobile.domain.model.Quality
import com.vdcs.mobile.domain.model.ReadState
import com.vdcs.mobile.domain.model.Severity
import com.vdcs.mobile.domain.model.Warning
import com.vdcs.mobile.domain.model.WarningRecord
import com.vdcs.mobile.domain.model.WarningType
import com.vdcs.mobile.session.AppMode
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class WarningRulesTest {
    private val now = 100_000L

    private fun warning(
        type: WarningType,
        active: Boolean,
        severity: Severity = Severity.CAUTION,
        quality: Quality = Quality.OK,
        occurrence: Long = 1,
        read: ReadState = ReadState.UNREAD,
    ) = Warning(type, active, severity, quality, occurrenceId = occurrence, domainBootId = 1, read = read, receivedAtMs = now, ageMs = 0)

    private fun record(type: WarningType, occurrence: Long, read: ReadState, active: Boolean = false) =
        WarningRecord(1, type, occurrence, Severity.CAUTION, active, read, firstReceivedAtMs = now - 60_000, updatedAtMs = now)

    @Test fun `등급 색 - 긴급과 주의는 다른 색, 믿을 수 없는 발생은 확인 불가`() {
        assertEquals(ChipTone.EMERGENCY, WarningRules.severityTone(Severity.EMERGENCY))
        assertEquals(ChipTone.ATTENTION, WarningRules.severityTone(Severity.CAUTION))
        assertEquals(ChipTone.EMERGENCY, WarningRules.activeTone(warning(WarningType.REAR, active = true, severity = Severity.EMERGENCY)))
        assertEquals(ChipTone.UNKNOWN, WarningRules.activeTone(warning(WarningType.REAR, active = true, severity = Severity.EMERGENCY, quality = Quality.STALE)))
    }

    @Test fun `배너는 발생 중 경고만 - 확인 불가·해제는 올리지 않는다`() {
        val active = warning(WarningType.REAR, active = true, quality = Quality.STALE)
        val uncertain = warning(WarningType.PINCH, active = false, quality = Quality.NO_DATA)
        val cleared = warning(WarningType.CIS_FAULT, active = false)
        assertEquals(listOf(active), WarningRules.bannerWarnings(listOf(cleared, uncertain, active)))
        assertTrue(WarningRules.bannerWarnings(listOf(cleared, uncertain)).isEmpty())
    }

    @Test fun `해제된 경고만 접는다 - 확인 불가는 펼친 쪽`() {
        val active = warning(WarningType.REAR, active = true)
        val uncertain = warning(WarningType.PINCH, active = false, quality = Quality.INVALID)
        val cleared = warning(WarningType.CIS_FAULT, active = false)
        val g = WarningRules.group(listOf(cleared, uncertain, active))
        assertEquals(listOf(active, uncertain), g.open)
        assertEquals(listOf(cleared), g.cleared)
        assertEquals("해제된 경고 1건 · 펼치기", WarningRules.clearedSummaryText(1, expanded = false))
        assertTrue(WarningRules.clearedSummaryText(1, expanded = true).endsWith("접기"))
    }

    @Test fun `동기화 전·보고 없음은 경고 없음과 구분`() {
        val cleared = listOf(warning(WarningType.REAR, active = false))
        for (c in ConnectionState.entries.filter { it != ConnectionState.AUTHENTICATED }) {
            assertEquals(c.name, WarningRules.NOT_SYNCED_TEXT, WarningRules.summaryText(cleared, c))
            assertEquals(c.name, WarningRules.NOT_SYNCED_TEXT, WarningRules.summaryText(emptyList(), c))
        }
        val ready = ConnectionState.AUTHENTICATED
        assertEquals(WarningRules.NO_REPORT_TEXT, WarningRules.summaryText(emptyList(), ready))
        assertEquals(WarningRules.NONE_ACTIVE_TEXT, WarningRules.summaryText(cleared, ready))
        val uncertain = cleared + warning(WarningType.PINCH, active = false, quality = Quality.STALE)
        assertNull(WarningRules.summaryText(uncertain, ready))
        assertNull(WarningRules.summaryText(listOf(warning(WarningType.REAR, active = true)), ready))
        assertNull(WarningRules.historySyncText(ready))
        assertEquals(WarningRules.HISTORY_NOT_SYNCED_TEXT, WarningRules.historySyncText(ConnectionState.CONNECTED))
    }

    @Test fun `모의 표기는 앱 모드로만 - 배너에도 붙는다`() {
        assertEquals(WarningRules.SIMULATED_TEXT, WarningRules.originLabel(AppMode.DEMO))
        assertNull(WarningRules.originLabel(AppMode.REAL))
        val w = warning(WarningType.REAR, active = true, severity = Severity.EMERGENCY)
        assertEquals("[긴급] 후방 물체 접근 · 발생 중 · 모의(시연)", WarningRules.bannerText(w, WarningRules.originLabel(AppMode.DEMO)))
        assertEquals("[긴급] 후방 물체 접근 · 발생 중", WarningRules.bannerText(w, null))
        assertEquals("경고 · 모의(시연)", WarningRules.sectionTitle(WarningRules.SIMULATED_TEXT))
        assertEquals("경고", WarningRules.sectionTitle(null))
    }

    @Test fun `보관된 미확인 이력 - 현재 경고로 보이는 발생과 확인된 기록은 뺀다`() {
        val current = listOf(warning(WarningType.REAR, active = true, occurrence = 3))
        val sameAsCurrent = record(WarningType.REAR, 3, ReadState.UNREAD, active = true)
        val olderUnread = record(WarningType.REAR, 2, ReadState.UNREAD)
        val read = record(WarningType.PINCH, 1, ReadState.READ)
        val archived = WarningRules.archived(listOf(sameAsCurrent, olderUnread, read), current)
        assertEquals(listOf(olderUnread, read), archived)
        assertNull(WarningRules.historyEmptyText(archived))
        assertEquals(WarningRules.HISTORY_EMPTY_TEXT, WarningRules.historyEmptyText(listOf(read)))
        assertTrue(WarningRules.recordMetaText(sameAsCurrent, now).contains("마지막 보고 발생 중"))
        assertEquals("주의 · 해제됨 · 1분 전 발생", WarningRules.recordMetaText(olderUnread, now))
    }

    @Test fun `미확인 수는 발생 이력 기준 - 발생한 적 없는 Type 의 UNREAD 주기 보고는 세지 않는다`() {
        val history = listOf(
            record(WarningType.REAR, 3, ReadState.UNREAD, active = true),
            record(WarningType.REAR, 2, ReadState.UNREAD),
            record(WarningType.PINCH, 1, ReadState.READ),
        )
        assertEquals(2, WarningRules.unreadCount(history))
        assertEquals(0, WarningRules.unreadCount(emptyList()))
        assertNull(WarningRules.unreadText(0))
        assertEquals("미확인 경고 2건", WarningRules.unreadText(2))
        assertFalse(WarningRules.unreadText(1).isNullOrBlank())
    }

    @Test fun `읽음 처리는 읽지 않은 발생에만 - OCCURRENCE_ID 0 은 확인할 발생 없음`() {
        assertTrue(WarningRules.isAcknowledgeable(warning(WarningType.REAR, active = true, occurrence = 3)))
        assertFalse(WarningRules.isAcknowledgeable(warning(WarningType.REAR, active = false, occurrence = 0)))
        assertFalse(WarningRules.isAcknowledgeable(warning(WarningType.REAR, active = true, occurrence = 3, read = ReadState.READ)))
    }

    @Test fun `미확인 문구에는 모의 표기를 붙이지 않는다 - 모의 표기는 머리 연결 줄에 한 번 (D21a)`() {
        val demo = WarningRules.originLabel(AppMode.DEMO)
        assertNotNull(demo)
        assertFalse(WarningRules.unreadText(2)!!.contains(demo!!))
        assertNull(WarningRules.originLabel(AppMode.REAL))
    }

    @Test fun `지난 경고 줄과 발생 중 카드 문구 - 상태 판정은 statusText 그대로`() {
        val cleared = warning(WarningType.REAR, active = false, severity = Severity.EMERGENCY, read = ReadState.READ)
        assertEquals("긴급 · 해제됨 · 0초 전 수신", WarningRules.clearedMetaText(cleared, now))
        assertEquals("미확인", WarningRules.readShortText(ReadState.UNREAD))
        assertEquals("확인함", WarningRules.readShortText(ReadState.READ))
        val active = warning(WarningType.REAR, active = true)
        assertEquals("발생 중 · 읽지 않음 · 모의(시연)", WarningRules.cardDetailText(active, WarningRules.SIMULATED_TEXT))
        val uncertain = warning(WarningType.PINCH, active = false, quality = Quality.STALE)
        assertEquals("확인 불가 · 읽지 않음", WarningRules.cardDetailText(uncertain, null))
    }

    @Test fun `모두 읽음 처리 대상은 읽지 않은 발생만`() {
        val list = listOf(
            warning(WarningType.REAR, active = true, occurrence = 3),
            warning(WarningType.PINCH, active = false, occurrence = 0),
            warning(WarningType.OCCUPANT_REMAINING, active = true, occurrence = 4, read = ReadState.READ),
            warning(WarningType.DOOR_OPEN_AFTER_EXIT, active = false, occurrence = 5),
        )
        assertEquals(listOf(WarningType.REAR, WarningType.DOOR_OPEN_AFTER_EXIT), WarningRules.acknowledgeable(list).map { it.type })
    }

    @Test fun `앱 확인한 보관 이력은 미확인 수에서 빠지고 "확인함 (앱)" 으로 차량 READ 와 구분된다 (D21f)`() {
        val current = listOf(warning(WarningType.REAR, active = true, occurrence = 3))
        val newest = record(WarningType.REAR, 3, ReadState.UNREAD, active = true)
        val appConfirmed = record(WarningType.REAR, 2, ReadState.UNREAD).copy(confirmedInApp = true)
        val vehicleRead = record(WarningType.REAR, 1, ReadState.READ)
        val history = listOf(newest, appConfirmed, vehicleRead)
        assertEquals(1, WarningRules.unreadCount(history))
        assertEquals(listOf(appConfirmed, vehicleRead), WarningRules.archived(history, current))
        assertEquals(WarningRules.HISTORY_EMPTY_TEXT, WarningRules.historyEmptyText(WarningRules.archived(history, current)))
        assertEquals(WarningRules.APP_CONFIRMED_TEXT, WarningRules.recordReadText(appConfirmed))
        assertEquals("확인함", WarningRules.recordReadText(vehicleRead))
        assertEquals("미확인", WarningRules.recordReadText(newest))
    }

    @Test fun `모두 읽음 처리 대상 - 현재 경고 ACK 와 밀려난 보관 이력 앱 확인을 합친다 (D21f)`() {
        val current = listOf(warning(WarningType.REAR, active = true, occurrence = 3), warning(WarningType.PINCH, active = false, occurrence = 0))
        val newest = record(WarningType.REAR, 3, ReadState.UNREAD, active = true)
        val older = record(WarningType.REAR, 2, ReadState.UNREAD)
        val targets = WarningRules.ackAllTargets(current, listOf(newest, older), currentDomainBootId = null)
        assertEquals(listOf(current.first()), targets.current)
        assertEquals(listOf(older), targets.archived)
        assertEquals(2, targets.size)
    }

    @Test fun `모두 읽음 처리 대상 - 현재 Domain 부팅이 아닌 발생도 앱 확인에 든다 (D21f)`() {
        val previousBoot = record(WarningType.DOOR_OPEN_AFTER_EXIT, 5, ReadState.UNREAD)
        val boot = previousBoot.domainBootId
        assertEquals(listOf(previousBoot), WarningRules.ackAllTargets(emptyList(), listOf(previousBoot), boot + 1).archived)
        assertTrue(WarningRules.ackAllTargets(emptyList(), listOf(previousBoot), boot).archived.isEmpty())
        assertTrue(WarningRules.ackAllTargets(emptyList(), listOf(previousBoot), currentDomainBootId = null).archived.isEmpty())
    }
}
