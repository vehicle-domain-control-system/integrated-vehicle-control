package com.vdcs.mobile.domain.logic

class SessionContext(
    private val random: () -> Long,
) {
    var registered: Boolean = false
        private set
    var domainLinkUp: Boolean = false
        private set
    var sessionReady: Boolean = false
        private set
    var deviceContextId: Long = 0
        private set
    var sessionId: Long = 0
        private set
    var domainBootId: Long = 0
        private set
    var appInstanceId: Long = 0
        private set

    var contextPending: Boolean = false
        private set

    fun newAppInstance() {
        var id: Long
        do {
            id = random() and 0xFFFF_FFFFL
        } while (id == 0L)
        appInstanceId = id
        if (hasAnyContext()) {
            sessionReady = false
            contextPending = true
        }
    }

    private fun hasAnyContext(): Boolean = sessionId != 0L || domainBootId != 0L || deviceContextId != 0L

    fun onGatewayStatus(
        registered: Boolean,
        domainLinkUp: Boolean,
        sessionReady: Boolean,
        deviceContextId: Long,
        sessionId: Long,
        domainBootId: Long,
    ): ContextChange {
        val change = when {
            this.sessionId == 0L && this.domainBootId == 0L && this.deviceContextId == 0L -> ContextChange.FIRST
            sessionId != this.sessionId ||
                domainBootId != this.domainBootId ||
                deviceContextId != this.deviceContextId -> ContextChange.CHANGED
            else -> ContextChange.SAME
        }
        this.registered = registered
        this.domainLinkUp = domainLinkUp
        this.sessionReady = sessionReady
        this.deviceContextId = deviceContextId
        this.sessionId = sessionId
        this.domainBootId = domainBootId
        if (change != ContextChange.SAME) {
            contextPending = false
        } else if (contextPending) {
            this.sessionReady = false
        }
        return change
    }

    fun onUnregistered() {
        registered = false
    }

    fun onLinkLost() {
        sessionReady = false
        domainLinkUp = false
    }

    fun matches(deviceContextId: Long, sessionId: Long, domainBootId: Long): Boolean =
        !contextPending &&
            this.deviceContextId != 0L && this.sessionId != 0L && this.domainBootId != 0L &&
            deviceContextId == this.deviceContextId &&
            sessionId == this.sessionId &&
            domainBootId == this.domainBootId

    fun hasContext(): Boolean = !contextPending && deviceContextId != 0L && sessionId != 0L && domainBootId != 0L

    val currentDomainBootId: Long? get() = if (hasContext()) domainBootId else null
}

enum class ContextChange { FIRST, SAME, CHANGED }
