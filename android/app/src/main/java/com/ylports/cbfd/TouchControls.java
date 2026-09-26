package com.ylports.cbfd;

import android.content.Context;
import android.graphics.*;
import android.util.SparseArray;
import android.view.*;

/** Native N64 input snapshot, including simultaneous stick + buttons. No key-event emulation. */
final class TouchControls extends View {
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final SparseArray<PointF> pointers = new SparseArray<>();
    private int stickPointer = -1, held;
    private float sx, sy;
    private static final String[] LABEL = {"A", "B", "Z", "L", "R", "START", "C↑", "C↓", "C←", "C→"};
    private static final int[] BIT = {0x8000, 0x4000, 0x2000, 0x20, 0x10, 0x1000, 8, 4, 2, 1};
    private static final float[] X = {.90f, .79f, .34f, .12f, .88f, .50f, .79f, .79f, .72f, .86f};
    private static final float[] Y = {.78f, .67f, .82f, .14f, .14f, .14f, .32f, .52f, .42f, .42f};
    TouchControls(Context context) { super(context); setFocusable(false); setContentDescription("Controles de Conker"); }
    private float radius(int index) { return getHeight() * (index < 2 ? .082f : .060f); }
    private float stickRadius() { return getHeight() * .17f; }
    private float stickX() { return getWidth() * .16f; }
    private float stickY() { return getHeight() * .70f; }
    @Override public boolean onTouchEvent(MotionEvent event) {
        int action = event.getActionMasked(), at = event.getActionIndex();
        if (action == MotionEvent.ACTION_CANCEL) { releaseAll(); return true; }
        for (int i = 0; i < event.getPointerCount(); i++) {
            int id = event.getPointerId(i);
            if ((action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP) && i == at) {
                pointers.remove(id);
                if (id == stickPointer) stickPointer = -1;
            } else {
                PointF point = pointers.get(id);
                if (point == null) { point = new PointF(); pointers.put(id, point); }
                point.set(event.getX(i), event.getY(i));
            }
        }
        if ((action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) && stickPointer < 0) {
            float dx = event.getX(at) - stickX(), dy = event.getY(at) - stickY();
            if (Math.hypot(dx, dy) <= stickRadius() * 1.35f) stickPointer = event.getPointerId(at);
        }
        held = 0; sx = 0; sy = 0;
        for (int i = 0; i < pointers.size(); i++) {
            PointF point = pointers.valueAt(i);
            if (pointers.keyAt(i) == stickPointer) {
                sx = (point.x - stickX()) / stickRadius();
                sy = (stickY() - point.y) / stickRadius();
                float length = (float)Math.hypot(sx, sy);
                if (length <= .10f) { sx = 0; sy = 0; }
                else {
                    float scale = Math.min(1f, (length - .10f) / .90f) / length;
                    sx *= scale; sy *= scale;
                }
            } else {
                for (int b = 0; b < BIT.length; b++) {
                    if (Math.hypot(point.x - getWidth()*X[b], point.y - getHeight()*Y[b]) <= radius(b)) held |= BIT[b];
                }
            }
        }
        GameActivity.nativeInput(held, sx, sy);
        invalidate();
        return true;
    }
    void releaseAll() {
        pointers.clear(); stickPointer = -1; held = 0; sx = sy = 0;
        GameActivity.nativeInput(0, 0, 0);
        invalidate();
    }
    @Override protected void onDraw(Canvas canvas) {
        paint.setStrokeWidth(Math.max(1f, getHeight()*.004f));
        paint.setColor(0x80FFFFFF); paint.setStyle(Paint.Style.STROKE);
        canvas.drawCircle(stickX(), stickY(), stickRadius(), paint);
        paint.setStyle(Paint.Style.FILL);
        canvas.drawCircle(stickX()+sx*stickRadius()*.65f, stickY()-sy*stickRadius()*.65f, stickRadius()*.32f, paint);
        paint.setTextAlign(Paint.Align.CENTER);
        paint.setTextSize(getHeight()*.037f);
        for (int b = 0; b < BIT.length; b++) {
            float x = getWidth()*X[b], y = getHeight()*Y[b];
            paint.setStyle(Paint.Style.FILL);
            paint.setColor((held & BIT[b]) != 0 ? 0x99FFFFFF : 0x33000000);
            canvas.drawCircle(x, y, radius(b), paint);
            paint.setColor(0xBFFFFFFF); paint.setStyle(Paint.Style.STROKE);
            canvas.drawCircle(x, y, radius(b), paint);
            paint.setStyle(Paint.Style.FILL);
            canvas.drawText(LABEL[b], x, y-(paint.ascent()+paint.descent())/2, paint);
        }
    }
}
