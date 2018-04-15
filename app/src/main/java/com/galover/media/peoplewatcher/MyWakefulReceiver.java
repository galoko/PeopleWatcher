package com.galover.media.peoplewatcher;

import android.content.Context;
import android.content.Intent;
import android.support.v4.content.WakefulBroadcastReceiver;
import android.util.Log;

public class MyWakefulReceiver extends WakefulBroadcastReceiver {

    private static final String WAKELOCK_TAG = "PW_WAKELOCK";

    @Override
    public void onReceive(Context context, Intent intent) {

        // This is the Intent to deliver to our service.
        Intent service = new Intent(context, MainService.class);

        startWakefulService(context, service);
    }
}