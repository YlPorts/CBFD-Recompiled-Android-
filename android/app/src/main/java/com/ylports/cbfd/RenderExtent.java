package com.ylports.cbfd;
/** Buffer size only. The view/controls retain the physical resolution and full aspect. */
final class RenderExtent {
    static int[] fit(int w,int h){
        if(w<2||h<2)throw new IllegalArgumentException("Invalid surface size");
        double f=Math.min(1,720.0/Math.min(w,h));
        return new int[]{Math.max(2,(int)Math.floor(w*f/2)*2),Math.max(2,(int)Math.floor(h*f/2)*2)};
    }
    private RenderExtent(){}
}
