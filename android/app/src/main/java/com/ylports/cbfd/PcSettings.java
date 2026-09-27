package com.ylports.cbfd;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.SharedPreferences;
import android.text.InputType;
import android.widget.EditText;
import android.widget.Toast;

/** Android surface for the same visible graphics options/defaults as PC RecompFrontend 0.1.2. */
final class PcSettings {
    private static final String PREFS="pc012_graphics";
    private static SharedPreferences p(Context c){ return c.getSharedPreferences(PREFS,Context.MODE_PRIVATE); }
    private static int get(Context c,String k,int d){ return p(c).getInt(k,d); }
    private static void put(Context c,String k,int v){ p(c).edit().putInt(k,v).apply(); }

    static int resolution(Context c){ return get(c,"resolution",2); }      // Original, Original2x, Auto
    static int downsampling(Context c){ return get(c,"downsampling",0); }  // Off, 2, 4
    static int aspect(Context c){ return get(c,"aspect",1); }              // Original, Expand
    static int refresh(Context c){ return get(c,"refresh",1); }            // Original, Display, Manual
    static int refreshValue(Context c){ return Math.max(20,Math.min(240,get(c,"refreshValue",60))); }
    static int msaa(Context c){ return get(c,"msaa",1); }                  // None, 2X, 4X
    static int hud(Context c){ return get(c,"hud",1); }                    // Original, 16:9, Expand
    static int highPrecision(Context c){ return get(c,"hpfb",2); }         // Auto, On, Off
    static boolean touch(Context c){ return p(c).getBoolean("touch",true); }
    static void setTouch(Context c,boolean value){ p(c).edit().putBoolean("touch",value).apply(); }

    static String[] nativeArgs(Context c){
        return new String[]{
            "--resolution",Integer.toString(resolution(c)),
            "--downsampling",Integer.toString(downsampling(c)),
            "--aspect",Integer.toString(aspect(c)),
            "--refresh",Integer.toString(refresh(c)),
            "--refresh-value",Integer.toString(refreshValue(c)),
            "--msaa",Integer.toString(msaa(c)),
            "--hud",Integer.toString(hud(c)),
            "--high-precision",Integer.toString(highPrecision(c))
        };
    }

    static float preferredRefreshRate(Context c){
        return refresh(c)==2 ? refreshValue(c) : 0f;
    }

    private static String name(String[] names,int index){
        return index>=0&&index<names.length?names[index]:"?";
    }
    static String summary(Context c){
        String[] res={"Original","Original 2x","Auto"};
        String[] ar={"Original","Expand"};
        String[] rr={"Original","Display","Manual"};
        String[] aa={"None","2X","4X"};
        String[] hp={"Auto","On","Off"};
        String[] h={"Original","16:9","Expand"};
        String refresh=name(rr,refresh(c))+(refresh(c)==2?" "+refreshValue(c)+" FPS":"");
        return "Resolution: "+name(res,resolution(c))+" · Aspect: "+name(ar,aspect(c))+
            " · Framerate: "+refresh+" · MSAA: "+name(aa,Math.min(msaa(c),2))+
            " · HUD: "+name(h,hud(c))+" · Vulkan";
    }

