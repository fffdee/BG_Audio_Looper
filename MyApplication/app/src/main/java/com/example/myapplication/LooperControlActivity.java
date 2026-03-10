package com.example.myapplication;

import android.content.res.ColorStateList;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.RadioGroup;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Looper Control Activity
 * Maintains state machine on App side, sends -r/-p/-t commands based on segment state.
 *
 * 顶部信息栏显示：BPM / 节拍 / 节拍器模式 / 硬件连接状态
 * 导航栏右侧"设置"按钮开启侧边栏，可修改全局参数并发送到硬件。
 *
 * State flow: INACTIVE -> (r) -> RECORDING -> (p) -> PLAYING -> (t) -> STOPPED -> (p) -> PLAYING...
 */
public class LooperControlActivity extends AppCompatActivity {

    private static final String BLE_UUID = "0000ab01-0000-1000-8000-00805f9b34fb";
    private static final int SEG_COUNT = 4;

    // -------- 节拍器模式常量 --------
    private static final int METRO_OFF       = 0;   // 全程关闭
    private static final int METRO_ALWAYS    = 1;   // 全程开启
    private static final int METRO_COUNTDOWN = 2;   // 倒数N拍后开始录制

    // Segment state enum
    private enum SegState { INACTIVE, RECORDING, PLAYING, STOPPED }

    private BluetoothHelper bluetoothHelper;
    private final Handler handler = new Handler(Looper.getMainLooper());

    // App-side state tracking
    private final SegState[] segStates = {
        SegState.INACTIVE, SegState.INACTIVE, SegState.INACTIVE, SegState.INACTIVE
    };

    // -------- 全局设置状态 --------
    private int currentBpm        = 120;
    private int currentBeats      = 4;   // 每小节拍数
    private int metronomeMode     = METRO_COUNTDOWN;
    private int countdownBeats    = 4;   // 倒数拍数

    // Color constants
    private static final int COLOR_INACTIVE  = Color.parseColor("#00D9FF");
    private static final int COLOR_RECORDING = Color.parseColor("#FF4444");
    private static final int COLOR_PLAYING   = Color.parseColor("#00FFA3");
    private static final int COLOR_STOPPED   = Color.parseColor("#888888");

    private static final int TINT_INACTIVE  = Color.parseColor("#1A3040");
    private static final int TINT_RECORDING = Color.parseColor("#3D1010");
    private static final int TINT_PLAYING   = Color.parseColor("#0D3020");
    private static final int TINT_STOPPED   = Color.parseColor("#222222");

    // -------- 每段独立配置 --------
    /** 录制小节数，0 = 手动停止（无限） */
    private final int[]     segMeasures = {0, 0, 0, 0};
    /** 录制结束后是否自动播放 */
    private final boolean[] segAutoPlay = {true, true, true, true};

    // -------- UI：循环段卡片 --------
    // segCards 指向外层 FrameLayout（负责背景着色）
    private View[]         segCards     = new View[SEG_COUNT];
    // segMainAreas 指向内层 LinearLayout（负责点击交互）
    private LinearLayout[] segMainAreas = new LinearLayout[SEG_COUNT];
    private TextView[]     tvSegName    = new TextView[SEG_COUNT];
    private TextView[]     tvSegState   = new TextView[SEG_COUNT];
    private TextView[]     tvSegHint    = new TextView[SEG_COUNT];
    /** 卡片底部的配置提示行 */
    private TextView[]     tvSegCfgHint = new TextView[SEG_COUNT];
    /** 右上角配置（齿轮）按钮 */
    private ImageButton[]  btnSegConfig = new ImageButton[SEG_COUNT];

    // -------- 自动停止定时器 --------
    /** 每个段的自动停止录制 Runnable，可用于取消 */
    private final Runnable[] autoStopRunnables = new Runnable[SEG_COUNT];

    /** 长按删除计时器（每个段独立） */
    private final Runnable[] longPressRunnables = new Runnable[SEG_COUNT];

    /** 录制前节拍器倒计时定时器（等待N拍后再开始录制） */
    private final Runnable[] countdownRunnables = new Runnable[SEG_COUNT];
    /** 该段是否正在节拍器倒计时中（尚未开始录制） */
    private final boolean[]  segInCountdown     = new boolean[SEG_COUNT];

    // -------- UI：Flash 管理按钮 --------
    private Button btnLooperClear;
    private Button btnLooperErase;
    private Button btnLooperReset;

    // -------- UI：顶部信息栏 --------
    private TextView tvTopBpm;
    private TextView tvTopBeats;
    private TextView tvTopMetroMode;
    private TextView tvTopHwStatus;

    // -------- UI：侧边栏相关 --------
    private View      drawerLooperSettings;
    private View      looperDrawerOverlay;
    private ImageButton btnCloseLooperDrawer;

    // 侧边栏：节拍器模式 RadioGroup
    private RadioGroup rgMetronomeMode;

    // 侧边栏：倒数拍数
    private LinearLayout layoutCountdownBeats;
    private TextView  tvCountdownValue;
    private Button    btnCountdownDec;
    private Button    btnCountdownInc;

    // 侧边栏：节拍（每小节拍数）
    private TextView  tvBeatsValue;
    private Button    btnBeatsDec;
    private Button    btnBeatsInc;

    // 侧边栏：BPM
    private TextView  tvBpmValue;
    private Button    btnBpmDec;
    private Button    btnBpmInc;
    private Button    btnBpmDecBig;
    private Button    btnBpmIncBig;

