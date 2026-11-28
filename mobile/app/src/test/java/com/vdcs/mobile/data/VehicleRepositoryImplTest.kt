package com.vdcs.mobile.data

import com.vdcs.mobile.data.ble.AttError
import com.vdcs.mobile.data.ble.BleTransport
import com.vdcs.mobile.data.ble.LinkState
import com.vdcs.mobile.data.ble.WriteResult
import com.vdcs.mobile.data.protocol.BleFrameCodec
import com.vdcs.mobile.data.protocol.ByteReader
import com.vdcs.mobile.data.protocol.ByteWriter
import com.vdcs.mobile.data.protocol.MessageCodec
import com.vdcs.mobile.data.protocol.MessageType
import com.vdcs.mobile.data.protocol.Reassembler
import com.vdcs.mobile.data.protocol.downstream
import com.vdcs.mobile.domain.logic.DisconnectCause
import com.vdcs.mobile.domain.logic.FreshnessEvaluator
import com.vdcs.mobile.domain.logic.RequestTracker
import com.vdcs.mobile.domain.logic.SessionContext
import com.vdcs.mobile.domain.model.ConnectionState
import com.vdcs.mobile.domain.model.Availability
import com.vdcs.mobile.domain.model.DoorIntegrity
import com.vdcs.mobile.domain.model.LockState
import com.vdcs.mobile.domain.model.VehicleFunction
import com.vdcs.mobile.domain.model.Quality
import com.vdcs.mobile.domain.model.ReadState
import com.vdcs.mobile.domain.model.RequestState
import com.vdcs.mobile.domain.model.UserRequest
import com.vdcs.mobile.domain.repository.InvalidRequest
import com.vdcs.mobile.domain.repository.PermissionMissing
import com.vdcs.mobile.domain.repository.QueryScope
import com.vdcs.mobile.domain.repository.RequestBlocked
import com.vdcs.mobile.domain.repository.RequestNotSent
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.advanceTimeBy
import kotlinx.coroutines.test.runCurrent
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test

@OptIn(ExperimentalCoroutinesApi::class)
class VehicleRepositoryImplTest {
    private sealed interface Event {
        val at: Long
        data class AppState(val active: Boolean, val appInstanceId: Long, override val at: Long) : Event
        data class GatewayRead(override val at: Long) : Event
        class Command(val type: Int, val payload: ByteArray, override val at: Long) : Event
    }

    private class Query(val id: Long, val scope: Int, val requestSession: Long, val requestId: Long)

    private class FakeTransport(private val clock: () -> Long) : BleTransport {
        val events = mutableListOf<Event>()
        override val link = MutableStateFlow<LinkState>(LinkState.Disconnected(null))
        val frames = MutableSharedFlow<ByteArray>(extraBufferCapacity = 256)
        val gateway = MutableSharedFlow<ByteArray>(extraBufferCapacity = 16)
        override val incomingFrames = frames
        override val gatewayStatus = gateway
        override var mtu = 185
        override var foreground = true
        val foregroundCalls = mutableListOf<Boolean>()
        override suspend fun setForeground(foreground: Boolean) {
            this.foreground = foreground
            foregroundCalls += foreground
        }
        var gatewayValue: ByteArray? = null
        var commandResult: (Int) -> WriteResult = { WriteResult.Ok }
        var appStateResult: WriteResult = WriteResult.Ok
        var connectError: Exception? = null
        var commandAttempts = 0
        private val upCodec = BleFrameCodec()
        private val upReassembler = Reassembler(1_000)

        override suspend fun connect() {
            connectError?.let { throw it }
        }
        override suspend fun disconnect() {}
        override suspend fun forget() {}

        override suspend fun readGatewayStatus(): ByteArray? {
            events += Event.GatewayRead(clock())
            return gatewayValue
        }

        override suspend fun writeAppCommand(frame: ByteArray): WriteResult {
            val f = upCodec.parseFrame(frame) ?: return WriteResult.Rejected(AttError.FRAGMENT_INVALID)
            commandAttempts++
            val r = commandResult(f.messageType)
            if (r != WriteResult.Ok) return r
            upReassembler.accept(f, clock())?.let { (type, payload) -> events += Event.Command(type, payload, clock()) }
            return WriteResult.Ok
        }

        override suspend fun writeAppState(payload: ByteArray): WriteResult {
            val r = ByteReader(payload)
            events += Event.AppState(r.u8(0) == 1, r.u32(1), clock())
            return appStateResult
        }

        fun commands(type: Int) = events.filterIsInstance<Event.Command>().filter { it.type == type }

        fun queries() = commands(MessageType.M_QUERY).map {
            val r = ByteReader(it.payload)
            Query(r.u32(0), r.u8(4), r.u64(6), r.u32(14))
        }

        fun requestIds() = commands(MessageType.M_REQUEST).map { ByteReader(it.payload).u32(0) }

        fun appStates() = events.filterIsInstance<Event.AppState>()
    }

    private class Rig(val test: TestScope, val historyStorage: MemoryWarningHistoryStorage = MemoryWarningHistoryStorage()) {
        val clock = { test.testScheduler.currentTime }
        val config = VdcsConfig()
        val transport = FakeTransport(clock)
        val repo = VehicleRepositoryImpl(
            transport = transport,
            frameCodec = BleFrameCodec(),
            reassembler = Reassembler(config.reassemblyTimeoutMs),
            messageCodec = MessageCodec(),
            requestTracker = RequestTracker(config.resultDeadlines(), config.recentRequestLimit),
            freshness = FreshnessEvaluator(),
            session = SessionContext { 0x0A0B0C0DL },
            warningHistoryStorage = historyStorage,
            config = config,
            scope = test.backgroundScope,
            clock = clock,
        )
        private val downCodec = BleFrameCodec()

        suspend fun emit(type: Int, payload: ByteArray, mtu: Int = 185) {
            downCodec.encode(type, payload, mtu).forEach { transport.frames.emit(it) }
            test.runCurrent()
        }

        suspend fun gatewayNotify(bytes: ByteArray) {
            transport.gateway.emit(bytes)
            test.runCurrent()
        }

        suspend fun respond(
            q: Query,
            items: List<Pair<Int, ByteArray>>,
            status: Int = 0,
            itemCount: Int? = null,
            session: Long = S1,
            fill: Boolean = true,
        ) {
            val fillers = if (fill) fillers(q, items.map { it.first }.toSet(), session) else emptyList()
            (items + fillers).forEach { (type, payload) -> emit(type, payload) }
            val count = (itemCount ?: items.size) + fillers.size
            emit(MessageType.M_QUERY_END, queryEnd(q.id, q.scope, status, count, session))
        }

        private fun fillers(q: Query, present: Set<Int>, session: Long): List<Pair<Int, ByteArray>> {
            val state = q.scope == SCOPE_CURRENT_STATE || q.scope == SCOPE_ALL
            val warnings = q.scope == SCOPE_WARNINGS || q.scope == SCOPE_ALL
            val out = ArrayList<Pair<Int, ByteArray>>()
            if (state) {
                QueryCoordinator.STATE_ITEMS.filter { it !in present }.forEach { type ->
                    out += type to downstream(type, sessionId = session, queryId = q.id)
                }
            }
            if (warnings && MessageType.M_WARNING !in present) {
                for (t in 1..8) out += MessageType.M_WARNING to downstream(MessageType.M_WARNING, sessionId = session, queryId = q.id) { u8(20, t) }
            }
            return out
        }

