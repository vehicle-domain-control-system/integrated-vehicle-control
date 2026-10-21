package com.vdcs.mobile.ui.screen

import android.view.View
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.viewinterop.AndroidView
import com.vdcs.mobile.R
import com.vdcs.mobile.domain.model.VehicleSnapshot
import com.vdcs.mobile.ui.CabinGlow
import com.vdcs.mobile.ui.CarPose
import com.vdcs.mobile.ui.CarStatus
import com.vdcs.mobile.ui.screen.carview.AnchorPinsOverlay
import com.vdcs.mobile.ui.screen.carview.AnchorLayout
import com.vdcs.mobile.ui.screen.carview.CarPoseBadge
import com.vdcs.mobile.ui.screen.carview.CarPoseTextSummary
import com.vdcs.mobile.ui.screen.carview.CarSceneView
import com.vdcs.mobile.ui.theme.Sizes
import com.vdcs.mobile.ui.theme.Spacing

@Composable
fun CarViewSlot(snapshot: VehicleSnapshot, origin: String?, nowMs: Long, modifier: Modifier = Modifier) {
    val pose = remember(snapshot.door, snapshot.light) { CarPose.from(snapshot) }
    val status = remember(snapshot.warnings, snapshot.climate, snapshot.rear, origin, nowMs) { CarStatus.from(snapshot, origin, nowMs) }
    Box(modifier.fillMaxWidth().height(Sizes.heroHeight)) {
        val context = LocalContext.current
        var unavailable by rememberSaveable { mutableStateOf(if (CarSceneView.isSupported(context)) null else R.string.car_view_unsupported) }
        val reason = unavailable
        if (reason != null) {
            CarPoseTextSummary(pose, stringResource(reason))
        } else {
            val creation = remember { SceneCreation() }
            val anchors = remember { mutableStateOf(AnchorLayout.NONE) }
            CarScene(pose, status.cabinGlow, creation, onAnchorsMoved = { anchors.value = it })
            AnchorPinsOverlay(status.pinsByAnchor, layout = { anchors.value })
            SideEffect { if (creation.failed) unavailable = R.string.car_view_init_failed }
            CarPoseBadge(pose, Modifier.align(Alignment.TopCenter).padding(Spacing.m))
        }
    }
}

private class SceneCreation {
    var failed = false
}

@Composable
private fun CarScene(pose: CarPose, glow: CabinGlow?, creation: SceneCreation, onAnchorsMoved: (AnchorLayout) -> Unit) {
    AndroidView(
        factory = { context ->
            val scene = CarSceneView.createOrNull(context)
            creation.failed = scene == null
            scene ?: View(context)
        },
        modifier = Modifier.fillMaxSize(),
        onReset = null,
        onRelease = { view -> (view as? CarSceneView)?.destroy() },
        update = { view ->
            (view as? CarSceneView)?.let {
                it.onAnchorsMoved = onAnchorsMoved
                it.show(pose, glow)
            }
        },
    )
}
