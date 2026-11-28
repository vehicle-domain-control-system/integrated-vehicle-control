package com.vdcs.mobile.ui

import android.os.Bundle
import android.graphics.Color as AndroidColor
import androidx.activity.ComponentActivity
import androidx.activity.SystemBarStyle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import com.vdcs.mobile.VdcsApplication
import com.vdcs.mobile.ui.screen.VdcsApp
import com.vdcs.mobile.ui.theme.VdcsTheme

class MainActivity : ComponentActivity() {
    private val viewModel: VehicleViewModel by viewModels {
        VehicleViewModel.factory((application as VdcsApplication).container)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge(
            statusBarStyle = SystemBarStyle.dark(AndroidColor.TRANSPARENT),
            navigationBarStyle = SystemBarStyle.dark(AndroidColor.TRANSPARENT),
        )
        super.onCreate(savedInstanceState)
        setContent {
            VdcsTheme {
                VdcsApp(viewModel)
            }
        }
    }
}
