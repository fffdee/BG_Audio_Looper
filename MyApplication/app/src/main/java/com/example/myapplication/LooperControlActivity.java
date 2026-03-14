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
import android.widget.SeekBar;
import android.widget.Switch;
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
    private static final int SEG_COUNT = 2;

    // -------- 节拍器独立开关 --------
    // true = 录制中开启节拍器；false = 录制中静音
    private boolean metroOnDuringRec   = false;
    // true = 录制前先倒数拍；false = 直接开始录制
    private boolean countdownBeforeRec = true;
    // true = LOOP 2 等待 LOOP 1 循环到头再开始录制
    private boolean seg1FollowSeg0     = false;
    // true = LOOP 2 录制时自动对齐 LOOP 1 的录制长度
    private boolean seg1MatchDuration  = false;

    // Segment state enum
    private enum SegState { INACTIVE, RECORDING, PLAYING, STOPPED }

    private BluetoothHelper bluetoothHelper;
    private final Handler handler = new Handler(Looper.getMainLooper());

    // App-side state tracking
    private final SegState[] segStates = {
        SegState.INACTIVE, SegState.INACTIVE
    };

    // -------- 全局设置状态 --------
    private int currentBpm     = 120;
    private int currentBeats   = 4;   // 每小节拍数
    private int countdownBeats = 4;   // 倒数拍数

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
    private final int[]     segMeasures = {0, 0};
    /** 录制结束后是否自动播放 */
    private final boolean[] segAutoPlay = {true, true};
    /** 每段播放音量 0-100，默认 100 */
    private final int[]     segVolumes  = {100, 100};
    /** 每段最大录制秒数：10 / 30 / 60 */
    private final int[]     segMaxRecSec = {10, 10};

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
    /** 卡片底部重置图标按钮 */
    private ImageButton[]  btnSegReset  = new ImageButton[SEG_COUNT];
    /** 卡片底部最大录制时长显示 */
    private TextView[]     tvSegMaxRec  = new TextView[SEG_COUNT];

    // -------- 自动停止定时器 --------
    /** 每个段的自动停止录制 Runnable，可用于取消 */
    private final Runnable[] autoStopRunnables = new Runnable[SEG_COUNT];

    /** 长按删除计时器（每个段独立） */
    private final Runnable[] longPressRunnables = new Runnable[SEG_COUNT];

    /** 录制前节拍器倒计时定时器（等待N拍后再开始录制） */
    private final Runnable[] countdownRunnables = new Runnable[SEG_COUNT];
    /** 该段是否正在节拍器倒计时中（尚未开始录制） */
    private final boolean[]  segInCountdown     = new boolean[SEG_COUNT];
    /** 该段是否处于 LOOP 跟随同步等待（区别于节拍器倒数） */
    private final boolean[]  segInSyncWait      = new boolean[SEG_COUNT];

    // -------- 循环时长追踪（用于 LOOP 2 跟随 LOOP 1 精确对齐） --------
    /** 每段录制开始的系统时间戳（ms）*/
    private final long[] segRecordStartTime = new long[SEG_COUNT];
    /** 每段的实际循环时长（ms），录制结束时计算 */
    private final long[] segLoopDurationMs  = new long[SEG_COUNT];
    /** 每段最近一次进入 PLAYING 状态的系统时间戳（ms）*/
    private final long[] segPlayStartTime   = new long[SEG_COUNT];

    // -------- UI：循环同步信息面板 --------
    private View     layoutLoopSyncInfo;
    private TextView tvLoopSyncInfo;
    private TextView tvLoopSyncTitle;

    // -------- UI：顶部信息栏 --------
    private TextView tvTopBpm;
    private TextView tvTopBeats;
    private TextView tvTopMetroMode;
    private TextView tvTopHwStatus;

    // -------- UI：侧边栏相关 --------
    private View      drawerLooperSettings;
    private View      looperDrawerOverlay;
    private ImageButton btnCloseLooperDrawer;

    // 侧边栏：节拍器开关
    private Switch swMetroDuringRec;
    private Switch swCountdownBeforeRec;

    // 侧边栏：Looper 设置
    private Switch swSeg1FollowSeg0;
    private Switch swSeg1MatchDuration;

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
    private LinearLayout layoutBpmValue;
    private TextView  tvBpmValue;
    private Button    btnBpmDec;
    private Button    btnBpmInc;

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
        // 查询Looper参数（延迟200ms等待BLE稳定）
        handler.postDelayed(() -> {
            if (bluetoothHelper != null && bluetoothHelper.isConnected()) {
                sendCommand("looper -q", null);
            }
        }, 200);
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
            R.id.card_seg0, R.id.card_seg1
        };
        // 内层 LinearLayout（点击交互用）
        int[] mainAreaIds = {
            R.id.card_seg0_main, R.id.card_seg1_main
        };
        int[][] subIds = {
            {R.id.tv_seg0_name, R.id.tv_seg0_state, R.id.tv_seg0_hint, R.id.tv_seg0_cfg_hint},
            {R.id.tv_seg1_name, R.id.tv_seg1_state, R.id.tv_seg1_hint, R.id.tv_seg1_cfg_hint}
        };
        int[] configBtnIds = {
            R.id.btn_seg0_config, R.id.btn_seg1_config
        };
        int[] resetBtnIds = {
            R.id.btn_seg0_reset, R.id.btn_seg1_reset
        };
        int[] maxRecTvIds = {
            R.id.tv_seg0_max_rec, R.id.tv_seg1_max_rec
        };
        for (int i = 0; i < SEG_COUNT; i++) {
            segCards[i]     = findViewById(cardIds[i]);
            segMainAreas[i] = findViewById(mainAreaIds[i]);
            tvSegName[i]    = findViewById(subIds[i][0]);
            tvSegState[i]   = findViewById(subIds[i][1]);
            tvSegHint[i]    = findViewById(subIds[i][2]);
            tvSegCfgHint[i] = findViewById(subIds[i][3]);
            btnSegConfig[i] = findViewById(configBtnIds[i]);
            btnSegReset[i]  = findViewById(resetBtnIds[i]);
            tvSegMaxRec[i]  = findViewById(maxRecTvIds[i]);
        }

        // 循环同步信息面板
        layoutLoopSyncInfo = findViewById(R.id.layout_loop_sync_info);
        tvLoopSyncInfo     = findViewById(R.id.tv_loop_sync_info);
        tvLoopSyncTitle    = findViewById(R.id.tv_loop_sync_title);

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

        // 节拍器开关
        swMetroDuringRec    = findViewById(R.id.sw_metro_during_rec);
        swCountdownBeforeRec = findViewById(R.id.sw_countdown_before_rec);
        // Looper 设置
        swSeg1FollowSeg0    = findViewById(R.id.sw_seg1_follow_seg0);
        swSeg1MatchDuration = findViewById(R.id.sw_seg1_match_duration);
        // 倒数拍数控件
        layoutCountdownBeats = findViewById(R.id.layout_countdown_beats);
        tvCountdownValue    = findViewById(R.id.tv_countdown_value);
        btnCountdownDec     = findViewById(R.id.btn_countdown_dec);
        btnCountdownInc     = findViewById(R.id.btn_countdown_inc);

        // 每小节拍数
        tvBeatsValue = findViewById(R.id.tv_beats_value);
        btnBeatsDec  = findViewById(R.id.btn_beats_dec);
        btnBeatsInc  = findViewById(R.id.btn_beats_inc);

        // BPM
        layoutBpmValue = findViewById(R.id.layout_bpm_value);
        tvBpmValue   = findViewById(R.id.tv_bpm_value);
        btnBpmDec    = findViewById(R.id.btn_bpm_dec);
        btnBpmInc    = findViewById(R.id.btn_bpm_inc);

        // 应用按钮
        btnApplyLooperSettings = findViewById(R.id.btn_apply_looper_settings);

        // 根据当前状态初始化 UI 显示
        syncDrawerUiFromState();
    }

    /** 将当前状态同步到侧边栏 UI */
    private void syncDrawerUiFromState() {
        // 节拍器开关
        swMetroDuringRec.setChecked(metroOnDuringRec);
        swCountdownBeforeRec.setChecked(countdownBeforeRec);
        // Looper设置
        swSeg1FollowSeg0.setChecked(seg1FollowSeg0);
        swSeg1MatchDuration.setChecked(seg1MatchDuration);
        // 倒数拍数行根据倒数开关显示/隐藏
        layoutCountdownBeats.setVisibility(
            countdownBeforeRec ? View.VISIBLE : View.GONE);

        tvCountdownValue.setText(String.valueOf(countdownBeats));
        tvBeatsValue.setText(String.valueOf(currentBeats));
        tvBpmValue.setText(String.valueOf(currentBpm));
    }

    /** 绑定侧边栏各控件的监听器 */
    private void setupDrawerListeners() {
        // 关闭按钮 & 遮罩
        btnCloseLooperDrawer.setOnClickListener(v -> closeDrawer());
        looperDrawerOverlay.setOnClickListener(v -> closeDrawer());

        // 节拍器开关：录制中是否开启节拍器
        swMetroDuringRec.setOnCheckedChangeListener((buttonView, isChecked) -> {
            metroOnDuringRec = isChecked;
        });

        // 节拍器开关：录制前是否倒数拍
        swCountdownBeforeRec.setOnCheckedChangeListener((buttonView, isChecked) -> {
            countdownBeforeRec = isChecked;
            layoutCountdownBeats.setVisibility(
                countdownBeforeRec ? View.VISIBLE : View.GONE);
        });

        // Looper设置：第二段跟随第一段
        swSeg1FollowSeg0.setOnCheckedChangeListener((buttonView, isChecked) -> {
            seg1FollowSeg0 = isChecked;
        });

        // Looper设置：LOOP 2 和 LOOP 1 录制等长
        swSeg1MatchDuration.setOnCheckedChangeListener((buttonView, isChecked) -> {
            seg1MatchDuration = isChecked;
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
        // BPM点击编辑
        layoutBpmValue.setOnClickListener(v -> showBpmEditDialog());

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

    /** 弹出BPM编辑对话框 */
    private void showBpmEditDialog() {
        final android.widget.EditText input = new android.widget.EditText(this);
        input.setInputType(android.text.InputType.TYPE_CLASS_NUMBER);
        input.setText(String.valueOf(currentBpm));
        input.setTextColor(Color.WHITE);
        input.setTextSize(18);
        input.setGravity(android.view.Gravity.CENTER);
        input.setSelection(input.getText().length());
        
        android.widget.LinearLayout container = new android.widget.LinearLayout(this);
        container.setOrientation(android.widget.LinearLayout.VERTICAL);
        container.setPadding(50, 20, 50, 10);
        container.addView(input);
        
        TextView hint = new TextView(this);
        hint.setText("范围：60 ~ 200 BPM");
        hint.setTextColor(Color.parseColor("#888888"));
        hint.setTextSize(12);
        hint.setGravity(android.view.Gravity.CENTER);
        hint.setPadding(0, 10, 0, 0);
        container.addView(hint);

        new AlertDialog.Builder(this)
            .setTitle("设置 BPM")
            .setView(container)
            .setPositiveButton("确定", (d, w) -> {
                try {
                    int newBpm = Integer.parseInt(input.getText().toString().trim());
                    if (newBpm < 60 || newBpm > 200) {
                        Toast.makeText(this, "BPM 范围必须在 60-200 之间", Toast.LENGTH_SHORT).show();
                        return;
                    }
                    currentBpm = newBpm;
                    tvBpmValue.setText(String.valueOf(currentBpm));
                } catch (NumberFormatException e) {
                    Toast.makeText(this, "请输入有效数字", Toast.LENGTH_SHORT).show();
                }
            })
            .setNegativeButton("取消", null)
            .show();
        
        // 自动弹出键盘
        input.requestFocus();
        handler.postDelayed(() -> {
            android.view.inputmethod.InputMethodManager imm = 
                (android.view.inputmethod.InputMethodManager) getSystemService(android.content.Context.INPUT_METHOD_SERVICE);
            if (imm != null) imm.showSoftInput(input, android.view.inputmethod.InputMethodManager.SHOW_IMPLICIT);
        }, 100);
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
        // 节拍器开/关由 App 側在录制时动态控制，这里先关闭
        sendCommand("metro off", null);
        
        refreshTopInfoBar();
        closeDrawer();
        Toast.makeText(this, "设置已应用并发送", Toast.LENGTH_SHORT).show();
    }

    /** 刷新顶部信息栏四个字段 */
    private void refreshTopInfoBar() {
        if (tvTopBpm == null) return;
        tvTopBpm.setText(String.valueOf(currentBpm));
        tvTopBeats.setText(currentBeats + "/4");
        
        // 节拍器显示逻辑：
        String metroLabel;
        int metroColor;
        if (metroOnDuringRec && countdownBeforeRec) {
            metroLabel = "倒数+录制";
            metroColor = Color.parseColor("#FFB800");
        } else if (metroOnDuringRec) {
            metroLabel = "录制中";
            metroColor = Color.parseColor("#00FFA3");
        } else if (countdownBeforeRec) {
            metroLabel = "倒数" + countdownBeats;
            metroColor = Color.parseColor("#FFB800");
        } else {
            metroLabel = "关闭";
            metroColor = Color.parseColor("#888888");
        }
        tvTopMetroMode.setText(metroLabel);
        tvTopMetroMode.setTextColor(metroColor);
        
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
            // 底部时长 chip：仅修改最大录制时长，不触发擦除
            tvSegMaxRec[i].setOnClickListener(v -> showMaxRecDialog(idx));
            // 底部删除按钮：确认后擦除对应 Flash 区域
            btnSegReset[i].setOnClickListener(v -> showSegDeleteDialog(idx));
        }
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

        final SegState prevState = current;
        sendCommandWithCallback(cmd, success -> {
            if (!success) return;
            segStates[idx] = nextState;
            if (nextState == SegState.PLAYING) {
                onSegmentEnteredPlaying(idx, prevState == SegState.RECORDING);
            }
            refreshSegUI(idx);
        });
    }

    /**
     * 根据侧边栏节拍器设置，决定是否先倒数拍子再开始录制。
     *
     * metroOnDuringRec=false & countdownBeforeRec=false: 直接录制，不开节拍器
     * metroOnDuringRec=true  & countdownBeforeRec=false: 先开节拍器，立即录制（节拍器全程响）
     * metroOnDuringRec=any   & countdownBeforeRec=true:  先开节拍器倒数N拍，等待后根据metroOnDuringRec决定是否关闭
     *
     * 倒数拍仅在有其他段正在播放时生效（避免第一个loop录制时的无意义等待）
     */
    private void startRecordingWithMetronome(int idx) {
        // 特殊逻辑：LOOP 2 跟随 LOOP 1 开头录制（仅在 LOOP 1 播放状态下生效）
        if (idx == 1 && seg1FollowSeg0 && segStates[0] == SegState.PLAYING) {
            waitForLoop0BoundaryThenRecord(idx);
            return;
        }

        // 检查是否有其他段正在播放
        boolean hasPlayingSegment = false;
        for (int i = 0; i < SEG_COUNT; i++) {
            if (i != idx && segStates[i] == SegState.PLAYING) {
                hasPlayingSegment = true;
                break;
            }
        }

        // 正常逻辑：无播放段时才倒数（建立节奏基准）；有播放时直接录制（节奏已有参考）
        if (countdownBeforeRec && !hasPlayingSegment) {
            // 无任何段在播放 → 倒数拍，帮助建立节奏
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
                // 倒数结束后：如果录制中不需要节拍器，就关闭
                if (!metroOnDuringRec) {
                    sendCommandWithCallback("metro off", offSuccess -> doStartRecording(idx));
                } else {
                    // 录制中要节拍器，保持开启
                    doStartRecording(idx);
                }
            };
            handler.postDelayed(countdownRunnables[idx], waitMs);
        } else if (metroOnDuringRec) {
            // 有播放段或不倒数，但录制中要节拍器
            refreshTopInfoBar();
            sendCommandWithCallback("metro on", onSuccess -> doStartRecording(idx));
        } else {
            // 有播放段，也不需要节拍器 → 直接录制
            doStartRecording(idx);
        }
    }

    /** 实际发送 looper -r 并切换 UI 状态 */
    private void doStartRecording(int idx) {
        sendCommandWithCallback("looper -r " + idx, success -> {
            if (!success) return;
            segStates[idx] = SegState.RECORDING;
            segRecordStartTime[idx] = System.currentTimeMillis();
            segInSyncWait[idx] = false;
            hideSyncInfoPanel();
            refreshSegUI(idx);
            // LOOP 2 跟随录制时长：若开关开启且 LOOP 1 时长已知，自动按相同时长停止
            if (idx == 1 && seg1MatchDuration && segLoopDurationMs[0] > 0) {
                long dur = segLoopDurationMs[0];
                autoStopRunnables[idx] = () -> autoStopSegment(idx);
                handler.postDelayed(autoStopRunnables[idx], dur);
                // 信息面板显示倒计时
                showMatchDurationCountdown(idx, System.currentTimeMillis() + dur);
            } else if (segMeasures[idx] > 0) {
                scheduleAutoStop(idx);
            }
        });
    }

    /** 在信息面板显示 LOOP 2 录制半长倒计时（每100ms刷新） */
    private void showMatchDurationCountdown(int idx, long endMs) {
        if (segStates[idx] != SegState.RECORDING) {
            hideSyncInfoPanel();
            return;
        }
        long remaining = endMs - System.currentTimeMillis();
        if (remaining < 0) remaining = 0;
        long secs   = remaining / 1000;
        long tenths = (remaining % 1000) / 100;
        tvLoopSyncTitle.setText("⏱ LOOP " + (idx + 1) + " 录制中（跟随 LOOP 1 长度）");
        tvLoopSyncInfo.setText(
            "LOOP 1 循环时长: " + String.format("%.2f", segLoopDurationMs[0] / 1000.0) + "s" +
            "\n完成倒计时: " + secs + "." + tenths + "s"
        );
        layoutLoopSyncInfo.setVisibility(View.VISIBLE);
        if (remaining > 80) {
            handler.postDelayed(() -> showMatchDurationCountdown(idx, endMs), 100);
        }
    }

    /**
     * 记录段进入 PLAYING 状态时的时间，计算循环时长。
     * fromRecording=true 表示从 RECORDING 直接转播放（此时可计算准确时长）
     */
    private void onSegmentEnteredPlaying(int idx, boolean fromRecording) {
        long now = System.currentTimeMillis();
        if (fromRecording && segRecordStartTime[idx] > 0) {
            segLoopDurationMs[idx] = now - segRecordStartTime[idx];
        }
        segPlayStartTime[idx] = now;
    }

    /**
     * 等待 LOOP 1 当前循环结束后再开始 LOOP 2 录制。
     * 利用录制时长 + 已播放时长精确计算剩余等待时间。
     * 倒计时显示在按钮下方信息面板，不影响卡片显示。
     */
    private void waitForLoop0BoundaryThenRecord(int idx) {
        long loopDuration = segLoopDurationMs[0];
        if (loopDuration <= 0) {
            Toast.makeText(this, "LOOP 1 时长未知（未录制），直接开始", Toast.LENGTH_SHORT).show();
            doStartRecordingAfterSync(idx);
            return;
        }
        long now = System.currentTimeMillis();
        // 计算 LOOP 1 已播放了多少 ms（取模得到当前循环内偏移）
        long elapsed   = (now - segPlayStartTime[0]) % loopDuration;
        long remaining = loopDuration - elapsed;
        long targetMs  = now + remaining;

        segInCountdown[idx] = true;
        segInSyncWait[idx]  = true;

        // 在信息面板显示 LOOP 1 时长 + 当前等待
        showSyncInfoPanel(idx, loopDuration, targetMs);

        countdownRunnables[idx] = () -> {
            countdownRunnables[idx] = null;
            segInCountdown[idx]    = false;
            segInSyncWait[idx]     = false;
            hideSyncInfoPanel();
            doStartRecordingAfterSync(idx);
        };
        handler.postDelayed(countdownRunnables[idx], remaining);
    }

    /** 在按钮下方信息面板显示 LOOP 同步信息（每 100ms 刷新） */
    private void showSyncInfoPanel(int idx, long loopDuration, long targetMs) {
        if (!segInSyncWait[idx]) return;
        long remaining = targetMs - System.currentTimeMillis();
        if (remaining < 0) remaining = 0;
        long secs   = remaining / 1000;
        long tenths = (remaining % 1000) / 100;

        tvLoopSyncTitle.setText("🔁 跟随录制 — LOOP " + (idx + 1) + " 等待同步");
        tvLoopSyncInfo.setText(
            "LOOP 1 循环时长: " + String.format("%.2f", loopDuration / 1000.0) + "s" +
            "\n⏳ 距下一循环头: " + secs + "." + tenths + "s\n" +
            "点击 LOOP " + (idx + 1) + " 卡片可取消等待"
        );
        layoutLoopSyncInfo.setVisibility(View.VISIBLE);

        if (remaining > 80) {
            handler.postDelayed(() -> showSyncInfoPanel(idx, loopDuration, targetMs), 100);
        }
    }

    /** 隐藏同步信息面板 */
    private void hideSyncInfoPanel() {
        if (layoutLoopSyncInfo != null) {
            layoutLoopSyncInfo.setVisibility(View.GONE);
        }
    }

    /** 同步等待结束后启动录制（可选开启节拍器） */
    private void doStartRecordingAfterSync(int idx) {
        if (metroOnDuringRec) {
            sendCommandWithCallback("metro on", ok -> doStartRecording(idx));
        } else {
            doStartRecording(idx);
        }
    }

    /**
     * 取消正在进行的倒计时（用户重新点击卡片时调用）。
     */
    private void cancelCountdown(int idx) {
        if (countdownRunnables[idx] != null) {
            handler.removeCallbacks(countdownRunnables[idx]);
            countdownRunnables[idx] = null;
        }
        if (segInSyncWait[idx]) {
            // 取消 LOOP 跟随同步等待 → 隐藏信息面板
            segInSyncWait[idx]  = false;
            segInCountdown[idx] = false;
            hideSyncInfoPanel();
            refreshSegUI(idx);
        } else if (segInCountdown[idx]) {
            // 取消节拍器倒数 → 关节拍器，隐藏信息面板，恢复卡片
            segInCountdown[idx] = false;
            sendCommand("metro off", null);
            refreshTopInfoBar();
            hideSyncInfoPanel();
            refreshSegUI(idx);
        }
    }

    /**
     * 在信息面板显示节拍器倒计时（每拍更新，不修改卡片本身）。
     */
    private void showCountdownHint(int idx, int remaining, long msPerBeat) {
        if (!segInCountdown[idx] || segInSyncWait[idx]) return;
        if (remaining > 0) {
            tvLoopSyncTitle.setText("🎵 LOOP " + (idx + 1) + " 录制准备");
            tvLoopSyncInfo.setText(
                "节拍器倒数中... 还剩 " + remaining + " 拍\n" +
                "BPM: " + currentBpm + "  ·  " + countdownBeats + "拍后开始录制\n" +
                "点击 LOOP " + (idx + 1) + " 卡片可取消"
            );
            layoutLoopSyncInfo.setVisibility(View.VISIBLE);
            final int r = remaining;
            handler.postDelayed(() -> showCountdownHint(idx, r - 1, msPerBeat), msPerBeat);
        } else {
            // 倒计时结束瞬间短暂提示"开始!"
            if (segStates[idx] == SegState.INACTIVE && segInCountdown[idx]) {
                tvLoopSyncInfo.setText("🎵 开始录制！");
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
        hideSyncInfoPanel();

        if (segAutoPlay[idx]) {
            // 停止录制并自动播放
            sendCommandWithCallback("looper -p " + idx, success -> {
                if (!success) return;
                segStates[idx] = SegState.PLAYING;
                onSegmentEnteredPlaying(idx, true);
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
            segLoopDurationMs[idx]  = 0;
            segRecordStartTime[idx] = 0;
            segPlayStartTime[idx]   = 0;
            // 如果有其他段在等待同步，且它在等 idx 段，取消等待
            for (int j = 0; j < SEG_COUNT; j++) {
                if (j != idx && segInSyncWait[j]) {
                    cancelCountdown(j);
                    Toast.makeText(this,
                        "LOOP " + (idx+1) + " 已删除，LOOP " + (j+1) + " 跟随等待已取消",
                        Toast.LENGTH_SHORT).show();
                }
            }
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

                // ── 二进制包路径：AA 55 21 09 ... (looper params, type=0x21) ──
                if (upper.startsWith("AA5521") && upper.length() >= 26) {
                    parseLooperParamsBinary(upper);
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

    /**
     * 解析 looper -q 返回的参数二进制包 (type=0x21)
     * 格式 (hex 字符串, 大写无空格):
     *   AA5521 09 [vol0][vol1][vol2][vol3][flash_status][bpm_lo][bpm_hi][beats][mode]
     * 偏移 (chars): 0  2  4  6   8   10  12  14  16          18     20    22     24
     * flash_status: 0=CLEAN(已初始化), 1=USED(需要擦除)
     */
    private void parseLooperParamsBinary(String hex) {
        try {
            if (hex.length() < 26) return;

            // 段音量 [offset 8..15]
            for (int i = 0; i < SEG_COUNT; i++) {
                int off = 8 + i * 2;
                segVolumes[i] = Integer.parseInt(hex.substring(off, off + 2), 16);
                refreshSegCfgHint(i);
            }

            // flash_status [offset 16]
            int flashStatus = Integer.parseInt(hex.substring(16, 18), 16);

            // BPM [offset 18..21]
            int bpmLo = Integer.parseInt(hex.substring(18, 20), 16);
            int bpmHi = Integer.parseInt(hex.substring(20, 22), 16);
            int bpm   = bpmLo | (bpmHi << 8);

            // beats [offset 22]
            int beats = Integer.parseInt(hex.substring(22, 24), 16);

            // 更新 BPM / beats
            if (bpm >= 60 && bpm <= 200) currentBpm = bpm;
            if (beats >= 2 && beats <= 8)  currentBeats = beats;
            refreshTopInfoBar();

            // 同步成功提示
            Toast.makeText(this,
                String.format("Looper 已同步: BPM=%d  Beats=%d", currentBpm, currentBeats),
                Toast.LENGTH_SHORT).show();

            // Flash 状态检查：如果不是 CLEAN，提示擦除
            if (flashStatus != 0) {
                checkAndPromptFlashErase();
            }
        } catch (NumberFormatException e) {
            android.util.Log.e("Looper", "parseLooperParamsBinary error: " + hex, e);
        }
    }

    /**
     * Flash 未初始化时弹对话框询问是否擦除
     */
    private void checkAndPromptFlashErase() {
        new AlertDialog.Builder(this)
            .setTitle("Flash 需要初始化")
            .setMessage("Looper Flash 尚未初始化（上次退出前有录音数据）。\n\n" +
                        "需要全片擦除 (~20s) 才能正常录音。\n是否立即擦除？")
            .setPositiveButton("立即擦除", (d, w) -> {
                sendCommand("looper -e", null);
                Toast.makeText(this, "Flash 擦除中，请稍等约20秒...", Toast.LENGTH_LONG).show();
                for (int i = 0; i < SEG_COUNT; i++) {
                    cancelAutoStop(i);
                    segStates[i] = SegState.INACTIVE;
                    refreshSegUI(i);
                }
            })
            .setNegativeButton("稍后", null)
            .show();
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
        final int[]     tmpVolume   = {segVolumes[idx]};

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

        // -- 音量区域 --
        android.widget.LinearLayout rowVolume = new android.widget.LinearLayout(this);
        rowVolume.setOrientation(android.widget.LinearLayout.VERTICAL);
        rowVolume.setBackgroundColor(Color.parseColor("#0F1419"));
        rowVolume.setPadding(24, 20, 24, 20);
        android.widget.LinearLayout.LayoutParams blockVolParams =
            new android.widget.LinearLayout.LayoutParams(
                android.widget.LinearLayout.LayoutParams.MATCH_PARENT,
                android.widget.LinearLayout.LayoutParams.WRAP_CONTENT);
        blockVolParams.setMargins(0, 0, 0, 8);
        rowVolume.setLayoutParams(blockVolParams);

        // 标题行：标签 + 当前数值
        android.widget.LinearLayout volTitleRow = new android.widget.LinearLayout(this);
        volTitleRow.setOrientation(android.widget.LinearLayout.HORIZONTAL);
        volTitleRow.setGravity(android.view.Gravity.CENTER_VERTICAL);

        TextView lblVolume = new TextView(this);
        lblVolume.setText("播放音量");
        lblVolume.setTextColor(Color.parseColor("#00D9FF"));
        lblVolume.setTextSize(14);
        lblVolume.setTypeface(null, android.graphics.Typeface.BOLD);
        android.widget.LinearLayout.LayoutParams lblVolP =
            new android.widget.LinearLayout.LayoutParams(0,
                android.widget.LinearLayout.LayoutParams.WRAP_CONTENT, 1f);
        lblVolume.setLayoutParams(lblVolP);

        final TextView tvVolValue = new TextView(this);
        tvVolValue.setText(tmpVolume[0] + "%");
        tvVolValue.setTextColor(Color.parseColor("#00FFA3"));
        tvVolValue.setTextSize(16);
        tvVolValue.setTypeface(null, android.graphics.Typeface.BOLD);
        tvVolValue.setGravity(android.view.Gravity.END);

        volTitleRow.addView(lblVolume);
        volTitleRow.addView(tvVolValue);
        rowVolume.addView(volTitleRow);

        // SeekBar 0-100
        final SeekBar sbVolume = new SeekBar(this);
        sbVolume.setMax(100);
        sbVolume.setProgress(tmpVolume[0]);
        android.widget.LinearLayout.LayoutParams sbP =
            new android.widget.LinearLayout.LayoutParams(
                android.widget.LinearLayout.LayoutParams.MATCH_PARENT,
                android.widget.LinearLayout.LayoutParams.WRAP_CONTENT);
        sbP.setMargins(0, 12, 0, 4);
        sbVolume.setLayoutParams(sbP);
        sbVolume.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar sb, int progress, boolean fromUser) {
                tmpVolume[0] = progress;
                tvVolValue.setText(progress + "%");
            }
            @Override public void onStartTrackingTouch(SeekBar sb) {}
            @Override public void onStopTrackingTouch(SeekBar sb) {}
        });
        rowVolume.addView(sbVolume);

        root.addView(rowVolume);

        // ---- 显示对话框 ----
        new AlertDialog.Builder(this)
            .setTitle("LOOP " + (idx + 1) + " 录制配置")
            .setView(root)
            .setPositiveButton("确定", (d, w) -> {
                segMeasures[idx] = tmpMeasures[0];
                segAutoPlay[idx] = tmpAutoPlay[0];
                segVolumes[idx]  = tmpVolume[0];
                refreshSegCfgHint(idx);
                // 串行发送：先发小节配置，再发音量（避免 BLE 并发写入被拒绝）
                final String volCmd = "looper -V " + idx + " " + segVolumes[idx];
                sendCommandWithCallback(
                    "looper -cfg " + idx + " autoplay " + (segAutoPlay[idx] ? "1" : "0"),
                    autoplaySent -> {
                        // 等上一条写完再发音量
                        handler.postDelayed(
                            () -> sendCommand(volCmd, null),
                            150);
                    });
                Toast.makeText(this,
                    "LOOP " + (idx+1) + " 音量 " + segVolumes[idx] + "% 已保存", Toast.LENGTH_SHORT).show();
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
        String vol      = "🔊" + segVolumes[idx] + "%";
        tvSegCfgHint[idx].setText(measures + "  " + action + "  " + vol);
        tvSegCfgHint[idx].setTextColor(
            segAutoPlay[idx]
                ? Color.parseColor("#50C8D8")
                : Color.parseColor("#909090"));
        // 同步底部最大录制时长 chip 标签
        if (tvSegMaxRec != null && tvSegMaxRec[idx] != null) {
            tvSegMaxRec[idx].setText("⏱ " + segMaxRecSec[idx] + "s");
        }
    }

    /** 仅修改最大录制时长（chip 点击），不触发擦除 */
    private void showMaxRecDialog(int idx) {
        final int[] options   = {10, 30, 60};
        final String[] labels = {"10 秒", "30 秒", "60 秒"};
        int initSel = 1;
        for (int k = 0; k < options.length; k++) {
            if (options[k] == segMaxRecSec[idx]) { initSel = k; break; }
        }
        final int[] checkedItem = {initSel};
        new AlertDialog.Builder(this)
            .setTitle("⏱ LOOP " + (idx + 1) + " 最大录制时长")
            .setSingleChoiceItems(labels, initSel, (dialog, which) ->
                checkedItem[0] = which)
            .setPositiveButton("确定", (d, w) -> {
                segMaxRecSec[idx] = options[checkedItem[0]];
                refreshSegCfgHint(idx);
            })
            .setNegativeButton("取消", null)
            .show();
    }

    /** 删除 LOOP 段：确认后按当前时长擦除对应 Flash 区域 */
    private void showSegDeleteDialog(int idx) {
        // 检查是否有任何段在播放中
        boolean hasPlayingSegment = false;
        for (int i = 0; i < SEG_COUNT; i++) {
            if (segStates[i] == SegState.PLAYING) {
                hasPlayingSegment = true;
                break;
            }
        }
        
        // 如果有段在播放，禁止删除
        if (hasPlayingSegment) {
            Toast.makeText(this,
                "播放中不允许删除段。请先停止播放。",
                Toast.LENGTH_SHORT).show();
            return;
        }

        int sec    = segMaxRecSec[idx];
        int blocks = (int) Math.ceil((sec * 192000.0) / (64 * 1024));
        int estSec = (int) Math.ceil(blocks * 0.15);
        new AlertDialog.Builder(this)
            .setTitle("🗑 删除 LOOP " + (idx + 1))
            .setMessage("将按当前最大录制时长 " + sec + "s (约 " + blocks +
                " 个 64KB 块）擦除对应 Flash 区域，预计约 " + estSec + "s。\n\n确认删除？")
            .setPositiveButton("确认删除", (d, w) -> {
                cancelAutoStop(idx);
                cancelCountdown(idx);
                segStates[idx]         = SegState.INACTIVE;
                segLoopDurationMs[idx]  = 0;
                segRecordStartTime[idx] = 0;
                segPlayStartTime[idx]   = 0;
                refreshSegUI(idx);
                refreshSegCfgHint(idx);
                sendCommand("looper -I " + idx + " " + sec, null);
                Toast.makeText(this,
                    "LOOP " + (idx + 1) + " 删除中，擦除 Flash 区域（" + sec + "s / " + blocks + " 块）...",
                    Toast.LENGTH_LONG).show();
            })
            .setNegativeButton("取消", null)
            .show();
    }

    interface SuccessCallback { void onResult(boolean success); }
}
