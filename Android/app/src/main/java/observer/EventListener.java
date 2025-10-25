package observer;

public interface EventListener<T> {
    void update(String eventType, T context);
}
