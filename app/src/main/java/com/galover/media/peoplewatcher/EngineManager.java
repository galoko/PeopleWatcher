package com.galover.media.peoplewatcher;

import android.os.Environment;

import java.nio.ByteBuffer;

final class EngineManager {

    static public native void initializeEngine(String rootDir);

    static public native void startRecord();

    static public native void sendFrame(ByteBuffer Y, ByteBuffer U, ByteBuffer V,
                                        int strideY, int strideU, int strideV, long timestamp);

    static public native void stopRecord();

    static public native void finalizeEngine();
}
