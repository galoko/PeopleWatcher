package com.galover.media.peoplewatcher;

import android.Manifest;
import android.app.Activity;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.content.pm.PackageManager;
import android.util.Log;
import java.io.*;

import android.media.MediaScannerConnection;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.content.Intent;

import am.util.ftpserver.*;
import org.apache.ftpserver.*;
import org.apache.ftpserver.ftplet.FtpException;

import android.net.wifi.WifiManager;
import android.net.wifi.WifiManager.WifiLock;

public class MainActivity extends Activity {

    private static final String MAIN_TAG = "PW_MAIN";
    private static final String REPORT_TAG = "PW_REPORT";

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("engine");
    }

    private Thread.UncaughtExceptionHandler nextHandler;

    private MyCameraManager cameraManager;

    private String FtpRootDir;

    void setupLogToFile() {
        File logFile = new File(FtpRootDir + "/PeopleWatcherLog.txt");

        Log.i(REPORT_TAG, "Logging to " + logFile.getAbsolutePath());

        if (logFile.exists())
            logFile.delete();

        try {
            logFile.createNewFile();

            // Runtime.getRuntime().exec("logcat -G 64KB");
            Runtime.getRuntime().exec("logcat -c");
            Runtime.getRuntime().exec("logcat ImageReader_JNI:S *:* -f " + logFile.getAbsolutePath());
        } catch (IOException e) {
            throw new Error("Couldn't setup log to file");
        }
    }

    private WakeLock wakeLock;

    void preventCPUTurnOff() {
        PowerManager powerManager = (PowerManager) getSystemService(POWER_SERVICE);
        if (powerManager == null)
            throw new Error("Couldn't get power manager");

        wakeLock = powerManager.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "peoplewatcher:mywakelock");
        wakeLock.acquire();
    }

    void startSimpleForegroundService() {
        Intent service = new Intent(this, SimpleService.class);
        service.setAction("NOTHING");
        startService(service);
    }

    private WifiLock wifiLock;

    public void preventWiFiTurnOff() {

        WifiManager wifiManager = (WifiManager) getApplicationContext().getSystemService(WIFI_SERVICE);

        wifiLock = wifiManager.createWifiLock(WifiManager.WIFI_MODE_FULL_HIGH_PERF, "peoplewatcher:wifilock");
        wifiLock.acquire();
    }

    @Override
    public void onStart() {
        super.onStart();
        Log.d(MAIN_TAG, "Activity started");
    }

    @Override
    public void onStop() {
        super.onStop();
        Log.d(MAIN_TAG, "Activity stopped");
    }

    void setupFTPServer() {

        new Thread() {

            @Override
            public void run() {
                final int port = 9436;
                final String home = FtpRootDir;
                FtpServer server = FTPHelper.createServer(port, 10, 5000, true, home);

                try {
                    server.start();
                } catch (FtpException e) {

                    throw new Error("FTP error: " + e.getMessage());
                }
            }
        }.start();
    }

    String createRecordsDir() {
        File recordsDir = new File(FtpRootDir + "/Records");
        if (!recordsDir.exists())
            recordsDir.mkdirs();

        return recordsDir.getAbsolutePath();
    }

    void startup() {

        FtpRootDir = this.getExternalFilesDir(null).getAbsolutePath();

        EngineManager.initializeEngine(createRecordsDir());

        setupLogToFile();

        startSimpleForegroundService();

        preventCPUTurnOff();
        preventWiFiTurnOff();

        setupFTPServer();

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
                        Manifest.permission.READ_EXTERNAL_STORAGE,
                        Manifest.permission.WAKE_LOCK
                },1);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {

        for (int grantResult : grantResults) {
            if (grantResult != PackageManager.PERMISSION_GRANTED)
                throw new Error("Permissions denied");
        }

        startup();
    }

    private File getReportsDir() {

        return new File(FtpRootDir, "Reports");
    }

    private File getReportFile() {

        return new File(getReportsDir(), "Report.txt");
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

        Log.e(REPORT_TAG, report);

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
            Log.e(REPORT_TAG, "Report write failed: " + e.toString());
        }

        finish();
    }

}
