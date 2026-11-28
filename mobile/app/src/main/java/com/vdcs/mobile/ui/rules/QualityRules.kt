package com.vdcs.mobile.ui.rules

import com.vdcs.mobile.domain.model.Qualified
import com.vdcs.mobile.domain.model.Quality

val Quality.isDisplayable: Boolean
    get() = when (this) {
        Quality.OK, Quality.STALE -> true
        Quality.INVALID, Quality.NO_DATA -> false
    }

val Quality.isTrusted: Boolean
    get() = this == Quality.OK

fun worst(a: Quality, b: Quality): Quality = if (a.rank >= b.rank) a else b

private val Quality.rank: Int
    get() = when (this) {
        Quality.OK -> 0
        Quality.STALE -> 1
        Quality.INVALID -> 2
        Quality.NO_DATA -> 3
    }

fun <T> Qualified<T>?.displayableValue(): T? = this?.takeIf { it.quality.isDisplayable }?.value

fun <T> Qualified<T>?.trustedValue(): T? = this?.takeIf { it.quality.isTrusted }?.value
