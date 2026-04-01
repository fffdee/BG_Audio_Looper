package com.example.myapplication;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.MediaStore;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import java.util.ArrayList;
import java.util.List;
import java.util.Timer;
import java.util.TimerTask;

public class MainActivity extends BaseActivity implements View.OnClickListener {

    private static final int REQUEST_PERMISSION = 100;
    private ImageView ivImage;
    private Button btnSelect, btnPrev, btnPlay, btnNext, btnMetronome;
    private TextView tvStatus;
    private List<String> imagePaths = new ArrayList<>();
    private int currentPosition = 0;
    private Timer timer;
    private boolean isPlaying = false;
    private final Handler handler = new Handler(Looper.getMainLooper());

    // 正确定义 ActivityResultLauncher（类成员变量）
    private ActivityResultLauncher<Intent> imagePickerLauncher;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        initViews();
        // 初始化图片选择 launcher（在 onCreate 中）
        initImagePickerLauncher();
    }

    private void initViews() {
        ivImage = findViewById(R.id.iv_image);
        btnSelect = findViewById(R.id.btn_select);
        btnPrev = findViewById(R.id.btn_prev);
        btnPlay = findViewById(R.id.btn_play);
        btnNext = findViewById(R.id.btn_next);
        btnMetronome = findViewById(R.id.btn_metronome);
        tvStatus = findViewById(R.id.tv_status);

        btnSelect.setOnClickListener(this);
        btnPrev.setOnClickListener(this);
        btnPlay.setOnClickListener(this);
        btnNext.setOnClickListener(this);
        
        // 节拍器按钮
        btnMetronome.setOnClickListener(v -> {
            Intent intent = new Intent(MainActivity.this, MetronomeActivity.class);
            startActivity(intent);
        });
    }

    // 初始化图片选择 launcher
    private void initImagePickerLauncher() {
        imagePickerLauncher = registerForActivityResult(
                new ActivityResultContracts.StartActivityForResult(),
                result -> {
                    if (result.getResultCode() == RESULT_OK && result.getData() != null) {
                        Intent data = result.getData();
                        imagePaths.clear();
                        if (data.getData() != null) {
                            Uri uri = data.getData();
                            String path = getImagePathFromUri(MainActivity.this, uri);
                            if (path != null) {
                                imagePaths.add(path);
                            }
                        } else if (data.getClipData() != null) {
                            int count = data.getClipData().getItemCount();
                            for (int i = 0; i < count; i++) {
                                Uri uri = data.getClipData().getItemAt(i).getUri();
                                String path = getImagePathFromUri(MainActivity.this, uri);
                                if (path != null) {
                                    imagePaths.add(path);
                                }
                            }
                        }
                        if (!imagePaths.isEmpty()) {
                            currentPosition = 0;
                            updateImageDisplay();
                            tvStatus.setText(String.format("已选择 %d 张图片", imagePaths.size()));
                        } else {
                            tvStatus.setText("未选择任何图片");
                        }
                    }
                }
        );
    }

    @Override
    public void onClick(View v) {
        if (v.getId() == R.id.btn_select) {
            checkPermissionAndSelectImages();
        } else if (v.getId() == R.id.btn_prev) {
            showPreviousImage();
        } else if (v.getId() == R.id.btn_play) {
            togglePlayStatus();
        } else if (v.getId() == R.id.btn_next) {
            showNextImage();
        }
    }

    // 修复权限检查：兼容 Android 12 及以下
    private void checkPermissionAndSelectImages() {
        String[] permissions;
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.TIRAMISU) {
            permissions = new String[]{android.Manifest.permission.READ_MEDIA_IMAGES};
        } else {
            permissions = new String[]{android.Manifest.permission.READ_EXTERNAL_STORAGE};
        }

        if (ContextCompat.checkSelfPermission(this, permissions[0])
                != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, permissions, REQUEST_PERMISSION);
        } else {
            openImageSelector();
        }
    }

    private void openImageSelector() {
        Intent intent = new Intent(Intent.ACTION_PICK, MediaStore.Images.Media.EXTERNAL_CONTENT_URI);
        intent.setType("image/*");
        intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        // 只使用 ActivityResultLauncher 启动
        imagePickerLauncher.launch(intent);
    }

    // 其他方法（onRequestPermissionsResult、getImagePathFromUri 等）保持不变
    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_PERMISSION) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                openImageSelector();
            } else {
                Toast.makeText(this, "需要存储权限才能选择图片", Toast.LENGTH_SHORT).show();
            }
        }
    }

    /**
     * 获取图片的真实路径
     * @param context 上下文
     * @param uri 图片Uri
     */
    public static String getImagePathFromUri(Context context, android.net.Uri uri) {
        String path = null;
        String[] projection = {android.provider.MediaStore.Images.Media.DATA};
        android.database.Cursor cursor = context.getContentResolver().query(uri, projection, null, null, null);
        if (cursor != null && cursor.moveToFirst()) {
            int columnIndex = cursor.getColumnIndexOrThrow(android.provider.MediaStore.Images.Media.DATA);
            path = cursor.getString(columnIndex);
            cursor.close();
        }
        return path;
    }

    private void showPreviousImage() {
        if (imagePaths.isEmpty()) {
            Toast.makeText(this, "请先选择图片", Toast.LENGTH_SHORT).show();
            return;
        }
        currentPosition = (currentPosition - 1 + imagePaths.size()) % imagePaths.size();
        updateImageDisplay();
    }

    private void showNextImage() {
        if (imagePaths.isEmpty()) {
            Toast.makeText(this, "请先选择图片", Toast.LENGTH_SHORT).show();
            return;
        }
        currentPosition = (currentPosition + 1) % imagePaths.size();
        updateImageDisplay();
    }

    private void togglePlayStatus() {
        if (imagePaths.isEmpty()) {
            Toast.makeText(this, "请先选择图片", Toast.LENGTH_SHORT).show();
            return;
        }

        isPlaying = !isPlaying;
        if (isPlaying) {
            startSlideShow();
            btnPlay.setText("停止播放");
            tvStatus.setText("播放中...");
        } else {
            stopSlideShow();
            btnPlay.setText("开始播放");
            tvStatus.setText(String.format("已选择 %d 张图片", imagePaths.size()));
        }
    }

    private void startSlideShow() {
        stopSlideShow(); // 先停止已有定时器
        timer = new Timer();
        timer.schedule(new TimerTask() {
            @Override
            public void run() {
                handler.post(() -> showNextImage());
            }
        }, 2000, 2000); // 每2秒切换一次
    }

    private void stopSlideShow() {
        if (timer != null) {
            timer.cancel();
            timer = null;
        }
    }

    private void updateImageDisplay() {
        if (!imagePaths.isEmpty()) {
            String path = imagePaths.get(currentPosition);
            ivImage.setImageURI(Uri.parse(path));
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        stopSlideShow(); // 页面销毁时停止定时器
    }

    // 在MainActivity中集成辅助类的调用逻辑
    // 例如：
    // 1. 创建项目时调用ImageProject和ProjectManager
    // 2. 拼接图片时调用ImageMerger
    // 3. 显示通知时调用NotificationHelper
    // 4. 通过项目列表跳转和参数传递
}