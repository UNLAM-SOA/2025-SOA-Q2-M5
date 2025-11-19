package mqtt;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.app.NotificationCompat;
import androidx.lifecycle.ViewModelProvider;
import androidx.lifecycle.ViewModelStoreOwner;

import com.example.prodlineclassifier.R;
import com.google.gson.Gson;
import com.google.gson.JsonObject;

import info.mqtt.android.service.MqttAndroidClient;
import mqtt.Constants;
import mqtt.viewmodel.MQTTViewModel;
import observer.topicmanager.ConveyorBeltSpeedListener;
import observer.topicmanager.SystemStatusListener;
import observer.topicmanager.TopicPublisher;
import okhttp3.Call;
import okhttp3.Callback;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.Response;

import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken;
import org.eclipse.paho.client.mqttv3.IMqttMessageListener;
import org.eclipse.paho.client.mqttv3.MqttException;

import java.io.IOException;

public class MQTTService extends Service implements MQTTListener {
    private static final String TAG = "MqttService";
    private String username;
    private String AIOKey;
    private static MQTTManager mqttManager;

    public MQTTService() {

    }

    @Override
    public void onCreate() {
        super.onCreate();
        startForegroundServiceWithNotification();
        Log.d(TAG, "Servicio MQTT iniciado");
    }

    private void startForegroundServiceWithNotification() {
        NotificationChannel channel = new NotificationChannel(
                "MQTT_CHANNEL",
                "MQTT Listener",
                NotificationManager.IMPORTANCE_LOW
        );
        NotificationManager manager = getSystemService(NotificationManager.class);
        manager.createNotificationChannel(channel);

        Notification notification = new NotificationCompat.Builder(this, "MQTT_CHANNEL")
                .setContentTitle("Adafruit MQTT conectado")
                .setContentText("Escuchando mensajes en tiempo real")
                .setSmallIcon(R.drawable.ic_launcher_foreground)
                .build();

        startForeground(1, notification);
        Log.d(TAG, "ARRANCA FOREGROUND");
    }

    public static void sendTopicMessageToAdafruit(String topic, String msg) {
        mqttManager.publish(topic, msg);
        Log.d("sendTopicMsgToAda", "[" + topic + "]" + " -> mensaje envidado: " + msg);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.d("OnStartCommand", "try onStartCommand");
        if (intent != null) {
            username = intent.getStringExtra("username");
            AIOKey = intent.getStringExtra("aio_key");

            Log.d(TAG, "Servicio MQTT iniciado con: " + username + " / " + AIOKey);

            mqttManager = MQTTManager.getInstance();
            mqttManager.connect(
                    getApplicationContext(),
                    username,
                    AIOKey,
                    this
            );
        }

        return START_STICKY; // Se reinicia si Android lo cierra
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mqttManager.disconnect();
        Log.d(TAG, "Servicio MQTT detenido");
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        // No usamos binding, es un servicio "autónomo"
        return null;
    }

    // ========================
    // Implementación del listener
    // ========================

    @Override
    public void onConnected() {
        Log.d(TAG, "Conectado al broker MQTT");
        mqttManager.subscribe(username + "/feeds/" + Constants.SYSTEM_STATUS_FEED_KEY);
        mqttManager.subscribe(username + "/feeds/" + Constants.DC_ENGINE_FEED_KEY);
        mqttManager.subscribe(username + "/feeds/" + Constants.DISTANCE_SENSOR_1_FEED_KEY);
        mqttManager.subscribe(username + "/feeds/" + Constants.COLOR_SENSOR_FEED_KEY);
        mqttManager.subscribe(username + "/feeds/" + Constants.DISTANCE_SENSOR_2_FEED_KEY);
        mqttManager.subscribe(username + "/feeds/" + Constants.SERVO_FEED_KEY);
    }

    @Override
    public void onConnectionLost(Throwable cause) {
        Log.e(TAG, "Conexión MQTT perdida");
    }

    @Override
    public void onConnectionError(Throwable exception) {
        Log.e(TAG, "Error al conectar: " + exception.getMessage());
        TopicPublisher.notifyTopic(Constants.CREDENTIALS_ERROR, exception.getMessage());
    }

    @Override
    public void onMessageReceived(String topic, String message) {
        Log.d(TAG, "[Desde MQTTService] Mensaje recibido (" + topic + "): " + message);

        // Enviar el mensaje a la Activity mediante un broadcast
        Intent broadcast = new Intent("MQTT_MESSAGE_RECEIVED");
        broadcast.putExtra("topic", topic);
        broadcast.putExtra("message", message);
        sendBroadcast(broadcast);
    }

    @Override
    public void onDeliveryComplete(IMqttDeliveryToken token) {
        Log.d(TAG, "Entrega completada");
    }
}
