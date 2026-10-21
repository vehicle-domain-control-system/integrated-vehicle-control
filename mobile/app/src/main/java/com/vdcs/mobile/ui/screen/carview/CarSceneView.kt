package com.vdcs.mobile.ui.screen.carview

import android.annotation.SuppressLint
import android.app.ActivityManager
import android.content.Context
import android.util.Log
import android.view.MotionEvent
import androidx.activity.ComponentActivity
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.colorspace.ColorSpaces
import com.google.android.filament.View
import com.vdcs.mobile.ui.CabinGlow
import com.vdcs.mobile.ui.CarPose
import com.vdcs.mobile.ui.theme.SceneColors
import io.github.sceneview.SceneView
import io.github.sceneview.gesture.CameraGestureDetector
import io.github.sceneview.math.Position
import io.github.sceneview.node.ImageNode
import io.github.sceneview.node.ModelNode

@SuppressLint("ViewConstructor")
internal class CarSceneView private constructor(context: Context) : SceneView(
    context = context,
    cameraManipulator = CameraGestureDetector.DefaultCameraManipulator(
        orbitHomePosition = CAMERA_HOME,
        targetPosition = CAMERA_TARGET,
    ),
) {
    private var model: ModelNode? = null
    private var groundShadow: ImageNode? = null
    private var rig: CarRig? = null
    private var lights: CabinLights? = null
    private val projector = AnchorProjector()

    var onAnchorsMoved: ((AnchorLayout) -> Unit)? = null

    override val activity: ComponentActivity? get() = null

    private fun load() {
        val colors = SceneColors.from(context)
        environment = ShowroomStage.studioEnvironment(environmentLoader)
        paintBackground(colors.background)
        cameraNode.position = CAMERA_HOME
        cameraNode.lookAt(CAMERA_TARGET)
        val node = ModelNode(modelLoader.createModelInstance(MODEL_ASSET), autoAnimate = false)
        model = node
        rig = CarRig(node, colors)
        val cabin = CabinLights(engine, colors)
        lights = cabin
        val shadow = ShowroomStage.groundShadow(materialLoader, colors.groundShadow)
        groundShadow = shadow
        childNodes = listOf(shadow, node) + cabin.nodes
    }

    private fun paintBackground(color: Color) {
        val srgb = color.convert(ColorSpaces.Srgb)
        skybox = null
        view.blendMode = View.BlendMode.TRANSLUCENT
        renderer.clearOptions = renderer.clearOptions.apply {
            clear = true
            clearColor = floatArrayOf(srgb.red, srgb.green, srgb.blue, OPAQUE)
        }
    }

    fun show(pose: CarPose, glow: CabinGlow?) {
        rig?.setTarget(pose)
        lights?.show(pose.light, glow)
    }

    override fun onFrame(frameTimeNanos: Long) {
        rig?.stepDoors(frameTimeNanos)
        super.onFrame(frameTimeNanos)
        if (rig != null) projector.project(view, cameraNode)?.let { onAnchorsMoved?.invoke(it) }
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouchEvent(event: MotionEvent): Boolean {
        ScrollInterceptGuard.onTouch(this, event)
        return super.onTouchEvent(event)
    }

    override fun destroy() {
        if (!isDestroyed) {
            onAnchorsMoved = null
            rig?.release()
            rig = null
            childNodes = emptyList()
            lights?.destroy()
            lights = null
            model?.destroy()
            model = null
            groundShadow?.destroy()
            groundShadow = null
        }
        super.destroy()
    }

    companion object {
        private const val LOG_TAG = "VdcsCarView"
        private const val MODEL_ASSET = "models/car.glb"

        private const val MIN_GLES_VERSION = 0x30000

        private const val OPAQUE = 1f

        private val CAMERA_TARGET = Position(0f, 0.55f, 0f)

        private val CAMERA_HOME = Position(4.3f, 2.3f, 3.1f)

        fun isSupported(context: Context): Boolean {
            val am = context.getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager ?: return false
            return am.deviceConfigurationInfo.reqGlEsVersion >= MIN_GLES_VERSION
        }

        fun createOrNull(context: Context): CarSceneView? {
            val view = renderStep("렌더러 생성") { CarSceneView(context) } ?: return null
            return renderStep("장면 로드") { view.also { it.load() } } ?: run {
                view.destroy()
                null
            }
        }

        private inline fun <T> renderStep(step: String, block: () -> T): T? =
            try {
                block()
            } catch (e: RuntimeException) {
                Log.w(LOG_TAG, "3D $step 실패", e)
                null
            } catch (e: LinkageError) {
                Log.w(LOG_TAG, "3D $step 실패 (네이티브)", e)
                null
            }
    }
}