        suspend fun goReady() {
            transport.gatewayValue = gateway()
            transport.link.value = LinkState.Linked
            test.runCurrent()
            val sync = transport.queries().last()
            respond(sync, listOf(MessageType.M_BCM_DOOR_STATE to door(lock = 0, queryId = sync.id)))
            assertEquals(ConnectionState.AUTHENTICATED, repo.connectionState.value)
        }

        val connection get() = repo.connectionState.value
        val detail get() = repo.connectionDetail.value
        val requests get() = repo.requests.value
        val doorLock get() = repo.vehicleState.value.door?.lock
    }

    private companion object {
        const val S1 = 0x1122334455667788L
        const val S2 = 0x0102030405060708L
        const val SCOPE_CURRENT_STATE = 0
        const val SCOPE_REQUEST_RESULT = 1
        const val SCOPE_WARNINGS = 2
        const val SCOPE_ALL = 3

        fun gateway(
            registration: Int = 1,
            linkUp: Int = 1,
            ready: Int = 1,
            deviceContextId: Long = 7,
            session: Long = S1,
            domainBootId: Long = 3,
        ): ByteArray = ByteWriter(21)
            .u8(0, 1).u8(1, registration).u8(2, linkUp).u8(3, ready)
            .u32(4, deviceContextId).u64(8, session).u32(16, domainBootId).u8(20, 1)
            .toByteArray()

        fun door(lock: Int, queryId: Long = 0, session: Long = S1, deviceContextId: Long = 7, domainBootId: Long = 3) =
            downstream(MessageType.M_BCM_DOOR_STATE, deviceContextId, session, domainBootId, queryId) {
                u8(28, lock).u8(29, 0).u8(30, 0).u8(31, 0).u8(32, 0).u8(33, 0)
            }

        fun result(requestSession: Long, requestId: Long, result: Int, queryId: Long = 0, session: Long = S1) =
            downstream(MessageType.M_RESULT, sessionId = session, queryId = queryId) {
                u64(20, requestSession).u32(28, requestId).u8(32, result).u16(33, 0).u8(35, 1).u8(36, 1)
            }

        fun queryEnd(queryId: Long, scope: Int, status: Int, itemCount: Int, session: Long = S1) =
            downstream(MessageType.M_QUERY_END, sessionId = session, queryId = queryId) {
                u8(20, scope).u8(21, status).u16(22, itemCount)
            }
    }

    private suspend inline fun <reified T : Throwable> expectThrows(block: () -> Unit): T {
        try {
            block()
        } catch (e: Throwable) {
            if (e is T) return e
            throw e
        }
        fail("${T::class.simpleName} 이 나야 한다")
        throw AssertionError()
    }

    @Test fun `§46 순서 - Linked 뒤 App State write, Gateway READ, M_QUERY(ALL), QUERY_END 완결 후 READY`() = runTest {
        val rig = Rig(this)
        val t = rig.transport
        t.gatewayValue = gateway(ready = 0)
        runCurrent()
        assertEquals(ConnectionState.DISCONNECTED, rig.connection)

        t.link.value = LinkState.Linked
        runCurrent()
        val first3 = t.events.take(3)
        assertTrue(first3[0] is Event.AppState && (first3[0] as Event.AppState).active)
        assertTrue(first3[1] is Event.GatewayRead)
        assertTrue(first3[2] is Event.Command && (first3[2] as Event.Command).type == MessageType.M_QUERY)
        val sync = t.queries().single()
        assertEquals(SCOPE_ALL, sync.scope)
        assertEquals(0L, sync.requestSession)
        assertEquals(ConnectionState.CONNECTED, rig.connection)

        rig.emit(MessageType.M_BCM_DOOR_STATE, door(lock = 0, queryId = sync.id))
        assertEquals(ConnectionState.CONNECTED, rig.connection)
        rig.respond(sync, emptyList(), itemCount = 1)
        assertEquals(ConnectionState.CONNECTED, rig.connection)
        rig.gatewayNotify(gateway(ready = 1))
        assertEquals(ConnectionState.AUTHENTICATED, rig.connection)
        assertNull(rig.detail)
        assertEquals(LockState.LOCKED, rig.doorLock!!.value)
        assertEquals(1, t.queries().size)
    }

    @Test fun `DOMAIN_BOOT_ID 미확인(0)이면 Query 도 READY 도 없다 - 확인되면 진행`() = runTest {
        val rig = Rig(this)
        val t = rig.transport
        t.gatewayValue = gateway(domainBootId = 0)
        t.link.value = LinkState.Linked
        runCurrent()
        advanceTimeBy(1_000); runCurrent()
        assertTrue(t.queries().isEmpty())
        assertEquals(ConnectionState.CONNECTED, rig.connection)
        expectThrows<RequestBlocked> { rig.repo.send(UserRequest.Door(lock = false)) }

        rig.gatewayNotify(gateway(domainBootId = 3))
        val sync = t.queries().single()
        rig.respond(sync, emptyList())
        assertEquals(ConnectionState.AUTHENTICATED, rig.connection)
    }

    @Test fun `READY 전 send 는 요청을 만들지 않는다`() = runTest {
        val rig = Rig(this)
        runCurrent()
        expectThrows<RequestBlocked> { rig.repo.send(UserRequest.Door(lock = false)) }
        assertTrue(rig.requests.isEmpty())
        assertTrue(rig.transport.requestIds().isEmpty())
    }

    @Test fun `foreground 동안 ACTIVE 1초 heartbeat, background 진입 시 INACTIVE 후 중단`() = runTest {
        val rig = Rig(this)
        val t = rig.transport
        runCurrent()
        t.link.value = LinkState.Linked
        runCurrent()
        advanceTimeBy(4_000); runCurrent()
        assertEquals(listOf(0L, 1_000L, 2_000L, 3_000L, 4_000L), t.appStates().filter { it.active }.map { it.at })
        assertTrue(t.appStates().all { it.appInstanceId != 0L })

        rig.repo.setForeground(false)
        runCurrent()
        assertFalse(t.appStates().last().active)
        val activeBefore = t.appStates().count { it.active }
        advanceTimeBy(3_000); runCurrent()
        assertEquals(activeBefore, t.appStates().count { it.active })

        rig.repo.setForeground(true)
        advanceTimeBy(200); runCurrent()
        val resumed = t.appStates().last()
        assertTrue(resumed.active)
        advanceTimeBy(1_000); runCurrent()
        val again = t.appStates().filter { it.active }.takeLast(2)
        assertEquals(1_000L, again[1].at - again[0].at)
    }

    @Test fun `링크가 없으면 heartbeat 를 쓰지 않는다`() = runTest {
        val rig = Rig(this)
        runCurrent()
        advanceTimeBy(3_000); runCurrent()
        assertTrue(rig.transport.appStates().isEmpty())
    }

    @Test fun `§17 DEVICE_CONTEXT_ID·SESSION_ID·DOMAIN_BOOT_ID 중 하나라도 다르면 적용하지 않는다`() = runTest {
        val rig = Rig(this)
        rig.goReady()
        assertEquals(LockState.LOCKED, rig.doorLock!!.value)

        rig.emit(MessageType.M_BCM_DOOR_STATE, door(lock = 1, session = S2))
        rig.emit(MessageType.M_BCM_DOOR_STATE, door(lock = 1, deviceContextId = 8))
        rig.emit(MessageType.M_BCM_DOOR_STATE, door(lock = 1, domainBootId = 4))
        assertEquals(LockState.LOCKED, rig.doorLock!!.value)

        rig.emit(MessageType.M_BCM_DOOR_STATE, door(lock = 1))
        assertEquals(LockState.UNLOCKED, rig.doorLock!!.value)
    }

