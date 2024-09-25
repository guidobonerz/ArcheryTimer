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
import android.util.AttributeSet;
import android.util.Log;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import org.json.JSONObject;

import de.drazil.archerytimer.udp.UDPSender;
import de.drazil.archerytimer.udp.UDPServer;


public class TournamentFragment extends Fragment implements IRemoteControl, IRemoteView {

    private boolean idle = true;
    private boolean hold = false;
    private boolean reshootAction = false;

    private String topic = null;

    private ImageButton startButton = null;
    private TextView passStatusView = null;
    private ImageView imageView = null;
    private ProgressControl progress = null;

    private RadioGroup tournamentPhaseGroup = null;

    private RadioGroup reshootActionGroup = null;


    private class ProgressControl extends Drawable {

        private int mode = 1;
        private int prepareTime = 0;
        private int actionTime = 0;
        private int warnTime = 0;
        private int remainingPrepareTime = 0;
        private int remainingActionTime = 0;
        private int currentGroup = 1;
        private int maxTime = 0;
        private int remainingOffset = 0;
        private int prepareColor = Color.rgb(255, 0, 0);
        private int actionColor = Color.rgb(0, 255, 0);
        private int warnColor = Color.rgb(241, 196, 15);
        private int separatorColor = Color.rgb(0, 0, 0);
        private int groupSeparatorColor = Color.rgb(255, 255, 255);
        private int overlayColor = Color.argb(150, 0, 0, 0);
        private int borderColor = Color.rgb(120, 120, 120);

        private boolean warnOnly = false;

        public ProgressControl() {
            reset();
        }

        public void setRemainingPrepareTime(int remainingPrepareTime) {
            this.remainingPrepareTime = remainingPrepareTime;
        }

        public void setRemainingActionTime(int remainingActionTime) {
            this.remainingActionTime = remainingActionTime;
        }

        public void setCurrentGroup(int currentGroup) {
            this.currentGroup = currentGroup;
        }


        public void reset() {
            SharedPreferences sharedPreferences = getActivity().getPreferences(Context.MODE_PRIVATE);
            mode = sharedPreferences.getInt(getString(R.string.modeStore), 1);
            prepareTime = sharedPreferences.getInt(getString(R.string.prepareTimeStore), 0);
            remainingPrepareTime = prepareTime;
            actionTime = sharedPreferences.getInt(getString(R.string.actionTimeStore), 0);
            remainingActionTime = actionTime;
            warnTime = sharedPreferences.getInt(getString(R.string.warnTimeStore), 0);

            if (warnTime > actionTime) {
                warnTime = actionTime;
                warnOnly = true;
            }
            maxTime = prepareTime + actionTime;
            remainingOffset = maxTime;
            invalidateSelf();
        }


        private int drawSection(Canvas canvas, Paint paint, int offset, int time, int maxTime, int maxWidth, int height, int color, boolean showSeparator) {
            int w = (int) Math.ceil((double) time / maxTime * maxWidth);
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(color);
            canvas.drawRect(offset, 0, offset + w, height, paint);
            if (showSeparator) {
                paint.setColor(separatorColor);
                paint.setStrokeWidth(6);
                canvas.drawLine(offset + w - 3, 0, offset + w - 3, height, paint);
            }
            return w;
        }

        @Override
        public void draw(Canvas canvas) {
            Paint paint = new Paint();

            int width = canvas.getWidth();
            int height = canvas.getHeight();

            int remainingTime = 0;
            int offset = 0;

            remainingTime = maxTime - (remainingPrepareTime + remainingActionTime);

            offset += drawSection(canvas, paint, offset, prepareTime, maxTime, width, height, prepareColor, true);
            if (warnOnly) {
                offset += drawSection(canvas, paint, offset, actionTime, maxTime, width, height, warnColor, false);
            } else {
                offset += drawSection(canvas, paint, offset, actionTime - warnTime, maxTime, width, height, actionColor, true);
                offset += drawSection(canvas, paint, offset, warnTime, maxTime, width, height, warnColor, false);
            }

            remainingOffset = (int) ((float) remainingTime / maxTime * width);

            //remainingTime overlay
            paint.setColor(overlayColor);
            canvas.drawRect(0, 0, remainingOffset, height, paint);

            //border color
            paint.setStyle(Paint.Style.STROKE);
            paint.setColor(borderColor);
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
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        UDPServer.setRemoteControl(this);
        return inflater.inflate(R.layout.fragment_tournament, container, false);
    }

