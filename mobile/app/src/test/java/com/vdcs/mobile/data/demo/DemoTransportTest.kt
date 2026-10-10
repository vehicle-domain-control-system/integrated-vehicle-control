package com.vdcs.mobile.data.demo

import com.vdcs.mobile.data.MemoryWarningHistoryStorage
import com.vdcs.mobile.data.VdcsConfig
import com.vdcs.mobile.data.VehicleRepositoryImpl
import com.vdcs.mobile.data.ble.GattSpec
import com.vdcs.mobile.data.ble.LinkState
import com.vdcs.mobile.data.protocol.BleFrameCodec
import com.vdcs.mobile.data.protocol.ByteReader
import com.vdcs.mobile.data.protocol.MessageCodec
import com.vdcs.mobile.data.protocol.MessageType
import com.vdcs.mobile.data.protocol.Reassembler
import com.vdcs.mobile.domain.logic.FreshnessEvaluator
import com.vdcs.mobile.domain.logic.RequestTracker
import com.vdcs.mobile.domain.logic.SessionContext
import com.vdcs.mobile.domain.model.ConnectionState
import com.vdcs.mobile.domain.model.LockState
import com.vdcs.mobile.domain.model.OpenState
import com.vdcs.mobile.domain.model.Quality
import com.vdcs.mobile.domain.model.ReadState
import com.vdcs.mobile.domain.model.RequestState
import com.vdcs.mobile.domain.model.ResultReason
import com.vdcs.mobile.domain.model.Rgb
import com.vdcs.mobile.domain.model.Severity
import com.vdcs.mobile.domain.model.UserRequest
import com.vdcs.mobile.domain.model.WarningType
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.launch
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.advanceTimeBy
import kotlinx.coroutines.test.runCurrent
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import kotlin.random.Random

@OptIn(ExperimentalCoroutinesApi::class)
class DemoTransportTest {
    private class Rig(test: TestScope, mtu: Int = GattSpec.PREFERRED_MTU) {
        val clock = { test.testScheduler.currentTime }
        val config = VdcsConfig()
        val demo = DemoTransport(test.backgroundScope, clock, mtu, Random(42))
        val repo = VehicleRepositoryImpl(
            transport = demo,
            frameCodec = BleFrameCodec(),
            reassembler = Reassembler(config.reassemblyTimeoutMs),
            messageCodec = MessageCodec(),
            requestTracker = RequestTracker(config.resultDeadlines(), config.recentRequestLimit),
            freshness = FreshnessEvaluator(),
            session = SessionContext { Random(7).nextLong() },
            warningHistoryStorage = MemoryWarningHistoryStorage(),
            config = config,
            scope = test.backgroundScope,
            clock = clock,
        )
        val state get() = repo.vehicleState.value
        fun request(id: Long) = repo.requests.value.single { it.requestId == id }
    }

    private suspend fun TestScope.ready(rig: Rig) {
        runCurrent()
        rig.repo.connect()
        advanceTimeBy(500); runCurrent()
        assertEquals(ConnectionState.AUTHENTICATED, rig.repo.connectionState.value)
    }

    @Test fun `데모로 READY 도달 - 초기 Query 로 상태·경고 8종이 채워진다`() = runTest {
        val rig = Rig(this)
        ready(rig)
        val s = rig.state
        assertEquals(LockState.LOCKED, s.door!!.lock.value)
        assertEquals(OpenState.CLOSED, s.door!!.open.value)
        assertEquals(24.5, s.environment!!.temperatureC.value!!, 0.001)
        assertEquals(WarningType.entries.toSet(), s.warnings.map { it.type }.toSet())
        assertTrue(s.warnings.none { it.active })

        advanceTimeBy(5_000); runCurrent()
        assertEquals(ConnectionState.AUTHENTICATED, rig.repo.connectionState.value)
        assertEquals(Quality.OK, rig.state.door!!.lock.quality)
        assertEquals(Quality.OK, rig.state.settings!!.lightRgb.quality)
        assertTrue(rig.state.warnings.all { it.quality == Quality.OK })
    }

    @Test fun `MTU 23 에서도 분할·조립을 거쳐 READY 도달`() = runTest {
        val rig = Rig(this, mtu = GattSpec.MIN_MTU)
        ready(rig)
        assertEquals(LockState.LOCKED, rig.state.door!!.lock.value)
        assertEquals(8, rig.state.warnings.size)
    }

    @Test fun `Door Unlock 요청 - ACCEPTED 뒤 DONE, 도어 상태 반영`() = runTest {
        val rig = Rig(this)
        ready(rig)
        val id = rig.repo.send(UserRequest.Door(lock = false))
        advanceTimeBy(100); runCurrent()
        assertEquals(RequestState.ACCEPTED, rig.request(id).state)
        assertEquals(LockState.LOCKED, rig.state.door!!.lock.value)

        advanceTimeBy(400); runCurrent()
        val done = rig.request(id)
        assertEquals(RequestState.DONE, done.state)
        assertTrue(done.confirmed)
        assertEquals(ResultReason.NONE, done.reason)
        assertEquals(LockState.UNLOCKED, rig.state.door!!.lock.value)

        advanceTimeBy(2_000); runCurrent()
        assertEquals(RequestState.DONE, rig.request(id).state)
    }

    @Test fun `조명 색 요청 - BCM 조명 상태와 사용자 설정에 반영`() = runTest {
        val rig = Rig(this)
        ready(rig)
        val color = Rgb(255, 64, 0)
        val id = rig.repo.send(UserRequest.LightColor(color))
        advanceTimeBy(500); runCurrent()
        assertEquals(RequestState.DONE, rig.request(id).state)
        assertEquals(color, rig.state.light!!.rgb.value)
        assertEquals(color, rig.state.settings!!.lightRgb.value)
    }

