package com.vdcs.mobile.ui.rules

import com.vdcs.mobile.domain.model.AntiPinchStatus
import com.vdcs.mobile.domain.model.AutoUnlockResult
import com.vdcs.mobile.domain.model.ConnectionState
import com.vdcs.mobile.domain.model.DoorIntegrity
import com.vdcs.mobile.domain.model.FanLevel
import com.vdcs.mobile.domain.model.FaultCategory
import com.vdcs.mobile.domain.model.HeatRemoval
import com.vdcs.mobile.domain.model.LightApplied
import com.vdcs.mobile.domain.model.LightType
import com.vdcs.mobile.domain.model.LockState
import com.vdcs.mobile.domain.model.OpenState
import com.vdcs.mobile.domain.model.ReadState
import com.vdcs.mobile.domain.model.RearProximity
import com.vdcs.mobile.domain.model.ResultReason
import com.vdcs.mobile.domain.model.Rgb
import com.vdcs.mobile.domain.model.Severity
import com.vdcs.mobile.domain.model.TempDirection
import com.vdcs.mobile.domain.model.VehicleFunction
import com.vdcs.mobile.domain.model.WarningType
import com.vdcs.mobile.domain.model.WindowEcuState
import com.vdcs.mobile.domain.model.WindowFaultStatus
import com.vdcs.mobile.domain.model.WindowMotion

val ConnectionState.label: String
    get() = when (this) {
        ConnectionState.DISCONNECTED -> "연결 안 됨"
        ConnectionState.CONNECTING -> "연결 중"
        ConnectionState.CONNECTED -> "연결됨 · 인증 대기"
        ConnectionState.AUTHENTICATED -> "인증됨"
    }

val WarningType.label: String
    get() = when (this) {
        WarningType.REAR -> "후방 물체 접근"
        WarningType.OCCUPANT_REMAINING -> "차 안에 탑승자 남음"
        WarningType.PINCH -> "창문 끼임 감지"
        WarningType.BCM_FAULT -> "차체 제어 장치 고장"
        WarningType.CIS_FAULT -> "실내 감지 장치 고장"
        WarningType.WINDOW_FAULT -> "창문 장치 고장"
        WarningType.VSS_FAULT -> "경고음 장치 고장"
        WarningType.DOOR_OPEN_AFTER_EXIT -> "하차 후 도어 열림"
    }

val Severity.label: String
    get() = when (this) {
        Severity.INFO -> "정보"
        Severity.CAUTION -> "주의"
        Severity.EMERGENCY -> "긴급"
    }

val ReadState.label: String
    get() = when (this) {
        ReadState.UNREAD -> "읽지 않음"
        ReadState.READ -> "읽음 (해제 아님)"
    }

val LockState.label: String
    get() = when (this) {
        LockState.LOCKED -> "잠김"
        LockState.UNLOCKED -> "잠금 해제됨"
        LockState.UNKNOWN -> "알 수 없음"
    }

val OpenState.label: String
    get() = when (this) {
        OpenState.CLOSED -> "닫힘"
        OpenState.OPEN -> "열림"
        OpenState.UNKNOWN -> "알 수 없음"
    }

val DoorIntegrity.label: String
    get() = when (this) {
        DoorIntegrity.NORMAL -> "정상"
        DoorIntegrity.INCONSISTENT -> "상태 불일치 (잠김인데 열림)"
        DoorIntegrity.UNTRUSTED -> "신뢰 불가"
    }

val FanLevel.label: String
    get() = when (this) {
        FanLevel.OFF -> "끔"
        FanLevel.LOW -> "약"
        FanLevel.MEDIUM -> "중"
        FanLevel.HIGH -> "강"
        FanLevel.UNKNOWN -> "알 수 없음"
    }

val TempDirection.label: String
    get() = when (this) {
        TempDirection.COOL -> "냉방"
        TempDirection.HEAT -> "난방"
        TempDirection.IDLE -> "대기"
    }

val HeatRemoval.label: String
    get() = when (this) {
        HeatRemoval.NORMAL -> "정상 (차단 없음)"
        HeatRemoval.OVERHEAT_CUTOFF -> "과열 차단 — 안전 정책으로 출력 미적용"
    }

val FaultCategory.label: String
    get() = when (this) {
        FaultCategory.NONE -> "오류 없음"
        FaultCategory.SENSOR -> "센서 오류"
        FaultCategory.FUNCTION -> "기능 오류"
        FaultCategory.COMM -> "통신 오류"
    }

val WindowMotion.label: String
    get() = when (this) {
        WindowMotion.STOPPED -> "정지"
        WindowMotion.OPENING -> "열리는 중"
        WindowMotion.CLOSING -> "닫히는 중"
        WindowMotion.ANTIPINCH_REVERSING -> "끼임 방지 역전 중"
    }

val WindowEcuState.label: String
    get() = when (this) {
        WindowEcuState.INIT -> "초기화 중"
        WindowEcuState.READY -> "준비됨"
        WindowEcuState.DEGRADED -> "기능 저하"
        WindowEcuState.FAULT -> "고장"
    }

