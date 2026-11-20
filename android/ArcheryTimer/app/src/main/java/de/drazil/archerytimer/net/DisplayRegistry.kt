package de.drazil.archerytimer.net

import android.os.Handler
import android.os.Looper
import org.json.JSONObject
import java.util.concurrent.CopyOnWriteArrayList

/**
 * Keeps track of every display that has announced itself. Displays send a
 * "hello" whenever they (re)join the network and a "heartbeat" every few
 * seconds, so a display that is carried out of range can be detected and,
 * more importantly, recognised when it comes back.
 */
object DisplayRegistry {

    /** a display counts as gone after roughly three missed heartbeats */
    private const val OFFLINE_TIMEOUT_MS = 10_000L

    /** a hello is sent three times in a row, only the first one is acted upon */
    private const val ANNOUNCE_DEBOUNCE_MS = 2_000L

    data class Display(
        /** key of this display in the registry - its id, or its address when the id is unusable */
        val id: String,
        /** the id the display reported, null when it did not report a usable one */
        val deviceId: String?,
        val displayNo: Int,
        val ip: String,
        val rssi: Int,
        val uptimeSeconds: Long,
        val lastSeen: Long,
        val online: Boolean,
        val offlineSince: Long
    )

    /** why a display has to be brought up to date */
    enum class Reason {
        /** heard from for the first time */
        NEW,

        /** was out of reach and is back */
        REJOINED,

        /** restarted and lost everything it knew */
        REBOOTED
    }

    interface Listener {
        /** any change to the display table - use it to refresh the ui */
        fun onDisplaysChanged(displays: List<Display>)

        /** a display joined or rejoined the network and needs to be brought up to date */
        fun onDisplayAvailable(display: Display, reason: Reason)
    }

    private val displays = LinkedHashMap<String, Display>()
    private val lastAnnouncement = HashMap<String, Long>()
    private val listeners = CopyOnWriteArrayList<Listener>()
    private val mainHandler = Handler(Looper.getMainLooper())

    /**
     * Handles a "hello" or "heartbeat" packet. Returns true when the packet was
     * a presence message.
     */
    fun onPresence(json: JSONObject, senderIp: String): Boolean {
        val command = json.optString("cmd")
        if (command != "hello" && command != "heartbeat") {
            return false
        }

        val reportedId = json.optString("id")
        val ip = json.optString("ip").ifEmpty { senderIp }
        val id = keyFor(reportedId, ip)
        val now = System.currentTimeMillis()
        val booted = json.optBoolean("boot", false)

        val previous: Display?
        val current: Display
        synchronized(displays) {
            previous = displays[id]
            current = Display(
                id = id,
                deviceId = reportedId.takeIf { isUsableId(it) },
                displayNo = json.optInt("dn", previous?.displayNo ?: 1),
                ip = ip,
                rssi = json.optInt("rssi", previous?.rssi ?: 0),
                uptimeSeconds = json.optLong("up", 0L),
                lastSeen = now,
                online = true,
                offlineSince = 0L
            )
            displays[id] = current
        }

        // A display only has to be brought up to date when it lost the thread:
        // it is new, it was away, or it restarted. The answer to a discover
        // from a display that has been on air all along says nothing is wrong
        // with it - resyncing it would only disturb a running countdown.
        val reason = when {
            previous == null -> Reason.NEW
            !previous.online -> Reason.REJOINED
            booted && command == "hello" -> Reason.REBOOTED
            else -> null
        }

        val event = when (reason) {
            Reason.NEW -> "verbunden (${current.ip}, ${current.rssi} dBm)"
            Reason.REJOINED -> {
                val gap = (now - previous!!.offlineSince).coerceAtLeast(0L) / 1000
                "wieder verbunden nach ${gap}s (${current.ip}, ${current.rssi} dBm)"
            }
            Reason.REBOOTED -> "neu gestartet (${current.ip})"
            null -> null
        }

        if (event != null && reason != null && notAnnouncedRecently(id, now)) {
            ConnectionLog.log("${describe(current)} $event")
            notifyAvailable(current, reason)
        }

        notifyChanged()
        return true
    }

    /**
     * A display that reports state is obviously on the network, even if its
     * hello was missed. Without this the arbitration below would have nothing
     * to go by and would let every display drive the ui.
     */
    fun touch(json: JSONObject, senderIp: String) {
        val reportedId = json.optString("id")
        val ip = json.optString("ip").ifEmpty { senderIp }
        val id = keyFor(reportedId, ip)
        val now = System.currentTimeMillis()

        val created: Boolean
        synchronized(displays) {
            val previous = displays[id]
            created = previous == null
            displays[id] = Display(
                id = id,
                deviceId = reportedId.takeIf { isUsableId(it) } ?: previous?.deviceId,
                displayNo = json.optInt("dn", previous?.displayNo ?: 1),
                ip = ip,
                rssi = previous?.rssi ?: 0,
                uptimeSeconds = previous?.uptimeSeconds ?: 0L,
                lastSeen = now,
                online = true,
                offlineSince = 0L
            )
        }
        if (created) {
            notifyChanged()
        }
    }

