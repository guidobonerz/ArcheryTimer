package de.drazil.archerytimer;

import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.fragment.app.Fragment;

import org.json.JSONObject;

import de.drazil.archerytimer.udp.Sender;


public class RelaxFragment extends Fragment {

    public RelaxFragment() {
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
            payload.put("name", "relax");
            Sender.broadcastJSON(payload.toString());
        } catch (Exception ex) {
            Log.e("Error", ex.getMessage());
        }
        return inflater.inflate(R.layout.fragment_relax, container, false);
    }

}