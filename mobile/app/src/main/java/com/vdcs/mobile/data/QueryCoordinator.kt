package com.vdcs.mobile.data

import com.vdcs.mobile.data.protocol.MessageType
import com.vdcs.mobile.data.protocol.VehicleMessage
import com.vdcs.mobile.domain.logic.RequestTracker
import com.vdcs.mobile.domain.repository.QueryScope

internal class QueryCoordinator(
    private val timeoutMs: Long,
    private val busyRetryMs: Long,
) {
    enum class Purpose {
        SYNC,
        FOLLOW_UP,
        USER,
    }

    data class Pending(
        val scope: QueryScope,
        val purpose: Purpose,
        val requestSession: Long = 0,
        val requestId: Long = 0,
    )

    data class Started(val id: Long, val pending: Pending)

    data class Finished(val pending: Pending, val complete: Boolean)

    private class Active(val id: Long, val pending: Pending, val startedAt: Long) {
        var received = 0
        val items = HashSet<Int>()
    }

    private var nextId = 1L
    private var active: Active? = null
    private val queue = ArrayDeque<Pending>()
    private var retryAt = 0L

    fun enqueue(p: Pending) = queue.addLast(p)

    fun enqueueSyncIfAbsent() {
        val present = active?.pending?.purpose == Purpose.SYNC || queue.any { it.purpose == Purpose.SYNC }
        if (!present) queue.addFirst(Pending(QueryScope.ALL, Purpose.SYNC))
    }

    fun startNext(now: Long): Started? {
        if (active != null || queue.isEmpty() || now < retryAt) return null
        val p = queue.removeFirst()
        val id = nextId
        nextId = if (nextId >= RequestTracker.MAX_U32) 1 else nextId + 1
        active = Active(id, p, now)
        return Started(id, p)
    }

    fun onSendFailed(started: Started, busy: Boolean, now: Long) {
        if (active?.id == started.id) active = null
        if (busy) {
            queue.addFirst(started.pending)
            retryAt = now + busyRetryMs
        }
    }

    fun onResponse(queryId: Long, item: Int?) {
        val a = active ?: return
        if (queryId == 0L || queryId != a.id) return
        a.received++
        if (item != null) a.items += item
    }

    fun onEnd(queryId: Long, status: Int, itemCount: Int, contextOk: Boolean): Finished? {
        val a = active ?: return null
        if (queryId != a.id) return null
        active = null
        val statusOk = status == STATUS_DONE || status == STATUS_NO_REQUEST || status == STATUS_PARTIAL
        val listOk = a.items.containsAll(required(a.pending.scope))
        return Finished(a.pending, complete = contextOk && statusOk && listOk && a.received == itemCount)
    }

    fun expire(now: Long): Finished? {
        val a = active ?: return null
        if (now - a.startedAt <= timeoutMs) return null
        active = null
        return Finished(a.pending, complete = false)
    }

    fun clear() {
        active = null
        queue.clear()
        retryAt = 0
    }

    companion object {
        private const val STATUS_DONE = 0
        private const val STATUS_NO_REQUEST = 1
        private const val STATUS_PARTIAL = 2

        private const val WARNING_ITEM_BASE = 0x1000

        val STATE_ITEMS: Set<Int> = setOf(
            MessageType.M_BCM_DOOR_STATE, MessageType.M_BCM_CLIMATE_STATE, MessageType.M_BCM_LIGHT_STATE,
            MessageType.M_BCM_STATUS, MessageType.M_CIS_ENVIRONMENT, MessageType.M_CIS_OCCUPANT,
            MessageType.M_CIS_REAR, MessageType.M_CIS_STATUS, MessageType.M_WINDOW_STATE,
            MessageType.M_WINDOW_FAULT, MessageType.M_VSS_STATUS,
            MessageType.M_USER_SETTINGS, MessageType.M_AVAILABILITY, MessageType.M_DIGITAL_STATUS,
        )

        val WARNING_ITEMS: Set<Int> = (1..8).map { WARNING_ITEM_BASE + it }.toSet()

        fun required(scope: QueryScope): Set<Int> = when (scope) {
            QueryScope.CURRENT_STATE -> STATE_ITEMS
            QueryScope.WARNINGS -> WARNING_ITEMS
            QueryScope.ALL -> STATE_ITEMS + WARNING_ITEMS
            QueryScope.REQUEST_RESULT -> emptySet()
        }

        fun itemOf(m: VehicleMessage): Int? = when (m) {
            is VehicleMessage.WarningMsg -> WARNING_ITEM_BASE + m.warningType
            is VehicleMessage.DoorStateMsg -> MessageType.M_BCM_DOOR_STATE
            is VehicleMessage.ClimateStateMsg -> MessageType.M_BCM_CLIMATE_STATE
            is VehicleMessage.LightStateMsg -> MessageType.M_BCM_LIGHT_STATE
            is VehicleMessage.BcmStatus -> MessageType.M_BCM_STATUS
            is VehicleMessage.CisEnvironment -> MessageType.M_CIS_ENVIRONMENT
            is VehicleMessage.CisOccupant -> MessageType.M_CIS_OCCUPANT
            is VehicleMessage.CisRear -> MessageType.M_CIS_REAR
            is VehicleMessage.CisStatus -> MessageType.M_CIS_STATUS
            is VehicleMessage.WindowStateMsg -> MessageType.M_WINDOW_STATE
            is VehicleMessage.WindowFaultMsg -> MessageType.M_WINDOW_FAULT
            is VehicleMessage.VssStatus -> MessageType.M_VSS_STATUS
            is VehicleMessage.UserSettings -> MessageType.M_USER_SETTINGS
            is VehicleMessage.Availability -> MessageType.M_AVAILABILITY
            is VehicleMessage.DigitalStatus -> MessageType.M_DIGITAL_STATUS
            is VehicleMessage.Result, is VehicleMessage.DigitalResult, is VehicleMessage.QueryEnd, is VehicleMessage.Raw -> null
        }
    }
}
