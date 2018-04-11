package com.galover.media.peoplewatcher;

import android.content.Context;
import android.graphics.ImageFormat;
import android.hardware.camera2.*;
import android.hardware.camera2.params.*;
import android.hardware.camera2.CaptureRequest.*;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.util.Log;
import android.view.Surface;
import android.media.*;
import java.nio.ByteBuffer;

import java.util.ArrayList;
import java.util.List;

class NotImplementedBehaviour extends Error {

    NotImplementedBehaviour(String message) {
        super(message);
    }
}

final class MyCameraManager  {

    private static final String CAMERA_TAG = "PW_CAMERA";

    static final int WIDTH  = 640;
    static final int HEIGHT = 480;

    private MainActivity context;
    private CameraManager manager;
    private CameraDevice camera;
    private ImageReader frame;
    private Surface frameSurface;
    private CameraCaptureSession session;
    private long startTime;

    MyCameraManager(MainActivity activity) {

        context = activity;
        frame = ImageReader.newInstance(WIDTH, HEIGHT, ImageFormat.YUV_420_888, 10);
        frameSurface = frame.getSurface();
        manager = (CameraManager) context.getSystemService(Context.CAMERA_SERVICE);
    }

    private final CameraDevice.StateCallback stateCallback = new CameraDevice.StateCallback() {

        @Override
        public void onOpened(@NonNull CameraDevice cameraDevice) {

            camera = cameraDevice;

            startRecordFrames();
        }

        @Override
        public void onDisconnected(@NonNull CameraDevice cameraDevice) {
            throw new NotImplementedBehaviour("Camera disconnected");
        }

        @Override
        public void onError(@NonNull CameraDevice cameraDevice, int error) {
            throw new Error("Camera open error: " + error);
        }
    };

    public class MyCaptureCallback extends CameraCaptureSession.CaptureCallback {


        @Override
        public void onCaptureStarted(@NonNull CameraCaptureSession session, @NonNull CaptureRequest request, long timestamp, long frameNumber) {
            // what should we do here? it's called for each frame
        }

        @Override
        public void onCaptureProgressed(@NonNull CameraCaptureSession session, @NonNull CaptureRequest request, @NonNull CaptureResult partialResult) {
            // that's all right
        }

        @Override
        public void onCaptureCompleted(@NonNull CameraCaptureSession session, @NonNull CaptureRequest request, @NonNull TotalCaptureResult result) {

            double frameTime = 0.0;

            while (true) {
                Image image = frame.acquireNextImage();
                if (image == null)
                    break;

                Image.Plane[] planes = image.getPlanes();

                Image.Plane planeY = planes[0];
                Image.Plane planeU = planes[1];
                Image.Plane planeV = planes[2];

                EngineManager.sendFrame(
                        planeY.getBuffer(), planeU.getBuffer(), planeV.getBuffer(),
                        planeY.getRowStride(), planeU.getRowStride(), planeV.getRowStride(),
                        image.getTimestamp());

                if (startTime == 0)
                    startTime = image.getTimestamp();

                frameTime = (image.getTimestamp() - startTime) / (1000.0 * 1000.0 * 1000.0);

                image.close();
            }

            if (frameTime > 24 * 60 * 60) {

                Log.i(CAMERA_TAG, "stopping");

                EngineManager.stopRecord();

                Log.i(CAMERA_TAG, "finalizing");

                EngineManager.finalizeEngine();

                Log.i(CAMERA_TAG, "finalized");

                context.finishAndRemoveTask();

                Log.i(CAMERA_TAG, "finished");

                System.exit(0);
            }
        }

        @Override
        public void onCaptureFailed(@NonNull CameraCaptureSession session, @NonNull CaptureRequest request, @NonNull CaptureFailure failure) {

            Log.e(CAMERA_TAG,"onCaptureFailed");

            throw new Error("Capture session failed: " + failure.getReason());
        }

        @Override
        public void onCaptureSequenceCompleted(@NonNull CameraCaptureSession session, int sequenceId, long frameNumber) {
            // throw new NotImplementedBehaviour("Capture sequence is completed");
        }

