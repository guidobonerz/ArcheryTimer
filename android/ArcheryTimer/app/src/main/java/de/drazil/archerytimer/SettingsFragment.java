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
        radioGroup.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(RadioGroup radioGroup, int i) {
                calcActionTime(view, sharedPreferences);
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
                ;
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
                String value = "0";
                if (charSequence.length() > 0) {
                    value = passesCountView.getText().toString();
                }
                calcActionTime(view, sharedPreferences);
                SharedPreferences.Editor editor = sharedPreferences.edit();
                editor.putInt(getString(R.string.passesCountStore), Integer.valueOf(value));
                editor.apply();
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
                String value = "0";
                if (charSequence.length() > 0) {
                    value = prepareTimeView.getText().toString();
                }
                calcActionTime(view, sharedPreferences);
                SharedPreferences.Editor editor = sharedPreferences.edit();
                editor.putInt(getString(R.string.prepareTimeStore), Integer.valueOf(value));
                editor.apply();
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
                String value = "0";
                if (charSequence.length() > 0) {
                    value = arrowTimeView.getText().toString();
                }
                calcActionTime(view, sharedPreferences);
                SharedPreferences.Editor editor = sharedPreferences.edit();
                editor.putInt(getString(R.string.arrowTimeStore), Integer.valueOf(value));
                editor.apply();
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

                String value = "0";
                if (charSequence.length() > 0) {
                    value = arrowCountView.getText().toString();
                }
                calcActionTime(view, sharedPreferences);
                SharedPreferences.Editor editor = sharedPreferences.edit();
                editor.putInt(getString(R.string.arrowCountStore), Integer.valueOf(value));
                editor.apply();
            }

            @Override
            public void afterTextChanged(Editable editable) {

            }
        });
        calcActionTime(view, sharedPreferences);


    }

    private void calcActionTime(View rootView, SharedPreferences sharedPreferences) {
        try {


            final EditText arrowTimeView = (EditText) rootView.findViewById(R.id.arrowTime);
            final EditText arrowAmountView = (EditText) rootView.findViewById(R.id.arrowCount);
            final EditText actionTimeView = (EditText) rootView.findViewById(R.id.actionTime);
            final EditText prepareTimeView = (EditText) rootView.findViewById(R.id.prepareTime);
            String prepareTime = prepareTimeView.getText().toString();
            String arrowTime = arrowTimeView.getText().toString();
            String arrowCount = arrowAmountView.getText().toString();
            int prepareTimeValue = null == prepareTime || prepareTime.equals("") ? 0 : Integer.valueOf(prepareTime);
            int arrowTimeValue = null == arrowTime || arrowTime.equals("") ? 0 : Integer.valueOf(arrowTime);
            int arrowCountValue = null == arrowCount || arrowCount.equals("") ? 0 : Integer.valueOf(arrowCount);
            int actionTime = arrowTimeValue * arrowCountValue;
            actionTimeView.setText(String.valueOf(actionTime));
            SharedPreferences.Editor editor = sharedPreferences.edit();
            editor.putInt(getString(R.string.actionTimeStore), actionTime);
            editor.apply();


            int selectedId = radioGroup.getCheckedRadioButtonId();
            RadioButton radioButton = (RadioButton) rootView.findViewById(selectedId);
            JSONObject valuesObject = new JSONObject();
            valuesObject.put("prepareTime", prepareTimeValue);
            valuesObject.put("actionTime", actionTime);
            valuesObject.put("mode", radioButton.getText().toString());
            JSONObject payload = new JSONObject();
            payload.put("name", "prepare");
            payload.put("values", valuesObject);
            Sender.broadcastJSON(payload);

        } catch (Exception ex) {
            Log.e("Error", ex.getMessage());
        }
    }
}