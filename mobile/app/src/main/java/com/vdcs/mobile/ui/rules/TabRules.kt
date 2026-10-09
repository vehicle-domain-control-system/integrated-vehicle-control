package com.vdcs.mobile.ui.rules

enum class AppTab { HOME, CLIMATE, LIGHT_KEY, ALERTS, VEHICLE }

val AppTab.label: String
    get() = when (this) {
        AppTab.HOME -> "차량"
        AppTab.CLIMATE -> "공조"
        AppTab.LIGHT_KEY -> "조명·키"
        AppTab.ALERTS -> "알림"
        AppTab.VEHICLE -> "진단"
    }

val AppTab.title: String
    get() = if (this == AppTab.HOME) "내 차" else label

enum class HomeSection { CAR_VIEW, STATUS_SENTENCE, CONNECT_CTA, QUICK_CONTROLS, WARNING_LINE, CONNECTION }

enum class VehicleSection { DOOR, CABIN, WINDOW, AVAILABILITY, ECU, REQUESTS, DEMO_CONTROLS }

object TabRules {
    val START_TAB = AppTab.HOME

    val HOME_ORDER: List<HomeSection> = HomeSection.entries

    fun homeSections(showConnectCta: Boolean): List<HomeSection> =
        HOME_ORDER.filter { it != HomeSection.CONNECT_CTA || showConnectCta }

    fun vehicleSections(hasDemoControls: Boolean): List<VehicleSection> =
        VehicleSection.entries.filter { it != VehicleSection.DEMO_CONTROLS || hasDemoControls }
}
