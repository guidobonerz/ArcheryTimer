package de.drazil.archerytimer.udp

import android.content.SharedPreferences
import android.util.Log
import de.drazil.archerytimer.net.ConnectionLog
import de.drazil.archerytimer.net.DisplayRegistry
import de.drazil.archerytimer.net.NetworkMonitor
import de.drazil.archerytimer.net.SessionState
import org.json.JSONObject
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.InetSocketAddress
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicLong

/**
 * Sends the commands to the displays and makes sure they arrive.
 *
 * Udp gives no guarantees, so every command carries a sequence number, is
 * repeated until each display has acknowledged it and is dropped by the
 * display when it has already been carried out. Timing does not suffer from
 * the repeats: every packet also carries how long the command has been under
 * way, and the displays start at (key press + lead time), so a command that
 * only gets through on the third attempt still starts the countdown at exactly
 * the same instant as everywhere else.
 */
object UDPSender {

    private const val BROADCAST_ADDRESS = "255.255.255.255"
    private const val PORT = 5005

    /** copies per attempt - cheap insurance against a single lost packet */
    private const val COPIES = 2

    private const val RETRY_INTERVAL_MS = 200L
    private const val MAX_ATTEMPTS = 6

    // All sends run on this single background thread: it keeps network I/O off the
    // main thread and preserves the order in which commands are queued (e.g. config
    // must reach the display before run/state commands).
    private val sender = Executors.newSingleThreadExecutor { runnable ->
        Thread(runnable, "udp-sender").apply { isDaemon = true }
    }

    private val retries = Executors.newSingleThreadScheduledExecutor { runnable ->
        Thread(runnable, "udp-retry").apply { isDaemon = true }
    }

    // Seeded from the wall clock so the numbers keep growing across app
    // restarts - a display must not mistake a fresh command for a repeat of
    // one it already carried out before the app was restarted.
    private val sequence = AtomicLong(System.currentTimeMillis())
    private val pending = ConcurrentHashMap<Long, Pending>()

    private class Pending(
        val seq: Long,
        val command: String,
        val json: JSONObject,
        val host: String?,
        val startTime: Long,
        val missing: MutableSet<String>
    ) {
        @Volatile
        var attempts: Int = 0

        @Volatile
        var nextAttemptAt: Long = 0
    }

    init {
        retries.scheduleWithFixedDelay(
            ::retryPending,
            RETRY_INTERVAL_MS,
            RETRY_INTERVAL_MS,
            TimeUnit.MILLISECONDS
        )
    }

    fun controlDisplay(name: String, additionalPayload: JSONObject?, startTime: Long) {
        val payload = JSONObject()
        try {
            payload.put("cmd", "change_view")
            payload.put("v", name)
            if (additionalPayload != null) {
                payload.put("val", additionalPayload)
            }
            broadcastJSON(payload, startTime)
        } catch (ex: Exception) {
            Log.e("Error", ex.message ?: "")
        }
    }

    fun broadcastJSON(json: JSONObject, startTime: Long) {
        dispatch(json, null, null, startTime, true)
    }

    fun broadcast(json: JSONObject, host: String, startTime: Long) {
        dispatch(json, null, host, startTime, true)
    }

    /** sends a command to a single display, addressed by its stable id */
    fun sendToDisplay(displayId: String?, host: String?, json: JSONObject, startTime: Long) {
        dispatch(json, displayId, host, startTime, true)
    }

    /** asks every display on the network to announce itself */
    fun sendDiscover() {
        val payload = JSONObject()
        try {
            payload.put("cmd", "discover")
            // discovery is what finds the displays in the first place, so there
            // is nobody who could acknowledge it
            dispatch(payload, null, null, System.currentTimeMillis(), false)
        } catch (ex: Exception) {
            Log.e("Error", ex.message ?: "")
        }
    }

    /**
     * Brings a single display back in step with the others - view, phase and the
     * remaining seconds of the pass that is currently running.
     */
    fun sendResync(displayId: String?, host: String?) {
        val payload = JSONObject()
        try {
            payload.put("cmd", "resync")
            payload.put("val", SessionState.toResyncPayload())
            sendToDisplay(displayId, host, payload, System.currentTimeMillis())
        } catch (ex: Exception) {
            Log.e("Error", ex.message ?: "")
        }
    }

    /** assigns the number a display is shown and sorted by */
    fun sendDisplayNo(displayId: String?, host: String?, displayNo: Int) {
        val payload = JSONObject()
        try {
            val valuesObject = JSONObject()
            valuesObject.put("dn", displayNo)
            payload.put("cmd", "set_display_no")
            payload.put("val", valuesObject)
            sendToDisplay(displayId, host, payload, System.currentTimeMillis())
        } catch (ex: Exception) {
            Log.e("Error", ex.message ?: "")
        }
    }

    /**
     * Sets the clock of a display to the time of this phone, in ms since 1970
     * (UTC). The age every packet carries is added by the display, so a repeat
     * that gets through late still sets the right time.
     */
    fun sendTime(displayId: String?, host: String?) {
        val payload = JSONObject()
        try {
            val now = System.currentTimeMillis()
            val valuesObject = JSONObject()
            valuesObject.put("t", now)
            payload.put("cmd", "set_time")
            payload.put("val", valuesObject)
            sendToDisplay(displayId, host, payload, now)
        } catch (ex: Exception) {
            Log.e("Error", ex.message ?: "")
        }
    }

    fun sendConfiguration(sharedPreferences: SharedPreferences, startTime: Long) {
        sendConfiguration(sharedPreferences, startTime, null, null)
    }

