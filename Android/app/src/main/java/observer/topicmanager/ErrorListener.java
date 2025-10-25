package observer.topicmanager;

import android.util.Log;

import mqtt.Constants;
import mqtt.viewmodel.MQTTViewModel;
import observer.EventListener;

public class ErrorListener<String> implements EventListener<String> {
    private final MQTTViewModel viewModel;
    public ErrorListener() {
        viewModel = null;
    }
    public ErrorListener(MQTTViewModel viewModelInstance) {
        viewModel = viewModelInstance;
    }
    @Override
    public void update(java.lang.String eventType, String context) {
        Log.d("SSL", "Recibido desde ErrorListener: " + eventType);
        if(viewModel != null) {
            Log.d("SSL", "Posteo a ViewModel desde ErrorListener");
            viewModel.postMessage(Constants.CREDENTIALS_ERROR, (java.lang.String) context);
        }
    }
}
