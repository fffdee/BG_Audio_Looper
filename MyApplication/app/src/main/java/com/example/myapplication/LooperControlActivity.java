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
import android.widget.ProgressBar;
import android.widget.SeekBar;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.core.content.ContextCompat;
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
public class LooperControlActivity extends BaseActivity {

    private static final String BLE_UUID = "0000ab01-0000-1000-8000-00805f9b34fb";
    private static final int SEG_COUNT = 4;

    // -------- 存储空间常量 --------
    /** 总Flash空间：8MB (与下位机一致) */
    private static final long TOTAL_STORAGE_BYTES = 8L * 1024 * 1024;  // 8388608 bytes
    /** 音频数据速率：48kHz × 32bit × 2声道 = 192000 bytes/sec (与下位机一致) */
    private static final long AUDIO_BYTES_PER_SEC = 192000L;
    /** 总录制时间（秒）= 总空间 / 数据速率 */
    private static final float TOTAL_REC_TIME_SEC = (float) TOTAL_STORAGE_BYTES / AUDIO_BYTES_PER_SEC;  // ≈43.69s
    /** 进度条最大值（千分比，精度0.1%） */
    private static final int PROGRESS_MAX = 1000;

    // -------- 节拍器独立开关（static：Activity 重建时保留） --------
    // true = 录制中开启节拍器；false = 录制中静音
    private static boolean metroOnDuringRec   = false;
    // true = 录制前先倒数拍；false = 直接开始录制
    private static boolean countdownBeforeRec = true;
    // 每段独立：跟随「先录段」循环头开始录制
    private static final boolean[] segFollowEnabled = {false, false, false, false};
    // 每段独立：录制时自动对齐参考段的录制长度
    private static final boolean[] segMatchEnabled  = {false, false, false, false};

    // Segment state enum
    private enum SegState { INACTIVE, RECORDING, PLAYING, STOPPED }

    private BluetoothHelper bluetoothHelper;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private boolean isLooperModeOn = true;

    // App-side state tracking（static：Activity 重建时保留段状态）
    private static final SegState[] segStates = {
            SegState.INACTIVE, SegState.INACTIVE, SegState.INACTIVE, SegState.INACTIVE
    };

    // -------- 全局设置状态（static：Activity 重建时保留） --------
    private static int currentBpm     = 120;
    private static int currentBeats   = 4;   // 每小节拍数
    private static int countdownBeats = 4;   // 倒数拍数

    // Color constants (theme-aware, initialized in initThemeColors)
    private int COLOR_INACTIVE;
    private int COLOR_RECORDING;
    private int COLOR_PLAYING;
    private int COLOR_STOPPED;

    private int TINT_INACTIVE;
    private int TINT_RECORDING;
    private int TINT_PLAYING;
    private int TINT_STOPPED;

    // -------- 每段独立配置（static：Activity 重建时保留） --------
    /** 录制小节数，0 = 手动停止（无限） */
    private static final int[]     segMeasures = {0, 0, 0, 0};
    /** 录制结束后是否自动播放 */
    private static final boolean[] segAutoPlay = {true, true, true, true};
    /** 每段播放音量 0-100，默认 100 */
    private static final int[]     segVolumes  = {100, 100, 100, 100};
    /** 每段最大录制秒数：10 / 30 / 60 */
    private static final int[]     segMaxRecSec = {10, 10, 10, 10};
    /** 每段预裁剪起始偏移（ms），录制完成后自动应用（0=不裁剪） */
    private static final long[] segPreCropStartMs = {0L, 0L, 0L, 0L};
    /** 每段预裁剪末尾裁除时长（ms，从末尾倒退），录制完成后自动应用（0=不裁剪） */
    private static final long[] segPreCropEndMs   = {0L, 0L, 0L, 0L};

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
    /** 卡片底部：跟随录制（每段独立）*/
    private ImageButton[]  btnSegFollow     = new ImageButton[SEG_COUNT];
    /** 卡片底部：录制等长（每段独立）*/
    private ImageButton[]  btnSegMatch      = new ImageButton[SEG_COUNT];
    /** 卡片底部：参照段结束后录制（每段独立）*/
    private ImageButton[]  btnSegRecordAfterRef = new ImageButton[SEG_COUNT];
    /** 卡片底部：衔接播放（每段独立）*/
    private ImageButton[]  btnSegChain       = new ImageButton[SEG_COUNT];
    /** 卡片底部：接入播放（每段独立）*/
    private ImageButton[]  btnSegJoin        = new ImageButton[SEG_COUNT];
    /** 卡片底部：等待本轮播完再停止（每段独立）*/
    private ImageButton[]  btnSegWaitFinish = new ImageButton[SEG_COUNT];

    // -------- UI：全局控制按钮 --------
    private Button btnPlayAll;    // 全部同时播放
    private Button btnStopAll;    // 全部同时停止
    private Button btnDeleteAll;  // 全部删除

    // -------- UI：倒数大数字框 --------
    private View     layoutCountdownBox;  // 红色方形容器
    private TextView tvCountdownBig;      // 大号数字
    private TextView tvCountdownBigLabel; // 「拍」标签

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

    // -------- 循环时长追踪（static：Activity 重建时保留对齐数据） --------
    /** 每段录制开始的系统时间戳（ms）*/
    private static final long[] segRecordStartTime = new long[SEG_COUNT];
    /** 每段的实际循环时长（ms），录制结束时计算 */
    private static final long[] segLoopDurationMs  = new long[SEG_COUNT];
    /** 每段最近一次进入 PLAYING 状态的系统时间戳（ms）*/
    private static final long[] segPlayStartTime   = new long[SEG_COUNT];

    // -------- 段裁剪起止页（static：Activity 重建时保留裁剪设置） --------
    /** 各段循环起始页（0=从头） */
    private static final int[] segTrimStartPage = new int[SEG_COUNT];
    /** 各段循环终止页（0=到录制末尾） */
    private static final int[] segTrimEndPage   = new int[SEG_COUNT];

    // -------- 全局控制状态（static 部分在 Activity 重建后保留） --------
    /** 每段独立的「等待本轮播完再停止」开关 */
    private static final boolean[] waitFinishEnabled = {false, false, false, false};
    // -------- 参照段追踪 --------
    /** 当前参照段（最早开始播放的段） */
    private int      referenceSegIndex = -1;
    /** UI：参照段显示控件 */
    private TextView tvTopReferenceSeg;
    /** 等待停止本地预测定时（每段独立：等待本轮结束后切换 STOPPED） */
    private final Runnable[] waitFinishPredictRunnables = new Runnable[SEG_COUNT];
    /** 同步录制：下位机已激活 SR（looper -SR 已发出，等待 trigger_seg 回绕） */
    private boolean  srArmed           = false;
    /** 同步录制：本地预测定时（trigger_seg 回绕时刻将 rec_seg 切为 RECORDING） */
    private Runnable srPredictRunnable = null;
    /** 同步录制当前的参考段索引（先录的那一段），-1 = 未激活 */
    private int      srRefSegIdx       = -1;

    // -------- UI：循环同步信息面板 --------
    private View     layoutLoopSyncInfo;
    private TextView tvLoopSyncInfo;
    private TextView tvLoopSyncTitle;

    // -------- UI：音频轨面板 --------
    private View          layoutAudioTracksPanel;
    private LinearLayout  containerAudioTracks;
    /** 每段对应的音频轨卡片 View（null = 未显示） */
    private final View[]  audioTrackViews = new View[SEG_COUNT];

    // -------- UI：录制空间占用条 --------
    private ProgressBar  progressStorage;
    private TextView     tvStorageUsed;
    private TextView     tvStorageTime;
    private TextView[]   tvSegStorage = new TextView[SEG_COUNT];

    // -------- 录制时实时更新存储显示的定时任务 --------
    private final Runnable[] storageUpdateRunnables = new Runnable[SEG_COUNT];

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
        setupBaseToolbar(true);

        initThemeColors();
        bluetoothHelper = BluetoothManager.getInstance().getBluetoothHelper();

        initViews();
        if (savedInstanceState == null) loadSettings();
        initSettingsDrawer();
        setupListeners();
        setupDrawerListeners();
        setupBleListener();
        refreshTopInfoBar();

