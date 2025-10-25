package mqtt.network;

import mqtt.model.FeedData;
import java.util.List;
import retrofit2.Call;
import retrofit2.http.*;

public interface AdafruitIOService {
    String BASE_URL = "https://io.adafruit.com/api/v2/";

    @GET("{username}/feeds/{feed_key}/data")
    Call<List<FeedData>> getFeedData(
            @Path("username") String username,
            @Path("feed_key") String feedKey,
            @Header("X-AIO-Key") String aioKey
    );

    @POST("{username}/feeds/{feed_key}/data")
    Call<FeedData> sendData(
            @Path("username") String username,
            @Path("feed_key") String feedKey,
            @Header("X-AIO-Key") String aioKey,
            @Body FeedData data
    );
}