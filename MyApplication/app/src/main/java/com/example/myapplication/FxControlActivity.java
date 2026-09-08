package com.example.myapplication;

import android.os.Bundle;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

import androidx.core.content.ContextCompat;

import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Queue;
import java.util.LinkedList;

/**
 * 音效控制界面。
 *
 * 每个效果一张卡片：标题 + bypass 开关 + 该效果的参数滑条。
 * 效果开关用固件的 bypass 语义（graph bypass <name> on|off）：
 *   - 开关"启用"  → graph bypass <name> off（关闭旁路，效果生效）
 *   - 开关"关闭"  → graph bypass <name> on （旁路直通，原声）
 * 注意不能用 graph node on|off：节点 disabled 时执行器会跳过该节点，
 * 输出缓冲不更新 → ADC 链路断音。
 *
 * 参数用 fx <id> <param> <value>；进入界面时用 graph query all 一次性
 * 取回全部节点的 bypass 与参数值回填 UI。
 */
public class FxControlActivity extends BaseActivity {
    private static final String TAG = "FxControl";

    /** 单个效果的定义 */
    private static class FxDef {
        final String   title;      // 卡片标题
        final String   nodeName;   // 固件节点名（graph bypass / query 用）
        final int      nodeId;     // 固件节点 ID（fx 命令用）
        final String[] labels;     // 参数显示名
        final String[] keys;       // 固件参数名
        final int[]    ranges;     // {min,max} 成对
        final int[]    defaults;

        FxDef(String title, String nodeName, int nodeId,
              String[] labels, String[] keys, int[] ranges, int[] defaults) {
            this.title = title; this.nodeName = nodeName; this.nodeId = nodeId;
            this.labels = labels; this.keys = keys;
            this.ranges = ranges; this.defaults = defaults;
        }
    }

    /** 可调效果清单（节点名/ID 需与固件 effect_graph_config.h 的默认图一致） */
    private static final FxDef[] FX_DEFS = {
        new FxDef("🎚️ DRC 动态压缩", "drc", 10,
                new String[]{"Threshold", "Ratio", "Attack", "Release"},
                new String[]{"threshold", "ratio", "attack", "release"},
                new int[]{-60, 0, 1, 20, 1, 500, 10, 2000},
                new int[]{-30, 10, 100, 200}),

        new FxDef("🎵 混响 Reverb", "reverb", 12,
                new String[]{"Room", "Damp", "Wet"},
                new String[]{"room", "damp", "wet"},
                new int[]{0, 100, 0, 100, 0, 100},
                new int[]{50, 50, 30}),

        new FxDef("⏱️ 延迟 Delay（吉他）", "delay_guitar_l", 22,
                new String[]{"Time", "F.Back", "Wet"},
                new String[]{"time", "feedback", "wet"},
                new int[]{10, 120, 0, 100, 0, 100},
                new int[]{120, 30, 30}),

        new FxDef("🎶 合唱 Chorus（吉他）", "chorus_guitar_l", 23,
                new String[]{"Depth", "Rate", "Wet", "F.Back"},
                new String[]{"depth", "rate", "wet", "feedback"},
                new int[]{0, 100, 0, 100, 0, 100, 0, 50},
                new int[]{30, 10, 60, 30}),

        new FxDef("🎤 合唱 Chorus（麦克风）", "chorus_mic_l", 24,
                new String[]{"Depth", "Rate", "Wet", "F.Back"},
                new String[]{"depth", "rate", "wet", "feedback"},
                new int[]{0, 100, 0, 100, 0, 100, 0, 50},
                new int[]{30, 10, 60, 30}),

        new FxDef("🔊 扩展器 Expander", "expander", 9,
                new String[]{"Threshold", "Ratio"},
                new String[]{"threshold", "ratio"},
                new int[]{-80, 0, 1, 10},
                new int[]{-40, 2}),
    };

    private BluetoothHelper bluetoothHelper;

