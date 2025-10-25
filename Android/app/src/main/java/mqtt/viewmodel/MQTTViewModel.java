package mqtt.viewmodel;

import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.ViewModel;

public class MQTTViewModel extends ViewModel {
    private final MutableLiveData<MqttMessage> mqttMessage = new MutableLiveData<>();

    public LiveData<MqttMessage> getMqttMessage() {
        return mqttMessage;
    }

    public void postMessage(String topic, String payload) {
        mqttMessage.postValue(new MqttMessage(topic, payload));
    }

    public static class MqttMessage {
        public final String topic;
        public final String message;
        public MqttMessage(String topicToSend, String messageToSend) {
            topic = topicToSend;
            message = messageToSend;
        }
    }
}
