package com.ylports.cbfd;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.os.Build;
import android.view.MotionEvent;
import android.view.View;
/** Compact controls in native UI resolution, independent of the lower game buffer. */
final class TouchControls extends View {
    private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF rect=new RectF();
    private final ControlLayout layout=new ControlLayout();
    private final TouchState state=new TouchState(layout);
    private int sl,st,sr,sb,lastButtons;private float lastX,lastY;
    TouchControls(Context c){super(c);setFocusable(false);setContentDescription("Controles de Conker");
        paint.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));paint.setTextAlign(Paint.Align.CENTER);paint.setStrokeCap(Paint.Cap.ROUND);
        setOnApplyWindowInsetsListener((v,insets)->{
            if(Build.VERSION.SDK_INT>=28&&insets.getDisplayCutout()!=null){android.view.DisplayCutout cut=insets.getDisplayCutout();
                sl=cut.getSafeInsetLeft();st=cut.getSafeInsetTop();sr=cut.getSafeInsetRight();sb=cut.getSafeInsetBottom();}
            else sl=st=sr=sb=0;
            resize();return insets;
        });
    }
    private void resize(){if(getWidth()<2||getHeight()<2)return;state.clear();layout.resize(getWidth(),getHeight(),getResources().getDisplayMetrics().density,sl,st,sr,sb);publish();postInvalidateOnAnimation();}
    @Override protected void onSizeChanged(int w,int h,int ow,int oh){super.onSizeChanged(w,h,ow,oh);resize();}
    @Override public boolean onTouchEvent(MotionEvent e){int a=e.getActionMasked(),at=e.getActionIndex();
        if(a==MotionEvent.ACTION_CANCEL){releaseAll();return true;}
        if(a==MotionEvent.ACTION_DOWN||a==MotionEvent.ACTION_POINTER_DOWN)state.down(e.getPointerId(at),e.getX(at),e.getY(at));
        for(int i=0;i<e.getPointerCount();i++){
            if((a==MotionEvent.ACTION_UP||a==MotionEvent.ACTION_POINTER_UP)&&i==at)state.up(e.getPointerId(i));
            else state.move(e.getPointerId(i),e.getX(i),e.getY(i));
        }publish();postInvalidateOnAnimation();return true;
    }
    private void publish(){if(lastButtons==state.held&&lastX==state.sx&&lastY==state.sy)return;
        lastButtons=state.held;lastX=state.sx;lastY=state.sy;GameActivity.nativeInput(lastButtons,lastX,lastY);}
    void releaseAll(){state.clear();publish();postInvalidateOnAnimation();}
    @Override protected void onDetachedFromWindow(){releaseAll();super.onDetachedFromWindow();}
    private void oval(Canvas c,float x,float y,float rx,float ry,int color,boolean stroke){paint.setColor(color);paint.setStyle(stroke?Paint.Style.STROKE:Paint.Style.FILL);rect.set(x-rx,y-ry,x+rx,y+ry);c.drawOval(rect,paint);}
    private int accent(int i){return i==0?0xFFB7E6DF:i==1?0xFFE7BA97:i>=6?0xFFD9CEAB:0xFFDCE1E2;}
    @Override protected void onDraw(Canvas c){float u=layout.unit;if(u<=0)return;paint.setStrokeWidth(Math.max(layout.density,.002f*u));
        float ox=state.stickActive()?state.originX:layout.stickX,oy=state.stickActive()?state.originY:layout.stickY,r=layout.stickRadius;
        oval(c,ox,oy,r,r,0x480C1519,false);oval(c,ox,oy,r,r,0x55E4EEEE,true);oval(c,ox,oy,r*.72f,r*.72f,0x22CFDFDD,true);
        float knob=r*.38f,kx=ox+state.sx*layout.stickTravel,ky=oy-state.sy*layout.stickTravel;
        oval(c,kx,ky+u*.004f,knob,knob,0x66070D11,false);oval(c,kx,ky,knob,knob,state.stickActive()?0xB89ABAB7:0x829AAEAF,false);oval(c,kx,ky,knob,knob,0x88E4EFEE,true);
        for(int i=0;i<10;i++){
            float x=layout.x[i],y=layout.y[i],rx=layout.rx[i],ry=layout.ry[i];boolean held=(state.held&ControlLayout.BIT[i])!=0;int tone=accent(i);
            paint.setStyle(Paint.Style.FILL);paint.setColor(held?0xAF314B50:0x600D191D);rect.set(x-rx,y-ry,x+rx,y+ry);
            if(i<2)c.drawOval(rect,paint);else c.drawRoundRect(rect,ry*.63f,ry*.63f,paint);
            paint.setStyle(Paint.Style.STROKE);paint.setColor((held?0xCC000000:0x60000000)|(tone&0xFFFFFF));
            if(i<2)c.drawOval(rect,paint);else c.drawRoundRect(rect,ry*.63f,ry*.63f,paint);
            paint.setColor((held?0xFF000000:0xD0000000)|(tone&0xFFFFFF));
            if(i>=6){float d=rx*.34f;paint.setStrokeWidth(Math.max(layout.density*1.5f,u*.003f));
                if(i==6||i==7){float s=i==6?-1:1;c.drawLine(x-d,y-s*d*.4f,x,y+s*d*.6f,paint);c.drawLine(x,y+s*d*.6f,x+d,y-s*d*.4f,paint);}
                else{float s=i==8?-1:1;c.drawLine(x-s*d*.4f,y-d,x+s*d*.6f,y,paint);c.drawLine(x+s*d*.6f,y,x-s*d*.4f,y+d,paint);}
                paint.setStrokeWidth(Math.max(layout.density,.002f*u));
            }else{paint.setStyle(Paint.Style.FILL);paint.setTextSize(u*(i<2?.039f:i==5?.022f:.029f));c.drawText(ControlLayout.LABEL[i],x,y-(paint.ascent()+paint.descent())/2,paint);}
        }
        paint.setStyle(Paint.Style.FILL);paint.setColor(0x88D9CEAB);paint.setTextSize(u*.020f);c.drawText("C",layout.x[6],layout.y[8]-(paint.ascent()+paint.descent())/2,paint);
    }
}
