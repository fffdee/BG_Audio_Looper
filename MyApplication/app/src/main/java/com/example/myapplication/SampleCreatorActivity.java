package com.example.myapplication;

import android.app.Activity;
import android.content.ContentUris;
import android.content.Intent;
import android.database.Cursor;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Bundle;
import android.provider.MediaStore;
import android.text.InputType;
import android.view.Gravity;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.drawerlayout.widget.DrawerLayout;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * 采样创作界面 — DAW 时间轨模式
 * - 动态轨道列表，每轨道可添加多个 WAV 片段
 * - 片段放置在时间轴指定位置，支持拖拽和剪辑
 * - 多轨道混音播放与导出
 */
public class SampleCreatorActivity extends BaseActivity {

    private static final int SAMPLE_RATE = 48000;

    /** 时间轴上的一个采样片段 */
    static class Clip {
        String fileName;
        android.net.Uri fileUri;
        /** 原始文件总时长，毫秒 */
        long durationMs;
        /** 片段在时间轴上的起始位置，毫秒（可拖拽改变） */
        long startTimeMs = 0;
        /** 从原始文件裁剪的起点，毫秒（默认0） */
        long trimStartMs = 0;
        /** 从原始文件裁剪的终点，毫秒（默认等于 durationMs） */
        long trimEndMs;

        Clip(String fileName, android.net.Uri fileUri, long durationMs) {
            this.fileName = fileName;
            this.fileUri = fileUri;
            this.durationMs = durationMs;
            this.trimEndMs = durationMs;
        }

        /** 实际播放时长（trimEnd - trimStart） */
        long playDurationMs() {
            return trimEndMs - trimStartMs;
        }

        /** 在时间轴上占据的结束位置，毫秒 */
        long endTimeMs() {
            return startTimeMs + playDurationMs();
        }
    }

    /** 一条轨道 */
    static class Track {
        String name;              // 轨道名称
        int volume = 80;          // 0-100
        boolean muted = false;
        List<Clip> clips = new ArrayList<>();
        int color;                // 轨道颜色（clip块颜色）

        Track(String name, int color) {
            this.name = name;
            this.color = color;
        }
    }

    /** 轨道颜色池 */
    private static final int[] TRACK_COLORS = {
            0xFF4E8BF5, // 蓝
            0xFF4EC07A, // 绿
            0xFFF5A623, // 橙
            0xFFE05C5C, // 红
            0xFF9B59B6, // 紫
            0xFF1ABC9C, // 青
            0xFFF39C12, // 黄
            0xFF2ECC71, // 亮绿
    };

    private List<Track> tracks = new ArrayList<>();
    private TimelineView timelineView;
    /** 单声道混音 */
    private final boolean isStereoMix = false;
    private LinearLayout trackHeadersContainer;
    private DrawerLayout drawerLayout;
    private TextView tvBpm, tvBeats, tvCurrentProject;

    /** BPM 和节拍设置 */
    private int bpm = 120;
    private int beatsPerBar = 4;

    private boolean isPlaying = false;
    /** 播放/停止合一按钮 */
    private ImageButton btnPlayStop;
    private AudioTrack audioTrack;
    private Thread playbackThread;

    /** 当前工程名（保存/加载后更新，用于界面提示） */
    private String currentProjectName = null;

    /** 启动项目列表界面并接收返回的项目名 */
    private final ActivityResultLauncher<Intent> projectListLauncher =
            registerForActivityResult(new ActivityResultContracts.StartActivityForResult(), result -> {
                if (result.getResultCode() == Activity.RESULT_OK && result.getData() != null) {
                    String projectName = result.getData().getStringExtra(SampleProjectListActivity.EXTRA_PROJECT_NAME);
                    if (projectName != null) {
                        loadProjectIntoActivity(projectName);
                    }
                }
            });

