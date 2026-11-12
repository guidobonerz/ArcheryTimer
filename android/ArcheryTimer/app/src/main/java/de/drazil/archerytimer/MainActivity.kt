package de.drazil.archerytimer

import android.content.Context
import android.net.Network
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.WindowManager
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import de.drazil.archerytimer.databinding.ActivityMainBinding
import de.drazil.archerytimer.net.ConnectionLog
import de.drazil.archerytimer.net.DisplayRegistry
import de.drazil.archerytimer.net.NetworkMonitor
import de.drazil.archerytimer.net.SessionState
import de.drazil.archerytimer.udp.UDPSender
import de.drazil.archerytimer.udp.UDPServer
import androidx.core.content.edit
import com.google.android.material.snackbar.Snackbar

class MainActivity : AppCompatActivity(), DisplayRegistry.Listener, NetworkMonitor.Listener {

    private lateinit var binding: ActivityMainBinding

    private val handler = Handler(Looper.getMainLooper())

    private var offlineCount = 0

    /**
     * Displays that asked to be brought up to date while the controller did
     * not know what was going on. They are served as soon as a display has
     * reported the current state, or at the latest when the wait runs out.
     */
    private val pendingSync = LinkedHashMap<String, DisplayRegistry.Display>()

    private var pendingSyncDeadline = 0L

