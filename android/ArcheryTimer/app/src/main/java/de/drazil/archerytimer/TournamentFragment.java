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
import android.widget.RadioButton;
import android.widget.RadioGroup;

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

    // TODO: Rename parameter arguments, choose names that match
    // the fragment initialization parameters, e.g. ARG_ITEM_NUMBER
    private static final String ARG_PARAM1 = "param1";
    private static final String ARG_PARAM2 = "param2";

    // TODO: Rename and change types of parameters
    private String mParam1;
    private String mParam2;


    public TournamentFragment() {

    }

    /**
     * Use this factory method to create a new instance of
     * this fragment using the provided parameters.
     *
     * @param param1 Parameter 1.
     * @param param2 Parameter 2.
     * @return A new instance of fragment TournamentFragment.
     */
    // TODO: Rename and change types and number of parameters
    public static TournamentFragment newInstance(String param1, String param2) {
        TournamentFragment fragment = new TournamentFragment();
        Bundle args = new Bundle();
        args.putString(ARG_PARAM1, param1);
        args.putString(ARG_PARAM2, param2);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (getArguments() != null) {
            mParam1 = getArguments().getString(ARG_PARAM1);
            mParam2 = getArguments().getString(ARG_PARAM2);
        }
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        SharedPreferences sharedPreferences = getActivity().getPreferences(Context.MODE_PRIVATE);
        int prepareTime = sharedPreferences.getInt("prepareTimeStore", 0);
        int actionTime = sharedPreferences.getInt("actionTimeStore", 0);
        try {
            Sender.broadcast("!tournament:0:0:0!");
        } catch (Exception ex) {
            Log.e("Error", ex.getMessage());
        }

        View rootView = inflater.inflate(R.layout.fragment_tournament, container, false);


        Button startButton = (Button) rootView.findViewById(R.id.start);
        startButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {


                String command = String.format("!start:%s:%s:%s!", "AB",
                        String.valueOf(prepareTime), String.valueOf(actionTime));
                System.out.println(command);
                try {
                    Sender.broadcast(command);
                } catch (Exception ex) {
                    Log.e("Error", ex.getMessage());
                }


            }
        });
        Button stopButton = (Button) rootView.findViewById(R.id.stop);
        stopButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                try {
                    Sender.broadcast("!stop:0:0:0!");
                } catch (Exception ex) {
                    Log.e("Error", ex.getMessage());
                }

            }
        });

        Button pauseButton = (Button) rootView.findViewById(R.id.pause);
        pauseButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                try {
                    Sender.broadcast("!break:0:0:0!");
                } catch (Exception ex) {
                    Log.e("Error", ex.getMessage());
                }
            }
        });
        return rootView;
    }


}