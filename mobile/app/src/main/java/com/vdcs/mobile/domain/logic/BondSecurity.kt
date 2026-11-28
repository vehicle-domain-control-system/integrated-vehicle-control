package com.vdcs.mobile.domain.logic

object BondSecurity {
    private val ATT_SECURITY = setOf(
        0x05,
        0x08,
        0x0C,
        0x0F,
    )

    private val LINK_SECURITY = setOf(
        0x05,
        0x06,
        0x3D,
    )

    fun isAttSecurityFailure(status: Int): Boolean = status in ATT_SECURITY

    fun isLinkSecurityFailure(status: Int): Boolean = status in LINK_SECURITY
}
