package com.vdcs.mobile.ui.screen.carview

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RadialGradient
import android.graphics.Shader
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import io.github.sceneview.environment.Environment
import io.github.sceneview.loaders.EnvironmentLoader
import io.github.sceneview.loaders.MaterialLoader
import io.github.sceneview.math.Rotation
import io.github.sceneview.math.Size
import io.github.sceneview.node.ImageNode

internal object ShowroomStage {
    private const val STUDIO_HDR_ASSET = "environments/studio_small_09_1k.hdr"

    private const val LAY_FLAT_DEG = -90f

    fun studioEnvironment(loader: EnvironmentLoader): Environment {
        val environment = checkNotNull(loader.createHDREnvironment(assetFileLocation = STUDIO_HDR_ASSET, createSkybox = false)) {
            "$STUDIO_HDR_ASSET 를 읽지 못함"
        }
        environment.indirectLight?.intensity = CarSceneTuning.IBL_INTENSITY
        return environment
    }

    fun groundShadow(loader: MaterialLoader, shadow: Color): ImageNode =
        ImageNode(
            materialLoader = loader,
            bitmap = radialFalloff(shadow),
            size = Size(CarSceneTuning.GROUND_SHADOW_WIDTH_M, CarSceneTuning.GROUND_SHADOW_LENGTH_M, 0f),
        ).apply { rotation = Rotation(x = LAY_FLAT_DEG) }

    private fun radialFalloff(shadow: Color): Bitmap {
        val px = CarSceneTuning.GROUND_SHADOW_TEXTURE_PX
        val half = px / 2f
        val dark = shadow.copy(alpha = CarSceneTuning.GROUND_SHADOW_ALPHA).toArgb()
        val clear = shadow.copy(alpha = 0f).toArgb()
        val gradient = RadialGradient(
            half, half, half,
            intArrayOf(dark, dark, clear),
            floatArrayOf(0f, CarSceneTuning.GROUND_SHADOW_CORE, 1f),
            Shader.TileMode.CLAMP,
        )
        return Bitmap.createBitmap(px, px, Bitmap.Config.ARGB_8888).also {
            Canvas(it).drawPaint(Paint().apply { shader = gradient })
        }
    }
}
