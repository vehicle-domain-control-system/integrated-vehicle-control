package com.vdcs.mobile.ui.rules

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class ClimateRulesTest {
    @Test fun `끝값에서 같은 방향 스텝은 막힌다`() {
        assertNotNull(ClimateRules.targetStepBlockReason(17.0, -1))
        assertNull(ClimateRules.targetStepBlockReason(17.0, +1))
        assertNotNull(ClimateRules.targetStepBlockReason(27.0, +1))
        assertNull(ClimateRules.targetStepBlockReason(27.0, -1))
        assertNull(ClimateRules.targetStepBlockReason(22.0, -1))
        assertEquals("최고 27.0°C — 더 올릴 수 없습니다", ClimateRules.targetStepBlockReason(27.0, +1))
        assertNotNull(ClimateRules.targetStepBlockReason(30.0, +1))
        assertNotNull(ClimateRules.targetStepBlockReason(15.0, -1))
    }

    @Test fun `스텝은 0_5 격자이고 범위 밖에서는 범위 안으로 들어온다`() {
        assertEquals(22.5, ClimateRules.stepTarget(22.0, +1), 0.0)
        assertEquals(26.5, ClimateRules.stepTarget(27.0, -1), 0.0)
        assertEquals(27.0, ClimateRules.stepTarget(30.0, -1), 0.0)
        assertEquals(17.0, ClimateRules.stepTarget(15.0, +1), 0.0)
    }

    @Test fun `링은 범위 안 위치 - 범위 밖이면 끝에 멈춘다`() {
        assertEquals(0f, ClimateRules.ringFraction(17.0), 0f)
        assertEquals(0.5f, ClimateRules.ringFraction(22.0), 0.0001f)
        assertEquals(1f, ClimateRules.ringFraction(27.0), 0f)
        assertEquals(1f, ClimateRules.ringFraction(31.5), 0f)
        assertEquals(0f, ClimateRules.ringFraction(10.0), 0f)
    }

    @Test fun `이름 붙은 값은 모를 때도 이름이 남는다`() {
        assertEquals("차량 반영 —", ValueFormat.labeled("차량 반영", ValueFormat.NO_DATA_VALUE).text)
    }
}
