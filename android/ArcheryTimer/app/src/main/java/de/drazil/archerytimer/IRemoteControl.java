package de.drazil.archerytimer;

import org.json.JSONObject;

public interface IRemoteControl {
    public void remoteTimerResponse(String command);

    public void remoteTimerStatusResponse(String status[]);

    public void handleResponse(JSONObject json);
}
