package com.facebook.socialvr.activity;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import com.melonloader.helpers.InjectionHelper;
import com.unity3d.player.UnityPlayerActivity;
import lanchon.dexpatcher.annotation.DexEdit;
import lanchon.dexpatcher.annotation.DexPrepend;

@DexEdit
public class SocialVrActivity extends UnityPlayerActivity {
    @DexPrepend
    @Override protected void onCreate(Bundle bundle) { InjectionHelper.Initialize(this); }
}