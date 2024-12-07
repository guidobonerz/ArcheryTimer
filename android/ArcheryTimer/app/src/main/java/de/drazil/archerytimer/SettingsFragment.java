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
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.TextView;

import com.google.android.material.slider.Slider;

import org.json.JSONObject;

import de.drazil.archerytimer.udp.UDPSender;


public class SettingsFragment extends Fragment implements IRemoteView {

    private RadioGroup radioGroup = null;
    private RadioButton group1 = null;
    private RadioButton group2 = null;
    private RadioButton group3 = null;

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
        return inflater.inflate(R.layout.fragment_settings, container, false);
    }

    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        SharedPreferences sharedPreferences = getActivity().getPreferences(Context.MODE_PRIVATE);
        group1 = (RadioButton) view.findViewById(R.id.modeAB);
        group2 = (RadioButton) view.findViewById(R.id.modeABCD);
        //group3 = (RadioButton) view.findViewById(R.id.modeABCDEF);
        final TextView selectedGroupText = (TextView) view.findViewById(R.id.groupSelection);
        if (sharedPreferences.getInt(getString(R.string.groupStore), 0) == 1) {
            group1.setChecked(true);
            group2.setChecked(false);
            //group3.setChecked(false);
            selectedGroupText.setText("AB");
        } else if (sharedPreferences.getInt(getString(R.string.groupStore), 0) == 2) {
            group1.setChecked(false);
            group2.setChecked(true);
            //group3.setChecked(false);
            selectedGroupText.setText("AB-CD");
        } else {
            group1.setChecked(false);
            group2.setChecked(false);
            //group3.setChecked(true);
            selectedGroupText.setText("AB-CD-EF");
        }
        radioGroup = (RadioGroup) view.findViewById(R.id.modeGroup);
        radioGroup.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(RadioGroup radioGroup, int i) {
                SharedPreferences.Editor editor = sharedPreferences.edit();
                final TextView selectedGroupText = (TextView) view.findViewById(R.id.groupSelection);
                if (i == R.id.modeAB) {
                    editor.putInt(getString(R.string.groupStore), 1);
                    selectedGroupText.setText("AB");
                } else if (i == R.id.modeABCD) {
                    editor.putInt(getString(R.string.groupStore), 2);
                    selectedGroupText.setText("AB-CD");
                } else {
                    editor.putInt(getString(R.string.groupStore), 3);
                    selectedGroupText.setText("AB-CD-EF");
                }
                editor.apply();
                calcActionTime(view);
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
                calcActionTime(view);
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
                calcActionTime(view);
            }

            @Override
            public void afterTextChanged(Editable editable) {
            }
        });


        final EditText shootInTimeView = (EditText) view.findViewById(R.id.shootInTime);
        shootInTimeView.setText(String.valueOf(sharedPreferences.getInt(getString(R.string.shootInTimeStore), 0)));
        shootInTimeView.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence charSequence, int i, int i1, int i2) {
            }

            @Override
            public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {
                int value = 0;
                if (charSequence.length() > 0) {
                    value = Integer.valueOf(shootInTimeView.getText().toString());
                }
                SharedPreferences.Editor editor = sharedPreferences.edit();
                editor.putInt(getString(R.string.shootInTimeStore), value);
                editor.apply();
                calcActionTime(view);
            }

            @Override
            public void afterTextChanged(Editable editable) {
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
                calcActionTime(view);
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
                calcActionTime(view);
            }

            @Override
            public void afterTextChanged(Editable editable) {
            }
        });

        final EditText warnTimeView = (EditText) view.findViewById(R.id.warnTime);
        warnTimeView.setText(String.valueOf(sharedPreferences.getInt(getString(R.string.warnTimeStore), 0)));
        warnTimeView.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence charSequence, int i, int i1, int i2) {

            }

            @Override
            public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {
                int value = 0;
                if (charSequence.length() > 0) {
                    value = Integer.valueOf(warnTimeView.getText().toString());
                }
                SharedPreferences.Editor editor = sharedPreferences.edit();
                editor.putInt(getString(R.string.warnTimeStore), value);
                editor.apply();
                calcActionTime(view);
            }

            @Override
            public void afterTextChanged(Editable editable) {
            }
        });
        ImageButton testSignalButton = (ImageButton) view.findViewById(R.id.testSignal);
        testSignalButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View _view) {
                try {
                    JSONObject payload = new JSONObject();
                    payload.put("cmd", "testsignal");
                    UDPSender.broadcastJSON(payload,System.currentTimeMillis());
                } catch (Exception ex) {
                    Log.e("Error", ex.getMessage());
                }
            }
        });
        final Slider volumeSelectorView = (Slider) view.findViewById(R.id.volumeSlider);
        volumeSelectorView.setValue(sharedPreferences.getFloat(getString(R.string.volumeStore), 10.0f));
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
                Log.i("volume",String.valueOf(volume));
                try {
                    JSONObject valuesObject = new JSONObject();
                    valuesObject.put("val", (int) (volume));
                    JSONObject payload = new JSONObject();
                    payload.put("cmd", "volume");
                    payload.put("val", valuesObject);
                    UDPSender.broadcastJSON(payload,System.currentTimeMillis());
                } catch (Exception ex) {
                    Log.e("Error", ex.getMessage());
                }
            }
        });

        final CheckBox flashingPrepareLightView = (CheckBox) view.findViewById(R.id.flashingPrepareLight);
        flashingPrepareLightView.setChecked(sharedPreferences.getBoolean(getString(R.string.flashingPrepareLightStore), true));
        flashingPrepareLightView.setOnCheckedChangeListener(new CheckBox.OnCheckedChangeListener(){
            @Override
            public void onCheckedChanged(CompoundButton compoundButton, boolean b) {
                SharedPreferences.Editor editor = sharedPreferences.edit();
                editor.putBoolean(getString(R.string.flashingPrepareLightStore), b);
                editor.apply();
                calcActionTime(view);
            }
        });


        calcActionTime(view);
    }

    private void calcActionTime(View view) {
        SharedPreferences sharedPreferences = getActivity().getPreferences(Context.MODE_PRIVATE);
        final EditText actionTimeView = (EditText) view.findViewById(R.id.actionTime);
        int actionTime = sharedPreferences.getInt(getString(R.string.arrowTimeStore), 0) * sharedPreferences.getInt(getString(R.string.arrowCountStore), 0);
        actionTimeView.setText(String.valueOf(actionTime));
        SharedPreferences.Editor editor = sharedPreferences.edit();
        editor.putInt(getString(R.string.actionTimeStore), actionTime);
        editor.apply();
        UDPSender.sendConfiguration(sharedPreferences,System.currentTimeMillis());
    }

    @Override
    public String getCurrentView() {
        return "setup";
    }
}