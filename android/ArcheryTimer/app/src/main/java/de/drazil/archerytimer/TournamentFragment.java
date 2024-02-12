package de.drazil.archerytimer;

import android.content.Context;
import android.content.SharedPreferences;
import android.nfc.Tag;
import android.os.Bundle;

import androidx.fragment.app.Fragment;

import android.os.StrictMode;
import android.preference.PreferenceManager;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.RadioButton;
import android.widget.RadioGroup;

import org.json.JSONObject;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;

import de.drazil.archerytimer.udp.Sender;

/**
 * A simple {@link Fragment} subclass.
 * Use the {@link TournamentFragment#newInstance} factory method to
 * create an instance of this fragment.
 */
public class TournamentFragment extends Fragment {

    private boolean start = true;

    public TournamentFragment() {

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
            payload.put("name", "tournament");
            Sender.broadcastJSON(payload.toString());
        } catch (Exception ex) {
            Log.e("Error", ex.getMessage());
        }

        View rootView = inflater.inflate(R.layout.fragment_tournament, container, false);

        ImageButton startButton = (ImageButton) rootView.findViewById(R.id.start);
        startButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {

                if (start) {
                    startButton.setImageResource(R.drawable.baseline_pause_24);

                } else {
                    startButton.setImageResource(R.drawable.baseline_play_arrow_24);

                }
                JSONObject payload= new JSONObject();
                try {
                    payload.put("name", start ? "start" : "pause");
                    Sender.broadcastJSON(payload.toString());
                } catch (Exception ex) {
                    Log.e("Error", ex.getMessage());
                }
                start = !start;



            }
        });
        ImageButton stopButton = (ImageButton) rootView.findViewById(R.id.stop);
        stopButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {

                JSONObject payload = new JSONObject();
                try {
                    start = true;
                    startButton.setImageResource(R.drawable.baseline_play_arrow_24);
                    payload.put("name", "stop");
                    Sender.broadcastJSON(payload.toString());
                } catch (Exception ex) {
                    Log.e("Error", ex.getMessage());
                }

            }
        });


        ImageButton emergencyButton = (ImageButton) rootView.findViewById(R.id.emergency);
        emergencyButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {


                JSONObject payload = new JSONObject();
                try {
                    payload.put("name", "emergency");
                    Sender.broadcastJSON(payload.toString());
                } catch (Exception ex) {
                    Log.e("Error", ex.getMessage());
                }

            }
        });

        ImageButton resetButton = (ImageButton) rootView.findViewById(R.id.reset);
        resetButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {

                JSONObject payload = new JSONObject();
                try {
                    start = true;
                    startButton.setImageResource(R.drawable.baseline_play_arrow_24);
                    payload.put("name", "reset");
                    Sender.broadcastJSON(payload.toString());
                } catch (Exception ex) {
                    Log.e("Error", ex.getMessage());
                }

            }
        });
        return rootView;
    }


}