package com.vdcs.mobile.ui.rules

import com.vdcs.mobile.domain.model.RequestState
import com.vdcs.mobile.domain.model.ResultReason
import com.vdcs.mobile.domain.model.TrackedRequest
import com.vdcs.mobile.domain.model.UserRequest
import com.vdcs.mobile.domain.model.VehicleFunction

enum class RequestTone { PENDING, SUCCESS, FAILURE, UNCONFIRMED, NEUTRAL }

data class RequestStatusText(val label: String, val tone: RequestTone, val detail: String?)

object RequestRules {
    private const val UNKNOWN_DETAIL = "결과를 받지 못했습니다 — 성공·실패를 알 수 없음"

    fun status(t: TrackedRequest): RequestStatusText {
        val reason = t.reason?.takeIf { it != ResultReason.NONE }?.label
        val label = stateLabel(t.state)
        return when (t.state) {
            RequestState.SENT, RequestState.ACCEPTED, RequestState.IN_PROGRESS -> RequestStatusText(label, RequestTone.PENDING, reason)
            RequestState.DONE -> RequestStatusText(label, RequestTone.SUCCESS, reason)
            RequestState.REJECTED, RequestState.FAILED -> RequestStatusText(label, RequestTone.FAILURE, reason)
            RequestState.CANCELLED -> RequestStatusText(label, RequestTone.NEUTRAL, reason)
            RequestState.UNKNOWN -> RequestStatusText(label, RequestTone.UNCONFIRMED, UNKNOWN_DETAIL)
        }
    }

    private fun stateLabel(state: RequestState): String = when (state) {
        RequestState.SENT -> "전송됨"
        RequestState.ACCEPTED -> "접수됨"
        RequestState.IN_PROGRESS -> "진행 중"
        RequestState.DONE -> "완료"
        RequestState.REJECTED -> "거부"
        RequestState.FAILED -> "실패"
        RequestState.CANCELLED -> "중단"
        RequestState.UNKNOWN -> "미확인"
    }

    fun progressText(t: TrackedRequest): String? =
        t.progress?.takeIf { it != t.state }?.let { "차량 진행 응답: ${stateLabel(it)}" }

    fun mismatchText(t: TrackedRequest): String? = if (t.resultMismatch) MISMATCH_TEXT else null

    const val MISMATCH_TEXT = "결과 불일치 — 차량이 서로 다른 결과를 보냈습니다. 차량 상태를 직접 확인하세요"

    fun confirmationText(t: TrackedRequest): String? = when (t.state) {
        RequestState.DONE, RequestState.REJECTED, RequestState.FAILED, RequestState.CANCELLED ->
            if (t.confirmed) "차량 확인됨" else "차량 확인 전 (실패 아님)"
        RequestState.SENT, RequestState.ACCEPTED, RequestState.IN_PROGRESS, RequestState.UNKNOWN -> null
    }

    fun isResendable(t: TrackedRequest): Boolean = t.state == RequestState.UNKNOWN
}

val UserRequest.label: String
    get() = when (this) {
        is UserRequest.Door -> if (lock) "도어 잠금" else "도어 잠금 해제 (사용자 요청)"
        is UserRequest.TargetTemperature -> "목표 온도 ${ValueFormat.celsius(celsius)}"
        is UserRequest.ClimateAuto -> "자동 공조 ${if (enabled) "켜기" else "끄기"}"
        is UserRequest.Fan -> "팬 ${level.label}"
        is UserRequest.LightEnabled -> "실내 조명 ${if (enabled) "켜기" else "끄기"}"
        is UserRequest.LightBrightness -> "조명 밝기 $percent%"
        is UserRequest.LightColor -> "조명 색상 ${rgb.label}"
        is UserRequest.ProximityUnlock -> "근접 자동 해제 ${if (enabled) "사용" else "사용 안 함"}"
    }

val UserRequest.function: VehicleFunction
    get() = when (this) {
        is UserRequest.Door -> VehicleFunction.DOOR
        is UserRequest.TargetTemperature, is UserRequest.ClimateAuto, is UserRequest.Fan -> VehicleFunction.CLIMATE
        is UserRequest.LightEnabled, is UserRequest.LightBrightness, is UserRequest.LightColor -> VehicleFunction.INTERIOR_LIGHT
        is UserRequest.ProximityUnlock -> VehicleFunction.DIGITAL_KEY
    }
