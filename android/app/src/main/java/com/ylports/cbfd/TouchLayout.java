package com.ylports.cbfd;

import java.util.HashMap;
import java.util.Map;

/** Density-based layout and pointer ownership. No Android dependencies: tested on the JVM. */
final class TouchLayout {
    static final int A=0, B=1, Z=2, L=3, R=4, START=5, CU=6, CD=7, CL=8, CR=9;
    static final int[] BITS={0x8000,0x4000,0x2000,0x20,0x10,0x1000,8,4,2,1};
    static final class Button {
        final int index;
        float x,y,rx,ry;
        boolean pill;
        Button(int i) { index=i; }
        boolean contains(float px,float py,float padding) {
            float dx=Math.abs(px-x),dy=Math.abs(py-y);
            if (pill) return dx<=rx+padding && dy<=ry+padding;
            return dx*dx+dy*dy<=(rx+padding)*(rx+padding);
        }
    }
    private static final class Pointer {
        float x,y; int button=-1;
        Pointer(float px,float py) { x=px;y=py; }
    }
    final Button[] buttons=new Button[BITS.length];
    private final Map<Integer,Pointer> pointers=new HashMap<>();
    float unit=1,baseX,baseY,stickX,stickY,stickRadius,cameraX,cameraY,axisX,axisY;
    float cameraRadius,cameraAxisX,cameraAxisY;
    int mask,stickPointer=-1,cameraPointer=-1;
    TouchLayout() { for(int i=0;i<buttons.length;i++) buttons[i]=new Button(i); }
    void resize(int width,int height,float density,float left,float top,float right,float bottom) {
        release();
        float w=Math.max(1,width-left-right),h=Math.max(1,height-top-bottom);
        unit=Math.max(.25f,Math.min(density,Math.min(w/680f,h/380f)));
        baseX=left+98*unit; baseY=top+h-100*unit; stickRadius=65*unit;
        stickX=baseX;stickY=baseY;
        place(A,left+w-74*unit,top+h-76*unit,32,32,false);
        place(B,left+w-144*unit,top+h-133*unit,28,28,false);
        place(Z,left+w-215*unit,top+h-78*unit,26,21,true);
        place(L,left+48*unit,top+32*unit,25,16,true);
        place(R,left+w-48*unit,top+32*unit,25,16,true);
        place(START,left+w*.5f,top+30*unit,32,15,true);
        cameraX=left+w-108*unit;cameraY=top+h-240*unit;cameraRadius=48*unit;
        place(CU,cameraX,cameraY-34*unit,19,19,false);
        place(CD,cameraX,cameraY+34*unit,19,19,false);
        place(CL,cameraX-34*unit,cameraY,19,19,false);
        place(CR,cameraX+34*unit,cameraY,19,19,false);
    }
    private void place(int index,float x,float y,float rx,float ry,boolean pill) {
        Button b=buttons[index];b.x=x;b.y=y;b.rx=rx*unit;b.ry=ry*unit;b.pill=pill;
    }
    void down(int id,float x,float y) {
        if(!Float.isFinite(x)||!Float.isFinite(y)) return;
        Pointer p=new Pointer(x,y);pointers.put(id,p);
        float dx=x-baseX,dy=y-baseY;
        if(stickPointer==-1 && dx*dx+dy*dy<=(stickRadius+18*unit)*(stickRadius+18*unit)) {
            stickPointer=id;
            // Slightly floating origin accommodates the thumb without moving over other controls.
            stickX=baseX+clamp(dx,-18*unit,18*unit);stickY=baseY+clamp(dy,-18*unit,18*unit);
        } else if(cameraPointer==-1 && Math.hypot(x-cameraX,y-cameraY)<=cameraRadius+10*unit) {
            cameraPointer=id;
        } else p.button=find(x,y);
        recompute();
    }
    void move(int id,float x,float y) {
        Pointer p=pointers.get(id);
        if(p==null||!Float.isFinite(x)||!Float.isFinite(y)) return;
        p.x=x;p.y=y;
        if(id!=stickPointer && id!=cameraPointer && (p.button<0 || !buttons[p.button].contains(x,y,10*unit))) p.button=find(x,y);
        recompute();
    }
    void up(int id) {
        pointers.remove(id);
        if(id==cameraPointer) cameraPointer=-1;
        if(id==stickPointer) { stickPointer=-1;stickX=baseX;stickY=baseY; }
        recompute();
    }
    void release() {
        pointers.clear();mask=0;axisX=axisY=cameraAxisX=cameraAxisY=0;stickPointer=cameraPointer=-1;stickX=baseX;stickY=baseY;
    }
    int pointerCount() { return pointers.size(); }
    private int find(float x,float y) {
        int best=-1;float distance=Float.POSITIVE_INFINITY;
        for(Button b:buttons) {
            if(b.index>=CU) continue; // Replaced by one owned right camera stick.
            // Small shoulder/camera graphics retain a generous touch area.
            float padding=Math.max(4*unit,24*unit-Math.min(b.rx,b.ry));
            if(b.contains(x,y,padding)) {
                float dx=(x-b.x)/b.rx,dy=(y-b.y)/b.ry,d=dx*dx+dy*dy;
                if(d<distance) { best=b.index;distance=d; }
            }
        }
        return best;
    }
    private void recompute() {
        mask=0;axisX=axisY=cameraAxisX=cameraAxisY=0;
        for(Map.Entry<Integer,Pointer> entry:pointers.entrySet()) {
            Pointer p=entry.getValue();
            if(entry.getKey()==stickPointer) {
                float x=(p.x-stickX)/stickRadius,y=(stickY-p.y)/stickRadius;
                float length=(float)Math.hypot(x,y);
                if(length>.08f) {
                    float scale=Math.min(1f,(length-.08f)/.92f)/length;
                    axisX=x*scale;axisY=y*scale;
                }
            } else if(entry.getKey()==cameraPointer) {
                float x=(p.x-cameraX)/cameraRadius,y=(cameraY-p.y)/cameraRadius;
                float length=(float)Math.hypot(x,y);
                if(length>.20f) {
                    float scale=Math.min(1f,(length-.20f)/.80f)/length;
                    cameraAxisX=x*scale;cameraAxisY=y*scale;
                    // Direct PC/original behavior: the right stick is a convenient
                    // four-way mapping to the N64 C-buttons.
                    if(cameraAxisX < -0.35f) mask |= BITS[CL];
                    if(cameraAxisX >  0.35f) mask |= BITS[CR];
                    if(cameraAxisY >  0.35f) mask |= BITS[CU];
                    if(cameraAxisY < -0.35f) mask |= BITS[CD];
                }
            } else if(p.button>=0) mask|=BITS[p.button];
        }
    }
    private static float clamp(float v,float low,float high) { return Math.max(low,Math.min(high,v)); }
}
