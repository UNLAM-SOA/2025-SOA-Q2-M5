package mqtt;

import android.content.Context;
import android.util.Log;

import androidx.annotation.NonNull;

import com.google.gson.Gson;
import com.google.gson.JsonArray;
import com.google.gson.JsonObject;

import info.mqtt.android.service.MqttAndroidClient;
import mqtt.viewmodel.MQTTViewModel;
import okhttp3.Call;
import okhttp3.Callback;
import okhttp3.HttpUrl;
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
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import com.google.gson.JsonArray;
import com.google.gson.JsonParser;
import com.google.gson.JsonElement;
import com.google.gson.reflect.TypeToken;

import java.io.IOException;
import java.lang.reflect.Type;
import java.util.List;
import java.util.ListIterator;
import java.util.concurrent.atomic.AtomicReference;

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

    public static void getTopicRecordCount(@NonNull String username, @NonNull String aioKey, @NonNull String feedKey, String condition, MQTTViewModel mqttViewModel) {
        final int[] count = {0};
        OkHttpClient client = new OkHttpClient();

        // Construir URL con parámetros de filtrado
        HttpUrl url = new HttpUrl.Builder()
                .scheme("https")
                .host("io.adafruit.com")
                .addPathSegment("api")
                .addPathSegment("v2")
                .addPathSegment(username)
                .addPathSegment("feeds")
                .addPathSegment(feedKey)
                .addPathSegment("data")
                .build();

        Request request = new Request.Builder()
                .url(url)
                .addHeader("X-AIO-Key", aioKey)
                .build();

        client.newCall(request).enqueue(new Callback() {
            @Override
            public void onFailure(@NonNull Call call, @NonNull IOException e) {
                Log.e("Adafruit", "Error: " + e.getMessage());
            }

            @Override
            public void onResponse(@NonNull Call call, @NonNull Response response) throws IOException {
                if (response.isSuccessful()) {
                    if(response.body() != null)
                    {
                        String body = response.body().string();
                        try {
                            JSONArray jsonArray = new JSONArray(body);
                            System.out.println("Cantidad de registros obtenidos: " + jsonArray.length());
                            if(!condition.isEmpty()) {
                                JSONObject obj;
                                String value;
                                for(int i = 0; i < jsonArray.length(); i++) {
                                    obj = jsonArray.getJSONObject(i);
                                    value = obj.get("value").toString();
                                    if (value.equalsIgnoreCase(condition)) {
                                        count[0]++;
                                    }
                                }
                                System.out.println("Cant elems para la condición: " + count[0]);
                            }
                            else {
                                count[0] = jsonArray.length();
                                System.out.println("Desde count: " + count[0]);
                            }
                            String mqttMsg = "{\"feed\":\""+feedKey+"\",\"filter\":\""+condition+"\",\"value\":\""+count[0]+"\"}";
                            Log.e("mqqtMANAGER", "json mqttmsg: " + mqttMsg);
                            mqttViewModel.postMessage(Constants.STATISTIC_REQ, mqttMsg);
                        } catch (JSONException e) {
                            throw new RuntimeException(e);
                        }
                    }
                }
            }
        });
    }

    public static void getTopicLastRecordJSON(@NonNull String username, @NonNull String aioKey, @NonNull String feedKey, @NonNull String condition, MQTTViewModel mqttViewModel) {
        OkHttpClient client = new OkHttpClient();

        // Construir URL con parámetros de filtrado
        HttpUrl url = new HttpUrl.Builder()
                .scheme("https")
                .host("io.adafruit.com")
                .addPathSegment("api")
                .addPathSegment("v2")
                .addPathSegment(username)
                .addPathSegment("feeds")
                .addPathSegment(feedKey)
                .addPathSegment("data")
                .build();

        Request request = new Request.Builder()
                .url(url)
                .addHeader("X-AIO-Key", aioKey)
                .build();

        client.newCall(request).enqueue(new Callback() {
            @Override
            public void onFailure(@NonNull Call call, @NonNull IOException e) {
                Log.e("Adafruit", "Error: " + e.getMessage());
            }

            @Override
            public void onResponse(@NonNull Call call, @NonNull Response response) throws IOException {
                if (response.isSuccessful()) {
                    if(response.body() != null)
                    {
                        String body = response.body().string();
                        JsonObject lastJSONData;
                        Gson gson = new Gson();
                        Type listType = new TypeToken<List<JsonObject>>() {}.getType();
                        List<JsonObject> records = gson.fromJson(body, listType);
                        ListIterator<JsonObject> it = records.listIterator(records.size());
                        Log.e("mqqtMANAGER", "json list: " + records.size());
                        while (it.hasPrevious()) {
                            lastJSONData = it.previous();
                            if(lastJSONData.get("value").getAsString().equalsIgnoreCase(condition)) {
                                Log.e("mqqtMANAGER", "json mqttmsg: " + lastJSONData.toString());
                                mqttViewModel.postMessage(Constants.STATISTIC_REQ, lastJSONData.toString());
                                return;
                            }
                        }
                    }
                }
            }
        });
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

    public static void getTopicLatestValue(@NonNull String username, @NonNull String aiokey, String topic, MQTTViewModel mqttViewModel) {
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

    public void disconnect() {
        try {
            client.disconnect();
            Log.d(TAG, "Desconectado de Adafruit IO");
        } catch (Exception e) {
            Log.e(TAG, "Error al desconectarse: " + e.getMessage());
        }
    }
}
