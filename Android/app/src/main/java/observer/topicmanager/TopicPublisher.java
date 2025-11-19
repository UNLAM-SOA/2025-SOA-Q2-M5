package observer.topicmanager;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import androidx.annotation.NonNull;

import mqtt.Constants;
import mqtt.MQTTListener;
import mqtt.viewmodel.MQTTViewModel;
import observer.EventListener;
import observer.EventManager;

public class TopicPublisher {

    public static EventManager events = new EventManager();

    public static void setTopics(String username) {
        events.clearManager();
        events.addEvents(username + "/feeds/distancesensor1",
                username + "/feeds/colorsensor",
                username + "/feeds/distancesensor2",
                username + "/feeds/servo",
                username + "/feeds/dcengine",
                username + "/feeds/systemstatus",
                Constants.STATISTIC_REQ,
                Constants.CREDENTIALS_ERROR);
    }

    public static void subscribeTopic(@NonNull String username, @NonNull String key, @NonNull MQTTViewModel mqttViewModel) {
       String topic = username + "/feeds/" + key;
        switch (key) {
            case Constants.DISTANCE_SENSOR_1_FEED_KEY:
                events.subscribe(topic, new DistanceSensor1Listener<>(mqttViewModel));
                break;
            case Constants.COLOR_SENSOR_FEED_KEY:
                events.subscribe(topic, new ProductColorListener<>(mqttViewModel));
                break;
            case Constants.DISTANCE_SENSOR_2_FEED_KEY:
                events.subscribe(topic, new DistanceSensor2Listener<>(mqttViewModel));
                break;
            case Constants.SERVO_FEED_KEY:
                events.subscribe(topic, new ServoListener<>(mqttViewModel));
                break;
            case Constants.DC_ENGINE_FEED_KEY:
                events.subscribe(topic, new ConveyorBeltSpeedListener<>(mqttViewModel));
                break;
            case Constants.SYSTEM_STATUS_FEED_KEY:
                events.subscribe(topic, new SystemStatusListener<>(mqttViewModel));
                break;
            case Constants.STATISTIC_REQ:
                events.subscribe(topic, new StatisticListener<>(mqttViewModel));
                break;
            case Constants.CREDENTIALS_ERROR:
                events.subscribe(topic, new ErrorListener<>(mqttViewModel));
                break;
        }
    }

    public static void notifyTopic(String topic, String msg){
        if(events == null) {
            return;
        }
        Log.d("TopicPublisher", "NOTIFICO DESDE updateTopicRelatedComponents");
        events.notify(topic, msg);
    }
}
