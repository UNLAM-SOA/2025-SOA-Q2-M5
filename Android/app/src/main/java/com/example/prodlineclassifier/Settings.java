package com.example.prodlineclassifier;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.widget.ImageButton;
import android.widget.SeekBar;
import android.widget.TextView;

import androidx.activity.EdgeToEdge;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;


public class Settings extends AppCompatActivity {

    public ImageButton btnBackSettings;
    private SeekBar seekBarConveyorBelt;
    private static final String PREFS_CONV_BELT = "ConvBeltPrefs";
    private static final String KEY_SEEKBAR_CONV_BELT = "seekBarValue";
    TextView txtViewConvBeltSpeed;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_settings);

        btnBackSettings = findViewById(R.id.btn_back_settings);

        btnBackSettings.setOnClickListener(v -> {
            finish(); // Cierra la Activity y vuelve al Main
        });

        seekBarConveyorBelt = findViewById(R.id.seekbar_conv_belt_settings);

        // Recuperar el valor previamente guardado
        SharedPreferences prefs = getSharedPreferences(PREFS_CONV_BELT, MODE_PRIVATE);
        int convBeltValue = prefs.getInt(KEY_SEEKBAR_CONV_BELT, 50); // 0 por defecto
        seekBarConveyorBelt.setProgress(convBeltValue);

        // Guardar el valor cuando cambie
        seekBarConveyorBelt.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                // Guardar el valor en SharedPreferences
                SharedPreferences.Editor editor = getSharedPreferences(PREFS_CONV_BELT, MODE_PRIVATE).edit();
                editor.putInt(KEY_SEEKBAR_CONV_BELT, progress);
                editor.apply();

                txtViewConvBeltSpeed.setText(String.valueOf(progress));
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
                // No requerido
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                // No requerido
            }
        });

        txtViewConvBeltSpeed = findViewById(R.id.txt_view_conv_belt_speed_settings);

        txtViewConvBeltSpeed.setText(String.valueOf(convBeltValue));

        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main), (v, insets) -> {
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom);
            return insets;
        });

    }

}