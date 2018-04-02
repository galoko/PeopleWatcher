package com.galover.media.peoplewatcher;

import android.Manifest;
import android.app.Activity;
import android.os.Bundle;
import android.os.Environment;
import android.support.annotation.NonNull;
import android.content.pm.PackageManager;
import android.util.Log;
import java.io.*;
import android.media.MediaScannerConnection;
import android.media.*;;

public class MainActivity extends Activity {

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("engine");
    }

    private Thread.UncaughtExceptionHandler nextHandler;

    private MyCameraManager cameraManager;

    void startup() {

        EngineManager.initialize();

        cameraManager = new MyCameraManager(this);

        if (!cameraManager.openBackRearCamera())
            throw new Error("Cannot open back rear camera");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        setupExceptionHandler();

        requestNeededPermissinos();
    }

    private void requestNeededPermissinos() {
        requestPermissions(new String[]
                {
                        Manifest.permission.CAMERA,
                        Manifest.permission.INTERNET,
                        Manifest.permission.ACCESS_NETWORK_STATE,
                        Manifest.permission.WRITE_EXTERNAL_STORAGE,
                        Manifest.permission.READ_EXTERNAL_STORAGE
                },1);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {

        for (int grantResult : grantResults) {
            if (grantResult != PackageManager.PERMISSION_GRANTED)
                finish();
        }

        startup();
    }

    private File getReportsDir() {

        return new File(Environment.getExternalStorageDirectory(), "Reports");
    }

    private File getReportFile() {

        return new File(getReportsDir(), "PeopleWatcherReport.txt");
    }

    private void setupExceptionHandler() {

        File reportFile = getReportFile();
        MediaScannerConnection.scanFile(this, new String[] { reportFile.getAbsolutePath() },null, null);

        nextHandler = Thread.getDefaultUncaughtExceptionHandler();

        Thread.setDefaultUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            @Override
            public void uncaughtException(Thread thread, Throwable throwable) {
                handleUncaughtException(thread, throwable);

                nextHandler.uncaughtException(thread, throwable);
            }
        });

    }

    private void handleUncaughtException(Thread thread, Throwable throwable) {

        String report = "";

        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        throwable.printStackTrace(pw);
        report = sw.toString(); // stack trace as a string

        Log.e("Report", report);

        try {
            File reportsDir = getReportsDir();
            File reportFile = getReportFile();

            if (!reportsDir.exists())
                reportsDir.mkdirs();

            if (reportFile.exists())
                reportFile.delete();

            reportFile.createNewFile();

            FileWriter reportWriter = new FileWriter(reportFile);
            reportWriter.append(report);
            reportWriter.flush();
            reportWriter.close();
        }
        catch (IOException e) {
            Log.e("Report", "Report write failed: " + e.toString());
        }

        finish();
    }

}
