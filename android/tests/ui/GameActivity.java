package com.ylports.cbfd;
/** Test input sink. This file is never part of the distributed APK. */
final class GameActivity {
    static int buttons;
    static float x,y;
    static void nativeInput(int b,float sx,float sy){buttons=b;x=sx;y=sy;}
}
