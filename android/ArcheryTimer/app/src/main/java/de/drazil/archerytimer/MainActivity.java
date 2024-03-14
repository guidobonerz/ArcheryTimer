package de.drazil.archerytimer;

import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;

import de.drazil.archerytimer.databinding.ActivityMainBinding;
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
        editor.putString(getString(R.string.modeStore), "AB");
        editor.putInt(getString(R.string.passesCountStore), 1);
        editor.putFloat(getString(R.string.volumeStore), 0.3f);
        editor.putInt(getString(R.string.arrowCountStore), 3);
        editor.putInt(getString(R.string.arrowTimeStore), 30);
        editor.putInt(getString(R.string.warnTimeStore), 30);
        editor.putInt(getString(R.string.prepareTimeStore), 10);
        editor.apply();
        UDPServer.start();


        if (savedInstanceState == null) {
            replaceFragment(new HomeFragment());
        }

        binding.bottomNavigationView.setOnItemSelectedListener(item -> {
            if (item.getItemId() == R.id.home) {
                replaceFragment(new HomeFragment());
            } else if (item.getItemId() == R.id.tournament) {
                replaceFragment(new TournamentFragment());
            } else if (item.getItemId() == R.id.settings) {
                replaceFragment(new SettingsFragment());
            } else if (item.getItemId() == R.id.relax) {
                replaceFragment(new RelaxFragment());
            }
            return true;
        });
    }

    private void replaceFragment(Fragment fragment) {
        FragmentManager fragmentManager = getSupportFragmentManager();
        FragmentTransaction fragmentTransaction = fragmentManager.beginTransaction();
        fragmentTransaction.replace(R.id.frame_layout, fragment);
        fragmentTransaction.commit();
    }
}