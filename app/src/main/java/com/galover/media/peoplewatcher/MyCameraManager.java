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

    private Context context;
    private CameraManager manager;
    private CameraDevice camera;
    private ImageReader frame;
    private Surface frameSurface;
    private CameraCaptureSession session;
    private long startTime;

    MyCameraManager(Context ctx) {

        context = ctx;
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

                        builder.set(CaptureRequest.CONTROL_AE_MODE, CaptureRequest.CONTROL_AE_MODE_ON);
                        builder.set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_AUTO);
                        builder.set(CaptureRequest.CONTROL_AWB_MODE, CaptureRequest.CONTROL_AWB_MODE_CLOUDY_DAYLIGHT);

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

                    boolean Support = false;
                    int [] values2 = characteristics.get(CameraCharacteristics.CONTROL_AWB_AVAILABLE_MODES);
                    for(int value2 : values2) {
                            if( value2 == CameraMetadata.CONTROL_AWB_MODE_OFF ) {
                                Support = true;
                                break;
                            };
                        }

                    if (Support)
                        Log.i(CAMERA_TAG, "Camera does support manual whitebalance");
                    else
                        Log.i(CAMERA_TAG, "Camera doesn't support manual whitebalance");

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
