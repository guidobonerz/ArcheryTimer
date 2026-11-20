package de.drazil.archerytimer

import org.json.JSONObject

interface IRemoteControl {

    fun handleResponse(json: JSONObject)
}
