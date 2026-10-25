package com.dishii.soh;

import android.app.Presentation;
import android.os.Bundle;
import android.util.Log;
import android.view.Display;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.ViewGroup;
import android.widget.FrameLayout;

/** Hosts the native HUD surface on a presentation-capable secondary display. */
public final class DualScreenHudPresentation extends Presentation {
    private static final String TAG = "DualScreenHud";

    private final MainActivity activity;
    private boolean surfaceActive;

    private final SurfaceHolder.Callback surfaceCallback = new SurfaceHolder.Callback() {
        @Override
        public void surfaceCreated(SurfaceHolder holder) {
            // surfaceChanged supplies the first usable dimensions immediately afterward.
        }

        @Override
        public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
            surfaceActive = true;
            activity.nativeSecondaryDisplaySurfaceChanged(holder.getSurface(), width, height);
        }

        @Override
        public void surfaceDestroyed(SurfaceHolder holder) {
            notifySurfaceDestroyedIfNeeded();
        }
    };

    public DualScreenHudPresentation(MainActivity activity, Display display) {
        super(activity, display);
        this.activity = activity;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        ImmersiveMode.apply(getWindow());

        FrameLayout root = new FrameLayout(getContext());
        SurfaceView surfaceView = new SurfaceView(getContext());
        surfaceView.getHolder().addCallback(surfaceCallback);
        root.addView(surfaceView, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        setContentView(root, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        activity.attachControllerOverlay(root);
    }

    @Override
    protected void onStart() {
        super.onStart();
        ImmersiveMode.apply(getWindow());
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            ImmersiveMode.apply(getWindow());
        }
    }

    void notifySurfaceDestroyedIfNeeded() {
        if (!surfaceActive) {
            return;
        }

        surfaceActive = false;
        activity.nativeSecondaryDisplaySurfaceDestroyed();
    }

    @Override
    public void dismiss() {
        notifySurfaceDestroyedIfNeeded();
        activity.attachControllerOverlayToPrimary();
        try {
            super.dismiss();
        } catch (RuntimeException exception) {
            Log.w(TAG, "Error dismissing secondary-display presentation", exception);
        }
    }
}
