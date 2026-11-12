package de.drazil.archerytimer.net

import android.os.Handler
import android.os.Looper
import android.util.Log
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.concurrent.CopyOnWriteArrayList

/**
 * In memory protocol of everything that happens on the display network. It is
 * deliberately not persisted - it only has to answer the question "did that
 * display come back?" while the app is running.
 */
object ConnectionLog {

    const val TAG = "ArcheryNet"

    private const val CAPACITY = 500

    data class Entry(val timestamp: Long, val message: String)

    fun interface Listener {
        fun onLogChanged()
    }

    private val entries = ArrayDeque<Entry>()
    private val listeners = CopyOnWriteArrayList<Listener>()
    private val mainHandler = Handler(Looper.getMainLooper())
    private val timeFormat = SimpleDateFormat("HH:mm:ss", Locale.GERMANY)

    fun log(message: String) {
        Log.i(TAG, message)
        synchronized(entries) {
            entries.addLast(Entry(System.currentTimeMillis(), message))
            while (entries.size > CAPACITY) {
                entries.removeFirst()
            }
        }
        notifyListeners()
    }

    fun snapshot(): List<Entry> = synchronized(entries) { entries.toList() }

    fun formatted(): String = snapshot().joinToString("\n") {
        "${timeFormat.format(Date(it.timestamp))}  ${it.message}"
    }

    fun clear() {
        synchronized(entries) { entries.clear() }
        notifyListeners()
    }

    fun addListener(listener: Listener) {
        listeners.addIfAbsent(listener)
    }

    fun removeListener(listener: Listener) {
        listeners.remove(listener)
    }

    private fun notifyListeners() {
        mainHandler.post {
            for (listener in listeners) {
                listener.onLogChanged()
            }
        }
    }
}
