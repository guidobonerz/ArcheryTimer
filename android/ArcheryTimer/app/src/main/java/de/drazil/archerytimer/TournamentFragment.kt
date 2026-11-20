package de.drazil.archerytimer

import android.app.AlertDialog
import android.content.Context
import android.content.SharedPreferences
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.ColorFilter
import android.graphics.Paint
import android.graphics.PixelFormat
import android.graphics.drawable.Drawable
import android.os.Bundle
import android.os.VibrationEffect
import android.os.VibratorManager
import android.util.Log
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.CheckBox
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.RadioButton
import android.widget.RadioGroup
import android.widget.TextView
import androidx.fragment.app.Fragment
import org.json.JSONObject
import de.drazil.archerytimer.net.DisplayRegistry
import de.drazil.archerytimer.net.SessionState
import de.drazil.archerytimer.udp.UDPSender
import de.drazil.archerytimer.udp.UDPServer
import androidx.core.content.edit

class TournamentFragment : Fragment(), IRemoteControl, IRemoteView, DisplayRegistry.Listener {

    private var running = false
    private var hold = false
    private var reshootAction = false

    private var topic: String? = null

    private var toggleButton: ImageButton? = null
    private var emergencyButton: ImageButton? = null
    private var stopButton: ImageButton? = null
    private var resetButton: ImageButton? = null
    private var passStatusView: TextView? = null
    private var imageView: ImageView? = null
    private var progress: ProgressControl? = null

    private var tournamentPhaseGroup: RadioGroup? = null

    private var reshootActionGroup: RadioGroup? = null

    private var lunchBreakBox: CheckBox? = null

    /** set while the box is changed by the code and not by a finger */
    private var lunchBreakSilent = false

    /** the same for the arrow buttons of the re-shoot */
    private var reshootSilent = false

    /** without a display on the network there is nobody to command */
    private var displaysOnline = false