    @Test fun `§17 다른 문맥의 M_RESULT 는 요청에 반영하지 않는다`() = runTest {
        val rig = Rig(this)
        rig.goReady()
        val id = rig.repo.send(UserRequest.Door(lock = false))
        rig.emit(MessageType.M_RESULT, result(S1, id, result = 2, session = S2))
        assertEquals(RequestState.SENT, rig.requests.single().state)
        rig.emit(MessageType.M_RESULT, result(S1, id, result = 2))
        assertEquals(RequestState.DONE, rig.requests.single().state)
    }

    @Test fun `현재 문맥이 미확인(0)이면 어떤 하행도 적용하지 않는다`() = runTest {
        val rig = Rig(this)
        rig.transport.link.value = LinkState.Linked
        runCurrent()
        rig.emit(MessageType.M_BCM_DOOR_STATE, door(lock = 1))
        rig.emit(MessageType.M_BCM_DOOR_STATE, door(lock = 1, session = 0, deviceContextId = 0, domainBootId = 0))
        assertNull(rig.repo.vehicleState.value.door)
    }

    @Test fun `SESSION_ID 변경 - 진행 요청 UNKNOWN, 재조립·Query 폐기, 이전 값 STALE, 전체 재Query`() = runTest {
        val rig = Rig(this)
        val t = rig.transport
        rig.goReady()
        val id = rig.repo.send(UserRequest.Door(lock = false))
        rig.repo.query(QueryScope.CURRENT_STATE)
        val userQuery = t.queries().last()
        assertEquals(0, userQuery.scope)
        advanceTimeBy(100); runCurrent()
        assertEquals(Quality.OK, rig.doorLock!!.quality)

        val pieces = BleFrameCodec().encode(MessageType.M_BCM_DOOR_STATE, door(lock = 1, session = S2), 23)
        assertEquals(4, pieces.size)
        t.frames.emit(pieces[0]); runCurrent()
        rig.gatewayNotify(gateway(session = S2, ready = 0))

        assertEquals(RequestState.UNKNOWN, rig.requests.single { it.requestId == id }.state)
        assertEquals(Quality.STALE, rig.doorLock!!.quality)
        assertEquals(ConnectionState.CONNECTED, rig.connection)
        val resync = t.queries().last()
        assertEquals(SCOPE_ALL, resync.scope)
        assertNotEquals(userQuery.id, resync.id)

        pieces.drop(1).forEach { t.frames.emit(it) }
        runCurrent()
        assertEquals(LockState.LOCKED, rig.doorLock!!.value)

        rig.emit(MessageType.M_QUERY_END, queryEnd(userQuery.id, 0, 0, 0, session = S2))
        assertEquals(ConnectionState.CONNECTED, rig.connection)

        rig.respond(resync, listOf(MessageType.M_BCM_DOOR_STATE to door(lock = 1, queryId = resync.id, session = S2)), session = S2)
        rig.gatewayNotify(gateway(session = S2, ready = 1))
        assertEquals(ConnectionState.AUTHENTICATED, rig.connection)
        assertEquals(LockState.UNLOCKED, rig.doorLock!!.value)
        assertEquals(Quality.OK, rig.doorLock!!.quality)
        assertEquals(RequestState.UNKNOWN, rig.requests.single { it.sessionId == S1 }.state)
    }

    @Test fun `DOMAIN_BOOT_ID 변경도 같은 연쇄 - 재Query 하고 READY 해제`() = runTest {
        val rig = Rig(this)
        rig.goReady()
        val before = rig.transport.queries().size
        rig.gatewayNotify(gateway(domainBootId = 9))
        assertEquals(ConnectionState.CONNECTED, rig.connection)
        assertEquals(before + 1, rig.transport.queries().size)
        assertEquals(SCOPE_ALL, rig.transport.queries().last().scope)
    }

    @Test fun `DOMAIN_BOOT_ID 만 바뀐 Gateway Status 뒤에도 같은 세션 REQUEST_ID 가 이어진다`() = runTest {
        val rig = Rig(this)
        val t = rig.transport
        rig.goReady()
        val first = rig.repo.send(UserRequest.Door(lock = false))

        rig.gatewayNotify(gateway(domainBootId = 9))
        assertEquals(RequestState.UNKNOWN, rig.requests.single().state)
        val resync = t.queries().last()
        assertEquals(SCOPE_ALL, resync.scope)
        rig.emit(MessageType.M_BCM_DOOR_STATE, door(lock = 1, queryId = resync.id, domainBootId = 9))
        val fillers = (QueryCoordinator.STATE_ITEMS - MessageType.M_BCM_DOOR_STATE).map {
            it to downstream(it, domainBootId = 9, queryId = resync.id)
        } + (1..8).map { w -> MessageType.M_WARNING to downstream(MessageType.M_WARNING, domainBootId = 9, queryId = resync.id) { u8(20, w) } }
        fillers.forEach { (type, payload) -> rig.emit(type, payload) }
        rig.emit(
            MessageType.M_QUERY_END,
            downstream(MessageType.M_QUERY_END, domainBootId = 9, queryId = resync.id) {
                u8(20, resync.scope).u8(21, 0).u16(22, 1 + fillers.size)
            },
        )
        assertEquals(ConnectionState.AUTHENTICATED, rig.connection)

        val second = rig.repo.send(UserRequest.Door(lock = true))
        assertEquals(first + 1, second)
        assertEquals(listOf(first, second), t.requestIds())
        assertEquals(2, rig.requests.size)

        rig.emit(
            MessageType.M_RESULT,
            downstream(MessageType.M_RESULT, domainBootId = 9) {
                u64(20, S1).u32(28, first).u8(32, 2).u16(33, 0).u8(35, 1).u8(36, 1)
            },
        )
        assertEquals(RequestState.DONE, rig.requests.single { it.requestId == first }.state)
        assertEquals(RequestState.SENT, rig.requests.single { it.requestId == second }.state)
    }

    @Test fun `다른 QUERY_ID·unsolicited 응답은 개수에 들지 않는다 - ITEM_COUNT 불일치면 미완결`() = runTest {
        val rig = Rig(this)
        val t = rig.transport
        t.gatewayValue = gateway()
        t.link.value = LinkState.Linked
        runCurrent()
        val sync = t.queries().single()
        rig.emit(MessageType.M_BCM_DOOR_STATE, door(lock = 0, queryId = sync.id + 100))
        rig.emit(MessageType.M_BCM_DOOR_STATE, door(lock = 0, queryId = 0))
        rig.emit(MessageType.M_QUERY_END, queryEnd(sync.id, SCOPE_ALL, 0, 2))
        assertEquals(ConnectionState.CONNECTED, rig.connection)
        assertEquals("초기 상태 동기화 미완료 — 다시 시도 중", rig.detail)
    }

    @Test fun `수신 수가 ITEM_COUNT 보다 적으면 미완결 - syncRetry 뒤 재Query`() = runTest {
        val rig = Rig(this)
        val t = rig.transport
        t.gatewayValue = gateway()
        t.link.value = LinkState.Linked
        runCurrent()
        val sync = t.queries().single()
        rig.respond(sync, listOf(MessageType.M_BCM_DOOR_STATE to door(lock = 0, queryId = sync.id)), itemCount = 2)
        assertEquals(ConnectionState.CONNECTED, rig.connection)

        advanceTimeBy(rig.config.syncRetryMs + rig.config.tickMs); runCurrent()
        val retry = t.queries().last()
        assertEquals(2, t.queries().size)
        assertEquals(SCOPE_ALL, retry.scope)
        assertNotEquals(sync.id, retry.id)
        rig.respond(retry, listOf(MessageType.M_BCM_DOOR_STATE to door(lock = 0, queryId = retry.id)))
        assertEquals(ConnectionState.AUTHENTICATED, rig.connection)
    }

