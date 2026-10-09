package com.vdcs.mobile.ui.rules

import com.vdcs.mobile.domain.model.ClimateState
import com.vdcs.mobile.domain.model.DigitalKeyState
import com.vdcs.mobile.domain.model.DoorState
import com.vdcs.mobile.domain.model.EcuHealth
import com.vdcs.mobile.domain.model.FaultCategory
import com.vdcs.mobile.domain.model.HeatRemoval
import com.vdcs.mobile.domain.model.LightState
import com.vdcs.mobile.domain.model.Qualified
import com.vdcs.mobile.domain.model.RearProximity
import com.vdcs.mobile.domain.model.RearState
import com.vdcs.mobile.domain.model.WindowFault
import com.vdcs.mobile.domain.model.WindowState

object VehicleValueRules {
    const val AUTO_UNLOCK_ORIGIN = "차량 근접 판정에 의한 자동 해제 — 사용자 요청 아님"
    const val LIGHT_COLOR_NOTE = "일반 조명에만 적용"

    const val NOT_RECEIVED_TEXT = "수신 없음"
    val NOT_RECEIVED_VALUE = ValueText(NOT_RECEIVED_TEXT, null, shown = false)

    private const val NO_AUTO_UNLOCK_TEXT = "기록 없음"
    private val NO_AUTO_UNLOCK_VALUE = ValueText(NO_AUTO_UNLOCK_TEXT, null, shown = true)

    private const val ECU_FAULTS_UNTRUSTED_TEXT = "고장 여부 ${ValueFormat.UNTRUSTED_TEXT}"
    private const val ECU_CATEGORY_UNTRUSTED_TEXT = "오류 분류 ${ValueFormat.UNTRUSTED_TEXT}"
    private val ECU_FAULTS_UNTRUSTED_VALUE = ValueText(ECU_FAULTS_UNTRUSTED_TEXT, null, shown = false)
    private val ECU_CATEGORY_UNTRUSTED_VALUE = ValueText(ECU_CATEGORY_UNTRUSTED_TEXT, null, shown = false)

    const val PHYSICAL_FEEDBACK_SUPPORTED = "지원 — 적용 결과가 실제 점등 확인입니다"
    const val PHYSICAL_FEEDBACK_UNSUPPORTED = "미지원 — 적용 결과는 지시 반영일 뿐 실제 점등 확인이 아닙니다"

    fun integrityText(door: DoorState?, nowMs: Long): ValueText =
        ValueFormat.value(door?.integrity, nowMs) { it.label }

    const val REAR_DISTANCE_NAME = "후방 거리"

    fun rearText(rear: RearState?, nowMs: Long): ValueText {
        if (rear == null) return ValueFormat.NO_DATA_VALUE
        val status = rear.status
        if (!status.quality.isDisplayable) return ValueFormat.value(status, nowMs) { it.label }
        return when (val proximity = status.value ?: RearProximity.UNKNOWN) {
            RearProximity.VALID_DISTANCE -> ValueFormat.value(rearDistance(rear), nowMs, ValueFormat::centimeters)
            RearProximity.NO_OBJECT -> ValueFormat.value(status, nowMs) { it.label }
            RearProximity.UNAVAILABLE, RearProximity.FAULT, RearProximity.RECOVERING, RearProximity.INACTIVE, RearProximity.UNKNOWN ->
                ValueText(proximity.label, ValueFormat.staleNoteOrNull(status.quality, status.receivedAtMs, nowMs), shown = false)
        }
    }

    private fun rearDistance(rear: RearState): Qualified<Double> =
        rear.distanceCm.copy(quality = worst(rear.distanceCm.quality, rear.status.quality))

    fun ecuName(name: String): String = ECU_NAMES[name]?.let { "$it ($name)" } ?: name

    private val ECU_NAMES = mapOf("BCM" to "차체 제어 장치", "CIS" to "실내 감지 장치", "VSS" to "경고음 장치")

    fun ecuStatusText(ecu: EcuHealth, nowMs: Long): ValueText =
        ValueFormat.value(Qualified(ecu, ecu.quality, ecu.receivedAtMs), nowMs) {
            it.state + if (it.recovering) " · 복구 중" else ""
        }