    @Override
    protected String getToolbarTitle() {
        return "采样创作";
    }

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_sample_creator);
        setupBaseToolbar(true);
        
        // 初始化项目管理器
        SampleProjectManager.init(this);

        // 初始化4条默认轨道
        for (int i = 0; i < 4; i++) {
            tracks.add(new Track("轨道 " + (i + 1), TRACK_COLORS[i % TRACK_COLORS.length]));
        }

        drawerLayout = findViewById(R.id.drawer_layout);
        tvBpm        = findViewById(R.id.tv_sc_bpm);
        tvBeats      = findViewById(R.id.tv_sc_beats);
        tvCurrentProject = findViewById(R.id.tv_current_project);

        // 初始化UI
        initTracks();
        initButtons();
    }

    private void initTracks() {
        timelineView = findViewById(R.id.timeline_view);
        trackHeadersContainer = findViewById(R.id.track_headers_container);

        timelineView.setTracks(tracks);
        timelineView.setTempo(bpm, beatsPerBar);
        timelineView.setOnClipEditListener(new TimelineView.OnClipEditListener() {
            @Override
            public void onClipMoved() {
                timelineView.requestLayout();
                timelineView.invalidate();
            }

            @Override
            public void onClipLongPress(Clip clip, int trackIdx) {
                showClipTrimDialog(clip, trackIdx);
            }
        });
        timelineView.setOnTrimListener(clip -> {
            // Trim 手柄松手后刷新视图
            timelineView.requestLayout();
            timelineView.invalidate();
        });

        renderTrackHeaders();
    }

    /** 动态渲染左侧轨道头面板 */
    private void renderTrackHeaders() {
        trackHeadersContainer.removeAllViews();
        for (int i = 0; i < tracks.size(); i++) {
            final int trackIdx = i;
            Track track = tracks.get(i);

            // 创建轨道头卡片（高度与 TimelineView 的 TRACK_HEIGHT 对齐，约 80dp）
            LinearLayout header = new LinearLayout(this);
            header.setOrientation(LinearLayout.VERTICAL);
            header.setBackgroundColor(trackIdx % 2 == 0 ? 0xFF1E1E2E : 0xFF252535);
            int trackH = (int)(80 * getResources().getDisplayMetrics().density);
            header.setLayoutParams(new LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT, trackH));
            header.setPadding(8, 4, 8, 4);

            // 长按删除轨道
            header.setOnLongClickListener(v -> {
                showDeleteTrackDialog(trackIdx);
                return true;
            });

            // 轨道名称 TextView
            TextView tvName = new TextView(this);
            tvName.setText(track.name);
            tvName.setTextColor(0xFFFFFFFF);
            tvName.setTextSize(12);
            tvName.setMaxLines(1);
            tvName.setEllipsize(android.text.TextUtils.TruncateAt.END);
            header.addView(tvName);

            // 静音按钮
            ImageButton btnMute = new ImageButton(this);
            btnMute.setImageResource(track.muted
                    ? android.R.drawable.ic_lock_silent_mode_off
                    : android.R.drawable.ic_lock_silent_mode);
            btnMute.setBackground(null);
            btnMute.setAlpha(track.muted ? 0.5f : 1.0f);
            btnMute.setOnClickListener(v -> {
                track.muted = !track.muted;
                btnMute.setAlpha(track.muted ? 0.5f : 1.0f);
                btnMute.setImageResource(track.muted
                        ? android.R.drawable.ic_lock_silent_mode_off
                        : android.R.drawable.ic_lock_silent_mode);
            });
            LinearLayout.LayoutParams muteLp = new LinearLayout.LayoutParams(32, 32);
            muteLp.setMargins(0, 4, 0, 0);
            btnMute.setLayoutParams(muteLp);
            header.addView(btnMute);

            // 音量 SeekBar（小型）+ 添加按钮水平排列
            LinearLayout bottomRow = new LinearLayout(this);
            bottomRow.setOrientation(LinearLayout.HORIZONTAL);
            bottomRow.setGravity(android.view.Gravity.CENTER_VERTICAL);
            LinearLayout.LayoutParams rowLp = new LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
            rowLp.setMargins(0, 4, 0, 0);
            bottomRow.setLayoutParams(rowLp);

            SeekBar seekBar = new SeekBar(this);
            seekBar.setMax(100);
            seekBar.setProgress(track.volume);
            LinearLayout.LayoutParams seekLp = new LinearLayout.LayoutParams(
                    0, LinearLayout.LayoutParams.WRAP_CONTENT, 1);
            seekBar.setLayoutParams(seekLp);
            seekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                @Override public void onProgressChanged(SeekBar sb, int progress, boolean fromUser) {
                    if (fromUser) track.volume = progress;
                }
                @Override public void onStartTrackingTouch(SeekBar sb) {}
                @Override public void onStopTrackingTouch(SeekBar sb) {}
            });
            bottomRow.addView(seekBar);

            // 添加 clip 按钮
            ImageButton btnAdd = new ImageButton(this);
            btnAdd.setImageResource(android.R.drawable.ic_input_add);
            btnAdd.setBackground(null);
            btnAdd.setOnClickListener(v -> selectWavFile(trackIdx));
            LinearLayout.LayoutParams addLp = new LinearLayout.LayoutParams(28, 28);
            addLp.setMarginStart(4);
            btnAdd.setLayoutParams(addLp);
            bottomRow.addView(btnAdd);

            header.addView(bottomRow);
            trackHeadersContainer.addView(header);
        }

        // 底部添加"新建轨道"按钮
        Button btnAddTrack = new Button(this);
        btnAddTrack.setText("+ 新建轨道");
        btnAddTrack.setTextSize(12);
        btnAddTrack.setTextColor(0xFF4EC07A);
        btnAddTrack.setBackgroundColor(0xFF2A2A3A);
        LinearLayout.LayoutParams btnLp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, (int)(48 * getResources().getDisplayMetrics().density));
        btnLp.setMargins(0, 8, 0, 0);
        btnAddTrack.setLayoutParams(btnLp);
        btnAddTrack.setOnClickListener(v -> addNewTrack());
        trackHeadersContainer.addView(btnAddTrack);
    }

    private void initButtons() {
        btnPlayStop  = findViewById(R.id.btn_sc_play);
        ImageButton btnExport  = findViewById(R.id.btn_sc_export);
        ImageButton btnSave    = findViewById(R.id.btn_sc_save);
        ImageButton btnProjects = findViewById(R.id.btn_sc_projects);

        btnPlayStop.setOnClickListener(v -> togglePlayback());
        btnExport.setOnClickListener(v -> exportMix());
        btnSave.setOnClickListener(v -> showSaveProjectDialog());
        btnProjects.setOnClickListener(v -> {
            if (drawerLayout != null) drawerLayout.openDrawer(Gravity.START);
        });

        // BPM 点击
        if (tvBpm != null) {
            tvBpm.setOnClickListener(v -> showTempoDialog());
        }
        // 拍号点击
        if (tvBeats != null) {
            tvBeats.setOnClickListener(v -> showBeatsDialog());
        }

        // 抽屉内按钮
        Button btnDrawerSave = findViewById(R.id.btn_drawer_save);
        Button btnDrawerLoad = findViewById(R.id.btn_drawer_load);
        Button btnDrawerNew  = findViewById(R.id.btn_drawer_new);
        if (btnDrawerSave != null) btnDrawerSave.setOnClickListener(v -> {
            if (drawerLayout != null) drawerLayout.closeDrawers();
            showSaveProjectDialog();
        });
        if (btnDrawerLoad != null) btnDrawerLoad.setOnClickListener(v -> {
            if (drawerLayout != null) drawerLayout.closeDrawers();
            openProjectList();
        });
        if (btnDrawerNew != null) btnDrawerNew.setOnClickListener(v -> {
            if (drawerLayout != null) drawerLayout.closeDrawers();
            showNewProjectDialog();
        });
    }

    private void showTempoDialog() {
        EditText et = new EditText(this);
        et.setInputType(InputType.TYPE_CLASS_NUMBER);
        et.setText(String.valueOf(bpm));
        et.selectAll();
        new AlertDialog.Builder(this)
                .setTitle("设置 BPM")
                .setView(et)
                .setPositiveButton("确定", (d, w) -> {
                    try {
                        int v = Integer.parseInt(et.getText().toString().trim());
                        if (v >= 20 && v <= 400) {
                            bpm = v;
                            tvBpm.setText(bpm + " BPM");
                            timelineView.setTempo(bpm, beatsPerBar);
                        } else {
                            Toast.makeText(this, "BPM 范围 20-400", Toast.LENGTH_SHORT).show();
                        }
                    } catch (NumberFormatException e) { /* ignore */ }
                })
                .setNegativeButton("取消", null)
                .show();
    }

    private void showBeatsDialog() {
        String[] items = {"2/4", "3/4", "4/4", "5/4", "6/8", "7/8"};
        int[] beats    = {2,    3,     4,     5,     6,     7};
        String current = beatsPerBar + "/4";
        new AlertDialog.Builder(this)
                .setTitle("设置拍号")
                .setItems(items, (d, which) -> {
                    beatsPerBar = beats[which];
                    tvBeats.setText(items[which]);
                    timelineView.setTempo(bpm, beatsPerBar);
                })
                .show();
    }

    private void showNewProjectDialog() {
        new AlertDialog.Builder(this)
                .setTitle("新建工程")
                .setMessage("这将清空当前所有轨道和片段，是否继续？")
                .setPositiveButton("确定", (d, w) -> {
                    stopPlayback();
                    tracks.clear();
                    for (int i = 0; i < 4; i++) {
                        tracks.add(new Track("轨道 " + (i + 1), TRACK_COLORS[i % TRACK_COLORS.length]));
                    }
                    currentProjectName = null;
                    updateProjectLabel();
                    renderTrackHeaders();
                    timelineView.setTracks(tracks);
                    timelineView.requestLayout();
                    timelineView.invalidate();
                })
                .setNegativeButton("取消", null)
                .show();
    }

    private void updateProjectLabel() {
        if (tvCurrentProject != null) {
            tvCurrentProject.setText("当前: " + (currentProjectName != null ? currentProjectName : "未保存"));
        }
    }

    // ==================== 项目保存 ====================

    private void showSaveProjectDialog() {
        EditText etName = new EditText(this);
        etName.setInputType(InputType.TYPE_CLASS_TEXT);
        etName.setHint("输入项目名称");
        if (currentProjectName != null) {
            etName.setText(currentProjectName);
            etName.selectAll();
        }

        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(60, 20, 60, 10);
        layout.addView(etName);

        new AlertDialog.Builder(this)
                .setTitle("保存项目")
                .setMessage("项目文件夹和 WAV 采样将保存到\nMusic/BanBox/Projects/")
                .setView(layout)
                .setPositiveButton("保存", (dialog, which) -> {
                    String name = etName.getText().toString().trim();
                    if (!SampleProjectManager.isValidProjectName(name)) {
                        Toast.makeText(this, "项目名不合法，请勿包含特殊字符", Toast.LENGTH_SHORT).show();
                        return;
                    }
                    doSaveProject(name);
                })
                .setNegativeButton("取消", null)
                .show();
    }

    private void doSaveProject(String name) {
        Toast.makeText(this, "正在保存...", Toast.LENGTH_SHORT).show();
        new Thread(() -> {
            File dir = SampleProjectManager.saveProject(this, name, tracks);
            runOnUiThread(() -> {
                if (dir != null) {
                    currentProjectName = name;
                    updateProjectLabel();
                    Toast.makeText(this, "项目已保存: " + name, Toast.LENGTH_LONG).show();
                } else {
                    Toast.makeText(this, "保存失败，请检查存储权限", Toast.LENGTH_SHORT).show();
                }
            });
        }).start();
    }

    // ==================== 项目列表 ====================

    private void openProjectList() {
        Intent intent = new Intent(this, SampleProjectListActivity.class);
        projectListLauncher.launch(intent);
    }

    // ==================== 加载项目 ====================

    private void loadProjectIntoActivity(String projectName) {
        Toast.makeText(this, "正在加载项目...", Toast.LENGTH_SHORT).show();
        new Thread(() -> {
            List<SampleCreatorActivity.Track> loaded =
                    SampleProjectManager.loadProject(projectName);
            runOnUiThread(() -> {
                if (loaded == null || loaded.isEmpty()) {
                    Toast.makeText(this, "加载失败或项目为空", Toast.LENGTH_SHORT).show();
                    return;
                }
                stopPlayback();
                tracks.clear();
                tracks.addAll(loaded);
                currentProjectName = projectName;
                updateProjectLabel();
                renderTrackHeaders();
                timelineView.setTracks(tracks);
                timelineView.requestLayout();
                timelineView.invalidate();
                Toast.makeText(this, "已加载: " + projectName, Toast.LENGTH_LONG).show();
            });
        }).start();
    }

    private void toggleMute(int trackIdx) {
        Track t = tracks.get(trackIdx);
        t.muted = !t.muted;
    }

    /** Step 7: 添加新轨道 */
    private void addNewTrack() {
        int newIdx = tracks.size() + 1;
        int colorIdx = tracks.size() % TRACK_COLORS.length;
        Track newTrack = new Track("轨道 " + newIdx, TRACK_COLORS[colorIdx]);
        tracks.add(newTrack);
        
        // 刷新界面
        renderTrackHeaders();
        timelineView.setTracks(tracks);
        timelineView.requestLayout();
        timelineView.invalidate();
        
        Toast.makeText(this, "已添加 " + newTrack.name, Toast.LENGTH_SHORT).show();
    }

    /** Step 7: 显示删除轨道确认对话框 */
    private void showDeleteTrackDialog(int trackIdx) {
        if (tracks.size() <= 1) {
            Toast.makeText(this, "至少需要保留一条轨道", Toast.LENGTH_SHORT).show();
            return;
        }

        Track track = tracks.get(trackIdx);
        String message = track.clips.isEmpty()
                ? "确定删除 " + track.name + " 吗？"
                : "确定删除 " + track.name + " 及其所有片段吗？";

        new AlertDialog.Builder(this)
                .setTitle("删除轨道")
                .setMessage(message)
                .setPositiveButton("删除", (dialog, which) -> {
                    tracks.remove(trackIdx);
                    renderTrackHeaders();
                    timelineView.setTracks(tracks);
                    timelineView.requestLayout();
                    timelineView.invalidate();
                    Toast.makeText(this, "已删除 " + track.name, Toast.LENGTH_SHORT).show();
                })
                .setNegativeButton("取消", null)
                .show();
    }

    /** Step 6: 显示 Clip 剪辑对话框 */
    private void showClipTrimDialog(Clip clip, int trackIdx) {
        Track track = tracks.get(trackIdx);
        
        // 创建对话框布局
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(40, 20, 40, 20);

        // 原始时长
        TextView tvOriginal = new TextView(this);
        tvOriginal.setText(String.format(Locale.US, "原始时长: %.2f 秒", clip.durationMs / 1000.0));
        tvOriginal.setTextSize(14);
        layout.addView(tvOriginal);

        // 裁剪起点
        TextView tvTrimStart = new TextView(this);
        tvTrimStart.setText(String.format(Locale.US, "裁剪起点: %.2f 秒", clip.trimStartMs / 1000.0));
        tvTrimStart.setTextSize(12);
        LinearLayout.LayoutParams lp1 = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        lp1.setMargins(0, 20, 0, 0);
        tvTrimStart.setLayoutParams(lp1);
        layout.addView(tvTrimStart);

        SeekBar seekTrimStart = new SeekBar(this);
        seekTrimStart.setMax((int)clip.durationMs);
        seekTrimStart.setProgress((int)clip.trimStartMs);
        layout.addView(seekTrimStart);

        // 裁剪终点
        TextView tvTrimEnd = new TextView(this);
        tvTrimEnd.setText(String.format(Locale.US, "裁剪终点: %.2f 秒", clip.trimEndMs / 1000.0));
        tvTrimEnd.setTextSize(12);
        LinearLayout.LayoutParams lp2 = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        lp2.setMargins(0, 20, 0, 0);
        tvTrimEnd.setLayoutParams(lp2);
        layout.addView(tvTrimEnd);

        SeekBar seekTrimEnd = new SeekBar(this);
        seekTrimEnd.setMax((int)clip.durationMs);
        seekTrimEnd.setProgress((int)clip.trimEndMs);
        layout.addView(seekTrimEnd);

        // 实际播放时长
        TextView tvPlayDuration = new TextView(this);
        tvPlayDuration.setText(String.format(Locale.US, "播放时长: %.2f 秒", clip.playDurationMs() / 1000.0));
        tvPlayDuration.setTextSize(14);
        tvPlayDuration.setTextColor(0xFF4EC07A);
        LinearLayout.LayoutParams lp3 = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        lp3.setMargins(0, 20, 0, 0);
        tvPlayDuration.setLayoutParams(lp3);
        layout.addView(tvPlayDuration);

        // SeekBar 监听器
        seekTrimStart.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar sb, int progress, boolean fromUser) {
                if (fromUser) {
                    clip.trimStartMs = progress;
                    if (clip.trimStartMs >= clip.trimEndMs) {
                        clip.trimEndMs = clip.trimStartMs + 100; // 至少保留 100ms
                        seekTrimEnd.setProgress((int)clip.trimEndMs);
                    }
                    tvTrimStart.setText(String.format(Locale.US, "裁剪起点: %.2f 秒", clip.trimStartMs / 1000.0));
                    tvPlayDuration.setText(String.format(Locale.US, "播放时长: %.2f 秒", clip.playDurationMs() / 1000.0));
                }
            }
            @Override public void onStartTrackingTouch(SeekBar sb) {}
            @Override public void onStopTrackingTouch(SeekBar sb) {}
        });

        seekTrimEnd.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar sb, int progress, boolean fromUser) {
                if (fromUser) {
                    clip.trimEndMs = progress;
                    if (clip.trimEndMs <= clip.trimStartMs) {
                        clip.trimStartMs = Math.max(0, clip.trimEndMs - 100);
                        seekTrimStart.setProgress((int)clip.trimStartMs);
                    }
                    tvTrimEnd.setText(String.format(Locale.US, "裁剪终点: %.2f 秒", clip.trimEndMs / 1000.0));
                    tvPlayDuration.setText(String.format(Locale.US, "播放时长: %.2f 秒", clip.playDurationMs() / 1000.0));
                }
            }
            @Override public void onStartTrackingTouch(SeekBar sb) {}
            @Override public void onStopTrackingTouch(SeekBar sb) {}
        });

        // 显示对话框
        new AlertDialog.Builder(this)
                .setTitle("剪辑片段: " + clip.fileName)
                .setView(layout)
                .setPositiveButton("完成", (dialog, which) -> {
                    timelineView.invalidate();
                    Toast.makeText(this, "已更新剪辑", Toast.LENGTH_SHORT).show();
                })
                .setNegativeButton("删除片段", (dialog, which) -> {
                    track.clips.remove(clip);
                    timelineView.setTracks(tracks);
                    timelineView.invalidate();
                    Toast.makeText(this, "已删除片段", Toast.LENGTH_SHORT).show();
                })
                .setNeutralButton("取消", null)
                .show();
    }

    private void selectWavFile(int trackIdx) {
        // 加载Music/BanBox目录中的WAV文件列表
        List<WavFile> wavFiles = scanWavFiles();
        if (wavFiles.isEmpty()) {
            Toast.makeText(this, "未找到WAV文件（请先导出采样）", Toast.LENGTH_SHORT).show();
            return;
        }

        CharSequence[] items = new CharSequence[wavFiles.size()];
        for (int i = 0; i < wavFiles.size(); i++) {
            items[i] = wavFiles.get(i).displayName;
        }

        final int track = trackIdx;
        final List<WavFile> files = wavFiles;
        new AlertDialog.Builder(this)
                .setTitle("选择WAV文件")
                .setItems(items, (dialog, which) -> {
                    WavFile selected = files.get(which);
                    addClipToTrack(track, selected);
                })
                .show();
    }

    private static class WavFile {
        String displayName;
        android.net.Uri uri;
        long durationMs;
    }

    private List<WavFile> scanWavFiles() {
        List<WavFile> result = new ArrayList<>();
        Cursor cursor = null;
        try {
            String[] projection = {
                    MediaStore.Audio.Media._ID,
                    MediaStore.Audio.Media.DISPLAY_NAME,
                    MediaStore.Audio.Media.DURATION
            };
            String selection = MediaStore.Audio.Media.RELATIVE_PATH + " LIKE ?";
            String[] args = {"%" + "BanBox" + "%"};
            cursor = getContentResolver().query(
                    MediaStore.Audio.Media.getContentUri(MediaStore.VOLUME_EXTERNAL),
                    projection, selection, args, null);

            if (cursor != null && cursor.moveToFirst()) {
                do {
                    long id = cursor.getLong(0);
                    String name = cursor.getString(1);
                    long duration = cursor.getLong(2);

                    if (name != null && name.toLowerCase(Locale.US).endsWith(".wav")) {
                        WavFile wf = new WavFile();
                        wf.displayName = name;
                        wf.uri = ContentUris.withAppendedId(
                                MediaStore.Audio.Media.getContentUri(MediaStore.VOLUME_EXTERNAL), id);
                        wf.durationMs = duration;
                        result.add(wf);
                    }
                } while (cursor.moveToNext());
            }
        } finally {
            if (cursor != null) cursor.close();
        }
        return result;
    }

    private void addClipToTrack(int trackIdx, WavFile wavFile) {
        Track track = tracks.get(trackIdx);
        // startTimeMs: 放置在该轨道最后一个 clip 之后
        long startMs = 0;
        for (Clip existing : track.clips) {
            startMs = Math.max(startMs, existing.endTimeMs());
        }
        Clip clip = new Clip(wavFile.displayName, wavFile.uri, wavFile.durationMs);
        clip.startTimeMs = startMs;
        track.clips.add(clip);
        
        // 刷新 TimelineView
        timelineView.setTracks(tracks);
        timelineView.invalidate();
        Toast.makeText(this, "已添加到" + track.name, Toast.LENGTH_SHORT).show();
    }

    /** Clip card 渲染将在 Step 2/3（TimelineView）中实现，此处存根 */
    private void renderClipCard(int trackIdx, Clip clip) {
        // TODO: replaced by TimelineView in Step 3
    }

    private void togglePlayback() {
        if (isPlaying) {
            stopPlayback();
        } else {
            startPlayback();
        }
    }

    private void startPlayback() {
        if (isPlaying) return;

        // 计算总长度
        long totalDurationMs = 0;
        for (Track track : tracks) {
            for (Clip clip : track.clips) {
                totalDurationMs = Math.max(totalDurationMs, clip.endTimeMs());
            }
        }

        if (totalDurationMs == 0) {
            Toast.makeText(this, "请先添加片段", Toast.LENGTH_SHORT).show();
            return;
        }

        stopPlayback();
        isPlaying = true;
        if (btnPlayStop != null) runOnUiThread(() -> btnPlayStop.setImageResource(R.drawable.ic_stop));

        runOnUiThread(() -> timelineView.setPlayheadMs(0));

        final long durationMs = totalDurationMs;
        playbackThread = new Thread(() -> {
            try {
                playAllTracks(durationMs);
            } catch (Exception e) {
                android.util.Log.e("SampleCreator", "Playback error", e);
                runOnUiThread(() -> Toast.makeText(this, "播放出错: " + e.getMessage(), Toast.LENGTH_SHORT).show());
            } finally {
                isPlaying = false;
                runOnUiThread(() -> {
                    timelineView.hidePlayhead();
                    if (btnPlayStop != null) btnPlayStop.setImageResource(R.drawable.ic_play);
                });
                if (audioTrack != null) {
                    audioTrack.stop();
                    audioTrack.release();
                    audioTrack = null;
                }
            }
        });
        playbackThread.start();
    }

    /**
     * 构建一个轨道的完整 PCM buffer（含 startTimeMs 位置信息）。
     * 每个 clip 放在其 startTimeMs 对应的位置，重叠时叠加。
     */
    private short[] buildTrackPcm(Track track) throws IOException {
        if (track.clips.isEmpty()) return new short[0];

        long trackEndMs = 0;
        for (Clip clip : track.clips) {
            trackEndMs = Math.max(trackEndMs, clip.endTimeMs());
        }
        int totalStereoSamples = (int)(trackEndMs * SAMPLE_RATE / 1000L) * 2;
        if (totalStereoSamples <= 0) return new short[0];

        short[] result = new short[totalStereoSamples];
        for (Clip clip : track.clips) {
            short[] clipPcm = loadClipPcm(clip);
            int startSample = (int)(clip.startTimeMs * SAMPLE_RATE / 1000L) * 2;
            int copyLen = Math.min(clipPcm.length, totalStereoSamples - startSample);
            if (startSample >= 0 && copyLen > 0) {
                for (int i = 0; i < copyLen; i++) {
                    int v = result[startSample + i] + clipPcm[i];
                    if (v > 32767) v = 32767;
                    if (v < -32768) v = -32768;
                    result[startSample + i] = (short) v;
                }
            }
        }
        return result;
    }

    /**
     * 混合所有轨道 PCM 并通过 AudioTrack 播放。
     */
    private void playAllTracks(long totalDurationMs) throws IOException {
        // 预加载所有轨道的 PCM（含时间偏移）
        int n = tracks.size();
        short[][] trackPcms = new short[n][];
        int maxLen = 0;
        for (int t = 0; t < n; t++) {
            Track track = tracks.get(t);
            if (track.muted || track.clips.isEmpty()) {
                trackPcms[t] = new short[0];
            } else {
                trackPcms[t] = buildTrackPcm(track);
            }
            maxLen = Math.max(maxLen, trackPcms[t].length);
        }

        if (maxLen == 0) return;

        // 混音：各轨道按 volume 缩放后叠加
        final boolean stereo = isStereoMix;
        short[] mixed = mixTracks(trackPcms, maxLen, stereo);

        // 创建 AudioTrack 输出（根据模式选择单/双声道）
        int channelConfig = stereo
                ? AudioFormat.CHANNEL_OUT_STEREO
                : AudioFormat.CHANNEL_OUT_MONO;
        int bufferSize = AudioTrack.getMinBufferSize(SAMPLE_RATE,
                channelConfig, AudioFormat.ENCODING_PCM_16BIT);
        bufferSize = Math.max(bufferSize, 4096);
        audioTrack = new AudioTrack(AudioManager.STREAM_MUSIC, SAMPLE_RATE,
                channelConfig, AudioFormat.ENCODING_PCM_16BIT,
                bufferSize, AudioTrack.MODE_STREAM);
        audioTrack.play();

        // 分块写入（blocking write），同时更新播放头
        // stereo: 每帧 2 shorts；mono: 每帧 1 short
        final int samplesPerFrame = stereo ? 2 : 1;
        int chunkSamples = bufferSize / 2;
        int offset = 0;
        while (isPlaying && offset < mixed.length) {
            int len = Math.min(chunkSamples, mixed.length - offset);
            audioTrack.write(mixed, offset, len);
            offset += len;

            final long currentMs = (long)((double) offset / samplesPerFrame * 1000.0 / SAMPLE_RATE);
            runOnUiThread(() -> timelineView.setPlayheadMs(currentMs));
        }
    }

    /**
     * 将多轨道 PCM 按 volume 混合。
     *
     * @param stereoMode true = 立体模式，输入输出均为 interleaved stereo [L0,R0,L1,R1,...]<br>
     *                  false = 单声道模式，将各轨道 L/R 平均后混为单声道信号，返回单声道数组
     */
    private short[] mixTracks(short[][] trackPcms, int maxLen, boolean stereoMode) {
        if (stereoMode) {
            // 立体：L/R 独立叠加（maxLen = nFrames * 2）
            float[] mixBuf = new float[maxLen];
            for (int t = 0; t < tracks.size(); t++) {
                if (trackPcms[t].length == 0) continue;
                float vol = tracks.get(t).volume / 100.0f;
                short[] pcm = trackPcms[t];
                for (int i = 0; i < pcm.length; i++) {
                    mixBuf[i] += pcm[i] * vol;
                }
            }
            short[] result = new short[maxLen];
            for (int i = 0; i < maxLen; i++) {
                int v = (int) mixBuf[i];
                if (v > 32767) v = 32767;
                if (v < -32768) v = -32768;
                result[i] = (short) v;
            }
            return result;
        } else {
            // 单声道：将各轨道的 L/R 平均合并为单 mono 信号（maxLen = nFrames * 2）
            int nFrames = maxLen / 2;
            float[] monoMix = new float[nFrames];
            for (int t = 0; t < tracks.size(); t++) {
                if (trackPcms[t].length == 0) continue;
                float vol = tracks.get(t).volume / 100.0f;
                short[] pcm = trackPcms[t];
                int frames = pcm.length / 2;
                for (int i = 0; i < frames; i++) {
                    // 将 interleaved 的 L/R 平均后叠加
                    monoMix[i] += (pcm[i * 2] + pcm[i * 2 + 1]) * 0.5f * vol;
                }
            }
            short[] result = new short[nFrames];
            for (int i = 0; i < nFrames; i++) {
                int v = (int) monoMix[i];
                if (v > 32767) v = 32767;
                if (v < -32768) v = -32768;
                result[i] = (short) v;
            }
            return result; // 纯单声道数组 [m0, m1, m2, ...]
        }
    }

    private void stopPlayback() {
        isPlaying = false;
        runOnUiThread(() -> { if (btnPlayStop != null) btnPlayStop.setImageResource(R.drawable.ic_play); });
        if (playbackThread != null) {
            try {
                playbackThread.join(2000);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
            playbackThread = null;
        }
        if (audioTrack != null) {
            try {
                audioTrack.stop();
                audioTrack.release();
            } catch (Exception e) {
                android.util.Log.e("SampleCreator", "Error stopping audio", e);
            }
            audioTrack = null;
        }
    }

    private void exportMix() {
        // 检查是否有 clip
        boolean hasClips = false;
        for (Track track : tracks) {
            if (!track.clips.isEmpty()) { hasClips = true; break; }
        }
        if (!hasClips) {
            Toast.makeText(this, "请先添加片段", Toast.LENGTH_SHORT).show();
            return;
        }

        Toast.makeText(this, "正在导出...", Toast.LENGTH_SHORT).show();

        new Thread(() -> {
            try {
                // 预加载并混合
                int n = tracks.size();
                short[][] trackPcms = new short[n][];
                int maxLen = 0;
                for (int t = 0; t < n; t++) {
                    Track tr = tracks.get(t);
                    if (tr.muted || tr.clips.isEmpty()) {
                        trackPcms[t] = new short[0];
                    } else {
                        trackPcms[t] = buildTrackPcm(tr);
                    }
                    maxLen = Math.max(maxLen, trackPcms[t].length);
                }

                final boolean stereo = isStereoMix;
                short[] mixed = mixTracks(trackPcms, maxLen, stereo);

                // 转为 byte[]（16-bit LE PCM）
                byte[] pcmBytes = new byte[mixed.length * 2];
                for (int i = 0; i < mixed.length; i++) {
                    pcmBytes[i * 2]     = (byte)(mixed[i] & 0xFF);
                    pcmBytes[i * 2 + 1] = (byte)((mixed[i] >> 8) & 0xFF);
                }

                // 构建 WAV 文件
                // 单声道模式：channels=1；立体模式：channels=2
                int channels = stereo ? 2 : 1;
                String suffix = stereo ? "_stereo" : "_mono";
                String fileName = "SampleCreator_" + new java.text.SimpleDateFormat(
                        "yyyyMMdd_HHmmss", java.util.Locale.US).format(new java.util.Date())
                        + suffix + ".wav";

                java.io.File dir = new java.io.File(getExternalFilesDir(null), "BanBox");
                if (!dir.exists()) dir.mkdirs();
                java.io.File outFile = new java.io.File(dir, fileName);

                java.io.FileOutputStream fos = new java.io.FileOutputStream(outFile);
                try {
                    writeWavHeader(fos, pcmBytes.length, channels, SAMPLE_RATE, 16);
                    fos.write(pcmBytes);
                } finally {
                    fos.close();
                }

                // 通知媒体库扫描
                android.media.MediaScannerConnection.scanFile(this,
                        new String[]{outFile.getAbsolutePath()}, null, null);

                runOnUiThread(() -> Toast.makeText(this,
                        "已导出: " + fileName, Toast.LENGTH_LONG).show());

            } catch (Exception e) {
                android.util.Log.e("SampleCreator", "Export error", e);
                runOnUiThread(() -> Toast.makeText(this,
                        "导出失败: " + e.getMessage(), Toast.LENGTH_SHORT).show());
            }
        }).start();
    }

    private static void writeWavHeader(java.io.OutputStream os, int pcmDataLen,
                                       int channels, int sampleRate, int bitsPerSample) throws IOException {
        int byteRate = sampleRate * channels * bitsPerSample / 8;
        int blockAlign = channels * bitsPerSample / 8;
        int totalSize = 36 + pcmDataLen;

        byte[] header = new byte[44];
        // RIFF chunk
        header[0]='R'; header[1]='I'; header[2]='F'; header[3]='F';
        writeInt32LE(header, 4, totalSize);
        header[8]='W'; header[9]='A'; header[10]='V'; header[11]='E';
        // fmt sub-chunk
        header[12]='f'; header[13]='m'; header[14]='t'; header[15]=' ';
        writeInt32LE(header, 16, 16); // subchunk1 size
        writeInt16LE(header, 20, 1);  // PCM format
        writeInt16LE(header, 22, channels);
        writeInt32LE(header, 24, sampleRate);
        writeInt32LE(header, 28, byteRate);
        writeInt16LE(header, 32, blockAlign);
        writeInt16LE(header, 34, bitsPerSample);
        // data sub-chunk
        header[36]='d'; header[37]='a'; header[38]='t'; header[39]='a';
        writeInt32LE(header, 40, pcmDataLen);

        os.write(header);
    }

    private static void writeInt32LE(byte[] buf, int off, int val) {
        buf[off]   = (byte)(val & 0xFF);
        buf[off+1] = (byte)((val >> 8) & 0xFF);
        buf[off+2] = (byte)((val >> 16) & 0xFF);
        buf[off+3] = (byte)((val >> 24) & 0xFF);
    }

    private static void writeInt16LE(byte[] buf, int off, int val) {
        buf[off]   = (byte)(val & 0xFF);
        buf[off+1] = (byte)((val >> 8) & 0xFF);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        stopPlayback();
    }

    // ========== WAV PCM 读取工具 ==========

    /** WAV 文件的关键信息 */
    private static class WavInfo {
        int channels;       // 1=mono, 2=stereo
        int sampleRate;
        int bitsPerSample;
        int dataOffset;     // PCM data 起始偏移（字节）
        int dataSize;       // PCM data 字节数
    }

    /**
     * 从 InputStream 解析 WAV 头，返回 PCM data 的偏移和大小。
     * 支持标准 RIFF WAV（可能有 LIST/INFO 等额外 chunk）。
     */
    private WavInfo parseWavHeader(java.io.InputStream is) throws IOException {
        byte[] buf = new byte[12];
        readFully(is, buf, 0, 12);
        // RIFF....WAVE
        if (buf[0] != 'R' || buf[1] != 'I' || buf[2] != 'F' || buf[3] != 'F')
            throw new IOException("Not a RIFF file");
        if (buf[8] != 'W' || buf[9] != 'A' || buf[10] != 'V' || buf[11] != 'E')
            throw new IOException("Not a WAVE file");

        WavInfo info = new WavInfo();
        int offset = 12;
        byte[] hdr = new byte[8];

        while (true) {
            readFully(is, hdr, 0, 8);
            String chunkId = new String(hdr, 0, 4, java.nio.charset.StandardCharsets.US_ASCII);
            int chunkSize = (hdr[4] & 0xFF) | ((hdr[5] & 0xFF) << 8)
                    | ((hdr[6] & 0xFF) << 16) | ((hdr[7] & 0xFF) << 24);
            offset += 8;

            if ("fmt ".equals(chunkId)) {
                byte[] fmt = new byte[chunkSize];
                readFully(is, fmt, 0, chunkSize);
                offset += chunkSize;
                info.channels = (fmt[2] & 0xFF) | ((fmt[3] & 0xFF) << 8);
                info.sampleRate = (fmt[4] & 0xFF) | ((fmt[5] & 0xFF) << 8)
                        | ((fmt[6] & 0xFF) << 16) | ((fmt[7] & 0xFF) << 24);
                info.bitsPerSample = (fmt[14] & 0xFF) | ((fmt[15] & 0xFF) << 8);
            } else if ("data".equals(chunkId)) {
                info.dataOffset = offset;
                info.dataSize = chunkSize;
                return info;
            } else {
                // 跳过未知 chunk
                long skipped = 0;
                while (skipped < chunkSize) {
                    long n = is.skip(chunkSize - skipped);
                    if (n <= 0) throw new IOException("Unexpected EOF in chunk " + chunkId);
                    skipped += n;
                }
                offset += chunkSize;
            }
        }
    }

    /**
     * 从一个 clip 的 URI 读取 PCM，应用 trim，转换为 16-bit stereo 48kHz short[].
     * 返回 interleaved stereo: [L0, R0, L1, R1, ...]
     */
    private short[] loadClipPcm(Clip clip) throws IOException {
        java.io.InputStream is = getContentResolver().openInputStream(clip.fileUri);
        if (is == null) throw new IOException("Cannot open " + clip.fileName);

        try {
            WavInfo info = parseWavHeader(is);
            byte[] rawPcm = new byte[info.dataSize];
            readFully(is, rawPcm, 0, info.dataSize);

            int bytesPerSample = info.bitsPerSample / 8;
            int frameCount = info.dataSize / (bytesPerSample * info.channels);

            // 转为 float L/R
            float[] left = new float[frameCount];
            float[] right = new float[frameCount];
            for (int i = 0; i < frameCount; i++) {
                int pos = i * bytesPerSample * info.channels;
                float l = sampleToFloat(rawPcm, pos, bytesPerSample);
                float r = (info.channels >= 2)
                        ? sampleToFloat(rawPcm, pos + bytesPerSample, bytesPerSample) : l;
                left[i] = l;
                right[i] = r;
            }

            // 重采样到 SAMPLE_RATE
            int outFrames;
            float[] outL, outR;
            if (info.sampleRate == SAMPLE_RATE) {
                outFrames = frameCount;
                outL = left;
                outR = right;
            } else {
                double ratio = (double) SAMPLE_RATE / info.sampleRate;
                outFrames = (int)(frameCount * ratio);
                outL = new float[outFrames];
                outR = new float[outFrames];
                for (int i = 0; i < outFrames; i++) {
                    double srcPos = i / ratio;
                    int idx = (int) srcPos;
                    float frac = (float)(srcPos - idx);
                    int next = Math.min(idx + 1, frameCount - 1);
                    outL[i] = left[idx] * (1 - frac) + left[next] * frac;
                    outR[i] = right[idx] * (1 - frac) + right[next] * frac;
                }
            }

            // 应用 trim
            int trimStartFrame = (int)(clip.trimStartMs * SAMPLE_RATE / 1000L);
            int trimEndFrame   = (int)(clip.trimEndMs   * SAMPLE_RATE / 1000L);
            trimStartFrame = Math.max(0, Math.min(trimStartFrame, outFrames));
            trimEndFrame   = Math.max(trimStartFrame, Math.min(trimEndFrame, outFrames));
            int playFrames = trimEndFrame - trimStartFrame;

            short[] result = new short[playFrames * 2];
            for (int i = 0; i < playFrames; i++) {
                result[i * 2]     = floatToShort(outL[trimStartFrame + i]);
                result[i * 2 + 1] = floatToShort(outR[trimStartFrame + i]);
            }
            return result;

        } finally {
            is.close();
        }
    }

    private static float sampleToFloat(byte[] buf, int offset, int bytesPerSample) {
        if (bytesPerSample == 2) {
            int val = (buf[offset] & 0xFF) | (buf[offset + 1] << 8);
            return val / 32768.0f;
        } else if (bytesPerSample == 3) {
            int val = (buf[offset] & 0xFF) | ((buf[offset + 1] & 0xFF) << 8) | (buf[offset + 2] << 16);
            return val / 8388608.0f;
        }
        // 8-bit unsigned
        return ((buf[offset] & 0xFF) - 128) / 128.0f;
    }

    private static short floatToShort(float v) {
        int i = (int)(v * 32767);
        if (i > 32767)  i = 32767;
        if (i < -32768) i = -32768;
        return (short) i;
    }

    private static void readFully(java.io.InputStream is, byte[] buf, int off, int len) throws IOException {
        int read = 0;
        while (read < len) {
            int n = is.read(buf, off + read, len - read);
            if (n < 0) throw new IOException("Unexpected EOF, need " + len + " got " + read);
            read += n;
        }
    }

    // ———— ID 映射方法已删除，将在 Step 3 (TimelineView) 中重新实现 ————
}
