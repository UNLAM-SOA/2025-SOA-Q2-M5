package observer.topicmanager;

import android.util.Log;

import mqtt.Constants;
import observer.EventManager;

public class TopicPublisher {
    public static EventManager events = new EventManager("prodlineclassifier/feeds/distancesensor1",
                                                        "prodlineclassifier/feeds/colorsensor",
                                                        "prodlineclassifier/feeds/distancesensor2",
                                                        "prodlineclassifier/feeds/servo",
                                                        "prodlineclassifier/feeds/dcengine",
                                                        "prodlineclassifier/feeds/systemstatus",
                                                        Constants.CREDENTIALS_ERROR);

    public static void updateTopicRelatedComponents(String topic, String msg){
        Log.d("TopicPublisher", "NOTIFICO DESDE updateTopicRelatedComponents");
        events.notify(topic, msg);
    }
}
