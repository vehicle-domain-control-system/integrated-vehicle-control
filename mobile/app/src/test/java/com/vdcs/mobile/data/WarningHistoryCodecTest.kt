package com.vdcs.mobile.data

import com.vdcs.mobile.domain.model.ReadState
import com.vdcs.mobile.domain.model.Severity
import com.vdcs.mobile.domain.model.WarningRecord
import com.vdcs.mobile.domain.model.WarningType
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class WarningHistoryCodecTest {
    private val records = listOf(
        WarningRecord(0xFFFF_FFFFL, WarningType.DOOR_OPEN_AFTER_EXIT, 0xFFFF_FFFFL, Severity.EMERGENCY, true, ReadState.UNREAD, 1_000, 2_000),
        WarningRecord(3, WarningType.REAR, 7, Severity.INFO, false, ReadState.READ, 5, 6),
        WarningRecord(3, WarningType.REAR, 6, Severity.CAUTION, false, ReadState.UNREAD, 3, 8, confirmedInApp = true),
    )

    @Test fun `왕복 보존`() {
        assertEquals(records, WarningHistoryCodec.decode(WarningHistoryCodec.encode(records)))
        assertTrue(WarningHistoryCodec.decode("").isEmpty())
    }

    @Test fun `깨진 줄·표에 없는 Type·Severity 는 버린다`() {
        val text = WarningHistoryCodec.encode(records) + "\n1,2,3\n3,99,1,1,1,0,1,1\n3,1,1,9,1,0,1,1\nx,1,1,1,1,0,1,1"
        assertEquals(records, WarningHistoryCodec.decode(text))
    }

    @Test fun `앱 확인 이전 형식(버전 줄 없는 8필드)도 읽는다 - 앱 확인 없음으로 복원`() {
        val legacy = "4294967295,8,4294967295,2,1,0,1000,2000\n3,1,7,0,0,1,5,6"
        assertEquals(
            listOf(
                WarningRecord(0xFFFF_FFFFL, WarningType.DOOR_OPEN_AFTER_EXIT, 0xFFFF_FFFFL, Severity.EMERGENCY, true, ReadState.UNREAD, 1_000, 2_000),
                WarningRecord(3, WarningType.REAR, 7, Severity.INFO, false, ReadState.READ, 5, 6),
            ),
            WarningHistoryCodec.decode(legacy),
        )
    }

    @Test fun `앱 확인은 READ 필드와 따로 저장된다`() {
        val confirmed = WarningRecord(3, WarningType.REAR, 6, Severity.CAUTION, false, ReadState.UNREAD, 3, 8, confirmedInApp = true)
        assertEquals("v2\n3,1,6,1,0,0,3,8,1", WarningHistoryCodec.encode(listOf(confirmed)))
        assertTrue(WarningHistoryCodec.decode(WarningHistoryCodec.encode(emptyList())).isEmpty())
    }
}
