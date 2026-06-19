package com.example.momentos_hu_zernike

import android.graphics.Bitmap
import android.os.Bundle
import android.widget.Button
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.GravityCompat
import androidx.drawerlayout.widget.DrawerLayout
import java.nio.ByteBuffer

class MainActivity : AppCompatActivity() {

    private lateinit var drawingView: DrawingView
    private lateinit var tvResult: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        drawingView = findViewById(R.id.drawingView)
        tvResult = findViewById(R.id.tvResult)

        val btnClear = findViewById<Button>(R.id.btnClear)
        val btnClassify = findViewById<Button>(R.id.btnClassify)
        val btnOpenDrawer = findViewById<TextView>(R.id.btnOpenDrawer)
        val drawerLayout = findViewById<DrawerLayout>(R.id.drawerLayout)

        // Bloquear el gesto de deslizar para que el DrawerLayout no pelee con la navegación del sistema
        drawerLayout.setDrawerLockMode(DrawerLayout.LOCK_MODE_LOCKED_CLOSED)

        btnOpenDrawer.setOnClickListener {
            drawerLayout.openDrawer(GravityCompat.START)
        }

        // Interceptar el gesto de "Atrás" (deslizar desde el borde) para evitar que la app se cierre por error
        var backPressedTime: Long = 0
        onBackPressedDispatcher.addCallback(this, object : androidx.activity.OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                if (drawerLayout.isDrawerOpen(GravityCompat.START)) {
                    // Si el panel está abierto, el gesto Atrás lo cierra
                    drawerLayout.closeDrawer(GravityCompat.START)
                } else {
                    // Si está cerrado, aplicamos "Doble Atrás para Salir"
                    if (backPressedTime + 2000 > System.currentTimeMillis()) {
                        isEnabled = false
                        onBackPressedDispatcher.onBackPressed()
                    } else {
                        android.widget.Toast.makeText(this@MainActivity, "Desliza el borde otra vez para salir de la app", android.widget.Toast.LENGTH_SHORT).show()
                    }
                    backPressedTime = System.currentTimeMillis()
                }
            }
        })

        // Cargar los descriptores de entrenamiento y enviarlos a C++
        val descriptorsText = resources.openRawResource(R.raw.descriptors_train).bufferedReader().use { it.readText() }
        initClassifier(descriptorsText)

        btnClear.setOnClickListener {
            drawingView.clearCanvas()
            tvResult.text = "Resultado: "
        }

        btnClassify.setOnClickListener {
            val bitmap = drawingView.getBitmap()
            if (bitmap != null) {
                // Obtenemos los pixeles de la imagen (Bitmap a un arreglo de bytes)
                val width = bitmap.width
                val height = bitmap.height
                
                val size = bitmap.rowBytes * bitmap.height
                val byteBuffer = ByteBuffer.allocate(size)
                bitmap.copyPixelsToBuffer(byteBuffer)
                val byteArray = byteBuffer.array()

                // Llamamos a la función nativa en C++ y recibimos el número de clase
                val predictedClass = classifyImage(byteArray, width, height)
                
                tvResult.text = "Resultado: Clase $predictedClass"
            }
        }
    }

    /**
     * Inicializa el clasificador en C++ enviándole el texto con los descriptores de entrenamiento.
     */
    external fun initClassifier(descriptorsData: String)

    /**
     * Llama al código C++ pasando los píxeles de la imagen dibujada.
     * Retorna el número de la clase (0 a 6) o -1 si hubo un error.
     */
    external fun classifyImage(imageData: ByteArray, width: Int, height: Int): Int

    companion object {
        init {
            System.loadLibrary("momentos_hu_zernike")
        }
    }
}