package com.vdcs.mobile.data

import com.vdcs.mobile.data.protocol.VehicleMessage
import com.vdcs.mobile.domain.logic.RequestTracker
import com.vdcs.mobile.domain.logic.WarningHistory
import com.vdcs.mobile.domain.model.ResultReason
import com.vdcs.mobile.domain.model.TrackedRequest
import com.vdcs.mobile.domain.model.VehicleSnapshot
import com.vdcs.mobile.domain.model.WarningRecord
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

internal class VehicleStateStore(
    private val requestTracker: RequestTracker,
    private val projector: DisplayProjector,
    private val history: WarningHistory,
    private val historyStorage: WarningHistoryStorage,
) {
    private var raw = VehicleSnapshot()

    private val _warningHistory: MutableStateFlow<List<WarningRecord>>
    val warningHistory: StateFlow<List<WarningRecord>>

    init {
        history.restore(historyStorage.load())
        _warningHistory = MutableStateFlow(history.records())
        warningHistory = _warningHistory.asStateFlow()
    }

    private val _vehicle = MutableStateFlow(VehicleSnapshot())
    val vehicle: StateFlow<VehicleSnapshot> = _vehicle.asStateFlow()

    private val _requests = MutableStateFlow<List<TrackedRequest>>(emptyList())
    val requests: StateFlow<List<TrackedRequest>> = _requests.asStateFlow()

    fun apply(m: VehicleMessage, now: Long) {
        raw = VehicleMessageMapper.apply(raw, m, now).copy(lastReceivedAtMs = now)
        if (m is VehicleMessage.WarningMsg) recordWarning(m, now)
    }

    private fun recordWarning(m: VehicleMessage.WarningMsg, now: Long) {
        val w = VehicleMessageMapper.warning(m, now) ?: return
        if (history.record(w)) persistHistory()
    }

    fun confirmArchivedWarnings(records: List<WarningRecord>, currentDomainBootId: Long?, now: Long) {
        if (history.confirmInApp(records, currentDomainBootId, now)) persistHistory()
    }

    private fun persistHistory() {
        val records = history.records()
        historyStorage.save(records)
        _warningHistory.value = records
    }

    fun applyResult(m: VehicleMessage.Result, now: Long) {
        raw = raw.copy(lastReceivedAtMs = now)
        val state = VehicleMessageMapper.requestState(m.result) ?: return
        requestTracker.onResult(m.requestSession, m.requestId, state, ResultReason.fromRaw(m.reason), m.confirmed, now)
    }

    fun markPreviousContext(now: Long) = projector.markPreviousContext(now)

    fun forgetAll() {
        raw = VehicleSnapshot()
        requestTracker.clear()
        history.clear()
        persistHistory()
    }

    fun publish(now: Long) {
        _vehicle.value = projector.project(raw, now)
        _requests.value = requestTracker.snapshot()
    }
}