        sendCommand("looper_mode on", null);
    }

    /** 便捷方法：获取主题感知颜色 */
    private int clr(int resId) { return ContextCompat.getColor(this, resId); }

    /** 初始化主题感知颜色常量 */
    private void initThemeColors() {
        COLOR_INACTIVE  = clr(R.color.color_inactive);
        COLOR_RECORDING = clr(R.color.color_recording);
        COLOR_PLAYING   = clr(R.color.color_playing);
        COLOR_STOPPED   = clr(R.color.color_stopped);
        TINT_INACTIVE   = clr(R.color.tint_inactive);
        TINT_RECORDING  = clr(R.color.tint_recording);
        TINT_PLAYING    = clr(R.color.tint_playing);
        TINT_STOPPED    = clr(R.color.tint_stopped);
    }

    @Override
    protected String getToolbarTitle() {
        return "Looper 控制";
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
                // 延迟查询各段裁剪参数
                for (int seg = 0; seg < SEG_COUNT; seg++) {
                    final int s = seg;
                    handler.postDelayed(() -> sendCommand("looper -T " + s, null), 300L + s * 150L);
                }
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
        sendCommand("looper_mode off", null);
        // 页面销毁时清除所有定时器
        if (srPredictRunnable    != null) { handler.removeCallbacks(srPredictRunnable);    srPredictRunnable    = null; }
        srArmed = false;
        for (int i = 0; i < SEG_COUNT; i++) {
            cancelAutoStop(i);
            cancelCountdown(i);
            if (longPressRunnables[i] != null) {
                handler.removeCallbacks(longPressRunnables[i]);
                longPressRunnables[i] = null;
            }
            if (waitFinishPredictRunnables[i] != null) {
                handler.removeCallbacks(waitFinishPredictRunnables[i]);
                waitFinishPredictRunnables[i] = null;
            }
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.menu_looper, menu);
        MenuItem looperModeItem = menu.findItem(R.id.menu_looper_mode);
        updateLooperModeIcon(looperModeItem);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();
        if (id == android.R.id.home) {
            finish();
            return true;
        } else if (id == R.id.menu_looper_mode) {
            isLooperModeOn = !isLooperModeOn;
            updateLooperModeIcon(item);
            sendCommand("looper_mode " + (isLooperModeOn ? "on" : "off"),
                    isLooperModeOn ? "传统模式" : "效果图模式");
            return true;
        } else if (id == R.id.menu_looper_settings) {
            openDrawer();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void updateLooperModeIcon(MenuItem item) {
        if (item == null) return;
        if (isLooperModeOn) {
            item.setIcon(R.drawable.ic_music_note);
            item.getIcon().setAlpha(255);
        } else {
            item.setIcon(R.drawable.ic_music_note);
            item.getIcon().setAlpha(80);
        }
    }

    private void initViews() {
        // 外层 FrameLayout（背景着色用）
        int[] cardIds = {
                R.id.card_seg0, R.id.card_seg1, R.id.card_seg2, R.id.card_seg3
        };
        // 内层 LinearLayout（点击交互用）
        int[] mainAreaIds = {
                R.id.card_seg0_main, R.id.card_seg1_main, R.id.card_seg2_main, R.id.card_seg3_main
        };
        int[][] subIds = {
                {R.id.tv_seg0_name, R.id.tv_seg0_state, R.id.tv_seg0_hint, R.id.tv_seg0_cfg_hint},
                {R.id.tv_seg1_name, R.id.tv_seg1_state, R.id.tv_seg1_hint, R.id.tv_seg1_cfg_hint},
                {R.id.tv_seg2_name, R.id.tv_seg2_state, R.id.tv_seg2_hint, R.id.tv_seg2_cfg_hint},
                {R.id.tv_seg3_name, R.id.tv_seg3_state, R.id.tv_seg3_hint, R.id.tv_seg3_cfg_hint}
        };
        int[] configBtnIds = {
                R.id.btn_seg0_config, R.id.btn_seg1_config, R.id.btn_seg2_config, R.id.btn_seg3_config
        };
        int[] resetBtnIds = {
                R.id.btn_seg0_reset, R.id.btn_seg1_reset, R.id.btn_seg2_reset, R.id.btn_seg3_reset
        };
        int[] maxRecTvIds = {
                R.id.tv_seg0_max_rec, R.id.tv_seg1_max_rec, R.id.tv_seg2_max_rec, R.id.tv_seg3_max_rec
        };
        int[] followBtnIds = {
                R.id.btn_seg0_follow, R.id.btn_seg1_follow, R.id.btn_seg2_follow, R.id.btn_seg3_follow
        };
        int[] matchBtnIds = {
                R.id.btn_seg0_match, R.id.btn_seg1_match, R.id.btn_seg2_match, R.id.btn_seg3_match
        };
        int[] recordAfterRefBtnIds = {
                R.id.btn_seg0_record_after_ref, R.id.btn_seg1_record_after_ref,
                R.id.btn_seg2_record_after_ref, R.id.btn_seg3_record_after_ref
        };
        int[] chainBtnIds = {
                R.id.btn_seg0_chain, R.id.btn_seg1_chain, R.id.btn_seg2_chain, R.id.btn_seg3_chain
        };
        int[] joinBtnIds = {
                R.id.btn_seg0_join, R.id.btn_seg1_join, R.id.btn_seg2_join, R.id.btn_seg3_join
        };
        int[] waitFinishBtnIds = {
                R.id.btn_seg0_wait_finish, R.id.btn_seg1_wait_finish, R.id.btn_seg2_wait_finish, R.id.btn_seg3_wait_finish
        };
        for (int i = 0; i < SEG_COUNT; i++) {
            segCards[i]        = findViewById(cardIds[i]);
            segMainAreas[i]    = findViewById(mainAreaIds[i]);
            tvSegName[i]       = findViewById(subIds[i][0]);
            tvSegState[i]      = findViewById(subIds[i][1]);
            tvSegHint[i]       = findViewById(subIds[i][2]);
            tvSegCfgHint[i]    = findViewById(subIds[i][3]);
            btnSegConfig[i]    = findViewById(configBtnIds[i]);
            btnSegReset[i]     = findViewById(resetBtnIds[i]);
            tvSegMaxRec[i]     = findViewById(maxRecTvIds[i]);
            btnSegFollow[i]    = findViewById(followBtnIds[i]);
            btnSegMatch[i]     = findViewById(matchBtnIds[i]);
            btnSegRecordAfterRef[i] = findViewById(recordAfterRefBtnIds[i]);
            btnSegChain[i]     = findViewById(chainBtnIds[i]);
            btnSegJoin[i]      = findViewById(joinBtnIds[i]);
            btnSegWaitFinish[i]= findViewById(waitFinishBtnIds[i]);
        }

        // 全局控制区
        btnPlayAll   = findViewById(R.id.btn_play_all);
        btnStopAll   = findViewById(R.id.btn_stop_all);
        btnDeleteAll = findViewById(R.id.btn_delete_all);

        // 参照段显示
        tvTopReferenceSeg = findViewById(R.id.tv_top_reference_seg);

        // 初始化播放时间数组
        for (int i = 0; i < SEG_COUNT; i++) {
            segPlayStartTime[i] = 0;
        }

        // 倒数大数字框
        layoutCountdownBox   = findViewById(R.id.layout_countdown_box);
        tvCountdownBig      = findViewById(R.id.tv_countdown_big);
        tvCountdownBigLabel = findViewById(R.id.tv_countdown_big_label);

        // 循环同步信息面板
        layoutLoopSyncInfo = findViewById(R.id.layout_loop_sync_info);
        tvLoopSyncInfo     = findViewById(R.id.tv_loop_sync_info);
        tvLoopSyncTitle    = findViewById(R.id.tv_loop_sync_title);

        // 音频轨面板
        layoutAudioTracksPanel = findViewById(R.id.layout_audio_tracks_panel);
        containerAudioTracks   = findViewById(R.id.container_audio_tracks);

        // 顶部信息栏
        tvTopBpm       = findViewById(R.id.tv_top_bpm);
        tvTopBeats     = findViewById(R.id.tv_top_beats);
        tvTopMetroMode = findViewById(R.id.tv_top_metro_mode);
        tvTopHwStatus  = findViewById(R.id.tv_top_hw_status);

        // 录制空间占用条
        progressStorage   = findViewById(R.id.progress_storage);
        tvStorageUsed     = findViewById(R.id.tv_storage_used);
        tvStorageTime     = findViewById(R.id.tv_storage_time);
        int[] segStorageIds = {
                R.id.tv_seg0_storage, R.id.tv_seg1_storage,
                R.id.tv_seg2_storage, R.id.tv_seg3_storage
        };
        for (int i = 0; i < SEG_COUNT; i++) {
            tvSegStorage[i] = findViewById(segStorageIds[i]);
        }

        for (int i = 0; i < SEG_COUNT; i++) {
            refreshSegUI(i);
            refreshSegCfgHint(i);
        }
        updateControlButtonStates();
        updateStorageDisplay();  // 初始化存储空间显示
    }

    /** 初始化侧边栏控件引用 */
    private void initSettingsDrawer() {
        drawerLooperSettings = findViewById(R.id.drawer_looper_settings);
        looperDrawerOverlay  = findViewById(R.id.looper_drawer_overlay);
        btnCloseLooperDrawer = findViewById(R.id.btn_close_looper_drawer);

        // 节拍器开关
        swMetroDuringRec    = findViewById(R.id.sw_metro_during_rec);
        swCountdownBeforeRec = findViewById(R.id.sw_countdown_before_rec);
        // Looper 跟随/等长已移至主界面段底部按钮，侧边栏不再有对应 Switch
        swSeg1FollowSeg0    = null;
        swSeg1MatchDuration = null;
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

        // Looper设置：第二段跟随第一段（已移至主界面，保留内部变量更新）
        // swSeg1FollowSeg0 已设为 null，通过 btnSegFollow 按钮控制

        // Looper设置：LOOP 2 和 LOOP 1 录制等长（已移至主界面，保留内部变量更新）
        // swSeg1MatchDuration 已设为 null，通过 btnSegMatch 按钮控制

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
        btnApplyLooperSettings.setOnClickListener(v -> { applyMetronomeSettings(); saveSettings(); });
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
        input.setTextColor(clr(R.color.text_primary));
        input.setTextSize(18);
        input.setGravity(android.view.Gravity.CENTER);
        input.setSelection(input.getText().length());

        android.widget.LinearLayout container = new android.widget.LinearLayout(this);
        container.setOrientation(android.widget.LinearLayout.VERTICAL);
        container.setPadding(50, 20, 50, 10);
        container.addView(input);

        TextView hint = new TextView(this);
        hint.setText("范围：60 ~ 200 BPM");
        hint.setTextColor(clr(R.color.dialog_sub_text));
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
            metroColor = clr(R.color.text_warning);
        } else if (metroOnDuringRec) {
            metroLabel = "录制中";
            metroColor = clr(R.color.text_success);
        } else if (countdownBeforeRec) {
            metroLabel = "倒数" + countdownBeats;
            metroColor = clr(R.color.text_warning);
        } else {
            metroLabel = "关闭";
            metroColor = clr(R.color.text_tertiary);
        }
        tvTopMetroMode.setText(metroLabel);
        tvTopMetroMode.setTextColor(metroColor);

        // 更新参照段显示
        updateReferenceSegmentDisplay();

        updateHwStatusIndicator();
    }

    /**
     * 更新参照段（追踪最早开始播放的段）
     */
    private void updateReferenceSegmentDisplay() {
        if (tvTopReferenceSeg == null) return;

        // 遍历所有播放中的段，找到最早开始的
        long earliestTime = Long.MAX_VALUE;
        int earliestSeg = -1;

        for (int i = 0; i < SEG_COUNT; i++) {
            if (segStates[i] == SegState.PLAYING) {
                long startTs = segPlayStartTime[i] > 0 ? segPlayStartTime[i] : Long.MAX_VALUE - i;
                if (startTs < earliestTime) {
                    earliestTime = startTs;
                    earliestSeg = i;
                }
            }
        }

        // 更新参照段索引
        referenceSegIndex = earliestSeg;

        // 更新显示
        if (referenceSegIndex >= 0) {
            tvTopReferenceSeg.setText("LOOP " + (referenceSegIndex + 1));
            tvTopReferenceSeg.setTextColor(COLOR_PLAYING);
        } else {
            tvTopReferenceSeg.setText("--");
            tvTopReferenceSeg.setTextColor(clr(R.color.text_secondary));
        }
    }

    /** 根据蓝牙连接状态更新"硬件"指示 */
    private void updateHwStatusIndicator() {
        if (tvTopHwStatus == null) return;
        boolean connected = bluetoothHelper != null && bluetoothHelper.isConnected();
        tvTopHwStatus.setText(connected ? "已连接" : "未连接");
        tvTopHwStatus.setTextColor(
                clr(connected ? R.color.status_connected : R.color.status_disconnected));
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
                            tvSegHint[idx].setTextColor(clr(R.color.text_error));
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
            // 底部跟随录制按钮（仅对 LOOP 2 有实际意义，LOOP 1 始终显示但无效）
            btnSegFollow[i].setOnClickListener(v -> onFollowButtonClick(idx));
            // 底部录制等长按钮
            btnSegMatch[i].setOnClickListener(v -> onMatchButtonClick(idx));
            // 底部参照段结束后录制按钮
            btnSegRecordAfterRef[i].setOnClickListener(v -> onRecordAfterRefClick(idx));
            // 底部衔接播放按钮
            btnSegChain[i].setOnClickListener(v -> onChainClick(idx));
            // 底部接入播放按钮
            btnSegJoin[i].setOnClickListener(v -> onJoinClick(idx));
            // 底部等待本轮播完再停止按钮（每段独立）
            btnSegWaitFinish[i].setOnClickListener(v -> onWaitFinishToggle(idx));
        }

        // 全局控制按钮
        btnPlayAll.setOnClickListener(v -> onPlayAllClick());
        btnStopAll.setOnClickListener(v -> onStopAllClick());
        btnDeleteAll.setOnClickListener(v -> onDeleteAllClick());
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
        final long prevPlayStartTime = segPlayStartTime[idx]; // 保存，用于回滚
        // 乐观更新：立即切换 UI，不等待 BLE 回复
        segStates[idx] = nextState;
        if (nextState == SegState.PLAYING) {
            onSegmentEnteredPlaying(idx, prevState == SegState.RECORDING);
        } else if (nextState == SegState.STOPPED) {
            segPlayStartTime[idx] = 0; // 暂停时清除播放位置基准
        }
        refreshSegUI(idx);
        refreshTopInfoBar();
        updateControlButtonStates();
        final String fullCmd;
        if (prevState == SegState.RECORDING && nextState == SegState.PLAYING) {
            String trimCmd = buildPreCropUpdate(idx);
            if (!trimCmd.isEmpty()) upsertAudioTrackCard(idx); // 用裁剪后的参数刷新音频轨卡片
            fullCmd = trimCmd.isEmpty() ? cmd : cmd + "\r\n" + trimCmd;
        } else {
            fullCmd = cmd;
        }
        sendCommandWithCallback(fullCmd, success -> {
            if (!success) {
                // 发送失败：回滚状态
                segStates[idx] = prevState;
                segPlayStartTime[idx] = prevPlayStartTime; // 恢复播放位置基准
                refreshSegUI(idx);
            }
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
        // 通用逻辑：若有先录段正在播放且跟随开关开启，则等待先录段循环头开始录制
        int refIdxForFollow = getRefPlayingSegIdx(idx);
        if (refIdxForFollow >= 0 && segFollowEnabled[idx]) {
            waitForRefBoundaryThenRecord(idx, refIdxForFollow);
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

    /** 实际发送 looper -r 并切换 UI 状态（乐观更新：发命令前先切 UI） */
    private void doStartRecording(int idx) {
        // 乐观更新：立即显示 RECORDING 状态
        final SegState prevState = segStates[idx];
        segStates[idx] = SegState.RECORDING;
        segRecordStartTime[idx] = System.currentTimeMillis();
        segInSyncWait[idx] = false;
        hideSyncInfoPanel();
        refreshSegUI(idx);

        // 启动录制时的存储空间实时更新（每200ms刷新一次）
        startStorageUpdateForRecording(idx);

        // 等长录制：若开关开启且参考段时长已知，自动按相同时长停止（考虑裁剪后的有效时长）
        if (segMatchEnabled[idx]) {
            int ref = (srRefSegIdx >= 0) ? srRefSegIdx : 0;
            long dur = getEffectiveLoopDurationMs(ref);
            if (dur > 0) {
                // 按 LOOP 1 有效时长精确停止，autoStopRunnables 持有引用以便取消
                autoStopRunnables[idx] = () -> autoStopSegment(idx);
                handler.postDelayed(autoStopRunnables[idx], dur);
                showMatchDurationCountdown(idx, System.currentTimeMillis() + dur);
            } else {
                // LOOP 1 时长未知时，按小节/最大时长兜底
                scheduleAutoStop(idx);
            }
            // 注意：不再重复调用 scheduleAutoStop，否则会以更短的小节计时覆盖上方的等长计时器
        }
        sendCommandWithCallback("looper -r " + idx, success -> {
            if (!success) {
                // 回滚
                cancelAutoStop(idx);
                stopStorageUpdateForRecording(idx);
                segStates[idx] = prevState;
                segRecordStartTime[idx] = 0;
                hideSyncInfoPanel();
                refreshSegUI(idx);
            }
        });
    }

    /**
     * 开始录制时的存储空间实时更新
     * 每200ms刷新一次空间占用显示
     */
    private void startStorageUpdateForRecording(final int idx) {
        if (storageUpdateRunnables[idx] != null) {
            handler.removeCallbacks(storageUpdateRunnables[idx]);
        }
        storageUpdateRunnables[idx] = new Runnable() {
            @Override
            public void run() {
                if (segStates[idx] == SegState.RECORDING) {
                    updateStorageDisplay();
                    // 继续下一次更新
                    handler.postDelayed(this, 200);
                }
            }
        };
        handler.post(storageUpdateRunnables[idx]);
    }

    /**
     * 停止录制时的存储空间更新任务
     */
    private void stopStorageUpdateForRecording(int idx) {
        if (storageUpdateRunnables[idx] != null) {
            handler.removeCallbacks(storageUpdateRunnables[idx]);
            storageUpdateRunnables[idx] = null;
        }
        // 最终更新一次显示
        updateStorageDisplay();
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
        tvLoopSyncTitle.setText("⏱ LOOP " + (idx + 1) + " 录制中（跟随先录段长度）");
        int ref = (srRefSegIdx >= 0) ? srRefSegIdx : 0;
        tvLoopSyncInfo.setText(
                "LOOP " + (ref + 1) + " 循环时长: " + String.format("%.2f", getEffectiveLoopDurationMs(ref) / 1000.0) + "s" +
                        "\n完成倒计时: " + secs + "." + tenths + "s"
        );
        layoutLoopSyncInfo.setVisibility(View.VISIBLE);
        // 大数字框显示剩余秒数
        tvCountdownBig.setText(String.valueOf(secs));
        tvCountdownBigLabel.setText("s");
        layoutCountdownBox.setVisibility(View.VISIBLE);
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

        // 录制完成后停止存储空间的实时更新
        if (fromRecording) {
            stopStorageUpdateForRecording(idx);
        }

        // 录制完成后在音频轨面板显示/更新该段的裁剪卡片
        if (fromRecording) {
            upsertAudioTrackCard(idx);
            // 注意：applyPreCrop 必须在 looper -p 确认后再发送，各调用路径各自负责
        }
    }

    /**
     * 同步录制：等待 LOOP 1 当前循环到头后让 LOOP idx 进入录制。
     *
     * ① 读取 LOOP 1 裁剪后总时长 & 当前播放位置，计算倒数时间
     *      remaining = 总时长 - 当前播放位置
     * ② 倒数归零 → 乐观切换 UI 至 RECORDING（不排 autoStop，等 0x23 精确修正）
     *      注意：srArmed 不在此处清除，保持为 true，使 0x23 仍能触发修正
     * ③ 0x23 通知固件已在精确边界触发 → 修正 segRecordStartTime，重排 autoStop
     *      等长录制：从修正后的精确时刻起计 capDur 后切换 UI 至 PLAYING
     */
    private void waitForRefBoundaryThenRecord(int idx, int refIdx) {
        // ① 参考段裁剪后总时长
        long loopTotal = getEffectiveLoopDurationMs(refIdx);
        if (loopTotal <= 0) {
            Toast.makeText(this, "LOOP " + (refIdx + 1) + " 时长未知，直接开始", Toast.LENGTH_SHORT).show();
            doStartRecordingAfterSync(idx);
            return;
        }
        // ② 当前播放位置（本轮循环起点 → 现在走了多少 ms）
        long loopPos = getLoopPositionMs(refIdx);
        // ③ 倒数时间 = 总时长 - 当前位置
        long remaining = (loopPos >= 0) ? (loopTotal - loopPos) : loopTotal;
        if (remaining <= 0) remaining = loopTotal; // 正好在边界时等一整圈
        final long capDur = loopTotal;
        final long targetMs = System.currentTimeMillis() + remaining;

        // UI：进入同步等待状态，展示倒计时面板
        segInCountdown[idx] = true;
        segInSyncWait[idx]  = true;
        showSyncInfoPanel(idx, refIdx, loopTotal, targetMs);

        if (metroOnDuringRec) { sendCommand("metro on", null); refreshTopInfoBar(); }

        // 告知固件：refIdx 回绕时开始录制；"match" 让固件在 refIdx 再次回绕时精确停止
        srRefSegIdx = refIdx;
        srArmed = true;
        sendCommandWithCallback(
                "looper -SR " + refIdx + " " + idx + (segMatchEnabled[idx] ? " match" : ""),
                success -> {
                    if (!success) {
                        srArmed = false;
                        srRefSegIdx = -1;
                        if (srPredictRunnable != null) {
                            handler.removeCallbacks(srPredictRunnable);
                            srPredictRunnable = null;
                        }
                        segInCountdown[idx] = false;
                        segInSyncWait[idx]  = false;
                        hideSyncInfoPanel();
                        updateControlButtonStates();
                    }
                }
        );

        // 预测时间轴：与 chain/join 完全一致的模式
        // —— predictRunnable 先到：清 srArmed，完整切 RECORDING + 排 autoStop；0x23 到 prevSrArmed=false 就跳过
        // —— 0x23 先到：取消 predictRunnable，0x23 自己完整切 RECORDING + 排 autoStop
        if (srPredictRunnable != null) handler.removeCallbacks(srPredictRunnable);
        final int capturedRefIdx = refIdx;
        srPredictRunnable = () -> {
            srPredictRunnable = null;
            if (!srArmed) return;  // 0x23 已先到达并完成处理
            srArmed = false;       // 清除标志，使 0x23 到达时跳过（prevSrArmed=false）
            long nowMs = System.currentTimeMillis();
            // 时间基准重定义：参考段刚刚回绕到位置 0，重置播放起点
            segPlayStartTime[capturedRefIdx] = nowMs;
            applySrRecordingState(idx, nowMs, capDur);
        };
        handler.postDelayed(srPredictRunnable, remaining);

        Toast.makeText(this, "同步录制已激活，等待 LOOP " + (refIdx + 1) + " 当前轮结束", Toast.LENGTH_SHORT).show();
        updateControlButtonStates();
    }

    /**
     * 将 idx 段切换至 RECORDING 状态，并按需排定等长自动停止。
     * 由 srPredictRunnable 和 0x23 handler 共用（与 applyChainPredictive / applyJoinPredictive 对称）。
     */
    private void applySrRecordingState(int idx, long startMs, long capDur) {
        segInCountdown[idx]     = false;
        segInSyncWait[idx]      = false;
        cancelAutoStop(idx);
        segStates[idx]          = SegState.RECORDING;
        segRecordStartTime[idx] = startMs;
        hideSyncInfoPanel();
        refreshSegUI(idx);
        updateControlButtonStates();

        // 同步录制时也启动存储空间实时更新
        startStorageUpdateForRecording(idx);

        // ① 等长录制：从 startMs 起 capDur 后切 PLAYING
        if (segMatchEnabled[idx] && capDur > 0) {
            long delay = Math.max(0L, (startMs + capDur) - System.currentTimeMillis());
            showMatchDurationCountdown(idx, startMs + capDur);
            autoStopRunnables[idx] = () -> {
                autoStopRunnables[idx] = null;
                if (segStates[idx] != SegState.RECORDING) return;
                // 固件已在边界精确停止并切换 PLAYING → App 仅同步 UI，不发 BLE 命令
                segStates[idx] = SegState.PLAYING;
                onSegmentEnteredPlaying(idx, true);
                hideSyncInfoPanel();
                refreshSegUI(idx);
                refreshSegCfgHint(idx);
                updateControlButtonStates();
                // 固件已切 PLAYING，BLE 队列空闲，可直接发送裁剪命令
                applyPreCrop(idx);
            };
            handler.postDelayed(autoStopRunnables[idx], delay);
        }
    }

    /** 在按钮下方信息面板显示 LOOP 同步信息（每 100ms 刷新） */
    private void showSyncInfoPanel(int idx, int refIdx, long loopDuration, long targetMs) {
        if (!segInSyncWait[idx]) return;
        long remaining = targetMs - System.currentTimeMillis();
        if (remaining < 0) remaining = 0;
        long secs   = remaining / 1000;
        long tenths = (remaining % 1000) / 100;

        tvLoopSyncTitle.setText("🔁 跟随录制 — LOOP " + (idx + 1) + " 等待同步");
        tvLoopSyncInfo.setText(
                "LOOP " + (refIdx + 1) + " 循环时长: " + String.format("%.2f", loopDuration / 1000.0) + "s" +
                        "\n⏳ 距下一循环头: " + secs + "." + tenths + "s\n" +
                        "点击 LOOP " + (idx + 1) + " 卡片可取消等待"
        );
        layoutLoopSyncInfo.setVisibility(View.VISIBLE);
        // 大数字框显示剩余秒数
        tvCountdownBig.setText(String.valueOf(secs));
        tvCountdownBigLabel.setText("s");
        layoutCountdownBox.setVisibility(View.VISIBLE);

        if (remaining > 80) {
            final int finalIdx = idx;
            final int finalRefIdx = refIdx;
            handler.postDelayed(() -> showSyncInfoPanel(finalIdx, finalRefIdx, loopDuration, targetMs), 100);
        }
    }

    /** 隐藏同步信息面板 */
    private void hideSyncInfoPanel() {
        if (layoutLoopSyncInfo != null) {
            layoutLoopSyncInfo.setVisibility(View.GONE);
        }
        updateControlButtonStates();
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
            // 取消固件侧同步录制
            if (srPredictRunnable != null) {
                handler.removeCallbacks(srPredictRunnable);
                srPredictRunnable = null;
            }
            if (srArmed) {
                srArmed = false;
                sendCommand("looper -SR cancel", null);
            }
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
            // 大数字框显示剩余拍数
            tvCountdownBig.setText(String.valueOf(remaining));
            tvCountdownBigLabel.setText("拍");
            layoutCountdownBox.setVisibility(View.VISIBLE);
            final int r = remaining;
            handler.postDelayed(() -> showCountdownHint(idx, r - 1, msPerBeat), msPerBeat);
        } else {
            // 倒计时结束瞬间短暂提示"开始!"
            if (segStates[idx] == SegState.INACTIVE && segInCountdown[idx]) {
                tvLoopSyncInfo.setText("🎵 开始录制！");
                tvCountdownBig.setText("GO");
                tvCountdownBigLabel.setText("");
            }
        }
    }

    /** 计算持续时长并启动自动停止定时器
     * segMeasures[idx] > 0 : 按小节数定时
     * segMeasures[idx] = 0 : 以 segMaxRecSec 为安全上限（防 Flash 溢出）
     */
    private void scheduleAutoStop(int idx) {
        long msPerBeat    = (long)(60000.0 / currentBpm);
        long msPerMeasure = msPerBeat * currentBeats;
        long totalMs;
        if (segMeasures[idx] > 0) {
            totalMs = msPerMeasure * segMeasures[idx];
            scheduleCountdownHint(idx, segMeasures[idx], msPerMeasure);
        } else {
            // 小节数为 0 时用最大录制时长作为兜底安全停止，防止 Flash 溢出
            totalMs = segMaxRecSec[idx] * 1000L;
        }
        autoStopRunnables[idx] = () -> autoStopSegment(idx);
        handler.postDelayed(autoStopRunnables[idx], totalMs);
    }

    /** 在卡片上展示录制倒计时提示（运行在 UI 线） */
    private void scheduleCountdownHint(int idx, int remaining, long msPerMeasure) {
        if (remaining <= 0 || segStates[idx] != SegState.RECORDING) return;
        final int r = remaining;
        tvSegHint[idx].setText("录制中... " + r + " 小节");
        tvSegHint[idx].setTextColor(clr(R.color.text_error));
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
            // 停止录制并自动播放（乐观更新）
            segStates[idx] = SegState.PLAYING;
            onSegmentEnteredPlaying(idx, true);
            refreshSegUI(idx);
            refreshSegCfgHint(idx);
            updateControlButtonStates();
            // 停录命令 + 裁剪命令拼在一起发送
            String preCropCmd = buildPreCropUpdate(idx);
            if (!preCropCmd.isEmpty()) upsertAudioTrackCard(idx); // 用裁剪后的参数刷新音频轨卡片
            String stopCmd = "looper -p " + idx + (preCropCmd.isEmpty() ? "" : "\r\n" + preCropCmd);
            sendCommandWithCallback(stopCmd, success -> {
                if (!success) {
                    segStates[idx] = SegState.RECORDING;
                    refreshSegUI(idx);
                    refreshSegCfgHint(idx);
                    updateControlButtonStates();
                }
            });
        } else {
            // 停止录制，不播放（乐观更新）
            segStates[idx] = SegState.STOPPED;
            segPlayStartTime[idx] = 0;

            // 停止录制时也停止存储空间的实时更新
            stopStorageUpdateForRecording(idx);

            refreshSegUI(idx);
            refreshSegCfgHint(idx);
            updateControlButtonStates();
            sendCommandWithCallback("looper -t " + idx, success -> {
                if (!success) {
                    segStates[idx] = SegState.RECORDING;
                    refreshSegUI(idx);
                    refreshSegCfgHint(idx);
                    updateControlButtonStates();
                }
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
     * 支持在 PLAYING/RECORDING 状态下删除（先停止再删除）
     */
    private void deleteSegment(int idx) {
        longPressRunnables[idx] = null;
        cancelAutoStop(idx);
        // 取消等待停止预测定时
        if (waitFinishPredictRunnables[idx] != null) {
            handler.removeCallbacks(waitFinishPredictRunnables[idx]);
            waitFinishPredictRunnables[idx] = null;
        }
        waitFinishEnabled[idx] = false;

        // 如果正在播放或录制，先发送停止命令
        if (segStates[idx] == SegState.PLAYING || segStates[idx] == SegState.RECORDING) {
            sendCommandWithCallback("looper -t " + idx, success -> {
                if (success) {
                    // 停止成功后执行实际删除操作
                    handler.postDelayed(() -> executeDeleteSegment(idx), 100);
                } else {
                    Toast.makeText(this, "停止失败，无法删除", Toast.LENGTH_SHORT).show();
                }
            });
        } else {
            executeDeleteSegment(idx);
        }
    }

    /**
     * 实际执行段删除的内部方法
     */
    private void executeDeleteSegment(int idx) {
        // 停止可能正在运行的存储空间更新任务
        stopStorageUpdateForRecording(idx);

        // 卡片闪烁反馈
        segCards[idx].animate().alpha(0.2f).setDuration(80)
                .withEndAction(() -> segCards[idx].animate().alpha(1f).setDuration(80).start())
                .start();
        // 乐观更新：立即清为 INACTIVE
        final SegState prevStateForDel = segStates[idx];
        segStates[idx] = SegState.INACTIVE;
        segLoopDurationMs[idx]  = 0;
        segRecordStartTime[idx] = 0;
        segPlayStartTime[idx]   = 0;
        segTrimStartPage[idx]   = 0;
        segTrimEndPage[idx]     = 0;
        removeAudioTrackCard(idx);
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
        updateControlButtonStates();
        Toast.makeText(this, "LOOP " + (idx + 1) + " 录音已删除", Toast.LENGTH_SHORT).show();
        sendCommandWithCallback("looper -c " + idx, success -> {
            if (!success) {
                // 回滚
                segStates[idx] = prevStateForDel;
                refreshSegUI(idx);
                refreshSegCfgHint(idx);
                updateControlButtonStates();
                Toast.makeText(this, "删除失败，已恢复", Toast.LENGTH_SHORT).show();
            }
        });
    }

    // ========================================================================
    // 段独立按钮：跟随录制 / 录制等长
    // ========================================================================

    /** 跟随录制按钮点击：等待先录段循环头开始录制 */
    private void onFollowButtonClick(int idx) {
        segFollowEnabled[idx] = !segFollowEnabled[idx];
        updateControlButtonStates();
        Toast.makeText(this,
                "LOOP " + (idx + 1) + " 跟随录制：" + (segFollowEnabled[idx] ? "开" : "关"),
                Toast.LENGTH_SHORT).show();
    }

    /** 录制等长按钮点击：与先录段长度一致自动停止 */
    private void onMatchButtonClick(int idx) {
        segMatchEnabled[idx] = !segMatchEnabled[idx];
        updateControlButtonStates();
        Toast.makeText(this,
                "LOOP " + (idx + 1) + " 录制等长：" + (segMatchEnabled[idx] ? "开" : "关"),
                Toast.LENGTH_SHORT).show();
    }

    /** 参照段结束后录制按钮点击：参照段结束后开始录制 */
    private void onRecordAfterRefClick(int idx) {
        if (!checkConnection()) return;
        if (segStates[idx] != SegState.INACTIVE) {
            Toast.makeText(this, "只能在未录制时使用", Toast.LENGTH_SHORT).show();
            return;
        }
        int refIdx = getRefPlayingSegIdx(idx);
        if (refIdx < 0) {
            Toast.makeText(this, "需要有参照段在播放", Toast.LENGTH_SHORT).show();
            return;
        }
        waitForRefBoundaryThenRecord(idx, refIdx);
    }

    /** 衔接播放按钮点击：参照段播完后停止参照段，当前段开始播放 */
    private void onChainClick(int idx) {
        if (!checkConnection()) return;
        int refIdx = referenceSegIndex;
        if (refIdx < 0 || segStates[refIdx] != SegState.PLAYING) {
            Toast.makeText(this, "需要有参照段在播放", Toast.LENGTH_SHORT).show();
            return;
        }
        if (segStates[idx] != SegState.STOPPED) {
            Toast.makeText(this, "该段需要在已停止状态", Toast.LENGTH_SHORT).show();
            return;
        }
        // 发送衔接命令
        sendCommand("looper -C " + refIdx + " " + idx, null);
        Toast.makeText(this, "衔接已激活，等待参照段结束", Toast.LENGTH_SHORT).show();
    }

    /** 接入播放按钮点击：参照段播完后当前段开始播放 */
    private void onJoinClick(int idx) {
        if (!checkConnection()) return;
        int refIdx = referenceSegIndex;
        if (refIdx < 0 || segStates[refIdx] != SegState.PLAYING) {
            Toast.makeText(this, "需要有参照段在播放", Toast.LENGTH_SHORT).show();
            return;
        }
        if (segStates[idx] != SegState.STOPPED) {
            Toast.makeText(this, "该段需要在已停止状态", Toast.LENGTH_SHORT).show();
            return;
        }
        // 发送接入命令
        sendCommand("looper -J " + idx, null);
        Toast.makeText(this, "接入已激活，等待参照段结束", Toast.LENGTH_SHORT).show();
    }

    // ========================================================================
    // 全局控制按钮：衔接 / 接入 / 全部播放 / 全部停止
    // ========================================================================

    /** 全部删除：删除所有已录制的段 */
    private void onDeleteAllClick() {
        if (!checkConnection()) return;

        // 检查是否有任何已录制的段
        boolean anyRecorded = false;
        for (int i = 0; i < SEG_COUNT; i++) {
            if (segStates[i] != SegState.INACTIVE) {
                anyRecorded = true;
                break;
            }
        }
        if (!anyRecorded) return;

        new AlertDialog.Builder(this)
                .setTitle("🗑 全部删除")
                .setMessage("确认删除所有已录制的段？")
                .setPositiveButton("确认删除", (d, w) -> {
                    // 先停止所有正在播放/录制的段
                    StringBuilder stopCmds = new StringBuilder();
                    for (int i = 0; i < SEG_COUNT; i++) {
                        if (segStates[i] == SegState.PLAYING || segStates[i] == SegState.RECORDING) {
                            stopCmds.append("looper -t ").append(i).append("\r\n");
                        }
                    }
                    if (stopCmds.length() > 0) {
                        sendCommand(stopCmds.toString().trim(), null);
                    }

                    // 延迟后执行删除
                    handler.postDelayed(() -> {
                        for (int i = 0; i < SEG_COUNT; i++) {
                            if (segStates[i] != SegState.INACTIVE) {
                                // 清理定时器等
                                cancelAutoStop(i);
                                cancelCountdown(i);
                                stopStorageUpdateForRecording(i);
                                if (waitFinishPredictRunnables[i] != null) {
                                    handler.removeCallbacks(waitFinishPredictRunnables[i]);
                                    waitFinishPredictRunnables[i] = null;
                                }
                                waitFinishEnabled[i] = false;

                                // 重置状态
                                segStates[i] = SegState.INACTIVE;
                                segLoopDurationMs[i] = 0;
                                segRecordStartTime[i] = 0;
                                segPlayStartTime[i] = 0;
                                segTrimStartPage[i] = 0;
                                segTrimEndPage[i] = 0;

                                // 刷新UI
                                removeAudioTrackCard(i);
                                refreshSegUI(i);
                                refreshSegCfgHint(i);
                            }
                        }

                        updateControlButtonStates();
                        updateStorageDisplay();
                        refreshTopInfoBar();

                        // 发送删除命令
                        StringBuilder deleteCmds = new StringBuilder();
                        for (int i = 0; i < SEG_COUNT; i++) {
                            deleteCmds.append("looper -c ").append(i).append("\r\n");
                        }
                        sendCommand(deleteCmds.toString().trim(), null);

                        Toast.makeText(this, "所有段已删除", Toast.LENGTH_SHORT).show();
                    }, 100);
                })
                .setNegativeButton("取消", null)
                .show();
    }

    /** 全部同时播放（仅对 STOPPED 的段；乐观更新） */
    private void onPlayAllClick() {
        if (!checkConnection()) return;

        // 乐观更新：先将所有 STOPPED 段改为 PLAYING
        StringBuilder cmds = new StringBuilder();
        for (int i = 0; i < SEG_COUNT; i++) {
            if (segStates[i] == SegState.STOPPED) {
                segStates[i] = SegState.PLAYING;
                onSegmentEnteredPlaying(i, false);
                refreshSegUI(i);
                cmds.append("looper -p ").append(i).append("\r\n");
            }
        }
        updateControlButtonStates();

        // 一次性发送所有播放命令
        if (cmds.length() > 0) {
            sendCommand(cmds.toString().trim(), null);
        }
    }

    /** 全部同时停止（带 wait-finish 感知：wait-finish 段保持 PLAYING 直到预测回绕点切换） */
    private void onStopAllClick() {
        if (!checkConnection()) return;
        StringBuilder cmds = new StringBuilder();
        for (int i = 0; i < SEG_COUNT; i++) {
            if (segStates[i] == SegState.PLAYING || segStates[i] == SegState.RECORDING) {
                cancelAutoStop(i);
                if (segStates[i] == SegState.RECORDING || !waitFinishEnabled[i]) {
                    // RECORDING 或 wait-finish 未启用：立即更新 STOPPED（乐观）
                    segStates[i] = SegState.STOPPED;
                    segPlayStartTime[i] = 0;
                    refreshSegUI(i);
                } else {
                    // PLAYING + wait-finish 启用：保持 PLAYING，安排预测定时
                    final int fi = i;
                    long rem = msUntilNextWrap(i);
                    if (rem > 0) {
                        if (waitFinishPredictRunnables[fi] != null)
                            handler.removeCallbacks(waitFinishPredictRunnables[fi]);
                        waitFinishPredictRunnables[fi] = () -> {
                            waitFinishPredictRunnables[fi] = null;
                            if (segStates[fi] == SegState.PLAYING) {
                                segStates[fi] = SegState.STOPPED;
                                segPlayStartTime[fi] = 0;
                                refreshSegUI(fi);
                                updateControlButtonStates();
                            }
                        };
                        handler.postDelayed(waitFinishPredictRunnables[fi], rem);
                    }
                }
                cmds.append("looper -t ").append(i).append("\r\n");
            }
        }
        updateControlButtonStates();
        if (cmds.length() > 0) {
            sendCommand(cmds.toString().trim(), null);
        }
    }

    /** 安全停止指定段（仍在 PLAYING 才发命令；wait-finish 激活时保持 PLAYING 直到预测回绕点） */
    private void stopSegIfPlaying(int idx) {
        if (segStates[idx] != SegState.PLAYING) return;
        if (waitFinishEnabled[idx]) {
            // 等待本轮播完：保持 PLAYING 显示，在预测回绕时间切换到 STOPPED
            long rem = msUntilNextWrap(idx);
            if (rem > 0) {
                if (waitFinishPredictRunnables[idx] != null)
                    handler.removeCallbacks(waitFinishPredictRunnables[idx]);
                waitFinishPredictRunnables[idx] = () -> {
                    waitFinishPredictRunnables[idx] = null;
                    if (segStates[idx] == SegState.PLAYING) {
                        segStates[idx] = SegState.STOPPED;
                        segPlayStartTime[idx] = 0;
                        refreshSegUI(idx);
                        updateControlButtonStates();
                    }
                };
                handler.postDelayed(waitFinishPredictRunnables[idx], rem);
            }
            sendCommandWithCallback("looper -t " + idx, success -> {
                if (!success && waitFinishPredictRunnables[idx] != null) {
                    handler.removeCallbacks(waitFinishPredictRunnables[idx]);
                    waitFinishPredictRunnables[idx] = null;
                }
            });
        } else {
            final long prevPST = segPlayStartTime[idx];
            segStates[idx] = SegState.STOPPED;
            segPlayStartTime[idx] = 0;
            refreshSegUI(idx);
            sendCommandWithCallback("looper -t " + idx, success -> {
                if (!success) {
                    segStates[idx] = SegState.PLAYING;
                    segPlayStartTime[idx] = prevPST; // 回滚恢复播放位置基准
                    refreshSegUI(idx);
                }
            });
        }
    }

    /** 切换指定段的「等待本轮播完再停止」开关（乐观更新，通知下位机） */
    private void onWaitFinishToggle(int idx) {
        if (!checkConnection()) return;
        // 乐观更新（0x23 通知包会再次校验）
        waitFinishEnabled[idx] = !waitFinishEnabled[idx];
        updateControlButtonStates();
        Toast.makeText(this,
                "LOOP " + (idx+1) + " 等待播完再停止：" + (waitFinishEnabled[idx] ? "开" : "关"),
                Toast.LENGTH_SHORT).show();
        sendCommandWithCallback("looper -W " + idx, success -> {
            if (!success) {
                // 回滚
                waitFinishEnabled[idx] = !waitFinishEnabled[idx];
                updateControlButtonStates();
            }
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

        // 每次段状态改变时同步刷新所有控制按钮和存储空间显示
        updateControlButtonStates();
        updateStorageDisplay();
        refreshTopInfoBar();
    }

    // ========================================================================
    // 录制空间占用显示
    // ========================================================================

    /**
     * 更新存储空间占用显示
     * 计算所有段的已用空间，更新进度条和文本
     */
    private void updateStorageDisplay() {
        if (progressStorage == null) return;  // 控件尚未初始化

        long totalUsedBytes = 0;

        for (int i = 0; i < SEG_COUNT; i++) {
            long segBytes = 0;
            String segText = "L" + (i+1) + ": ";

            if (segStates[i] != SegState.INACTIVE && segLoopDurationMs[i] > 0) {
                // 计算该段已用字节数 = 录制时长(ms) × 每毫秒字节数
                segBytes = (long)(segLoopDurationMs[i] * (AUDIO_BYTES_PER_SEC / 1000.0));
                float segSec = segLoopDurationMs[i] / 1000.0f;
                segText += String.format("%.1fs", segSec);
            } else if (segStates[i] == SegState.RECORDING) {
                // 正在录制中：显示实时时长（使用当前时间戳）
                long now = System.currentTimeMillis();
                long recMs = now - segRecordStartTime[i];
                segBytes = (long)(recMs * (AUDIO_BYTES_PER_SEC / 1000.0));
                float recSec = recMs / 1000.0f;
                segText += String.format("⏺%.1fs", recSec);
            } else {
                segText += "--";
            }

            totalUsedBytes += segBytes;

            // 更新各段明细文本
            if (tvSegStorage[i] != null) {
                tvSegStorage[i].setText(segText);
                // 根据段状态设置颜色
                if (segStates[i] == SegState.RECORDING) {
                    tvSegStorage[i].setTextColor(clr(R.color.color_recording));
                } else if (segBytes > 0) {
                    tvSegStorage[i].setTextColor(clr(R.color.text_secondary));
                } else {
                    tvSegStorage[i].setTextColor(clr(R.color.text_tertiary));
                }
            }
        }

        // 计算总使用量和剩余量
        float usedMB = totalUsedBytes / (1024.0f * 1024.0f);
        float totalMB = TOTAL_STORAGE_BYTES / (1024.0f * 1024.0f);
        float remainMB = totalMB - usedMB;
        float usedPercent = (totalMB > 0) ? (usedMB / totalMB * 100.0f) : 0f;

        // 计算剩余录制时间（秒）
        float remainSec = remainMB * 1024.0f * 1024.0f / AUDIO_BYTES_PER_SEC;

        // 更新主显示文本
        if (tvStorageUsed != null) {
            tvStorageUsed.setText(String.format("%.1f%%", usedPercent));

            // 根据占用率设置颜色警告
            if (usedPercent >= 90) {
                tvStorageUsed.setTextColor(0xFFFF5722);  // 深橙红
            } else if (usedPercent >= 70) {
                tvStorageUsed.setTextColor(0xFFFFC107);  // 琥珀色
            } else {
                tvStorageUsed.setTextColor(clr(R.color.primary_accent));  // 蓝色
            }
        }

        if (tvStorageTime != null) {
            if (remainSec > 0) {
                tvStorageTime.setText(String.format("(剩余 %.1fs)", remainSec));
                tvStorageTime.setTextColor(clr(R.color.text_secondary));
            } else {
                tvStorageTime.setText("(已满)");
                tvStorageTime.setTextColor(clr(R.color.text_error));
            }
        }

        // 更新进度条（千分比）
        int progressValue = (int)(usedPercent * 10);  // 转换为0-1000
        progressStorage.setProgress(Math.min(progressValue, PROGRESS_MAX));
    }

    /**
     * 统一刷新所有控制按钮的三态（失能/使能/激活）。
     * 互斥规则：
     *   btnSegFollow[i]/btnSegMatch[i]: 有参照段播放时，未录制的段可跟随
     *   btnSegWaitFinish[i]: 对应段 PLAYING 或 STOPPED 时使能
     *   btnChainPlay/btnJoinPlay: 有参照段播放，且存在已录好的停止段
     *   btnPlayAll: 有已录好的段，且至少有一段 STOPPED
     *   btnStopAll: 有已录好的段，且至少有一段 PLAYING
     */
    private void updateControlButtonStates() {
        if (btnPlayAll == null) return; // 控件尚未绑定

        // 检查所有段的状态
        boolean anyDone = false;
        boolean hasPlaying = false;
        boolean hasStopped = false;
        int playingCount = 0;
        int stoppedCount = 0;
        int stoppedSegIdx = -1;

        for (int i = 0; i < SEG_COUNT; i++) {
            boolean isDone = (segStates[i] == SegState.PLAYING || segStates[i] == SegState.STOPPED);
            if (isDone) {
                anyDone = true;
                if (segStates[i] == SegState.PLAYING) {
                    hasPlaying = true;
                    playingCount++;
                } else if (segStates[i] == SegState.STOPPED) {
                    hasStopped = true;
                    stoppedCount++;
                    if (stoppedSegIdx == -1) stoppedSegIdx = i; // 记录第一个停止的段
                }
            }
        }

        // ---------- 段独立按钮（跟随/等长）：任意先录段播放时，未录制的段可跟随 ----------
        for (int i = 0; i < SEG_COUNT; i++) {
            boolean canFollow = (getRefPlayingSegIdx(i) >= 0 && segStates[i] == SegState.INACTIVE);
            applyButtonState(btnSegFollow[i], canFollow, canFollow && segFollowEnabled[i], 0xFF22A855);
            applyButtonState(btnSegMatch[i],  canFollow, canFollow && segMatchEnabled[i],  0xFF0097A7);
            applyButtonState(btnSegRecordAfterRef[i], canFollow, false, 0xFF9C27B0);
        }

        // ---------- 段独立按钮（衔接/接入）：有参照段播放时，停止段可操作 ----------
        boolean hasRefPlaying = (referenceSegIndex >= 0 && segStates[referenceSegIndex] == SegState.PLAYING);
        for (int i = 0; i < SEG_COUNT; i++) {
            boolean canChainJoin = hasRefPlaying && segStates[i] == SegState.STOPPED;
            applyButtonState(btnSegChain[i], canChainJoin, false, 0xFF1D6FDB);
            applyButtonState(btnSegJoin[i], canChainJoin, false, 0xFF9B59B6);
        }

        // ---------- 每段独立：等待本轮播完再停止 ----------
        for (int i = 0; i < SEG_COUNT; i++) {
            boolean segPlayable = (segStates[i] == SegState.PLAYING || segStates[i] == SegState.STOPPED);
            applyButtonState(btnSegWaitFinish[i], segPlayable, waitFinishEnabled[i], 0xFFE08B0A);
        }

        // ---------- 全局按钮 ----------
        // 全部播放：有已录好的段，且至少有一段 STOPPED
        boolean canPlayAll = anyDone && hasStopped;
        applyButtonState(btnPlayAll, canPlayAll, false, 0xFF1D6FDB);

        // 全部删除：有已录好的段
        boolean canDeleteAll = anyDone;
        applyButtonState(btnDeleteAll, canDeleteAll, false, 0xFFCC3333);

        // 全部停止：有已录好的段，且至少有一段 PLAYING
        boolean canStopAll = anyDone && hasPlaying;
        applyButtonState(btnStopAll, canStopAll, false, 0xFFCC3333);
    }

    /**
     * 将一个控件设置为三态之一（适配 Button 和 ImageButton）：
     * @param btn         目标控件
     * @param enabled     是否可点击使能
     * @param active      是否处于「激活/按下」态（高亮）
     * @param activeColor 激活态背景/图标着色（ARGB）
     */
    private void applyButtonState(View btn, boolean enabled, boolean active, int activeColor) {
        if (btn == null) return;
        btn.setEnabled(enabled);
        if (!enabled) {
            btn.setAlpha(0.35f);
            btn.setBackgroundTintList(ColorStateList.valueOf(clr(R.color.dialog_btn_bg)));
            if (btn instanceof ImageButton) {
                ((ImageButton) btn).setImageTintList(
                        ColorStateList.valueOf(clr(R.color.text_tertiary)));
            }
        } else if (active) {
            btn.setAlpha(1.0f);
            btn.setBackgroundTintList(ColorStateList.valueOf(activeColor));
            if (btn instanceof ImageButton) {
                ((ImageButton) btn).setImageTintList(
                        ColorStateList.valueOf(Color.WHITE));
            }
        } else {
            // 使能未激活：浅蓝色背景，明显区别于失能态
            btn.setAlpha(1.0f);
            btn.setBackgroundTintList(ColorStateList.valueOf(clr(R.color.global_btn_active_bg)));
            if (btn instanceof ImageButton) {
                ((ImageButton) btn).setImageTintList(
                        ColorStateList.valueOf(clr(R.color.global_btn_active_text)));
            }
        }
    }

    /** @deprecated 旧签名兼容重载（仅内部使用） */
    @Deprecated
    private void applyButtonState(View btn, boolean enabled, boolean active) {
        applyButtonState(btn, enabled, active, 0xFF1D6FDB);
    }

    /** BLE 断开时更新 App UI：将 PLAYING/RECORDING 段置为 STOPPED（固件已停止播放） */
    private void onBleDisconnected() {
        // 取消所有预测定时器
        if (srPredictRunnable    != null) { handler.removeCallbacks(srPredictRunnable);    srPredictRunnable    = null; }
        srArmed = false;
        for (int i = 0; i < SEG_COUNT; i++) {
            if (waitFinishPredictRunnables[i] != null) {
                handler.removeCallbacks(waitFinishPredictRunnables[i]);
                waitFinishPredictRunnables[i] = null;
            }
        }
        for (int i = 0; i < SEG_COUNT; i++) {
            cancelAutoStop(i);
            if (segStates[i] == SegState.PLAYING || segStates[i] == SegState.RECORDING) {
                segStates[i] = SegState.STOPPED;
                refreshSegUI(i);
            }
        }

        // BLE 断开 → looper 设置恢复默认
        for (int i = 0; i < SEG_COUNT; i++) {
            segStates[i]          = SegState.INACTIVE;
            segLoopDurationMs[i]  = 0;
            segRecordStartTime[i] = 0;
            segPlayStartTime[i]   = 0;
            segTrimStartPage[i]   = 0;
            segTrimEndPage[i]     = 0;
            segInCountdown[i]     = false;
            segInSyncWait[i]      = false;
            refreshSegUI(i);
            removeAudioTrackCard(i);
        }
        for (int i = 0; i < SEG_COUNT; i++) { segFollowEnabled[i] = false; segMatchEnabled[i] = false; }
        srRefSegIdx = -1;
        hideSyncInfoPanel();

        updateControlButtonStates();
        updateHwStatusIndicator();
    }

    private void setupBleListener() {
        final StringBuilder accum = new StringBuilder();
        bluetoothHelper.setOnConnectionChangedListener(new BluetoothHelper.OnConnectionChangedListener() {
            @Override public void onConnected(String deviceName, android.bluetooth.BluetoothGatt gatt) {
                runOnUiThread(() -> updateHwStatusIndicator());
            }
            @Override public void onDisconnected() {
                runOnUiThread(() -> onBleDisconnected());
            }
        });
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

                // ── 二进制包路径：AA 55 22 09 ... (looper trim params, type=0x22) ──
                if (upper.startsWith("AA5522") && upper.length() >= 26) {
                    parseLooperTrimBinary(upper);
                    accum.setLength(0);
                    return;
                }

                // ── 二进制包路径：AA 55 23 01 ... (定时操作状态通知, type=0x23) ──
                if (upper.startsWith("AA5523") && upper.length() >= 10) {
                    parseLooperTimedOpsBinary(upper);
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
                if (parsed == SegState.STOPPED || parsed == SegState.INACTIVE) {
                    segPlayStartTime[i] = 0; // 停止/无效时清除播放位置基准
                } else if (parsed == SegState.PLAYING && segPlayStartTime[i] == 0) {
                    segPlayStartTime[i] = System.currentTimeMillis();
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
            updateControlButtonStates();
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
            if (parsed == SegState.STOPPED || parsed == SegState.INACTIVE) {
                segPlayStartTime[segIdx] = 0;
            } else if (parsed == SegState.PLAYING && segPlayStartTime[segIdx] == 0) {
                segPlayStartTime[segIdx] = System.currentTimeMillis();
            }
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
        updateControlButtonStates();
    }

    /**
     * 解析下位机定时操作状态通知包（type=0x23）
     * 格式 (hex 字符串, 大写无空格):
     *   AA5523 01 [timed_ops_state]
     * 偏移 (chars): 0  2  4  6   8
     * timed_ops_state 位定义:
     *   bit0 = chain_armed (衔接等待中)
     *   bit1 = join_armed  (接入等待中)
     *   bit2 = wait_finish[0]
     *   bit3 = wait_finish[1]
     *   bit4 = deferred_stop[0]
     *   bit5 = deferred_stop[1]
     * 通知时机：下位机每次执行完定时操作后推送
     */
    private void parseLooperTimedOpsBinary(String hex) {
        try {
            if (hex.length() < 10) return;
            int state = Integer.parseInt(hex.substring(8, 10), 16);

            boolean prevWF0         = waitFinishEnabled[0];
            boolean prevWF1         = waitFinishEnabled[1];
            boolean prevSrArmed     = srArmed;

            waitFinishEnabled[0]  = (state & 0x04) != 0;
            waitFinishEnabled[1]  = (state & 0x08) != 0;
            srArmed               = (state & 0x40) != 0; // bit6: SR 激活中

            // ── wait-finish 刚执行完（固件已清 wait_finish_mask）→ 段停止 ──
            for (int i = 0; i < Math.min(SEG_COUNT, 2); i++) {
                boolean prevWF = (i == 0) ? prevWF0 : prevWF1;
                if (prevWF && !waitFinishEnabled[i]) {
                    // 取消预测定时（若 0x23 先到），确认段已停止
                    if (waitFinishPredictRunnables[i] != null) {
                        handler.removeCallbacks(waitFinishPredictRunnables[i]);
                        waitFinishPredictRunnables[i] = null;
                    }
                    if (segStates[i] == SegState.PLAYING) {
                        segStates[i] = SegState.STOPPED;
                        segPlayStartTime[i] = 0;
                        refreshSegUI(i);
                    }
                }
            }

            // ── 同步录制刚触发完（was armed, now cleared）→ 以 0x23 确认时刻修正 segRecordStartTime 并排 autoStop ──
            // ── 同步录制固件已触发（armed → cleared）→ 以 0x23 到达时刻精确修正 segRecordStartTime 并安排 autoStop ──
            // ── 同步录制固件已触发（armed → cleared）→ 与 chain/join 对称处理 ──
            if (prevSrArmed && !srArmed) {
                if (srPredictRunnable != null) {
                    // 0x23 先于预测定时到达：取消预测，由 0x23 完整处理
                    handler.removeCallbacks(srPredictRunnable);
                    srPredictRunnable = null;
                    long nowMs = System.currentTimeMillis();
                    // 时间基准重定义：固件确认参照段刚刚回绕到位置 0
                    if (srRefSegIdx >= 0) {
                        segPlayStartTime[srRefSegIdx] = nowMs;
                    }
                    for (int i = 0; i < SEG_COUNT; i++) {
                        if (segInSyncWait[i]) {
                            long refDur = (srRefSegIdx >= 0) ? getEffectiveLoopDurationMs(srRefSegIdx) : 0;
                            applySrRecordingState(i, nowMs, refDur);
                            break;
                        }
                    }
                }
                // else: srPredictRunnable 已先触发并清除了 srArmed，状态已完整更新，无需重复处理
            }

            updateControlButtonStates();
        } catch (NumberFormatException e) {
            // 包不完整，忽略
        }
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
     * 解析 looper -T 返回的裁剪参数二进制包 (type=0x22)
     * 格式 (hex 字符串, 大写无空格):
     *   AA5522 09 [seg][start_lo][start_b1][start_b2][start_hi][end_lo][end_b1][end_b2][end_hi]
     * 偏移 (chars): 0  2  4  6   8   10  12  14  16  18  20  22  24
     */
    private void parseLooperTrimBinary(String hex) {
        try {
            if (hex.length() < 26) return;
            int seg       = Integer.parseInt(hex.substring(8, 10), 16);
            if (seg < 0 || seg >= SEG_COUNT) return;
            long startLo  = Long.parseLong(hex.substring(10, 12), 16);
            long startB1  = Long.parseLong(hex.substring(12, 14), 16);
            long startB2  = Long.parseLong(hex.substring(14, 16), 16);
            long startHi  = Long.parseLong(hex.substring(16, 18), 16);
            long endLo    = Long.parseLong(hex.substring(18, 20), 16);
            long endB1    = Long.parseLong(hex.substring(20, 22), 16);
            long endB2    = Long.parseLong(hex.substring(22, 24), 16);
            long endHi    = Long.parseLong(hex.substring(24, 26), 16);
            int startPage = (int)(startLo | (startB1 << 8) | (startB2 << 16) | (startHi << 24));
            int endPage   = (int)(endLo   | (endB1   << 8) | (endB2   << 16) | (endHi   << 24));
            segTrimStartPage[seg] = startPage;
            segTrimEndPage[seg]   = endPage;
            // 若当前会话内有录音时长信息，一并更新有效循环时长并重新锁定播放相位
            long rawMs = segLoopDurationMs[seg];
            if (rawMs > 0) {
                int rawPages = (int)(rawMs * PAGES_PER_SEC / 1000L);
                int effEnd   = (endPage > 0) ? endPage : rawPages;
                if (effEnd > startPage) {
                    long newDurMs = (long)(effEnd - startPage) * 1000L / PAGES_PER_SEC;
                    segLoopDurationMs[seg] = newDurMs;
                    // 重新锁定播放相位：就算 loopTotal 变了，% 操作也能得到正确的循环内位置
                    if (segStates[seg] == SegState.PLAYING
                            && newDurMs > 0 && segPlayStartTime[seg] > 0) {
                        long now2 = System.currentTimeMillis();
                        long el   = (now2 - segPlayStartTime[seg]) % newDurMs;
                        segPlayStartTime[seg] = now2 - el;
                    }
                }
            }
            // 更新音频轨面板（若已显示则刷新裁剪位置）
            updateAudioTrackTrimDisplay(seg);
        } catch (NumberFormatException e) {
            android.util.Log.e("Looper", "parseLooperTrimBinary error: " + hex, e);
        }
    }

    /**
     * 弹出段裁剪对话框：设置循环起止页（不修改Flash数据，仅影响播放范围）
     * 以页为单位；1页 = 192字节 ≈ 1ms（48kHz 32bit 双声道：192000B/s ÷ 192B/page ≈ 1000页/s)
     */
    private void showSegTrimDialog(int idx) {
        // 先查询当前裁剪点
        sendCommand("looper -T " + idx, null);

        // 页→毫秒换算：192000 B/s ÷ 192 B/page = 1000 pages/s
        final int PAGES_PER_SEC = 1000;

        // 用当前已知录制长度来换算 maxPages（如未知取段最大录制时长估算）
        // segLoopDurationMs 在录制结束时被记录，单位ms
        long durationMs = segLoopDurationMs[idx];
        final int maxPages = (durationMs > 0)
                ? (int)(durationMs * PAGES_PER_SEC / 1000L)
                : (int)((long)segMaxRecSec[idx] * PAGES_PER_SEC);

        final int[] tmpStart = { segTrimStartPage[idx] };
        final int[] tmpEnd   = { (segTrimEndPage[idx] > 0) ? segTrimEndPage[idx] : maxPages };

        android.widget.LinearLayout root = new android.widget.LinearLayout(this);
        root.setOrientation(android.widget.LinearLayout.VERTICAL);
        root.setPadding(48, 32, 48, 16);
        root.setBackgroundColor(clr(R.color.dialog_bg));

        // 标题行
        TextView title = new TextView(this);
        title.setText("LOOP " + (idx + 1) + "  裁剪范围");
        title.setTextColor(clr(R.color.text_accent));
        title.setTextSize(16);
        title.setTypeface(null, android.graphics.Typeface.BOLD);
        title.setPadding(0, 0, 0, 16);
        root.addView(title);

        // 说明文字
        TextView hint = new TextView(this);
        hint.setText("以「页」为单位（1000 页 ≈ 1 秒）。\n起点=0 表示从头；终点=0 或等于最大值表示到末尾。");
        hint.setTextColor(clr(R.color.dialog_hint_text));
        hint.setTextSize(11);
        hint.setPadding(0, 0, 0, 20);
        root.addView(hint);

        // ---- 起始页 SeekBar ----
        android.widget.LinearLayout blockStart = buildTrimSeekRow(
                "起始页（trim start）", tmpStart, 0, maxPages, PAGES_PER_SEC);
        root.addView(blockStart);

        // ---- 终止页 SeekBar ----
        android.widget.LinearLayout blockEnd = buildTrimSeekRow(
                "终止页（trim end）", tmpEnd, 0, maxPages, PAGES_PER_SEC);
        root.addView(blockEnd);

        new AlertDialog.Builder(this)
                .setTitle("裁剪 LOOP " + (idx + 1))
                .setView(root)
                .setPositiveButton("应用", (d, w) -> {
                    int startPage = tmpStart[0];
                    int endPage   = (tmpEnd[0] >= maxPages) ? 0 : tmpEnd[0]; // 等于最大则传0表示到末尾
                    if (startPage > 0 && endPage > 0 && startPage >= endPage) {
                        Toast.makeText(this, "起始页必须小于终止页", Toast.LENGTH_SHORT).show();
                        return;
                    }
                    segTrimStartPage[idx] = startPage;
                    segTrimEndPage[idx]   = endPage;
                    sendCommand("looper -T " + idx + " " + startPage + " " + endPage, null);

                    // 更新循环时长并重新锁定相位（无论是否开启等长）
                    int effectiveEnd   = (endPage > 0) ? endPage : maxPages;
                    long newDurMs      = (long)(effectiveEnd - startPage) * 1000L / PAGES_PER_SEC;
                    segLoopDurationMs[idx] = newDurMs;
                    if (segStates[idx] == SegState.PLAYING && newDurMs > 0 && segPlayStartTime[idx] > 0) {
                        long now2 = System.currentTimeMillis();
                        long el   = (now2 - segPlayStartTime[idx]) % newDurMs;
                        segPlayStartTime[idx] = now2 - el;
                    }

                    Toast.makeText(this,
                            "LOOP " + (idx + 1) + " 裁剪已应用: " +
                                    startPage + " → " + (endPage == 0 ? "末尾" : String.valueOf(endPage)),
                            Toast.LENGTH_SHORT).show();
                })
                .setNeutralButton("重置", (d, w) -> {
                    segTrimStartPage[idx] = 0;
                    segTrimEndPage[idx]   = 0;
                    sendCommand("looper -T " + idx + " 0 0", null);
                    // 裁剪重置时，用原始录音时长恢复循环时长，重新锁定相位
                    if (idx == 0) {
                        long origMs = segLoopDurationMs[0]; // 如果已是裁剪后的小值，用 rawMs 传参重算
                        // parseLooperTrimBinary 收到 0 0 回包后会用 rawPages 恢复。
                        // 这里先用 segLoopDurationMs 不变（回包后在 parseTrim 里恢复）
                    }
                    Toast.makeText(this, "LOOP " + (idx + 1) + " 裁剪已重置", Toast.LENGTH_SHORT).show();
                })
                .setNegativeButton("取消", null)
                .show();
    }

    /** 构建「标签 + 当前值文本 + SeekBar」的一行控件，供 showSegTrimDialog 复用 */
    private android.widget.LinearLayout buildTrimSeekRow(
            String label, int[] valueHolder, int min, int max, int pagesPerSec) {
        android.widget.LinearLayout block = new android.widget.LinearLayout(this);
        block.setOrientation(android.widget.LinearLayout.VERTICAL);
        block.setBackgroundColor(clr(R.color.dialog_section_bg));
        block.setPadding(24, 20, 24, 20);
        android.widget.LinearLayout.LayoutParams bp =
                new android.widget.LinearLayout.LayoutParams(
                        android.widget.LinearLayout.LayoutParams.MATCH_PARENT,
                        android.widget.LinearLayout.LayoutParams.WRAP_CONTENT);
        bp.setMargins(0, 0, 0, 8);
        block.setLayoutParams(bp);

        // 标题行：标签 + 数值
        android.widget.LinearLayout titleRow = new android.widget.LinearLayout(this);
        titleRow.setOrientation(android.widget.LinearLayout.HORIZONTAL);
        titleRow.setGravity(android.view.Gravity.CENTER_VERTICAL);

        TextView lbl = new TextView(this);
        lbl.setText(label);
        lbl.setTextColor(clr(R.color.text_accent));
        android.widget.LinearLayout.LayoutParams lblP =
                new android.widget.LinearLayout.LayoutParams(0,
                        android.widget.LinearLayout.LayoutParams.WRAP_CONTENT, 1f);
        lbl.setLayoutParams(lblP);

        final TextView tvVal = new TextView(this);
        int initSec10 = valueHolder[0] * 10 / pagesPerSec; // 精确到0.1s
        tvVal.setText(valueHolder[0] + " p (" + (initSec10 / 10) + "." + (initSec10 % 10) + "s)");
        tvVal.setTextColor(clr(R.color.text_success));
        tvVal.setTextSize(13);
        tvVal.setTypeface(null, android.graphics.Typeface.BOLD);
        tvVal.setGravity(android.view.Gravity.END);

        titleRow.addView(lbl);
        titleRow.addView(tvVal);
        block.addView(titleRow);

        // SeekBar
        final SeekBar sb = new SeekBar(this);
        sb.setMax(max - min);
        sb.setProgress(valueHolder[0] - min);
        android.widget.LinearLayout.LayoutParams sbP =
                new android.widget.LinearLayout.LayoutParams(
                        android.widget.LinearLayout.LayoutParams.MATCH_PARENT,
                        android.widget.LinearLayout.LayoutParams.WRAP_CONTENT);
        sbP.setMargins(0, 12, 0, 4);
        sb.setLayoutParams(sbP);
        sb.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar s, int progress, boolean fromUser) {
                valueHolder[0] = progress + min;
                int sec10 = valueHolder[0] * 10 / pagesPerSec;
                tvVal.setText(valueHolder[0] + " p (" + (sec10 / 10) + "." + (sec10 % 10) + "s)");
            }
            @Override public void onStartTrackingTouch(SeekBar s) {}
            @Override public void onStopTrackingTouch(SeekBar s) {}
        });
        block.addView(sb);
        return block;
    }

    /**
     * 步骤7：弹出每段录制配置对话框
     * 可设置：录制多少小节后自动停止 / 停止后是否自动播放
     */
    private void showSegConfigDialog(int idx) {
        // 用可变数组模拟 "局部 final" 变量
        final int[]     tmpMeasures     = {segMeasures[idx]};
        final boolean[] tmpAutoPlay     = {segAutoPlay[idx]};
        final int[]     tmpVolume       = {segVolumes[idx]};
        final long[]    tmpPreCropStart = {segPreCropStartMs[idx]};
        final long[]    tmpPreCropEnd   = {segPreCropEndMs[idx]};

        // ---- 构建弹窗主体布局 ----
        android.widget.LinearLayout root = new android.widget.LinearLayout(this);
        root.setOrientation(android.widget.LinearLayout.VERTICAL);
        root.setPadding(48, 32, 48, 16);
        root.setBackgroundColor(clr(R.color.dialog_bg));

        // -- 小节数区域 --
        android.widget.LinearLayout rowMeasures = new android.widget.LinearLayout(this);
        rowMeasures.setOrientation(android.widget.LinearLayout.VERTICAL);
        rowMeasures.setBackgroundColor(clr(R.color.dialog_section_bg));
        rowMeasures.setPadding(24, 20, 24, 20);
        android.widget.LinearLayout.LayoutParams blockParams =
                new android.widget.LinearLayout.LayoutParams(
                        android.widget.LinearLayout.LayoutParams.MATCH_PARENT,
                        android.widget.LinearLayout.LayoutParams.WRAP_CONTENT);
        blockParams.setMargins(0, 0, 0, 16);
        rowMeasures.setLayoutParams(blockParams);

        TextView lblMeasures = new TextView(this);
        lblMeasures.setText("录制小节数");
        lblMeasures.setTextColor(clr(R.color.text_accent));
        lblMeasures.setTextSize(14);
        lblMeasures.setTypeface(null, android.graphics.Typeface.BOLD);
        lblMeasures.setPadding(0, 0, 0, 12);
        rowMeasures.addView(lblMeasures);

        TextView subMeasures = new TextView(this);
        subMeasures.setText("设为 0 = 手动停止（无限录制）");
        subMeasures.setTextColor(clr(R.color.dialog_sub_text));
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
        btnDec.setTextColor(clr(R.color.text_primary));
        btnDec.setBackgroundColor(clr(R.color.dialog_btn_bg));
        android.widget.LinearLayout.LayoutParams btnP =
                new android.widget.LinearLayout.LayoutParams(120, 120);
        btnDec.setLayoutParams(btnP);

        final TextView tvVal = new TextView(this);
        tvVal.setTextColor(clr(R.color.text_success));
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
        btnInc.setTextColor(clr(R.color.text_primary));
        btnInc.setBackgroundColor(clr(R.color.dialog_btn_bg));
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
        unitHint.setTextColor(clr(R.color.dialog_unit_text));
        unitHint.setTextSize(10);
        unitHint.setGravity(android.view.Gravity.CENTER);
        unitHint.setPadding(0, 10, 0, 0);
        rowMeasures.addView(unitHint);

        root.addView(rowMeasures);

        // -- 播放模式区域 --
        android.widget.LinearLayout rowPlay = new android.widget.LinearLayout(this);
        rowPlay.setOrientation(android.widget.LinearLayout.VERTICAL);
        rowPlay.setBackgroundColor(clr(R.color.dialog_section_bg));
        rowPlay.setPadding(24, 20, 24, 20);
        android.widget.LinearLayout.LayoutParams blockParams2 =
                new android.widget.LinearLayout.LayoutParams(
                        android.widget.LinearLayout.LayoutParams.MATCH_PARENT,
                        android.widget.LinearLayout.LayoutParams.WRAP_CONTENT);
        blockParams2.setMargins(0, 0, 0, 8);
        rowPlay.setLayoutParams(blockParams2);

        TextView lblPlay = new TextView(this);
        lblPlay.setText("录制结束后");
        lblPlay.setTextColor(clr(R.color.text_accent));
        lblPlay.setTextSize(14);
        lblPlay.setTypeface(null, android.graphics.Typeface.BOLD);
        lblPlay.setPadding(0, 0, 0, 14);
        rowPlay.addView(lblPlay);

        android.widget.RadioGroup rgPlay = new android.widget.RadioGroup(this);
        rgPlay.setOrientation(android.widget.RadioGroup.VERTICAL);

        android.widget.RadioButton rbAutoPlay = new android.widget.RadioButton(this);
        rbAutoPlay.setText("自动开始播放");
        rbAutoPlay.setTextColor(clr(R.color.text_primary));
        rbAutoPlay.setTextSize(14);
        rbAutoPlay.setButtonTintList(android.content.res.ColorStateList.valueOf(
                clr(R.color.text_accent)));
        rbAutoPlay.setId(View.generateViewId());

        android.widget.RadioButton rbNoPlay = new android.widget.RadioButton(this);
        rbNoPlay.setText("停止录制，不自动播放");
        rbNoPlay.setTextColor(clr(R.color.text_primary));
        rbNoPlay.setTextSize(14);
        rbNoPlay.setButtonTintList(android.content.res.ColorStateList.valueOf(
                clr(R.color.text_accent)));
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
        rowVolume.setBackgroundColor(clr(R.color.dialog_section_bg));
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
        lblVolume.setTextColor(clr(R.color.text_accent));
        lblVolume.setTextSize(14);
        lblVolume.setTypeface(null, android.graphics.Typeface.BOLD);
        android.widget.LinearLayout.LayoutParams lblVolP =
                new android.widget.LinearLayout.LayoutParams(0,
                        android.widget.LinearLayout.LayoutParams.WRAP_CONTENT, 1f);
        lblVolume.setLayoutParams(lblVolP);

        final TextView tvVolValue = new TextView(this);
        tvVolValue.setText(tmpVolume[0] + "%");
        tvVolValue.setTextColor(clr(R.color.text_success));
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

        // -- 预裁剪区域 --
        android.widget.LinearLayout rowPreCrop = new android.widget.LinearLayout(this);
        rowPreCrop.setOrientation(android.widget.LinearLayout.VERTICAL);
        rowPreCrop.setBackgroundColor(clr(R.color.dialog_section_bg));
        rowPreCrop.setPadding(24, 20, 24, 20);
        android.widget.LinearLayout.LayoutParams blockPreParams =
                new android.widget.LinearLayout.LayoutParams(
                        android.widget.LinearLayout.LayoutParams.MATCH_PARENT,
                        android.widget.LinearLayout.LayoutParams.WRAP_CONTENT);
        blockPreParams.setMargins(0, 0, 0, 8);
        rowPreCrop.setLayoutParams(blockPreParams);

        TextView lblPreCrop = new TextView(this);
        lblPreCrop.setText("预裁剪");
        lblPreCrop.setTextColor(clr(R.color.text_accent));
        lblPreCrop.setTextSize(14);
        lblPreCrop.setTypeface(null, android.graphics.Typeface.BOLD);
        lblPreCrop.setPadding(0, 0, 0, 4);
        rowPreCrop.addView(lblPreCrop);

        TextView subPreCrop = new TextView(this);
        subPreCrop.setText("录制完成后自动截去首/尾多余时长（0.00s = 不截）");
        subPreCrop.setTextColor(clr(R.color.dialog_sub_text));
        subPreCrop.setTextSize(11);
        subPreCrop.setPadding(0, 0, 0, 14);
        rowPreCrop.addView(subPreCrop);

        // 起始截去行
        android.widget.LinearLayout startRow = new android.widget.LinearLayout(this);
        startRow.setOrientation(android.widget.LinearLayout.HORIZONTAL);
        startRow.setGravity(android.view.Gravity.CENTER_VERTICAL);
        TextView lblCropStart = new TextView(this);
        lblCropStart.setText("截去开头");
        lblCropStart.setTextColor(clr(R.color.text_primary));
        lblCropStart.setTextSize(13);
        android.widget.LinearLayout.LayoutParams lblCropStartP =
                new android.widget.LinearLayout.LayoutParams(0,
                        android.widget.LinearLayout.LayoutParams.WRAP_CONTENT, 1f);
        lblCropStart.setLayoutParams(lblCropStartP);
        final TextView tvCropStartVal = new TextView(this);
        tvCropStartVal.setTextColor(clr(R.color.text_success));
        tvCropStartVal.setTextSize(14);
        tvCropStartVal.setTypeface(null, android.graphics.Typeface.BOLD);
        tvCropStartVal.setGravity(android.view.Gravity.END);
        tvCropStartVal.setText(String.format("%.2fs", tmpPreCropStart[0] / 1000.0));
        startRow.addView(lblCropStart);
        startRow.addView(tvCropStartVal);
        rowPreCrop.addView(startRow);

        android.widget.LinearLayout.LayoutParams sbCropP =
                new android.widget.LinearLayout.LayoutParams(
                        android.widget.LinearLayout.LayoutParams.MATCH_PARENT,
                        android.widget.LinearLayout.LayoutParams.WRAP_CONTENT);
        sbCropP.setMargins(0, 8, 0, 16);
        final SeekBar sbCropStart = new SeekBar(this);
        sbCropStart.setMax(2000);
        sbCropStart.setProgress((int) tmpPreCropStart[0]);
        sbCropStart.setLayoutParams(sbCropP);
        sbCropStart.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar sb, int p, boolean fromUser) {
                tmpPreCropStart[0] = p;
                tvCropStartVal.setText(String.format("%.2fs", p / 1000.0));
            }
            @Override public void onStartTrackingTouch(SeekBar sb) {}
            @Override public void onStopTrackingTouch(SeekBar sb) {}
        });
        rowPreCrop.addView(sbCropStart);

        // 末尾截去行
        android.widget.LinearLayout endRow = new android.widget.LinearLayout(this);
        endRow.setOrientation(android.widget.LinearLayout.HORIZONTAL);
        endRow.setGravity(android.view.Gravity.CENTER_VERTICAL);
        TextView lblCropEnd = new TextView(this);
        lblCropEnd.setText("截去结尾");
        lblCropEnd.setTextColor(clr(R.color.text_primary));
        lblCropEnd.setTextSize(13);
        android.widget.LinearLayout.LayoutParams lblCropEndP =
                new android.widget.LinearLayout.LayoutParams(0,
                        android.widget.LinearLayout.LayoutParams.WRAP_CONTENT, 1f);
        lblCropEnd.setLayoutParams(lblCropEndP);
        final TextView tvCropEndVal = new TextView(this);
        tvCropEndVal.setTextColor(clr(R.color.text_success));
        tvCropEndVal.setTextSize(14);
        tvCropEndVal.setTypeface(null, android.graphics.Typeface.BOLD);
        tvCropEndVal.setGravity(android.view.Gravity.END);
        tvCropEndVal.setText(String.format("%.2fs", tmpPreCropEnd[0] / 1000.0));
        endRow.addView(lblCropEnd);
        endRow.addView(tvCropEndVal);
        rowPreCrop.addView(endRow);

        android.widget.LinearLayout.LayoutParams sbCropEndP =
                new android.widget.LinearLayout.LayoutParams(
                        android.widget.LinearLayout.LayoutParams.MATCH_PARENT,
                        android.widget.LinearLayout.LayoutParams.WRAP_CONTENT);
        sbCropEndP.setMargins(0, 8, 0, 4);
        final SeekBar sbCropEnd = new SeekBar(this);
        sbCropEnd.setMax(2000);
        sbCropEnd.setProgress((int) tmpPreCropEnd[0]);
        sbCropEnd.setLayoutParams(sbCropEndP);
        sbCropEnd.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar sb, int p, boolean fromUser) {
                tmpPreCropEnd[0] = p;
                tvCropEndVal.setText(String.format("%.2fs", p / 1000.0));
            }
            @Override public void onStartTrackingTouch(SeekBar sb) {}
            @Override public void onStopTrackingTouch(SeekBar sb) {}
        });
        rowPreCrop.addView(sbCropEnd);

        root.addView(rowPreCrop);

        // ---- 显示对话框 ----
        new AlertDialog.Builder(this)
                .setTitle("LOOP " + (idx + 1) + " 录制配置")
                .setView(root)
                .setPositiveButton("确定", (d, w) -> {
                    segMeasures[idx]       = tmpMeasures[0];
                    segAutoPlay[idx]       = tmpAutoPlay[0];
                    segVolumes[idx]        = tmpVolume[0];
                    segPreCropStartMs[idx] = tmpPreCropStart[0];
                    segPreCropEndMs[idx]   = tmpPreCropEnd[0];
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
                    saveSettings();
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
                        ? clr(R.color.seg_cfg_active)
                        : clr(R.color.seg_cfg_inactive));
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
                    saveSettings();
                })
                .setNegativeButton("取消", null)
                .show();
    }

    /** 删除 LOOP 段 */
    private void showSegDeleteDialog(int idx) {
        // 检查要删除的段是否正在播放或录制
        boolean isTargetActive = (segStates[idx] == SegState.PLAYING || segStates[idx] == SegState.RECORDING);
        String warningMsg = "";

        if (isTargetActive) {
            warningMsg = (segStates[idx] == SegState.PLAYING) ?
                    "\n⚠️ 该段正在播放，将先停止播放再删除。" :
                    "\n⚠️ 该段正在录制，将停止录制并删除。";
        }

        new AlertDialog.Builder(this)
                .setTitle("🗑 删除 LOOP " + (idx + 1))
                .setMessage("确认删除 LOOP " + (idx + 1) + "？" + warningMsg)
                .setPositiveButton("确认删除", (d, w) -> {
                    // 如果目标段正在播放或录制，先发送停止命令
                    if (segStates[idx] == SegState.PLAYING || segStates[idx] == SegState.RECORDING) {
                        sendCommandWithCallback("looper -t " + idx, success -> {
                            if (success) {
                                handler.postDelayed(() -> performSegmentDeletionSimple(idx), 150);
                            } else {
                                Toast.makeText(this, "停止失败，无法删除", Toast.LENGTH_SHORT).show();
                            }
                        });
                    } else {
                        performSegmentDeletionSimple(idx);
                    }
                })
                .setNegativeButton("取消", null)
                .show();
    }

    /**
     * 执行段删除操作（简化版，PSRAM无需擦除）
     */
    private void performSegmentDeletionSimple(int idx) {
        cancelAutoStop(idx);
        cancelCountdown(idx);

        // 停止可能正在运行的存储空间更新任务
        stopStorageUpdateForRecording(idx);

        // 取消等待停止预测定时
        if (waitFinishPredictRunnables[idx] != null) {
            handler.removeCallbacks(waitFinishPredictRunnables[idx]);
            waitFinishPredictRunnables[idx] = null;
        }
        waitFinishEnabled[idx] = false;
        segStates[idx]         = SegState.INACTIVE;
        segLoopDurationMs[idx]  = 0;
        segRecordStartTime[idx] = 0;
        segPlayStartTime[idx]   = 0;
        segTrimStartPage[idx]   = 0;
        segTrimEndPage[idx]     = 0;
        removeAudioTrackCard(idx);
        refreshSegUI(idx);
        refreshSegCfgHint(idx);
        updateControlButtonStates();
        updateStorageDisplay();
        sendCommand("looper -c " + idx, null);  // 使用清除命令
        Toast.makeText(this,
                "LOOP " + (idx + 1) + " 已删除",
                Toast.LENGTH_SHORT).show();
    }

    interface SuccessCallback { void onResult(boolean success); }

    // ========================================================================
    // 音频轨面板：upsert / remove / updateTrimDisplay / applyTrim
    // ========================================================================

    private static final int PAGES_PER_SEC = 1000; // 192000 B/s ÷ 192 B/page

    /**
     * 新增或更新指定段的音频轨卡片。
     * 每次录制完成（fromRecording=true）后调用，重新构建整个卡片以反映最新时长。
     */
    private void upsertAudioTrackCard(final int idx) {
        if (containerAudioTracks == null) return;

        // 移除旧卡片（如有）
        if (audioTrackViews[idx] != null) {
            containerAudioTracks.removeView(audioTrackViews[idx]);
        }

        long durationMs = segLoopDurationMs[idx];
        final float durationSec = durationMs / 1000f;
        final int maxPages = durationSec > 0
                ? (int)(durationSec * PAGES_PER_SEC)
                : segMaxRecSec[idx] * PAGES_PER_SEC;

        // Inflate 卡片布局
        View card = getLayoutInflater().inflate(R.layout.item_audio_track, containerAudioTracks, false);
        audioTrackViews[idx] = card;

        // ── 子视图引用 ──
        TextView tvLabel      = card.findViewById(R.id.tv_track_label);
        TextView tvDuration   = card.findViewById(R.id.tv_track_duration);
        AudioTrackTrimView trimView  = card.findViewById(R.id.trim_view);
        TextView tvStartLabel = card.findViewById(R.id.tv_trim_start_label);
        TextView tvEndLabel   = card.findViewById(R.id.tv_trim_end_label);
        android.widget.EditText etStart = card.findViewById(R.id.et_trim_start);
        android.widget.EditText etEnd   = card.findViewById(R.id.et_trim_end);
        Button btnApply = card.findViewById(R.id.btn_track_apply_trim);
        Button btnReset = card.findViewById(R.id.btn_track_reset_trim);

        tvLabel.setText("🎵 LOOP " + (idx + 1));
        // 显示总时长及预裁剪设置
        long preCropS = segPreCropStartMs[idx];
        long preCropE = segPreCropEndMs[idx];
        if (preCropS > 0 || preCropE > 0) {
            StringBuilder sb = new StringBuilder(String.format("%.2fs", durationSec));
            sb.append("  ✂");
            if (preCropS > 0) sb.append(String.format(" 去头%.2fs", preCropS / 1000.0));
            if (preCropE > 0) sb.append(String.format(" 去尾%.2fs", preCropE / 1000.0));
            tvDuration.setText(sb.toString());
        } else {
            tvDuration.setText(String.format("%.2fs", durationSec));
        }

        // ── 初始裁剪位置 ──
        int startPage = segTrimStartPage[idx];
        int endPage   = segTrimEndPage[idx] > 0 ? segTrimEndPage[idx] : maxPages;
        float startFrac = maxPages > 0 ? (float)startPage / maxPages : 0f;
        float endFrac   = maxPages > 0 ? (float)endPage   / maxPages : 1f;

        trimView.setTrimFractions(startFrac, endFrac);

        float startSec0 = startPage / (float)PAGES_PER_SEC;
        float endSec0   = endPage   / (float)PAGES_PER_SEC;
        tvStartLabel.setText(String.format("%.2fs", startSec0));
        tvEndLabel.setText(String.format("%.2fs", endSec0));
        etStart.setText(String.format("%.2f", startSec0));
        etEnd.setText(String.format("%.2f", endSec0));

        // ── 拖拽回调：实时同步 EditText & 标签（不发指令）──
        trimView.setOnTrimChangedListener((sf, ef) -> {
            float sS = sf * durationSec;
            float eS = ef * durationSec;
            tvStartLabel.setText(String.format("%.2fs", sS));
            tvEndLabel.setText(String.format("%.2fs", eS));
            etStart.setText(String.format("%.2f", sS));
            etEnd.setText(String.format("%.2f", eS));
        });
        // ── 松手回调：揧开手指才发送 BLE 指令 ──
        trimView.setOnTrimCommittedListener((sf, ef) -> {
            float sSec = sf * durationSec;
            float eSec = ef * durationSec;
            applyTrimForSegment(idx, sSec, eSec, maxPages);
        });

        // ── 应用按钮 ──
        btnApply.setOnClickListener(v -> {
            float sSec, eSec;
            try { sSec = Float.parseFloat(etStart.getText().toString()); }
            catch (NumberFormatException ex) { sSec = 0f; }
            try { eSec = Float.parseFloat(etEnd.getText().toString()); }
            catch (NumberFormatException ex) { eSec = durationSec; }
            if (sSec < 0) sSec = 0;
            if (durationSec > 0 && eSec > durationSec) eSec = durationSec;
            if (sSec >= eSec) {
                Toast.makeText(this, "头必须小于尾", Toast.LENGTH_SHORT).show();
                return;
            }
            // 更新可视裁剪条
            float sF = durationSec > 0 ? sSec / durationSec : 0f;
            float eF = durationSec > 0 ? eSec / durationSec : 1f;
            trimView.setTrimFractions(sF, eF);
            tvStartLabel.setText(String.format("%.2fs", sSec));
            tvEndLabel.setText(String.format("%.2fs", eSec));
            // 应用裁剪
            applyTrimForSegment(idx, sSec, eSec, maxPages);
            // 收起键盘
            android.view.inputmethod.InputMethodManager imm =
                    (android.view.inputmethod.InputMethodManager)getSystemService(INPUT_METHOD_SERVICE);
            if (imm != null) imm.hideSoftInputFromWindow(v.getWindowToken(), 0);
        });

        // ── 重置按钮 ──
        btnReset.setOnClickListener(v -> {
            segTrimStartPage[idx] = 0;
            segTrimEndPage[idx]   = 0;
            trimView.setTrimFractions(0f, 1f);
            tvStartLabel.setText("0.00s");
            tvEndLabel.setText(String.format("%.2fs", durationSec));
            etStart.setText("0.00");
            etEnd.setText(String.format("%.2f", durationSec));
            sendCommand("looper -T " + idx + " 0 0", null);
            Toast.makeText(this, "LOOP " + (idx + 1) + " 裁剪已重置", Toast.LENGTH_SHORT).show();
        });

        containerAudioTracks.addView(card);
        if (layoutAudioTracksPanel != null) {
            layoutAudioTracksPanel.setVisibility(View.VISIBLE);
        }
    }

    /** 移除指定段的音频轨卡片；若容器为空则隐藏面板 */
    private void removeAudioTrackCard(int idx) {
        if (containerAudioTracks == null) return;
        if (audioTrackViews[idx] != null) {
            containerAudioTracks.removeView(audioTrackViews[idx]);
            audioTrackViews[idx] = null;
        }
        if (containerAudioTracks.getChildCount() == 0 && layoutAudioTracksPanel != null) {
            layoutAudioTracksPanel.setVisibility(View.GONE);
        }
    }

    /**
     * BLE 返回 trim 二进制包后，刷新对应音频轨卡片中的裁剪位置显示。
     * 仅在卡片已存在时有效（不触发新建）。
     */
    private void updateAudioTrackTrimDisplay(int idx) {
        View card = audioTrackViews[idx];
        if (card == null) return;
        AudioTrackTrimView trimView  = card.findViewById(R.id.trim_view);
        android.widget.EditText etStart = card.findViewById(R.id.et_trim_start);
        android.widget.EditText etEnd   = card.findViewById(R.id.et_trim_end);
        TextView tvStartLabel = card.findViewById(R.id.tv_trim_start_label);
        TextView tvEndLabel   = card.findViewById(R.id.tv_trim_end_label);
        if (trimView == null) return;

        long durationMs = segLoopDurationMs[idx];
        float durationSec = durationMs / 1000f;
        int maxPages = durationSec > 0
                ? (int)(durationSec * PAGES_PER_SEC)
                : segMaxRecSec[idx] * PAGES_PER_SEC;

        int startPage = segTrimStartPage[idx];
        int endPage   = segTrimEndPage[idx] > 0 ? segTrimEndPage[idx] : maxPages;

        float startSec = startPage / (float)PAGES_PER_SEC;
        float endSec   = endPage   / (float)PAGES_PER_SEC;
        float sf = maxPages > 0 ? (float)startPage / maxPages : 0f;
        float ef = maxPages > 0 ? (float)endPage   / maxPages : 1f;

        trimView.setTrimFractions(sf, ef);
        if (etStart != null) etStart.setText(String.format("%.2f", startSec));
        if (etEnd   != null) etEnd.setText(String.format("%.2f", endSec));
        if (tvStartLabel != null) tvStartLabel.setText(String.format("%.2fs", startSec));
        if (tvEndLabel   != null) tvEndLabel.setText(String.format("%.2fs", endSec));
    }

    /**
     * 将秒数转换为 Flash 页并发送 looper -T 命令。
     * endPage == maxPages 时传 0（表示录到末尾），与下位机约定一致。
     */
    private void applyTrimForSegment(int idx, float startSec, float endSec, int maxPages) {
        int startPage    = (int)(startSec * PAGES_PER_SEC);
        int endPage      = (int)(endSec   * PAGES_PER_SEC);
        int sendEndPage  = (endPage >= maxPages) ? 0 : endPage;

        segTrimStartPage[idx] = startPage;
        segTrimEndPage[idx]   = sendEndPage;

        sendCommand("looper -T " + idx + " " + startPage + " " + sendEndPage, null);

        // 始终以裁剪后的有效长度更新循环时长（不依赖 seg1MatchDuration 是否开启）
        int effectiveEnd = (sendEndPage > 0) ? sendEndPage : maxPages;
        long newDurMs = (long)(effectiveEnd - startPage) * 1000L / PAGES_PER_SEC;
        segLoopDurationMs[idx] = newDurMs;
        // 重新锚定播放相位：让跟随录制的边界计算以新周期为基准
        if (segStates[idx] == SegState.PLAYING && newDurMs > 0) {
            long now = System.currentTimeMillis();
            long elapsed = (now - segPlayStartTime[idx]) % newDurMs;
            segPlayStartTime[idx] = now - elapsed;
        }
        Toast.makeText(this, "LOOP " + (idx + 1) + " 裁剪已应用", Toast.LENGTH_SHORT).show();
    }

    /**
     * 获取指定 PLAYING 段在当前循环周期内的已播放位置（ms）。
     * 即：从本轮循环起点到当前时刻走过了多少毫秒。
     * 返回 -1 表示无法计算（段未播放或时长未知）。
     */
    private long getLoopPositionMs(int segIdx) {
        long dur = getEffectiveLoopDurationMs(segIdx);
        if (dur <= 0) return -1L;
        long loopStart = segPlayStartTime[segIdx];  // 最近一次循环起点时间戳
        if (loopStart <= 0) return -1L;
        return (System.currentTimeMillis() - loopStart) % dur;
    }

    /**
     * 距下次回绕的剩余毫秒数 = 循环总时长 - 当前播放位置。
     * 返回 -1 表示无法计算。
     */
    private long msUntilNextWrap(int segIdx) {
        long dur = getEffectiveLoopDurationMs(segIdx);
        if (dur <= 0) return -1L;
        long pos = getLoopPositionMs(segIdx);   // 当前播放位置
        if (pos < 0) return -1L;
        return dur - pos;                        // 总时长 - 当前位置 = 剩余时间
    }

    /**
     * 返回第 idx 段考虑裁剪后的有效循环时长（ms）。
     * 若 trim pages 已设置则直接从页数计算；否则返回 segLoopDurationMs。
     * 此方法不依赖 segLoopDurationMs 是否已被更新，始终能给出正确结果。
     */
    private long getEffectiveLoopDurationMs(int idx) {
        long rawMs    = segLoopDurationMs[idx];
        if (rawMs <= 0) return 0;
        int startPage = segTrimStartPage[idx];
        int endPage   = segTrimEndPage[idx];
        if (startPage == 0 && endPage == 0) return rawMs;   // 未裁剪
        int rawPages  = (int)(rawMs * PAGES_PER_SEC / 1000L);
        int effEnd    = (endPage > 0) ? endPage : rawPages;
        if (effEnd <= startPage) return rawMs;              // 裁剪范围非法，回退到原始时长
        return (long)(effEnd - startPage) * 1000L / PAGES_PER_SEC;
    }

    /**
     * 返回目前唯一正在 PLAYING 且不是 recIdx 的段索引；
     * 若无此段或有多个 PLAYING 段，返回 -1。
     */
    /**
     * 获取参照播放段索引（优先使用最早开始播放的段）
     * @param recIdx 待录制段索引
     * @return 参照段索引，-1 表示无法确定
     */
    private int getRefPlayingSegIdx(int recIdx) {
        // 如果已有确定的参照段且正在播放，优先使用它
        if (referenceSegIndex >= 0 && referenceSegIndex != recIdx &&
                segStates[referenceSegIndex] == SegState.PLAYING) {
            return referenceSegIndex;
        }

        // 否则回退到旧逻辑：查找任意一个正在播放的段
        int found = -1;
        for (int i = 0; i < SEG_COUNT; i++) {
            if (i != recIdx && segStates[i] == SegState.PLAYING) {
                if (found >= 0) return -1; // 多个 PLAYING 段，无法确定参考
                found = i;
            }
        }
        return found;
    }

    /**
     * 录制完成后自动应用预裁剪设置。
     * segPreCropStartMs[idx] = 从录制开头截去的时长（ms）
     * segPreCropEndMs[idx]   = 从录制末尾截去的时长（ms）
     */
    private void applyPreCrop(int idx) {
        // SR 路径专用：固件已自行完成停录，单独发送裁剪命令
        String cmd = buildPreCropUpdate(idx);
        if (!cmd.isEmpty()) {
            upsertAudioTrackCard(idx); // 用裁剪后的参数刷新音频轨卡片
            sendCommand(cmd, null);
        }
    }

    /**
     * 计算预裁剪参数并更新 Java 内部状态（segTrimStartPage/End、segLoopDurationMs、segPlayStartTime）。
     * 返回需要发送的 "looper -T ..." 命令字符串，若无预裁剪则返回 ""。
     * 调用者负责将返回値并入所属 BLE 包一并发送。
     */
    private String buildPreCropUpdate(int idx) {
        long startCropMs = segPreCropStartMs[idx];
        long endCropMs   = segPreCropEndMs[idx];
        if (startCropMs <= 0 && endCropMs <= 0) return "";
        long durMs = segLoopDurationMs[idx];
        if (durMs <= 0) return "";
        int startPage = (startCropMs > 0) ? (int)(startCropMs * PAGES_PER_SEC / 1000L) : 0;
        int endPage   = 0;
        if (endCropMs > 0) {
            long absEndMs = durMs - endCropMs;
            if (absEndMs > 0) endPage = (int)(absEndMs * PAGES_PER_SEC / 1000L);
        }
        if (startPage == 0 && endPage == 0) return "";
        if (endPage > 0 && endPage <= startPage) return ""; // 裁剪范围非法
        segTrimStartPage[idx] = startPage;
        segTrimEndPage[idx]   = endPage;
        // 更新有效时长
        int rawPages = (int)(durMs * PAGES_PER_SEC / 1000L);
        int effEnd   = (endPage > 0) ? endPage : rawPages;
        if (effEnd > startPage) {
            long newDurMs = (long)(effEnd - startPage) * 1000L / PAGES_PER_SEC;
            segLoopDurationMs[idx] = newDurMs;
            if (segPlayStartTime[idx] > 0) {
                long now2    = System.currentTimeMillis();
                long elapsed = (now2 - segPlayStartTime[idx]) % newDurMs;
                segPlayStartTime[idx] = now2 - elapsed;
            }
        }
        return "looper -T " + idx + " " + startPage + " " + endPage;
    }

    /** 将所有本地设置持久化到 SharedPreferences */
    private void saveSettings() {
        android.content.SharedPreferences.Editor ed =
                getSharedPreferences("LooperSettings", MODE_PRIVATE).edit();
        ed.putBoolean("metro_on_during_rec",  metroOnDuringRec);
        ed.putBoolean("countdown_before_rec", countdownBeforeRec);
        ed.putInt("countdown_beats", countdownBeats);
        ed.putInt("current_beats",   currentBeats);
        for (int i = 0; i < SEG_COUNT; i++) {
            ed.putInt    ("seg_measures_"         + i, segMeasures[i]);
            ed.putBoolean("seg_auto_play_"        + i, segAutoPlay[i]);
            ed.putInt    ("seg_volumes_"           + i, segVolumes[i]);
            ed.putInt    ("seg_max_rec_sec_"       + i, segMaxRecSec[i]);
            ed.putInt    ("seg_pre_crop_start_ms_" + i, (int) segPreCropStartMs[i]);
            ed.putInt    ("seg_pre_crop_end_ms_"   + i, (int) segPreCropEndMs[i]);
        }
        ed.apply();
    }

    /** 从 SharedPreferences 恢复上次保存的设置 */
    private void loadSettings() {
        android.content.SharedPreferences sp =
                getSharedPreferences("LooperSettings", MODE_PRIVATE);
        metroOnDuringRec   = sp.getBoolean("metro_on_during_rec",  false);
        countdownBeforeRec = sp.getBoolean("countdown_before_rec", true);
        countdownBeats     = sp.getInt("countdown_beats", 4);
        currentBeats       = sp.getInt("current_beats",   4);
        for (int i = 0; i < SEG_COUNT; i++) {
            segMeasures[i]       = sp.getInt    ("seg_measures_"         + i, 0);
            segAutoPlay[i]       = sp.getBoolean("seg_auto_play_"        + i, true);
            segVolumes[i]        = sp.getInt    ("seg_volumes_"           + i, 100);
            segMaxRecSec[i]      = sp.getInt    ("seg_max_rec_sec_"       + i, 10);
            segPreCropStartMs[i] = sp.getInt    ("seg_pre_crop_start_ms_" + i, 0);
            segPreCropEndMs[i]   = sp.getInt    ("seg_pre_crop_end_ms_"   + i, 0);
        }
    }
}