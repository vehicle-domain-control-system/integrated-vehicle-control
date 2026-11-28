package com.vdcs.mobile.data.ble

import java.util.UUID

object GattSpec {
    val SERVICE: UUID = UUID.fromString("A0F00000-7C21-4B5A-9E32-5F4D4F42494C")

    val CHAR_GATEWAY_STATUS: UUID = UUID.fromString("A0F00001-7C21-4B5A-9E32-5F4D4F42494C")

    val CHAR_APP_COMMAND: UUID = UUID.fromString("A0F00002-7C21-4B5A-9E32-5F4D4F42494C")

    val CHAR_VEHICLE_DATA: UUID = UUID.fromString("A0F00003-7C21-4B5A-9E32-5F4D4F42494C")

    val CHAR_APP_STATE: UUID = UUID.fromString("A0F00004-7C21-4B5A-9E32-5F4D4F42494C")

    val CCCD: UUID = UUID.fromString("00002902-0000-1000-8000-00805F9B34FB")

    const val PREFERRED_MTU = 185
    const val MIN_MTU = 23

    const val MIN_SUPPORTED_MTU = 24

    const val HEADER_SIZE = 8
    const val PROTOCOL_VERSION = 1

    fun fragmentDataMax(negotiatedMtu: Int): Int = negotiatedMtu - 3 - HEADER_SIZE
}
