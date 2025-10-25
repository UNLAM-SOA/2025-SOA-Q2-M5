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
    private static MQTTManager mqttManager;

    public MQTTService() {

    }
    @Override
    public void onCreate() {
        super.onCreate();
        startForegroundServiceWithNotification();
        Log.d(TAG, "Servicio MQTT iniciado");
        mqttManager = MQTTManager.getInstance();
        Log.d(TAG, "ANTES DE CONEC");
        // Conectarse a Adafruit IO
        mqttManager.connect(getApplicationContext(),
                Constants.ADAFRUIT_USERNAME,
                Constants.ADAFRUIT_AIO_KEY,
                this);
        Log.d(TAG, "TERMINA ESTO");
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

    public static void getLastFeedValue(String username, String feedKey, String aioKey, Callback callback) {
        OkHttpClient client = new OkHttpClient();
        String url = "https://io.adafruit.com/api/v2/" + username + "/feeds/" + feedKey + "/data/last";

        Request request = new Request.Builder()
                .url(url)
                .addHeader("X-AIO-Key", aioKey)
                .build();

        client.newCall(request).enqueue(callback);
    }

    public static void getTopicLatestValue(String topic, MQTTViewModel mqttViewModel) {
        MQTTService.getLastFeedValue(Constants.ADAFRUIT_USERNAME,
                topic,
                Constants.ADAFRUIT_AIO_KEY,
                new Callback() {
                    @Override
                    public void onFailure(@NonNull Call call, @NonNull IOException e) {
                        Log.e("Adafruit", "Error: " + e.getMessage());
                    }

                    @Override
                    public void onResponse(@NonNull Call call, @NonNull Response response) throws IOException {
                        if (response.isSuccessful()) {
                            if(response.body() != null)
                            {
                                String lastValue;
                                String body = response.body().string();
                                Gson gson = new Gson();
                                JsonObject jsonObject = gson.fromJson(body, JsonObject.class);
                                lastValue = jsonObject.get("value").getAsString();
                                Log.d("Adafruit", "Último valor de " + topic + ": " + lastValue);
                                Log.d("Adafruit", "json " + body);
                                mqttViewModel.postMessage(topic, lastValue);
                            }
                        }
                    }
                });
    }

    public static void sendTopicMessageToAdafruit(String topic, String msg) {
        mqttManager.publish(topic, msg);
        Log.d("sendTopicMsgToAda", "[" + topic + "]" + " -> mensaje envidado: " + msg);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.d(TAG, "Servicio MQTT en ejecución");
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
        mqttManager.subscribe(Constants.ADAFRUIT_USERNAME + "/feeds/" + Constants.SYSTEM_STATUS_FEED_KEY);
        mqttManager.subscribe(Constants.ADAFRUIT_USERNAME + "/feeds/" + Constants.DC_ENGINE_FEED_KEY);
    }

    @Override
    public void onConnectionLost(Throwable cause) {
        Log.e(TAG, "Conexión MQTT perdida");
    }

    @Override
    public void onConnectionError(Throwable exception) {
        Log.e(TAG, "Error al conectar: " + exception.getMessage());
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