    fun ecuFaultsText(ecu: EcuHealth): ValueText {
        if (!ecu.quality.isDisplayable) return ECU_FAULTS_UNTRUSTED_VALUE
        val list = if (ecu.faults.isEmpty()) "고장 없음" else "고장: " + ecu.faults.joinToString(", ")
        val stale = if (ecu.quality.isTrusted) "" else " (최신 아님)"
        return ValueText(list + stale, null, shown = true, alert = ecu.faults.isNotEmpty())
    }

    fun ecuReceivedText(ecu: EcuHealth, nowMs: Long): String? {
        val carriedByStatus = ecu.quality.isDisplayable &&
            ValueFormat.staleNoteOrNull(ecu.quality, ecu.receivedAtMs, nowMs) != null
        return if (carriedByStatus) null else ValueFormat.receivedText(ecu.receivedAtMs, nowMs)
    }

    fun lastAutoUnlockText(key: DigitalKeyState?, nowMs: Long): ValueText {
        val last = key?.lastAutoUnlock ?: return NO_AUTO_UNLOCK_VALUE
        return ValueFormat.value(last, nowMs) { it.label }
    }

    fun heatRemovalText(climate: ClimateState?, nowMs: Long): ValueText {
        val v = ValueFormat.value(climate?.heatRemoval, nowMs) { it.label }
        return v.copy(alert = v.shown && climate?.heatRemoval?.value == HeatRemoval.OVERHEAT_CUTOFF)
    }

    fun physicalFeedbackText(light: LightState?, nowMs: Long): ValueText =
        ValueFormat.value(light?.physicalFeedbackSupported, nowMs) {
            if (it) PHYSICAL_FEEDBACK_SUPPORTED else PHYSICAL_FEEDBACK_UNSUPPORTED
        }

    fun ecuCategoryText(ecu: EcuHealth): ValueText? {
        val category = ecu.faultCategory ?: return null
        if (!ecu.quality.isDisplayable) return ECU_CATEGORY_UNTRUSTED_VALUE
        val stale = if (ecu.quality.isTrusted) "" else " (최신 아님)"
        return ValueText(category.label + stale, null, shown = true, alert = category != FaultCategory.NONE)
    }

    fun ecuFaultDetailText(ecu: EcuHealth): ValueText? {
        if (!ecu.quality.isDisplayable || ecu.faultCategory == null || ecu.faultCategory == FaultCategory.NONE) return null
        val parts = listOfNotNull(
            ecu.faultCode?.let { "코드 $it" },
            ecu.affectedFunctions.takeIf { it.isNotEmpty() }?.let { fs -> "영향: " + fs.sortedBy { it.raw }.joinToString(", ") { it.label } },
        )
        return if (parts.isEmpty()) null else ValueText(parts.joinToString(" · "), null, shown = true)
    }

    fun ecuActionText(ecu: EcuHealth): String? =
        faultActionText(ecu.faultCategory?.takeIf { ecu.quality.isDisplayable })

    const val WINDOW_NOT_RECEIVED_TEXT = "창문 상태 수신 없음"

    fun windowNothingReceived(window: WindowState?, fault: WindowFault?): Boolean {
        val values: List<Qualified<*>> = listOfNotNull(
            window?.motion, window?.ecuState, window?.positionClosedPercent, window?.fullyOpen,
            window?.fullyClosed, window?.antiPinch, window?.reversing,
            fault?.category, fault?.code, fault?.status,
        )
        return values.none { it.quality.isDisplayable }
    }

    fun windowFaultActionText(fault: WindowFault?): String? = faultActionText(fault?.category.displayableValue())

    fun faultActionText(category: FaultCategory?): String? = when (category) {
        FaultCategory.SENSOR -> "센서 값을 믿을 수 없습니다 — 관련 표시값으로 판단하지 말고 차량에서 직접 확인하세요"
        FaultCategory.FUNCTION -> "해당 기능이 정상 동작하지 않을 수 있습니다 — 사용을 멈추고 점검을 받으세요"
        FaultCategory.COMM -> "차량 내부 통신 오류입니다 — 잠시 뒤 다시 확인하고, 계속되면 점검을 받으세요"
        FaultCategory.NONE, null -> null
    }

    fun lastReceivedText(lastReceivedAtMs: Long?, nowMs: Long): ValueText =
        if (lastReceivedAtMs == null) NOT_RECEIVED_VALUE
        else ValueText(ValueFormat.receivedText(lastReceivedAtMs, nowMs), null, shown = true)
}
