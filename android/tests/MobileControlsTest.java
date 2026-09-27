package com.ylports.cbfd;

/** Pure-Java regression tests: real layout/input policies, no Android or ROM. */
public final class MobileControlsTest {
    private static int checks;
    private static void check(boolean condition, String message) {
        checks++;
        if (!condition) throw new AssertionError(message);
    }
    private static boolean near(float a, float b) { return Math.abs(a-b) < 0.0001f; }
    private static void screen(int w, int h) {
        int[] extent=RenderExtent.fit(w,h);
        check(Math.min(extent[0],extent[1])<=720,"buffer cap");
        check(extent[0]<=w && extent[1]<=h,"no upscaling");
        check((extent[0]&1)==0 && (extent[1]&1)==0,"even dimensions");
        check(Math.abs((double)w/h-(double)extent[0]/extent[1])<.005,"aspect retained");
        ControlLayout l=new ControlLayout();
        l.resize(w,h,2.5f,32,0,0,0);
        for(int i=0;i<10;i++) {
            check(l.hit(l.x[i],l.y[i])==i,"center hit "+i+" "+w+"x"+h);
            check(l.x[i]-l.rx[i]>=32 && l.x[i]+l.rx[i]<w,"safe width "+i);
            check(l.y[i]-l.ry[i]>=0 && l.y[i]+l.ry[i]<h,"safe height "+i);
        }
        check(l.hitsStick(l.stickX,l.stickY),"stick center");
        check(!l.hitsStick(w/2f,h/2f),"center of scene stays clear");
        TouchState s=new TouchState(l);
        s.down(0,l.stickX,l.stickY);
        check(s.stickActive() && s.held==0 && s.sx==0 && s.sy==0,"neutral stick");
        s.move(0,l.stickX+l.stickTravel,l.stickY);
        check(near(s.sx,1) && near(s.sy,0),"full right");
        s.down(1,l.x[0],l.y[0]); s.down(2,l.x[1],l.y[1]);
        check(s.held==0xC000 && near(s.sx,1),"stick A B simultaneous");
        s.up(1);check(s.held==0x4000 && s.stickActive(),"release A only");
        s.down(3,l.x[1],l.y[1]);s.up(2);
        check(s.held==0x4000,"two fingers same button: one remains");
        s.up(3);s.move(0,l.x[0],l.y[0]);
        check(s.held==0 && s.stickActive(),"stick capture does not press A");
        check(Math.hypot(s.sx,s.sy)<=1.00001,"radial clamp");
        s.clear();check(s.held==0 && s.sx==0 && s.sy==0 && !s.stickActive(),"cancel/focus clear");
        s.down(0,l.stickX,l.stickY);s.move(0,l.stickX+l.stickTravel*.03f,l.stickY);
        check(s.sx==0 && s.sy==0,"deadzone");s.up(0);
        s.down(0,l.stickX,l.stickY);s.move(0,l.stickX,l.stickY-l.stickTravel);
        check(near(s.sx,0) && near(s.sy,1),"up is positive Y");s.clear();
        for(int i=0;i<10;i++) {
            s.down(i,l.x[i],l.y[i]);check(s.held==ControlLayout.BIT[i],"N64 mapping "+i);
            s.up(i);check(s.held==0,"button release "+i);
        }
        s.down(-1,0,0);s.down(32,0,0);s.down(0,Float.NaN,0);s.down(1,0,Float.POSITIVE_INFINITY);
        check(s.held==0 && !s.stickActive(),"invalid pointers ignored");
        s.down(0,l.x[0],l.y[0]);
        s.move(0,l.x[0]+Math.max(l.rx[0],24*l.density)*1.1f,l.y[0]);
        check(s.held==0x8000,"release margin prevents accidental release");
        s.move(0,w/2f,h/2f);check(s.held==0,"drag outside releases");
        s.clear();s.move(0,l.x[0],l.y[0]);check(s.held==0,"move without down ignored");
    }
    public static void main(String[] args) {
        int[][] screens={{2340,1080},{2400,1080},{1920,1080},{1280,720},{1600,1200},{2560,1600}};
        for(int[] p:screens)screen(p[0],p[1]);
        int[] a=RenderExtent.fit(2340,1080);check(a[0]==1560 && a[1]==720,"A15 extent");
        a=RenderExtent.fit(641,481);check(a[0]==640 && a[1]==480,"odd low resolution");
        a=RenderExtent.fit(2,2);check(a[0]==2 && a[1]==2,"minimum size");
        boolean failed=false;try{RenderExtent.fit(0,1080);}catch(IllegalArgumentException e){failed=true;}
        check(failed,"reject invalid size");
        System.out.println("PASS "+checks+" layout, multitouch and output-extent assertions across six sizes (not an Android gameplay test)");
    }
}
