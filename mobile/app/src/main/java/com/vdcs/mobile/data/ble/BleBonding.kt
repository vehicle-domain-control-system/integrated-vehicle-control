package com.vdcs.mobile.data.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.withTimeoutOrNull

@SuppressLint("MissingPermission")
internal class BleBonding(
    private val appContext: Context,
    private val timeoutMs: Long,
) {
    suspend fun ensureBonded(device: BluetoothDevice): Boolean {
        if (device.bondState == BluetoothDevice.BOND_BONDED) return true
        val result = CompletableDeferred<Boolean>()
        val receiver = object : BroadcastReceiver() {
            override fun onReceive(c: Context, intent: Intent) {
                val d = intent.parcelableExtra<BluetoothDevice>(BluetoothDevice.EXTRA_DEVICE)
                if (d?.address != device.address) return
                when (intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE, BluetoothDevice.ERROR)) {
                    BluetoothDevice.BOND_BONDED -> result.complete(true)
                    BluetoothDevice.BOND_NONE -> result.complete(false)
                }
            }
        }
        val filter = IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED)
        if (Build.VERSION.SDK_INT >= 33) {
            appContext.registerReceiver(receiver, filter, Context.RECEIVER_EXPORTED)
        } else {
            appContext.registerReceiver(receiver, filter)
        }
        return try {
            when (device.bondState) {
                BluetoothDevice.BOND_BONDED -> true
                BluetoothDevice.BOND_BONDING -> awaitBond(device, result)
                else -> if (device.createBond()) {
                    awaitBond(device, result)
                } else {
                    when (device.bondState) {
                        BluetoothDevice.BOND_BONDED -> true
                        BluetoothDevice.BOND_BONDING -> awaitBond(device, result)
                        else -> false
                    }
                }
            }
        } finally {
            runCatching { appContext.unregisterReceiver(receiver) }
        }
    }

    private suspend fun awaitBond(device: BluetoothDevice, result: CompletableDeferred<Boolean>): Boolean =
        withTimeoutOrNull(timeoutMs) { result.await() } ?: (device.bondState == BluetoothDevice.BOND_BONDED)

    private inline fun <reified T> Intent.parcelableExtra(key: String): T? =
        if (Build.VERSION.SDK_INT >= 33) getParcelableExtra(key, T::class.java)
        else @Suppress("DEPRECATION") getParcelableExtra(key) as? T
}
