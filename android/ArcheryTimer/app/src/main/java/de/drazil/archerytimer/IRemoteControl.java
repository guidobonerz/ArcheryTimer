package de.drazil.archerytimer;

import org.json.JSONObject;

public interface IRemoteControl {

    public void handleResponse(JSONObject json);
}
