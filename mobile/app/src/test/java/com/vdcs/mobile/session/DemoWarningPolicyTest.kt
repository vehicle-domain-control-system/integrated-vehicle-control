package com.vdcs.mobile.session

import com.vdcs.mobile.domain.model.Severity
import com.vdcs.mobile.domain.model.WarningType
import org.junit.Assert.assertEquals
import org.junit.Test

class DemoWarningPolicyTest {
    @Test fun `끼임·잔류 탑승자는 긴급만`() {
        assertEquals(listOf(Severity.EMERGENCY), DemoWarningPolicy.choices(WarningType.PINCH))
        assertEquals(listOf(Severity.EMERGENCY), DemoWarningPolicy.choices(WarningType.OCCUPANT_REMAINING))
    }

    @Test fun `후방만 주의·긴급 중 고른다`() {
        assertEquals(listOf(Severity.CAUTION, Severity.EMERGENCY), DemoWarningPolicy.choices(WarningType.REAR))
    }

    @Test fun `어떤 종류도 정보 등급을 만들지 않는다`() {
        WarningType.entries.forEach { assertEquals("$it", false, Severity.INFO in DemoWarningPolicy.choices(it)) }
    }
}
