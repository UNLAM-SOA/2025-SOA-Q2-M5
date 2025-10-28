package observer.topicmanager;

import android.util.Log;

import com.example.prodlineclassifier.databinding.ActivitySettingsBinding;

import mqtt.Constants;
import mqtt.viewmodel.MQTTViewModel;
import observer.EventListener;

public class ProductColorListener<String> implements EventListener<String> {
    private final MQTTViewModel viewModel;
    public ProductColorListener() {
        viewModel = null;
    }
    public ProductColorListener(MQTTViewModel viewModelInstance) {
        viewModel = viewModelInstance;
    }
    @Override
    public void update(java.lang.String eventType, String colorDetected) {
        Log.d("SSL", "Recibido desde ProductColorListener: " + eventType + ": " + colorDetected);
        if(viewModel != null) {
            Log.d("SSL", "Posteo a ViewModel desde ProductColorListener");
            viewModel.postMessage(Constants.COLOR_SENSOR_FEED_KEY, (java.lang.String) colorDetected);
        }
    }
}
