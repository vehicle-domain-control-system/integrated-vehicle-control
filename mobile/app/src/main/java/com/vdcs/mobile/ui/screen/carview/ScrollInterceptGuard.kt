package com.vdcs.mobile.ui.screen.carview

import android.view.MotionEvent
import android.view.View

internal object ScrollInterceptGuard {
    fun onTouch(view: View, event: MotionEvent) {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> view.parent?.requestDisallowInterceptTouchEvent(true)
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> view.parent?.requestDisallowInterceptTouchEvent(false)
        }
    }
}