    /** UI 引用：节点名 → 组件 */
    private final Map<String, Switch>                          fxSwitches      = new LinkedHashMap<>();
    private final Map<String, Map<String, VerticalSeekBar>>    effectSeekBars  = new LinkedHashMap<>();
    private final Map<String, Map<String, TextView>>           effectValueTexts= new LinkedHashMap<>();

    /** 程序回填状态时置位，避免触发 Switch 回调把状态又发回设备 */
    private boolean applyingState = false;

    /* 命令队列：BLE 写入必须串行 */
    private android.os.Handler commandHandler = new android.os.Handler(android.os.Looper.getMainLooper());
    private final Queue<Runnable> commandQueue = new LinkedList<>();
    private boolean isSendingCommand = false;

    /** 等待 graph query all 的 JSON（BLE 会分片，需要拼接） */
    private boolean awaitingGraphJson = false;
    private final StringBuilder rxBuffer = new StringBuilder();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        bluetoothHelper = BluetoothManager.getInstance().getBluetoothHelper();
        if (bluetoothHelper == null || !bluetoothHelper.isConnected()) {
            Toast.makeText(this, "请先连接蓝牙设备", Toast.LENGTH_LONG).show();
            finish();
            return;
        }

        setContentView(R.layout.activity_fx_control);
        setupBaseToolbar(true);

        ImageButton btnSave = findViewById(R.id.btn_save_fx);
        btnSave.setOnClickListener(v -> saveFxSettings());

        Switch swAll = findViewById(R.id.sw_all_fx);
        swAll.setOnCheckedChangeListener((btn, on) -> {
            if (applyingState) return;
            setAllEffectsEnabled(on);
        });