    /** marks displays as offline once their heartbeats stop */
    private val sweeper = object : Runnable {
        override fun run() {
            DisplayRegistry.sweep()
            flushPendingSync()
            handler.postDelayed(this, SWEEP_INTERVAL_MS)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // The controller drives the displays and is watched during a pass, so
        // the screen has to stay on. Letting the phone doze off also cut the
        // receiver and left the app with an outdated picture of the displays.
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        val sharedPreferences = getPreferences(Context.MODE_PRIVATE)
        sharedPreferences.edit {
            putInt(getString(R.string.groupStore), 1)
            putInt(getString(R.string.passesCountStore), 10)
            putFloat(getString(R.string.volumeStore), 10.0f)
            putInt(getString(R.string.arrowCountStore), 3)
            putInt(getString(R.string.arrowTimeStore), 30)
            putInt(getString(R.string.warnTimeStore), 30)
            putInt(getString(R.string.reshootArrowCountStore), 0)
            putInt(getString(R.string.prepareTimeStore), 10)
            putInt(getString(R.string.shootInTimeStore), 45)
            putBoolean(getString(R.string.flashingPrepareLightStore), true)
        }
        if (savedInstanceState == null) {
            displayFragment(HomeFragment::class.java, "home")
        }
        binding.bottomNavigationView.setOnItemSelectedListener { item ->
            when (item.itemId) {
                R.id.home -> displayFragment(HomeFragment::class.java, "home")
                R.id.tournament -> displayFragment(TournamentFragment::class.java, "tournament")
                R.id.settings -> displayFragment(SettingsFragment::class.java, "setup")
                R.id.state -> displayFragment(StateFragment::class.java, "state")
            }
            true
        }
    }

    /**
     * The receiver lives as long as the activity is on screen. It used to be
     * started and stopped by the tournament fragment, so switching tabs took
     * the whole communication down.
     */
    override fun onStart() {
        super.onStart()
        // the gap was on this side: the displays were never really away
        DisplayRegistry.resumeAfterPause()
        NetworkMonitor.start(this)
        NetworkMonitor.addListener(this)
        DisplayRegistry.addListener(this)
        UDPServer.start()
        handler.postDelayed(sweeper, SWEEP_INTERVAL_MS)
        UDPSender.sendDiscover()
    }

    override fun onStop() {
        super.onStop()
        // from here on nothing is received, so whatever the controller knows
        // about the running pass ages - it must not be pushed to a display
        // before it has been confirmed by one.
        SessionState.markStale()
        handler.removeCallbacks(sweeper)
        DisplayRegistry.removeListener(this)
        NetworkMonitor.removeListener(this)
        UDPServer.stop()
        NetworkMonitor.stop()
    }

    override fun onNetworkChanged(network: Network?) {
        if (network != null) {
            // the displays may have been running all along - find out who is there
            UDPSender.sendDiscover()
        }
    }

    /**
     * With n displays a toast per display would flood the screen, so only the
     * number of unreachable displays is reported and only when it grows.
     */
    override fun onDisplaysChanged(displays: List<DisplayRegistry.Display>) {
        val offline = displays.count { !it.online }
        if (offline > offlineCount) {
            showNotice(getString(R.string.displaysOffline, offline))
        }
        offlineCount = offline
    }

    /**
     * A display announced itself. It gets the current configuration and, if a
     * pass is running, the position of the other displays, so it does not stay
     * behind on an old view.
     *
     * Right after the app comes back to the screen the controller does not yet
     * know what the displays are doing. Sending the state of the moment the
     * phone was put away would rewind a countdown that has been running
     * correctly all along, so the display is put on hold until a display has
     * reported in.
     */
    override fun onDisplayAvailable(display: DisplayRegistry.Display, reason: DisplayRegistry.Reason) {
        if (reason == DisplayRegistry.Reason.REJOINED) {
            showNotice(getString(R.string.displayReconnected, display.displayNo))
        }

        if (SessionState.needsRefresh()) {
            pendingSync[display.id] = display
            if (pendingSyncDeadline == 0L) {
                pendingSyncDeadline = System.currentTimeMillis() + STATE_REFRESH_TIMEOUT_MS
            }
            return
        }
        syncDisplay(display)
    }

    /** brings the displays that were waiting for a known state up to date */
    private fun flushPendingSync() {
        if (pendingSync.isEmpty()) {
            return
        }
        if (SessionState.needsRefresh() && System.currentTimeMillis() < pendingSyncDeadline) {
            return // a display may still report in
        }
        if (SessionState.needsRefresh()) {
            ConnectionLog.log("Kein Display meldet einen Stand - synchronisiere mit dem letzten bekannten")
        }
        val waiting = pendingSync.values.toList()
        pendingSync.clear()
        pendingSyncDeadline = 0L
        for (display in waiting) {
            syncDisplay(display)
        }
    }

    private fun syncDisplay(display: DisplayRegistry.Display) {
        val startTime = System.currentTimeMillis()
        val host = display.ip.ifEmpty { null }
        UDPSender.sendConfiguration(
            getPreferences(Context.MODE_PRIVATE),
            startTime,
            display.deviceId,
            host
        )
        UDPSender.sendResync(display.deviceId, host)
        ConnectionLog.log("Display ${display.displayNo} neu synchronisiert")
    }

    private fun showNotice(text: String) {
        runCatching {
            Snackbar.make(binding.root, text, Snackbar.LENGTH_SHORT).show()
        }
    }

    private fun displayFragment(fragmentClass: Class<out Fragment>, tagName: String) {
        val fm = supportFragmentManager
        val ft = fm.beginTransaction()
        try {
            for (fragment in fm.fragments) {
                if (!fragment.tag.equals(tagName, ignoreCase = true)) {
                    ft.hide(fragment)
                }
            }
            var f = fm.findFragmentByTag(tagName)
            if (f == null) {
                f = fragmentClass.newInstance()
                ft.add(R.id.frame_layout, f, tagName)

                UDPSender.sendConfiguration(getPreferences(Context.MODE_PRIVATE), System.currentTimeMillis())
            } else {
                ft.show(f)
            }
            ft.commit()
            UDPSender.controlDisplay((f as IRemoteView).currentView, null, System.currentTimeMillis())
        } catch (ex: Exception) {
            Log.e("Error", ex.toString())
        }
    }

    companion object {
        private const val SWEEP_INTERVAL_MS = 2000L

        /**
         * How long a display that needs a resync waits for the controller to
         * learn the current state. The displays send a heartbeat every three
         * seconds, and a running pass reports every second.
         */
        private const val STATE_REFRESH_TIMEOUT_MS = 5000L
    }
}