    static void showGraphics(Activity a,Runnable changed){
        String[] res={"Original","Original 2x","Auto"};
        String[] ds={"Off","2x","4x"};
        String[] ar={"Original","Expand"};
        String[] rr={"Original","Display","Manual"};
        String[] aa={"None","2X","4X"};
        String[] hudNames={"Original","16:9","Expand"};
        String[] hp={"Auto","On","Off"};
        int dsIndex=downsampling(a)==2?1:downsampling(a)==4?2:0;
        String[] rows={
            "Resolution · "+name(res,resolution(a)),
            "Downsampling Quality · "+name(ds,dsIndex),
            "Aspect Ratio · "+name(ar,aspect(a)),
            "Framerate · "+name(rr,refresh(a))+(refresh(a)==2?" "+refreshValue(a):""),
            "MS Anti-Aliasing · "+name(aa,Math.min(msaa(a),2)),
            "HUD Placement · "+name(hudNames,hud(a)),
            "High Precision Framebuffer · "+name(hp,highPrecision(a)),
            "Graphics API · Vulkan (Android)"
        };
        new AlertDialog.Builder(a).setTitle("Settings · Graphics")
            .setItems(rows,(d,which)->{
                switch(which){
                    case 0: choose(a,"Resolution",res,resolution(a),"resolution",new int[]{0,1,2},changed); break;
                    case 1: choose(a,"Downsampling Quality",ds,dsIndex,"downsampling",new int[]{0,2,4},changed); break;
                    case 2: choose(a,"Aspect Ratio",ar,aspect(a),"aspect",new int[]{0,1},changed); break;
                    case 3: chooseRefresh(a,changed); break;
                    case 4: choose(a,"MS Anti-Aliasing",aa,Math.min(msaa(a),2),"msaa",new int[]{0,1,2},changed); break;
                    case 5: choose(a,"HUD Placement",hudNames,hud(a),"hud",new int[]{0,1,2},changed); break;
                    case 6: choose(a,"High Precision Framebuffer",hp,highPrecision(a),"hpfb",new int[]{0,1,2},changed); break;
                    default: Toast.makeText(a,"PC 0.1.2 usa RT64; Android usa su backend Vulkan.",Toast.LENGTH_SHORT).show();
                }
            }).setNegativeButton("Close",null).show();
    }

    private static void choose(Activity a,String title,String[] labels,int selected,String key,int[] values,Runnable changed){
        new AlertDialog.Builder(a).setTitle(title).setSingleChoiceItems(labels,selected,(dialog,which)->{
            put(a,key,values[which]); dialog.dismiss(); if(changed!=null)changed.run();
        }).setNegativeButton("Cancel",null).show();
    }

    private static void chooseRefresh(Activity a,Runnable changed){
        String[] labels={"Original","Display","Manual"};
        new AlertDialog.Builder(a).setTitle("Framerate").setSingleChoiceItems(labels,refresh(a),(dialog,which)->{
            dialog.dismiss();
            put(a,"refresh",which);
            if(which==2) manualRefresh(a,changed); else if(changed!=null)changed.run();
        }).setNegativeButton("Cancel",null).show();
    }

    private static void manualRefresh(Activity a,Runnable changed){
        EditText value=new EditText(a);
        value.setInputType(InputType.TYPE_CLASS_NUMBER);
        value.setText(Integer.toString(refreshValue(a)));
        value.setSelectAllOnFocus(true);
        new AlertDialog.Builder(a).setTitle("Manual framerate (20–240)")
            .setView(value).setPositiveButton("Save",(d,w)->{
                int fps=60;
                try{fps=Integer.parseInt(value.getText().toString());}catch(NumberFormatException ignored){}
                put(a,"refreshValue",Math.max(20,Math.min(240,fps)));
                if(changed!=null)changed.run();
            }).setNegativeButton("Cancel",null).show();
    }

    static void showControls(Activity a,Runnable changed){
        String state=touch(a)?"On":"Off";
        new AlertDialog.Builder(a).setTitle("Controls")
            .setItems(new String[]{"Touch controls · "+state,
                "Controller mapping",
                "Reset touch controls"},(d,which)->{
                if(which==0){ setTouch(a,!touch(a)); if(changed!=null)changed.run(); }
                else if(which==1) new AlertDialog.Builder(a).setTitle("Controller")
                    .setMessage("Left stick: N64 stick\nRight stick: C buttons\nA/B: A/B\nX or LT: Z\nLB/RB: L/R\nStart: Start\nD-pad: N64 D-pad\nRumble Pak supported.")
                    .setPositiveButton("OK",null).show();
                else { setTouch(a,true); if(changed!=null)changed.run(); }
            }).setNegativeButton("Close",null).show();
    }

    static void resetGraphics(Context c){
        p(c).edit().remove("resolution").remove("downsampling").remove("aspect")
            .remove("refresh").remove("refreshValue").remove("msaa").remove("hud").remove("hpfb").apply();
    }
    private PcSettings(){}
}