        buildCards();
        setupBleNotificationListener();
        queryAllFx();
    }

    @Override
    protected String getToolbarTitle() {
        return "音效控制";
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        commandHandler.removeCallbacksAndMessages(null);
        commandQueue.clear();
        if (bluetoothHelper != null) {
            bluetoothHelper.setBleNotifyListener(null);
        }
    }

    /* ==================== UI 构建 ==================== */

    private void buildCards() {
        LinearLayout container = findViewById(R.id.fx_cards_container);
        container.removeAllViews();
        for (FxDef def : FX_DEFS) {
            container.addView(createCard(def));
        }
    }

    private View createCard(FxDef def) {
        int pad = getResources().getDimensionPixelSize(R.dimen.toolbar_padding);

        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setBackgroundColor(ContextCompat.getColor(this, R.color.bg_card));
        card.setPadding(pad, pad, pad, pad);
        LinearLayout.LayoutParams cp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        cp.bottomMargin = getResources().getDimensionPixelSize(R.dimen.spacing_medium);
        card.setLayoutParams(cp);

        /* 标题 + 启用开关 */
        LinearLayout header = new LinearLayout(this);
        header.setOrientation(LinearLayout.HORIZONTAL);
        header.setGravity(Gravity.CENTER_VERTICAL);
        header.setLayoutParams(new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));

        TextView title = new TextView(this);
        title.setText(def.title);
        title.setTextSize(16);
        title.setTextColor(ContextCompat.getColor(this, R.color.primary_accent_glow));
        title.setTypeface(null, android.graphics.Typeface.BOLD);
        title.setLayoutParams(new LinearLayout.LayoutParams(
                0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f));
        header.addView(title);

        Switch sw = new Switch(this);
        sw.setText("启用");
        sw.setTextSize(12);
        sw.setChecked(false);   // 固件默认 bypass，等 query 回来再回填
        sw.setOnCheckedChangeListener((btn, isChecked) -> {
            if (applyingState) return;
            sendBypass(def.nodeName, !isChecked);   // 启用 = 关闭旁路
        });
        header.addView(sw);
        card.addView(header);
        fxSwitches.put(def.nodeName, sw);

        /* 参数滑条组 */
        LinearLayout sliders = new LinearLayout(this);
        sliders.setOrientation(LinearLayout.HORIZONTAL);
        sliders.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams sp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, dp(220));
        sp.topMargin = pad;
        sliders.setLayoutParams(sp);
        createSliders(sliders, def);
        card.addView(sliders);

        return card;
    }

    private void createSliders(LinearLayout container, FxDef def) {
        Map<String, VerticalSeekBar> bars  = new HashMap<>();
        Map<String, TextView>        texts = new HashMap<>();

        for (int i = 0; i < def.labels.length; i++) {
            final String key = def.keys[i];
            final int min = def.ranges[i * 2];
            final int max = def.ranges[i * 2 + 1];
            final int defVal = def.defaults[i];

            LinearLayout col = new LinearLayout(this);
            col.setOrientation(LinearLayout.VERTICAL);
            col.setGravity(Gravity.CENTER);
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                    0, LinearLayout.LayoutParams.MATCH_PARENT, 1f);
            lp.setMargins(8, 0, 8, 0);
            col.setLayoutParams(lp);

            TextView valueText = new TextView(this);
            valueText.setText(String.valueOf(defVal));
            valueText.setTextSize(16);
            valueText.setTextColor(ContextCompat.getColor(this, R.color.text_accent));
            valueText.setGravity(Gravity.CENTER);
            valueText.setTextAlignment(View.TEXT_ALIGNMENT_CENTER);
            valueText.setPadding(0, 0, 0, 12);
            col.addView(valueText);

            VerticalSeekBar bar = new VerticalSeekBar(this);
            bar.setMax(max - min);
            bar.setProgress(defVal - min);
            bar.setLayoutParams(new LinearLayout.LayoutParams(80, 0, 1f));
            bar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                @Override public void onProgressChanged(SeekBar sb, int progress, boolean fromUser) {
                    valueText.setText(String.valueOf(min + progress));
                }
                @Override public void onStartTrackingTouch(SeekBar sb) { }
                @Override public void onStopTrackingTouch(SeekBar sb) {
                    sendFxCommand(def.nodeId, key, min + sb.getProgress());
                }
            });
            col.addView(bar);

            TextView label = new TextView(this);
            label.setText(def.labels[i]);
            label.setTextSize(13);
            label.setTextColor(ContextCompat.getColor(this, R.color.text_primary));
            label.setGravity(Gravity.CENTER);
            label.setTextAlignment(View.TEXT_ALIGNMENT_CENTER);
            label.setPadding(0, 12, 0, 0);
            col.addView(label);

            container.addView(col);
            bars.put(key, bar);
            texts.put(key, valueText);
        }

        effectSeekBars.put(def.nodeName, bars);
        effectValueTexts.put(def.nodeName, texts);
    }

    private int dp(int value) {
        return (int) (value * getResources().getDisplayMetrics().density + 0.5f);
    }

    /* ==================== 查询与回填 ==================== */

    private void queryAllFx() {
        rxBuffer.setLength(0);
        awaitingGraphJson = true;
        queueCommand(() -> writeRaw("graph query all"));
        // 超时保护：分片未收全也尝试解析
        commandHandler.postDelayed(() -> {
            if (awaitingGraphJson) {
                awaitingGraphJson = false;
                tryParseGraphJson();
            }
        }, 2500);
    }

    private void setupBleNotificationListener() {
        if (bluetoothHelper == null) return;
        bluetoothHelper.setBleNotifyListener(data -> {
            if (data == null) return;

            if (awaitingGraphJson) {
                rxBuffer.append(data);
                String s = rxBuffer.toString();
                if (s.contains("\"nodes\"") && s.indexOf("]}") > 0) {
                    awaitingGraphJson = false;
                    tryParseGraphJson();
                }
                return;
            }
            // 兼容旧的二进制同步包（DRC/Reverb）
            if (looksLikeHex(data)) {
                parseBinaryFxData(data);
            }
        });
    }

    private void tryParseGraphJson() {
        String s = rxBuffer.toString();
        rxBuffer.setLength(0);

        int start = s.indexOf('{');
        int end = s.lastIndexOf("]}");
        if (start < 0 || end < 0 || end <= start) return;

        String json = s.substring(start, end + 2);
        try {
            org.json.JSONObject root = new org.json.JSONObject(json);
            org.json.JSONArray nodes = root.optJSONArray("nodes");
            if (nodes == null) return;

            final Map<String, Boolean> enabledMap = new HashMap<>();
            final Map<String, org.json.JSONObject> paramMap = new HashMap<>();
            for (int i = 0; i < nodes.length(); i++) {
                org.json.JSONObject n = nodes.getJSONObject(i);
                String name = n.optString("name", "");
                if (name.isEmpty()) continue;
                // 缺省按旁路处理（固件默认 bypass=true 保持原音色）
                boolean on = (n.optInt("bypass", 1) == 0);
                enabledMap.put(name, on);
                FxState.put(name, on);
                org.json.JSONObject p = n.optJSONObject("params");
                if (p != null) paramMap.put(name, p);
            }
            FxState.setLoaded(true);
            runOnUiThread(() -> applyQueriedState(enabledMap, paramMap));
        } catch (Exception e) {
            Log.e(TAG, "graph query parse failed: " + json, e);
        }
    }

    private void applyQueriedState(Map<String, Boolean> enabledMap,
                                   Map<String, org.json.JSONObject> paramMap) {
        applyingState = true;
        try {
            for (FxDef def : FX_DEFS) {
                Boolean en = enabledMap.get(def.nodeName);
                Switch sw = fxSwitches.get(def.nodeName);
                if (en != null && sw != null) sw.setChecked(en);

                org.json.JSONObject p = paramMap.get(def.nodeName);
                if (p == null) continue;
                Map<String, VerticalSeekBar> bars  = effectSeekBars.get(def.nodeName);
                Map<String, TextView>        texts = effectValueTexts.get(def.nodeName);
                if (bars == null || texts == null) continue;

                for (int i = 0; i < def.keys.length; i++) {
                    if (!p.has(def.keys[i])) continue;
                    int v = p.optInt(def.keys[i]);
                    int min = def.ranges[i * 2];
                    int max = def.ranges[i * 2 + 1];
                    v = Math.max(min, Math.min(max, v));
                    VerticalSeekBar bar = bars.get(def.keys[i]);
                    TextView tv = texts.get(def.keys[i]);
                    if (bar != null) bar.setProgress(v - min);
                    if (tv != null) tv.setText(String.valueOf(v));
                }
            }
            updateAllSwitch();
        } finally {
            applyingState = false;
        }
    }

    /** 总开关：所有效果都启用时才点亮 */
    private void updateAllSwitch() {
        Switch swAll = findViewById(R.id.sw_all_fx);
        if (swAll == null) return;
        boolean allOn = true, anyFound = false;
        for (FxDef def : FX_DEFS) {
            Switch sw = fxSwitches.get(def.nodeName);
            if (sw != null) {
                anyFound = true;
                if (!sw.isChecked()) allOn = false;
            }
        }
        swAll.setChecked(anyFound && allOn);
    }

    /* ==================== 命令发送 ==================== */

    /** 启用/关闭单个效果（on=启用→关闭旁路） */
    private void sendBypass(String nodeName, boolean bypass) {
        FxState.put(nodeName, !bypass);
        queueCommand(() -> writeRaw("graph bypass " + nodeName + " " + (bypass ? "on" : "off")));
    }

    private void setAllEffectsEnabled(boolean on) {
        for (FxDef def : FX_DEFS) {
            sendBypass(def.nodeName, !on);
        }
        applyingState = true;
        try {
            for (FxDef def : FX_DEFS) {
                Switch sw = fxSwitches.get(def.nodeName);
                if (sw != null) sw.setChecked(on);
            }
        } finally {
            applyingState = false;
        }
    }

    private void sendFxCommand(int nodeId, String param, int value) {
        queueCommand(() -> writeRaw("fx " + nodeId + " " + param + " " + value));
    }

    private void saveFxSettings() {
        queueCommand(() -> writeRaw("chain -S"));
        runOnUiThread(() -> Toast.makeText(this, "效果参数已保存", Toast.LENGTH_SHORT).show());
    }

    private void writeRaw(String cmd) {
        if (bluetoothHelper != null && bluetoothHelper.isConnected()) {
            bluetoothHelper.writeCharacteristic("0000ab01-0000-1000-8000-00805f9b34fb",
                    (cmd + "\r\n").getBytes(), success -> {
                        if (!success) {
                            Log.w(TAG, "send failed: " + cmd);
                        }
                        commandHandler.postDelayed(() -> {
                            isSendingCommand = false;
                            processNextCommand();
                        }, 150);
                    });
        } else {
            isSendingCommand = false;
            processNextCommand();
        }
    }

    private void queueCommand(Runnable command) {
        commandQueue.offer(command);
        processNextCommand();
    }

    private void processNextCommand() {
        if (isSendingCommand || commandQueue.isEmpty()) return;
        isSendingCommand = true;
        Runnable cmd = commandQueue.poll();
        if (cmd != null) {
            cmd.run();
        } else {
            isSendingCommand = false;
        }
    }

    /* ==================== 旧二进制同步包兼容 ==================== */

    private boolean looksLikeHex(String data) {
        String t = data.replace("0x", "").replaceAll("\\s+", "");
        return !t.isEmpty() && t.matches("[0-9A-Fa-f]+") && t.length() >= 8;
    }

    private boolean parseBinaryFxData(String data) {
        try {
            String[] parts = data.replace("0x", "").split("\\s+");
            byte[] bytes = new byte[parts.length];
            for (int i = 0; i < parts.length; i++) {
                bytes[i] = (byte) Integer.parseInt(parts[i], 16);
            }
            // AA 55 <type> <len> ...
            if (bytes.length < 6) return false;
            if (bytes[0] != (byte) 0xAA || bytes[1] != (byte) 0x55) return false;

            byte type = bytes[2];
            byte len = bytes[3];
            if (bytes.length < 4 + (len & 0xFF)) return false;

            if (type == 0x10 && (len & 0xFF) == 8) {          // DRC
                int idx = 4;
                final int threshold = (bytes[idx++] & 0xFF) | ((bytes[idx++] & 0xFF) << 8);
                final int ratio     = (bytes[idx++] & 0xFF) | ((bytes[idx++] & 0xFF) << 8);
                final int attack    = (bytes[idx++] & 0xFF) | ((bytes[idx++] & 0xFF) << 8);
                final int release   = (bytes[idx++] & 0xFF) | ((bytes[idx++] & 0xFF) << 8);
                runOnUiThread(() -> applyLegacyValues("drc",
                        new String[]{"threshold", "ratio", "attack", "release"},
                        new int[]{threshold / 100, ratio / 10, attack, release}));
                return true;
            } else if (type == 0x11 && (len & 0xFF) == 3) {     // Reverb
                int idx = 4;
                final int room  = bytes[idx++] & 0xFF;
                final int damp  = bytes[idx++] & 0xFF;
                final int wet   = bytes[idx++] & 0xFF;
                runOnUiThread(() -> applyLegacyValues("reverb",
                        new String[]{"room", "damp", "wet"},
                        new int[]{room, damp, wet}));
                return true;
            }
        } catch (Exception e) {
            Log.w(TAG, "legacy binary parse failed", e);
        }
        return false;
    }

    private void applyLegacyValues(String nodeName, String[] keys, int[] values) {
        Map<String, VerticalSeekBar> bars  = effectSeekBars.get(nodeName);
        Map<String, TextView>        texts = effectValueTexts.get(nodeName);
        if (bars == null || texts == null) return;

        applyingState = true;
        try {
            for (FxDef def : FX_DEFS) {
                if (!def.nodeName.equals(nodeName)) continue;
                for (int i = 0; i < keys.length && i < values.length; i++) {
                    int min = def.ranges[i * 2];
                    int max = def.ranges[i * 2 + 1];
                    int v = Math.max(min, Math.min(max, values[i]));
                    VerticalSeekBar bar = bars.get(keys[i]);
                    TextView tv = texts.get(keys[i]);
                    if (bar != null) bar.setProgress(v - min);
                    if (tv != null) tv.setText(String.valueOf(v));
                }
            }
        } finally {
            applyingState = false;
        }
    }
}
