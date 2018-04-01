package com.galover.media.peoplewatcher;

import android.os.Environment;

import java.nio.ByteBuffer;

final class EngineManager {

    static private native void initialize(String sdCardPath);

    static public native void startRecord();

    static public native void sendFrame(ByteBuffer Y, ByteBuffer U, ByteBuffer V,
                                        int strideY, int strideU, int strideV, long timestamp);

    static private native void workerThreadLoop();

    static public void initialize() {

        initialize(Environment.getExternalStorageDirectory().getAbsolutePath());

        new Thread() {
            @Override
            public void run() {
                workerThreadLoop();
            }
        }.start();
    }
}
