package com.vdcs.mobile.data

import com.vdcs.mobile.domain.model.WarningRecord

class MemoryWarningHistoryStorage(initial: List<WarningRecord> = emptyList()) : WarningHistoryStorage {
    var saved: List<WarningRecord> = initial
        private set
    var saveCount = 0
        private set

    override fun load(): List<WarningRecord> = saved

    override fun save(records: List<WarningRecord>) {
        saved = records
        saveCount++
    }
}
