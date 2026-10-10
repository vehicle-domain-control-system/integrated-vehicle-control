package com.vdcs.mobile.data.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.content.Context
import com.vdcs.mobile.domain.logic.BondSecurity
import com.vdcs.mobile.domain.logic.DisconnectCause

@SuppressLint("MissingPermission")
internal class BleLinkProcedure(
    appContext: Context,
    private val adapter: BluetoothAdapter?,
    private val scanner: BleScanner,
    private val bonding: BleBonding,
    private val gatt: GattOperationQueue,
) {
    private val prefs = appContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    suspend fun run(onStep: (String) -> Unit, checkAlive: suspend () -> Unit): LinkOutcome {
        onStep("차량 찾는 중")
        val device = rememberedDevice() ?: scanner.findVehicle()
        checkAlive()
        if (device == null) return LinkOutcome.Settled(DisconnectCause.LINK_LOST, "차량을 찾지 못함")

        onStep("연결 중")
        gatt.open(device)
        checkAlive()

        onStep("서비스 확인 중")
        gatt.op(GattOpKind.Discover) { g, op -> if (!g.discoverServices()) op.fail(AttError.LOCAL_START_FAILED) }
            ?: throw LinkFailure("서비스 탐색 실패")
        checkAlive()
        if (!gatt.isOpen) throw LinkFailure("연결 끊김")
        if (!gatt.hasService()) throw LinkFailure("차량 서비스 없음")

        gatt.op(GattOpKind.Mtu) { g, op -> if (!g.requestMtu(GattSpec.PREFERRED_MTU)) op.fail(AttError.LOCAL_START_FAILED) }
        checkAlive()
        if (gatt.mtu < GattSpec.MIN_SUPPORTED_MTU) {
            gatt.close()
            return LinkOutcome.Settled(DisconnectCause.LINK_LOST, UNSUPPORTED_MTU_TEXT)
        }

        onStep("등록 확인 중")
        val bonded = bonding.ensureBonded(device)
        checkAlive()
        if (!bonded) {
            gatt.close()
            return LinkOutcome.Settled(DisconnectCause.AUTH_FAILED, "등록(본딩) 실패")
        }

        onStep("알림 구독 중")
        try {
            gatt.subscribe(GattSpec.CHAR_VEHICLE_DATA)
            gatt.subscribe(GattSpec.CHAR_GATEWAY_STATUS)
        } catch (e: BondMismatch) {
            return bondMismatch(e.message)
        } catch (e: LinkFailure) {
            val status = gatt.lastDisconnectStatus
            if (status != null && BondSecurity.isLinkSecurityFailure(status)) return bondMismatch("링크 끊김 (status $status)")
            throw e
        }

        checkAlive()
        prefs.edit().putString(KEY_ADDRESS, device.address).apply()
        return LinkOutcome.Linked
    }

    private fun bondMismatch(detail: String?): LinkOutcome.Settled {
        gatt.close()
        return LinkOutcome.Settled(DisconnectCause.BOND_MISMATCH, detail ?: "본딩 키 불일치")
    }

    fun forgetDevice() {
        prefs.edit().remove(KEY_ADDRESS).apply()
    }

    private fun rememberedDevice(): BluetoothDevice? {
        val address = prefs.getString(KEY_ADDRESS, null) ?: return null
        return runCatching { adapter?.getRemoteDevice(address) }.getOrNull()
    }

    private companion object {
        const val UNSUPPORTED_MTU_TEXT = "이 휴대폰은 지원하지 않습니다 — 블루투스 전송 크기(MTU) 협상 실패"
        const val PREFS = "vdcs_ble"
        const val KEY_ADDRESS = "device_address"
    }
}

internal sealed interface LinkOutcome {
    data object Linked : LinkOutcome

    data class Settled(val cause: DisconnectCause, val detail: String) : LinkOutcome
}
