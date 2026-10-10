package com.vdcs.mobile.data

import android.content.Context
import com.vdcs.mobile.domain.model.ReadState
import com.vdcs.mobile.domain.model.Severity
import com.vdcs.mobile.domain.model.WarningRecord
import com.vdcs.mobile.domain.model.WarningType

interface WarningHistoryStorage {
    fun load(): List<WarningRecord>
    fun save(records: List<WarningRecord>)
}

class PrefsWarningHistoryStorage(context: Context, fileName: String) : WarningHistoryStorage {
    private val prefs = context.applicationContext.getSharedPreferences(fileName, Context.MODE_PRIVATE)

    override fun load(): List<WarningRecord> = WarningHistoryCodec.decode(prefs.getString(KEY, null).orEmpty())

    override fun save(records: List<WarningRecord>) {
        prefs.edit().putString(KEY, WarningHistoryCodec.encode(records)).apply()
    }

    private companion object {
        const val KEY = "records"
    }
}

object WarningHistoryCodec {
    fun encode(records: List<WarningRecord>): String = (listOf(V2_HEADER) + records.map(::encodeLine)).joinToString("\n")

    fun decode(text: String): List<WarningRecord> {
        val lines = text.lines()
        return if (lines.first() == V2_HEADER) {
            lines.drop(1).mapNotNull(::decodeV2)
        } else {
            lines.mapNotNull(::decodeV1)
        }
    }

    private fun encodeLine(r: WarningRecord): String = listOf(
        r.domainBootId, r.type.raw, r.occurrenceId, r.severity.raw,
        flag(r.active), flag(r.read == ReadState.READ), r.firstReceivedAtMs, r.updatedAtMs, flag(r.confirmedInApp),
    ).joinToString(",")

    private fun decodeV1(line: String): WarningRecord? = fields(line, V1_FIELD_COUNT)?.let(::toRecord)

    private fun decodeV2(line: String): WarningRecord? =
        fields(line, V2_FIELD_COUNT)?.let { v -> toRecord(v)?.copy(confirmedInApp = v[8] == 1L) }

    private fun fields(line: String, count: Int): List<Long>? {
        val f = line.split(",").map { it.toLongOrNull() }
        if (f.size != count || f.any { it == null }) return null
        return f.requireNoNulls()
    }

    private fun toRecord(v: List<Long>): WarningRecord? {
        return WarningRecord(
            domainBootId = v[0],
            type = WarningType.fromRaw(v[1].toInt()) ?: return null,
            occurrenceId = v[2],
            severity = Severity.fromRaw(v[3].toInt()) ?: return null,
            active = v[4] == 1L,
            read = if (v[5] == 1L) ReadState.READ else ReadState.UNREAD,
            firstReceivedAtMs = v[6],
            updatedAtMs = v[7],
        )
    }

    private fun flag(on: Boolean): Long = if (on) 1 else 0

    private const val V2_HEADER = "v2"
    private const val V1_FIELD_COUNT = 8
    private const val V2_FIELD_COUNT = 9
}
