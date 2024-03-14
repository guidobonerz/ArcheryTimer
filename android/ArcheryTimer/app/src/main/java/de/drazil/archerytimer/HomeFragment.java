package de.drazil.archerytimer;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;

import androidx.fragment.app.Fragment;

import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.json.JSONObject;

import java.net.InetAddress;
import java.util.HashMap;
import java.util.Map;

import de.drazil.archerytimer.udp.Sender;


public class HomeFragment extends Fragment {


    public HomeFragment() {
        // Required empty public constructor
    }


    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {

        JSONObject payload = new JSONObject();
        try {
            payload.put("name", "home");
            Sender.broadcastJSON(payload);
        } catch (Exception ex) {
            Log.e("Error", ex.getMessage());
        }
        return inflater.inflate(R.layout.fragment_home, container, false);
    }
}