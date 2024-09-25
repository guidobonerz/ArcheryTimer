package de.drazil.archerytimer.udp;

import android.util.Log;

import org.json.JSONObject;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import de.drazil.archerytimer.IRemoteControl;

public class UDPServer implements Runnable {


    private IRemoteControl rc = null;
    private boolean serverRunning = false;
    private final int MAX_UDP_DATAGRAM_LEN = 1500;
    private Pattern pattern = Pattern.compile("(archery_timer_display)!(.*)");
    private static UDPServer server = null;
    private static Thread serverThread = null;
    private DatagramSocket udpSocket = null;


    private static UDPServer getInstance() {
        if (server == null) {
            server = new UDPServer();
        }
        return server;
    }


    public void run() {
        try {


            while (serverRunning) {
                byte[] message = new byte[8000];

                DatagramPacket packet = new DatagramPacket(message, message.length);
                Log.i("UDP client: ", "about to wait to receive");
                udpSocket.receive(packet);

                String text = new String(message, 0, packet.getLength());
                Log.i("response", text);
                if (rc != null) {
                    JSONObject jsonObject = new JSONObject(text);
                    if (jsonObject.get("source").equals("display")) {
                        rc.handleResponse(jsonObject);
                    }
                }
            }
        } catch (Exception e) {
            Log.e("UDP client has IOException", "error: ", e);
            serverRunning = false;
        }
    }

    public static void start() {
        getInstance()._start();
    }

    public void _start() {
        serverRunning = true;
        if (serverThread == null || (serverThread != null && !serverThread.isAlive())) {
            Log.i("server", "thread is dead, restarting...");
            try {
                if (udpSocket == null) {
                    udpSocket = new DatagramSocket(5005);
                    udpSocket.setReuseAddress(true);
                }
                serverThread = new Thread(server);
                serverThread.start();
            } catch (Exception ex) {

            }
        }
    }

    public static void stop() {
        getInstance()._stop();
    }

    public void _stop() {
        serverRunning = false;
        if (udpSocket != null) {
            udpSocket.close();
            udpSocket = null;
        }
    }

    public static void setRemoteControl(IRemoteControl remoteControl) {
        getInstance()._setRemoteControl(remoteControl);
    }

    public void _setRemoteControl(IRemoteControl remoteControl) {
        rc = remoteControl;
    }
}
