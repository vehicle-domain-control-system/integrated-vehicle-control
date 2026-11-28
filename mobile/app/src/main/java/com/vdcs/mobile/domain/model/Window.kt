package com.vdcs.mobile.domain.model

data class WindowState(
    val motion: Qualified<WindowMotion>,
    val ecuState: Qualified<WindowEcuState>,
    val positionClosedPercent: Qualified<Int>,
    val fullyOpen: Qualified<Boolean>,
    val fullyClosed: Qualified<Boolean>,
    val antiPinch: Qualified<AntiPinchStatus>,
    val reversing: Qualified<Boolean>,
)

enum class WindowMotion { STOPPED, OPENING, CLOSING, ANTIPINCH_REVERSING }

enum class WindowEcuState { INIT, READY, DEGRADED, FAULT }

enum class AntiPinchStatus { IDLE, ACTIVE, COMPLETED, ABORTED }

data class WindowFault(
    val category: Qualified<FaultCategory>,
    val code: Qualified<Int>,
    val status: Qualified<WindowFaultStatus>,
    val occurrenceId: Long,
)

enum class WindowFaultStatus { NONE, ACTIVE, RECOVERING }
