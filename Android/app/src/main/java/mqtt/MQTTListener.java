package mqtt;

import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken;

public interface MQTTListener {
    void onConnected();
    void onConnectionLost(Throwable cause);
    void onConnectionError(Throwable exception);
    void onMessageReceived(String topic, String message);
    void onDeliveryComplete(IMqttDeliveryToken token);
}
