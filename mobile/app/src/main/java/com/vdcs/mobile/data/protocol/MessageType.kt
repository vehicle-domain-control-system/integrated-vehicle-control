package com.vdcs.mobile.data.protocol

object MessageType {
    const val M_REQUEST = 0x10
    const val M_QUERY = 0x11
    const val M_WARNING_ACK = 0x12

    const val M_RESULT = 0x20
    const val M_WARNING = 0x21
    const val M_AVAILABILITY = 0x22
    const val M_DIGITAL_STATUS = 0x23
    const val M_DIGITAL_RESULT = 0x24
    const val M_USER_SETTINGS = 0x25
    const val M_BCM_DOOR_STATE = 0x30
    const val M_BCM_CLIMATE_STATE = 0x31
    const val M_BCM_LIGHT_STATE = 0x32
    const val M_BCM_STATUS = 0x33
    const val M_CIS_ENVIRONMENT = 0x34
    const val M_CIS_OCCUPANT = 0x35
    const val M_CIS_REAR = 0x36
    const val M_CIS_STATUS = 0x37
    const val M_WINDOW_STATE = 0x38
    const val M_WINDOW_FAULT = 0x39
    const val M_VSS_STATUS = 0x3A
    const val M_QUERY_END = 0x40

    val PAYLOAD_LENGTH: Map<Int, Int> = mapOf(
        M_REQUEST to 12, M_QUERY to 20, M_WARNING_ACK to 9,
        M_RESULT to 44, M_WARNING to 40, M_AVAILABILITY to 62,
        M_DIGITAL_STATUS to 30, M_DIGITAL_RESULT to 35, M_USER_SETTINGS to 36,
        M_BCM_DOOR_STATE to 40, M_BCM_CLIMATE_STATE to 44, M_BCM_LIGHT_STATE to 45,
        M_BCM_STATUS to 42, M_CIS_ENVIRONMENT to 60, M_CIS_OCCUPANT to 45,
        M_CIS_REAR to 39, M_CIS_STATUS to 54, M_WINDOW_STATE to 42,
        M_WINDOW_FAULT to 42, M_VSS_STATUS to 48, M_QUERY_END to 24,
    )
}
