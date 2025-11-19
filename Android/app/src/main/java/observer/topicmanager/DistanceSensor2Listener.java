package observer.topicmanager;

import android.util.Log;

import mqtt.Constants;
import mqtt.viewmodel.MQTTViewModel;
import observer.EventListener;

public class DistanceSensor2Listener <String> implements EventListener<String> {
    private final MQTTViewModel viewModel;
    public DistanceSensor2Listener() {
        viewModel = null;
    }
    public DistanceSensor2Listener(MQTTViewModel viewModelInstance) {
        viewModel = viewModelInstance;
    }
    @Override
    public void update(java.lang.String eventType, String distanceDetected) {
        Log.d("SSL", "Recibido desde DistanceSensor2Listener: " + eventType + ": " + distanceDetected);
        if(viewModel != null) {
            Log.d("SSL", "Posteo a ViewModel desde DistanceSensor2Listener");
            viewModel.postMessage(Constants.DISTANCE_SENSOR_2_FEED_KEY, (java.lang.String) distanceDetected);
        }
    }
}
