package com.vdcs.mobile.data

import kotlinx.coroutines.CoroutineScope
import kotlin.coroutines.ContinuationInterceptor
import kotlin.coroutines.CoroutineContext

internal fun CoroutineScope.confinedDispatcher(): CoroutineContext =
    requireNotNull(coroutineContext[ContinuationInterceptor]) { "scope 에 단일 병렬도 디스패처가 있어야 한다" }
