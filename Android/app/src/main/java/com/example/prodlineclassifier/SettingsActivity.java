package com.example.prodlineclassifier;

import android.content.Context;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.SeekBar;
import android.widget.TextView;

import androidx.activity.EdgeToEdge;
import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.lifecycle.ViewModelProvider;

import com.google.gson.Gson;
import com.google.gson.JsonObject;

import java.io.IOException;
import java.util.concurrent.atomic.AtomicReference;

import mqtt.Constants;
import mqtt.MQTTBroadcastReceiver;
import mqtt.MQTTService;
import mqtt.viewmodel.MQTTViewModel;
import observer.topicmanager.ConveyorBeltSpeedListener;
import observer.topicmanager.TopicPublisher;
import okhttp3.Call;
import okhttp3.Callback;
import okhttp3.Response;

public class SettingsActivity extends AppCompatActivity {
    MQTTViewModel mqttViewModel;
    public ImageButton btnBackSettings;
    public Button btnUpdateConvBeltSpeedSettings;
    public SeekBar seekBarConveyorBelt;
    private static final String PREFS_CONV_BELT = "ConvBeltPrefs";
    private static final String KEY_SEEKBAR_CONV_BELT = "seekBarValue";
    TextView txtViewCurrentConvBeltSpeed;
    TextView txtViewNewConvBeltSpeed;
    private MQTTBroadcastReceiver mqttReceiver;

    @RequiresApi(api = Build.VERSION_CODES.TIRAMISU)
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_settings);

        Log.d("Settings", "INICIO Settings");
        mqttViewModel = new ViewModelProvider(this).get(MQTTViewModel.class);
        setViewModelResponses();

        TopicPublisher.events.subscribe("prodlineclassifier/feeds/dcengine", new ConveyorBeltSpeedListener<>(mqttViewModel));

        mqttReceiver = new MQTTBroadcastReceiver();
        registerReceiver(mqttReceiver,
                new IntentFilter("MQTT_MESSAGE_RECEIVED"),
                Context.RECEIVER_EXPORTED);

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

                txtViewNewConvBeltSpeed.setText(String.valueOf(progress));
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

        btnUpdateConvBeltSpeedSettings = findViewById(R.id.btn_update_conv_belt_speed_settings);

        btnUpdateConvBeltSpeedSettings.setOnClickListener(v -> {
            MQTTService.sendTopicMessageToAdafruit("prodlineclassifier/feeds/dcengine", (String) txtViewNewConvBeltSpeed.getText());
        });

        txtViewNewConvBeltSpeed = findViewById(R.id.txt_view_new_conv_belt_speed_settings);

        txtViewNewConvBeltSpeed.setText(String.valueOf(convBeltValue));

        txtViewCurrentConvBeltSpeed = findViewById(R.id.txt_view_current_conv_belt_speed_settings);

        txtViewCurrentConvBeltSpeed.setText("N/A");

        MQTTService.getTopicLatestValue(Constants.DC_ENGINE_FEED_KEY, mqttViewModel);

        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main), (v, insets) -> {
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom);
            return insets;
        });
    }

    private void setViewModelResponses() {
        mqttViewModel.getMqttMessage().observe(this, mqttMsg -> {
            Log.d("Settings", "Recibido ViewModel desde Settings");
            Log.d("Settings", "Topic:   " + mqttMsg.topic + "  Message: " + mqttMsg.message);
            if (mqttMsg.topic.equals(Constants.DC_ENGINE_FEED_KEY)) {
                txtViewCurrentConvBeltSpeed.setText(mqttMsg.message);
            }
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        unregisterReceiver(mqttReceiver);
    }
}