package com.example.prodlineclassifier;

import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
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

import java.util.concurrent.atomic.AtomicReference;

import mqtt.Constants;
import mqtt.MQTTBroadcastReceiver;
import mqtt.MQTTManager;
import mqtt.MQTTService;
import mqtt.viewmodel.MQTTViewModel;
import observer.topicmanager.ErrorListener;
import observer.topicmanager.SystemStatusListener;
import observer.topicmanager.TopicPublisher;

public class MainActivity extends AppCompatActivity implements SensorEventListener {
    private MQTTViewModel mqttViewModel;
    public Button btnSettingsMain;
    public Button btnStartMain;
    public Button btnStopMain;
    public Button btnProcessLogMain;
    public Button btnLogOut;
    public TextView txtViewSystemStatus;
    public String systemStatus;
    private MQTTBroadcastReceiver mqttReceiver;
    String username = null;
    String aioKey = null;
    // Variables para el Sensor
    private SensorManager mSensorManager;
    private Sensor mAccelerometer;
    // Constantes y variables para la detección de shake
    private static final float SHAKE_THRESHOLD_GRAVITY = 8.0F; // Umbral de detección (ajustable)
    private static final int SHAKE_SLOP_TIME_MS = 500; // Tiempo mínimo entre detecciones
    private static final int SHAKE_COUNT_RESET_TIME_MS = 3000; // Tiempo para resetear el contador de sacudidas
    private long mShakeTimestamp; // Marca de tiempo del último evento
    private int mShakeCount; // Contador de sacudidas