    private inner class ProgressControl : Drawable() {

        private var group = 1
        private var prepareTime = 0
        private var actionTime = 0
        private var warnTime = 0
        private var remainingPrepareTime = 0
        private var remainingActionTime = 0
        private var currentGroup = 1

        private var maxTime = 0
        private var remainingOffset = 0
        private val prepareColor = Color.rgb(255, 0, 0)
        private val actionColor = Color.rgb(0, 255, 0)
        private val warnColor = Color.rgb(241, 196, 15)
        private val separatorColor = Color.rgb(0, 0, 0)
        private val groupSeparatorColor = Color.rgb(255, 255, 255)
        private val overlayColor = Color.argb(150, 0, 0, 0)
        private val borderColor = Color.rgb(120, 120, 120)

        private var warnOnly = false

        init {
            reset()
        }

        fun setRemainingPrepareTime(remainingPrepareTime: Int) {
            this.remainingPrepareTime = remainingPrepareTime
        }

        fun setRemainingActionTime(remainingActionTime: Int) {
            this.remainingActionTime = remainingActionTime
        }

        fun setCurrentGroup(currentGroup: Int) {
            this.currentGroup = currentGroup
        }

        /**
         * Segment lengths as reported by the display. The app used to derive
         * them from the preferences, which was wrong for a re-shoot: the
         * display counts reshootArrowCount * arrowTime there, the bar drew a
         * full pass.
         *
         * The second group of a pass used to be drawn with a double
         * preparation segment, from the time the display ran straight on from
         * one group into the next. Each group runs its own preparation and
         * shooting time and the display reports them per group, so both are
         * drawn alike.
         */
        fun setSegments(prepare: Int, action: Int, warn: Int) {
            if (action <= 0) {
                return
            }
            prepareTime = prepare
            actionTime = action
            warnOnly = warn >= action
            warnTime = if (warnOnly) action else warn
            maxTime = prepareTime + actionTime
        }

        /** initial guess, replaced by the values the display reports */
        fun reset() {
            val sharedPreferences = requireActivity().getPreferences(Context.MODE_PRIVATE)
            group = sharedPreferences.getInt(getString(R.string.groupStore), 1)
            prepareTime = sharedPreferences.getInt(getString(R.string.prepareTimeStore), 0)
            remainingPrepareTime = prepareTime
            actionTime = if (reshootAction) {
                // a re-shoot only runs for the arrows chosen above
                sharedPreferences.getInt(getString(R.string.reshootArrowCountStore), 1) *
                        sharedPreferences.getInt(getString(R.string.arrowTimeStore), 0)
            } else {
                sharedPreferences.getInt(getString(R.string.actionTimeStore), 0)
            }
            remainingActionTime = actionTime
            warnTime = sharedPreferences.getInt(getString(R.string.warnTimeStore), 0)

            warnOnly = warnTime >= actionTime
            if (warnOnly) {
                warnTime = actionTime
            }
            maxTime = prepareTime + actionTime
            remainingOffset = maxTime
            invalidateSelf()
        }

        private fun drawSection(
            canvas: Canvas,
            paint: Paint,
            offset: Int,
            time: Int,
            maxTime: Int,
            maxWidth: Int,
            height: Int,
            color: Int,
            showSeparator: Boolean
        ): Int {
            val w = Math.ceil(time.toDouble() / maxTime * maxWidth).toInt()
            paint.style = Paint.Style.FILL
            paint.color = color
            canvas.drawRect(offset.toFloat(), 0f, (offset + w).toFloat(), height.toFloat(), paint)
            if (showSeparator) {
                paint.color = separatorColor
                paint.strokeWidth = 6f
                canvas.drawLine((offset + w - 3).toFloat(), 0f, (offset + w - 3).toFloat(), height.toFloat(), paint)
            }
            return w
        }

        override fun draw(canvas: Canvas) {
            val paint = Paint()

            val width = canvas.width
            val height = canvas.height

            var offset = 0

            val remainingTime = maxTime - (remainingPrepareTime + remainingActionTime)

            offset += drawSection(canvas, paint, offset, prepareTime, maxTime, width, height, prepareColor, true)
            if (warnOnly) {
                offset += drawSection(canvas, paint, offset, actionTime, maxTime, width, height, warnColor, false)
            } else {
                offset += drawSection(canvas, paint, offset, actionTime - warnTime, maxTime, width, height, actionColor, true)
                offset += drawSection(canvas, paint, offset, warnTime, maxTime, width, height, warnColor, false)
            }

            remainingOffset = (remainingTime.toFloat() / maxTime * width).toInt()

            //remainingTime overlay
            paint.color = overlayColor
            canvas.drawRect(0f, 0f, remainingOffset.toFloat(), height.toFloat(), paint)

            //border color
            paint.style = Paint.Style.STROKE
            paint.color = borderColor
            paint.strokeWidth = 25f
            canvas.drawLine(0f, 0f, width.toFloat(), 0f, paint)
            canvas.drawLine(0f, height.toFloat(), width.toFloat(), height.toFloat(), paint)
        }

        @Deprecated("Deprecated in Java", ReplaceWith("PixelFormat.UNKNOWN", "android.graphics.PixelFormat"))
        override fun getOpacity(): Int {
            return PixelFormat.UNKNOWN
        }

        override fun setAlpha(alpha: Int) {
        }

        override fun setColorFilter(cf: ColorFilter?) {
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        UDPServer.addRemoteControl(this)
        return inflater.inflate(R.layout.fragment_tournament, container, false)
    }

    override fun onDestroyView() {
        super.onDestroyView()
        DisplayRegistry.removeListener(this)
        UDPServer.removeRemoteControl(this)
    }

    override fun onDisplaysChanged(displays: List<DisplayRegistry.Display>) {
        val online = displays.any { it.online }
        if (online != displaysOnline) {
            displaysOnline = online
            updateEmergencyButton()
        }
    }

    override fun onDisplayAvailable(display: DisplayRegistry.Display, reason: DisplayRegistry.Reason) {
        // the table change above is all this page needs
    }

    private fun sendRun(startTime: Long) {
        val payload = JSONObject()
        try {
            val valuesObject = JSONObject()
            valuesObject.put("t", 0)
            valuesObject.put("h", false)
            payload.put("cmd", "run")
            payload.put("val", valuesObject)
            UDPSender.broadcastJSON(payload, startTime)
        } catch (ex: Exception) {
            Log.e("Error", ex.message ?: "")
        }
    }

    private fun sendPause(startTime: Long) {
        val payload = JSONObject()
        try {
            val valuesObject = JSONObject()
            valuesObject.put("t", 0)
            valuesObject.put("h", true)
            payload.put("cmd", "pause")
            payload.put("val", valuesObject)
            UDPSender.broadcastJSON(payload, startTime)
        } catch (ex: Exception) {
            Log.e("Error", ex.message ?: "")
        }
    }

    private fun sendStop(startTime: Long) {
        val payload = JSONObject()
        try {
            val valuesObject = JSONObject()
            valuesObject.put("h", false)
            payload.put("cmd", "stop")
            payload.put("val", valuesObject)
            UDPSender.broadcastJSON(payload, startTime)
        } catch (ex: Exception) {
            Log.e("Error", ex.message ?: "")
        }
    }

    private fun sendReset(startTime: Long) {
        val sharedPreferences = requireActivity().getPreferences(Context.MODE_PRIVATE)
        UDPSender.sendConfiguration(sharedPreferences, System.currentTimeMillis())
        val payload = JSONObject()
        try {
            payload.put("cmd", "reset")
            UDPSender.broadcastJSON(payload, startTime)
        } catch (ex: Exception) {
            Log.e("Error", ex.message ?: "")
        }
    }

    /** sets or releases the safety stop */
    private fun sendEmergency(on: Boolean, startTime: Long) {
        val payload = JSONObject()
        try {
            val valuesObject = JSONObject()
            valuesObject.put("e", on)
            payload.put("cmd", "emergency")
            payload.put("val", valuesObject)
            UDPSender.broadcastJSON(payload, startTime)
        } catch (ex: Exception) {
            Log.e("Error", ex.message ?: "")
        }
    }

    override fun handleResponse(json: JSONObject) {
        if (!isAdded) {
            return
        }
        try {
            val command = json.optString("cmd")
            if (command.equals("run", ignoreCase = true)) {
                //hold = false;
                toggleButton!!.setImageResource(R.drawable.pause)
            } else if (command.equals("pause", ignoreCase = true)) {
                //hold = true;
                toggleButton!!.setImageResource(R.drawable.play)
            } else if (command.equals("stop", ignoreCase = true)) {
                hold = false
                running = false
                toggleButton!!.setImageResource(R.drawable.play)
                updateArrowButtons()
            } else if (command.equals("reset", ignoreCase = true)) {
                running = false
                hold = false
                toggleButton!!.setImageResource(R.drawable.play)
                setGroupAndPassInfo("AB", 0, 0)
                updateArrowButtons()
                progress!!.reset()
            } else if (command.equals("emergency", ignoreCase = true)) {
                // set or released by the touch controller - the session
                // state has taken it over already
                updateEmergencyButton()
            } else if (command.equals("state", ignoreCase = true)) {
                updateEmergencyButton()
                val sharedPreferences = requireActivity().getPreferences(Context.MODE_PRIVATE)
                setGroupAndPassInfo(
                    if (json.getInt("cg") == 1) "AB" else "CD",
                    json.getInt("cp"),
                    sharedPreferences.getInt(getString(R.string.passesCountStore), 0)
                )
                if (json.has("sat")) {
                    progress!!.setSegments(
                        json.optInt("spt", 0),
                        json.optInt("sat", 0),
                        json.optInt("swt", 0)
                    )
                }
                progress!!.setCurrentGroup(json.getInt("cg"))
                progress!!.setRemainingPrepareTime(json.getInt("cpt"))
                progress!!.setRemainingActionTime(json.getInt("cat"))
                progress!!.invalidateSelf()
                applyReportedPhase(json)
            }
        } catch (ex: Exception) {
        }
    }

    /**
     * The display is the one that knows whether a pass is still running: it
     * ends on its own when the time is up, and after the app was in the
     * background it is the only source left. Following its report keeps the
     * start button from showing "pause" for a pass that is long over.
     */
    private fun applyReportedPhase(json: JSONObject) {
        if (!json.has("ph")) {
            return
        }
        running = json.optInt("ph", 0) == 1
        hold = running && json.optBoolean("h", hold)
        toggleButton!!.setImageResource(
            if (running && !hold) R.drawable.pause else R.drawable.play
        )
        updateArrowButtons()
    }

    /**
     * The number of arrows of a re-shoot is chosen before the start. During
     * the pass the displays are counting with it, so it stays as it is.
     */
    private fun updateArrowButtons() {
        // The displays keep their view and their arrows for the length of a
        // pass, and a reset would throw the pass away - it has to be stopped
        // first. During the safety stop nothing but the bell works.
        val idle = !running && !SessionState.emergency
        // and with no display on the network there is nothing to set up
        val free = idle && displaysOnline
        reshootActionGroup?.let { group ->
            for (i in 0 until group.childCount) {
                group.getChildAt(i).isEnabled = reshootAction && free
            }
            group.alpha = if (free) 1.0f else 0.3f
        }
        tournamentPhaseGroup?.let { group ->
            group.alpha = if (free) 1.0f else 0.3f
            for (i in 0 until group.childCount) {
                group.getChildAt(i).isEnabled = free
            }
        }
        resetButton?.isEnabled = free
        resetButton?.alpha = if (free) 1.0f else 0.3f
        lunchBreakBox?.isEnabled = idle
        lunchBreakBox?.alpha = if (idle) 1.0f else 0.3f
    }

    /** the bell on red for as long as the safety stop is set */
    private fun updateEmergencyButton() {
        val emergency = SessionState.emergency
        emergencyButton?.setBackgroundColor(if (emergency) Color.RED else Color.BLACK)
        // during the safety stop only the bell is left to press, and
        // without a display nobody would hear start or stop
        val usable = !emergency && displaysOnline
        for (button in listOf<View?>(toggleButton, stopButton)) {
            button?.isEnabled = usable
            button?.alpha = if (usable) 1.0f else 0.3f
        }
        // the bar shows what a display reports - without one it shows nothing true
        imageView?.alpha = if (displaysOnline) 1.0f else 0.3f
        updateArrowButtons()
    }

    private fun startPass() {
        sendRun(System.currentTimeMillis())
        running = true
        hold = false
        updateArrowButtons()
    }

    private fun setGroupAndPassInfo(groupInfo: String, currentPass: Int, maxPass: Int) {
        passStatusView!!.text = String.format(
            "%s: %s - %s: %02d/%02d",
            getString(R.string.groupText), groupInfo, getString(R.string.passesText), currentPass, maxPass
        )
    }

    override fun onViewCreated(rootView: View, savedInstanceState: Bundle?) {
        super.onViewCreated(rootView, savedInstanceState)
        progress = ProgressControl()
        imageView = rootView.findViewById(R.id.timerProgress)
        imageView!!.setImageDrawable(progress)
        reshootActionGroup = rootView.findViewById(R.id.reShootActionGroup)
        reshootActionGroup!!.setOnCheckedChangeListener { _, id ->
            // only a finger on a button counts - the phase listener below
            // sets the buttons and sends the configuration itself
            if (reshootSilent || id <= 0) {
                return@setOnCheckedChangeListener
            }
            val startTime = System.currentTimeMillis()
            val sharedPreferences = requireActivity().getPreferences(Context.MODE_PRIVATE)
            sharedPreferences.edit {
                putInt(getString(R.string.reshootArrowCountStore), id)
            }
            UDPSender.sendConfiguration(sharedPreferences, startTime)
            progress!!.reset()
        }

        tournamentPhaseGroup = rootView.findViewById(R.id.tournamentPhase)
        lunchBreakBox = rootView.findViewById(R.id.lunchBreak)
        lunchBreakBox!!.setOnCheckedChangeListener { box, checked ->
            if (lunchBreakSilent) {
                return@setOnCheckedChangeListener
            }
            if (running) {
                // the displays do not change the view during a pass, so the
                // box must not claim they did
                lunchBreakSilent = true
                box.isChecked = !checked
                lunchBreakSilent = false
                return@setOnCheckedChangeListener
            }
            val sharedPreferences = requireActivity().getPreferences(Context.MODE_PRIVATE)
            UDPSender.controlDisplay(
                currentView,
                viewPayload(sharedPreferences),
                System.currentTimeMillis()
            )
        }

        tournamentPhaseGroup!!.setOnCheckedChangeListener { radioGroup, id ->
            val startTime = System.currentTimeMillis()
            val sharedPreferences = requireActivity().getPreferences(Context.MODE_PRIVATE)
            // choosing a phase puts its view on the displays - the break is over
            lunchBreakSilent = true
            lunchBreakBox!!.isChecked = false
            lunchBreakSilent = false
            reshootAction = false
            reshootActionGroup!!.visibility = View.INVISIBLE
            val subViewName = getSubViewName()
            UDPSender.controlDisplay(subViewName, null, System.currentTimeMillis())
            if (subViewName.equals("re_shoot", ignoreCase = true)) {
                val radioButton = radioGroup.findViewById<RadioButton>(id)
                reshootAction = radioButton.isChecked
                reshootActionGroup!!.visibility = View.VISIBLE
                // Every re-shoot starts out with one arrow. The number is
                // written here and not left to the buttons: a button that is
                // already checked does not report being checked again, and the
                // displays would be left with the count of the time before.
                sharedPreferences.edit {
                    putInt(getString(R.string.reshootArrowCountStore), 1)
                }
                reshootSilent = true
                reshootActionGroup!!.check(1)
                reshootSilent = false
            }
            updateArrowButtons()
            try {
                UDPSender.sendConfiguration(sharedPreferences, startTime)
            } catch (ex: Exception) {
                Log.e("Error", ex.message ?: "")
            }
            if (!running) {
                progress!!.reset()
            }
        }

        val sharedPreferences = requireActivity().getPreferences(Context.MODE_PRIVATE)
        val arrowCount = sharedPreferences.getInt(getString(R.string.arrowCountStore), 0)
        for (i in 1 until arrowCount + 1) {
            val radioButton = RadioButton(rootView.context)
            radioButton.id = i
            radioButton.gravity = Gravity.CENTER
            radioButton.setPadding(20, 20, 25, 20)
            radioButton.background = null
            when (i) {
                1 -> radioButton.setButtonDrawable(R.drawable.custom_btn1_radio)
                2 -> radioButton.setButtonDrawable(R.drawable.custom_btn2_radio)
                3 -> radioButton.setButtonDrawable(R.drawable.custom_btn3_radio)
                4 -> radioButton.setButtonDrawable(R.drawable.custom_btn4_radio)
                5 -> radioButton.setButtonDrawable(R.drawable.custom_btn5_radio)
                6 -> radioButton.setButtonDrawable(R.drawable.custom_btn6_radio)
            }
            radioButton.isChecked = i == 1
            radioButton.isEnabled = false
            reshootActionGroup!!.addView(radioButton)
        }
        passStatusView = rootView.findViewById(R.id.passStatus)
        toggleButton = rootView.findViewById(R.id.start)
        toggleButton!!.setOnClickListener { v ->
            // only the bell lets the tournament go on
            if (SessionState.emergency) {
                return@setOnClickListener
            }
            vibrate(150, 200)
            if (!running) {
                // back to the timer first: unchecking sends the view change,
                // and the commands reach the displays in the order sent
                lunchBreakBox!!.isChecked = false
                if (reshootAction) {
                    val builder1 = AlertDialog.Builder(v.context)
                    builder1.setMessage(getString(R.string.confirmReShootAction))
                    builder1.setCancelable(true)
                    builder1.setPositiveButton(getString(R.string.yes)) { dialog, _ ->
                        dialog.cancel()
                        startPass()
                    }
                    builder1.setNegativeButton(getString(R.string.no)) { dialog, _ ->
                        dialog.cancel()
                    }
                    val alert11 = builder1.create()
                    alert11.show()
                } else {
                    startPass()
                }
            } else {
                if (!hold) {
                    hold = true
                    sendPause(System.currentTimeMillis())
                } else {
                    hold = false
                    sendRun(System.currentTimeMillis())
                }
            }
        }
        stopButton = rootView.findViewById(R.id.stop)
        stopButton!!.setOnClickListener {
            if (SessionState.emergency) {
                return@setOnClickListener
            }
            vibrate(250, 200)
            running = false
            sendStop(System.currentTimeMillis())
            updateArrowButtons()
        }
        emergencyButton = rootView.findViewById(R.id.emergency)
        emergencyButton!!.setOnClickListener {
            // the bell sets the safety stop, the next press releases it
            vibrate(450, 255)
            sendEmergency(!SessionState.emergency, System.currentTimeMillis())
            updateEmergencyButton()
        }
        resetButton = rootView.findViewById(R.id.reset)
        resetButton!!.setOnClickListener {
            if (SessionState.emergency) {
                return@setOnClickListener
            }
            //vibrator.vibrate(VibrationEffect.createWaveform(new long[]{0,40,80,40,80,40,0},-1));
            sendReset(System.currentTimeMillis())
        }
        setGroupAndPassInfo("AB", 0, sharedPreferences.getInt(getString(R.string.passesCountStore), 0))
        // once every button is there: the page may be opened during a safety stop
        displaysOnline = DisplayRegistry.onlineCount() > 0
        updateEmergencyButton()
        DisplayRegistry.addListener(this)
    }

    /**
     * Confirms a key press in the hand. The tournament leader watches the
     * shooting line, not the phone, and has to feel that the key was hit.
     */
    private fun vibrate(durationMs: Long, amplitude: Int) {
        val manager = context?.getSystemService(VibratorManager::class.java) ?: return
        manager.defaultVibrator.vibrate(VibrationEffect.createOneShot(durationMs, amplitude))
    }

    /** the lunch break as long as its box is checked, else the chosen phase */
    override val currentView: String
        get() = if (lunchBreakBox?.isChecked == true) LunchBreak.VIEW_NAME else getSubViewName()

    override fun viewPayload(sharedPreferences: SharedPreferences): JSONObject? =
        if (lunchBreakBox?.isChecked == true) LunchBreak.payload(sharedPreferences) else null

    private fun getSubViewName(): String {
        var viewName = ""
        if (tournamentPhaseGroup == null) {
            viewName = "tournament"
        } else {
            val id = tournamentPhaseGroup!!.checkedRadioButtonId
            if (id == R.id.modeShootIn) {
                viewName = "shoot_in"
            } else if (id == R.id.modeTournament) {
                viewName = "tournament"
            } else if (id == R.id.modeReShoot) {
                viewName = "re_shoot"
            }
        }
        return viewName
    }
}
