package de.drazil.archerytimer.net

import org.json.JSONObject

/**
 * What the controller believes the displays are currently doing. A display that
 * rejoins in the middle of a pass gets this state pushed to it so it does not
 * stay behind on an old view or run out of step with the other displays.
 */
object SessionState {

    @Volatile
    var viewName: String = "home"

    /** end of the lunch break as shown by the lunch view, "HH:MM" */
    @Volatile
    var lunchTime: String = ""

    /** the same instant in seconds since 1970, 0 = unknown */
    @Volatile
    var lunchEnd: Long = 0L

    @Volatile
    var running: Boolean = false

    @Volatile
    var hold: Boolean = false

    @Volatile
    var prepare: Boolean = true

    @Volatile
    var remainingSeconds: Int = 0

    @Volatile
    var currentGroup: Int = 1

    @Volatile
    var currentPass: Int = 1

    @Volatile
    var groupSet: Int = 0

    /**
     * True while the controller cannot know what the displays are doing.
     *
     * It is set whenever the activity leaves the screen: nothing is received
     * while the app is in the background, so the values above describe the
     * moment the phone was put away, not the present. Pushing them to a
     * display that kept counting all along is what took the displays out of
     * step after a wake-up, so a stale state must never be sent as a resync -
     * it is rebuilt from the displays first.
     */
    @Volatile
    var stale: Boolean = false
        private set

    /** when the reporting display last told the controller what is going on */
    @Volatile
    private var lastStateAt: Long = 0L

    fun markStale() {
        stale = true
    }

    /**
     * True when the controller cannot trust what it knows: either it was not
     * listening, or it believes a pass is running while the display that
     * reports has gone quiet - a running pass reports every second, so silence
     * means the pass has ended and the message was lost.
     */
    fun needsRefresh(): Boolean =
        stale || (running && System.currentTimeMillis() - lastStateAt > STATE_SILENCE_MS)

    /** called for every command the controller sends out */
    fun onCommandSent(json: JSONObject) {
        when (json.optString("cmd")) {
            "change_view" -> {
                // the displays ignore a view change while a pass is running,
                // so the controller must not record one either
                if (!running) {
                    viewName = json.optString("v", viewName)
                    val values = json.optJSONObject("val")
                    val time = values?.optString("t").orEmpty()
                    if (time.isNotEmpty()) {
                        lunchTime = time
                        lunchEnd = values?.optLong("e", 0L) ?: 0L
                    }
                    hold = false
                    prepare = true
                    remainingSeconds = 0
                }
            }

            "run" -> {
                // the user just defined the state, so it is known again
                stale = false
                lastStateAt = System.currentTimeMillis()
                running = true
                hold = false
            }

            "pause" -> hold = true

            "stop", "reset", "emergency" -> {
                stale = false
                running = false
                hold = false
                prepare = true
                remainingSeconds = 0
                currentGroup = 1
                currentPass = 1
                groupSet = 0
            }
        }
    }

    /**
     * Called for every state packet of the reporting display - and for its
     * heartbeats, which carry the same fields. Whatever the display reports is
     * the truth, so the controller is up to date again afterwards.
     */
    fun onStateReceived(json: JSONObject) {
        stale = false
        lastStateAt = System.currentTimeMillis()
        currentGroup = json.optInt("cg", currentGroup)
        currentPass = json.optInt("cp", currentPass)
        groupSet = json.optInt("gs", groupSet)
        val remainingPrepare = json.optInt("cpt", 0)
        val remainingAction = json.optInt("cat", 0)
        prepare = remainingPrepare > 0
        remainingSeconds = if (prepare) remainingPrepare else remainingAction
        if (json.has("ph")) {
            running = json.optInt("ph", 0) == 1
        }
        if (json.has("h")) {
            hold = json.optBoolean("h", hold)
        }
    }

    fun onDisplayStopped() {
        stale = false
        lastStateAt = System.currentTimeMillis()
        running = false
        hold = false
        remainingSeconds = 0
    }

    /** a running pass reports every second, so this is a long silence */
    private const val STATE_SILENCE_MS = 4000L

    /** payload of the resync command sent to a display that just rejoined */
    fun toResyncPayload(): JSONObject = JSONObject().apply {
        put("v", viewName)
        put("r", running && remainingSeconds > 0)
        put("h", hold)
        put("pr", prepare)
        put("t", remainingSeconds)
        put("cg", currentGroup)
        put("cp", currentPass)
        put("gs", groupSet)
        // local time minus UTC - the displays know no time zone of their own
        put("tz", java.util.TimeZone.getDefault().getOffset(System.currentTimeMillis()) / 1000)
        if (lunchTime.isNotEmpty()) {
            put("lt", lunchTime)
            put("le", lunchEnd)
        }
    }
}
