package mqtt.model;

import androidx.annotation.NonNull;

public class FeedData {
    private String value;

    public FeedData(String newValue) {
        value = newValue;
    }

    public String getValue() {
        return value;
    }

    public void setValue(String newValue) {
        value = newValue;
    }
}