    private void toggleTimer() {
        JSONObject payload = new JSONObject();
        try {
            JSONObject valuesObject = new JSONObject();
            valuesObject.put("time", 0);
            valuesObject.put("hold", hold);
            payload.put("command", "start_pause_action");
            payload.put("values", valuesObject);
            UDPSender.broadcastJSON(payload);
            hold = !hold;

        } catch (Exception ex) {
            Log.e("Error", ex.getMessage());
        }
    }


    public void handleResponse(JSONObject json) {
        String command = "";
        try {
            command = json.getString("command");
            if (command.equalsIgnoreCase("start_pause_action")) {
                startButton.setImageResource(hold ? R.drawable.pause : R.drawable.play);
            } else if (command.equalsIgnoreCase("stop")) {
                hold = false;
                idle = true;
                startButton.setImageResource(R.drawable.play);
            } else if (command.equalsIgnoreCase("reset")) {
                hold = false;
                idle = true;
                startButton.setImageResource(R.drawable.play);
                setGroupAndPassInfo("AB", 0, 0);
            } else if (command.equalsIgnoreCase("emergency")) {
                hold = false;
                idle = true;
                startButton.setImageResource(R.drawable.play);
            } else {
            }
        } catch (Exception ex) {

        }
    }

    public void remoteTimerResponse(String command) {

    }

    public void remoteTimerStatusResponse(String status[]) {
        SharedPreferences sharedPreferences = getActivity().getPreferences(Context.MODE_PRIVATE);
        setGroupAndPassInfo(status[1].equals("1") ? "AB" : "CD", Integer.valueOf(status[2]), sharedPreferences.getInt(getString(R.string.passesCountStore), 0));
        progress.setCurrentGroup(Integer.valueOf(status[3]));
        Log.i("groupCount", status[3]);
        progress.setRemainingPrepareTime(Integer.valueOf(status[4]));
        progress.setRemainingActionTime(Integer.valueOf(status[5]));
        progress.invalidateSelf();
    }

    private void setGroupAndPassInfo(String groupInfo, int currentPass, int maxPass) {
        passStatusView.setText(String.format("%s: %s - %s: %02d/%02d", getString(R.string.groupText), groupInfo, getString(R.string.passesText), currentPass, maxPass));
    }

    @Override
    public void onViewCreated(@NonNull View rootView, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(rootView, savedInstanceState);
        final VibratorManager vibrator = (VibratorManager) getActivity().getSystemService(Context.VIBRATOR_MANAGER_SERVICE);
        progress = new ProgressControl();
        imageView = (ImageView) rootView.findViewById(R.id.timerProgress);
        imageView.setImageDrawable(progress);
        reshootActionGroup = (RadioGroup) rootView.findViewById(R.id.reShootActionGroup);
        reshootActionGroup.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(RadioGroup radioGroup, int id) {
                SharedPreferences sharedPreferences = getActivity().getPreferences(Context.MODE_PRIVATE);
                Log.i("button", String.valueOf(id));
                SharedPreferences.Editor editor = sharedPreferences.edit();
                editor.putInt(getString(R.string.reshootArrowCountStore), id);
                editor.apply();
                UDPSender.sendConfiguration(sharedPreferences);
            }
        });

