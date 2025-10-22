package com.example.prodlineclassifier;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.util.Log;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.EdgeToEdge;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

public class MainActivity extends AppCompatActivity {

    public Button btnSettingsMain;
    public Button btnStartMain;
    public Button btnStopMain;
    public Button btnProcessLogMain;
    public TextView txtViewSystemStatus;
    public String systemStatus;

    public MainBroadcastReceiver mainBroadcastReceiver;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_main);

        systemStatus = getString(R.string.txt_view_status);

        txtViewSystemStatus = findViewById(R.id.txt_view_systemstatus_main);

        btnSettingsMain = findViewById(R.id.btn_settings_main);

        btnSettingsMain.setOnClickListener(v -> {
            // Intent para abrir Settings.java
            Intent intent = new Intent(MainActivity.this, Settings.class);
            startActivity(intent);
        });

        btnProcessLogMain = findViewById(R.id.btn_process_log_main);

        btnProcessLogMain.setOnClickListener(v -> {
            // Intent para abrir Settings.java
            Intent intent = new Intent(MainActivity.this, ProcessLog.class);
            startActivity(intent);
        });

        btnStartMain = findViewById(R.id.btn_start_main);

        btnStartMain.setOnClickListener(v -> {
            updateSysStatus(getString(R.string.info_sysstat_start_main));
        });

        btnStopMain = findViewById(R.id.btn_stop_main);

        btnStopMain.setOnClickListener(v -> {
            updateSysStatus(getString(R.string.info_sysstat_stop_main));
        });

        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main), (v, insets) -> {
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom);
            return insets;
        });
    }

    private void updateSysStatus(String newSystemStatus) {
        txtViewSystemStatus = findViewById(R.id.txt_view_systemstatus_main);

        GradientDrawable bgDrawable = (GradientDrawable) txtViewSystemStatus.getBackground();
        int color;

        switch (newSystemStatus) {
            case "Running":
                if(systemStatus.equals("Running")) {
                    Toast.makeText(this, "System is already running", Toast.LENGTH_SHORT).show();
                    return;
                }
                color = Color.parseColor(getString(R.string.color_sysstat_running));
                break;
            case "Manually Stopped":
                if(!systemStatus.equals("Running")) {
                    Toast.makeText(this, "System isn't running right now", Toast.LENGTH_SHORT).show();
                    return;
                }
                color = Color.parseColor(getString(R.string.color_sysstat_manually_stopped));
                break;
            default:
                color = Color.parseColor(getString(R.string.color_sysstat_undefined));
                break;
        }

        txtViewSystemStatus.setText(newSystemStatus);
        systemStatus = newSystemStatus;
        bgDrawable.setColor(color);
    }

    public static class MainBroadcastReceiver extends BroadcastReceiver {

        @Override
        public void onReceive(Context context, Intent intent) {
            StringBuilder sb = new StringBuilder();
            sb.append("Action: ").append(intent.getAction()).append("\n");
            String log = sb.toString();
            // String year = Objects.requireNonNull(intent.getExtras()).getString("year");
            // setYearInTextView(year);
            Log.i("MainActivity", log);
        }

    }

    /* @SuppressLint("SetTextI18N")
    public void setYearInTextView() {

    } */

    @Override
    public void onStart() {
        super.onStart();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
    }

    @Override
    public void onRestart() {
        super.onRestart();
    }

    @Override
    public void onPause() {
        super.onPause();
    }

}