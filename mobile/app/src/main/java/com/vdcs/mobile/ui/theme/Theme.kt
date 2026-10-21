package com.vdcs.mobile.ui.theme

import android.content.Context
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.ColorScheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Shapes
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.ReadOnlyComposable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.colorResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.vdcs.mobile.R

@Composable
private fun vdcsDarkColors(): ColorScheme {
    val background = colorResource(R.color.vdcs_background)
    val surface = colorResource(R.color.vdcs_surface)
    val surfaceVariant = colorResource(R.color.vdcs_surface_variant)
    val onSurface = colorResource(R.color.vdcs_on_surface)
    return darkColorScheme(
        primary = colorResource(R.color.vdcs_accent),
        onPrimary = colorResource(R.color.vdcs_on_accent),
        primaryContainer = colorResource(R.color.vdcs_accent_container),
        onPrimaryContainer = colorResource(R.color.vdcs_on_accent_container),
        secondary = colorResource(R.color.vdcs_neutral),
        onSecondary = background,
        secondaryContainer = colorResource(R.color.vdcs_neutral_container),
        onSecondaryContainer = onSurface,
        tertiary = colorResource(R.color.vdcs_caution),
        onTertiary = background,
        tertiaryContainer = colorResource(R.color.vdcs_caution_container),
        onTertiaryContainer = colorResource(R.color.vdcs_on_caution_container),
        error = colorResource(R.color.vdcs_emergency),
        onError = background,
        errorContainer = colorResource(R.color.vdcs_emergency_container),
        onErrorContainer = colorResource(R.color.vdcs_on_emergency_container),
        background = background,
        onBackground = onSurface,
        surface = surface,
        onSurface = onSurface,
        surfaceVariant = surfaceVariant,
        onSurfaceVariant = colorResource(R.color.vdcs_on_surface_variant),
        surfaceContainer = surface,
        surfaceContainerLow = surface,
        surfaceContainerHigh = surfaceVariant,
        surfaceContainerHighest = surfaceVariant,
        outline = colorResource(R.color.vdcs_outline),
        outlineVariant = colorResource(R.color.vdcs_outline_variant),
    )
}

val ColorScheme.ok: Color
    @Composable @ReadOnlyComposable
    get() = colorResource(R.color.vdcs_ok)

val ColorScheme.interiorLight: Color
    @Composable @ReadOnlyComposable
    get() = colorResource(R.color.vdcs_interior_light)

val ColorScheme.onInteriorLight: Color
    @Composable @ReadOnlyComposable
    get() = colorResource(R.color.vdcs_on_interior_light)

internal class SceneColors(
    val background: Color,
    val doorHighlight: Color,
    val climateCool: Color,
    val climateHeat: Color,
    val groundShadow: Color,
) {
    companion object {
        fun from(context: Context) = SceneColors(
            background = Color(context.getColor(R.color.vdcs_background)),
            doorHighlight = Color(context.getColor(R.color.vdcs_caution)),
            climateCool = Color(context.getColor(R.color.scene_climate_cool)),
            climateHeat = Color(context.getColor(R.color.scene_climate_heat)),
            groundShadow = Color(context.getColor(R.color.scene_ground_shadow)),
        )
    }
}

private val VdcsShapes = Shapes(
    extraSmall = RoundedCornerShape(4.dp),
    small = RoundedCornerShape(6.dp),
    medium = RoundedCornerShape(8.dp),
    large = RoundedCornerShape(10.dp),
    extraLarge = RoundedCornerShape(10.dp),
)

private val VdcsTypography = Typography().run { copy(titleLarge = titleLarge.copy(fontWeight = FontWeight.Bold)) }

@Composable
fun VdcsTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = vdcsDarkColors(), shapes = VdcsShapes, typography = VdcsTypography, content = content)
}
