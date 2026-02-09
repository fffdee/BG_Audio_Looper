package com.example.myapplication;

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import java.util.ArrayList;
import java.util.Timer;
import java.util.TimerTask;
import android.widget.Button;
import android.widget.SeekBar;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import java.util.List;

public class ProjectDetailActivity extends AppCompatActivity {
    private static final String TAG = "ProjectDetailActivity";

    private RecyclerView rvImages;
    private Button btnPlayPause;
    private SeekBar sbSpeed;
    private LinearLayoutManager layoutManager;
    private List<String> imagePaths;
    private Timer autoScrollTimer;
    private boolean isPlaying = false;
    private final Handler handler = new Handler(Looper.getMainLooper());
    
    // 滚动参数
    private int scrollStep = 3; // 默认步长（像素）
    private int scrollDelay = 40; // 默认间隔（毫秒）

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_project_detail);
        
        Log.d(TAG, "===== ProjectDetailActivity onCreate =====");
        
        // 获取图片路径列表
        imagePaths = getIntent().getStringArrayListExtra("image_paths");
        
        Log.d(TAG, "imagePaths 是否为null: " + (imagePaths == null));
        
        if (imagePaths == null || imagePaths.isEmpty()) {
            Log.e(TAG, "错误：未找到图片列表");
            if (imagePaths == null) {
                Log.e(TAG, "imagePaths 为 null");
            } else {
                Log.e(TAG, "imagePaths 为空列表，大小: " + imagePaths.size());
            }
            Toast.makeText(this, "未找到图片列表", Toast.LENGTH_SHORT).show();
            finish();
            return;
        }
        
        Log.d(TAG, "加载项目，共 " + imagePaths.size() + " 张图片");
        for (int i = 0; i < imagePaths.size(); i++) {
            Log.d(TAG, "  图片 " + (i+1) + ": " + imagePaths.get(i));
        }
        
        initViews();
        setupRecyclerView();
        initSpeedControl();
    }

    private void initViews() {
        rvImages = findViewById(R.id.rv_images);
        btnPlayPause = findViewById(R.id.btn_play_pause);
        sbSpeed = findViewById(R.id.sb_speed);
        btnPlayPause.setOnClickListener(v -> togglePlay());
    }
    
    private void setupRecyclerView() {
        layoutManager = new LinearLayoutManager(this);
        rvImages.setLayoutManager(layoutManager);
        
        int screenWidth = getResources().getDisplayMetrics().widthPixels;
        ImagePlayAdapter adapter = new ImagePlayAdapter(imagePaths, screenWidth);
        rvImages.setAdapter(adapter);
        
        Log.d(TAG, "RecyclerView设置完成");
    }

    // 初始化速度调节（SeekBar实时控制）
    private void initSpeedControl() {
        sbSpeed.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (!fromUser) return;
                scrollStep = 1 + (progress / 15); // 进度每增加15，步长+1
                scrollDelay = 100 - (progress); // 进度每增加1，间隔-1（最低20ms）
                scrollDelay = Math.max(scrollDelay, 20);
                if (isPlaying) {
                    startAutoScroll();
                }
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });
    }



    private void togglePlay() {
        isPlaying = !isPlaying;
        if (isPlaying) {
            btnPlayPause.setText("暂停");
            startAutoScroll();
        } else {
            btnPlayPause.setText("播放");
            stopAutoScroll();
        }
    }

    private void startAutoScroll() {
        stopAutoScroll();
        autoScrollTimer = new Timer();
        autoScrollTimer.schedule(new TimerTask() {
            @Override
            public void run() {
                handler.post(() -> {
                    // 使用smoothScrollBy实现平滑滚动
                    rvImages.smoothScrollBy(0, scrollStep);
                    
                    // 检查是否滚动到底部
                    if (!rvImages.canScrollVertically(1)) {
                        stopAutoScroll();
                        isPlaying = false;
                        btnPlayPause.setText("播放");
                        Log.d(TAG, "已滚动到底部");
                    }
                });
            }
        }, 0, scrollDelay);
    }

    private void stopAutoScroll() {
        if (autoScrollTimer != null) {
            autoScrollTimer.cancel();
            autoScrollTimer = null;
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        stopAutoScroll();
        handler.removeCallbacksAndMessages(null);
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (isPlaying) {
            stopAutoScroll();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (isPlaying) {
            startAutoScroll();
        }
    }
}