    @Test fun `QUERY_STATUS 3(문맥 거부)은 개수가 맞아도 미완결, 필수 항목이 빠져도 미완결, 2 는 완결`() = runTest {
        val rig = Rig(this)
        val t = rig.transport
        t.gatewayValue = gateway()
        t.link.value = LinkState.Linked
        runCurrent()
        for ((status, fill) in listOf(3 to true, 0 to false)) {
            val q = t.queries().last()
            rig.respond(q, listOf(MessageType.M_BCM_DOOR_STATE to door(lock = 0, queryId = q.id)), status = status, fill = fill)
            assertEquals("STATUS $status fill $fill", ConnectionState.CONNECTED, rig.connection)
            advanceTimeBy(rig.config.syncRetryMs + rig.config.tickMs); runCurrent()
        }
        assertEquals(3, t.queries().size)
        val q = t.queries().last()
        rig.respond(q, listOf(MessageType.M_BCM_DOOR_STATE to door(lock = 0, queryId = q.id)), status = 2)
        assertEquals(ConnectionState.AUTHENTICATED, rig.connection)
    }

    @Test fun `QUERY_END 의 문맥이 현재와 다르면 미완결`() = runTest {
        val rig = Rig(this)
        val t = rig.transport
        t.gatewayValue = gateway()
        t.link.value = LinkState.Linked
        runCurrent()
        val sync = t.queries().single()
        rig.respond(sync, emptyList(), session = S2)
        assertEquals(ConnectionState.CONNECTED, rig.connection)
    }

    @Test fun `구현 보류 WINDOW(Raw) 응답도 ITEM_COUNT 에 센다`() = runTest {
        val rig = Rig(this)
        val t = rig.transport
        t.gatewayValue = gateway()
        t.link.value = LinkState.Linked
        runCurrent()
        val sync = t.queries().single()
        rig.respond(
            sync,
            listOf(
                MessageType.M_BCM_DOOR_STATE to door(lock = 0, queryId = sync.id),
                MessageType.M_WINDOW_STATE to downstream(MessageType.M_WINDOW_STATE, queryId = sync.id),
                MessageType.M_WINDOW_FAULT to downstream(MessageType.M_WINDOW_FAULT, queryId = sync.id),
            ),
        )
        assertEquals(ConnectionState.AUTHENTICATED, rig.connection)
    }

    @Test fun `QUERY_END 가 대기 한도(3초)를 넘기면 미완결 - 늦은 끝은 무시, 재Query`() = runTest {
        val rig = Rig(this)
        val t = rig.transport
        t.gatewayValue = gateway()
        t.link.value = LinkState.Linked
        runCurrent()
        val sync = t.queries().single()
        advanceTimeBy(2_900); runCurrent()
        assertEquals("초기 상태 동기화 중", rig.detail)
        advanceTimeBy(500); runCurrent()
        assertEquals("초기 상태 동기화 미완료 — 다시 시도 중", rig.detail)
        assertEquals(1, t.queries().size)

        rig.emit(MessageType.M_QUERY_END, queryEnd(sync.id, SCOPE_ALL, 0, 0))
        assertEquals(ConnectionState.CONNECTED, rig.connection)

        advanceTimeBy(rig.config.syncRetryMs); runCurrent()
        assertEquals(2, t.queries().size)
        val retry = t.queries().last()
        rig.respond(retry, emptyList())
        assertEquals(ConnectionState.AUTHENTICATED, rig.connection)
    }

    @Test fun `ATT 0x85·0x87 거절 - 요청을 추적하지 않고 사유를 알린다`() = runTest {
        for (code in listOf(AttError.SESSION_NOT_READY, AttError.QUERY_BUSY)) {
            val rig = Rig(this)
            rig.goReady()
            rig.transport.commandResult = { if (it == MessageType.M_REQUEST) WriteResult.Rejected(code) else WriteResult.Ok }
            val e = expectThrows<RequestNotSent> { rig.repo.send(UserRequest.Door(lock = false)) }
            assertEquals(AttError.describe(code), e.message)
            assertTrue(rig.requests.isEmpty())
            assertEquals(ConnectionState.AUTHENTICATED, rig.connection)
            rig.transport.commandResult = { WriteResult.Ok }
            rig.repo.send(UserRequest.Door(lock = false))
            assertEquals(1, rig.requests.size)
        }
    }

    @Test fun `ATT 0x86 거절은 AUTH_FAILED - 이후 요청 금지, Gateway 알림이 덮지 않는다`() = runTest {
        val rig = Rig(this)
        rig.goReady()
        rig.transport.commandResult = { WriteResult.Rejected(AttError.NOT_REGISTERED) }
        val e = expectThrows<RequestNotSent> { rig.repo.send(UserRequest.Door(lock = false)) }
        assertEquals("등록되지 않은 단말", e.message)
        assertTrue(rig.requests.isEmpty())
        assertEquals(ConnectionState.CONNECTED, rig.connection)
        assertEquals("등록되지 않은 단말입니다", rig.detail)

        rig.transport.commandResult = { WriteResult.Ok }
        rig.gatewayNotify(gateway())
        assertEquals(ConnectionState.CONNECTED, rig.connection)
        expectThrows<RequestBlocked> { rig.repo.send(UserRequest.Door(lock = false)) }
        assertTrue(rig.transport.requestIds().isEmpty())
    }

    @Test fun `Gateway REGISTRATION_STATE NOT_REGISTERED 면 READY 가 되지 않는다`() = runTest {
        val rig = Rig(this)
        rig.transport.gatewayValue = gateway(registration = 0)
        rig.transport.link.value = LinkState.Linked
        runCurrent()
        assertTrue(rig.transport.queries().isEmpty())
        assertEquals("등록되지 않은 단말입니다", rig.detail)
        assertFalse(rig.repo.registered.value)
    }

    @Test fun `끊김 - 재조립 폐기, 재연결 뒤 요청 자동 재전송 없음`() = runTest {
        val rig = Rig(this)
        val t = rig.transport
        rig.goReady()
        val id = rig.repo.send(UserRequest.Door(lock = false))
        assertEquals(listOf(id), t.requestIds())

        val pieces = BleFrameCodec().encode(MessageType.M_BCM_DOOR_STATE, door(lock = 1), 23)
        t.frames.emit(pieces[0]); runCurrent()
        t.link.value = LinkState.Disconnected(DisconnectCause.LINK_LOST, "신호 약함")
        runCurrent()
        assertEquals(ConnectionState.DISCONNECTED, rig.connection)
        assertEquals("신호 약함", rig.detail)
        assertEquals(RequestState.UNKNOWN, rig.requests.single().state)
        expectThrows<RequestBlocked> { rig.repo.send(UserRequest.Door(lock = true)) }

        t.link.value = LinkState.Linked
        runCurrent()
        pieces.drop(1).forEach { t.frames.emit(it) }
        runCurrent()
        assertEquals(LockState.LOCKED, rig.doorLock!!.value)
        val sync = t.queries().last()
        assertEquals(SCOPE_ALL, sync.scope)
        rig.respond(sync, emptyList())
        assertEquals(ConnectionState.AUTHENTICATED, rig.connection)

        advanceTimeBy(1_500); runCurrent()
        assertEquals(listOf(id), t.requestIds())
        assertEquals(RequestState.UNKNOWN, rig.requests.single().state)
        assertEquals(SCOPE_REQUEST_RESULT, t.queries().last().scope)
    }

