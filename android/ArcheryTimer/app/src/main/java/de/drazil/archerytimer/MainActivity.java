package de.drazil.archerytimer;

import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.util.Log;

import de.drazil.archerytimer.databinding.ActivityMainBinding;
import de.drazil.archerytimer.udp.UDPSender;
import de.drazil.archerytimer.udp.UDPServer;

public class MainActivity extends AppCompatActivity {

    ActivityMainBinding binding;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        SharedPreferences sharedPreferences = getPreferences(Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = sharedPreferences.edit();
        editor.putInt(getString(R.string.groupStore), 1);
        editor.putInt(getString(R.string.passesCountStore), 10);
        editor.putFloat(getString(R.string.volumeStore), 10.0f);
        editor.putInt(getString(R.string.arrowCountStore), 3);
        editor.putInt(getString(R.string.arrowTimeStore), 30);
        editor.putInt(getString(R.string.warnTimeStore), 30);
        editor.putInt(getString(R.string.reshootArrowCountStore), 0);
        editor.putInt(getString(R.string.prepareTimeStore), 10);
        editor.putInt(getString(R.string.shootInTimeStore), 45);
        editor.putBoolean(getString(R.string.flashingPrepareLightStore), true);
        editor.apply();
        UDPServer.start();
        if (savedInstanceState == null) {
            displayFragment(HomeFragment.class, "home");
        }
        binding.bottomNavigationView.setOnItemSelectedListener(item -> {
            if (item.getItemId() == R.id.home) {
                displayFragment(HomeFragment.class, "home");
            } else if (item.getItemId() == R.id.tournament) {
                displayFragment(TournamentFragment.class, "tournament");
            } else if (item.getItemId() == R.id.settings) {
                displayFragment(SettingsFragment.class, "setup");
            } else if (item.getItemId() == R.id.state) {
                displayFragment(StateFragment.class, "state");
            }
            return true;
        });
    }

    protected void displayFragment(Class<? extends Fragment> fragmentClass, String tagName) {
        FragmentTransaction ft = getSupportFragmentManager().beginTransaction();
        FragmentManager fm = getSupportFragmentManager();
        try {
            for (Fragment fragment : fm.getFragments()) {
                if (!fragment.getTag().equalsIgnoreCase(tagName)) {
                    ft.hide(fragment);
                }
            }
            Fragment f = fm.findFragmentByTag(tagName);
            if (f == null) {
                f = fragmentClass.newInstance();
                ft.add(R.id.frame_layout, f, tagName);

                UDPSender.sendConfiguration(getPreferences(Context.MODE_PRIVATE), System.currentTimeMillis());
            } else {
                ft.show(f);
            }
            ft.commit();
            UDPSender.controlDisplay(((IRemoteView) f).getCurrentView(), null, System.currentTimeMillis());
        } catch (Exception ex) {
            Log.e("Error", ex.toString());
        }
    }
}