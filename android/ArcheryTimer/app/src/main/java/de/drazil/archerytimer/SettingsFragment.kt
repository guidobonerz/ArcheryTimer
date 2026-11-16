package de.drazil.archerytimer

import android.app.Dialog
import android.app.TimePickerDialog
import android.content.Context
import android.content.DialogInterface
import android.content.SharedPreferences
import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.Window
import android.widget.CheckBox
import android.widget.EditText
import android.widget.ImageButton
import android.widget.RadioButton
import android.widget.RadioGroup
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import com.google.android.material.slider.Slider
import org.json.JSONObject
import de.drazil.archerytimer.udp.UDPSender
import androidx.core.content.edit

class SettingsFragment : Fragment(), IRemoteView {

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        return inflater.inflate(R.layout.fragment_settings, container, false)
    }

    private fun showSoundDialog() {
        val dialog = Dialog(requireActivity())
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE)
        dialog.setCancelable(true)
        dialog.setContentView(R.layout.fragment_sounds)
        dialog.show()
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val sharedPreferences = requireActivity().getPreferences(Context.MODE_PRIVATE)

        val soundSettingsView = view.findViewById<TextView>(R.id.signalSettings)
        soundSettingsView.setOnClickListener { showSoundDialog() }

        val group1 = view.findViewById<RadioButton>(R.id.modeAB)
        val group2 = view.findViewById<RadioButton>(R.id.modeABCD)
        //val group3 = view.findViewById<RadioButton>(R.id.modeABCDEF)
        val selectedGroupText = view.findViewById<TextView>(R.id.groupSelection)
        if (sharedPreferences.getInt(getString(R.string.groupStore), 0) == 1) {
            group1.isChecked = true
            group2.isChecked = false
            //group3.isChecked = false
            selectedGroupText.setText(R.string.ab)
        } else if (sharedPreferences.getInt(getString(R.string.groupStore), 0) == 2) {
            group1.isChecked = false
            group2.isChecked = true
            //group3.isChecked = false
            selectedGroupText.setText(R.string.abcd)
        } /*else {
            group1.isChecked = false
            group2.isChecked = false
            //group3.isChecked = true
            selectedGroupText.text = "AB-CD-EF"
        }*/

        val radioGroup = view.findViewById<RadioGroup>(R.id.modeGroup)
        radioGroup.setOnCheckedChangeListener { _, i ->
            sharedPreferences.edit {
                val groupText = view.findViewById<TextView>(R.id.groupSelection)
                if (i == R.id.modeAB) {
                    putInt(getString(R.string.groupStore), 1)
                    groupText.setText(R.string.ab)
                } else if (i == R.id.modeABCD) {
                    putInt(getString(R.string.groupStore), 2)
                    groupText.setText(R.string.abcd)
                } /*else {
                editor.putInt(getString(R.string.groupStore), 3)
                groupText.text = "AB-CD-EF"
            }*/
            }
            calcActionTime(view)
        }

        val passesCountView = view.findViewById<EditText>(R.id.passesCount)
        passesCountView.setText(sharedPreferences.getInt(getString(R.string.passesCountStore), 0).toString())
        bindIntPreference(view, sharedPreferences, passesCountView, getString(R.string.passesCountStore))

        val arrowCountView = view.findViewById<EditText>(R.id.arrowCount)
        arrowCountView.setText(sharedPreferences.getInt(getString(R.string.arrowCountStore), 0).toString())
        bindIntPreference(view, sharedPreferences, arrowCountView, getString(R.string.arrowCountStore))

        val shootInTimeView = view.findViewById<EditText>(R.id.shootInTime)
        shootInTimeView.setText(sharedPreferences.getInt(getString(R.string.shootInTimeStore), 0).toString())
        bindIntPreference(view, sharedPreferences, shootInTimeView, getString(R.string.shootInTimeStore))

        val prepareTimeView = view.findViewById<EditText>(R.id.prepareTime)
        prepareTimeView.setText(sharedPreferences.getInt(getString(R.string.prepareTimeStore), 0).toString())
        bindIntPreference(view, sharedPreferences, prepareTimeView, getString(R.string.prepareTimeStore))

        val arrowTimeView = view.findViewById<EditText>(R.id.arrowTime)
        arrowTimeView.setText(sharedPreferences.getInt(getString(R.string.arrowTimeStore), 0).toString())
        bindIntPreference(view, sharedPreferences, arrowTimeView, getString(R.string.arrowTimeStore))

        val warnTimeView = view.findViewById<EditText>(R.id.warnTime)
        warnTimeView.setText(sharedPreferences.getInt(getString(R.string.warnTimeStore), 0).toString())
        bindIntPreference(view, sharedPreferences, warnTimeView, getString(R.string.warnTimeStore))

        val testSignalButton = view.findViewById<ImageButton>(R.id.testSignal)
        testSignalButton.setOnClickListener {
            try {
                val payload = JSONObject()
                payload.put("cmd", "testsignal")
                UDPSender.broadcastJSON(payload, System.currentTimeMillis())
            } catch (ex: Exception) {
                Log.e("Error", ex.message ?: "")
            }
        }

        val volumeSelectorView = view.findViewById<Slider>(R.id.volumeSlider)
        volumeSelectorView.value = sharedPreferences.getFloat(getString(R.string.volumeStore), 10.0f)
        volumeSelectorView.addOnChangeListener { _, value, _ ->
            sharedPreferences.edit { putFloat(getString(R.string.volumeStore), value) }
        }
        volumeSelectorView.addOnSliderTouchListener(object : Slider.OnSliderTouchListener {
            override fun onStartTrackingTouch(slider: Slider) {
            }

            override fun onStopTrackingTouch(slider: Slider) {
                val volume = sharedPreferences.getFloat(getString(R.string.volumeStore), 0f)
                Log.i("volume", volume.toString())
                try {
                    val valuesObject = JSONObject()
                    valuesObject.put("val", volume.toInt())
                    val payload = JSONObject()
                    payload.put("cmd", "volume")
                    payload.put("val", valuesObject)
                    UDPSender.broadcastJSON(payload, System.currentTimeMillis())
                } catch (ex: Exception) {
                    Log.e("Error", ex.message ?: "")
                }
            }
        })

        val flashingPrepareLightView = view.findViewById<CheckBox>(R.id.flashingPrepareLight)
        flashingPrepareLightView.isChecked =
            sharedPreferences.getBoolean(getString(R.string.flashingPrepareLightStore), true)
        flashingPrepareLightView.setOnCheckedChangeListener { _, b ->
            sharedPreferences.edit { putBoolean(getString(R.string.flashingPrepareLightStore), b) }
            calcActionTime(view)
        }

        // the end of the lunch break - tapped, it opens the clock to set it
        val lunchEndTimeView = view.findViewById<TextView>(R.id.lunchEndTime)
        lunchEndTimeView.text = LunchBreak.storedTime(sharedPreferences)
        view.findViewById<View>(R.id.lunchEndRow).setOnClickListener {
            // An explicit dialog theme: with the theme of the app the buttons
            // of the dialog take its primary colour, which is black - on the
            // dark dialog there was no visible OK to confirm the time with.
            val dialog = TimePickerDialog(
                requireActivity(),
                android.R.style.Theme_DeviceDefault_Dialog_Alert,
                { _, hour, minute ->
                    LunchBreak.storeTime(sharedPreferences, hour, minute)
                    lunchEndTimeView.text = LunchBreak.storedTime(sharedPreferences)
                },
                LunchBreak.hour(sharedPreferences),
                LunchBreak.minute(sharedPreferences),
                true
            )
            dialog.show()
            // the buttons only exist once the dialog is on screen
            val orange = ContextCompat.getColor(requireContext(), R.color.orange)
            dialog.getButton(DialogInterface.BUTTON_POSITIVE)?.setTextColor(orange)
            dialog.getButton(DialogInterface.BUTTON_NEGATIVE)?.setTextColor(orange)
        }

        calcActionTime(view)
    }

    private fun bindIntPreference(
        view: View,
        sharedPreferences: SharedPreferences,
        editText: EditText,
        key: String
    ) {
        editText.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(charSequence: CharSequence?, i: Int, i1: Int, i2: Int) {
            }

            override fun onTextChanged(charSequence: CharSequence?, i: Int, i1: Int, i2: Int) {
                var value = 0
                if (!charSequence.isNullOrEmpty()) {
                    value = editText.text.toString().toInt()
                }
                sharedPreferences.edit { putInt(key, value) }
                calcActionTime(view)
            }

            override fun afterTextChanged(editable: Editable?) {
            }
        })
    }

    private fun calcActionTime(view: View) {
        val sharedPreferences = requireActivity().getPreferences(Context.MODE_PRIVATE)
        val actionTimeView = view.findViewById<EditText>(R.id.actionTime)
        val actionTime = sharedPreferences.getInt(getString(R.string.arrowTimeStore), 0) *
                sharedPreferences.getInt(getString(R.string.arrowCountStore), 0)
        actionTimeView.setText(actionTime.toString())
        sharedPreferences.edit { putInt(getString(R.string.actionTimeStore), actionTime) }
        UDPSender.sendConfiguration(sharedPreferences, System.currentTimeMillis())
    }

    override val currentView: String
        get() = "setup"
}
