/*
 * Copyright (C) 2012 The CyanogenMod Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.cyanogenmod.settings.device;

import android.app.Dialog;
import android.app.AlertDialog;
import android.app.AlertDialog.Builder;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.content.SharedPreferences;
import android.media.AudioManager;
import android.os.Bundle;
import android.preference.CheckBoxPreference;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceFragment;
import android.preference.PreferenceManager;
import android.preference.PreferenceScreen;
import android.telephony.PhoneStateListener;
import android.telephony.TelephonyManager;
import android.util.Log;

import com.cyanogenmod.settings.device.R;

public class RadioFragmentActivity extends PreferenceFragment {

    private static final String PREF_ENABLED = "1";
    private static final String TAG = "GalaxyS3Settings_Radio";

    private static PhoneStateListener mPhoneStateListener = null;
    private static AlertDialog alertDialog = null;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        addPreferencesFromResource(R.xml.radio_preferences);

        PreferenceScreen prefSet = getPreferenceScreen();

        // HD-Voice handling
        mPhoneStateListener = new PhoneStateListener() {
            @Override
            public void onCallStateChanged (int state, String incomingNumber) {
                AudioManager am = (AudioManager) getActivity().getSystemService(Context.AUDIO_SERVICE);

                if (state == TelephonyManager.CALL_STATE_OFFHOOK) {
                    TelephonyManager tm = (TelephonyManager) getActivity().getSystemService(Context.TELEPHONY_SERVICE);

                    // set HD-Voice audio parameter
                    if (tm.getNetworkType() == TelephonyManager.NETWORK_TYPE_UMTS || tm.getNetworkType() == TelephonyManager.NETWORK_TYPE_HSPA || tm.getNetworkType() == TelephonyManager.NETWORK_TYPE_HSPAP) {
                        Log.i(TAG, "Setting HD-Voice parameter");
                        am.setParameters ("wb_amr=on");
                    }
                } else {
                    // unset HD-Voice audio parameter
                    Log.i(TAG, "Removing HD-Voice parameter");
                    am.setParameters ("wb_amr=off");
                }
            }
        };
    }

    @Override
    public boolean onPreferenceTreeClick(PreferenceScreen preferenceScreen, final Preference preference) {
        String boxValue;
        String key = preference.getKey();

        Log.w(TAG, "key: " + key);

        if (key.compareTo(DeviceSettings.KEY_HDVOICE) == 0) {
            final TelephonyManager tm = (TelephonyManager) getActivity().getSystemService(Context.TELEPHONY_SERVICE);

            if (((CheckBoxPreference)preference).isChecked()) {
                /* Display the warning dialog */
                alertDialog = new AlertDialog.Builder(getActivity()).create();
                alertDialog.setTitle(R.string.hdvoice_warning_title);
                alertDialog.setMessage(getResources().getString(R.string.hdvoice_warning));
                alertDialog.setButton(DialogInterface.BUTTON_POSITIVE,
                        getResources().getString(com.android.internal.R.string.ok),
                        new DialogInterface.OnClickListener() {
                            public void onClick(DialogInterface dialog, int which) {
                                // listen for call state
                                Log.i(TAG, "HD-Voice feature enabled, listening...");
                                tm.listen(mPhoneStateListener, PhoneStateListener.LISTEN_CALL_STATE);
                                return;
                            }
                        });
                alertDialog.setButton(DialogInterface.BUTTON_NEGATIVE,
                        getResources().getString(com.android.internal.R.string.cancel),
                        new DialogInterface.OnClickListener() {
                            public void onClick(DialogInterface dialog, int which) {
                                // listen for nothing
                                ((CheckBoxPreference)preference).setChecked(false);
                                Log.i(TAG, "HD-Voice feature disabled.");
                                tm.listen(mPhoneStateListener, PhoneStateListener.LISTEN_NONE);
                                return;
                            }
                        });

                alertDialog.show();
            } else {
                // listen for nothing
                Log.i(TAG, "HD-Voice feature disabled.");
                tm.listen(mPhoneStateListener, PhoneStateListener.LISTEN_NONE);
            }
        }

        return true;
    }

    public static boolean isSupported(String FILE) {
        return Utils.fileExists(FILE);
    }

    public static void restore(Context context) {
        SharedPreferences sharedPrefs = PreferenceManager.getDefaultSharedPreferences(context);

        if (sharedPrefs.getBoolean(DeviceSettings.KEY_HDVOICE, true)) {
            TelephonyManager tm = (TelephonyManager) context.getSystemService(Context.TELEPHONY_SERVICE);
            tm.listen(mPhoneStateListener, PhoneStateListener.LISTEN_CALL_STATE);
        }
    }
}
