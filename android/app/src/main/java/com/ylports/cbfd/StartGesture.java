package com.ylports.cbfd;

/** Defer an isolated START press so capturing evidence does not pause the scene. */
final class StartGesture {
    static final int START = 0x1000;
    private boolean waiting, consumed, pulse;
    boolean waiting() { return waiting; }
    boolean pulse() { return pulse; }

    void update(int buttons, int pointers, boolean down, boolean up) {
        boolean held = (buttons & START) != 0;
        boolean isolated = buttons == START && pointers == 1;
        if (waiting && !isolated) {
            // Only a clean release is a tap. Sliding off or adding a finger cancels capture.
            pulse = up && pointers == 0;
            waiting = false;
        }
        if (!held) consumed = false;
        if (down && isolated && !consumed) waiting = true;
    }
    boolean capture() {
        if (!waiting) return false;
        waiting = false; consumed = true; pulse = false;
        return true;
    }
    int filter(int buttons) {
        return ((waiting || consumed) ? buttons & ~START : buttons) | (pulse ? START : 0);
    }
    void endPulse() { pulse = false; }
    void cancel() { waiting = consumed = pulse = false; }
}
