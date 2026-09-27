package com.ylports.cbfd;
/** Shared geometry for rendering and hit tests, without Android dependencies. */
final class ControlLayout {
    static final String[] LABEL={"A","B","Z","L","R","START","","","",""};
    static final int[] BIT={0x8000,0x4000,0x2000,0x20,0x10,0x1000,8,4,2,1};
    final float[] x=new float[10],y=new float[10],rx=new float[10],ry=new float[10];
    float unit,density,stickX,stickY,stickRadius,stickTravel;
    void resize(int width,int height,float dpi,int left,int top,int right,int bottom){
        density=Math.max(1,dpi);
        float l=Math.max(left,12*density),t=Math.max(top,8*density);
        float r=width-Math.max(right,12*density),b=height-Math.max(bottom,8*density);
        unit=Math.max(1,Math.min(b-t,(r-l)/1.45f));
        stickX=l+.24f*unit;stickY=b-.21f*unit;stickRadius=.122f*unit;stickTravel=.083f*unit;
        set(0,r-.135f*unit,b-.15f*unit,.070f*unit,.070f*unit);
        set(1,r-.315f*unit,b-.275f*unit,.062f*unit,.062f*unit);
        set(2,l+.235f*unit,b-.44f*unit,.096f*unit,.040f*unit);
        set(3,l+.17f*unit,t+.11f*unit,.078f*unit,.034f*unit);
        set(4,r-.12f*unit,b-.395f*unit,.078f*unit,.034f*unit);
        set(5,(l+r)/2,t+.065f*unit,.090f*unit,.030f*unit);
        float cx=r-.275f*unit,cy=b-.565f*unit,d=.105f*unit,a=.041f*unit;
        set(6,cx,cy-d,a,a);set(7,cx,cy+d,a,a);set(8,cx-d,cy,a,a);set(9,cx+d,cy,a,a);
    }
    private void set(int i,float a,float b,float c,float d){x[i]=a;y[i]=b;rx[i]=c;ry[i]=d;}
    boolean contains(int i,float px,float py,float slop){
        float ax=Math.max(rx[i],24*density)*slop,ay=Math.max(ry[i],24*density)*slop;
        float dx=(px-x[i])/ax,dy=(py-y[i])/ay;return dx*dx+dy*dy<=1;
    }
    int hit(float px,float py){int best=-1;float distance=Float.MAX_VALUE;
        for(int i=0;i<10;i++)if(contains(i,px,py,1)){float dx=px-x[i],dy=py-y[i],d=dx*dx+dy*dy;if(d<distance){distance=d;best=i;}}
        return best;
    }
    boolean hitsStick(float px,float py){float dx=px-stickX,dy=py-stickY,r=stickRadius*1.35f;return dx*dx+dy*dy<=r*r;}
}
