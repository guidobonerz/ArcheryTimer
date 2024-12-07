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


    public static void controlDisplay(String name, JSONObject additionalPayload, long startTime) {
        JSONObject payload = new JSONObject();
        try {
            payload.put("cmd", "change_view");
            payload.put("v", name);
            if (additionalPayload != null) {
                payload.put("val", additionalPayload);
            }
            UDPSender.broadcastJSON(payload, startTime);
        } catch (Exception ex) {
            Log.e("Error", ex.getMessage());
        }
    }


    public static void broadcastJSON(JSONObject json, long startTime) throws IOException {
        broadcast(json, InetAddress.getByName("255.255.255.255"), startTime);
    }

    public static void broadcast(JSONObject json, InetAddress address, long startTime) throws IOException {

        Log.i("INFO", json.toString());

        StrictMode.ThreadPolicy policy = new StrictMode.ThreadPolicy.Builder().permitAll().build();
        StrictMode.setThreadPolicy(policy);
        DatagramSocket socket = new DatagramSocket();

        socket.setReuseAddress(true);
        socket.setBroadcast(true);
        for (int i = 0; i < 2; i++) {
            try {
                json.put("src", "controller");
                json.put("sd", System.currentTimeMillis() - startTime);
            } catch (JSONException jex) {

            }
            byte[] buffer = (json.toString()).getBytes();
            DatagramPacket packet = new DatagramPacket(buffer, buffer.length, address, 5005);
            socket.send(packet);
            socket.close();
        }
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

    public static void sendConfiguration(SharedPreferences sharedPreferences, long startTime) {
        try {
            JSONObject valuesObject = new JSONObject();
            valuesObject.put("pt", sharedPreferences.getInt("prepareTimeStore", 0));
            valuesObject.put("at", sharedPreferences.getInt("arrowTimeStore", 0));
            valuesObject.put("ac", sharedPreferences.getInt("arrowCountStore", 0));
            valuesObject.put("rac", sharedPreferences.getInt("reshootArrowCountStore", 0));
            valuesObject.put("sit", sharedPreferences.getInt("shootInTimeStore", 0));
            valuesObject.put("wt", sharedPreferences.getInt("warnTimeStore", 0));
            valuesObject.put("g", sharedPreferences.getInt("groupStore", 1));
            valuesObject.put("p", sharedPreferences.getInt("passesCountStore", 1));
            valuesObject.put("fpl",sharedPreferences.getBoolean("flashingPrepareLightStore",true));
            JSONObject payload = new JSONObject();
            payload.put("cmd", "config");
            payload.put("val", valuesObject);
            broadcastJSON(payload, startTime);
        } catch (Exception ex) {
            Log.e("Error", ex.getMessage());
        }
    }
}
