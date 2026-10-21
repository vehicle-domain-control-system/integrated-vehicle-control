package com.vdcs.mobile.domain.logic

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class BondSecurityTest {
    @Test fun `ATT 인증·권한·암호화 부족은 키 불일치`() {
        for (s in listOf(0x05, 0x08, 0x0C, 0x0F)) assertTrue("0x%02X".format(s), BondSecurity.isAttSecurityFailure(s))
    }

    @Test fun `일반 ATT 실패·앱 오류 코드는 키 불일치가 아니다`() {
        for (s in listOf(0x00, 0x01, 0x03, 0x0D, 0x80, 0x85, 0x86, 0x87, 0x101)) {
            assertFalse("0x%02X".format(s), BondSecurity.isAttSecurityFailure(s))
        }
    }

    @Test fun `링크 끊김 - 인증 실패·키 없음·MIC 실패만 키 불일치`() {
        for (s in listOf(0x05, 0x06, 0x3D)) assertTrue("0x%02X".format(s), BondSecurity.isLinkSecurityFailure(s))
        for (s in listOf(0x08, 0x13, 0x16, 0x22, 0x3E, 0x85)) assertFalse("0x%02X".format(s), BondSecurity.isLinkSecurityFailure(s))
    }

    @Test fun `키 불일치 끊김은 자동 재연결 대상이 아니다`() {
        assertFalse(ReconnectPolicy(maxAttempts = 5, windowMs = 60_000).shouldRetry(DisconnectCause.BOND_MISMATCH, 1, 0))
    }
}