        tournamentPhaseGroup = (RadioGroup) rootView.findViewById(R.id.tournamentPhase);
        tournamentPhaseGroup.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(RadioGroup radioGroup, int id) {
                SharedPreferences sharedPreferences = getActivity().getPreferences(Context.MODE_PRIVATE);
                reshootAction = false;
                reshootActionGroup.setVisibility(View.INVISIBLE);
                String subViewName = getSubViewName();
                UDPSender.controlDisplay(subViewName, null);
                if (subViewName.equalsIgnoreCase("re_shoot")) {
                    RadioButton radioButton = (RadioButton) radioGroup.findViewById(id);
                    reshootAction = radioButton.isChecked();
                    reshootActionGroup.setVisibility(View.VISIBLE);
                    int arrowCount = sharedPreferences.getInt(getString(R.string.arrowCountStore), 0);
                    for (int i = 1; i < arrowCount + 1; i++) {
                        RadioButton rb = (RadioButton) rootView.findViewById(i);
                        rb.setEnabled(reshootAction);
                        rb.setChecked(i == 1);
                    }
                }
                try {
                    UDPSender.sendConfiguration(sharedPreferences);
                } catch (Exception ex) {
                    Log.e("Error", ex.getMessage());
                }
            }
        });

        SharedPreferences sharedPreferences = getActivity().getPreferences(Context.MODE_PRIVATE);
        int arrowCount = sharedPreferences.getInt(getString(R.string.arrowCountStore), 0);
        RadioButton radioButton = null;
        for (int i = 1; i < arrowCount + 1; i++) {
            radioButton = new RadioButton(rootView.getContext());
            radioButton.setId(i);
            radioButton.setGravity(Gravity.CENTER);
            radioButton.setPadding(20, 20, 25, 20);
            radioButton.setBackground(null);
            if (i == 1) {
                radioButton.setButtonDrawable(R.drawable.custom_btn1_radio);
            } else if (i == 2) {
                radioButton.setButtonDrawable(R.drawable.custom_btn2_radio);
            } else if (i == 3) {
                radioButton.setButtonDrawable(R.drawable.custom_btn3_radio);
            } else if (i == 4) {
                radioButton.setButtonDrawable(R.drawable.custom_btn4_radio);
            } else if (i == 5) {
                radioButton.setButtonDrawable(R.drawable.custom_btn5_radio);
            } else if (i == 6) {
                radioButton.setButtonDrawable(R.drawable.custom_btn6_radio);
            } else {
            }
            radioButton.setChecked(i == 1);
            radioButton.setEnabled(false);
            reshootActionGroup.addView(radioButton);
        }
        passStatusView = (TextView) rootView.findViewById(R.id.passStatus);
        startButton = (ImageButton) rootView.findViewById(R.id.start);
        startButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (!idle) {
/*
                    int selectedId = radioGroup.getCheckedRadioButtonId();
                    RadioButton radioButton = (RadioButton) radioGroup.findViewById(selectedId);
                    String mode = radioButton.getText().toString();
*/
                    //vibrator.vibrate(VibrationEffect.createOneShot(150,200));
                    if (reshootAction) {
                        AlertDialog.Builder builder1 = new AlertDialog.Builder(v.getContext());
                        builder1.setMessage(getString(R.string.confirmReShootAction));
                        builder1.setCancelable(true);
                        builder1.setPositiveButton(getString(R.string.yes), new DialogInterface.OnClickListener() {
                            public void onClick(DialogInterface dialog, int id) {
                                dialog.cancel();
                                toggleTimer();
                            }
                        });
                        builder1.setNegativeButton(getString(R.string.no), new DialogInterface.OnClickListener() {
                            public void onClick(DialogInterface dialog, int id) {
                                dialog.cancel();
                            }
                        });
                        AlertDialog alert11 = builder1.create();
                        alert11.show();
                    } else {
                        toggleTimer();
                    }
                    idle = false;
                } else {
                    //vibrator.vibrate(VibrationEffect.createOneShot(150,200));
                    toggleTimer();
                }
            }
        });
        ImageButton stopButton = (ImageButton) rootView.findViewById(R.id.stop);
        stopButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                //vibrator.vibrate(VibrationEffect.createOneShot(250,200));
                JSONObject payload = new JSONObject();
                try {
                    payload.put("command", "stop");
                    UDPSender.broadcastJSON(payload);
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
                    payload.put("command", "emergency");
                    UDPSender.broadcastJSON(payload);
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
                    payload.put("command", "reset");
                    UDPSender.broadcastJSON(payload);
                } catch (Exception ex) {
                    Log.e("Error", ex.getMessage());
                }
            }
        });
        setGroupAndPassInfo("AB", 0, sharedPreferences.getInt(getString(R.string.passesCountStore), 0));
    }

    @Override
    public String getCurrentView() {
        return getSubViewName();
    }

    private String getSubViewName() {
        String viewName = "";
        if (tournamentPhaseGroup == null) {
            viewName = "tournament";
        } else {
            int id = tournamentPhaseGroup.getCheckedRadioButtonId();
            if (id == R.id.modeShootIn) {
                viewName = "shoot_in";
            } else if (id == R.id.modeTournament) {
                viewName = "tournament";
            } else if (id == R.id.modeReShoot) {
                viewName = "re_shoot";
            }
        }
        return viewName;
    }
}