package mqtt;

import android.content.Context;
import android.util.Log;

import androidx.annotation.NonNull;

import com.google.gson.Gson;
import com.google.gson.JsonObject;

import info.mqtt.android.service.MqttAndroidClient;
import mqtt.viewmodel.MQTTViewModel;
import okhttp3.Call;
import okhttp3.Callback;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.Response;

import org.eclipse.paho.client.mqttv3.IMqttActionListener;
import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken;
import org.eclipse.paho.client.mqttv3.IMqttToken;
import org.eclipse.paho.client.mqttv3.MqttCallbackExtended;
import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttException;
import org.eclipse.paho.client.mqttv3.MqttMessage;

import java.io.IOException;

public class MQTTManager {
    private static final String TAG = "MqttManager";
    private static MQTTManager instance;
    private MqttAndroidClient client;
    MqttConnectOptions options;

    private MQTTManager() {}

    public static MQTTManager getInstance() {
        if (instance == null) {
            instance = new MQTTManager();
        }
        return instance;
    }

    public void connect(Context context, String username, String aioKey, MQTTListener listener) {
        String serverUri = "tcp://io.adafruit.com:1883";
        String clientId = MqttClient.generateClientId();

        client = new MqttAndroidClient(context, serverUri, clientId);

        options = new MqttConnectOptions();
        options.setUserName(username);
        options.setPassword(aioKey.toCharArray());
        options.setAutomaticReconnect(true);
        options.setCleanSession(true);

        Log.d(TAG, "LLEGA ACÁ 1");

        try {
            Log.d(TAG, "LLEGA ACÁ 2");
            IMqttToken token = client.connect(options);
            token.setActionCallback(new IMqttActionListener() {
                @Override
                public void onSuccess(IMqttToken asyncActionToken) {
                    Log.d(TAG, "Conectado a Adafruit IO");
                    listener.onConnected();
                }

                @Override
                public void onFailure(IMqttToken asyncActionToken, Throwable exception) {
                    Log.e(TAG, "Error al conectar: " + exception.getMessage());
                    listener.onConnectionError(exception);
                }
            });
            Log.d(TAG, "LLEGA ACÁ N");
        } catch (Exception e) {
            Log.e(TAG, "Error al intentar conectarse: " + e.getMessage());
        }

        client.setCallback(new MqttCallbackExtended() {
            @Override
            public void connectComplete(boolean reconnect, String serverURI) {
                Log.d(TAG, "Conectado a Adafruit IO");
                if (listener != null) {
                    listener.onConnected();
                }
            }
            @Override
            public void connectionLost(Throwable cause) {
                Log.e(TAG, "Conexión perdida: " + cause.getMessage());
                if(listener != null) {
                    listener.onConnectionLost(cause);
                }
            }

            @Override
            public void messageArrived(String topic, MqttMessage message) {
                String payload = new String(message.getPayload());
                Log.d(TAG, "[Desde MQTTManager] Mensaje recibido: [" + topic + "]: " + payload);
                if(listener != null) {
                    listener.onMessageReceived(topic, payload);
                }
            }

            @Override
            public void deliveryComplete(IMqttDeliveryToken token) {
                listener.onDeliveryComplete(token);
                Log.d(TAG, "Mensaje entregado con éxito");
            }
        });
    }

    public void subscribe(String topic) {
        try {
            client.subscribe(topic, 1, null, new IMqttActionListener() {
                @Override
                public void onSuccess(IMqttToken asyncActionToken) {
                    Log.d(TAG, "Suscripción exitosa al tópico: " + topic);
                }

                @Override
                public void onFailure(IMqttToken asyncActionToken, Throwable exception) {
                    Log.e(TAG, "Error al suscribirse: " + exception.getMessage());
                }
            });
        } catch (Exception e) {
            Log.e(TAG, "Error al suscribirse: " + e.getMessage());
        }
    }

    public void publish(String topic, String message) {
        try {
            MqttMessage mqttMessage = new MqttMessage();
            mqttMessage.setPayload(message.getBytes());
            client.publish(topic, mqttMessage);
            Log.d(TAG, "Publicado en " + topic + ": " + message);
        } catch (Exception e) {
            Log.e(TAG, "Error al publicar: " + e.getMessage());
        }
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

    public static void getTopicLatestValue(String username, String aiokey, String topic, MQTTViewModel mqttViewModel) {
        getLastFeedValue(username,
                topic,
                aiokey,
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
                                if(topic != null && lastValue != null) {
                                    mqttViewModel.postMessage(topic, lastValue);
                                }
                            }
                        }
                    }
                });
    }

    public MqttAndroidClient getClient() {
        return client;
    }

    public void disconnect() {
        try {
            client.disconnect();
            Log.d(TAG, "Desconectado de Adafruit IO");
        } catch (Exception e) {
            Log.e(TAG, "Error al desconectarse: " + e.getMessage());
        }
    }
}
