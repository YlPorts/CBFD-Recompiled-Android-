package org.libsdl.app;
import android.content.Context;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
/** Test adapter only: observes Android surface callbacks, does not invoke SDL/native/game. */
public class SDLSurface extends SurfaceView implements SurfaceHolder.Callback {
    public int readyWidth,readyHeight,readyCount;
    public SDLSurface(Context c){super(c);getHolder().addCallback(this);}
    public void surfaceCreated(SurfaceHolder h){}
    public void surfaceDestroyed(SurfaceHolder h){readyWidth=readyHeight=0;}
    public void surfaceChanged(SurfaceHolder h,int f,int w,int height){readyWidth=w;readyHeight=height;readyCount++;}
}
