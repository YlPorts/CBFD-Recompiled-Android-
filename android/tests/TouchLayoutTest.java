package com.ylports.cbfd;

/** Actual layout/pointer model, not a duplicate implementation or Android mock. */
public final class TouchLayoutTest {
    private static int checks;
    static void check(boolean ok,String label) { if(!ok) throw new AssertionError(label);checks++; }
    static TouchLayout layout() { TouchLayout t=new TouchLayout();t.resize(2340,1080,2.75f,36,0,0,0);return t; }
    static void press(TouchLayout t,int button,int finger) { TouchLayout.Button b=t.buttons[button];t.down(finger,b.x,b.y); }
    public static void main(String[] args) {
        int[][] sizes={{2340,1080},{2400,1080},{1920,1080},{1280,720},{1536,709},{640,360}};
        for(int[] size:sizes) {
            TouchLayout t=new TouchLayout();t.resize(size[0],size[1],2.75f,20,0,10,0);
            for(TouchLayout.Button b:t.buttons) {
                check(b.x-b.rx>20&&b.x+b.rx<size[0]-10&&b.y-b.ry>0&&b.y+b.ry<size[1],"safe bounds "+b.index);
                press(t,b.index,30);
                check(t.mask==TouchLayout.BITS[b.index],"isolated N64 input "+b.index);
                if(b.index>=TouchLayout.CU)check(Math.hypot(t.cameraAxisX,t.cameraAxisY)>.2,"camera direction is analog");
                t.up(30);
            }
            check(t.baseX-t.stickRadius>20&&t.baseY+t.stickRadius<size[1],"stick inside safe area");
        }
        TouchLayout t=layout();
        t.down(7,t.baseX,t.baseY);check(t.axisX==0&&t.axisY==0,"neutral stick");
        t.move(7,t.baseX+t.stickRadius,t.baseY);check(t.axisX>.99&&t.axisY==0,"full right");
        press(t,TouchLayout.A,11);check(t.mask==0x8000&&t.axisX>.99,"stick plus jump");
        press(t,TouchLayout.Z,42);check(t.mask==(0x8000|0x2000),"jump plus Z plus stick");
        t.up(11);check(t.mask==0x2000&&t.axisX>.99,"one lifted finger preserves others");
        t.up(7);check(t.axisX==0&&t.axisY==0&&t.mask==0x2000,"stick releases independently");
        t.release();check(t.mask==0&&t.pointerCount()==0,"cancel and focus loss");
        press(t,TouchLayout.A,2);press(t,TouchLayout.A,3);t.up(2);check(t.mask==0x8000,"two fingers same button");
        t.up(3);check(t.mask==0,"last finger release");
        press(t,TouchLayout.A,6);TouchLayout.Button a=t.buttons[TouchLayout.A];
        t.move(6,a.x+a.rx+8*t.unit,a.y);check(t.mask==0x8000,"button hysteresis");
        t.move(6,1200,530);check(t.mask==0,"drag outside releases");
        TouchLayout.Button b=t.buttons[TouchLayout.B];t.move(6,b.x,b.y);check(t.mask==0x4000,"slide to another button");t.release();
        t.down(9,t.baseX,t.baseY);t.move(9,t.baseX+1000,t.baseY-1000);
        check(Math.abs(Math.hypot(t.axisX,t.axisY)-1)<.001,"diagonal clamped");check(t.mask==0,"stick cannot press right buttons");
        t.move(9,Float.NaN,Float.POSITIVE_INFINITY);check(Float.isFinite(t.axisX),"invalid coordinates ignored");
        t.resize(1280,720,2f,0,0,0,0);check(t.mask==0&&t.axisX==0&&t.pointerCount()==0,"resize cancels ownership");
        t.down(12,t.cameraX,t.cameraY);check(t.mask==0,"camera deadzone");
        t.move(12,t.cameraX+t.cameraRadius,t.cameraY-t.cameraRadius);
        check(t.mask==(TouchLayout.BITS[TouchLayout.CU]|TouchLayout.BITS[TouchLayout.CR]) && t.cameraAxisX>0 && t.cameraAxisY>0,"diagonal right stick maps to C-up/right");
        check(Math.abs(Math.hypot(t.cameraAxisX,t.cameraAxisY)-1)<.001,"camera axis clamped");
        t.down(13,t.baseX,t.baseY);t.move(13,t.baseX+t.stickRadius,t.baseY);
        press(t,TouchLayout.A,14);check(t.mask==(0x8000|TouchLayout.BITS[TouchLayout.CU]|TouchLayout.BITS[TouchLayout.CR])&&t.axisX>.99,"two sticks plus A");
        t.up(12);check(t.mask==0x8000&&t.axisX>.99&&t.cameraAxisX==0,"camera release independent");
        t.release();
        t.down(12,t.cameraX,t.cameraY);t.down(13,t.cameraX+t.cameraRadius,t.cameraY);
        check(t.cameraPointer==12&&t.mask==0,"second camera finger cannot steal ownership");
        t.move(12,t.buttons[TouchLayout.A].x,t.buttons[TouchLayout.A].y);
        check((t.mask&0x8000)==0,"camera drag cannot press A");t.release();
        t.down(50,5,5);check(t.mask==0&&t.axisX==0,"outside controls neutral");t.up(50);
        for(int small:new int[]{TouchLayout.L,TouchLayout.R,TouchLayout.START}) {
            TouchLayout.Button p=t.buttons[small];t.down(201,p.x,p.y+23*t.unit);
            check(t.mask==TouchLayout.BITS[small],"shoulder touch halo "+small);t.release();
        }
        t.down(301,t.cameraX+t.cameraRadius+8*t.unit,t.cameraY);
        check(t.mask==TouchLayout.BITS[TouchLayout.CR] && t.cameraAxisX>.99,"right stick extended hit area");t.release();
        t.down(301,t.cameraX,t.cameraY);t.move(301,Float.NaN,Float.POSITIVE_INFINITY);
        check(t.cameraAxisX==0&&t.cameraAxisY==0,"camera ignores invalid motion");t.release();
        t.down(909,t.cameraX,t.cameraY);
        t.move(909,t.cameraX+t.cameraRadius*.24f,t.cameraY);
        check(t.cameraAxisX>.01f && t.cameraAxisX<.1f && t.mask==0,"slow analog motion is not a digital C press");
        float slow=t.cameraAxisX;
        t.move(909,t.cameraX+t.cameraRadius*.5f,t.cameraY);
        check(t.cameraAxisX>slow && t.cameraAxisX<.5f && t.mask==TouchLayout.BITS[TouchLayout.CR],"C-right crosses digital threshold");
        press(t,TouchLayout.B,911);check(t.mask==(0x4000|TouchLayout.BITS[TouchLayout.CR]) && t.cameraAxisX>0,"C-right plus B");
        t.up(911);check(t.mask==TouchLayout.BITS[TouchLayout.CR] && t.cameraAxisX>0,"button lift keeps C-right");
        t.release();check(t.cameraAxisX==0 && t.cameraAxisY==0 && t.mask==0,"cancel releases C-stick mapping");
        System.out.println("PASS "+checks+" touch layout/ownership checks");
    }
}
