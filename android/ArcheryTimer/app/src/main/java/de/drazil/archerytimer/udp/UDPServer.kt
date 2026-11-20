package de.drazil.archerytimer.udp

import android.net.Network
import android.os.Handler
import android.os.Looper
import android.util.Log
import de.drazil.archerytimer.IRemoteControl
import de.drazil.archerytimer.net.ConnectionLog
import de.drazil.archerytimer.net.DisplayRegistry
import de.drazil.archerytimer.net.NetworkMonitor
import de.drazil.archerytimer.net.SessionState
import org.json.JSONObject
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetSocketAddress
import java.net.SocketTimeoutException
import java.util.concurrent.CopyOnWriteArrayList
import kotlin.math.abs

/**
 * Receives everything the displays send.
 *
 * The receiver used to die silently: when the wifi went away the blocking
 * receive threw, the loop was left and nobody ever restarted it. It now runs
 * with a read timeout, rebuilds its socket after every failure and after every
 * change of the underlying network, so it comes back on its own.
 */
object UDPServer : NetworkMonitor.Listener {

    private const val PORT = 5005
    private const val RECEIVE_TIMEOUT_MS = 1000
    private const val BACKOFF_MIN_MS = 500L
    private const val BACKOFF_MAX_MS = 5000L
    private const val BUFFER_SIZE = 1024

    /**
     * How far the clock of a display may be off before it is set. A heartbeat
     * is a broadcast and can sit in the access point for a few hundred ms
     * before it is delivered, so this must not be too tight.
     */
    private const val CLOCK_TOLERANCE_MS = 2000L

    /** a display is sent the time at most this often */
    private const val CLOCK_SYNC_INTERVAL_MS = 10_000L

    /** when each display was last sent the time - only touched by the receiving thread */
    private val lastClockSync = HashMap<String, Long>()

    private val listeners = CopyOnWriteArrayList<IRemoteControl>()

    // the fragments update views from handleResponse, so they must not be
    // called on the receiving thread
    private val mainHandler = Handler(Looper.getMainLooper())

    @Volatile
    private var running = false

    @Volatile
    private var socket: DatagramSocket? = null

    /** set when the socket has to be thrown away and built again */
    @Volatile
    private var rebindRequested = false

    private var thread: Thread? = null

    @Synchronized
    fun start() {
        if (running && thread?.isAlive == true) {
            return
        }
        running = true
        NetworkMonitor.addListener(this)
        thread = Thread(::receiveLoop, "udp-server").apply {
            isDaemon = true
            start()
        }
        ConnectionLog.log("Empfänger gestartet")
    }

    @Synchronized
    fun stop() {
        running = false
        NetworkMonitor.removeListener(this)
        closeSocket()
        thread = null
        ConnectionLog.log("Empfänger gestoppt")
    }

    fun addRemoteControl(remoteControl: IRemoteControl) {
        listeners.addIfAbsent(remoteControl)
    }

    fun removeRemoteControl(remoteControl: IRemoteControl) {
        listeners.remove(remoteControl)
    }

    override fun onNetworkChanged(network: Network?) {
        // the socket belongs to the old network and would never receive again
        rebindRequested = true
        closeSocket()
    }

    private fun receiveLoop() {
        var backoff = BACKOFF_MIN_MS
        val buffer = ByteArray(BUFFER_SIZE)

        while (running) {
            val active = socket
            if (active == null || active.isClosed) {
                try {
                    socket = openSocket()
                    rebindRequested = false
                    backoff = BACKOFF_MIN_MS
                    ConnectionLog.log("Empfänger lauscht auf Port $PORT")
                    // whoever is out there: report in
                    UDPSender.sendDiscover()
                } catch (ex: Exception) {
                    ConnectionLog.log("Empfänger konnte nicht starten: ${ex.message}")
                    sleep(backoff)
                    backoff = (backoff * 2).coerceAtMost(BACKOFF_MAX_MS)
                }
                continue
            }

            try {
                val packet = DatagramPacket(buffer, buffer.size)
                active.receive(packet)
                handlePacket(String(buffer, 0, packet.length), packet.address?.hostAddress ?: "")
            } catch (ignored: SocketTimeoutException) {
                // nothing arrived within the timeout, just keep going - this is
                // what keeps the loop responsive to stop() and to rebinds
            } catch (ex: Exception) {
                if (running) {
                    ConnectionLog.log("Empfang unterbrochen (${ex.message}), verbinde neu")
                    closeSocket()
                    sleep(backoff)
                    backoff = (backoff * 2).coerceAtMost(BACKOFF_MAX_MS)
                }
            }

            if (rebindRequested) {
                closeSocket()
            }
        }
        closeSocket()
    }

