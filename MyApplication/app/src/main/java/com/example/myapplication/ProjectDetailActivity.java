package com.example.myapplication;

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Timer;
import java.util.TimerTask;
import android.widget.Button;
import android.widget.SeekBar;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.ItemTouchHelper;
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
    private ItemTouchHelper itemTouchHelper;
    private ImagePlayAdapter adapter;
    
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
        adapter = new ImagePlayAdapter(imagePaths, screenWidth);
        rvImages.setAdapter(adapter);
        
        // 设置拖拽功能
        setupItemTouchHelper();
        
        Log.d(TAG, "RecyclerView设置完成");
    }

    /**
     * 设置拖拽排序功能
     */
    private void setupItemTouchHelper() {
        ItemTouchHelper.Callback callback = new ItemTouchHelper.Callback() {
            @Override
            public int getMovementFlags(@NonNull RecyclerView recyclerView, @NonNull RecyclerView.ViewHolder viewHolder) {
                // 只在停止播放时允许拖拽
                if (isPlaying) {
                    return makeMovementFlags(0, 0);
                }
                int dragFlags = ItemTouchHelper.UP | ItemTouchHelper.DOWN;
                return makeMovementFlags(dragFlags, 0);
            }

            @Override
            public boolean onMove(@NonNull RecyclerView recyclerView, 
                                @NonNull RecyclerView.ViewHolder viewHolder, 
                                @NonNull RecyclerView.ViewHolder target) {
                int fromPosition = viewHolder.getAdapterPosition();
                int toPosition = target.getAdapterPosition();
                
                // 交换数据
                Collections.swap(imagePaths, fromPosition, toPosition);
                adapter.notifyItemMoved(fromPosition, toPosition);
                
                return true;
            }

            @Override
            public void onSwiped(@NonNull RecyclerView.ViewHolder viewHolder, int direction) {
                // 不支持滑动删除
            }

            @Override
            public boolean isLongPressDragEnabled() {
                // 只在停止播放时允许长按拖拽
                return !isPlaying;
            }

            @Override
            public void onSelectedChanged(RecyclerView.ViewHolder viewHolder, int actionState) {
                super.onSelectedChanged(viewHolder, actionState);
                if (actionState == ItemTouchHelper.ACTION_STATE_DRAG) {
                    // 拖拽开始时的提示
                    viewHolder.itemView.setAlpha(0.7f);
                }
            }

            @Override
            public void clearView(@NonNull RecyclerView recyclerView, @NonNull RecyclerView.ViewHolder viewHolder) {
                super.clearView(recyclerView, viewHolder);
                // 拖拽结束时恢复透明度
                viewHolder.itemView.setAlpha(1.0f);
            }
        };
        
        itemTouchHelper = new ItemTouchHelper(callback);
        itemTouchHelper.attachToRecyclerView(rvImages);
    }

    // 初始化速度调节（SeekBar实时控制）
    private void initSpeedControl() {
        sbSpeed.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (!fromUser) return;
                scrollStep = 1 + (progress / 10); // 进度每增加10，步长+1（最大速度提升1.5倍）
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
            Toast.makeText(this, "播放中，无法调整图片顺序", Toast.LENGTH_SHORT).show();
        } else {
            btnPlayPause.setText("播放");
            stopAutoScroll();
            Toast.makeText(this, "已停止，可长按图片调整顺序", Toast.LENGTH_SHORT).show();
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
