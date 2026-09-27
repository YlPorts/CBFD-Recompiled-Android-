package com.ylports.cbfd;

import android.content.Context;
import android.graphics.*;
import android.os.Build;
import android.view.*;

/** Lightweight hardware-canvas overlay: compact thumb cluster and independent pointers. */
final class TouchControls extends View {
    interface InputSink { void accept(int buttons,float x,float y); }
    private final InputSink input;
    private final TouchLayout layout=new TouchLayout();
    private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF rect=new RectF();
    private final Path arrow=new Path();
    private final Runnable diagnostics;
    private float insetLeft,insetRight,insetTop,insetBottom;
    private int sentMask;
    private float sentX,sentY;
    private final StartGesture startGesture=new StartGesture();
    private static final String[] LABEL={"A","B","Z","L","R","START"};
    private static final int[] ACCENT={0xff8cc3f5,0xff91d4a0,0xffdae0e8,0xffd5dce4,0xffd5dce4,0xffd5dce4,
        0xffebcd87,0xffebcd87,0xffebcd87,0xffebcd87};
    private final Runnable longStart;
    private final Runnable releaseStartPulse=()->{startGesture.endPulse();sendInput();};
    TouchControls(Context context,Runnable copyDiagnostics) {
        this(context,copyDiagnostics,GameActivity::nativeInput);
    }
    TouchControls(Context context,Runnable copyDiagnostics,InputSink sink) {
        super(context);diagnostics=copyDiagnostics;input=sink;setFocusable(false);
        longStart=()->{
            if(startGesture.capture() && diagnostics!=null) diagnostics.run();
        };
        setContentDescription("Controles: palanca izquierda para moverse, palanca derecha para los botones C, A B Z. Mantener START copia el diagnóstico.");
        paint.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));
    }
    @Override protected void onSizeChanged(int w,int h,int oldw,int oldh) { resize(); }
    @Override public WindowInsets onApplyWindowInsets(WindowInsets insets) {
        float left=0,right=0,top=0,bottom=0;
        if(Build.VERSION.SDK_INT>=28 && insets.getDisplayCutout()!=null) {
            DisplayCutout cutout=insets.getDisplayCutout();
            left=cutout.getSafeInsetLeft();right=cutout.getSafeInsetRight();
            top=cutout.getSafeInsetTop();bottom=cutout.getSafeInsetBottom();
        }
        if(left!=insetLeft||right!=insetRight||top!=insetTop||bottom!=insetBottom) {
            insetLeft=left;insetRight=right;insetTop=top;insetBottom=bottom;resize();
        }
        return insets;
    }
    private void resize() {
        releaseAll();layout.resize(getWidth(),getHeight(),getResources().getDisplayMetrics().density,
            insetLeft,insetTop,insetRight,insetBottom);invalidate();
    }
    @Override public boolean onTouchEvent(MotionEvent event) {
        int action=event.getActionMasked(),index=event.getActionIndex();
        if(action==MotionEvent.ACTION_CANCEL) { releaseAll();return true; }
        if(action==MotionEvent.ACTION_DOWN||action==MotionEvent.ACTION_POINTER_DOWN)
            layout.down(event.getPointerId(index),event.getX(index),event.getY(index));
        for(int i=0;i<event.getPointerCount();i++) {
            int id=event.getPointerId(i);
            if((action==MotionEvent.ACTION_UP||action==MotionEvent.ACTION_POINTER_UP)&&i==index) layout.up(id);
            else layout.move(id,event.getX(i),event.getY(i));
        }
        boolean wasWaiting=startGesture.waiting(),wasPulse=startGesture.pulse();
        startGesture.update(layout.mask,layout.pointerCount(),
            action==MotionEvent.ACTION_DOWN||action==MotionEvent.ACTION_POINTER_DOWN,
            action==MotionEvent.ACTION_UP||action==MotionEvent.ACTION_POINTER_UP);
        if(startGesture.waiting()&&!wasWaiting) postDelayed(longStart,1400);
        else if(!startGesture.waiting()) removeCallbacks(longStart);
        if(startGesture.pulse()&&!wasPulse) postDelayed(releaseStartPulse,100);
        sendInput();postInvalidateOnAnimation();return true;
    }
    private void sendInput() {
        final int buttons=startGesture.filter(layout.mask);
        // Quantize below the N64 stick's precision; never drop button edges.
        if(buttons!=sentMask||Math.abs(layout.axisX-sentX)>1f/512||Math.abs(layout.axisY-sentY)>1f/512
            ||(layout.axisX==0&&sentX!=0)||(layout.axisY==0&&sentY!=0)) {
            input.accept(buttons,layout.axisX,layout.axisY);
            sentMask=buttons;sentX=layout.axisX;sentY=layout.axisY;
        }
    }
    void releaseAll() {
        removeCallbacks(longStart);removeCallbacks(releaseStartPulse);startGesture.cancel();layout.release();sendInput();invalidate();
    }
    @Override protected void onDetachedFromWindow() { releaseAll();super.onDetachedFromWindow(); }
    @Override protected void onDraw(Canvas canvas) {
        if(getWidth()==0||getHeight()==0) return;
        final float u=layout.unit,r=layout.stickRadius,x=layout.stickX,y=layout.stickY;
        paint.setStyle(Paint.Style.FILL);paint.setColor(0x3510161d);canvas.drawCircle(x,y,r,paint);
        paint.setStrokeWidth(1.2f*u);paint.setStyle(Paint.Style.STROKE);paint.setColor(0x6597a9b8);
        canvas.drawCircle(x,y,r,paint);paint.setColor(0x1fffffff);canvas.drawCircle(x,y,r*.68f,paint);
        paint.setStrokeWidth(1.3f*u);paint.setColor(0x428e9ca9);
        for(int i=0;i<4;i++) {
            float dx=(i==2?-1:i==3?1:0),dy=(i==0?-1:i==1?1:0);
            float tx=x+dx*r*.81f,ty=y+dy*r*.81f;
            canvas.drawLine(tx-dy*3*u,ty+dx*3*u,tx+dx*3*u,ty+dy*3*u,paint);
            canvas.drawLine(tx+dx*3*u,ty+dy*3*u,tx+dy*3*u,ty-dx*3*u,paint);
        }
        float kx=x+layout.axisX*r*.54f,ky=y-layout.axisY*r*.54f,kr=r*.40f;
        paint.setStyle(Paint.Style.FILL);paint.setColor(layout.stickPointer<0?0x88586370:0xb78196a7);
        canvas.drawCircle(kx,ky,kr,paint);paint.setColor(0x429baab6);canvas.drawCircle(kx-u,ky-2*u,kr*.89f,paint);
        paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(1.2f*u);paint.setColor(0x94d5e1e9);canvas.drawCircle(kx,ky,kr,paint);
        for(TouchLayout.Button b:layout.buttons) {
            if(b.index>=TouchLayout.CU) continue;
            boolean held=(layout.mask&TouchLayout.BITS[b.index])!=0;
            rect.set(b.x-b.rx,b.y-b.ry,b.x+b.rx,b.y+b.ry);
            paint.setStyle(Paint.Style.FILL);paint.setColor(held?(ACCENT[b.index]&0x00ffffff)|0x99000000:0x58141a22);
            shape(canvas,b);
            paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth((held?1.6f:1.1f)*u);
            paint.setColor((ACCENT[b.index]&0x00ffffff)|(held?0xdd000000:0x82000000));shape(canvas,b);
            paint.setStyle(Paint.Style.FILL);paint.setColor(held?0xffffffff:ACCENT[b.index]);
            if(b.index>=TouchLayout.CU) drawArrow(canvas,b.x,b.y,b.index-TouchLayout.CU,u);
            else {
                paint.setTextAlign(Paint.Align.CENTER);paint.setTextSize((b.index==TouchLayout.START?10:b.index<2?20:14)*u);
                canvas.drawText(LABEL[b.index],b.x,b.y-(paint.ascent()+paint.descent())*.5f,paint);
            }
        }
        final float cx=layout.cameraX,cy=layout.cameraY,cr=layout.cameraRadius;
        paint.setStyle(Paint.Style.FILL);paint.setColor(0x45171c23);canvas.drawCircle(cx,cy,cr,paint);
        paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(1.2f*u);paint.setColor(0x85e7ce99);
        canvas.drawCircle(cx,cy,cr,paint);paint.setColor(0x25e7ce99);canvas.drawCircle(cx,cy,cr*.62f,paint);
        float tx=cx+layout.cameraAxisX*cr*.55f,ty=cy-layout.cameraAxisY*cr*.55f;
        paint.setStyle(Paint.Style.FILL);paint.setColor(layout.cameraPointer<0?0x60696459:0x997d7560);
        canvas.drawCircle(tx,ty,cr*.32f,paint);
        paint.setStyle(Paint.Style.STROKE);paint.setColor(0xa6e7ce99);canvas.drawCircle(tx,ty,cr*.32f,paint);
        paint.setStyle(Paint.Style.FILL);paint.setTextAlign(Paint.Align.CENTER);paint.setTextSize(7.5f*u);paint.setColor(0xbbe7ce99);
        canvas.drawText("CAM",tx,ty-(paint.ascent()+paint.descent())*.5f,paint);
    }
    private void shape(Canvas canvas,TouchLayout.Button b) {
        if(b.pill) canvas.drawRoundRect(rect,b.ry,b.ry,paint);else canvas.drawCircle(b.x,b.y,b.rx,paint);
    }
    private void drawArrow(Canvas c,float x,float y,int direction,float u) {
        c.save();c.translate(x,y);
        c.rotate(direction==0?0:direction==1?180:direction==2?270:90);
        arrow.reset();arrow.moveTo(0,-6*u);arrow.lineTo(6*u,3*u);arrow.lineTo(-6*u,3*u);arrow.close();
        c.drawPath(arrow,paint);c.restore();
    }
}
