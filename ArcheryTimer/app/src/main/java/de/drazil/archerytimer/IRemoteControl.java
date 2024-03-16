package de.drazil.archerytimer;

public interface IRemoteControl {
    public void remoteTimerResponse(String command);

    public void remoteTimerStatusResponse(String status[]);
}
