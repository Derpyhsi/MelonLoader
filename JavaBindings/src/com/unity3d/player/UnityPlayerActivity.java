package com.unity3d.player;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import com.melonloader.helpers.InjectionHelper;
import lanchon.dexpatcher.annotation.DexEdit;
import lanchon.dexpatcher.annotation.DexPrepend;
import lanchon.dexpatcher.annotation.DexIgnore;

@DexIgnore
public class UnityPlayerActivity extends Activity { 

}