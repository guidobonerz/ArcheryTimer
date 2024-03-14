package de.drazil.archerytimer;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.VibratorManager;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.RadioButton;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import org.json.JSONObject;

import de.drazil.archerytimer.udp.Sender;
import de.drazil.archerytimer.udp.UDPServer;


public class TournamentFragment extends Fragment implements IRemoteControl {

    private boolean start = true;
    private boolean extendedAction = false;
    private ImageButton startButton = null;
    private TextView passStatusView = null;
    private ImageView imageView = null;
    private MyDrawable progress = null;


    private class MyDrawable extends Drawable {

        private int prepareTime = 0;
        private int actionTime = 0;
        private int warnTime = 30;
        private int maxPrepareTime = 0;
        private int maxActionTime = 0;

        public void setPrepareTime(int prepareTime) {
            this.prepareTime = prepareTime;
        }

        public void setActionTime(int actionTime) {
            this.actionTime = actionTime;

        }


        @Override
        public void draw(Canvas canvas) {
            Paint paint = new Paint();
            SharedPreferences sharedPreferences = getActivity().getPreferences(Context.MODE_PRIVATE);
            maxPrepareTime = sharedPreferences.getInt(getString(R.string.prepareTimeStore), 0);
            maxActionTime = sharedPreferences.getInt(getString(R.string.actionTimeStore), 0);
            warnTime = sharedPreferences.getInt(getString(R.string.warnTimeStore), 0);


            int maxTime = maxPrepareTime + maxActionTime;
            int width = canvas.getWidth();
            int height = canvas.getHeight();

            if (maxActionTime < warnTime) {
                warnTime = maxActionTime;
            }
            int warnWidth = (int) ((float) warnTime / maxTime * width);
            int offset = (int) ((float) (maxTime - prepareTime - actionTime) / maxTime * width);
            int maxPrepareWidth = (int) ((float) maxPrepareTime / maxTime * width);

            // prepare section
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(Color.rgb(255, 0, 0));
            canvas.drawRect(0, 0, maxPrepareWidth, height, paint);

            if (maxActionTime > warnTime) {
                // action section
                paint.setColor(Color.rgb(0, 255, 0));
                canvas.drawRect(maxPrepareWidth, 0, width - warnWidth, height, paint);
            }

            // warn section
            paint.setColor(Color.rgb(241, 196, 15));
            canvas.drawRect(width - warnWidth, 0, width, height, paint);

            //remain overlay
            paint.setColor(Color.argb(150, 0, 0, 0));
            canvas.drawRect(0, 0, offset, height, paint);

            //borders
            paint.setColor(Color.rgb(0, 0, 0));
            paint.setStrokeWidth(15);
            canvas.drawLine(maxPrepareWidth, 0, maxPrepareWidth, height, paint);

            if (maxActionTime > warnTime) {
                paint.setStrokeWidth(15);
                canvas.drawLine(width - warnWidth, 0, width - warnWidth, height, paint);
            }
            paint.setStyle(Paint.Style.STROKE);
            paint.setColor(Color.rgb(120, 120, 120));
            paint.setStrokeWidth(25);
            canvas.drawLine(0, 0, width, 0, paint);
            canvas.drawLine(0, height, width, height, paint);
        }

        @Override
        public int getOpacity() {
            return PixelFormat.UNKNOWN;
        }

        @Override
        public void setAlpha(int alpha) {
        }

        @Override
        public void setColorFilter(ColorFilter cf) {
        }
    }

    public TournamentFragment() {

    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.i("UDPServer", "start");
        UDPServer.start();

    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.i("UDPServer", "stop");
        UDPServer.stop();
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        JSONObject payload = new JSONObject();
        try {
            payload.put("name", "tournament");
            Sender.broadcastJSON(payload);
        } catch (Exception ex) {
            Log.e("Error", ex.getMessage());
        }
        UDPServer.setRemoteControl(this);
        return inflater.inflate(R.layout.fragment_tournament, container, false);
    }

    private void toggleTimer(boolean flag) {

        JSONObject payload = new JSONObject();
        try {

            payload.put("name", "toggle_action");
            Sender.broadcastJSON(payload);
            start = !start;
        } catch (Exception ex) {
            Log.e("Error", ex.getMessage());
        }
    }

    public void remoteTimerResponse(String command) {
        if (command.equalsIgnoreCase("toggle_action")) {
            Log.i("start", String.valueOf(start));
            startButton.setImageResource(start ? R.drawable.play : R.drawable.pause);
        } else if (command.equalsIgnoreCase("stop")) {
            start = true;
            startButton.setImageResource(R.drawable.play);
        } else if (command.equalsIgnoreCase("reset")) {
            start = true;
            startButton.setImageResource(R.drawable.play);
        } else if (command.equalsIgnoreCase("emergency")) {
            start = true;
            startButton.setImageResource(R.drawable.play);
        } else {
        }
    }

