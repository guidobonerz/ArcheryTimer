package de.drazil.archerytimer

import android.graphics.Color
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.fragment.app.Fragment
import de.drazil.archerytimer.net.ConnectionLog
import de.drazil.archerytimer.net.DisplayRegistry
import de.drazil.archerytimer.udp.UDPSender

/**
 * Shows which displays are on the network and the protocol of everything that
 * happened to the connections - including every display that came back.
 */
class StateFragment : Fragment(), IRemoteView, DisplayRegistry.Listener, ConnectionLog.Listener {

    private var header: TextView? = null
    private var displayList: LinearLayout? = null
    private var logView: TextView? = null
    private var logScroll: ScrollView? = null

    private val handler = Handler(Looper.getMainLooper())

    /** keeps the "last seen" column moving while nothing arrives */
    private val refresh = object : Runnable {
        override fun run() {
            renderDisplays(DisplayRegistry.snapshot())
            handler.postDelayed(this, REFRESH_INTERVAL_MS)
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        return inflater.inflate(R.layout.fragment_state, container, false)
    }

    override fun onViewCreated(rootView: View, savedInstanceState: Bundle?) {
        super.onViewCreated(rootView, savedInstanceState)
        header = rootView.findViewById(R.id.displayHeader)
        displayList = rootView.findViewById(R.id.displayList)
        logView = rootView.findViewById(R.id.logView)
        logScroll = rootView.findViewById(R.id.logScroll)

        rootView.findViewById<Button>(R.id.clearLog).setOnClickListener {
            ConnectionLog.clear()
        }
    }

    override fun onStart() {
        super.onStart()
        DisplayRegistry.addListener(this)
        ConnectionLog.addListener(this)
        renderDisplays(DisplayRegistry.snapshot())
        onLogChanged()
        handler.postDelayed(refresh, REFRESH_INTERVAL_MS)
        UDPSender.sendDiscover()
    }

    override fun onStop() {
        super.onStop()
        handler.removeCallbacks(refresh)
        DisplayRegistry.removeListener(this)
        ConnectionLog.removeListener(this)
    }

    override fun onDisplaysChanged(displays: List<DisplayRegistry.Display>) {
        renderDisplays(displays)
    }

    override fun onDisplayAvailable(display: DisplayRegistry.Display, reason: DisplayRegistry.Reason) {
        // the log entry is written by the registry, nothing to do here
    }

    override fun onLogChanged() {
        val view = logView ?: return
        view.text = ConnectionLog.formatted()
        logScroll?.post { logScroll?.fullScroll(View.FOCUS_DOWN) }
    }

    private fun renderDisplays(displays: List<DisplayRegistry.Display>) {
        val list = displayList ?: return
        val context = context ?: return

        val online = displays.count { it.online }
        header?.text = getString(R.string.displaysOnline, online, displays.size)

        list.removeAllViews()
        if (displays.isEmpty()) {
            list.addView(row(context, getString(R.string.noDisplays), Color.LTGRAY))
            return
        }

        val now = System.currentTimeMillis()
        val reporting = DisplayRegistry.reportingDisplayId()
        for (display in displays) {
            val age = (now - display.lastSeen) / 1000
            val marker = if (display.online) "●" else "○"
            val role = if (display.id == reporting) " ★" else ""
            val text = "$marker Display ${display.displayNo}$role  ${display.id}\n" +
                    "    ${display.ip}  ${display.rssi} dBm  " +
                    getString(R.string.lastSeenSeconds, age)
            list.addView(row(context, text, if (display.online) ONLINE_COLOR else OFFLINE_COLOR))
        }
    }

    private fun row(context: android.content.Context, text: String, color: Int): TextView =
        TextView(context).apply {
            this.text = text
            setTextColor(color)
            textSize = 14f
            gravity = Gravity.START
            setPadding(0, 8, 0, 8)
        }

    override val currentView: String
        get() = "state"

    companion object {
        private const val REFRESH_INTERVAL_MS = 1000L
        private val ONLINE_COLOR = Color.rgb(0, 200, 83)
        private val OFFLINE_COLOR = Color.rgb(229, 57, 53)
    }
}
