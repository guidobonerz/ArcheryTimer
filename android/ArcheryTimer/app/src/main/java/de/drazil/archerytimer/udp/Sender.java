package de.drazil.archerytimer.udp;

import android.os.StrictMode;
import android.util.Log;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;

public class Sender {

    public static void broadcast(String broadcastMessage) throws IOException {
        broadcast(broadcastMessage,InetAddress.getByName("255.255.255.255"));
    }
    public static void broadcast(String broadcastMessage, InetAddress address) throws IOException {
        Log.i("INFO", broadcastMessage);
        StrictMode.ThreadPolicy policy = new StrictMode.ThreadPolicy.Builder().permitAll().build();
        StrictMode.setThreadPolicy(policy);
        DatagramSocket socket = new DatagramSocket();

        socket.setReuseAddress(true);
        socket.setBroadcast(true);
        byte[] buffer = broadcastMessage.getBytes();
        DatagramPacket packet = new DatagramPacket(buffer, buffer.length, address, 5005);
        socket.send(packet);
        socket.close();
    }
}