    /**
     * Displays are told apart by the id they report. A firmware that cannot
     * read its mac would send the same id on every board, so an unusable id
     * falls back to the network address - two boards must never collapse into
     * one entry.
     */
    private fun keyFor(id: String, address: String): String =
        if (isUsableId(id)) id else address.ifEmpty { id }

    private fun isUsableId(id: String): Boolean = id.isNotEmpty() && id.any { it != '0' }

    /**
     * Nothing is received while the app is in the background, so every display
     * looks unreachable the moment it comes back. Giving them a fresh grace
     * period keeps them from being declared lost - and resynced - for a gap
     * that was on the phone side, not on theirs.
     */
    fun resumeAfterPause() {
        val now = System.currentTimeMillis()
        synchronized(displays) {
            for ((id, display) in displays) {
                if (display.online) {
                    displays[id] = display.copy(lastSeen = now)
                }
            }
        }
    }

    /** marks displays that stopped sending heartbeats as offline */
    fun sweep() {
        val now = System.currentTimeMillis()
        val lost = mutableListOf<Display>()
        synchronized(displays) {
            for ((id, display) in displays) {
                if (display.online && now - display.lastSeen > OFFLINE_TIMEOUT_MS) {
                    val offline = display.copy(online = false, offlineSince = display.lastSeen)
                    displays[id] = offline
                    lost.add(offline)
                }
            }
        }
        for (display in lost) {
            ConnectionLog.log("${describe(display)} nicht mehr erreichbar")
        }
        if (lost.isNotEmpty()) {
            notifyChanged()
        }
    }

    fun snapshot(): List<Display> = synchronized(displays) {
        displays.values.sortedWith(compareBy({ it.displayNo }, { it.id }))
    }

    fun onlineCount(): Int = snapshot().count { it.online }

    /**
     * The display that reports the tournament state. With n displays only one
     * may drive the controller ui, otherwise the values overwrite each other.
     * The lowest display number wins, ties are broken by the id.
     */
    fun reportingDisplayId(): String? =
        snapshot().firstOrNull { it.online }?.id

    /** the registry key of the display a packet came from */
    fun keyOf(json: JSONObject, senderIp: String): String =
        keyFor(json.optString("id"), json.optString("ip").ifEmpty { senderIp })

    /** the displays a broadcast command has to be acknowledged by */
    fun onlineKeys(): Set<String> = snapshot().filter { it.online }.map { it.id }.toSet()

    /** the registry key of the display with that address, if it is known */
    fun keyOfAddress(address: String): String? =
        snapshot().firstOrNull { it.ip == address }?.id

    fun displayNoOf(key: String): Int? = snapshot().firstOrNull { it.id == key }?.displayNo

    /** true when a state packet from this display may be applied to the ui */
    fun mayReportState(json: JSONObject, senderIp: String): Boolean {
        val reporting = reportingDisplayId() ?: return true
        val id = keyFor(json.optString("id"), json.optString("ip").ifEmpty { senderIp })
        return reporting == id
    }

    private fun notAnnouncedRecently(id: String, now: Long): Boolean =
        synchronized(lastAnnouncement) {
            val last = lastAnnouncement[id] ?: 0L
            if (now - last < ANNOUNCE_DEBOUNCE_MS) {
                false
            } else {
                lastAnnouncement[id] = now
                true
            }
        }

    fun forget(id: String) {
        synchronized(displays) { displays.remove(id) }
        synchronized(lastAnnouncement) { lastAnnouncement.remove(id) }
        notifyChanged()
    }

    fun clear() {
        synchronized(displays) { displays.clear() }
        synchronized(lastAnnouncement) { lastAnnouncement.clear() }
        notifyChanged()
    }

    fun addListener(listener: Listener) {
        listeners.addIfAbsent(listener)
        notifyChanged()
    }

    fun removeListener(listener: Listener) {
        listeners.remove(listener)
    }

    private fun describe(display: Display): String =
        "Display ${display.displayNo} (${display.id})"

    private fun notifyChanged() {
        val current = snapshot()
        mainHandler.post {
            for (listener in listeners) {
                listener.onDisplaysChanged(current)
            }
        }
    }

    private fun notifyAvailable(display: Display, reason: Reason) {
        mainHandler.post {
            for (listener in listeners) {
                listener.onDisplayAvailable(display, reason)
            }
        }
    }
}
