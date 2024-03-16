package de.drazil.archerytimer.udp;

import android.os.StrictMode;
import android.util.Log;

import org.json.JSONObject;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.util.Collections;
import java.util.List;

public class Sender {


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
        byte[] buffer = ("archery_timer_control!" + json.toString()).getBytes();
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
}
