package com.vdcs.mobile.domain.logic

import com.vdcs.mobile.domain.model.Quality
import com.vdcs.mobile.domain.model.ReadState
import com.vdcs.mobile.domain.model.Severity
import com.vdcs.mobile.domain.model.Warning
import com.vdcs.mobile.domain.model.WarningType
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class WarningHistoryTest {
    private fun w(
        type: WarningType = WarningType.REAR,
        occurrence: Long = 1,
        boot: Long = 3,
        active: Boolean = true,
        read: ReadState = ReadState.UNREAD,
        quality: Quality = Quality.OK,
        severity: Severity = Severity.CAUTION,
        at: Long = 0,
    ) = Warning(type, active, severity, quality, occurrence, boot, read, at, ageMs = 0)

    @Test fun `같은 Type 의 새 발생은 이전 발생을 덮지 않는다`() {
        val h = WarningHistory(limit = 10)
        assertTrue(h.record(w(occurrence = 1, at = 10)))
        assertTrue(h.record(w(occurrence = 2, at = 20)))
        assertEquals(listOf(2L, 1L), h.records().map { it.occurrenceId })
        assertTrue(h.records().all { it.unread })
    }

    @Test fun `키는 DOMAIN_BOOT_ID + TYPE + OCCURRENCE_ID`() {
        val h = WarningHistory(limit = 10)
        h.record(w(type = WarningType.REAR, occurrence = 1, boot = 3))
        h.record(w(type = WarningType.PINCH, occurrence = 1, boot = 3))
        h.record(w(type = WarningType.REAR, occurrence = 1, boot = 4))
        assertEquals(3, h.records().size)
    }

    @Test fun `CLEAR 주기 보고·품질 OK 아닌 ACTIVE 만으로는 발생을 만들지 않는다`() {
        val h = WarningHistory(limit = 10)
        assertFalse(h.record(w(active = false)))
        assertFalse(h.record(w(quality = Quality.NO_DATA)))
        assertTrue(h.records().isEmpty())
    }

    @Test fun `주기 보고 반복은 변경 아님 - 해제·확인·등급 변경만 갱신`() {
        val h = WarningHistory(limit = 10)
        h.record(w(at = 10))
        assertFalse(h.record(w(at = 1_010)))
        assertTrue(h.record(w(active = false, at = 2_000)))
        assertTrue(h.record(w(active = false, read = ReadState.READ, at = 3_000)))
        val r = h.records().single()
        assertFalse(r.active)
        assertFalse(r.unread)
        assertEquals(10L, r.firstReceivedAtMs)
        assertEquals(3_000L, r.updatedAtMs)
        assertTrue(h.record(w(active = false, read = ReadState.READ, severity = Severity.EMERGENCY, at = 4_000)))
    }

    @Test fun `상한 초과 - 확인된 오래된 기록부터, 모두 미확인이면 가장 오래된 기록을 버린다`() {
        val h = WarningHistory(limit = 2)
        h.record(w(occurrence = 1, at = 1))
        h.record(w(occurrence = 2, at = 2))
        h.record(w(occurrence = 2, read = ReadState.READ, at = 3))
        h.record(w(occurrence = 3, at = 4))
        assertEquals(listOf(3L, 1L), h.records().map { it.occurrenceId })
        h.record(w(occurrence = 4, at = 5))
        assertEquals(listOf(4L, 3L), h.records().map { it.occurrenceId })
    }

    @Test fun `저장본으로 복원하고 해제 시 비운다`() {
        val first = WarningHistory(limit = 10)
        first.record(w(occurrence = 1, at = 1))
        first.record(w(occurrence = 2, at = 2))
        val restored = WarningHistory(limit = 10).apply { restore(first.records()) }
        assertEquals(first.records(), restored.records())
        assertFalse(restored.record(w(occurrence = 1, at = 9)))
        restored.clear()
        assertTrue(restored.records().isEmpty())
    }

    @Test fun `앱 확인 대상은 같은 Type 의 새 발생에 밀려난 미확인 발생만 (D21f)`() {
        val h = WarningHistory(limit = 10)
        h.record(w(type = WarningType.REAR, occurrence = 1, at = 1))
        h.record(w(type = WarningType.PINCH, occurrence = 1, read = ReadState.READ, at = 2))
        h.record(w(type = WarningType.PINCH, occurrence = 2, at = 3))
        h.record(w(type = WarningType.REAR, occurrence = 2, at = 4))
        h.record(w(type = WarningType.REAR, occurrence = 3, at = 5))
        val confirmable = WarningHistory.confirmableInApp(h.records(), currentDomainBootId = 3)
        assertEquals(listOf(WarningType.REAR to 2L, WarningType.REAR to 1L), confirmable.map { it.type to it.occurrenceId })
    }

    @Test fun `앱 확인은 READ_STATE 를 바꾸지 않고 따로 남고, 미확인에서 빠진다 (D21f)`() {
        val h = WarningHistory(limit = 10)
        h.record(w(occurrence = 1, at = 1))
        h.record(w(occurrence = 2, at = 2))
        assertTrue(h.confirmInApp(h.records(), currentDomainBootId = 3, nowMs = 50))
        val (current, archived) = h.records()
        assertTrue(archived.confirmedInApp)
        assertEquals(ReadState.UNREAD, archived.read)
        assertFalse(archived.unread)
        assertEquals(50L, archived.updatedAtMs)
        assertFalse(current.confirmedInApp)
        assertTrue(current.unread)
        assertFalse(h.confirmInApp(h.records(), currentDomainBootId = 3, nowMs = 60))
        assertTrue(WarningHistory.confirmableInApp(h.records(), currentDomainBootId = 3).isEmpty())
    }

    @Test fun `앱 확인 뒤 차량 보고가 와도 앱 확인은 유지되고, 상한 정리에서 확인된 기록으로 먼저 빠진다`() {
        val h = WarningHistory(limit = 2)
        h.record(w(occurrence = 1, at = 1))
        h.record(w(occurrence = 2, at = 2))
        h.confirmInApp(h.records(), currentDomainBootId = 3, nowMs = 3)
        assertTrue(h.record(w(occurrence = 1, active = false, at = 4)))
        assertTrue(h.records().last().confirmedInApp)
        h.record(w(type = WarningType.PINCH, occurrence = 1, at = 5))
        assertEquals(listOf(WarningType.PINCH to 1L, WarningType.REAR to 2L), h.records().map { it.type to it.occurrenceId })
    }

    @Test fun `이전 Domain 부팅의 미확인 발생은 최신이어도 앱 확인 대상 - 현재 부팅의 최신 발생은 아니다 (D21f)`() {
        val h = WarningHistory(limit = 10)
        h.record(w(type = WarningType.DOOR_OPEN_AFTER_EXIT, occurrence = 5, boot = 3, at = 1))
        h.record(w(type = WarningType.REAR, occurrence = 1, boot = 4, at = 2))
        val confirmable = WarningHistory.confirmableInApp(h.records(), currentDomainBootId = 4)
        assertEquals(listOf(WarningType.DOOR_OPEN_AFTER_EXIT to 3L), confirmable.map { it.type to it.domainBootId })
        assertTrue(h.confirmInApp(h.records(), currentDomainBootId = 4, nowMs = 9))
        assertEquals(listOf(false, true), h.records().map { it.confirmedInApp })
    }

    @Test fun `현재 부팅을 모르면(문맥 미확정) 부팅 규칙을 쓰지 않는다 - 밀려난 발생만 대상 (D21f)`() {
        val h = WarningHistory(limit = 10)
        h.record(w(type = WarningType.DOOR_OPEN_AFTER_EXIT, occurrence = 5, boot = 3, at = 1))
        h.record(w(type = WarningType.REAR, occurrence = 1, boot = 4, at = 2))
        h.record(w(type = WarningType.REAR, occurrence = 2, boot = 4, at = 3))
        val confirmable = WarningHistory.confirmableInApp(h.records(), currentDomainBootId = null)
        assertEquals(listOf(WarningType.REAR to 1L), confirmable.map { it.type to it.occurrenceId })
        assertFalse(h.confirmInApp(h.records().filter { it.type == WarningType.DOOR_OPEN_AFTER_EXIT }, currentDomainBootId = null, nowMs = 9))
    }
}
