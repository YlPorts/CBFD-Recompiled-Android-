package com.ylports.cbfd;

import android.content.Context;
import java.io.File;
import java.util.ArrayList;
import org.json.JSONArray;
import org.json.JSONObject;

final class LauncherMods {
    static final class Mod {
        final String id,name,description,version,file;
        final boolean enabled,toggleable,defaultEnabled,customGamemode;
        Mod(JSONObject o){
            id=o.optString("id");
            name=o.optString("name",id);
            description=o.optString("description","");
            version=o.optString("version","");
            file=o.optString("file","");
            enabled=o.optBoolean("enabled");
            toggleable=o.optBoolean("toggleable");
            defaultEnabled=o.optBoolean("defaultEnabled");
            customGamemode=o.optBoolean("customGamemode");
        }
    }
    private static boolean loaded;
    private static synchronized void ensureLoaded(){
        if(loaded)return;
        System.loadLibrary("c++_shared");
        System.loadLibrary("SDL2");
        System.loadLibrary("main");
        loaded=true;
    }
    private static native String nativeList(String statePath);
    private static native boolean nativeSetEnabled(String statePath,String id,boolean enabled);
    private static String state(Context c){return new File(c.getFilesDir(),"state").getAbsolutePath();}

    static ArrayList<Mod> list(Context c)throws Exception{
        ensureLoaded();
        JSONObject error=null;
        String json=nativeList(state(c));
        ArrayList<Mod> out=new ArrayList<>();
        if(json==null||json.isEmpty())return out;
        if(json.charAt(0)=='{'){
            error=new JSONObject(json);
            throw new Exception(error.optString("error","Could not scan mods."));
        }
        JSONArray a=new JSONArray(json);
        for(int i=0;i<a.length();i++)out.add(new Mod(a.getJSONObject(i)));
        return out;
    }
    static boolean setEnabled(Context c,String id,boolean enabled){
        ensureLoaded();
        return nativeSetEnabled(state(c),id,enabled);
    }
    private LauncherMods(){}
}