val AntiPinchStatus.label: String
    get() = when (this) {
        AntiPinchStatus.IDLE -> "대기"
        AntiPinchStatus.ACTIVE -> "진행 중"
        AntiPinchStatus.COMPLETED -> "완료"
        AntiPinchStatus.ABORTED -> "중단"
    }

val WindowFaultStatus.label: String
    get() = when (this) {
        WindowFaultStatus.NONE -> "고장 없음"
        WindowFaultStatus.ACTIVE -> "현재 고장"
        WindowFaultStatus.RECOVERING -> "복구 중"
    }

val LightType.label: String
    get() = when (this) {
        LightType.NORMAL -> "일반"
        LightType.GOODBYE -> "하차 안내"
        LightType.WARNING -> "경고 알림"
        LightType.FAULT -> "고장 알림"
    }

val LightApplied.label: String
    get() = when (this) {
        LightApplied.APPLIED -> "적용됨"
        LightApplied.APPLY_FAILED -> "적용 실패"
        LightApplied.UNKNOWN -> ValueFormat.UNTRUSTED_TEXT
    }

val AutoUnlockResult.label: String
    get() = when (this) {
        AutoUnlockResult.DONE -> "자동 해제됨"
        AutoUnlockResult.REJECTED -> "자동 해제 거부"
        AutoUnlockResult.FAILED -> "자동 해제 실패"
        AutoUnlockResult.UNKNOWN -> "자동 해제 결과 미확인"
    }

val RearProximity.label: String
    get() = when (this) {
        RearProximity.VALID_DISTANCE -> "거리 측정"
        RearProximity.NO_OBJECT -> "감지 물체 없음"
        RearProximity.UNAVAILABLE -> "사용 불가"
        RearProximity.FAULT -> "고장"
        RearProximity.RECOVERING -> "복구 중"
        RearProximity.INACTIVE -> "감지 꺼짐"
        RearProximity.UNKNOWN -> ValueFormat.UNTRUSTED_TEXT
    }

val VehicleFunction.label: String
    get() = when (this) {
        VehicleFunction.DOOR -> "도어"
        VehicleFunction.CLIMATE -> "공조"
        VehicleFunction.INTERIOR_LIGHT -> "실내 조명"
        VehicleFunction.DIGITAL_KEY -> "디지털 키"
        VehicleFunction.OCCUPANT -> "탑승자 감지"
        VehicleFunction.ENVIRONMENT -> "실내 환경"
        VehicleFunction.REAR_WARNING -> "후방 경고"
        VehicleFunction.WINDOW -> "창문"
        VehicleFunction.VSS -> "VSS"
    }

val ResultReason.label: String
    get() = when (this) {
        ResultReason.NONE -> "사유 없음"
        ResultReason.NOT_REGISTERED -> "등록되지 않은 단말"
        ResultReason.LINK_LOST -> "연결 끊김"
        ResultReason.SESSION_INVALID -> "세션이 유효하지 않음"
        ResultReason.DOOR_OPEN -> "도어가 열려 있음"
        ResultReason.STATE_UNTRUSTED -> "차량 상태를 신뢰할 수 없음"
        ResultReason.CMD_INVALID -> "잘못된 명령"
        ResultReason.NO_FEEDBACK -> "동작 확인 신호 없음"
        ResultReason.DRIVE_LIMIT_EXCEEDED -> "구동 한도 초과"
        ResultReason.ALREADY_AT_TARGET -> "이미 목표 상태 — 구동 없이 완료"
        ResultReason.LOCAL_OVERRIDE -> "차량 내부 조작이 우선함"
        ResultReason.STOP_REQUESTED -> "정지 요청됨"
        ResultReason.SUPERSEDED -> "새 요청으로 대체됨"
        ResultReason.ANTIPINCH -> "끼임 방지 동작"
        ResultReason.FAULT -> "고장"
        ResultReason.STALE -> "값이 최신 아님"
        ResultReason.NO_DATA -> "데이터 없음"
        ResultReason.NOT_READY -> "준비 안 됨"
        ResultReason.OUT_OF_RANGE -> "허용 범위 밖"
        ResultReason.SOURCE_FAILURE -> "입력원 고장"
        ResultReason.UNKNOWN_REQUEST -> "알 수 없는 요청"
        ResultReason.EXPIRED -> "요청 만료"
        ResultReason.ID_CONFLICT -> "요청 번호 충돌"
        ResultReason.POWER_NOT_ALLOWED -> "전원 상태상 허용 안 됨"
        ResultReason.OVERHEAT -> "과열"
        ResultReason.FAN_MISMATCH -> "팬 지시·측정 불일치"
        ResultReason.UNSPECIFIED -> "정의되지 않은 사유 코드"
    }

val LIGHT_PRESETS: List<Pair<String, Rgb>> = listOf(
    "흰색" to Rgb(255, 255, 255),
    "전구색" to Rgb(255, 180, 107),
    "빨강" to Rgb(255, 0, 0),
    "초록" to Rgb(0, 255, 0),
    "파랑" to Rgb(0, 0, 255),
    "보라" to Rgb(160, 32, 240),
)

val Rgb.label: String
    get() = LIGHT_PRESETS.firstOrNull { it.second == this }?.first ?: "사용자 지정 (${ValueFormat.rgbHex(this)})"