    // 侧边栏：应用按钮
    private Button btnApplyLooperSettings;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_looper_control);

        bluetoothHelper = BluetoothManager.getInstance().getBluetoothHelper();

        Toolbar toolbar = findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);
        if (getSupportActionBar() != null) {
            getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        }

        initViews();
        initSettingsDrawer();
        setupListeners();
        setupDrawerListeners();
        setupBleListener();
        refreshTopInfoBar();
    }

    @Override
    protected void onResume() {
        super.onResume();
        setupBleListener();
        // 刷新硬件连接状态
        updateHwStatusIndicator();
    }

    @Override
    protected void onPause() {
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        // 页面销毁时清除所有定时器
        for (int i = 0; i < SEG_COUNT; i++) {
            cancelAutoStop(i);
            cancelCountdown(i);
            if (longPressRunnables[i] != null) {
                handler.removeCallbacks(longPressRunnables[i]);
                longPressRunnables[i] = null;
            }
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.menu_looper, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();
        if (id == android.R.id.home) {
            finish();
            return true;
        } else if (id == R.id.menu_looper_settings) {
            // 打开全局设置侧边栏
            openDrawer();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void initViews() {
        // 外层 FrameLayout（背景着色用）
        int[] cardIds = {
            R.id.card_seg0, R.id.card_seg1, R.id.card_seg2, R.id.card_seg3
        };
        // 内层 LinearLayout（点击交互用）
        int[] mainAreaIds = {
            R.id.card_seg0_main, R.id.card_seg1_main,
            R.id.card_seg2_main, R.id.card_seg3_main
        };
        int[][] subIds = {
            {R.id.tv_seg0_name, R.id.tv_seg0_state, R.id.tv_seg0_hint, R.id.tv_seg0_cfg_hint},
            {R.id.tv_seg1_name, R.id.tv_seg1_state, R.id.tv_seg1_hint, R.id.tv_seg1_cfg_hint},
            {R.id.tv_seg2_name, R.id.tv_seg2_state, R.id.tv_seg2_hint, R.id.tv_seg2_cfg_hint},
            {R.id.tv_seg3_name, R.id.tv_seg3_state, R.id.tv_seg3_hint, R.id.tv_seg3_cfg_hint},
        };
        int[] configBtnIds = {
            R.id.btn_seg0_config, R.id.btn_seg1_config,
            R.id.btn_seg2_config, R.id.btn_seg3_config
        };
        for (int i = 0; i < SEG_COUNT; i++) {
            segCards[i]     = findViewById(cardIds[i]);
            segMainAreas[i] = findViewById(mainAreaIds[i]);
            tvSegName[i]    = findViewById(subIds[i][0]);
            tvSegState[i]   = findViewById(subIds[i][1]);
            tvSegHint[i]    = findViewById(subIds[i][2]);
            tvSegCfgHint[i] = findViewById(subIds[i][3]);
            btnSegConfig[i] = findViewById(configBtnIds[i]);
        }
        btnLooperClear = findViewById(R.id.btn_looper_clear);
        btnLooperErase = findViewById(R.id.btn_looper_erase);
        btnLooperReset = findViewById(R.id.btn_looper_reset);

        // 顶部信息栏
        tvTopBpm       = findViewById(R.id.tv_top_bpm);
        tvTopBeats     = findViewById(R.id.tv_top_beats);
        tvTopMetroMode = findViewById(R.id.tv_top_metro_mode);
        tvTopHwStatus  = findViewById(R.id.tv_top_hw_status);

        for (int i = 0; i < SEG_COUNT; i++) {
            refreshSegUI(i);
            refreshSegCfgHint(i);
        }
    }

    /** 初始化侧边栏控件引用 */
    private void initSettingsDrawer() {
        drawerLooperSettings = findViewById(R.id.drawer_looper_settings);
        looperDrawerOverlay  = findViewById(R.id.looper_drawer_overlay);
        btnCloseLooperDrawer = findViewById(R.id.btn_close_looper_drawer);

        // 节拍器模式
        rgMetronomeMode     = findViewById(R.id.rg_metronome_mode);
        layoutCountdownBeats = findViewById(R.id.layout_countdown_beats);
        tvCountdownValue    = findViewById(R.id.tv_countdown_value);
        btnCountdownDec     = findViewById(R.id.btn_countdown_dec);
        btnCountdownInc     = findViewById(R.id.btn_countdown_inc);

        // 每小节拍数
        tvBeatsValue = findViewById(R.id.tv_beats_value);
        btnBeatsDec  = findViewById(R.id.btn_beats_dec);
        btnBeatsInc  = findViewById(R.id.btn_beats_inc);

        // BPM
        tvBpmValue   = findViewById(R.id.tv_bpm_value);
        btnBpmDec    = findViewById(R.id.btn_bpm_dec);
        btnBpmInc    = findViewById(R.id.btn_bpm_inc);
        btnBpmDecBig = findViewById(R.id.btn_bpm_dec_big);
        btnBpmIncBig = findViewById(R.id.btn_bpm_inc_big);

        // 应用按钮
        btnApplyLooperSettings = findViewById(R.id.btn_apply_looper_settings);

        // 根据当前状态初始化 UI 显示
        syncDrawerUiFromState();
    }

    /** 将当前状态同步到侧边栏 UI */
    private void syncDrawerUiFromState() {
        // RadioGroup
        switch (metronomeMode) {
            case METRO_OFF:       rgMetronomeMode.check(R.id.rb_metro_off);       break;
            case METRO_ALWAYS:    rgMetronomeMode.check(R.id.rb_metro_always);    break;
            case METRO_COUNTDOWN: rgMetronomeMode.check(R.id.rb_metro_countdown); break;
        }
        layoutCountdownBeats.setVisibility(
            metronomeMode == METRO_COUNTDOWN ? View.VISIBLE : View.GONE);

        tvCountdownValue.setText(String.valueOf(countdownBeats));
        tvBeatsValue.setText(String.valueOf(currentBeats));
        tvBpmValue.setText(String.valueOf(currentBpm));
    }

    /** 绑定侧边栏各控件的监听器 */
    private void setupDrawerListeners() {
        // 关闭按钮 & 遮罩
        btnCloseLooperDrawer.setOnClickListener(v -> closeDrawer());
        looperDrawerOverlay.setOnClickListener(v -> closeDrawer());

        // 节拍器模式切换
        rgMetronomeMode.setOnCheckedChangeListener((group, checkedId) -> {
            if (checkedId == R.id.rb_metro_off) {
                metronomeMode = METRO_OFF;
            } else if (checkedId == R.id.rb_metro_always) {
                metronomeMode = METRO_ALWAYS;
            } else if (checkedId == R.id.rb_metro_countdown) {
                metronomeMode = METRO_COUNTDOWN;
            }
            layoutCountdownBeats.setVisibility(
                metronomeMode == METRO_COUNTDOWN ? View.VISIBLE : View.GONE);
        });

        // 倒数拍数 +/-
        btnCountdownDec.setOnClickListener(v -> {
            if (countdownBeats > 1) {
                countdownBeats--;
                tvCountdownValue.setText(String.valueOf(countdownBeats));
            }
        });
        btnCountdownInc.setOnClickListener(v -> {
            if (countdownBeats < 16) {
                countdownBeats++;
                tvCountdownValue.setText(String.valueOf(countdownBeats));
            }
        });

        // 每小节拍数 +/-
        btnBeatsDec.setOnClickListener(v -> {
            if (currentBeats > 2) {
                currentBeats--;
                tvBeatsValue.setText(String.valueOf(currentBeats));
            }
        });
        btnBeatsInc.setOnClickListener(v -> {
            if (currentBeats < 8) {
                currentBeats++;
                tvBeatsValue.setText(String.valueOf(currentBeats));
            }
        });

        // BPM单步 +/-
        btnBpmDec.setOnClickListener(v -> adjustBpm(-1));
        btnBpmInc.setOnClickListener(v -> adjustBpm(+1));
        // BPM大步 ±5
        btnBpmDecBig.setOnClickListener(v -> adjustBpm(-5));
        btnBpmIncBig.setOnClickListener(v -> adjustBpm(+5));

        // 应用并发送
        btnApplyLooperSettings.setOnClickListener(v -> applyMetronomeSettings());
    }

    /** 调整 BPM 并更新侧边栏显示 */
    private void adjustBpm(int delta) {
        int newBpm = currentBpm + delta;
        if (newBpm < 60)  newBpm = 60;
        if (newBpm > 200) newBpm = 200;
        currentBpm = newBpm;
        tvBpmValue.setText(String.valueOf(currentBpm));
    }

    /** 打开侧边栏（带滑入动画） */
    private void openDrawer() {
        syncDrawerUiFromState();
        drawerLooperSettings.setVisibility(View.VISIBLE);
        looperDrawerOverlay.setVisibility(View.VISIBLE);
        drawerLooperSettings.setTranslationX(drawerLooperSettings.getWidth());
        drawerLooperSettings.animate().translationX(0).setDuration(250).start();
    }

    /** 关闭侧边栏（带滑出动画） */
    private void closeDrawer() {
        drawerLooperSettings.animate()
            .translationX(drawerLooperSettings.getWidth())
            .setDuration(250)
            .withEndAction(() -> {
                drawerLooperSettings.setVisibility(View.GONE);
                looperDrawerOverlay.setVisibility(View.GONE);
            }).start();
    }

    /** 将当前设置应用到顶部信息栏 & 发送 BLE 命令到硬件 */
    private void applyMetronomeSettings() {
        // 发送 BPM
        sendCommand("metro bpm " + currentBpm, null);
        // 发送每小节拍数
        sendCommand("metro beats " + currentBeats, null);
        // 发送节拍器模式
        switch (metronomeMode) {
            case METRO_OFF:
                sendCommand("metro off", null);
                break;
            case METRO_ALWAYS:
                sendCommand("metro on", null);
                break;
            case METRO_COUNTDOWN:
                // 倒数逻辑由 App 侧控制，硬件保持关闭状态，
                // 录制时再动态发送 metro on/off
                sendCommand("metro off", null);
                break;
        }
        refreshTopInfoBar();
        closeDrawer();
        Toast.makeText(this, "设置已应用并发送", Toast.LENGTH_SHORT).show();
    }

    /** 刷新顶部信息栏四个字段 */
    private void refreshTopInfoBar() {
        if (tvTopBpm == null) return;
        tvTopBpm.setText(String.valueOf(currentBpm));
        tvTopBeats.setText(currentBeats + "/4");
        switch (metronomeMode) {
            case METRO_OFF:
                tvTopMetroMode.setText("关闭");
                tvTopMetroMode.setTextColor(Color.parseColor("#888888"));
                break;
            case METRO_ALWAYS:
                tvTopMetroMode.setText("全程");
                tvTopMetroMode.setTextColor(Color.parseColor("#00FFA3"));
                break;
            case METRO_COUNTDOWN:
                tvTopMetroMode.setText("倒数" + countdownBeats);
                tvTopMetroMode.setTextColor(Color.parseColor("#FFB800"));
                break;
        }
        updateHwStatusIndicator();
    }

    /** 根据蓝牙连接状态更新"硬件"指示 */
    private void updateHwStatusIndicator() {
        if (tvTopHwStatus == null) return;
        boolean connected = bluetoothHelper != null && bluetoothHelper.isConnected();
        tvTopHwStatus.setText(connected ? "已连接" : "未连接");
        tvTopHwStatus.setTextColor(
            Color.parseColor(connected ? "#4CAF50" : "#FF4444"));
    }

    private void setupListeners() {
        for (int i = 0; i < SEG_COUNT; i++) {
            final int idx = i;
            // 主点击区：单击 + 长按 2s 删除
            segMainAreas[i].setOnClickListener(v -> onSegmentCardClick(idx));
            segMainAreas[i].setOnTouchListener((v, event) -> {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        // 只对已录制/已播放/已停止的段开启长按删除
                        if (segStates[idx] != SegState.INACTIVE) {
                            longPressRunnables[idx] = () -> deleteSegment(idx);
                            handler.postDelayed(longPressRunnables[idx], 2000);
                            // 显示长按提示
                            tvSegHint[idx].setText("‹ 长按 2s 删除...");
                            tvSegHint[idx].setTextColor(Color.parseColor("#FF6666"));
                        }
                        break;
                    case MotionEvent.ACTION_UP:
                    case MotionEvent.ACTION_CANCEL:
                        // 指头抬起取消长按计时并恢复提示
                        if (longPressRunnables[idx] != null) {
                            handler.removeCallbacks(longPressRunnables[idx]);
                            longPressRunnables[idx] = null;
                            refreshSegUI(idx); // 恢复 hint 文字
                        }
                        break;
                }
                return false; // 返回 false 让 onClick 仍然触发
            });
            // 右上角齿轮按钮打开配置弹窗
            btnSegConfig[i].setOnClickListener(v -> showSegConfigDialog(idx));
        }

        btnLooperClear.setOnClickListener(v -> {
            sendCommand("looper -c", "Sent: Clear all segments");
            for (int i = 0; i < SEG_COUNT; i++) {
                cancelAutoStop(i);
                segStates[i] = SegState.INACTIVE;
                refreshSegUI(i);
            }
        });

        btnLooperErase.setOnClickListener(v -> showConfirmDialog(
            "Erase Flash",
            "This will erase all recorded data (~20s). Continue?",
            () -> {
                sendCommand("looper -e", "Sent: Erase chip");
                for (int i = 0; i < SEG_COUNT; i++) {
                    cancelAutoStop(i);
                    segStates[i] = SegState.INACTIVE;
                    refreshSegUI(i);
                }
            }
        ));

        btnLooperReset.setOnClickListener(v -> showConfirmDialog(
            "Looper Reset",
            "This will reset all states and erase Flash. Continue?",
            () -> {
                sendCommand("looper -R", "Sent: Reset");
                for (int i = 0; i < SEG_COUNT; i++) {
                    cancelAutoStop(i);
                    segStates[i] = SegState.INACTIVE;
                    refreshSegUI(i);
                }
            }
        ));
    }

    private void onSegmentCardClick(int idx) {
        // 如果长按计时器未完成，单击仅取消计时器不做其它操作
        if (longPressRunnables[idx] != null) return;
        if (!checkConnection()) return;

        // 正在倒计时中 → 再次点击取消倒计时
        if (segInCountdown[idx]) {
            cancelCountdown(idx);
            return;
        }

        // 切换到新状态前，先取消该段的自动停止定时
        cancelAutoStop(idx);

        SegState current = segStates[idx];

        if (current == SegState.INACTIVE) {
            // INACTIVE → 根据节拍器模式决定是否先倒计时再录制
            startRecordingWithMetronome(idx);
            return;
        }

        final String cmd;
        final SegState nextState;
        switch (current) {
            case RECORDING:
                cmd = "looper -p " + idx;
                nextState = SegState.PLAYING;
                break;
            case PLAYING:
                cmd = "looper -t " + idx;
                nextState = SegState.STOPPED;
                break;
            case STOPPED:
            default:
                cmd = "looper -p " + idx;
                nextState = SegState.PLAYING;
                break;
        }

        sendCommandWithCallback(cmd, success -> {
            if (!success) return;
            segStates[idx] = nextState;
            refreshSegUI(idx);
        });
    }

    /**
     * 根据侧边栏节拍器模式，决定是否先倒数拍子再开始录制。
     *
     * METRO_OFF:       直接开始录制，不发节拍器指令
     * METRO_ALWAYS:    先发 metronome -on，再立即开始录制（节拍器全程响）
     * METRO_COUNTDOWN: 先发 metronome -on 倒数 N 拍，等待后关节拍器再开始录制
     */
    private void startRecordingWithMetronome(int idx) {
        switch (metronomeMode) {

            case METRO_OFF:
                // 不动节拍器，直接录制
                doStartRecording(idx);
                break;

            case METRO_ALWAYS:
                // 打开节拍器，等写入完成后再开始录制
                refreshTopInfoBar();
                sendCommandWithCallback("metro on", onSuccess -> doStartRecording(idx));
                break;

            case METRO_COUNTDOWN: {
                // 先打开节拍器，倒数 countdownBeats 拍后，关节拍器再开始录制
                sendCommand("metro on", null);
                refreshTopInfoBar();

                long msPerBeat = (long)(60000.0 / currentBpm);
                long waitMs    = msPerBeat * countdownBeats;

                segInCountdown[idx] = true;
                showCountdownHint(idx, countdownBeats, msPerBeat);

                countdownRunnables[idx] = () -> {
                    countdownRunnables[idx] = null;
                    segInCountdown[idx] = false;
                    refreshTopInfoBar();
                    // 等 metro off 写入完成后，再发送录制命令，避免 BLE 并发写入被拒绝
                    sendCommandWithCallback("metro off", offSuccess -> doStartRecording(idx));
                };
                handler.postDelayed(countdownRunnables[idx], waitMs);
                break;
            }
        }
    }

    /** 实际发送 looper -r 并切换 UI 状态 */
    private void doStartRecording(int idx) {
        sendCommandWithCallback("looper -r " + idx, success -> {
            if (!success) return;
            segStates[idx] = SegState.RECORDING;
            refreshSegUI(idx);
            // 配置了小节数 → 启动自动停止计时
            if (segMeasures[idx] > 0) {
                scheduleAutoStop(idx);
            }
        });
    }

    /**
     * 取消正在进行的倒计时（用户重新点击卡片时调用）。
     * 同时关闭节拍器并恢复卡片显示。
     */
    private void cancelCountdown(int idx) {
        if (countdownRunnables[idx] != null) {
            handler.removeCallbacks(countdownRunnables[idx]);
            countdownRunnables[idx] = null;
        }
        if (segInCountdown[idx]) {
            segInCountdown[idx] = false;
            sendCommand("metro off", null);
            refreshTopInfoBar();
            refreshSegUI(idx);
        }
    }

    /**
     * 在卡片上显示倒计时提示（每拍更新一次，直到倒计时结束或被取消）。
     */
    private void showCountdownHint(int idx, int remaining, long msPerBeat) {
        if (remaining > 0) {
            // 倒计时进行中：如已被取消则停止更新
            if (!segInCountdown[idx]) return;
            tvSegState[idx].setText("READY");
            tvSegState[idx].setTextColor(COLOR_INACTIVE);
            tvSegHint[idx].setText("🎵 准备... " + remaining + " 拍");
            tvSegHint[idx].setTextColor(Color.parseColor("#FFB800"));
            final int r = remaining;
            handler.postDelayed(() -> showCountdownHint(idx, r - 1, msPerBeat), msPerBeat);
        } else {
            // 倒计时结束瞬间：无论 segInCountdown 是否已被 countdownRunnable 清除，
            // 只要卡片仍在 INACTIVE（尚未进入 RECORDING）就显示"开始!"提示
            if (segStates[idx] == SegState.INACTIVE) {
                tvSegHint[idx].setText("🎵 开始!");
                tvSegHint[idx].setTextColor(Color.parseColor("#FFB800"));
            }
        }
    }

    /** 计算持续时长并启动自动停止定时器 */
    private void scheduleAutoStop(int idx) {
        long msPerBeat    = (long)(60000.0 / currentBpm);
        long msPerMeasure = msPerBeat * currentBeats;
        long totalMs      = msPerMeasure * segMeasures[idx];

        autoStopRunnables[idx] = () -> autoStopSegment(idx);
        handler.postDelayed(autoStopRunnables[idx], totalMs);

        // 卡片提示展示剩余小节数倒计时
        scheduleCountdownHint(idx, segMeasures[idx], msPerMeasure);
    }

    /** 在卡片上展示录制倒计时提示（运行在 UI 线） */
    private void scheduleCountdownHint(int idx, int remaining, long msPerMeasure) {
        if (remaining <= 0 || segStates[idx] != SegState.RECORDING) return;
        final int r = remaining;
        tvSegHint[idx].setText("录制中... " + r + " 小节");
        tvSegHint[idx].setTextColor(Color.parseColor("#FF6644"));
        handler.postDelayed(() -> {
            if (segStates[idx] == SegState.RECORDING) {
                scheduleCountdownHint(idx, r - 1, msPerMeasure);
            }
        }, msPerMeasure);
    }

    /** 自动停止录制（计时器到期回调） */
    private void autoStopSegment(int idx) {
        autoStopRunnables[idx] = null;
        if (segStates[idx] != SegState.RECORDING) return;

        if (segAutoPlay[idx]) {
            // 停止录制并自动播放
            sendCommandWithCallback("looper -p " + idx, success -> {
                if (!success) return;
                segStates[idx] = SegState.PLAYING;
                refreshSegUI(idx);
                refreshSegCfgHint(idx);
            });
        } else {
            // 停止录制，不播放
            sendCommandWithCallback("looper -t " + idx, success -> {
                if (!success) return;
                segStates[idx] = SegState.STOPPED;
                refreshSegUI(idx);
                refreshSegCfgHint(idx);
            });
        }
    }

    /** 取消指定段的自动停止定时 */
    private void cancelAutoStop(int idx) {
        if (autoStopRunnables[idx] != null) {
            handler.removeCallbacks(autoStopRunnables[idx]);
            autoStopRunnables[idx] = null;
        }
    }

    /**
     * 长按 2s 触发：删除该段录音并恢复到 INACTIVE
     * 对应硬件命令：looper -c <idx>
     */
    private void deleteSegment(int idx) {
        longPressRunnables[idx] = null;
        cancelAutoStop(idx);
        // 卡片闪烁反馈
        segCards[idx].animate().alpha(0.2f).setDuration(80)
            .withEndAction(() -> segCards[idx].animate().alpha(1f).setDuration(80).start())
            .start();
        sendCommandWithCallback("looper -c " + idx, success -> {
            segStates[idx] = SegState.INACTIVE;
            refreshSegUI(idx);
            refreshSegCfgHint(idx);
            Toast.makeText(this, "LOOP " + (idx + 1) + " 录音已删除", Toast.LENGTH_SHORT).show();
        });
    }

    private void refreshSegUI(int idx) {
        String stateLabel, hintLabel;
        int textColor, bgTint;

        switch (segStates[idx]) {
            case RECORDING:
                stateLabel = "REC";
                hintLabel  = "Tap to stop & play";
                textColor  = COLOR_RECORDING;
                bgTint     = TINT_RECORDING;
                break;
            case PLAYING:
                stateLabel = "PLAY";
                hintLabel  = "Tap to pause";
                textColor  = COLOR_PLAYING;
                bgTint     = TINT_PLAYING;
                break;
            case STOPPED:
                stateLabel = "PAUSED";
                hintLabel  = "Tap to resume";
                textColor  = COLOR_STOPPED;
                bgTint     = TINT_STOPPED;
                break;
            default:
                stateLabel = "READY";
                hintLabel  = "Tap to record";
                textColor  = COLOR_INACTIVE;
                bgTint     = TINT_INACTIVE;
                break;
        }

        tvSegName[idx].setText("LOOP " + (idx + 1));
        tvSegName[idx].setTextColor(textColor);
        segCards[idx].setBackgroundTintList(ColorStateList.valueOf(bgTint));

        // 倒计时期间保留倒计时提示，不覆盖 state/hint 文字
        if (segInCountdown[idx]) return;

        tvSegState[idx].setText(stateLabel);
        tvSegState[idx].setTextColor(textColor);
        tvSegHint[idx].setText(hintLabel);
    }

    private void setupBleListener() {
        final StringBuilder accum = new StringBuilder();
        bluetoothHelper.setBleNotifyListener(data -> {
            if (data == null || data.isEmpty()) return;
            runOnUiThread(() -> {
                updateHwStatusIndicator();

                // ── 二进制包路径：AA 55 20 0E ... (looper runtime status) ──
                String upper = data.toUpperCase();
                if (upper.startsWith("AA5520") && upper.length() >= 36) {
                    parseLooperStatusBinary(upper);
                    accum.setLength(0);
                    return;
                }

                // ── 文本路径：累积直到看到 Seg[3]: ──
                accum.append(data);
                String buf = accum.toString();
                if (buf.contains("Seg[3]:")) {
                    parseLooperStatusText(buf);
                    accum.setLength(0);
                } else if (buf.contains("\r\n") &&
                        (buf.contains("RECORDING") || buf.contains("PLAYING")
                         || buf.contains("STOPPED")  || buf.contains("INACTIVE")
                         || buf.contains("Error:"))) {
                    accum.setLength(0);
                }
            });
        });
    }

    /**
     * 解析下位机二进制状态包（新协议）
     * 格式: AA 55 20 0E [global_state is_rec is_play active_segs cur_seg erase_pending
     *                   seg0 seg1 seg2 seg3 metro_en bpm_lo bpm_hi beats]
     * 共 18 字节 → hex 字符串 36 字符（大写，无空格）
     */
    private void parseLooperStatusBinary(String hex) {
        try {
            if (hex.length() < 36) return;

            // 段状态：字节 [10..13] → hex 偏移 20
            for (int i = 0; i < SEG_COUNT; i++) {
                // 倒计时中的段不接受 BLE 状态同步，避免覆盖倒计时 UI
                if (segInCountdown[i]) continue;

                int off = 20 + i * 2;
                int val = Integer.parseInt(hex.substring(off, off + 2), 16);
                SegState parsed;
                switch (val) {
                    case 1:  parsed = SegState.RECORDING; break;
                    case 2:  parsed = SegState.PLAYING;   break;
                    case 3:  parsed = SegState.STOPPED;   break;
                    default: parsed = SegState.INACTIVE;  break;
                }
                segStates[i] = parsed;
                refreshSegUI(i);
                if (parsed == SegState.RECORDING
                        && segMeasures[i] > 0
                        && autoStopRunnables[i] == null) {
                    scheduleAutoStop(i);
                }
                if (parsed != SegState.RECORDING) {
                    cancelAutoStop(i);
                }
            }

            // 节拍器信息：字节 [14..17] → hex 偏移 28
            // [14] metro_enabled, [15] bpm_lo, [16] bpm_hi, [17] beats
            int bpmLo = Integer.parseInt(hex.substring(30, 32), 16);
            int bpmHi = Integer.parseInt(hex.substring(32, 34), 16);
            int beats = Integer.parseInt(hex.substring(34, 36), 16);
            int bpm   = bpmLo | (bpmHi << 8);

            if (bpm >= 60 && bpm <= 200) currentBpm    = bpm;
            if (beats >= 2 && beats <= 8) currentBeats = beats;

            refreshTopInfoBar();
        } catch (NumberFormatException e) {
            // 包不完整或损坏，忽略
        }
    }

    /**
     * 解析下位机文本格式状态（调试串口兼容路径）
     * 触发条件：收到包含 "Seg[3]:" 的完整文本块
     */
    private void parseLooperStatusText(String raw) {
        // 从 Metro: 行提取 BPM 和 Beats
        Pattern mp = Pattern.compile("Metro:.*BPM=(\\d+).*Beats=(\\d+)");
        Matcher mm = mp.matcher(raw);
        if (mm.find()) {
            int bpm   = Integer.parseInt(mm.group(1));
            int beats = Integer.parseInt(mm.group(2));
            if (bpm >= 60 && bpm <= 200) currentBpm    = bpm;
            if (beats >= 2 && beats <= 8) currentBeats = beats;
        }

        Pattern p = Pattern.compile("Seg\\[(\\d)\\]:\\s*(\\w+)");
        Matcher m = p.matcher(raw);
        while (m.find()) {
            int    segIdx   = Integer.parseInt(m.group(1));
            String stateStr = m.group(2).toUpperCase();
            if (segIdx < 0 || segIdx >= SEG_COUNT) continue;
            // 倒计时中的段不接受 BLE 状态同步
            if (segInCountdown[segIdx]) continue;
            SegState parsed;
            switch (stateStr) {
                case "RECORDING": parsed = SegState.RECORDING; break;
                case "PLAYING":   parsed = SegState.PLAYING;   break;
                case "STOPPED":   parsed = SegState.STOPPED;   break;
                default:          parsed = SegState.INACTIVE;  break;
            }
            segStates[segIdx] = parsed;
            refreshSegUI(segIdx);
            if (parsed == SegState.RECORDING
                    && segMeasures[segIdx] > 0
                    && autoStopRunnables[segIdx] == null) {
                scheduleAutoStop(segIdx);
            }
            if (parsed != SegState.RECORDING) {
                cancelAutoStop(segIdx);
            }
        }
        refreshTopInfoBar();
    }

    private boolean checkConnection() {
        if (!bluetoothHelper.isConnected()) {
            Toast.makeText(this, "Connect Bluetooth first", Toast.LENGTH_SHORT).show();
            return false;
        }
        return true;
    }

    private void sendCommand(String cmd, String successMsg) {
        if (!bluetoothHelper.isConnected()) return;
        bluetoothHelper.writeCharacteristic(BLE_UUID, (cmd + "\r\n").getBytes(), success -> {
            if (success && successMsg != null)
                runOnUiThread(() -> Toast.makeText(this, successMsg, Toast.LENGTH_SHORT).show());
            else if (!success)
                runOnUiThread(() -> Toast.makeText(this, "Send failed", Toast.LENGTH_SHORT).show());
        });
    }

    private void sendCommandWithCallback(String cmd, SuccessCallback cb) {
        if (!checkConnection()) return;
        bluetoothHelper.writeCharacteristic(BLE_UUID, (cmd + "\r\n").getBytes(), success -> {
            if (!success)
                runOnUiThread(() -> Toast.makeText(this, "Send failed", Toast.LENGTH_SHORT).show());
            if (cb != null) runOnUiThread(() -> cb.onResult(success));
        });
    }

    private void showConfirmDialog(String title, String message, Runnable onConfirm) {
        new AlertDialog.Builder(this)
                .setTitle(title).setMessage(message)
                .setPositiveButton("OK", (d, w) -> onConfirm.run())
                .setNegativeButton("Cancel", null).show();
    }

    /**
     * 步骤7：弹出每段录制配置对话框
     * 可设置：录制多少小节后自动停止 / 停止后是否自动播放
     */
    private void showSegConfigDialog(int idx) {
        // 用可变数组模拟 "局部 final" 变量
        final int[]     tmpMeasures = {segMeasures[idx]};
        final boolean[] tmpAutoPlay = {segAutoPlay[idx]};

        // ---- 构建弹窗主体布局 ----
        android.widget.LinearLayout root = new android.widget.LinearLayout(this);
        root.setOrientation(android.widget.LinearLayout.VERTICAL);
        root.setPadding(48, 32, 48, 16);
        root.setBackgroundColor(Color.parseColor("#16213E"));

        // -- 小节数区域 --
        android.widget.LinearLayout rowMeasures = new android.widget.LinearLayout(this);
        rowMeasures.setOrientation(android.widget.LinearLayout.VERTICAL);
        rowMeasures.setBackgroundColor(Color.parseColor("#0F1419"));
        rowMeasures.setPadding(24, 20, 24, 20);
        android.widget.LinearLayout.LayoutParams blockParams =
            new android.widget.LinearLayout.LayoutParams(
                android.widget.LinearLayout.LayoutParams.MATCH_PARENT,
                android.widget.LinearLayout.LayoutParams.WRAP_CONTENT);
        blockParams.setMargins(0, 0, 0, 16);
        rowMeasures.setLayoutParams(blockParams);

        TextView lblMeasures = new TextView(this);
        lblMeasures.setText("录制小节数");
        lblMeasures.setTextColor(Color.parseColor("#00D9FF"));
        lblMeasures.setTextSize(14);
        lblMeasures.setTypeface(null, android.graphics.Typeface.BOLD);
        lblMeasures.setPadding(0, 0, 0, 12);
        rowMeasures.addView(lblMeasures);

        TextView subMeasures = new TextView(this);
        subMeasures.setText("设为 0 = 手动停止（无限录制）");
        subMeasures.setTextColor(Color.parseColor("#888888"));
        subMeasures.setTextSize(11);
        subMeasures.setPadding(0, 0, 0, 14);
        rowMeasures.addView(subMeasures);

        // +/- 行
        android.widget.LinearLayout rowCtrl = new android.widget.LinearLayout(this);
        rowCtrl.setOrientation(android.widget.LinearLayout.HORIZONTAL);
        rowCtrl.setGravity(android.view.Gravity.CENTER);

        Button btnDec = new Button(this);
        btnDec.setText("−");
        btnDec.setTextSize(22);
        btnDec.setTextColor(Color.WHITE);
        btnDec.setBackgroundColor(Color.parseColor("#1F2937"));
        android.widget.LinearLayout.LayoutParams btnP =
            new android.widget.LinearLayout.LayoutParams(120, 120);
        btnDec.setLayoutParams(btnP);

        final TextView tvVal = new TextView(this);
        tvVal.setTextColor(Color.parseColor("#00FFA3"));
        tvVal.setTextSize(30);
        tvVal.setTypeface(null, android.graphics.Typeface.BOLD);
        tvVal.setGravity(android.view.Gravity.CENTER);
        android.widget.LinearLayout.LayoutParams valP =
            new android.widget.LinearLayout.LayoutParams(0,
                android.widget.LinearLayout.LayoutParams.WRAP_CONTENT, 1f);
        tvVal.setLayoutParams(valP);
        tvVal.setText(tmpMeasures[0] == 0 ? "∞" : String.valueOf(tmpMeasures[0]));

        Button btnInc = new Button(this);
        btnInc.setText("+");
        btnInc.setTextSize(22);
        btnInc.setTextColor(Color.WHITE);
        btnInc.setBackgroundColor(Color.parseColor("#1F2937"));
        btnInc.setLayoutParams(btnP);

        btnDec.setOnClickListener(v -> {
            if (tmpMeasures[0] > 0) tmpMeasures[0]--;
            tvVal.setText(tmpMeasures[0] == 0 ? "∞" : String.valueOf(tmpMeasures[0]));
        });
        btnInc.setOnClickListener(v -> {
            if (tmpMeasures[0] < 64) tmpMeasures[0]++;
            tvVal.setText(tmpMeasures[0] == 0 ? "∞" : String.valueOf(tmpMeasures[0]));
        });

        rowCtrl.addView(btnDec);
        rowCtrl.addView(tvVal);
        rowCtrl.addView(btnInc);
        rowMeasures.addView(rowCtrl);

        TextView unitHint = new TextView(this);
        unitHint.setText("小节（范围：0~64，0 = 无限）");
        unitHint.setTextColor(Color.parseColor("#606060"));
        unitHint.setTextSize(10);
        unitHint.setGravity(android.view.Gravity.CENTER);
        unitHint.setPadding(0, 10, 0, 0);
        rowMeasures.addView(unitHint);

        root.addView(rowMeasures);

        // -- 播放模式区域 --
        android.widget.LinearLayout rowPlay = new android.widget.LinearLayout(this);
        rowPlay.setOrientation(android.widget.LinearLayout.VERTICAL);
        rowPlay.setBackgroundColor(Color.parseColor("#0F1419"));
        rowPlay.setPadding(24, 20, 24, 20);
        android.widget.LinearLayout.LayoutParams blockParams2 =
            new android.widget.LinearLayout.LayoutParams(
                android.widget.LinearLayout.LayoutParams.MATCH_PARENT,
                android.widget.LinearLayout.LayoutParams.WRAP_CONTENT);
        blockParams2.setMargins(0, 0, 0, 8);
        rowPlay.setLayoutParams(blockParams2);

        TextView lblPlay = new TextView(this);
        lblPlay.setText("录制结束后");
        lblPlay.setTextColor(Color.parseColor("#00D9FF"));
        lblPlay.setTextSize(14);
        lblPlay.setTypeface(null, android.graphics.Typeface.BOLD);
        lblPlay.setPadding(0, 0, 0, 14);
        rowPlay.addView(lblPlay);

        android.widget.RadioGroup rgPlay = new android.widget.RadioGroup(this);
        rgPlay.setOrientation(android.widget.RadioGroup.VERTICAL);

        android.widget.RadioButton rbAutoPlay = new android.widget.RadioButton(this);
        rbAutoPlay.setText("自动开始播放");
        rbAutoPlay.setTextColor(Color.WHITE);
        rbAutoPlay.setTextSize(14);
        rbAutoPlay.setButtonTintList(android.content.res.ColorStateList.valueOf(
            Color.parseColor("#00D9FF")));
        rbAutoPlay.setId(View.generateViewId());

        android.widget.RadioButton rbNoPlay = new android.widget.RadioButton(this);
        rbNoPlay.setText("停止录制，不自动播放");
        rbNoPlay.setTextColor(Color.WHITE);
        rbNoPlay.setTextSize(14);
        rbNoPlay.setButtonTintList(android.content.res.ColorStateList.valueOf(
            Color.parseColor("#00D9FF")));
        rbNoPlay.setId(View.generateViewId());

        rgPlay.addView(rbAutoPlay);
        rgPlay.addView(rbNoPlay);
        if (tmpAutoPlay[0]) rgPlay.check(rbAutoPlay.getId());
        else                rgPlay.check(rbNoPlay.getId());

        rgPlay.setOnCheckedChangeListener((g, id) ->
            tmpAutoPlay[0] = (id == rbAutoPlay.getId()));

        rowPlay.addView(rgPlay);
        root.addView(rowPlay);

        // ---- 显示对话框 ----
        new AlertDialog.Builder(this)
            .setTitle("LOOP " + (idx + 1) + " 录制配置")
            .setView(root)
            .setPositiveButton("确定", (d, w) -> {
                segMeasures[idx] = tmpMeasures[0];
                segAutoPlay[idx] = tmpAutoPlay[0];
                refreshSegCfgHint(idx);
                // 发送配置到硬件
                if (segMeasures[idx] == 0) {
                    sendCommand("looper -cfg " + idx + " measures 0", null);
                } else {
                    sendCommand("looper -cfg " + idx + " measures " + segMeasures[idx], null);
                }
                sendCommand(
                    "looper -cfg " + idx + " autoplay " + (segAutoPlay[idx] ? "1" : "0"), null);
                Toast.makeText(this,
                    "LOOP " + (idx+1) + " 配置已保存", Toast.LENGTH_SHORT).show();
            })
            .setNegativeButton("取消", null)
            .show();
    }

    /**
     * 步骤8：刷新卡片底部配置提示文字
     * 格式："∞小节 ► 播放" / "4小节 ■ 停止"
     */
    private void refreshSegCfgHint(int idx) {
        if (tvSegCfgHint == null || tvSegCfgHint[idx] == null) return;
        String measures = (segMeasures[idx] == 0) ? "∞" : (segMeasures[idx] + "小节");
        String action   = segAutoPlay[idx] ? "► 播放" : "■ 停止";
        tvSegCfgHint[idx].setText(measures + "  " + action);
        tvSegCfgHint[idx].setTextColor(
            segAutoPlay[idx]
                ? Color.parseColor("#50C8D8")
                : Color.parseColor("#909090"));
    }

    interface SuccessCallback { void onResult(boolean success); }
}
