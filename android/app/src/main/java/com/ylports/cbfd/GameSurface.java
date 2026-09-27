package com.ylports.cbfd;
import android.content.Context;
import android.os.Build;
import android.view.Surface;
import android.view.SurfaceHolder;
import org.libsdl.app.SDLSurface;
/** Bounded VI/post-process resolution; Android scales it without stretching the aspect. */
final class GameSurface extends SDLSurface {
    private int requestedWidth,requestedHeight;
    GameSurface(Context c){super(c);}
    @Override protected void onSizeChanged(int w,int h,int ow,int oh){super.onSizeChanged(w,h,ow,oh);requestBuffer(w,h);}
    private void requestBuffer(int w,int h){
        if(w<2||h<2)return;
        int[] e=RenderExtent.fit(w,h);
        if(e[0]==requestedWidth&&e[1]==requestedHeight)return;
        requestedWidth=e[0];requestedHeight=e[1];getHolder().setFixedSize(e[0],e[1]);
        StartupDiagnostics.log("Game buffer="+e[0]+"x"+e[1]+" view="+w+"x"+h+"; full aspect; target=60");
    }
    @Override public void surfaceChanged(SurfaceHolder h,int format,int w,int height){
        requestBuffer(getWidth()>1?getWidth():w,getHeight()>1?getHeight():height);
        // Publish the final buffer once, before starting native SDL/Vulkan.
        if(w!=requestedWidth||height!=requestedHeight)return;
        if(Build.VERSION.SDK_INT>=30){try{h.getSurface().setFrameRate(60,Surface.FRAME_RATE_COMPATIBILITY_DEFAULT);}
            catch(IllegalArgumentException|IllegalStateException e){StartupDiagnostics.log("Frame rate hint: "+e);}}
        super.surfaceChanged(h,format,w,height);
    }
}
