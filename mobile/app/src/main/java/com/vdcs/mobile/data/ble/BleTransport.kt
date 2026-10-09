package com.vdcs.mobile.data.ble

import com.vdcs.mobile.domain.logic.DisconnectCause
import com.vdcs.mobile.domain.logic.ReconnectStop
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.StateFlow

interface BleTransport {
    val link: StateFlow<LinkState>

    val incomingFrames: Flow<ByteArray>

    val gatewayStatus: Flow<ByteArray>

    val mtu: Int

    val foreground: Boolean

    suspend fun setForeground(foreground: Boolean)

    suspend fun connect()

    suspend fun disconnect()

    suspend fun forget()

    suspend fun readGatewayStatus(): ByteArray?

    suspend fun writeAppCommand(frame: ByteArray): WriteResult

    suspend fun writeAppState(payload: ByteArray): WriteResult
}

sealed interface LinkState {
    data class Disconnected(
        val cause: DisconnectCause?,
        val detail: String? = null,
        val reconnectStop: ReconnectStop? = null,
    ) : LinkState
    data class Connecting(val attempt: Int, val step: String) : LinkState
    data object Linked : LinkState
}

sealed interface WriteResult {
    data object Ok : WriteResult
    data class Rejected(val status: Int) : WriteResult {
        val reason: String get() = AttError.describe(status)
    }
    data object NotConnected : WriteResult
}

object AttError {
    const val BLE_VERSION_UNSUPPORTED = 0x80
    const val MESSAGE_TYPE_UNSUPPORTED = 0x81
    const val FRAGMENT_INVALID = 0x82
    const val PAYLOAD_LENGTH_INVALID = 0x83
    const val DOMAIN_LINK_DOWN = 0x84
    const val SESSION_NOT_READY = 0x85
    const val NOT_REGISTERED = 0x86
    const val QUERY_BUSY = 0x87
    const val VALUE_INVALID = 0x88
    const val NOTIFY_NOT_SUBSCRIBED = 0x89

    const val LOCAL_TIMEOUT = -1
    const val LOCAL_START_FAILED = -2

    fun describe(status: Int): String = when (status) {
        LOCAL_TIMEOUT -> "응답 시간 초과"
        LOCAL_START_FAILED -> "전송 시작 실패"
        BLE_VERSION_UNSUPPORTED -> "프로토콜 버전 불일치"
        MESSAGE_TYPE_UNSUPPORTED -> "지원하지 않는 메시지"
        FRAGMENT_INVALID -> "조각 구성 오류"
        PAYLOAD_LENGTH_INVALID -> "메시지 길이 오류"
        DOMAIN_LINK_DOWN -> "차량 중앙 연결 없음"
        SESSION_NOT_READY -> "차량 세션 준비 안 됨"
        NOT_REGISTERED -> "등록되지 않은 단말"
        QUERY_BUSY -> "다른 조회 진행 중"
        VALUE_INVALID -> "요청 값 오류"
        NOTIFY_NOT_SUBSCRIBED -> "알림 구독 안 됨"
        else -> "전송 실패 (GATT $status)"
    }
}

internal const val BLE_LOG_TAG = "VdcsBle"
