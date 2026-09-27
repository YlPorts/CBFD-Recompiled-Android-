package com.ylports.cbfd;
/** Pointer capture and multi-finger input; move events do not allocate objects. */
final class TouchState {
    private final boolean[] down=new boolean[32];
    private final float[] px=new float[32],py=new float[32];
    private final int[] button=new int[32];
    private int stick=-1;
    int held;float sx,sy,originX,originY;
    final ControlLayout layout;
    TouchState(ControlLayout l){layout=l;}
    void down(int id,float x,float y){
        if(id<0||id>=32||!Float.isFinite(x)||!Float.isFinite(y))return;
        down[id]=true;px[id]=x;py[id]=y;button[id]=layout.hit(x,y);
        if(stick<0&&button[id]<0&&layout.hitsStick(x,y)){
            stick=id;float dx=x-layout.stickX,dy=y-layout.stickY;
            float length=(float)Math.hypot(dx,dy),limit=layout.stickRadius*.28f,scale=length>limit?limit/length:1;
            originX=layout.stickX+dx*scale;originY=layout.stickY+dy*scale;
        }recompute();
    }
    void move(int id,float x,float y){
        if(id<0||id>=32||!down[id]||!Float.isFinite(x)||!Float.isFinite(y))return;
        px[id]=x;py[id]=y;
        if(id!=stick&&(button[id]<0||!layout.contains(button[id],x,y,1.18f)))button[id]=layout.hit(x,y);
        recompute();
    }
    void up(int id){if(id<0||id>=32)return;down[id]=false;button[id]=-1;if(id==stick)stick=-1;recompute();}
    boolean stickActive(){return stick>=0;}
    void clear(){java.util.Arrays.fill(down,false);stick=-1;recompute();}
    private void recompute(){held=0;sx=sy=0;
        for(int i=0;i<32;i++)if(down[i]){
            if(i==stick){float x=(px[i]-originX)/Math.max(1,layout.stickTravel),y=(originY-py[i])/Math.max(1,layout.stickTravel);
                float length=(float)Math.hypot(x,y);if(length>.08f){float scale=Math.min(1,(length-.08f)/.92f)/length;sx=x*scale;sy=y*scale;}}
            else if(button[i]>=0)held|=ControlLayout.BIT[button[i]];
        }
    }
}
