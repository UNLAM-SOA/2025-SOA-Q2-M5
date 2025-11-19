package observer.topicmanager;

import android.util.Log;

import mqtt.Constants;
import mqtt.viewmodel.MQTTViewModel;
import observer.EventListener;

public class ServoListener <String> implements EventListener<String> {
    private final MQTTViewModel viewModel;
    public ServoListener() {
        viewModel = null;
    }
    public ServoListener(MQTTViewModel viewModelInstance) {
        viewModel = viewModelInstance;
    }
    @Override
    public void update(java.lang.String eventType, String position) {
        Log.d("SSL", "Recibido desde ServoListener: " + eventType + ": " + position);
        if(viewModel != null) {
            Log.d("SSL", "Posteo a ViewModel desde ServoListener");
            viewModel.postMessage(Constants.SERVO_FEED_KEY, (java.lang.String) position);
        }
    }
}
