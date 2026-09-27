package com.ylports.cbfd;
import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.widget.FrameLayout;
/** Real GameSurface/TouchControls on Android; SDL and JNI replaced only in this test. */
public final class MobileUiProbe extends Activity {
    private GameSurface surface;
    private TouchControls touch;
    private final Handler handler=new Handler(Looper.getMainLooper());
    @Override public void onCreate(Bundle b){
        super.onCreate(b);FrameLayout frame=new FrameLayout(this);
        frame.setBackgroundColor(0xff1b252c);
        surface=new GameSurface(this);touch=new TouchControls(this);
        frame.addView(surface,new FrameLayout.LayoutParams(-1,-1));
        frame.addView(touch,new FrameLayout.LayoutParams(-1,-1));
        setContentView(frame);Immersive.apply(this);handler.postDelayed(()->verify(0),2000);
    }
    private void require(boolean c,String label){if(!c)throw new AssertionError(label);}
    private void tap(int action,float x,float y){long now=SystemClock.uptimeMillis();MotionEvent e=MotionEvent.obtain(now,now,action,x,y,0);touch.dispatchTouchEvent(e);e.recycle();}
    private void verify(int attempt){
        if(surface.readyWidth==0 && attempt<12){handler.postDelayed(()->verify(attempt+1),1000);return;}
        try {
            int[] extent=RenderExtent.fit(surface.getWidth(),surface.getHeight());
            require(surface.readyWidth==extent[0]&&surface.readyHeight==extent[1],"final buffer not forwarded");
            require(surface.getHolder().getSurfaceFrame().width()==extent[0],"holder width mismatch");
            require(surface.getHolder().getSurfaceFrame().height()==extent[1],"holder height mismatch");
            require(touch.getWidth()==surface.getWidth()&&touch.getHeight()==surface.getHeight(),"overlay scaled with buffer");
            java.lang.reflect.Field field=TouchControls.class.getDeclaredField("layout");field.setAccessible(true);
            ControlLayout l=(ControlLayout)field.get(touch);
            for(int i=0;i<10;i++){
                tap(MotionEvent.ACTION_DOWN,l.x[i],l.y[i]);require(GameActivity.buttons==ControlLayout.BIT[i],"wrong actual touch button "+i);
                tap(MotionEvent.ACTION_UP,l.x[i],l.y[i]);require(GameActivity.buttons==0,"stuck actual touch button "+i);
            }
            tap(MotionEvent.ACTION_DOWN,l.stickX,l.stickY);
            tap(MotionEvent.ACTION_MOVE,l.stickX+l.stickTravel,l.stickY);
            require(GameActivity.x>.99f&&Math.abs(GameActivity.y)<.001,"actual stick event");
            tap(MotionEvent.ACTION_CANCEL,l.stickX,l.stickY);
            require(GameActivity.x==0&&GameActivity.buttons==0,"cancel input release");
            String msg="PASS Android surface: buffer="+surface.readyWidth+"x"+surface.readyHeight+" view="+surface.getWidth()+"x"+surface.getHeight()+" callbacks="+surface.readyCount+"; 10 real button event pairs, stick and cancel. NO native game or FPS test.";
            android.util.Log.i("ConkerUiProbe",msg);
        } catch(Throwable e){android.util.Log.e("ConkerUiProbe","FAIL Android UI probe",e);}
    }
}
