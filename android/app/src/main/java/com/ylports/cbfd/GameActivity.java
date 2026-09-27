package com.ylports.cbfd;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.os.Bundle;
import android.view.ViewGroup;
import android.widget.Toast;
import java.io.File;
import java.util.ArrayList;
import org.libsdl.app.SDLActivity;

public final class GameActivity extends SDLActivity {
    private TouchControls touch;
    private boolean returnToLauncher;
    private android.app.AlertDialog exitDialog;

    static native void nativeInput(int buttons,float x,float y);
    private static native void nativeRequestQuit();
    private static native String nativeCaptureDiagnostics();

    @Override protected String[] getLibraries(){
        return new String[]{"c++_shared","SDL2","main"};
    }
    @Override protected String getMainSharedObject(){ return "libmain.so"; }

    @Override public void loadLibraries(){
        for(String library:getLibraries()){
            StartupDiagnostics.log("Loading "+library);
            try{System.loadLibrary(library);}
            catch(UnsatisfiedLinkError|RuntimeException error){
                StartupDiagnostics.failure("Loading "+library,error);
                throw error;
            }
        }
        StartupDiagnostics.log("All native libraries loaded");
    }

    @Override protected String[] getArguments(){
        ArrayList<String> args=new ArrayList<>();
        args.add("--rom");
        args.add(new File(getFilesDir(),"roms/"+RomImporter.ROM_NAME).getAbsolutePath());
        args.add("--data");
        args.add(new File(getFilesDir(),"state").getAbsolutePath());
        for(String option:PcSettings.nativeArgs(this)) args.add(option);
        return args.toArray(new String[0]);
    }

    @Override protected void onCreate(Bundle state){
        StartupDiagnostics.log("GameActivity.onCreate: PC 0.1.2 direct port");
        super.onCreate(state);
        if(isFinishing()||mBrokenLibraries) return;
        Immersive.apply(this);
        if(PcSettings.touch(this)){
            touch=new TouchControls(this,this::copyDiagnostics);
            ViewGroup content=findViewById(android.R.id.content);
            content.addView(touch,new ViewGroup.LayoutParams(-1,-1));
        }
    }

    private void copyDiagnostics(){
        new Thread(()->{
            StartupDiagnostics.capture(nativeCaptureDiagnostics());
            String report=StartupDiagnostics.report();
            runOnUiThread(()->{
                if(isFinishing()||isDestroyed()) return;
                ClipboardManager clipboard=(ClipboardManager)getSystemService(CLIPBOARD_SERVICE);
                if(clipboard!=null) clipboard.setPrimaryClip(ClipData.newPlainText("Conker: diagnostic",report));
                Toast.makeText(this,"Diagnostic copied",Toast.LENGTH_SHORT).show();
            });
        },"ConkerDiagnostic").start();
    }

    @Override public void onWindowFocusChanged(boolean focused){
        super.onWindowFocusChanged(focused);
        if(focused) Immersive.apply(this);
        else if(touch!=null) touch.releaseAll();
    }

    @Override protected void onPause(){
        if(touch!=null) touch.releaseAll();
        super.onPause();
    }

    @Override public boolean dispatchKeyEvent(android.view.KeyEvent event){
        if(event.getKeyCode()==android.view.KeyEvent.KEYCODE_BACK){
            if(event.getAction()==android.view.KeyEvent.ACTION_UP&&!event.isCanceled()) onBackPressed();
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    @Override public void onBackPressed(){
        if(mBrokenLibraries){finish();return;}
        if(exitDialog!=null&&exitDialog.isShowing()) return;
        if(touch!=null) touch.releaseAll();

        exitDialog=new android.app.AlertDialog.Builder(this)
            .setTitle("Conker's Bad Fur Day: Recompiled")
            .setItems(new String[]{"Continue","Settings","Controls","Copy diagnostic","Return to launcher","Exit"},(dialog,which)->{
                if(which==1) PcSettings.showGraphics(this,()->
                    Toast.makeText(this,"Graphics changes apply on the next launch.",Toast.LENGTH_SHORT).show());
                else if(which==2) PcSettings.showControls(this,()->
                    Toast.makeText(this,"Control changes apply on the next launch.",Toast.LENGTH_SHORT).show());
                else if(which==3) copyDiagnostics();
                else if(which==4){returnToLauncher=true;nativeRequestQuit();}
                else if(which==5) nativeRequestQuit();
                Immersive.apply(this);
            }).create();
        exitDialog.show();
    }

    @Override protected void onDestroy(){
        if(touch!=null) touch.releaseAll();
        super.onDestroy();
        StartupDiagnostics.gameDestroyed(mBrokenLibraries);
        if(returnToLauncher&&isFinishing()&&!isChangingConfigurations()){
            startActivity(new android.content.Intent(this,LauncherActivity.class)
                .putExtra(LauncherActivity.SHOW_MENU,true)
                .addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK|android.content.Intent.FLAG_ACTIVITY_CLEAR_TOP));
        }
        if(isFinishing()&&!isChangingConfigurations()) android.os.Process.killProcess(android.os.Process.myPid());
    }
}
