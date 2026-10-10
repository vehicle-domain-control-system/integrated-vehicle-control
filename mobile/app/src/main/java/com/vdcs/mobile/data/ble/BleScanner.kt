package com.vdcs.mobile.data.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.os.ParcelUuid
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withTimeoutOrNull
import kotlin.coroutines.resume

@SuppressLint("MissingPermission")
internal class BleScanner(
    private val adapter: BluetoothAdapter?,
    private val timeoutMs: Long,
) {
    suspend fun findVehicle(): BluetoothDevice? {
        val scanner = adapter?.bluetoothLeScanner ?: return null
        val filter = ScanFilter.Builder().setServiceUuid(ParcelUuid(GattSpec.SERVICE)).build()
        val settings = ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build()
        return withTimeoutOrNull(timeoutMs) {
            suspendCancellableCoroutine { cont ->
                val callback = object : ScanCallback() {
                    override fun onScanResult(callbackType: Int, result: ScanResult) {
                        runCatching { scanner.stopScan(this) }
                        if (cont.isActive) cont.resume(result.device)
                    }

                    override fun onScanFailed(errorCode: Int) {
                        if (cont.isActive) cont.resume(null)
                    }
                }
                cont.invokeOnCancellation { runCatching { scanner.stopScan(callback) } }
                scanner.startScan(listOf(filter), settings, callback)
            }
        }
    }
}
