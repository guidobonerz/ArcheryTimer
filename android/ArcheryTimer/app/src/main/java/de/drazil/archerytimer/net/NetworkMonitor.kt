package de.drazil.archerytimer.net

import android.content.Context
import android.net.ConnectivityManager
import android.net.LinkProperties
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.net.wifi.WifiManager
import java.net.DatagramSocket
import java.net.Inet4Address
import java.net.InetAddress
import java.util.concurrent.CopyOnWriteArrayList

/**
 * Watches the wifi connection of the phone.
 *
 * Two things are needed for udp broadcasts to keep working:
 *   - a multicast lock, otherwise android drops broadcast packets as soon as
 *     the device dozes off,
 *   - the current [Network], because sockets stay bound to the network they
 *     were created on. After the wifi comes back the sockets have to be built
 *     again on the new network, otherwise nothing is received any more.
 */
object NetworkMonitor {

    fun interface Listener {
        fun onNetworkChanged(network: Network?)
    }

    @Volatile
    var network: Network? = null
        private set

    @Volatile
    private var broadcastAddress: InetAddress? = null

    private val listeners = CopyOnWriteArrayList<Listener>()

    private var connectivityManager: ConnectivityManager? = null
    private var multicastLock: WifiManager.MulticastLock? = null
    private var wifiLock: WifiManager.WifiLock? = null
    private var callback: ConnectivityManager.NetworkCallback? = null

    fun addListener(listener: Listener) {
        listeners.addIfAbsent(listener)
    }

    fun removeListener(listener: Listener) {
        listeners.remove(listener)
    }

    fun start(context: Context) {
        val appContext = context.applicationContext
        if (callback != null) {
            return
        }

        val wifiManager = appContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
        multicastLock = wifiManager.createMulticastLock("archerytimer").apply {
            setReferenceCounted(false)
            acquire()
        }
        wifiLock = wifiManager.createWifiLock(
            WifiManager.WIFI_MODE_FULL_HIGH_PERF,
            "archerytimer"
        ).apply {
            setReferenceCounted(false)
            acquire()
        }

        val manager = appContext.getSystemService(ConnectivityManager::class.java)
        connectivityManager = manager

        val networkCallback = object : ConnectivityManager.NetworkCallback() {
            override fun onAvailable(available: Network) {
                network = available
                broadcastAddress = manager?.getLinkProperties(available)?.let { subnetBroadcast(it) }
                ConnectionLog.log("WLAN verfügbar")
                publish()
            }

            override fun onLinkPropertiesChanged(changed: Network, linkProperties: LinkProperties) {
                if (changed == network) {
                    broadcastAddress = subnetBroadcast(linkProperties)
                    publish()
                }
            }

            override fun onLost(lost: Network) {
                if (lost == network) {
                    network = null
                    broadcastAddress = null
                    ConnectionLog.log("WLAN verloren")
                    publish()
                }
            }
        }
        callback = networkCallback

        val request = NetworkRequest.Builder()
            .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
            .addCapability(NetworkCapabilities.NET_CAPABILITY_NOT_VPN)
            .build()
        manager?.registerNetworkCallback(request, networkCallback)
    }

    fun stop() {
        callback?.let { runCatching { connectivityManager?.unregisterNetworkCallback(it) } }
        callback = null
        connectivityManager = null
        multicastLock?.let { if (it.isHeld) it.release() }
        multicastLock = null
        wifiLock?.let { if (it.isHeld) it.release() }
        wifiLock = null
    }

    /** binds a freshly created socket to the network the wifi is currently on */
    fun bindSocket(socket: DatagramSocket) {
        network?.let { runCatching { it.bindSocket(socket) } }
    }

    /**
     * The directed broadcast address of the current subnet. Some access points
     * drop 255.255.255.255 but forward 192.168.x.255, so both are used.
     */
    fun subnetBroadcastAddress(): InetAddress? = broadcastAddress

    private fun subnetBroadcast(linkProperties: LinkProperties): InetAddress? {
        for (linkAddress in linkProperties.linkAddresses) {
            val address = linkAddress.address
            if (address !is Inet4Address) {
                continue
            }
            val raw = address.address
            var value = 0
            for (byte in raw) {
                value = (value shl 8) or (byte.toInt() and 0xff)
            }
            val hostBits = 32 - linkAddress.prefixLength
            if (hostBits <= 0 || hostBits >= 32) {
                continue
            }
            val broadcast = value or ((1 shl hostBits) - 1)
            val bytes = byteArrayOf(
                (broadcast ushr 24).toByte(),
                (broadcast ushr 16).toByte(),
                (broadcast ushr 8).toByte(),
                broadcast.toByte()
            )
            return runCatching { InetAddress.getByAddress(bytes) }.getOrNull()
        }
        return null
    }

    private fun publish() {
        val current = network
        for (listener in listeners) {
            listener.onNetworkChanged(current)
        }
    }
}
