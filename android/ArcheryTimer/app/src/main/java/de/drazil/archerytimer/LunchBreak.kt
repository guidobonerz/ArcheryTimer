package de.drazil.archerytimer

import android.content.SharedPreferences
import androidx.core.content.edit
import org.json.JSONObject
import java.util.Calendar
import java.util.Locale

/**
 * The lunch break view of the displays: the end of the break is set on the
 * settings page, the view is switched on and off on the tournament page.
 */
object LunchBreak {

    const val VIEW_NAME = "lunch"

    private const val TIME_STORE = "lunchTimeStore"
    private const val DEFAULT_TIME = "13:00"

    private val TIME_PATTERN = Regex("[0-9]{2}:[0-9]{2}")

    /** the end of the lunch break as "HH:MM" - the display takes nothing else */
    fun storedTime(sharedPreferences: SharedPreferences): String {
        val time = sharedPreferences.getString(TIME_STORE, DEFAULT_TIME) ?: DEFAULT_TIME
        return if (TIME_PATTERN.matches(time)) time else DEFAULT_TIME
    }

    fun storeTime(sharedPreferences: SharedPreferences, hour: Int, minute: Int) {
        sharedPreferences.edit {
            putString(TIME_STORE, String.format(Locale.ROOT, "%02d:%02d", hour, minute))
        }
    }

    fun hour(sharedPreferences: SharedPreferences): Int =
        storedTime(sharedPreferences).substring(0, 2).toInt()

    fun minute(sharedPreferences: SharedPreferences): Int =
        storedTime(sharedPreferences).substring(3, 5).toInt()

    /**
     * The time as text for the display to show, and the same time of today as
     * seconds since 1970. The displays keep UTC and know no time zone, so they
     * cannot tell on their own when "13:30" is reached.
     */
    fun payload(sharedPreferences: SharedPreferences): JSONObject {
        val end = Calendar.getInstance().apply {
            set(Calendar.HOUR_OF_DAY, hour(sharedPreferences))
            set(Calendar.MINUTE, minute(sharedPreferences))
            set(Calendar.SECOND, 0)
            set(Calendar.MILLISECOND, 0)
        }
        return JSONObject()
            .put("t", storedTime(sharedPreferences))
            .put("e", end.timeInMillis / 1000)
    }
}
