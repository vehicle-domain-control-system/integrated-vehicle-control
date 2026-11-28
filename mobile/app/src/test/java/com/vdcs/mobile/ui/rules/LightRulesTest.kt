package com.vdcs.mobile.ui.rules

import com.vdcs.mobile.domain.model.LightApplied
import com.vdcs.mobile.domain.model.LightState
import com.vdcs.mobile.domain.model.LightType
import com.vdcs.mobile.domain.model.Qualified
import com.vdcs.mobile.domain.model.Quality
import com.vdcs.mobile.domain.model.Rgb
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class LightRulesTest {
    private val now = 100_000L
    private val white = Rgb(255, 255, 255)
    private fun <T> q(v: T?, quality: Quality = Quality.OK) = Qualified(v, quality, now - 1_000)

    private fun light(
        level: Qualified<Int> = q(60),
        rgb: Qualified<Rgb> = q(white),
    ) = LightState(q(LightType.NORMAL), q(LightApplied.APPLIED), level, rgb, q(false))

    @Test fun `보고된 색과 밝기가 있을 때만 빛난다`() {
        assertEquals(LightGlow(white, 0.6f), LightRules.glow(light()))
        assertNull(LightRules.glow(null))
        assertNull(LightRules.glow(light(level = q(0))))
        assertNull(LightRules.glow(light(level = q(null, Quality.NO_DATA))))
        assertNull(LightRules.glow(light(rgb = q(null, Quality.INVALID))))
        assertTrue(LightRules.glowDescription(null, now).contains("점등 보고 없음"))
        assertEquals("현재 점등 · 흰색 · 60%", LightRules.glowDescription(light(), now))
    }

    @Test fun `현재 적용 짧은 줄 - 하나라도 모르면 흐리게`() {
        val applied = LightRules.appliedText(light(), now)
        assertEquals("현재 적용 일반 · 적용됨", applied.text)
        assertTrue(applied.shown)
        assertFalse(LightRules.appliedText(null, now).shown)
    }
}