    @Test fun `첫 응답 기한(3초) 무결과면 UNKNOWN 후 M_QUERY(scope 1, REQUEST_SESSION+ID) - 진행 중 Query 가 끝날 때까지 대기`() = runTest {
        val rig = Rig(this)
        val t = rig.transport
        rig.goReady()
        val id = rig.repo.send(UserRequest.Door(lock = false))
        advanceTimeBy(rig.config.resultObserveMs - 1_000); runCurrent()
        rig.repo.query(QueryScope.CURRENT_STATE)
        val userQuery = t.queries().last()
        val queriesBefore = t.queries().size

        advanceTimeBy(900); runCurrent()
        assertEquals(RequestState.SENT, rig.requests.single().state)
        advanceTimeBy(300); runCurrent()
        assertEquals(RequestState.UNKNOWN, rig.requests.single().state)
        assertEquals(queriesBefore, t.queries().size)

        rig.respond(userQuery, emptyList())
        val follow = t.queries().last()
        assertEquals(queriesBefore + 1, t.queries().size)
        assertEquals(SCOPE_REQUEST_RESULT, follow.scope)
        assertEquals(S1, follow.requestSession)
        assertEquals(id, follow.requestId)

        rig.respond(follow, listOf(MessageType.M_RESULT to result(S1, id, result = 2, queryId = follow.id)))
        assertEquals(RequestState.DONE, rig.requests.single().state)
        assertEquals(listOf(id), t.requestIds())
    }

    @Test fun `Result ACCEPTED 가 첫 응답 기한 안에 오면 UNKNOWN 이 되지 않는다`() = runTest {
        val rig = Rig(this)
        rig.goReady()
        val id = rig.repo.send(UserRequest.Door(lock = false))
        advanceTimeBy(300); runCurrent()
        rig.emit(MessageType.M_RESULT, result(S1, id, result = 0))
        advanceTimeBy(2_000); runCurrent()
        assertEquals(RequestState.ACCEPTED, rig.requests.single().state)
        assertTrue(rig.transport.queries().none { it.scope == SCOPE_REQUEST_RESULT })
    }

    @Test fun `D10 UNKNOWN 재요청 - 같은 세션이면 같은 REQUEST_ID, 세션이 바뀌면 새 번호공간`() = runTest {
        val rig = Rig(this)
        val t = rig.transport
        rig.goReady()
        val id = rig.repo.send(UserRequest.Door(lock = false))
        advanceTimeBy(rig.config.resultObserveMs + 200); runCurrent()
        rig.respond(t.queries().last(), emptyList(), status = 1)
        val unknown = rig.requests.single()
        assertEquals(RequestState.UNKNOWN, unknown.state)

        rig.repo.resend(unknown)
        assertEquals(listOf(id, id), t.requestIds())
        assertEquals(RequestState.SENT, rig.requests.single().state)

        advanceTimeBy(rig.config.resultObserveMs + 200); runCurrent()
        rig.respond(t.queries().last(), emptyList(), status = 1)
        val stillUnknown = rig.requests.single()
        assertEquals(RequestState.UNKNOWN, stillUnknown.state)

        rig.gatewayNotify(gateway(session = S2))
        rig.respond(t.queries().last(), emptyList(), session = S2)
        assertEquals(ConnectionState.AUTHENTICATED, rig.connection)
        rig.repo.resend(stillUnknown)
        assertEquals(3, t.requestIds().size)
        val fresh = rig.requests.single { it.sessionId == S2 }
        assertEquals(RequestState.SENT, fresh.state)
        assertEquals(UserRequest.Door(lock = false), fresh.request)
        assertEquals(RequestState.UNKNOWN, rig.requests.single { it.sessionId == S1 }.state)
    }

    @Test fun `같은 ID 재요청의 쓰기가 실패하면 기록을 지우지 않고 UNKNOWN 유지`() = runTest {
        val rig = Rig(this)
        rig.goReady()
        rig.repo.send(UserRequest.Door(lock = false))
        advanceTimeBy(rig.config.resultObserveMs + 200); runCurrent()
        rig.respond(rig.transport.queries().last(), emptyList(), status = 1)
        val unknown = rig.requests.single()

        rig.transport.commandResult = { WriteResult.Rejected(AttError.SESSION_NOT_READY) }
        expectThrows<RequestNotSent> { rig.repo.resend(unknown) }
        val kept = rig.requests.single()
        assertEquals(unknown.requestId, kept.requestId)
        assertEquals(RequestState.UNKNOWN, kept.state)
    }

    @Test fun `UNKNOWN 이 아닌 요청은 재요청할 수 없다`() = runTest {
        val rig = Rig(this)
        rig.goReady()
        rig.repo.send(UserRequest.Door(lock = false))
        expectThrows<RequestBlocked> { rig.repo.resend(rig.requests.single()) }
        assertEquals(1, rig.transport.requestIds().size)
    }

    @Test fun `SYNC 전체 Query 가 STATUS 1(요청 없음)이어도 개수가 맞으면 READY`() = runTest {
        val rig = Rig(this)
        val t = rig.transport
        t.gatewayValue = gateway()
        t.link.value = LinkState.Linked
        runCurrent()
        val sync = t.queries().single()
        rig.respond(sync, listOf(MessageType.M_BCM_DOOR_STATE to door(lock = 0, queryId = sync.id)), status = 1)
        assertEquals(ConnectionState.AUTHENTICATED, rig.connection)
        assertEquals(1, t.queries().size)
    }

    @Test fun `가용성·ECU 상태는 3초 수신 중단이면 STALE - 값은 남는다`() = runTest {
        val rig = Rig(this)
        rig.goReady()
        rig.emit(MessageType.M_AVAILABILITY, availabilityPayload())
        rig.emit(MessageType.M_BCM_STATUS, downstream(MessageType.M_BCM_STATUS) { u8(28, 1) })
        val fresh = rig.repo.vehicleState.value
        assertEquals(Quality.OK, fresh.availability.getValue(VehicleFunction.DOOR).quality)
        assertEquals(Quality.OK, fresh.ecus.single { it.name == "BCM" }.quality)

        advanceTimeBy(2_800); runCurrent()
        assertEquals(Quality.OK, rig.repo.vehicleState.value.availability.getValue(VehicleFunction.DOOR).quality)
        advanceTimeBy(400); runCurrent()
        val old = rig.repo.vehicleState.value
        val door = old.availability.getValue(VehicleFunction.DOOR)
        assertEquals(Quality.STALE, door.quality)
        assertEquals(Availability.AVAILABLE, door.availability)
        assertEquals(Quality.STALE, old.ecus.single { it.name == "BCM" }.quality)

        rig.emit(MessageType.M_AVAILABILITY, availabilityPayload())
        assertEquals(Quality.OK, rig.repo.vehicleState.value.availability.getValue(VehicleFunction.DOOR).quality)
    }

    @Test fun `문맥이 바뀌면 이전 가용성·ECU 상태는 바로 STALE (D8)`() = runTest {
        val rig = Rig(this)
        rig.goReady()
        rig.emit(MessageType.M_AVAILABILITY, availabilityPayload())
        rig.emit(MessageType.M_BCM_STATUS, downstream(MessageType.M_BCM_STATUS) { u8(28, 1) })
        advanceTimeBy(100); runCurrent()
        rig.gatewayNotify(gateway(session = S2, ready = 0))
        val s = rig.repo.vehicleState.value
        assertTrue(s.availability.values.all { it.quality == Quality.STALE })
        assertEquals(Quality.STALE, s.ecus.single { it.name == "BCM" }.quality)
    }

