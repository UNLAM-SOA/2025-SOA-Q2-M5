package com.example.prodlineclassifier;

import android.content.Context;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.EdgeToEdge;
import androidx.annotation.RequiresApi;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.lifecycle.ViewModelProvider;

import com.google.gson.Gson;
import com.google.gson.JsonObject;

import java.util.concurrent.CompletableFuture;

import mqtt.Constants;
import mqtt.MQTTBroadcastReceiver;
import mqtt.MQTTManager;
import mqtt.MQTTService;
import mqtt.viewmodel.MQTTViewModel;
import observer.topicmanager.TopicPublisher;

public class ProcessLogActivity extends AppCompatActivity {
    private MQTTViewModel mqttViewModel;
    private MQTTBroadcastReceiver mqttReceiver;
    ImageButton btnBackSettings;
    private String username;
    private String aioKey;
    private TextView txtValueEmergencyStopped;
    private TextView txtValueManualStopped;
    private TextView txtValueRedProds;
    private TextView txtValueBlueProds;
    private TextView txtValueUnknownProds;
    private TextView txtValueLastEmergencyStoppedDate;

    @RequiresApi(api = Build.VERSION_CODES.TIRAMISU)
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_process_log);

        SharedPreferences prefsSession = getSharedPreferences("AdafruitPrefs", MODE_PRIVATE);
        username = prefsSession.getString("username", null);
        aioKey = prefsSession.getString("aioKey", null);

        if (username == null || aioKey == null) {
            Toast.makeText(this, "Session expired", Toast.LENGTH_SHORT).show();
            finish();
        }

        mqttViewModel = new ViewModelProvider(this).get(MQTTViewModel.class);
        setViewModelResponses();

        mqttReceiver = new MQTTBroadcastReceiver();
        registerReceiver(mqttReceiver,
                new IntentFilter("MQTT_MESSAGE_RECEIVED"),
                Context.RECEIVER_EXPORTED);

        TopicPublisher.subscribeTopic(username, Constants.COLOR_SENSOR_FEED_KEY, mqttViewModel);
        TopicPublisher.subscribeTopic(username, Constants.SYSTEM_STATUS_FEED_KEY, mqttViewModel);
        TopicPublisher.subscribeTopic(username, Constants.CREDENTIALS_ERROR, mqttViewModel);
        TopicPublisher.subscribeTopic(username, Constants.STATISTIC_REQ, mqttViewModel);

        btnBackSettings = findViewById(R.id.btn_back_process_log);

        btnBackSettings.setOnClickListener(v -> {
            finish(); // Cierra la Activity y vuelve al Main
        });

        txtValueEmergencyStopped = findViewById(R.id.txt_value_emergency_stops);
        txtValueManualStopped = findViewById(R.id.txt_value_manual_stops);
        txtValueBlueProds = findViewById(R.id.txt_value_amount_blue_prods);
        txtValueRedProds = findViewById(R.id.txt_value_amount_red_prods);
        txtValueUnknownProds = findViewById(R.id.txt_value_amount_unknown_prods);
        txtValueLastEmergencyStoppedDate = findViewById(R.id.txt_value_last_emergency_stop_date);

        getStatisticsValue();

                ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main), (v, insets) -> {
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom);
            return insets;
        });
    }

    private void setViewModelResponses() {
        mqttViewModel.getMqttMessage().observe(this, mqttMsg -> {
            Log.d("ProcessLogActivity", "Recibido ViewModel desde ProcessLogActivity");
            Log.d("ProcessLogActivity", "Topic:   " + mqttMsg.topic + "  Message: " + mqttMsg.message);
            if (mqttMsg.topic.equals(Constants.CREDENTIALS_ERROR)) {
                Toast.makeText(this, "Invalid credentials, please try again", Toast.LENGTH_SHORT).show();
                finish();
            }
            if (mqttMsg.topic.equals(Constants.STATISTIC_REQ)) {
                String value;
                Gson gson = new Gson();
                JsonObject jsonObject = gson.fromJson(mqttMsg.message, JsonObject.class);
                value = jsonObject.get("value").getAsString();
                if (jsonObject.has("filter")) {
                    String feed;
                    String filter;
                    feed = jsonObject.get("feed").getAsString();
                    filter = jsonObject.get("filter").getAsString();
                    Log.d("Adafruit", "a getTopicRecordCount:  " + feed + " - " + filter + " - " + value);
                    CompletableFuture.runAsync(() -> updatesStatisticValue(feed, filter, value));
                }
                else {
                    String date = jsonObject.get("created_at").getAsString(); // ISO 8601
                    updateStatisticDate(value, date);
                }
            }
        });
    }

    private void getStatisticsValue() {
        MQTTManager.getTopicRecordCount(username, aioKey, Constants.SYSTEM_STATUS_FEED_KEY, "Emergency Stopped", mqttViewModel);
        MQTTManager.getTopicRecordCount(username, aioKey, Constants.SYSTEM_STATUS_FEED_KEY, "Manually Stopped", mqttViewModel);
        MQTTManager.getTopicRecordCount(username, aioKey, Constants.COLOR_SENSOR_FEED_KEY, "Blue", mqttViewModel);
        MQTTManager.getTopicRecordCount(username, aioKey, Constants.COLOR_SENSOR_FEED_KEY, "Red", mqttViewModel);
        MQTTManager.getTopicRecordCount(username, aioKey, Constants.COLOR_SENSOR_FEED_KEY, "Unknown", mqttViewModel);
        MQTTManager.getTopicLastRecordJSON(username, aioKey, Constants.SYSTEM_STATUS_FEED_KEY, "Emergency Stopped", mqttViewModel);
    }

    private void updatesStatisticValue(String feed, String filter, String value) {

        Log.d("ProcessLogActivity", "feed value:   " + value);
        switch (feed) {
            case Constants.SYSTEM_STATUS_FEED_KEY:
                switch (filter) {
                    case "Emergency Stopped":
                        txtValueEmergencyStopped.setText(value);
                        break;
                    case  "Manually Stopped":
                        txtValueManualStopped.setText(value);
                        break;
                }
                break;
            case Constants.COLOR_SENSOR_FEED_KEY:
                switch (filter) {
                    case "Blue":
                        txtValueBlueProds.setText(value);
                        break;
                    case "Red":
                        txtValueRedProds.setText(value);
                        break;
                    case "Unknown":
                        txtValueUnknownProds.setText(value);
                }
                break;
        }
    }

    private void updateStatisticDate(String statistic, String date) {
        if(statistic.equals("Emergency Stopped")) {
            txtValueLastEmergencyStoppedDate.setText(date);
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        unregisterReceiver(mqttReceiver);
    }
}