package com.vdcs.mobile.domain.repository

import com.vdcs.mobile.domain.model.ConnectionState
import kotlinx.coroutines.flow.Flow

interface ConnectionController {
    val connectionState: Flow<ConnectionState>

    val registered: Flow<Boolean>

    val domainBootId: Flow<Long?>

    val connectionDetail: Flow<String?>

    suspend fun connect()

    suspend fun disconnect()

    suspend fun unregister()

    fun setForeground(foreground: Boolean)
}
