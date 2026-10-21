package com.vdcs.mobile

import android.content.Context
import com.vdcs.mobile.data.PrefsWarningHistoryStorage
import com.vdcs.mobile.data.VdcsConfig
import com.vdcs.mobile.data.VehicleRepositoryImpl
import com.vdcs.mobile.data.ble.BleConnectionManager
import com.vdcs.mobile.data.ble.BleTransport
import com.vdcs.mobile.data.demo.DemoTransport
import com.vdcs.mobile.data.protocol.BleFrameCodec
import com.vdcs.mobile.data.protocol.MessageCodec
import com.vdcs.mobile.data.protocol.Reassembler
import com.vdcs.mobile.domain.logic.FreshnessEvaluator
import com.vdcs.mobile.domain.logic.ReconnectPolicy
import com.vdcs.mobile.domain.logic.RequestTracker
import com.vdcs.mobile.domain.logic.SessionContext
import com.vdcs.mobile.domain.model.Severity
import com.vdcs.mobile.domain.model.WarningType
import com.vdcs.mobile.session.AppMode
import com.vdcs.mobile.session.DemoControls
import com.vdcs.mobile.session.SessionSource
import com.vdcs.mobile.session.VehicleSession
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.TimeoutCancellationException
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withTimeout
import java.security.SecureRandom

class AppContainer(context: Context) : SessionSource {
    private val appContext = context.applicationContext
    private val prefs = appContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    @OptIn(ExperimentalCoroutinesApi::class)
    private val dispatcher = Dispatchers.Default.limitedParallelism(1)
    private val appJob = SupervisorJob()
    private val appScope = CoroutineScope(appJob + dispatcher)

    val config = VdcsConfig()
    private val clock: () -> Long = System::currentTimeMillis
    private val secureRandom = SecureRandom()
    private val switchLock = Mutex()

    private class Assembled(val session: VehicleSession, val scope: CoroutineScope)

    private var current: Assembled = assemble(savedMode())
    private var foreground = false
    private val _session = MutableStateFlow(current.session)
    override val session: StateFlow<VehicleSession> = _session.asStateFlow()

    override fun switchMode(mode: AppMode) {
        appScope.launch {
            switchLock.withLock {
                val old = current
                if (old.session.mode == mode) return@withLock
                disconnectQuietly(old)
                old.scope.cancel()
                prefs.edit().putString(KEY_MODE, mode.name).apply()
                current = assemble(mode)
                current.session.connection.setForeground(foreground)
                _session.value = current.session
            }
        }
    }

    fun setForeground(value: Boolean) {
        appScope.launch {
            foreground = value
            current.session.connection.setForeground(value)
        }
    }

    private suspend fun disconnectQuietly(old: Assembled) {
        try {
            withTimeout(config.disconnectGraceMs) { old.session.connection.disconnect() }
        } catch (e: TimeoutCancellationException) {
        } catch (e: CancellationException) {
            throw e
        } catch (e: Exception) {
        }
    }

    private fun assemble(mode: AppMode): Assembled {
        val scope = CoroutineScope(SupervisorJob(appJob) + dispatcher)
        val transport: BleTransport
        val demo: DemoControls?
        when (mode) {
            AppMode.REAL -> {
                transport = BleConnectionManager(
                    context = appContext,
                    reconnectPolicy = ReconnectPolicy(config.reconnectMaxAttempts, config.reconnectWindowMs),
                    scope = scope,
                    clock = clock,
                    config = config,
                )
                demo = null
            }
            AppMode.DEMO -> {
                val virtual = DemoTransport(scope = scope, clock = clock)
                transport = virtual
                demo = DemoAdapter(virtual)
            }
        }
        val repository = VehicleRepositoryImpl(
            transport = transport,
            frameCodec = BleFrameCodec(),
            reassembler = Reassembler(config.reassemblyTimeoutMs),
            messageCodec = MessageCodec(),
            requestTracker = RequestTracker(config.resultDeadlines(), config.recentRequestLimit),
            freshness = FreshnessEvaluator(),
            session = SessionContext(random = { secureRandom.nextLong() }),
            warningHistoryStorage = PrefsWarningHistoryStorage(appContext, "${WARNING_HISTORY_PREFS}_${mode.name.lowercase()}"),
            config = config,
            scope = scope,
            clock = clock,
        )
        return Assembled(VehicleSession(mode, repository, repository, demo), scope)
    }

    private fun savedMode(): AppMode =
        prefs.getString(KEY_MODE, null)?.let { saved -> AppMode.entries.firstOrNull { it.name == saved } } ?: AppMode.REAL

    private class DemoAdapter(private val t: DemoTransport) : DemoControls {
        override suspend fun openDoor() = t.openDoor()
        override suspend fun closeDoor() = t.closeDoor()
        override suspend fun raiseWarning(type: WarningType, severity: Severity) = t.raiseWarning(type, severity)
        override suspend fun clearWarning(type: WarningType) = t.clearWarning(type)
        override suspend fun clearAllWarnings() = t.clearAllWarnings()
        override suspend fun setCabinTemperature(celsius: Double) = t.setCabinTemperature(celsius)
    }

    private companion object {
        const val PREFS = "vdcs_app"
        const val KEY_MODE = "mode"
        const val WARNING_HISTORY_PREFS = "vdcs_warning_history"
    }
}
