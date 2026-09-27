package com.ylports.cbfd;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.view.Gravity;
import android.view.View;
import android.widget.*;
import java.io.*;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.Arrays;
import java.util.concurrent.*;

/** Android presentation of the PC 0.1.2 launcher flow. */
public final class LauncherActivity extends Activity {
    static final String SHOW_MENU="com.ylports.cbfd.SHOW_MENU";
    private static final int PICK_ROM=10;
    private static final int PICK_MOD=11;

    private final ExecutorService worker=Executors.newSingleThreadExecutor();
    private TextView status,graphicsSummary;
    private Button start,version,addRom;
    private File[] available=new File[0];
    private boolean busy;

    private File romDirectory(){return new File(getFilesDir(),"roms");}
    private File modsDirectory(){return new File(getFilesDir(),"state/mods");}

    @Override public void onCreate(Bundle state){
        super.onCreate(state);
        StartupDiagnostics.log("LauncherActivity.onCreate PC 0.1.2 direct port");
        if(StartupDiagnostics.needsRecovery()){showInterruptedRun();return;}
        showLauncher();
    }

    private LinearLayout column(int paddingDp){
        LinearLayout box=new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setGravity(Gravity.CENTER_HORIZONTAL);
        int p=Math.round(paddingDp*getResources().getDisplayMetrics().density);
        box.setPadding(p,p,p,p);
        return box;
    }

    private Button menuButton(String text){
        Button b=new Button(this);
        b.setText(text);
        b.setAllCaps(false);
        b.setTextSize(16);
        LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(
            Math.round(420*getResources().getDisplayMetrics().density),
            LinearLayout.LayoutParams.WRAP_CONTENT);
        lp.setMargins(0,5,0,5);
        b.setLayoutParams(lp);
        return b;
    }

    private void showLauncher(){
        LinearLayout content=column(20);

        try(InputStream image=getAssets().open("pc/thumbnail.png")){
            android.graphics.Bitmap bitmap=android.graphics.BitmapFactory.decodeStream(image);
            if(bitmap!=null){
                ImageView thumbnail=new ImageView(this);
                thumbnail.setImageBitmap(bitmap);
                thumbnail.setAdjustViewBounds(true);
                thumbnail.setScaleType(ImageView.ScaleType.FIT_CENTER);
                LinearLayout.LayoutParams imageParams=new LinearLayout.LayoutParams(
                    Math.round(360*getResources().getDisplayMetrics().density),
                    Math.round(150*getResources().getDisplayMetrics().density));
                imageParams.setMargins(0,0,0,8);
                thumbnail.setLayoutParams(imageParams);
                content.addView(thumbnail);
            }
        }catch(IOException ignored){}

        TextView title=new TextView(this);
        title.setText("CONKER'S BAD FUR DAY\nRECOMPILED");
        title.setTextSize(29);
        title.setGravity(Gravity.CENTER);
        title.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);
        content.addView(title);

        TextView build=new TextView(this);
        build.setText("PC 0.1.2 · direct Android port · RT64/Vulkan");
        build.setGravity(Gravity.CENTER);
        build.setTextSize(13);
        content.addView(build);

        status=new TextView(this);
        status.setGravity(Gravity.CENTER);
        status.setPadding(0,10,0,12);
        content.addView(status);

        start=menuButton(RomVersions.hasRom(romDirectory())?"Start Game":"Load ROM");
        start.setOnClickListener(v->{
            if(busy)return;
            if(RomVersions.hasRom(romDirectory()))launch();
            else pick(PICK_ROM);
        });
        content.addView(start);

        version=menuButton("Version");
        version.setOnClickListener(v->showVersions());
        content.addView(version);

        addRom=menuButton("Add ROM");
        addRom.setOnClickListener(v->{if(!busy)pick(PICK_ROM);});
        content.addView(addRom);

        Button controls=menuButton("Controls");
        controls.setOnClickListener(v->PcSettings.showControls(this,this::refreshSettingsText));
        content.addView(controls);

        Button settings=menuButton("Settings");
        settings.setOnClickListener(v->PcSettings.showGraphics(this,this::refreshSettingsText));
        content.addView(settings);

        Button mods=menuButton("Mods");
        mods.setOnClickListener(v->showMods());
        content.addView(mods);

        Button exit=menuButton("Exit");
        exit.setOnClickListener(v->{finishAndRemoveTask();});
        content.addView(exit);

        graphicsSummary=new TextView(this);
        graphicsSummary.setGravity(Gravity.CENTER);
        graphicsSummary.setTextSize(12);
        graphicsSummary.setPadding(0,12,0,0);
        content.addView(graphicsSummary);
        refreshSettingsText();

