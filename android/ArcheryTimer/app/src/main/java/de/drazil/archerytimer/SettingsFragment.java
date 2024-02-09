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
import android.widget.RadioButton;
import android.widget.RadioGroup;

import com.google.android.material.slider.Slider;

import java.net.InetAddress;

import de.drazil.archerytimer.udp.Sender;

/**
 * A simple {@link Fragment} subclass.
 * Use the {@link SettingsFragment#newInstance} factory method to
 * create an instance of this fragment.
 */
public class SettingsFragment extends Fragment {


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

        try {
            Sender.broadcast("!tournament:0:0:0!");
        } catch (Exception ex) {
            Log.e("Error", ex.getMessage());
        }
        return inflater.inflate(R.layout.fragment_settings, container, false);
    }

    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        SharedPreferences sharedPreferences = getActivity().getPreferences(Context.MODE_PRIVATE);
        RadioGroup radioGroup = (RadioGroup) view.findViewById(R.id.modeGroup);
        int selectedId = radioGroup.getCheckedRadioButtonId();
        RadioButton radioButton = (RadioButton) view.findViewById(selectedId);


        final EditText passesCountView = (EditText) view.findViewById(R.id.passesCount);
        float v1 = sharedPreferences.getFloat(getString(R.string.passesCountStore), 0);
        passesCountView.setText(String.valueOf(v1));
        final Slider passSelectorView = (Slider) view.findViewById(R.id.passSlider);
        passSelectorView.setValue(sharedPreferences.getFloat(getString(R.string.passesCountStore), 0));
        passSelectorView.addOnChangeListener(new Slider.OnChangeListener() {

            @Override
            public void onValueChange(@NonNull Slider slider, float value, boolean fromUser) {
                passesCountView.setText(String.valueOf(value ));
                SharedPreferences.Editor editor = sharedPreferences.edit();
                editor.putFloat(getString(R.string.passesCountStore), value);
                editor.apply();
            }
        });

        final Slider volumeSelectorView = (Slider) view.findViewById(R.id.volumeSlider);
        volumeSelectorView.setValue(sharedPreferences.getFloat(getString(R.string.volumeStore), 0.5f));
        volumeSelectorView.addOnChangeListener(new Slider.OnChangeListener() {
            @Override
            public void onValueChange(@NonNull Slider slider, float value, boolean fromUser) {
                SharedPreferences.Editor editor = sharedPreferences.edit();
                Log.i("VOLUME", String.valueOf(value));
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
                String command = String.format("!volume:0:%s:0!", String.valueOf(volume));
                try {
                    Sender.broadcast(command);
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
                SharedPreferences.Editor editor = sharedPreferences.edit();
                editor.putInt(getString(R.string.prepareTimeStore), Integer.valueOf(charSequence.toString()));
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
                if (charSequence.length() > 0) {
                    calcActionTime(view, sharedPreferences);
                    SharedPreferences.Editor editor = sharedPreferences.edit();
                    editor.putInt(getString(R.string.arrowTimeStore), Integer.valueOf(charSequence.toString()));
                    editor.apply();
                }
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
                if (charSequence.length() > 0) {
                    calcActionTime(view, sharedPreferences);
                    SharedPreferences.Editor editor = sharedPreferences.edit();
                    editor.putInt(getString(R.string.arrowCountStore), Integer.valueOf(charSequence.toString()));
                    editor.apply();
                }
            }

            @Override
            public void afterTextChanged(Editable editable) {

            }
        });
        calcActionTime(view, sharedPreferences);


    }

    private void calcActionTime(View rootView, SharedPreferences sharedPreferences) {
        final EditText arrowTimeView = (EditText) rootView.findViewById(R.id.arrowTime);
        final EditText arrowAmountView = (EditText) rootView.findViewById(R.id.arrowCount);
        final EditText actionTimeView = (EditText) rootView.findViewById(R.id.actionTime);
        String arrowTime = arrowTimeView.getText().toString();
        String arrowCount = arrowAmountView.getText().toString();
        int arrowTimeValue = null == arrowTime || arrowTime.equals("") ? 0 : Integer.valueOf(arrowTime);
        int arrowCountValue = null == arrowCount || arrowCount.equals("") ? 0 : Integer.valueOf(arrowCount);
        int actionTime = arrowTimeValue * arrowCountValue;
        actionTimeView.setText(String.valueOf(actionTime));
        SharedPreferences.Editor editor = sharedPreferences.edit();
        editor.putInt(getString(R.string.actionTimeStore), actionTime);
        editor.apply();

    }
}