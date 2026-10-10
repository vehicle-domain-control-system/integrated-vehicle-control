package com.vdcs.mobile.ui.screen

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class EditDraftTest {
    private val t0 = 1_000_000L

    @Test fun `시작값은 보고값, 없으면 fallback`() {
        assertEquals(Draft(23.5, editedAtMs = null, lastReported = 23.5), Draft.start(23.5, 22.0))
        assertEquals(Draft(22.0, editedAtMs = null, lastReported = null), Draft.start<Double>(null, 22.0))
    }

    @Test fun `손대기 전에는 보고값을 따라간다`() {
        assertEquals(24.0, Draft.start(23.5, 22.0).synced(24.0, t0).value, 0.0)
    }

    @Test fun `보고값이 없어지면(보여 줄 수 없음) 마지막 값을 유지`() {
        assertEquals(23.5, Draft.start(23.5, 22.0).synced(null, t0).value, 0.0)
    }

    @Test fun `편집 중에는 보고값이 와도 덮어쓰지 않는다`() {
        val editing = Draft.start(23.5, 22.0).edit(25.0, t0)
        assertTrue(editing.isEditing(t0 + EDIT_IDLE_MS - 1))
        assertEquals(25.0, editing.synced(20.0, t0 + EDIT_IDLE_MS - 1).value, 0.0)
    }

    @Test fun `버린 편집은 idle 뒤 보고값으로 돌아간다`() {
        val abandoned = Draft.start(23.5, 22.0).edit(25.0, t0)
        assertFalse(abandoned.isEditing(t0 + EDIT_IDLE_MS))
        val synced = abandoned.synced(20.0, t0 + EDIT_IDLE_MS)
        assertEquals(20.0, synced.value, 0.0)
        assertFalse(synced.isEditing(t0 + EDIT_IDLE_MS))
        assertEquals(20.5, synced.edit(20.5, t0 + EDIT_IDLE_MS + 1).value, 0.0)
    }

    @Test fun `idle 전 추가 edit 은 타이머를 연장한다`() {
        val first = Draft.start(23.5, 22.0).edit(24.0, t0)
        val later = first.edit(24.5, t0 + 4_000)
        assertTrue(later.isEditing(t0 + EDIT_IDLE_MS + 1))
        assertEquals(24.5, later.synced(20.0, t0 + EDIT_IDLE_MS + 1).value, 0.0)
        assertEquals(EDIT_IDLE_MS - 1_000, later.idleRemainingMs(t0 + 5_000))
        assertEquals(20.0, later.synced(20.0, t0 + 4_000 + EDIT_IDLE_MS).value, 0.0)
    }

    @Test fun `commit 직후에는 보낸 값 유지`() {
        val sent = Draft.start(23.5, 22.0).edit(25.0, t0).committed(t0 + 100)
        assertTrue(sent.isEditing(t0 + 100))
        assertEquals(25.0, sent.value, 0.0)
        assertEquals(25.0, sent.synced(23.5, t0 + 100).value, 0.0)
    }

    @Test fun `commit 후 보고값이 바뀌면 즉시 반영`() {
        val sent = Draft.start(23.5, 22.0).edit(25.0, t0).committed(t0 + 100)
        val applied = sent.synced(24.5, t0 + 300)
        assertEquals(24.5, applied.value, 0.0)
        assertFalse(applied.isEditing(t0 + 300))
    }

    @Test fun `거부(보고값 그대로)면 idle 뒤 보고값`() {
        val sent = Draft.start(23.5, 22.0).edit(25.0, t0).committed(t0 + 100)
        assertEquals(25.0, sent.synced(23.5, t0 + 100 + EDIT_IDLE_MS - 1).value, 0.0)
        assertEquals(23.5, sent.synced(23.5, t0 + 100 + EDIT_IDLE_MS).value, 0.0)
    }

    @Test fun `편집 중이 아닐 때 남은 시간은 0`() {
        assertEquals(0L, Draft.start(23.5, 22.0).idleRemainingMs(t0))
    }
}
