package mqtt;

import android.content.Context;
import android.util.Log;

import info.mqtt.android.service.MqttAndroidClient;
import org.eclipse.paho.client.mqttv3.IMqttActionListener;
import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken;
import org.eclipse.paho.client.mqttv3.IMqttToken;
import org.eclipse.paho.client.mqttv3.MqttCallbackExtended;
import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttException;
import org.eclipse.paho.client.mqttv3.MqttMessage;

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
