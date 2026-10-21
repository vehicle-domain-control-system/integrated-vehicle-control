package com.vdcs.mobile.data

import com.vdcs.mobile.domain.logic.ResultDeadlines

data class VdcsConfig(
    val reassemblyTimeoutMs: Long = 1_000,

    val resultObserveMs: Long = 3_000,
    val fastStaleMs: Long = 600,
    val slowStaleMs: Long = 3_000,
    val heartbeatMs: Long = 1_000,
    val queryTimeoutMs: Long = 3_000,
    val syncRetryMs: Long = 2_000,
    val reconnectMaxAttempts: Int = 5,
    val reconnectWindowMs: Long = 60_000,
    val queryBusyRetryMs: Long = 400,
    val tickMs: Long = 200,
    val scanTimeoutMs: Long = 10_000,
    val gattConnectTimeoutMs: Long = 10_000,
    val gattOpTimeoutMs: Long = 5_000,
    val bondTimeoutMs: Long = 30_000,
    val disconnectGraceMs: Long = 3_000,

    val finalResultSettingMs: Long = 2_000,
    val finalResultDoorMs: Long = 3_000,
    val finalResultClimateMs: Long = 5_000,
    val recentRequestLimit: Int = 5,
    val recoveryQueryMax: Int = 8,
    val warningHistoryLimit: Int = 50,
) {
    fun resultDeadlines() = ResultDeadlines(
        firstResponseMs = resultObserveMs,
        settingMs = finalResultSettingMs,
        doorMs = finalResultDoorMs,
        climateMs = finalResultClimateMs,
    )
}
