package com.vdcs.mobile.domain.model

enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    AUTHENTICATED,
}

enum class RequestState {
    SENT,
    ACCEPTED,
    IN_PROGRESS,
    DONE,
    REJECTED,
    FAILED,
    CANCELLED,
    UNKNOWN,
}

enum class Quality(val raw: Int) {
    OK(0),
    STALE(1),
    INVALID(2),
    NO_DATA(3);

    companion object {
        fun fromRaw(raw: Int): Quality = entries.firstOrNull { it.raw == raw } ?: NO_DATA
    }
}

enum class ReadState { UNREAD, READ }