    private fun openSocket(): DatagramSocket {
        // reuseAddress has to be set before the bind, otherwise it has no effect
        // and rebinding after a network change fails with "address in use"
        val created = DatagramSocket(null)
        created.reuseAddress = true
        created.broadcast = true
        created.soTimeout = RECEIVE_TIMEOUT_MS
        created.bind(InetSocketAddress(PORT))
        NetworkMonitor.bindSocket(created)
        return created
    }

    private fun closeSocket() {
        socket?.let { runCatching { it.close() } }
        socket = null
    }

    private fun handlePacket(text: String, senderIp: String) {
        val json = try {
            JSONObject(text)
        } catch (ex: Exception) {
            Log.w(ConnectionLog.TAG, "unparsable packet from $senderIp: $text")
            return
        }

        if (json.optString("src") != "display") {
            return
        }

        if (DisplayRegistry.onPresence(json, senderIp)) {
            adoptStateFromPresence(json, senderIp)
            keepClockInSync(json, senderIp)
            return // presence traffic never reaches the ui
        }

        // every display acknowledges every command, not just the reporting one
        if (json.optString("cmd") == "ack") {
            UDPSender.onAck(json.optLong("seq", 0L), DisplayRegistry.keyOf(json, senderIp))
            return
        }

        DisplayRegistry.touch(json, senderIp)

        // with n displays only the reporting one may drive the ui
        if (!DisplayRegistry.mayReportState(json, senderIp)) {
            return
        }

        Log.i(ConnectionLog.TAG, "received $text")

        when (json.optString("cmd")) {
            "state" -> SessionState.onStateReceived(json)
            "stop", "reset" -> SessionState.onDisplayStopped()
            // set or released by the other controller
            "emergency" -> SessionState.emergency = json.optBoolean("e", false)
        }

        mainHandler.post {
            for (listener in listeners) {
                try {
                    listener.handleResponse(json)
                } catch (ex: Exception) {
                    Log.e(ConnectionLog.TAG, "listener failed", ex)
                }
            }
        }
    }

    /**
     * A heartbeat carries what the display is doing. After the app was in the
     * background the controller has no idea any more, so it takes the answer
     * from the display that reports instead of pushing its own outdated view
     * back onto the displays.
     */
    private fun adoptStateFromPresence(json: JSONObject, senderIp: String) {
        if (!SessionState.needsRefresh() || !json.has("ph")) {
            return
        }
        if (!DisplayRegistry.mayReportState(json, senderIp)) {
            return
        }

        SessionState.onStateReceived(json)
        ConnectionLog.log("Stand von Display ${json.optInt("dn", 0)} übernommen")

        // the fragments only listen for state packets, so the heartbeat is
        // handed to them as one
        val mirrored = runCatching { JSONObject(json.toString()).put("cmd", "state") }.getOrNull()
            ?: return
        mainHandler.post {
            for (listener in listeners) {
                try {
                    listener.handleResponse(mirrored)
                } catch (ex: Exception) {
                    Log.e(ConnectionLog.TAG, "listener failed", ex)
                }
            }
        }
    }

    /**
     * Every hello and heartbeat carries the clock of the display. When it is
     * off - never set, drifted, or the phone changed its own time - the display
     * is sent the time of this phone. That covers a new display, one that was
     * away and one that has been running for days with the same check.
     */
    private fun keepClockInSync(json: JSONObject, senderIp: String) {
        if (!json.has("t")) {
            return // a firmware without a clock
        }
        val now = System.currentTimeMillis()
        val drift = json.optLong("t", 0L) - now
        if (abs(drift) <= CLOCK_TOLERANCE_MS) {
            return
        }
        val key = DisplayRegistry.keyOf(json, senderIp)
        if (now - (lastClockSync[key] ?: 0L) < CLOCK_SYNC_INTERVAL_MS) {
            return // the correction is on its way, give it time to show
        }
        lastClockSync[key] = now

        val display = DisplayRegistry.snapshot().firstOrNull { it.id == key } ?: return
        UDPSender.sendTime(display.deviceId, display.ip.ifEmpty { null })
        val reason = if (json.optLong("t", 0L) == 0L) "nicht gestellt" else "weicht ${drift / 1000}s ab"
        ConnectionLog.log("Uhr von Display ${display.displayNo} $reason - Zeit gesendet")
    }

    private fun sleep(millis: Long) {
        try {
            Thread.sleep(millis)
        } catch (ex: InterruptedException) {
            Thread.currentThread().interrupt()
        }
    }
}
