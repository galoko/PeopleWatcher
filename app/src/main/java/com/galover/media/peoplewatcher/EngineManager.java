package com.galover.media.peoplewatcher;

import java.nio.ByteBuffer;

final class EngineManager {

    // huita
    static public native void initialize(String sdCardPath);

    static public native void startRecord(long timestamp);

    static public native void sendFrame(ByteBuffer Y, ByteBuffer U, ByteBuffer V,
                                        int strideY, int strideU, int strideV, long timestamp);
}