    @Test fun `열린 문 잠금 요청은 REJECTED(DOOR_OPEN) - 시연 조작 도어 열기·닫기`() = runTest {
        val rig = Rig(this)
        ready(rig)
        rig.repo.send(UserRequest.Door(lock = false))
        advanceTimeBy(500); runCurrent()
        rig.demo.openDoor()
        runCurrent()
        assertEquals(OpenState.OPEN, rig.state.door!!.open.value)

        val id = rig.repo.send(UserRequest.Door(lock = true))
        advanceTimeBy(500); runCurrent()
        assertEquals(RequestState.REJECTED, rig.request(id).state)
        assertEquals(ResultReason.DOOR_OPEN, rig.request(id).reason)
        assertEquals(LockState.UNLOCKED, rig.state.door!!.lock.value)

        rig.demo.closeDoor()
        runCurrent()
        assertEquals(OpenState.CLOSED, rig.state.door!!.open.value)
    }

    @Test fun `Type 8 경고 발생 - 읽음 처리(M_WARNING_ACK) 후 READ, 해제는 별개`() = runTest {
        val rig = Rig(this)
        ready(rig)
        rig.demo.raiseWarning(WarningType.DOOR_OPEN_AFTER_EXIT, Severity.CAUTION)
        runCurrent()
        val w = rig.state.warnings.single { it.type == WarningType.DOOR_OPEN_AFTER_EXIT }
        assertTrue(w.active)
        assertEquals(ReadState.UNREAD, w.read)

        rig.repo.acknowledgeWarning(w)
        advanceTimeBy(100); runCurrent()
        val read = rig.state.warnings.single { it.type == WarningType.DOOR_OPEN_AFTER_EXIT }
        assertEquals(ReadState.READ, read.read)
        assertTrue(read.active)

        rig.demo.clearWarning(WarningType.DOOR_OPEN_AFTER_EXIT)
        runCurrent()
        assertTrue(!rig.state.warnings.single { it.type == WarningType.DOOR_OPEN_AFTER_EXIT }.active)
    }

    @Test fun `경고 모두 해제 - 발생 중이던 경고가 전부 CLEAR`() = runTest {
        val rig = Rig(this)
        ready(rig)
        rig.demo.raiseWarning(WarningType.REAR, Severity.EMERGENCY)
        rig.demo.raiseWarning(WarningType.PINCH, Severity.EMERGENCY)
        runCurrent()
        assertEquals(2, rig.state.warnings.count { it.active })

        rig.demo.clearAllWarnings()
        runCurrent()
        assertEquals(0, rig.state.warnings.count { it.active })
    }

    @Test fun `실내 온도 변화가 실내 환경에 반영`() = runTest {
        val rig = Rig(this)
        ready(rig)
        rig.demo.setCabinTemperature(30.25)
        runCurrent()
        assertEquals(30.25, rig.state.environment!!.temperatureC.value!!, 0.001)
    }

    @Test fun `연결 해제 후 재연결 - 같은 앱 인스턴스면 같은 문맥으로 다시 READY`() = runTest {
        val rig = Rig(this)
        ready(rig)
        rig.repo.disconnect()
        runCurrent()
        assertEquals(ConnectionState.DISCONNECTED, rig.repo.connectionState.value)
        advanceTimeBy(1_000); runCurrent()
        ready(rig)
    }

    @Test fun `connect 를 겹쳐 불러도 주기 송신은 한 벌 - 도어 상태 200 ms 마다 1건`() = runTest {
        val rig = Rig(this)
        val doorAt = mutableListOf<Long>()
        backgroundScope.launch {
            rig.demo.incomingFrames.collect { f ->
                if (f[1].toInt() == MessageType.M_BCM_DOOR_STATE && ByteReader(f).u32(8 + 16) == 0L) {
                    doorAt += testScheduler.currentTime
                }
            }
        }
        runCurrent()
        launch { rig.repo.connect() }
        launch { rig.demo.connect() }
        launch { rig.repo.connect() }
        advanceTimeBy(1_000); runCurrent()
        assertEquals(ConnectionState.AUTHENTICATED, rig.repo.connectionState.value)

        rig.demo.connect()
        val from = testScheduler.currentTime
        advanceTimeBy(1_000); runCurrent()
        val window = doorAt.filter { it > from && it <= from + 1_000 }
        assertEquals(5, window.size)
        assertEquals(window.size, window.toSet().size)
    }

    @Test fun `연결 중 해제하면 연결을 마치지 않는다`() = runTest {
        val rig = Rig(this)
        runCurrent()
        launch { rig.demo.connect() }
        advanceTimeBy(100); runCurrent()
        rig.demo.disconnect()
        advanceTimeBy(1_000); runCurrent()
        assertEquals(ConnectionState.DISCONNECTED, rig.repo.connectionState.value)
    }

    @Test fun `연결 중 해제 뒤 다시 연결하면 새 시도의 시간에 연결된다 - 옛 시도가 마치지 않는다 (E10)`() = runTest {
        val rig = Rig(this)
        runCurrent()
        launch { rig.demo.connect() }
        advanceTimeBy(100); runCurrent()
        rig.demo.disconnect()
        advanceTimeBy(50); runCurrent()
        launch { rig.demo.connect() }
        advanceTimeBy(200); runCurrent()
        assertTrue(rig.demo.link.value is LinkState.Connecting)
        advanceTimeBy(150); runCurrent()
        assertEquals(LinkState.Linked, rig.demo.link.value)
    }
}
