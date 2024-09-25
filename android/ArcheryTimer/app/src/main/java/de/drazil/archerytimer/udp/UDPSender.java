package de.drazil.archerytimer.udp;

import android.content.SharedPreferences;
import android.os.StrictMode;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.util.Collections;
import java.util.List;

import de.drazil.archerytimer.R;

public class UDPSender {


    public static void controlDisplay(String name, JSONObject additionalPayload) {
        JSONObject payload = new JSONObject();
        try {
            payload.put("command", "change_view");
            payload.put("view", name);
            if (additionalPayload != null) {
                payload.put("values", additionalPayload);
            }
            UDPSender.broadcastJSON(payload);
        } catch (Exception ex) {
            Log.e("Error", ex.getMessage());
        }
    }

    public static void broadcastJSON(JSONObject json) throws IOException {
        broadcast(json, InetAddress.getByName("255.255.255.255"));
    }

    public static void broadcast(JSONObject json, InetAddress address) throws IOException {
        Log.i("INFO", json.toString());
        try {
            json.put("ip", getIPAddress(true));
        } catch (Exception ex) {
        }
        StrictMode.ThreadPolicy policy = new StrictMode.ThreadPolicy.Builder().permitAll().build();
        StrictMode.setThreadPolicy(policy);
        DatagramSocket socket = new DatagramSocket();

        socket.setReuseAddress(true);
        socket.setBroadcast(true);
        try {
            json.put("source", "controller");
        } catch (JSONException jex) {

        }

        byte[] buffer = (json.toString()).getBytes();
        DatagramPacket packet = new DatagramPacket(buffer, buffer.length, address, 5005);
        socket.send(packet);
        socket.close();
    }

    public static String getIPAddress(boolean useIPv4) {
        try {
            List<NetworkInterface> interfaces = Collections.list(NetworkInterface.getNetworkInterfaces());
            for (NetworkInterface intf : interfaces) {
                List<InetAddress> addrs = Collections.list(intf.getInetAddresses());
                for (InetAddress addr : addrs) {
                    if (!addr.isLoopbackAddress()) {
                        String sAddr = addr.getHostAddress();
                        //boolean isIPv4 = InetAddressUtils.isIPv4Address(sAddr);
                        boolean isIPv4 = sAddr.indexOf(':') < 0;

                        if (useIPv4) {
                            if (isIPv4)
                                return sAddr;
                        } else {
                            if (!isIPv4) {
                                int delim = sAddr.indexOf('%'); // drop ip6 zone suffix
                                return delim < 0 ? sAddr.toUpperCase() : sAddr.substring(0, delim).toUpperCase();
                            }
                        }
                    }
                }
            }
        } catch (Exception ignored) {
        } // for now eat exceptions
        return "";
    }

    public static void sendConfiguration(SharedPreferences sharedPreferences) {
        try {
            JSONObject valuesObject = new JSONObject();
            valuesObject.put("prepareTime", sharedPreferences.getInt("prepareTimeStore", 0));
            valuesObject.put("arrowTime", sharedPreferences.getInt("arrowTimeStore", 0));
            valuesObject.put("arrowCount", sharedPreferences.getInt("arrowCountStore", 0));
            valuesObject.put("reshootArrowCount", sharedPreferences.getInt("reshootArrowCountStore", 0));
            valuesObject.put("shootInTime", sharedPreferences.getInt("shootInTimeStore", 0));
            valuesObject.put("warnTime", sharedPreferences.getInt("warnTimeStore", 0));
            valuesObject.put("mode", sharedPreferences.getInt("modeStore", 1));
            valuesObject.put("passes", sharedPreferences.getInt("passesCountStore", 1));
            JSONObject payload = new JSONObject();
            payload.put("command", "config");
            payload.put("values", valuesObject);
            broadcastJSON(payload);
        } catch (Exception ex) {
            Log.e("Error", ex.getMessage());
        }
    }
}
