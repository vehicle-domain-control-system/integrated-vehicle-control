package com.vdcs.mobile.domain.logic

import com.vdcs.mobile.domain.model.Quality
import com.vdcs.mobile.domain.model.Warning
import com.vdcs.mobile.domain.model.WarningRecord
import com.vdcs.mobile.domain.model.WarningType

class WarningHistory(private val limit: Int) {
    private data class Key(val domainBootId: Long, val type: Int, val occurrenceId: Long)

    private val records = LinkedHashMap<Key, WarningRecord>()

    fun restore(saved: List<WarningRecord>) {
        records.clear()
        saved.asReversed().forEach { records[it.key()] = it }
        trim()
    }

    fun record(w: Warning): Boolean {
        val key = Key(w.domainBootId, w.type.raw, w.occurrenceId)
        val old = records[key]
        if (old == null) {
            if (!w.active || w.quality != Quality.OK) return false
            records[key] = WarningRecord(
                domainBootId = w.domainBootId, type = w.type, occurrenceId = w.occurrenceId,
                severity = w.severity, active = true, read = w.read,
                firstReceivedAtMs = w.receivedAtMs, updatedAtMs = w.receivedAtMs,
            )
            trim()
            return true
        }
        if (old.severity == w.severity && old.active == w.active && old.read == w.read) return false
        records[key] = old.copy(severity = w.severity, active = w.active, read = w.read, updatedAtMs = w.receivedAtMs)
        return true
    }

    fun confirmInApp(targets: List<WarningRecord>, currentDomainBootId: Long?, nowMs: Long): Boolean {
        val eligible = confirmableInApp(records(), currentDomainBootId).map { it.key() }.toSet()
        val keys = targets.map { it.key() }.filter { it in eligible }
        keys.forEach { k -> records[k] = records.getValue(k).copy(confirmedInApp = true, updatedAtMs = nowMs) }
        return keys.isNotEmpty()
    }

    fun records(): List<WarningRecord> = records.values.reversed()

    fun clear() {
        records.clear()
    }

    private fun trim() {
        while (records.size > limit) {
            val victim = records.entries.firstOrNull { !it.value.unread } ?: records.entries.first()
            records.remove(victim.key)
        }
    }

    private fun WarningRecord.key() = Key(domainBootId, type.raw, occurrenceId)

    companion object {
        fun confirmableInApp(records: List<WarningRecord>, currentDomainBootId: Long?): List<WarningRecord> {
            val newerSeen = mutableSetOf<WarningType>()
            return records.filter { r ->
                val superseded = !newerSeen.add(r.type)
                val previousBoot = currentDomainBootId != null && r.domainBootId != currentDomainBootId
                r.unread && (superseded || previousBoot)
            }
        }
    }
}