    /**
     * @param displayId when set the configuration only goes to that one display,
     *        so a display that just rejoined can be set up without disturbing
     *        the displays that are already running.
     */
    fun sendConfiguration(
        sharedPreferences: SharedPreferences,
        startTime: Long,
        displayId: String?,
        host: String?
    ) {
        try {
            val valuesObject = JSONObject()
            valuesObject.put("pt", sharedPreferences.getInt("prepareTimeStore", 0))
            valuesObject.put("at", sharedPreferences.getInt("arrowTimeStore", 0))
            valuesObject.put("ac", sharedPreferences.getInt("arrowCountStore", 0))
            valuesObject.put("rac", sharedPreferences.getInt("reshootArrowCountStore", 0))
            valuesObject.put("sit", sharedPreferences.getInt("shootInTimeStore", 0))
            valuesObject.put("wt", sharedPreferences.getInt("warnTimeStore", 0))
            valuesObject.put("g", sharedPreferences.getInt("groupStore", 1))
            valuesObject.put("p", sharedPreferences.getInt("passesCountStore", 1))
            valuesObject.put("fpl", sharedPreferences.getBoolean("flashingPrepareLightStore", true))
            val payload = JSONObject()
            payload.put("cmd", "config")
            payload.put("val", valuesObject)
            dispatch(payload, displayId, host, startTime, true)
        } catch (ex: Exception) {
            Log.e("Error", ex.message ?: "")
        }
    }

    /** a display confirmed that it carried out a command */
    fun onAck(seq: Long, displayKey: String) {
        if (seq == 0L) {
            return
        }
        val entry = pending[seq] ?: return
        val complete: Boolean
        synchronized(entry.missing) {
            entry.missing.remove(displayKey)
            complete = entry.missing.isEmpty()
        }
        if (complete) {
            pending.remove(seq)
            if (entry.attempts > 1) {
                ConnectionLog.log("'${entry.command}' nach ${entry.attempts} Versuchen von allen Displays bestätigt")
            }
        }
    }

    private fun dispatch(
        json: JSONObject,
        displayId: String?,
        host: String?,
        startTime: Long,
        reliable: Boolean
    ) {
        try {
            json.put("src", "controller")
            if (displayId != null) {
                json.put("dst", displayId)
            }

            val command = json.optString("cmd")
            val seq = if (reliable) sequence.incrementAndGet() else 0L
            if (seq != 0L) {
                json.put("seq", seq)
            }

            // the session state must follow the command once, not once per repeat
            SessionState.onCommandSent(json)

            val entry = Pending(seq, command, json, host, startTime, expectedAckers(host))
            if (seq != 0L && entry.missing.isNotEmpty()) {
                pending[seq] = entry
            }
            transmit(entry)
        } catch (ex: Exception) {
            ConnectionLog.log("Senden fehlgeschlagen: ${ex.message}")
        }
    }

    /**
     * The displays that have to confirm. Nothing known yet means nothing to
     * wait for - the command still goes out, it just cannot be checked.
     */
    private fun expectedAckers(host: String?): MutableSet<String> {
        if (host != null) {
            val key = DisplayRegistry.keyOfAddress(host)
            return if (key != null) mutableSetOf(key) else mutableSetOf()
        }
        return DisplayRegistry.onlineKeys().toMutableSet()
    }

    private fun transmit(entry: Pending) {
        entry.attempts++
        entry.nextAttemptAt = System.currentTimeMillis() + RETRY_INTERVAL_MS
        sender.execute {
            try {
                // stamped per attempt: a later attempt reports a larger age, so
                // every display still arrives at the same absolute start instant
                entry.json.put("sd", System.currentTimeMillis() - entry.startTime)
                val message = entry.json.toString().toByteArray()
                newSocket().use { socket ->
                    for (address in targets(entry.host)) {
                        for (i in 0 until COPIES) {
                            socket.send(DatagramPacket(message, message.size, address, PORT))
                        }
                    }
                }
                Log.i(ConnectionLog.TAG, "sent ${entry.json}")
            } catch (ex: Exception) {
                ConnectionLog.log("Senden fehlgeschlagen: ${ex.message}")
            }
        }
    }

    private fun retryPending() {
        val now = System.currentTimeMillis()
        for (entry in pending.values) {
            if (now < entry.nextAttemptAt) {
                continue
            }
            if (entry.attempts >= MAX_ATTEMPTS) {
                pending.remove(entry.seq)
                val missing = synchronized(entry.missing) { entry.missing.toList() }
                for (key in missing) {
                    val number = DisplayRegistry.displayNoOf(key)
                    val name = if (number != null) "Display $number ($key)" else key
                    ConnectionLog.log("$name hat '${entry.command}' nicht bestätigt")
                }
                continue
            }
            transmit(entry)
        }
    }

    /**
     * A fresh socket per send, bound to the network the wifi is currently on.
     * A socket created on an old network stays dead after a reconnect.
     */
    private fun newSocket(): DatagramSocket {
        val socket = DatagramSocket(null)
        socket.reuseAddress = true
        socket.broadcast = true
        socket.bind(InetSocketAddress(0))
        NetworkMonitor.bindSocket(socket)
        return socket
    }

    private fun targets(host: String?): List<InetAddress> {
        if (host != null) {
            return listOf(InetAddress.getByName(host))
        }
        val addresses = mutableListOf(InetAddress.getByName(BROADCAST_ADDRESS))
        // some access points drop the all-ones broadcast but forward the
        // directed one, so both are used
        NetworkMonitor.subnetBroadcastAddress()?.let {
            if (!addresses.contains(it)) {
                addresses.add(it)
            }
        }
        return addresses
    }
}
