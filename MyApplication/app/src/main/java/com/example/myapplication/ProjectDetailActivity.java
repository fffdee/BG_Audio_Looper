package com.example.myapplication;

import android.app.AlertDialog;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import java.util.Timer;
import java.util.TimerTask;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import com.bumptech.glide.Glide;
import com.bumptech.glide.load.resource.bitmap.DownsampleStrategy;
import com.bumptech.glide.request.target.Target;

import java.io.File;

public class ProjectDetailActivity extends AppCompatActivity {

    private ImageView ivMergedImage;
    private Button btnPlayPause;
    private ScrollView scrollView;
    private SeekBar sbSpeed;
    private String mergedImagePath;
    private Timer autoScrollTimer;
    private boolean isPlaying = false;
    private final Handler handler = new Handler(Looper.getMainLooper());
    // 速度级别：0=慢速，1=中速（默认），2=快速
    private int currentSpeedLevel = 1;
    // 不同速度对应的滚动步长（像素/次）和间隔（毫秒）
    private final int[][] speedConfig = {
        {2, 60},   // 慢速：步长2，间隔60ms
        {3, 40},   // 中速：步长3，间隔40ms（默认）
        {5, 20}    // 快速：步长5，间隔20ms
    };
    // 当前使用的速度参数
    private int scrollStep = 3; // 默认步长
    private int scrollDelay = 40; // 默认间隔（毫秒）

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_project_detail);
        mergedImagePath = getIntent().getStringExtra("merged_image_path");
        if (mergedImagePath == null || !new java.io.File(mergedImagePath).exists()) {
            Toast.makeText(this, "图片不存在", Toast.LENGTH_SHORT).show();
            finish();
            return;
        }
        initViews();
        loadMergedImage();
        initSpeedControl();
    }

    private void initViews() {
        scrollView = findViewById(R.id.scroll_view);
        ivMergedImage = findViewById(R.id.iv_merged_image);
        btnPlayPause = findViewById(R.id.btn_play_pause);
        sbSpeed = findViewById(R.id.sb_speed);
        btnPlayPause.setOnClickListener(v -> togglePlay());
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

    private void loadMergedImage() {
        int screenWidth = getResources().getDisplayMetrics().widthPixels;
        Glide.with(this)
                .load(new java.io.File(mergedImagePath))
                .override(screenWidth, Integer.MAX_VALUE)
                .fitCenter()
                .into(ivMergedImage);
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
                    int currentScrollY = scrollView.getScrollY();
                    int imageTotalHeight = ivMergedImage.getHeight();
                    int scrollViewHeight = scrollView.getHeight();
                    if (currentScrollY + scrollViewHeight >= imageTotalHeight) {
                        stopAutoScroll();
                        isPlaying = false;
                        btnPlayPause.setText("播放");
                    } else {
                        scrollView.scrollBy(0, scrollStep);
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