    @Test fun `DIGITAL_KEY_SETTING_STATE 가 0·1 밖이면 값 없음·NO_DATA (자체 추정 금지)`() = runTest {
        val rig = Rig(this)
        rig.goReady()
        rig.emit(MessageType.M_DIGITAL_STATUS, downstream(MessageType.M_DIGITAL_STATUS) { u8(20, 7) })
        val unknown = rig.repo.vehicleState.value.digitalKey!!.proximityUnlockEnabled
        assertNull(unknown.value)
        assertEquals(Quality.NO_DATA, unknown.quality)
        rig.emit(MessageType.M_DIGITAL_STATUS, downstream(MessageType.M_DIGITAL_STATUS) { u8(20, 1) })
        val on = rig.repo.vehicleState.value.digitalKey!!.proximityUnlockEnabled
        assertEquals(true, on.value)
        assertEquals(Quality.OK, on.quality)
    }

    @Test fun `App State write 실패면 동기화·요청을 막고 heartbeat 간격으로 재시도`() = runTest {
        val rig = Rig(this)
        val t = rig.transport
        t.appStateResult = WriteResult.Rejected(AttError.PAYLOAD_LENGTH_INVALID)
        t.gatewayValue = gateway()
        t.link.value = LinkState.Linked
        runCurrent()
        assertTrue(t.queries().isEmpty())
        assertEquals(ConnectionState.CONNECTED, rig.connection)
        assertEquals("앱 상태 전송 실패 — 다시 시도 중", rig.detail)

        val writes = t.appStates().size
        advanceTimeBy(rig.config.tickMs); runCurrent()
        assertEquals(writes, t.appStates().size)
        advanceTimeBy(rig.config.heartbeatMs - rig.config.tickMs); runCurrent()
        assertEquals(writes + 1, t.appStates().size)
        assertTrue(t.queries().isEmpty())

        t.appStateResult = WriteResult.Ok
        advanceTimeBy(rig.config.heartbeatMs); runCurrent()
        val sync = t.queries().single()
        assertEquals(SCOPE_ALL, sync.scope)
        rig.respond(sync, emptyList())
        assertEquals(ConnectionState.AUTHENTICATED, rig.connection)
    }

    @Test fun `READY 뒤 App State write 가 실패하면 요청을 막는다`() = runTest {
        val rig = Rig(this)
        rig.goReady()
        rig.transport.appStateResult = WriteResult.NotConnected
        advanceTimeBy(1_000); runCurrent()
        assertEquals(ConnectionState.CONNECTED, rig.connection)
        expectThrows<RequestBlocked> { rig.repo.send(UserRequest.Door(lock = false)) }
        assertTrue(rig.transport.requestIds().isEmpty())

        rig.transport.appStateResult = WriteResult.Ok
        advanceTimeBy(rig.config.heartbeatMs); runCurrent()
        assertEquals(ConnectionState.AUTHENTICATED, rig.connection)
    }

    private fun availabilityPayload() = downstream(MessageType.M_AVAILABILITY) {
        for (i in 0 until 9) u8(20 + i * 4, i + 1).u8(21 + i * 4, 0).u16(22 + i * 4, 0)
    }

    @Test fun `App State 실패가 계속되면 재시도는 1초에 1회`() = runTest {
        val rig = Rig(this)
        val t = rig.transport
        t.appStateResult = WriteResult.NotConnected
        t.link.value = LinkState.Linked
        runCurrent()
        advanceTimeBy(5_000); runCurrent()
        val times = t.appStates().map { it.at }
        assertEquals(listOf(0L, 1_000L, 2_000L, 3_000L, 4_000L, 5_000L), times)
    }

    @Test fun `도어 무결성·후방 상태 품질 - 600 ms 수신 중단이면 STALE, 문맥 변경이면 바로 STALE`() = runTest {
        val rig = Rig(this)
        rig.goReady()
        rig.emit(MessageType.M_CIS_REAR, rearPayload(valid = 1))
        assertEquals(Quality.OK, rig.repo.vehicleState.value.door!!.integrity.quality)
        assertEquals(Quality.OK, rig.repo.vehicleState.value.rear!!.status.quality)

        advanceTimeBy(800); runCurrent()
        val old = rig.repo.vehicleState.value
        assertEquals(Quality.STALE, old.door!!.integrity.quality)
        assertEquals(DoorIntegrity.NORMAL, old.door!!.integrity.value)
        assertEquals(Quality.STALE, old.rear!!.status.quality)

        rig.emit(MessageType.M_BCM_DOOR_STATE, door(lock = 0))
        rig.emit(MessageType.M_CIS_REAR, rearPayload(valid = 1))
        assertEquals(Quality.OK, rig.repo.vehicleState.value.door!!.integrity.quality)
        assertEquals(Quality.OK, rig.repo.vehicleState.value.rear!!.status.quality)

        advanceTimeBy(100); runCurrent()
        rig.gatewayNotify(gateway(session = S2, ready = 0))
        assertEquals(Quality.STALE, rig.repo.vehicleState.value.door!!.integrity.quality)
        assertEquals(Quality.STALE, rig.repo.vehicleState.value.rear!!.status.quality)
    }

    @Test fun `무결성 품질은 COMPOSITE_QUALITY, 후방 상태 품질은 VALIDITY 를 따른다`() = runTest {
        val rig = Rig(this)
        rig.goReady()
        rig.emit(
            MessageType.M_BCM_DOOR_STATE,
            downstream(MessageType.M_BCM_DOOR_STATE) { u8(28, 0).u8(29, 0).u8(30, 0).u8(31, 0).u8(32, 0).u8(33, 2) },
        )
        rig.emit(MessageType.M_CIS_REAR, rearPayload(valid = 0))
        assertEquals(Quality.INVALID, rig.repo.vehicleState.value.door!!.integrity.quality)
        assertEquals(Quality.INVALID, rig.repo.vehicleState.value.rear!!.status.quality)
    }

    @Test fun `와이어 범위 밖 요청 값은 InvalidRequest - 추적·송신하지 않는다 (B-13)`() = runTest {
        val rig = Rig(this)
        rig.goReady()
        expectThrows<InvalidRequest> { rig.repo.send(UserRequest.TargetTemperature(400.0)) }
        assertTrue(rig.requests.isEmpty())
        assertTrue(rig.transport.requestIds().isEmpty())
        assertEquals(ConnectionState.AUTHENTICATED, rig.connection)
    }

    @Test fun `권한 SecurityException 은 PermissionMissing 으로 바뀐다 (B-13)`() = runTest {
        val rig = Rig(this)
        rig.transport.connectError = SecurityException("BLUETOOTH_CONNECT")
        runCurrent()
        expectThrows<PermissionMissing> { rig.repo.connect() }
    }

