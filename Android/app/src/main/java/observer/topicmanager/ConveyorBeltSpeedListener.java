package observer.topicmanager;

import android.util.Log;

import mqtt.Constants;
import mqtt.viewmodel.MQTTViewModel;
import observer.EventListener;

public class ConveyorBeltSpeedListener<String> implements EventListener<String> {
    private final MQTTViewModel viewModel;
    public ConveyorBeltSpeedListener() {
        viewModel = null;
    }
    public ConveyorBeltSpeedListener(MQTTViewModel viewModelInstance) {
        viewModel = viewModelInstance;
    }
    @Override
    public void update(java.lang.String eventType, String newCurrentSpeed) {
        Log.d("CBSL", "Recibido desde ConvBeltSpeedList: " + eventType + ": " + newCurrentSpeed);
        if(viewModel != null) {
            viewModel.postMessage(Constants.DC_ENGINE_FEED_KEY, (java.lang.String) newCurrentSpeed);
        }
    }
}