        @Override
        public void onCaptureSequenceAborted(@NonNull CameraCaptureSession session, int sequenceId) {
            // throw new NotImplementedBehaviour("Capture sequence aborted");
        }

        @Override
        public void onCaptureBufferLost(@NonNull CameraCaptureSession session, @NonNull CaptureRequest request, @NonNull Surface target, long frameNumber) {

            Log.e(CAMERA_TAG,"onCaptureBufferLost");

            throw new NotImplementedBehaviour("Buffer lost in capture session");
        }
    }

    private static RggbChannelVector getTemperatureVector(int WhiteBalanceValue){

        float InsertTemperature = WhiteBalanceValue;
        float temperature = InsertTemperature / 100;
        float red;
        float green;
        float blue;

        //Calculate red

        if (temperature <= 66)
            red = 255;
        else {
            red = temperature - 60;
            red = (float) (329.698727446 * (Math.pow((double) red, -0.1332047592)));
            if (red < 0)
                red = 0;
            if (red > 255)
                red = 255;
        }


        //Calculate green
        if (temperature <= 66) {
            green = temperature;
            green = (float) (99.4708025861 * Math.log(green) - 161.1195681661);
            if (green < 0)
                green = 0;
            if (green > 255)
                green = 255;
        } else
            green = temperature - 60;
        green = (float) (288.1221695283 * (Math.pow((double) red, -0.0755148492)));
        if (green < 0)
            green = 0;
        if (green > 255)
            green = 255;


        //calculate blue
        if (temperature >= 66)
            blue = 255;
        else if (temperature <= 19)
            blue = 0;
        else {
            blue = temperature - 10;
            blue = (float) (138.5177312231 * Math.log(blue) - 305.0447927307);
            if (blue < 0)
                blue = 0;
            if (blue > 255)
                blue = 255;
        }
        RggbChannelVector finalTemperatureValue = new RggbChannelVector(red/255,(green/255)/2,(green/255)/2,blue/255);
        return finalTemperatureValue;
    }

    private static int[] 				colorTransformMatrix 		= new int[]{258, 128, -119, 128, -10, 128, -40, 128, 209, 128, -41, 128, -1, 128, -74, 128, 203, 128};

    private void startRecordFrames() {

        List<Surface> surfaces = new ArrayList<>();
        surfaces.add(frameSurface);

        try {
            camera.createCaptureSession(surfaces, new CameraCaptureSession.StateCallback() {

                @Override
                public void onConfigured(@NonNull CameraCaptureSession cameraCaptureSession) {
                    try {

                        Log.i(CAMERA_TAG, "camera configured");

                        EngineManager.startRecord();

                        startTime = 0;

                        session = cameraCaptureSession;

                        CaptureRequest.Builder builder = camera.createCaptureRequest(CameraDevice.TEMPLATE_RECORD);

                        builder.addTarget(frameSurface);
                        builder.set(CaptureRequest.CONTROL_MODE, CaptureRequest.CONTROL_MODE_AUTO);

                        session.setRepeatingRequest(builder.build(), new MyCaptureCallback(), null);
                    } catch (CameraAccessException e) {
                        throw new Error("");
                    }
                }

                @Override
                public void onConfigureFailed(@NonNull CameraCaptureSession cameraCaptureSession) {
                    throw new Error("Camera configure failed");
                }
            }, null);
        } catch (CameraAccessException e) {
            throw new Error("Access to Camera is denied");
        }
    }

    boolean openBackRearCamera() {

        if (manager == null)
            return false;

        try {
            String[] cameraIdList = manager.getCameraIdList();

            for (String cameraId : cameraIdList) {
                CameraCharacteristics characteristics = manager.getCameraCharacteristics(cameraId);

                Integer facing = characteristics.get(CameraCharacteristics.LENS_FACING);
                if (facing != null && facing == CameraCharacteristics.LENS_FACING_BACK) {

                    StreamConfigurationMap map = characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
                    if (map == null) {
                        continue;
                    }

                    manager.openCamera(cameraId, stateCallback, null);

                    return true;
                }
            }
        }
        catch (CameraAccessException|SecurityException E) {
            return false;
        }

        return false;
    }
}
