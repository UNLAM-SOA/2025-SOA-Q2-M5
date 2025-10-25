package com.example.prodlineclassifier;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.EdgeToEdge;
import androidx.annotation.RequiresApi;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.lifecycle.ViewModelProvider;

import com.example.prodlineclassifier.databinding.ActivityMainBinding;
import com.example.prodlineclassifier.databinding.ActivitySettingsBinding;

import mqtt.Constants;
import mqtt.MQTTBroadcastReceiver;
import mqtt.MQTTService;
import mqtt.viewmodel.MQTTViewModel;
import observer.topicmanager.ConveyorBeltSpeedListener;
import observer.topicmanager.SystemStatusListener;
import observer.topicmanager.TopicPublisher;

public class MainActivity extends AppCompatActivity {
    MQTTViewModel mqttViewModel;
    public Button btnSettingsMain;
    public Button btnStartMain;
    public Button btnStopMain;
    public Button btnProcessLogMain;
    public TextView txtViewSystemStatus;
    public String systemStatus;
    private MQTTBroadcastReceiver mqttReceiver;

    @RequiresApi(api = Build.VERSION_CODES.TIRAMISU)
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_main);

        mqttViewModel = new ViewModelProvider(this).get(MQTTViewModel.class);
        setViewModelResponses();

        TopicPublisher.events.subscribe("prodlineclassifier/feeds/systemstatus", new SystemStatusListener<>(mqttViewModel));

        // Iniciar el servicio MQTT
        // Intent serviceIntent = new Intent(this, MQTTService.class);
        // startService(serviceIntent);
        startForegroundService(new Intent(this, MQTTService.class));

        // Registrar receiver para recibir mensajes del servicio
        mqttReceiver = new MQTTBroadcastReceiver();
        registerReceiver(mqttReceiver,
                         new IntentFilter("MQTT_MESSAGE_RECEIVED"),
                         Context.RECEIVER_EXPORTED);

        systemStatus = getString(R.string.txt_view_status);

        txtViewSystemStatus = findViewById(R.id.txt_view_systemstatus_main);

        btnSettingsMain = findViewById(R.id.btn_settings_main);

        btnSettingsMain.setOnClickListener(v -> {
            // Intent para abrir Settings.java
            Intent intent = new Intent(MainActivity.this, SettingsActivity.class);
            startActivity(intent);
        });

        btnProcessLogMain = findViewById(R.id.btn_process_log_main);

        btnProcessLogMain.setOnClickListener(v -> {
            // Intent para abrir Settings.java
            Intent intent = new Intent(MainActivity.this, ProcessLogActivity.class);
            startActivity(intent);
        });

        btnStartMain = findViewById(R.id.btn_start_main);

        btnStartMain.setOnClickListener(v -> {
            updateSysStatus(getString(R.string.info_sysstat_start_main), Constants.UPDATE_SYSSTAT_SOURCE_MAIN);
        });

        btnStopMain = findViewById(R.id.btn_stop_main);

        btnStopMain.setOnClickListener(v -> {
            updateSysStatus(getString(R.string.info_sysstat_stop_main), Constants.UPDATE_SYSSTAT_SOURCE_MAIN);
        });

        MQTTService.getTopicLatestValue(Constants.SYSTEM_STATUS_FEED_KEY, mqttViewModel);

        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main), (v, insets) -> {
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom);
            return insets;
        });
    }

    private void setViewModelResponses() {
        mqttViewModel.getMqttMessage().observe(this, mqttMsg -> {
            Log.d("Main", "Recibido ViewModel desde Main");
            Log.d("Main", "Topic:   " + mqttMsg.topic + "   Message: " + mqttMsg.message);
            if (mqttMsg.topic.equals(Constants.SYSTEM_STATUS_FEED_KEY)) {
                Log.d("Main", "Actualizo SysStat");
                updateSysStatus(mqttMsg.message, Constants.UPDATE_SYSSTAT_SOURCE_ADAFRUIT);
            }
        });
    }

    private void updateSysStatus(String newSystemStatus, String source) {
        txtViewSystemStatus = findViewById(R.id.txt_view_systemstatus_main);
        GradientDrawable bgDrawable = (GradientDrawable) txtViewSystemStatus.getBackground();
        int color;

        switch (newSystemStatus) {
            case "Running":
                if(systemStatus.equals("Running")) {
                    Toast.makeText(this, "System is already running", Toast.LENGTH_SHORT).show();
                    return;
                }
                if(!systemStatus.equals("Manually Stopped")
                    && !systemStatus.equals("Emergency Stopped")
                    && source.equals(Constants.UPDATE_SYSSTAT_SOURCE_MAIN))
                {
                    Toast.makeText(this, "System is offline", Toast.LENGTH_SHORT).show();
                    return;
                }
                color = Color.parseColor(getString(R.string.color_sysstat_running));
                if(source.equals(Constants.UPDATE_SYSSTAT_SOURCE_MAIN))
                {
                    MQTTService.sendTopicMessageToAdafruit("prodlineclassifier/feeds/systemstatus", newSystemStatus);
                }
                break;
            case "Manually Stopped":
                if(!systemStatus.equals("Running")) {
                    Toast.makeText(this, "System isn't running right now", Toast.LENGTH_SHORT).show();
                    return;
                }
                color = Color.parseColor(getString(R.string.color_sysstat_manually_stopped));
                if(source.equals(Constants.UPDATE_SYSSTAT_SOURCE_MAIN))
                {
                    MQTTService.sendTopicMessageToAdafruit("prodlineclassifier/feeds/systemstatus", newSystemStatus);
                }
                break;
            default:
                color = Color.parseColor(getString(R.string.color_sysstat_undefined));
                break;
        }

        txtViewSystemStatus.setText(newSystemStatus);
        systemStatus = newSystemStatus;
        bgDrawable.setColor(color);
    }

    @Override
    public void onStart() {
        super.onStart();
    }

    @Override
    public void onRestart() {
        super.onRestart();
    }

    @Override
    public void onPause() {
        super.onPause();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        unregisterReceiver(mqttReceiver);
    }
}