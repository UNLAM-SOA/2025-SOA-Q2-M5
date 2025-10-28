package mqtt;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

import com.example.prodlineclassifier.databinding.ActivitySettingsBinding;

import observer.EventListener;
import observer.topicmanager.ConveyorBeltSpeedListener;
import observer.topicmanager.SystemStatusListener;
import observer.topicmanager.TopicPublisher;

public class MQTTBroadcastReceiver extends BroadcastReceiver {

    public MQTTBroadcastReceiver() {

    }

    @Override
    public void onReceive(Context context, Intent intent) {
        Log.d("MqttBroadcastReceiver", "[Mensaje desde MQTTBroadcastReceiver]");
        if ("MQTT_MESSAGE_RECEIVED".equals(intent.getAction())) {
            String topic = intent.getStringExtra("topic");
            String message = intent.getStringExtra("message");
            Log.d("MqttBroadcastReceiver", "Mensaje recibido desde MQTTBroadcastReceiver: " + message + " del tópico: " + topic);
            // Acá se puede actualizar la UI o mostrar una notificación
            TopicPublisher.notifyTopic(topic, message);
        }
    }
}
