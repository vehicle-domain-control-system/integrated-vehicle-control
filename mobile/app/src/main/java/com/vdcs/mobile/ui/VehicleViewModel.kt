package com.vdcs.mobile.ui

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import androidx.lifecycle.viewmodel.initializer
import androidx.lifecycle.viewmodel.viewModelFactory
import com.vdcs.mobile.domain.model.ConnectionState
import com.vdcs.mobile.domain.model.TrackedRequest
import com.vdcs.mobile.domain.model.UserRequest
import com.vdcs.mobile.domain.model.Warning
import com.vdcs.mobile.domain.model.WarningRecord
import com.vdcs.mobile.session.AppMode
import com.vdcs.mobile.session.DemoControls
import com.vdcs.mobile.session.SessionSource
import com.vdcs.mobile.session.VehicleSession
import com.vdcs.mobile.ui.rules.AckTargets
import com.vdcs.mobile.ui.rules.ActionMessages
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.flatMapLatest
import kotlinx.coroutines.flow.flow
import kotlinx.coroutines.flow.receiveAsFlow
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch

@OptIn(ExperimentalCoroutinesApi::class)
class VehicleViewModel(
    private val source: SessionSource,
    private val clock: () -> Long = System::currentTimeMillis,
) : ViewModel() {
    private val _messages = Channel<String>(Channel.BUFFERED)

    val messages: Flow<String> = _messages.receiveAsFlow()

    val uiState: StateFlow<VehicleUiState> = source.session
        .flatMapLatest { s ->
            val link = combine(
                s.connection.connectionState,
                s.connection.registered,
                s.connection.connectionDetail,
                s.connection.domainBootId,
                ::Link,
            )
            combine(
                s.repository.vehicleState,
                s.repository.requests,
                s.repository.warningHistory,
                link,
            ) { snapshot, requests, history, l ->
                VehicleUiState(
                    mode = s.mode,
                    hasDemoControls = s.demo != null,
                    connection = l.connection,
                    registered = l.registered,
                    connectionDetail = l.detail,
                    domainBootId = l.domainBootId,
                    snapshot = snapshot,
                    requests = requests,
                    warningHistory = history,
                )
            }
        }
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(STOP_TIMEOUT_MS), initialState(source.session.value))

    private fun initialState(s: VehicleSession) = VehicleUiState(mode = s.mode, hasDemoControls = s.demo != null)

    private data class Link(val connection: ConnectionState, val registered: Boolean, val detail: String?, val domainBootId: Long?)

    val nowMs: StateFlow<Long> = flow {
        while (true) {
            emit(clock())
            delay(TICK_MS)
        }
    }.stateIn(viewModelScope, SharingStarted.WhileSubscribed(STOP_TIMEOUT_MS), clock())

    init {
        viewModelScope.launch {
            source.session.collectLatest { s -> s.repository.notices.collect { _messages.send(it) } }
        }
    }

    fun switchMode(mode: AppMode) = source.switchMode(mode)

    fun connect() = act { it.connection.connect() }
    fun disconnect() = act { it.connection.disconnect() }
    fun unregister() = act { it.connection.unregister() }

    fun send(request: UserRequest) = act { it.repository.send(request) }

    fun resend(request: TrackedRequest) = act { it.repository.resend(request) }

    fun acknowledge(warning: Warning) = act { it.repository.acknowledgeWarning(warning) }

    fun confirmArchived(record: WarningRecord) = act { it.repository.confirmArchivedWarnings(listOf(record)) }

    fun acknowledgeAll(targets: AckTargets) = act { s ->
        s.repository.confirmArchivedWarnings(targets.archived)
        targets.current.forEach { s.repository.acknowledgeWarning(it) }
    }

    fun demo(block: suspend DemoControls.() -> Unit) = act { s ->
        val controls = s.demo ?: return@act
        controls.block()
    }

    private fun act(block: suspend (VehicleSession) -> Unit) {
        val s = source.session.value
        viewModelScope.launch {
            try {
                block(s)
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                _messages.send(ActionMessages.of(e))
            }
        }
    }

    companion object {
        private const val TICK_MS = 1_000L
        private const val STOP_TIMEOUT_MS = 5_000L

        fun factory(source: SessionSource): ViewModelProvider.Factory = viewModelFactory {
            initializer { VehicleViewModel(source) }
        }
    }
}
