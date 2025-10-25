package observer.topicmanager;

import android.util.Log;

import com.example.prodlineclassifier.databinding.ActivitySettingsBinding;

import mqtt.Constants;
import mqtt.viewmodel.MQTTViewModel;
import observer.EventListener;

public class SystemStatusListener<String> implements EventListener<String> {
    private final MQTTViewModel viewModel;
    public SystemStatusListener() {
        viewModel = null;
    }
    public SystemStatusListener(MQTTViewModel viewModelInstance) {
        viewModel = viewModelInstance;
    }
    @Override
    public void update(java.lang.String eventType, String newSysStat) {
        Log.d("SSL", "Recibido desde SystemStatusListener: " + eventType + ": " + newSysStat);
        if(viewModel != null) {
            Log.d("SSL", "Posteo a ViewModel desde SystemStatusListener");
            viewModel.postMessage(Constants.SYSTEM_STATUS_FEED_KEY, (java.lang.String) newSysStat);
        }
    }
}
