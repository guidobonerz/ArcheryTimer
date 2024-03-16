package de.drazil.archerytimer;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;

import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.RadioButton;
import android.widget.RadioGroup;

import com.google.android.material.slider.Slider;

import org.json.JSONObject;

import java.net.InetAddress;
import java.util.HashMap;
import java.util.Map;

import de.drazil.archerytimer.udp.Sender;


public class SettingsFragment extends Fragment {

    private RadioGroup radioGroup = null;
    private RadioButton group1 = null;
    private RadioButton group2 = null;

    public SettingsFragment() {
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
            payload.put("name", "setup");
            Sender.broadcastJSON(payload);
        } catch (Exception ex) {
            Log.e("Error", ex.getMessage());
        }
        return inflater.inflate(R.layout.fragment_settings, container, false);
    }

    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        SharedPreferences sharedPreferences = getActivity().getPreferences(Context.MODE_PRIVATE);


        radioGroup = (RadioGroup) view.findViewById(R.id.modeGroup);
        group1 = (RadioButton) view.findViewById(R.id.modeAB);
        group2 = (RadioButton) view.findViewById(R.id.modeABCD);
        if (sharedPreferences.getInt(getString(R.string.modeStore), 0) == 1) {
            group1.setChecked(true);
            group2.setChecked(false);
        } else {
            group1.setChecked(false);
            group2.setChecked(true);
        }

        radioGroup.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(RadioGroup radioGroup, int i) {
                int selectedId = radioGroup.getCheckedRadioButtonId();
                RadioButton radioButton = (RadioButton) radioGroup.findViewById(selectedId);
                String mode = radioButton.getText().toString();
                SharedPreferences.Editor editor = sharedPreferences.edit();
                editor.putInt(getString(R.string.modeStore), mode.equals("AB") ? 1 : 2);
                editor.apply();
                setActionTime(view, calcActionTime(sharedPreferences));
            }
        });


        ImageButton testSignalButton = (ImageButton) view.findViewById(R.id.testSignal);
        testSignalButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View _view) {
                try {
                    JSONObject payload = new JSONObject();
                    payload.put("name", "testsignal");
                    Sender.broadcastJSON(payload);
                } catch (Exception ex) {
                    Log.e("Error", ex.getMessage());
                }
            }
        });

        final EditText passesCountView = (EditText) view.findViewById(R.id.passesCount);
        int v1 = sharedPreferences.getInt(getString(R.string.passesCountStore), 0);
        passesCountView.setText(String.valueOf(v1));
        passesCountView.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence charSequence, int i, int i1, int i2) {
            }

            @Override
            public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {
                int value = 0;
                if (charSequence.length() > 0) {
                    value = Integer.valueOf(passesCountView.getText().toString());
                }
                SharedPreferences.Editor editor = sharedPreferences.edit();
                editor.putInt(getString(R.string.passesCountStore), value);
                editor.apply();
                setActionTime(view, calcActionTime(sharedPreferences));
            }

            @Override
            public void afterTextChanged(Editable editable) {
            }
        });

        final Slider volumeSelectorView = (Slider) view.findViewById(R.id.volumeSlider);
        volumeSelectorView.setValue(sharedPreferences.getFloat(getString(R.string.volumeStore), 0.5f));
        volumeSelectorView.addOnChangeListener(new Slider.OnChangeListener() {
            @Override
            public void onValueChange(@NonNull Slider slider, float value, boolean fromUser) {
                SharedPreferences.Editor editor = sharedPreferences.edit();
                editor.putFloat(getString(R.string.volumeStore), value);
                editor.apply();
            }
        });
        volumeSelectorView.addOnSliderTouchListener(new Slider.OnSliderTouchListener() {
            @Override
            public void onStartTrackingTouch(@NonNull Slider slider) {

            }

            @Override
            public void onStopTrackingTouch(@NonNull Slider slider) {
                float volume = sharedPreferences.getFloat(getString(R.string.volumeStore), 0);

                try {
                    JSONObject valuesObject = new JSONObject();
                    valuesObject.put("value", (int) (volume * 100));
                    JSONObject payload = new JSONObject();
                    payload.put("name", "volume");
                    payload.put("values", valuesObject);
                    Sender.broadcastJSON(payload);
                } catch (Exception ex) {
                    Log.e("Error", ex.getMessage());
                }
            }
        });


        final EditText prepareTimeView = (EditText) view.findViewById(R.id.prepareTime);
        prepareTimeView.setText(String.valueOf(sharedPreferences.getInt(getString(R.string.prepareTimeStore), 0)));

        prepareTimeView.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence charSequence, int i, int i1, int i2) {
            }

            @Override
            public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {
                int value = 0;
                if (charSequence.length() > 0) {
                    value = Integer.valueOf(prepareTimeView.getText().toString());
                }

                SharedPreferences.Editor editor = sharedPreferences.edit();
                editor.putInt(getString(R.string.prepareTimeStore), value);
                editor.apply();

                setActionTime(view, calcActionTime(sharedPreferences));
            }

            @Override
            public void afterTextChanged(Editable editable) {
            }
        });
        final EditText arrowTimeView = (EditText) view.findViewById(R.id.arrowTime);
        arrowTimeView.setText(String.valueOf(sharedPreferences.getInt(getString(R.string.arrowTimeStore), 0)));
        arrowTimeView.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence charSequence, int i, int i1, int i2) {

            }

            @Override
            public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {
                int value = 0;
                if (charSequence.length() > 0) {
                    value = Integer.valueOf(arrowTimeView.getText().toString());
                }

                SharedPreferences.Editor editor = sharedPreferences.edit();
                editor.putInt(getString(R.string.arrowTimeStore), value);
                editor.apply();

                setActionTime(view, calcActionTime(sharedPreferences));
            }

            @Override
            public void afterTextChanged(Editable editable) {

            }
        });
        final EditText arrowCountView = (EditText) view.findViewById(R.id.arrowCount);
        arrowCountView.setText(String.valueOf(sharedPreferences.getInt(getString(R.string.arrowCountStore), 0)));
        arrowCountView.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence charSequence, int i, int i1, int i2) {

            }

            @Override
            public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {

                int value = 0;
                if (charSequence.length() > 0) {
                    value = Integer.valueOf(arrowCountView.getText().toString());
                }
                SharedPreferences.Editor editor = sharedPreferences.edit();
                editor.putInt(getString(R.string.arrowCountStore), value);
                editor.apply();
                setActionTime(view, calcActionTime(sharedPreferences));
            }

            @Override
            public void afterTextChanged(Editable editable) {

            }
        });
        setActionTime(view, calcActionTime(sharedPreferences));
    }

    private void setActionTime(View view, int actionTime) {
        SharedPreferences sharedPreferences = getActivity().getPreferences(Context.MODE_PRIVATE);
        final EditText actionTimeView = (EditText) view.findViewById(R.id.actionTime);
        actionTimeView.setText(String.valueOf(actionTime));
        SharedPreferences.Editor editor = sharedPreferences.edit();
        editor.putInt(getString(R.string.actionTimeStore), actionTime);
        editor.apply();
    }

    private int calcActionTime(SharedPreferences sharedPreferences) {
        int actionTime = sharedPreferences.getInt(getString(R.string.arrowTimeStore), 0) * sharedPreferences.getInt(getString(R.string.arrowCountStore), 0);
        int prepareTime = sharedPreferences.getInt(getString(R.string.prepareTimeStore), 0);
        int mode = sharedPreferences.getInt(getString(R.string.modeStore), 1);
        int passes = sharedPreferences.getInt(getString(R.string.passesCountStore), 1);
        try {
            JSONObject valuesObject = new JSONObject();
            valuesObject.put("prepareTime", prepareTime);
            valuesObject.put("actionTime", actionTime);
            valuesObject.put("mode", mode);
            valuesObject.put("passes", passes);
            JSONObject payload = new JSONObject();
            payload.put("name", "configure");
            payload.put("values", valuesObject);
            Sender.broadcastJSON(payload);

        } catch (Exception ex) {
            Log.e("Error", ex.getMessage());
        }
        return actionTime;
    }
}