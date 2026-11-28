package com.vdcs.mobile.domain.logic

import com.vdcs.mobile.domain.model.Quality

class FreshnessEvaluator {
    fun evaluate(reported: Quality, receivedAtMs: Long, sourceAgeMs: Long, limitMs: Long, nowMs: Long): Quality =
        if (reported == Quality.OK && isReceptionLost(receivedAtMs, sourceAgeMs, limitMs, nowMs)) Quality.STALE
        else reported

    fun isReceptionLost(lastGoodAtMs: Long, sourceAgeMs: Long, limitMs: Long, nowMs: Long): Boolean =
        (nowMs - lastGoodAtMs) + sourceAgeMs > limitMs
}