    @RequiresApi(api = Build.VERSION_CODES.TIRAMISU)
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_main);

        //SharedPreferences prefsSession = getSharedPreferences("AdafruitPrefs", MODE_PRIVATE);
        //String username = prefsSession.getString("username", null);
        //String aioKey = prefsSession.getString("aioKey", null);

        sessionConfig();

        // Registrar receiver para recibir mensajes del servicio
        mqttReceiver = new MQTTBroadcastReceiver();
        IntentFilter filter = new IntentFilter();
        filter.addAction("MQTT_MESSAGE_RECEIVED");
        registerReceiver(mqttReceiver,
                         filter,
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

        btnLogOut = findViewById(R.id.btn_log_out_main);

        btnLogOut.setOnClickListener(v -> {
            // Intent para abrir Settings.java
            closeSession();
            sessionConfig();
        });

        // Inicializar el SensorManager y el Acelerómetro
        mSensorManager = (SensorManager) getSystemService(SENSOR_SERVICE);
        mAccelerometer = mSensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);

        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main), (v, insets) -> {
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom);
            return insets;
        });
    }

    private void requestCredentials(CredentialsCallback callback) {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        View view = LayoutInflater.from(this).inflate(R.layout.dialog_adafruit_login, null);

        EditText etUser = view.findViewById(R.id.edt_username_login);
        EditText etKey = view.findViewById(R.id.edt_aiokey_login);
        Button btnConnect = view.findViewById(R.id.btn_connect_login);

        builder.setView(view);
        builder.setCancelable(false); // no se puede cerrar sin ingresar datos

        AlertDialog dialog = builder.create();
        dialog.show();

        btnConnect.setOnClickListener(v -> {
            String username = etUser.getText().toString().trim();
            String AIOkey = etKey.getText().toString().trim();

            if (username.isEmpty() || AIOkey.isEmpty()) {
                Toast.makeText(this, "Please, fill all the credential fields", Toast.LENGTH_SHORT).show();
                return;
            }

            dialog.dismiss();
            callback.onCredentialsRegistered(username, AIOkey);
        });
    }

    interface CredentialsCallback {
        void onCredentialsRegistered(String username, String AIOKey);
    }

    private void sessionConfig() {
        requestCredentials((usernameSession, aioKeySession) -> {
            SharedPreferences.Editor editor = getSharedPreferences("AdafruitPrefs", MODE_PRIVATE).edit();
            editor.putString("username", usernameSession);
            editor.putString("aioKey", aioKeySession);
            editor.apply();

            username = usernameSession;
            aioKey = aioKeySession;

            Log.d("Main", "user " + usernameSession);
            Log.d("Main", "aiok " + aioKeySession);
            // Iniciar el servicio MQTT
            // Intent serviceIntent = new Intent(this, MQTTService.class);
            // startService(serviceIntent);
            Intent serviceIntent = new Intent(this, MQTTService.class);
            serviceIntent.putExtra("username", usernameSession);
            serviceIntent.putExtra("aio_key", aioKeySession);
            startForegroundService(serviceIntent);

            mqttViewModel = new ViewModelProvider(this).get(MQTTViewModel.class);
            setViewModelResponses();

            MQTTManager.getTopicLatestValue(usernameSession, aioKeySession, Constants.SYSTEM_STATUS_FEED_KEY, mqttViewModel);

            TopicPublisher.setTopics(username);

            TopicPublisher.subscribeTopic(username, Constants.SYSTEM_STATUS_FEED_KEY, mqttViewModel);
            TopicPublisher.subscribeTopic(username, Constants.CREDENTIALS_ERROR, mqttViewModel);
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
            if (mqttMsg.topic.equals(Constants.CREDENTIALS_ERROR)) {
                Toast.makeText(this, "Invalid credentials, please try again", Toast.LENGTH_SHORT).show();
                sessionConfig();
            }
        });
    }

    private void closeSession() {
        SharedPreferences prefs = getSharedPreferences("AdafruitPrefs", Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = prefs.edit();
        editor.clear();
        editor.apply();
        Toast.makeText(this, "Session closed", Toast.LENGTH_SHORT).show();
    }

    private void updateSysStatus(String newSystemStatus, String source) {
        txtViewSystemStatus = findViewById(R.id.txt_view_systemstatus_main);
        GradientDrawable bgDrawable = (GradientDrawable) txtViewSystemStatus.getBackground();
        int color;

        switch (newSystemStatus) {
            case "ST_IDLE":
                if(!systemStatus.equals("ST_MANUAL_STOP")
                    && source.equals(Constants.UPDATE_SYSSTAT_SOURCE_MAIN)) {

                    Toast.makeText(this, "System is already running", Toast.LENGTH_SHORT).show();

                    return;
                }
                /*
                if(!systemStatus.equals("ST_MANUAL_STOP")
                    && !systemStatus.equals("Emergency Stopped")
                    && source.equals(Constants.UPDATE_SYSSTAT_SOURCE_MAIN))
                {
                    Toast.makeText(this, "System is offline", Toast.LENGTH_SHORT).show();
                    return;
                }
                */
                color = Color.parseColor(getString(R.string.color_sysstat_idle));
                if(source.equals(Constants.UPDATE_SYSSTAT_SOURCE_MAIN))
                {
                    MQTTService.sendTopicMessageToAdafruit(username + "/feeds/systemstatus", newSystemStatus);
                }
                txtViewSystemStatus.setTextColor(Color.parseColor("#000000"));
                break;
            case "ST_COLOR_DETECTED":
                color = Color.parseColor(getString(R.string.color_sysstat_running));
                txtViewSystemStatus.setTextColor(Color.parseColor("#000000"));
                break;
            case "ST_MANUAL_STOP":
                if(systemStatus.equals("ST_MANUAL_STOP")
                   && source.equals(Constants.UPDATE_SYSSTAT_SOURCE_MAIN)) {
                    Toast.makeText(this, "System isn't running right now", Toast.LENGTH_SHORT).show();
                    return;
                }
                color = Color.parseColor(getString(R.string.color_sysstat_manually_stopped));
                if(source.equals(Constants.UPDATE_SYSSTAT_SOURCE_MAIN))
                {
                    MQTTService.sendTopicMessageToAdafruit( username + "/feeds/systemstatus", newSystemStatus);
                }
                txtViewSystemStatus.setTextColor(Color.parseColor("#000000"));
                break;
            case "ST_ERROR":
                color = Color.parseColor(getString(R.string.color_sysstat_emergency_stopped));
                txtViewSystemStatus.setTextColor(Color.parseColor("#FFFFFF"));
                break;
            default:
                color = Color.parseColor(getString(R.string.color_sysstat_undefined));
                txtViewSystemStatus.setTextColor(Color.parseColor("#000000"));
                break;
        }

        txtViewSystemStatus.setText(newSystemStatus);
        systemStatus = newSystemStatus;
        bgDrawable.setColor(color);
    }

    /**
     * Método a ejecutar cuando se detecta una sacudida.
     * @param count El número de sacudidas detectadas en la ráfaga actual.
     */
    private void handleShakeEvent(int count) {
        // Para que solo se ejecute una vez por ráfaga, si count == 1
        if(count == 1)
        {
            String message = "Restarting the system...";
            android.widget.Toast.makeText(this, message, android.widget.Toast.LENGTH_SHORT).show();
            updateSysStatus(getString(R.string.info_sysstat_start_main), Constants.UPDATE_SYSSTAT_SOURCE_MAIN);
        }
    }

    @Override
    public void onSensorChanged(SensorEvent event) {
        if (mAccelerometer != null) {
            float x = event.values[0];
            float y = event.values[1];
            float z = event.values[2];

            // Calcular la fuerza de la aceleración (usando la norma vectorial)
            float gForce = (float) Math.sqrt(x * x + y * y + z * z) - SensorManager.GRAVITY_EARTH;

            if (gForce > SHAKE_THRESHOLD_GRAVITY) {
                final long now = System.currentTimeMillis();

                // Ignorar sacudidas muy cercanas en el tiempo
                if (mShakeTimestamp + SHAKE_SLOP_TIME_MS > now) {
                    return;
                }

                // Resetear el contador si ha pasado mucho tiempo desde la última sacudida
                if (mShakeTimestamp + SHAKE_COUNT_RESET_TIME_MS < now) {
                    mShakeCount = 0;
                }

                mShakeTimestamp = now;
                mShakeCount++;

                handleShakeEvent(mShakeCount);
            }
        }
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
        // No es necesario implementar lógica para la detección de shakes
    }

    @Override
    protected void onResume() {
        super.onResume();
        // Registrar el listener cuando la actividad está visible
        if (mAccelerometer != null) {
            mSensorManager.registerListener(this, mAccelerometer, SensorManager.SENSOR_DELAY_UI);
        }
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

        // Desregistrar el listener para ahorrar batería cuando la actividad no está en primer plano
        if (mAccelerometer != null) {
            mSensorManager.unregisterListener(this);
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        unregisterReceiver(mqttReceiver);
        if (mSensorManager != null) {
            mSensorManager.unregisterListener(this);
        }
    }
}