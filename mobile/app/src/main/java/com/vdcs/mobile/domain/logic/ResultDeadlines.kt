package com.vdcs.mobile.domain.logic

import com.vdcs.mobile.domain.model.UserRequest

data class ResultDeadlines(
    val firstResponseMs: Long,
    val settingMs: Long,
    val doorMs: Long,
    val climateMs: Long,
) {
    fun finalFor(request: UserRequest): Long = when (request) {
        is UserRequest.Door -> doorMs
        is UserRequest.Fan -> climateMs
        is UserRequest.TargetTemperature, is UserRequest.ClimateAuto,
        is UserRequest.LightEnabled, is UserRequest.LightBrightness, is UserRequest.LightColor,
        is UserRequest.ProximityUnlock -> settingMs
    }
}