    public void remoteTimerStatusResponse(String status[]) {
        SharedPreferences sharedPreferences = getActivity().getPreferences(Context.MODE_PRIVATE);
        setGroupAndPassInfo(status[1].equals("1") ? "AB" : "CD", String.format("%02d/%02d", Integer.valueOf(status[2]), sharedPreferences.getInt(getString(R.string.passesCountStore), 0)));

        progress.setPrepareTime(Integer.valueOf(status[3]));
        progress.setActionTime(Integer.valueOf(status[4]));
        progress.invalidateSelf();
    }

    private void setGroupAndPassInfo(String groupInfo, String passInfo) {
        passStatusView.setText(String.format("Gruppe: %s - Passen: %s", groupInfo, passInfo));
    }

    @Override
    public void onViewCreated(@NonNull View rootView, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(rootView, savedInstanceState);

        final VibratorManager vibrator = (VibratorManager) getActivity().getSystemService(Context.VIBRATOR_MANAGER_SERVICE);
        CheckBox extendedActionButton = (CheckBox) rootView.findViewById(R.id.extendedAction);
        extendedActionButton.setOnClickListener(v -> {
            extendedAction = extendedActionButton.isChecked();
            RadioButton arrow1 = (RadioButton) rootView.findViewById(R.id.arrowButton1);
            RadioButton arrow2 = (RadioButton) rootView.findViewById(R.id.arrowButton2);
            RadioButton arrow3 = (RadioButton) rootView.findViewById(R.id.arrowButton3);
            arrow1.setEnabled(extendedAction);
            arrow1.setChecked(true);
            arrow2.setEnabled(extendedAction);
            arrow3.setEnabled(extendedAction);
        });
        progress = new MyDrawable();
        imageView = (ImageView) rootView.findViewById(R.id.timerProgress);
        imageView.setImageDrawable(progress);

        passStatusView = (TextView) rootView.findViewById(R.id.passStatus);

        startButton = (ImageButton) rootView.findViewById(R.id.start);
        startButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {

                if (start) {
                    //vibrator.vibrate(VibrationEffect.createOneShot(150,200));
                    if (extendedAction) {

                        AlertDialog.Builder builder1 = new AlertDialog.Builder(v.getContext());

                        builder1.setMessage("Wirklich nachschiessen?");
                        builder1.setCancelable(true);

                        builder1.setPositiveButton(
                                "Ja",
                                new DialogInterface.OnClickListener() {
                                    public void onClick(DialogInterface dialog, int id) {
                                        dialog.cancel();
                                        toggleTimer(start);
                                    }
                                });

                        builder1.setNegativeButton(
                                "Nein",
                                new DialogInterface.OnClickListener() {
                                    public void onClick(DialogInterface dialog, int id) {
                                        dialog.cancel();
                                    }
                                });
                        AlertDialog alert11 = builder1.create();
                        alert11.show();
                    }
                }
                //vibrator.vibrate(VibrationEffect.createOneShot(150,200));
                toggleTimer(start);
            }
        });
        ImageButton stopButton = (ImageButton) rootView.findViewById(R.id.stop);
        stopButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                //vibrator.vibrate(VibrationEffect.createOneShot(250,200));
                JSONObject payload = new JSONObject();
                try {
                    payload.put("name", "stop");
                    Sender.broadcastJSON(payload);
                } catch (Exception ex) {
                    Log.e("Error", ex.getMessage());
                }
            }
        });


        ImageButton emergencyButton = (ImageButton) rootView.findViewById(R.id.emergency);
        emergencyButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                //vibrator.vibrate(VibrationEffect.createOneShot(450,255));
                JSONObject payload = new JSONObject();
                try {

                    payload.put("name", "emergency");
                    Sender.broadcastJSON(payload);
                } catch (Exception ex) {
                    Log.e("Error", ex.getMessage());
                }
            }
        });

        ImageButton resetButton = (ImageButton) rootView.findViewById(R.id.reset);
        resetButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                //vibrator.vibrate(VibrationEffect.createWaveform(new long[]{0,40,80,40,80,40,0},-1));
                JSONObject payload = new JSONObject();
                try {

                    payload.put("name", "reset");
                    Sender.broadcastJSON(payload);
                } catch (Exception ex) {
                    Log.e("Error", ex.getMessage());
                }
            }
        });


    }
}