        ScrollView scroll=new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.addView(content);
        setContentView(scroll);
        Immersive.apply(this);
        setBusy(true);
        worker.submit(this::refreshVersions);
    }

    private void refreshSettingsText(){
        if(graphicsSummary!=null)graphicsSummary.setText(PcSettings.summary(this));
    }

    private void setBusy(boolean value){
        busy=value;
        if(start!=null)start.setEnabled(!value);
        if(version!=null)version.setEnabled(!value&&available.length>1);
        if(addRom!=null)addRom.setEnabled(!value);
    }

    private void refreshVersions(){
        try{
            File[] found=RomVersions.list(romDirectory());
            String current=RomVersions.currentLabel(romDirectory());
            runOnUiThread(()->{
                if(isDestroyed()||isFinishing())return;
                available=found;
                boolean has=RomVersions.hasRom(romDirectory());
                start.setText(has?"Start Game":"Load ROM");
                version.setVisibility(has?View.VISIBLE:View.GONE);
                addRom.setVisibility(has?View.VISIBLE:View.GONE);
                version.setText("Version: "+current);
                status.setText(has?current+"\nUS Original or asset-only US ROM hack":
                    "Select your legally obtained US ROM to begin.");
                setBusy(false);
            });
        }catch(IOException error){showError(error);}
    }

    private void showVersions(){
        if(busy||available.length==0)return;
        String[] labels=new String[available.length];
        for(int i=0;i<labels.length;i++)labels[i]=RomVersions.label(available[i]);
        new AlertDialog.Builder(this).setTitle("Version").setItems(labels,(d,index)->{
            setBusy(true);
            File selected=available[index];
            worker.submit(()->{
                try{RomVersions.select(selected,romDirectory());refreshVersions();}
                catch(IOException error){showError(error);}
            });
        }).setNegativeButton("Cancel",null).show();
    }

    private void pick(int request){
        Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        startActivityForResult(intent,request);
    }

    @Override protected void onActivityResult(int request,int result,Intent data){
        super.onActivityResult(request,result,data);
        if(result!=RESULT_OK||data==null||data.getData()==null)return;
        Uri uri=data.getData();
        if(request==PICK_ROM){
            if(busy)return;
            setBusy(true);
            status.setText("Checking ROM…");
            worker.submit(()->{
                try(InputStream stream=getContentResolver().openInputStream(uri)){
                    RomImporter.importRom(stream,romDirectory());
                    refreshVersions();
                }catch(IOException|SecurityException error){showError(error);}
            });
        }else if(request==PICK_MOD){
            worker.submit(()->importMod(uri));
        }
    }

    private String displayName(Uri uri){
        try(Cursor cursor=getContentResolver().query(uri,null,null,null,null)){
            if(cursor!=null&&cursor.moveToFirst()){
                int index=cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if(index>=0)return cursor.getString(index);
            }
        }catch(RuntimeException ignored){}
        return "imported-"+System.currentTimeMillis()+".nrm";
    }

    private void importMod(Uri uri){
        File dir=modsDirectory();
        if(!dir.isDirectory()&&!dir.mkdirs()){showError(new IOException("Could not create mods folder."));return;}
        String name=displayName(uri);
        if(name==null||!name.toLowerCase(java.util.Locale.ROOT).endsWith(".nrm")){
            showError(new IOException("PC 0.1.2 mods must use the .nrm format."));return;
        }
        name=name.replaceAll("[^A-Za-z0-9._ -]","_");
        File target=new File(dir,name),temp=new File(dir,"."+name+".tmp");
        try(InputStream in=getContentResolver().openInputStream(uri);
            OutputStream out=new FileOutputStream(temp)){
            if(in==null)throw new IOException("Could not open mod.");
            byte[] buffer=new byte[65536];
            for(int n;(n=in.read(buffer))>=0;)out.write(buffer,0,n);
            out.flush();
            Files.move(temp.toPath(),target.toPath(),StandardCopyOption.REPLACE_EXISTING,StandardCopyOption.ATOMIC_MOVE);
            runOnUiThread(()->{
                Toast.makeText(this,"Mod imported. PC runtime will scan it on launch.",Toast.LENGTH_LONG).show();
                showMods();
            });
        }catch(IOException|SecurityException error){
            temp.delete();
            showError(error);
        }
    }

    private void showMods(){
        final ProgressDialog progress=ProgressDialog.show(this,"Mods","Scanning PC 0.1.2 mods…",true,false);
        worker.submit(()->{
            try{
                java.util.ArrayList<LauncherMods.Mod> mods=LauncherMods.list(this);
                runOnUiThread(()->{
                    progress.dismiss();
                    String[] items=new String[mods.size()+1];
                    items[0]="Add Mod (.nrm)";
                    for(int i=0;i<mods.size();i++){
                        LauncherMods.Mod mod=mods.get(i);
                        items[i+1]=(mod.enabled?"✓ ":"○ ")+mod.name+
                            (mod.version.isEmpty()?"":" · "+mod.version);
                    }
                    new AlertDialog.Builder(this).setTitle("Mods")
                        .setItems(items,(dialog,which)->{
                            if(which==0){pick(PICK_MOD);return;}
                            LauncherMods.Mod mod=mods.get(which-1);
                            String details=(mod.description.isEmpty()?"":mod.description+"\n\n")+
                                "ID: "+mod.id+
                                (mod.version.isEmpty()?"":"\nVersion: "+mod.version)+
                                "\nState: "+(mod.enabled?"Enabled":"Disabled")+
                                (mod.customGamemode?"\nCustom game mode":"");
                            new AlertDialog.Builder(this).setTitle(mod.name)
                                .setMessage(details)
                                .setPositiveButton(mod.enabled?"Disable":"Enable",(d,w)->{
                                    boolean state=LauncherMods.setEnabled(this,mod.id,!mod.enabled);
                                    Toast.makeText(this,state?"Enabled":"Disabled",Toast.LENGTH_SHORT).show();
                                    // librecomp intentionally coalesces mods.json writes for about a second.
                                    getWindow().getDecorView().postDelayed(this::showMods,1300);
                                })
                                .setNeutralButton("Remove",(d,w)->removeModFile(mod.file))
                                .setNegativeButton("Close",null).show();
                        }).setNegativeButton("Close",null).show();
                });
            }catch(Exception error){
                runOnUiThread(()->{progress.dismiss();showError(error);});
            }
        });
    }

    private void removeModFile(String filename){
        File dir=modsDirectory();
        File exact=new File(dir,filename);
        try{
            if(!exact.getCanonicalFile().getParentFile().equals(dir.getCanonicalFile()))throw new IOException("Invalid mod path.");
        }catch(IOException error){showError(error);return;}
        if(exact.isFile()&&exact.delete()){
            Toast.makeText(this,"Mod removed",Toast.LENGTH_SHORT).show();
            showMods();
        }else{
            Toast.makeText(this,"Could not remove the mod file.",Toast.LENGTH_LONG).show();
        }
    }

    private void showError(Exception error){
        runOnUiThread(()->{
            if(isDestroyed()||isFinishing())return;
            setBusy(false);
            String message=error.getMessage()!=null?error.getMessage():"Operation failed.";
            if(status!=null)status.setText(message);
            Toast.makeText(this,message,Toast.LENGTH_LONG).show();
        });
    }

    private void showInterruptedRun(){
        LinearLayout content=column(24);
        TextView message=new TextView(this);
        message.setText("The previous game session did not finish cleanly.");
        message.setTextSize(18);
        message.setGravity(Gravity.CENTER);
        content.addView(message);

        Button copy=menuButton("Copy diagnostic");
        copy.setOnClickListener(v->{
            android.content.ClipboardManager clipboard=(android.content.ClipboardManager)getSystemService(CLIPBOARD_SERVICE);
            if(clipboard!=null)clipboard.setPrimaryClip(android.content.ClipData.newPlainText("Conker: diagnostic",StartupDiagnostics.report()));
            Toast.makeText(this,"Diagnostic copied",Toast.LENGTH_SHORT).show();
        });
        content.addView(copy);

        Button menu=menuButton("Open launcher");
        menu.setOnClickListener(v->{StartupDiagnostics.acknowledge();showLauncher();});
        content.addView(menu);
        setContentView(content);
        Immersive.apply(this);
    }

    private void launch(){
        if(!RomVersions.hasRom(romDirectory())){pick(PICK_ROM);return;}
        StartupDiagnostics.beginLaunch();
        try{
            startActivity(new Intent(this,GameActivity.class));
        }catch(RuntimeException|LinkageError error){
            StartupDiagnostics.failure("Could not open GameActivity",error);
            showInterruptedRun();
        }
    }

    @Override public void onWindowFocusChanged(boolean focused){
        super.onWindowFocusChanged(focused);
        if(focused)Immersive.apply(this);
    }

    @Override protected void onDestroy(){
        worker.shutdownNow();
        super.onDestroy();
    }
}
