package observer.topicmanager;

import android.util.Log;

import com.example.prodlineclassifier.databinding.ActivitySettingsBinding;

import mqtt.Constants;
import mqtt.viewmodel.MQTTViewModel;
import observer.EventListener;

public class StatisticListener<String> implements EventListener<String> {
    private final MQTTViewModel viewModel;
    public StatisticListener() {
        viewModel = null;
    }
    public StatisticListener(MQTTViewModel viewModelInstance) {
        viewModel = viewModelInstance;
    }
    @Override
    public void update(java.lang.String eventType, String statData) {
        Log.d("SSL", "Recibido desde StatisticListener: " + eventType + ": " + statData);
        if(viewModel != null) {
            Log.d("SSL", "Posteo a ViewModel desde StatisticListener");
            viewModel.postMessage(Constants.STATISTIC_REQ, (java.lang.String) statData);
        }
    }
}