    @Test fun `도어 무결성 - 값은 보고 그대로, COMPOSITE_QUALITY 는 품질로만 (A-4)`() = runTest {
        val rig = Rig(this)
        rig.goReady()
        rig.emit(
            MessageType.M_BCM_DOOR_STATE,
            downstream(MessageType.M_BCM_DOOR_STATE) { u8(28, 0).u8(29, 0).u8(30, 0).u8(31, 0).u8(32, 0).u8(33, 1) },
        )
        val stale = rig.repo.vehicleState.value.door!!.integrity
        assertEquals(DoorIntegrity.NORMAL, stale.value)
        assertEquals(Quality.STALE, stale.quality)
        rig.emit(
            MessageType.M_BCM_DOOR_STATE,
            downstream(MessageType.M_BCM_DOOR_STATE) { u8(28, 0).u8(29, 1).u8(30, 1).u8(31, 0).u8(32, 0).u8(33, 0) },
        )
        val inconsistent = rig.repo.vehicleState.value.door!!.integrity
        assertEquals(DoorIntegrity.INCONSISTENT, inconsistent.value)
        assertEquals(Quality.OK, inconsistent.quality)
    }

    @Test fun `초기 Query 가 ATT 0x86 으로 거절되면 AUTH_FAILED - 동기화를 되풀이하지 않는다`() = runTest {
        val rig = Rig(this)
        val t = rig.transport
        t.commandResult = { WriteResult.Rejected(AttError.NOT_REGISTERED) }
        t.gatewayValue = gateway()
        t.link.value = LinkState.Linked
        runCurrent()
        assertEquals(1, t.commandAttempts)
        assertEquals(ConnectionState.CONNECTED, rig.connection)
        assertEquals("등록되지 않은 단말입니다", rig.detail)
        advanceTimeBy(5_000); runCurrent()
        assertEquals(1, t.commandAttempts)

        t.commandResult = { WriteResult.Ok }
        rig.repo.connect()
        advanceTimeBy(rig.config.tickMs); runCurrent()
        val sync = t.queries().single()
        rig.respond(sync, emptyList())
        assertEquals(ConnectionState.AUTHENTICATED, rig.connection)
    }

    @Test fun `setForeground 는 저장소 디스패처에서 처리된다 - 호출 즉시가 아니라 다음 실행에서`() = runTest {
        val rig = Rig(this)
        val t = rig.transport
        rig.goReady()
        val before = t.appStates().size
        rig.repo.setForeground(false)
        assertEquals(before, t.appStates().size)
        runCurrent()
        assertFalse(t.appStates().last().active)
    }

    @Test fun `범위 밖 요청은 REQUEST_ID 를 쓰지 않는다 - 다음 요청이 1번 (E7)`() = runTest {
        val rig = Rig(this)
        rig.goReady()
        expectThrows<InvalidRequest> { rig.repo.send(UserRequest.LightBrightness(150)) }
        val id = rig.repo.send(UserRequest.Door(lock = false))
        assertEquals(1L, id)
        assertEquals(listOf(1L), rig.transport.requestIds())
    }

    @Test fun `App State 미수락 중에는 사용자 Query 를 보내지 않고 기다린다 (E4)`() = runTest {
        val rig = Rig(this)
        val t = rig.transport
        rig.goReady()
        t.appStateResult = WriteResult.NotConnected
        advanceTimeBy(1_000); runCurrent()
        assertEquals("앱 상태 전송 실패 — 다시 시도 중", rig.detail)
        val before = t.queries().size
        rig.repo.query(QueryScope.WARNINGS)
        advanceTimeBy(500); runCurrent()
        assertEquals(before, t.queries().size)

        t.appStateResult = WriteResult.Ok
        advanceTimeBy(rig.config.heartbeatMs); runCurrent()
        assertEquals(ConnectionState.AUTHENTICATED, rig.connection)
        assertEquals(2, t.queries().last().scope)
        assertEquals(before + 1, t.queries().size)
    }

    @Test fun `AUTH_FAILED 중에는 사용자 Query 를 보내지 않는다 (E4)`() = runTest {
        val rig = Rig(this)
        val t = rig.transport
        rig.goReady()
        t.commandResult = { if (it == MessageType.M_REQUEST) WriteResult.Rejected(AttError.NOT_REGISTERED) else WriteResult.Ok }
        expectThrows<RequestNotSent> { rig.repo.send(UserRequest.Door(lock = false)) }
        val before = t.queries().size
        rig.repo.query(QueryScope.CURRENT_STATE)
        advanceTimeBy(1_000); runCurrent()
        assertEquals(before, t.queries().size)
        assertEquals("등록되지 않은 단말입니다", rig.detail)
    }

    @Test fun `AUTH_FAILED 중 App State 가 실패했다면 connect 로 풀어도 App State 확인 전에는 READY 아님 (E1)`() = runTest {
        val rig = Rig(this)
        val t = rig.transport
        rig.goReady()
        t.commandResult = { if (it == MessageType.M_REQUEST) WriteResult.Rejected(AttError.NOT_REGISTERED) else WriteResult.Ok }
        expectThrows<RequestNotSent> { rig.repo.send(UserRequest.Door(lock = false)) }
        t.appStateResult = WriteResult.NotConnected
        advanceTimeBy(1_000); runCurrent()
        assertEquals("등록되지 않은 단말입니다", rig.detail)

        t.commandResult = { WriteResult.Ok }
        rig.repo.connect()
        runCurrent()
        assertEquals(ConnectionState.CONNECTED, rig.connection)
        assertEquals("앱 상태 전송 실패 — 다시 시도 중", rig.detail)
        expectThrows<RequestBlocked> { rig.repo.send(UserRequest.Door(lock = false)) }

        t.appStateResult = WriteResult.Ok
        advanceTimeBy(rig.config.heartbeatMs); runCurrent()
        assertEquals(ConnectionState.AUTHENTICATED, rig.connection)
    }

    @Test fun `등록 해제하면 registered 가 false - Gateway 가 다시 알리면 따른다 (E5)`() = runTest {
        val rig = Rig(this)
        rig.goReady()
        assertTrue(rig.repo.registered.value)
        rig.repo.unregister()
        runCurrent()
        assertFalse(rig.repo.registered.value)
        rig.gatewayNotify(gateway())
        assertTrue(rig.repo.registered.value)
    }

    private fun rearPayload(valid: Int) = downstream(MessageType.M_CIS_REAR) {
        u16(34, 1234).u8(36, valid).u8(37, 0).u8(38, 0)
    }

    private fun warningPayload(type: Int, occurrence: Long, active: Int = 1, read: Int = 0, session: Long = S1) =
        downstream(MessageType.M_WARNING, sessionId = session) {
            u8(20, type).u32(21, occurrence).u8(25, 1).u8(26, active).u8(27, read).u8(28, 0).u16(38, 0)
        }

    @Test fun `경고 이력 - 같은 Type 새 발생이 이전을 덮지 않고, 앱 재진입 뒤에도 미확인 이력이 남는다`() = runTest {
        val storage = MemoryWarningHistoryStorage()
        val rig = Rig(this, storage)
        rig.goReady()
        rig.emit(MessageType.M_WARNING, warningPayload(type = 1, occurrence = 10))
        rig.emit(MessageType.M_WARNING, warningPayload(type = 1, occurrence = 10))
        rig.emit(MessageType.M_WARNING, warningPayload(type = 1, occurrence = 11))
        assertEquals(1, rig.repo.vehicleState.value.warnings.count { it.occurrenceId != 0L })
        assertEquals(listOf(11L, 10L), rig.repo.warningHistory.value.map { it.occurrenceId })
        assertEquals(2, storage.saveCount)

        val reentered = Rig(this, storage)
        assertEquals(listOf(11L, 10L), reentered.repo.warningHistory.value.map { it.occurrenceId })
        assertTrue(reentered.repo.warningHistory.value.all { it.unread })
    }

