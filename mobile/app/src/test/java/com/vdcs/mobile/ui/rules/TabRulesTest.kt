package com.vdcs.mobile.ui.rules

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Test

class TabRulesTest {
    @Test fun `탭 순서 - 차량 공조 조명·키 알림 진단, 시작은 홈 (D21)`() {
        assertEquals(listOf("차량", "공조", "조명·키", "알림", "진단"), AppTab.entries.map { it.label })
        assertEquals(AppTab.HOME, TabRules.START_TAB)
    }

    @Test fun `머리 제목 - 홈은 내 차, 나머지는 탭 이름 (D21)`() {
        assertEquals(listOf("내 차", "공조", "조명·키", "알림", "진단"), AppTab.entries.map { it.title })
    }

    @Test fun `홈 - 히어로 3D, 상태 문장, (끊겨 있으면) 연결 버튼, 빠른 제어, 경고 줄, 연결 영역 순 (D21)`() {
        assertEquals(
            listOf(HomeSection.CAR_VIEW, HomeSection.STATUS_SENTENCE, HomeSection.QUICK_CONTROLS, HomeSection.WARNING_LINE, HomeSection.CONNECTION),
            TabRules.homeSections(showConnectCta = false),
        )
        val withCta = TabRules.homeSections(showConnectCta = true)
        assertEquals(listOf(HomeSection.STATUS_SENTENCE, HomeSection.CONNECT_CTA), withCta.subList(1, 3))
        assertEquals(HomeSection.entries.toSet(), withCta.toSet())
    }

    @Test fun `차량 상태 탭 - 시연 조작은 맨 아래, 데모 조작 창구가 없으면 빠진다`() {
        assertEquals(VehicleSection.DEMO_CONTROLS, TabRules.vehicleSections(hasDemoControls = true).last())
        assertFalse(VehicleSection.DEMO_CONTROLS in TabRules.vehicleSections(hasDemoControls = false))
        assertEquals(VehicleSection.entries.size - 1, TabRules.vehicleSections(hasDemoControls = false).size)
    }

    @Test fun `알림 탭 점 - 낭독은 미확인 문구 한 출처, 0 이면 점 없음`() {
        assertNull(WarningRules.unreadText(0))
        assertEquals("미확인 경고 3건", WarningRules.unreadText(3))
    }

    @Test fun `홈 경고 줄 - 짧은 미확인 표기, 0 이면 없음 (D21)`() {
        assertNull(WarningRules.unreadShortText(0))
        assertEquals("미확인 6", WarningRules.unreadShortText(6))
    }
}
