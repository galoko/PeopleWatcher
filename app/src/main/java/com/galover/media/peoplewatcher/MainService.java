package com.galover.media.peoplewatcher;

import android.app.Notification;
import android.app.Service;
import android.content.Intent;
import android.media.MediaScannerConnection;
import android.net.wifi.WifiManager;
import android.os.IBinder;
import android.app.PendingIntent;
import android.os.PowerManager;
import android.util.Log;
import android.app.IntentService;

import org.apache.ftpserver.FtpServer;
import org.apache.ftpserver.ftplet.FtpException;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;

import am.util.ftpserver.FTPHelper;

public class MainService extends IntentService {

    private static final String SERVICE_TAG = "PW_SERVICE";
    private static final String REPORT_TAG = "PW_REPORT";

    private Thread.UncaughtExceptionHandler nextHandler;
    private String FtpRootDir;
    private MyCameraManager cameraManager;
    private PowerManager.WakeLock wakeLock;
    private WifiManager.WifiLock wifiLock;

    void setupLogToFile() {
        File logFile = new File(FtpRootDir + "/PeopleWatcherLog.txt");

        Log.i(SERVICE_TAG, "Logging to " + logFile.getAbsolutePath());

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

    void preventCPUTurnOff() {
        PowerManager powerManager = (PowerManager) getApplicationContext().getSystemService(POWER_SERVICE);
        if (powerManager == null)
            throw new Error("Couldn't get power manager");

        wakeLock = powerManager.newWakeLock(PowerManager.SCREEN_DIM_WAKE_LOCK | PowerManager.ACQUIRE_CAUSES_WAKEUP,
                "peoplewatcher:wakelock");
        wakeLock.acquire();
    }

    public void preventWiFiTurnOff() {

        WifiManager wifiManager = (WifiManager) getApplicationContext().getSystemService(WIFI_SERVICE);

        wifiLock = wifiManager.createWifiLock(WifiManager.WIFI_MODE_FULL, "peoplewatcher:wifilock");
        wifiLock.acquire();

        if (!wifiLock.isHeld())
            Log.w(SERVICE_TAG, "Couldn't acquire wifi lock");
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

    private void startup() {

        FtpRootDir = this.getExternalFilesDir(null).getAbsolutePath();

        setupExceptionHandler();

        setupLogToFile();

        // preventCPUTurnOff();
        preventWiFiTurnOff();

        EngineManager.initializeEngine(createRecordsDir());

        setupFTPServer();

        cameraManager = new MyCameraManager(this);

        if (!cameraManager.openBackRearCamera())
            throw new Error("Cannot open back rear camera");
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

        this.stopSelf();
    }

    // service stuff

    public MainService() {
        super("MainService");
    }

    @Override
    public void onCreate() {

        Intent notificationIntent = new Intent(this, MainActivity.class);

        PendingIntent pendingIntent = PendingIntent.getActivity(this, 0,
                notificationIntent, 0);

        Notification notification = new Notification.Builder(this)
                .setSmallIcon(R.drawable.ic_notification)
                .setContentTitle("Watching people")
                .setContentText("We gonna watch some people")
                .setContentIntent(pendingIntent).build();

        startForeground(746546, notification);

        startup();
    }

    @Override
    protected void onHandleIntent(Intent intent) {

    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent arg0) {
        return null;
    }

    @Override
    public void onDestroy() {

        Log.e(SERVICE_TAG, "Service is destroyed, this isn't supposed to happen");
    }
}