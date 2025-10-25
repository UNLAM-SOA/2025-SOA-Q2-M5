package observer;

import android.util.Log;


import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class EventManager {
    Map<String, List<EventListener<Object>>> listeners = new HashMap<>();

    public EventManager(String... eventTypes) {
        for (String eventType : eventTypes) {
            this.listeners.put(eventType, new ArrayList<>());
        }
    }

    public void subscribe(String eventType, EventListener<Object> listener) {
        List<EventListener<Object>> users = listeners.get(eventType);
        if(users != null)
        {
            users.add(listener);
            Log.d("EventManager", "SUSCRIBO TÓPICO");
        }
    }

    public void unsubscribe(String eventType, EventListener<Object> listener) {
        List<EventListener<Object>> users = listeners.get(eventType);
        if(users != null)
        {
            users.remove(listener);
        }
    }

    public void notify(String eventType, Object data) {
        List<EventListener<Object>> users = listeners.get(eventType);
        if(users != null)
        {
            for (EventListener<Object> listener : users) {
                listener.update(eventType, data);
                Log.d("EventManager", "NOTIFICO TÓPICO");
            }
        }
    }
}
