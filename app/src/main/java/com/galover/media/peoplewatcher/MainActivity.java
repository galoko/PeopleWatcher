package com.galover.media.peoplewatcher;

import android.Manifest;
import android.app.Activity;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.content.pm.PackageManager;
import android.util.Log;
import android.content.Intent;
import android.support.v4.content.WakefulBroadcastReceiver;
import android.content.Context;
import android.app.PendingIntent;

public class MainActivity extends Activity {

    private static final String ACTIVITY_TAG = "PW_ACTIVITY";

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("engine");
    }

    void startMainForegroundService() {

        sendBroadcast(new Intent(this, MyWakefulReceiver.class));
    }

    @Override
    public void onStart() {
        super.onStart();
        Log.d(ACTIVITY_TAG, "Activity started");
    }

    @Override
    public void onStop() {
        super.onStop();
        Log.d(ACTIVITY_TAG, "Activity stopped");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        requestNeededPermissinos();
    }

    private void requestNeededPermissinos() {
        requestPermissions(new String[]
                {
                        Manifest.permission.CAMERA,
                        Manifest.permission.INTERNET,
                        Manifest.permission.ACCESS_NETWORK_STATE,
                        Manifest.permission.WRITE_EXTERNAL_STORAGE,
                        Manifest.permission.READ_EXTERNAL_STORAGE,
                        Manifest.permission.WAKE_LOCK
                },1);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {

        for (int grantResult : grantResults) {
            if (grantResult != PackageManager.PERMISSION_GRANTED) {
                // FIXME this exception will occur before we setup exception handler in Service
                throw new Error("Permissions denied");
            }
        }

        startMainForegroundService();
    }
}
