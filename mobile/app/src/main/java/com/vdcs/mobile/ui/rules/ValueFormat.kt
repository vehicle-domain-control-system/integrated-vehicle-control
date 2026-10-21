package com.vdcs.mobile.ui.rules

import com.vdcs.mobile.domain.model.Qualified
import com.vdcs.mobile.domain.model.Quality
import com.vdcs.mobile.domain.model.Rgb
import java.util.Locale
import kotlin.math.roundToInt

data class ValueText(
    val text: String,
    val note: String? = null,
    val shown: Boolean,
    val alert: Boolean = false,
)

object ValueFormat {
    const val UNTRUSTED_TEXT = "확인 불가"
    const val NO_DATA_TEXT = "—"

    val NO_DATA_VALUE = ValueText(NO_DATA_TEXT, null, shown = false)

    fun <T> value(q: Qualified<T>?, nowMs: Long, format: (T) -> String): ValueText {
        if (q == null) return NO_DATA_VALUE
        return when (q.quality) {
            Quality.INVALID -> ValueText(UNTRUSTED_TEXT, "값 무효", shown = false)
            Quality.NO_DATA -> ValueText(NO_DATA_TEXT, "수신 값 없음", shown = false)
            Quality.OK, Quality.STALE -> {
                val v = q.value ?: return NO_DATA_VALUE
                ValueText(format(v), staleNoteOrNull(q.quality, q.receivedAtMs, nowMs), shown = true)
            }
        }
    }

    fun labeled(name: String, v: ValueText): ValueText = v.copy(text = "$name ${v.text}")

    fun staleNoteOrNull(quality: Quality, receivedAtMs: Long, nowMs: Long): String? =
        if (quality.isTrusted) null else staleNote(receivedAtMs, nowMs)

    private fun staleNote(receivedAtMs: Long, nowMs: Long): String =
        if (receivedAtMs <= 0) "최신 아님" else "${receivedText(receivedAtMs, nowMs)} · 최신 아님"

    fun receivedText(receivedAtMs: Long, nowMs: Long): String = "${ago(receivedAtMs, nowMs)} 수신"

    fun ago(atMs: Long, nowMs: Long): String {
        val s = ((nowMs - atMs).coerceAtLeast(0)) / 1000
        return when {
            s < 60 -> "${s}초 전"
            s < 3600 -> "${s / 60}분 전"
            else -> "${s / 3600}시간 전"
        }
    }

    fun celsius(v: Double): String = String.format(Locale.ROOT, "%.1f°C", v)
    fun percent(v: Int): String = "$v%"
    fun humidity(v: Double): String = String.format(Locale.ROOT, "%.0f%%", v)
    fun lux(v: Long): String = "$v lx"
    fun centimeters(v: Double): String = String.format(Locale.ROOT, "%.0f cm", v)
    fun onOff(v: Boolean): String = if (v) "켜짐" else "꺼짐"
    fun yesNo(v: Boolean): String = if (v) "예" else "아니오"
    fun presence(v: Boolean): String = if (v) "있음" else "없음"
    fun people(v: Int): String = "${v}명"
    fun rgbHex(c: Rgb): String = String.format(Locale.ROOT, "#%02X%02X%02X", c.r, c.g, c.b)

    fun stepTarget(current: Double, steps: Int): Double = ((current * 2).roundToInt() + steps) / 2.0
}
