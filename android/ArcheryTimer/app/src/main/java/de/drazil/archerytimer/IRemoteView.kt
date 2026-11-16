package de.drazil.archerytimer

import android.content.SharedPreferences
import org.json.JSONObject

interface IRemoteView {

    val currentView: String

    /**
     * What the display needs besides the name of the view, null if nothing.
     * The preferences are handed in because this is asked for before the
     * fragment is attached to the activity.
     */
    fun viewPayload(sharedPreferences: SharedPreferences): JSONObject? = null
}
