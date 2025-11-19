package observer.topicmanager;

import android.util.Log;

import mqtt.Constants;
import mqtt.viewmodel.MQTTViewModel;
import observer.EventListener;

public class DistanceSensor1Listener <String> implements EventListener<String> {
    private final MQTTViewModel viewModel;
    public DistanceSensor1Listener() {
        viewModel = null;
    }
    public DistanceSensor1Listener(MQTTViewModel viewModelInstance) {
        viewModel = viewModelInstance;
    }
    @Override
    public void update(java.lang.String eventType, String distanceDetected) {
        Log.d("SSL", "Recibido desde DistanceSensor1Listener: " + eventType + ": " + distanceDetected);
        if(viewModel != null) {
            Log.d("SSL", "Posteo a ViewModel desde DistanceSensor1Listener");
            viewModel.postMessage(Constants.DISTANCE_SENSOR_1_FEED_KEY, (java.lang.String) distanceDetected);
        }
    }
}