    @Test fun `보관 이력 앱 확인 - 밀려난 발생만, 차량에 보내지 않고 연결 없이도 저장되어 재진입 뒤 남는다 (D21f)`() = runTest {
        val storage = MemoryWarningHistoryStorage()
        val rig = Rig(this, storage)
        rig.goReady()
        rig.emit(MessageType.M_WARNING, warningPayload(type = 1, occurrence = 10))
        rig.emit(MessageType.M_WARNING, warningPayload(type = 1, occurrence = 11))

        val offline = Rig(this, storage)
        offline.repo.confirmArchivedWarnings(offline.repo.warningHistory.value)
        val (current, archived) = offline.repo.warningHistory.value
        assertTrue(archived.confirmedInApp)
        assertEquals(ReadState.UNREAD, archived.read)
        assertTrue(current.unread)
        assertEquals(0, offline.transport.commandAttempts)

        val reentered = Rig(this, storage)
        assertEquals(listOf(false, true), reentered.repo.warningHistory.value.map { it.confirmedInApp })
    }

    @Test fun `이전 Domain 부팅의 마지막 발생은 앱 확인 대상 - 현재 부팅은 문맥 확정 뒤에만 안다 (D21f)`() = runTest {
        val rig = Rig(this)
        assertNull(rig.repo.domainBootId.value)
        rig.goReady()
        rig.emit(MessageType.M_WARNING, warningPayload(type = 1, occurrence = 10))
        assertEquals(3L, rig.repo.domainBootId.value)
        rig.repo.confirmArchivedWarnings(rig.repo.warningHistory.value)
        assertTrue(rig.repo.warningHistory.value.single().unread)

        rig.gatewayNotify(gateway(domainBootId = 9))
        assertEquals(9L, rig.repo.domainBootId.value)
        rig.repo.confirmArchivedWarnings(rig.repo.warningHistory.value)
        assertTrue(rig.repo.warningHistory.value.single().confirmedInApp)
    }

    @Test fun `등록 해제 - 요청·대기 Query·경고 이력·차량 상태를 정리한다 (SEC-005)`() = runTest {
        val rig = Rig(this)
        rig.goReady()
        rig.emit(MessageType.M_WARNING, warningPayload(type = 2, occurrence = 1))
        rig.repo.send(UserRequest.Door(lock = false))
        assertEquals(1, rig.requests.size)
        rig.repo.unregister()
        runCurrent()
        assertTrue(rig.requests.isEmpty())
        assertTrue(rig.repo.warningHistory.value.isEmpty())
        assertTrue(rig.historyStorage.saved.isEmpty())
        assertNull(rig.repo.vehicleState.value.door)
        assertNull(rig.repo.vehicleState.value.lastReceivedAtMs)
        val queriesBefore = rig.transport.queries().size
        advanceTimeBy(5_000); runCurrent()
        assertEquals(queriesBefore, rig.transport.queries().size)
    }

    @Test fun `새 세션 수립 뒤 이전 세션 UNKNOWN 결과를 REQUEST_SESSION+ID 로 조회 (CON-004)`() = runTest {
        val rig = Rig(this)
        val t = rig.transport
        rig.goReady()
        val id = rig.repo.send(UserRequest.Door(lock = false))
        rig.gatewayNotify(gateway(session = S2))
        assertEquals(RequestState.UNKNOWN, rig.requests.single().state)
        val sync = t.queries().last()
        assertEquals(SCOPE_ALL, sync.scope)
        rig.respond(sync, emptyList(), session = S2)
        val recovery = t.queries().last()
        assertEquals(SCOPE_REQUEST_RESULT, recovery.scope)
        assertEquals(S1, recovery.requestSession)
        assertEquals(id, recovery.requestId)
        rig.respond(recovery, listOf(MessageType.M_RESULT to result(S1, id, 2, queryId = recovery.id, session = S2)), session = S2)
        assertEquals(RequestState.DONE, rig.requests.single().state)
    }

    @Test fun `수용 뒤 도어 최종 기한 3 s 를 넘기면 UNKNOWN 과 결과 조회 (PERF-003)`() = runTest {
        val rig = Rig(this)
        val t = rig.transport
        rig.goReady()
        val id = rig.repo.send(UserRequest.Door(lock = false))
        rig.emit(MessageType.M_RESULT, result(S1, id, 1))
        advanceTimeBy(2_900); runCurrent()
        assertEquals(RequestState.IN_PROGRESS, rig.requests.single().state)
        advanceTimeBy(rig.config.finalResultDoorMs); runCurrent()
        assertEquals(RequestState.UNKNOWN, rig.requests.single().state)
        assertEquals(SCOPE_REQUEST_RESULT, t.queries().last().scope)
        assertEquals(id, t.queries().last().requestId)
    }

    @Test fun `차량 전체 마지막 정상 수신 시각 - 문맥이 맞는 완성 메시지만 (DSP-010)`() = runTest {
        val rig = Rig(this)
        assertNull(rig.repo.vehicleState.value.lastReceivedAtMs)
        rig.goReady()
        advanceTimeBy(500); runCurrent()
        val at = testScheduler.currentTime
        rig.emit(MessageType.M_BCM_DOOR_STATE, door(lock = 1))
        assertEquals(at, rig.repo.vehicleState.value.lastReceivedAtMs)
        advanceTimeBy(500); runCurrent()
        rig.emit(MessageType.M_BCM_DOOR_STATE, door(lock = 1, session = S2))
        assertEquals(at, rig.repo.vehicleState.value.lastReceivedAtMs)
    }

    @Test fun `원본 경과(SOURCE_AGE)를 더해 STALE 판정 (SEM-007)`() = runTest {
        val rig = Rig(this)
        rig.goReady()
        val aged = downstream(MessageType.M_BCM_DOOR_STATE) { u8(28, 0).u16(38, 500) }
        rig.emit(MessageType.M_BCM_DOOR_STATE, aged)
        assertEquals(Quality.OK, rig.doorLock!!.quality)
        advanceTimeBy(rig.config.tickMs); runCurrent()
        assertEquals(Quality.STALE, rig.doorLock!!.quality)
    }

    @Test fun `WINDOW 상태 표시 - 미수신이면 null, 600 ms 수신 중단이면 STALE (D15)`() = runTest {
        val rig = Rig(this)
        assertNull(rig.repo.vehicleState.value.window)
        rig.goReady()
        rig.emit(MessageType.M_WINDOW_STATE, downstream(MessageType.M_WINDOW_STATE) { u8(28, 0).u8(30, 100).u8(33, 1) })
        assertEquals(Quality.OK, rig.repo.vehicleState.value.window!!.motion.quality)
        advanceTimeBy(rig.config.fastStaleMs + rig.config.tickMs); runCurrent()
        assertEquals(Quality.STALE, rig.repo.vehicleState.value.window!!.motion.quality)
        assertEquals(true, rig.repo.vehicleState.value.window!!.fullyClosed.value)
    }

    @Test fun `foreground 신호는 transport 한 곳에 - background 면 INACTIVE 한 번, 복귀하면 ACTIVE (D16)`() = runTest {
        val rig = Rig(this)
        rig.goReady()
        rig.repo.setForeground(false); runCurrent()
        assertEquals(listOf(false), rig.transport.foregroundCalls)
        assertFalse(rig.transport.appStates().last().active)
        val writes = rig.transport.appStates().size
        advanceTimeBy(3_000); runCurrent()
        assertEquals(writes, rig.transport.appStates().size)
        rig.repo.setForeground(true); advanceTimeBy(rig.config.tickMs); runCurrent()
        assertTrue(rig.transport.appStates().last().active)
    }
}
