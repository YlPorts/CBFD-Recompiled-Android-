package com.ylports.cbfd;

import android.app.Application;

/** Capture startup errors before either activity or SDL is constructed. */
public final class ConkerApplication extends Application {
    @Override public void onCreate() {
        super.onCreate();
        StartupDiagnostics.install(this);
    }
}
