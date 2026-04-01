package com.example.myapplication;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Toast;

import android.widget.TextView;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.List;

public class CreateProjectActivity extends BaseActivity {
    private EditText etProjectName;
    private TextView tvSelectedCount;
    private RecyclerView rvSelectedImages;
    private SelectedImageAdapter imageAdapter;
    private List<String> selectedImagePaths = new ArrayList<>();
    private static final int REQUEST_PERMISSION = 101;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_create_project);

        etProjectName = findViewById(R.id.et_project_name);
        Button btnSelectImages = findViewById(R.id.btn_select_images);
        Button btnConfirm = findViewById(R.id.btn_confirm);
        Button btnCancel = findViewById(R.id.btn_cancel);
        tvSelectedCount = findViewById(R.id.tv_selected_count);
        rvSelectedImages = findViewById(R.id.rv_selected_images);

        // 初始化RecyclerView
        rvSelectedImages.setLayoutManager(new GridLayoutManager(this, 3));
        imageAdapter = new SelectedImageAdapter(selectedImagePaths, this::onImageRemoved);
        rvSelectedImages.setAdapter(imageAdapter);

        btnSelectImages.setOnClickListener(v -> checkPermissionAndSelectImages());
        btnConfirm.setOnClickListener(v -> createProject());
        btnCancel.setOnClickListener(v -> finish());
    }

    private void checkPermissionAndSelectImages() {
        if (ContextCompat.checkSelfPermission(this, android.Manifest.permission.READ_MEDIA_IMAGES)
                != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this,
                    new String[]{android.Manifest.permission.READ_MEDIA_IMAGES},
                    REQUEST_PERMISSION);
        } else {
            openImageSelector();
        }
    }

    private void openImageSelector() {
        Intent intent = new Intent(Intent.ACTION_PICK, android.provider.MediaStore.Images.Media.EXTERNAL_CONTENT_URI);
        intent.setType("image/*");
        intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        imagePickerLauncher.launch(intent);
    }

    private final ActivityResultLauncher<Intent> imagePickerLauncher = registerForActivityResult(
            new ActivityResultContracts.StartActivityForResult(),
            result -> {
                if (result.getResultCode() == Activity.RESULT_OK && result.getData() != null) {
                    Intent data = result.getData();
                    selectedImagePaths.clear();
                    if (data.getData() != null) {
                        String path = MainActivity.getImagePathFromUri(CreateProjectActivity.this, data.getData());
                        if (path != null) selectedImagePaths.add(path);
                    } else if (data.getClipData() != null) {
                        for (int i = 0; i < data.getClipData().getItemCount(); i++) {
                            String path = MainActivity.getImagePathFromUri(CreateProjectActivity.this, data.getClipData().getItemAt(i).getUri());
                            if (path != null) selectedImagePaths.add(path);
                        }
                    }
                    updateImagePreview();
                }
            }
    );

    /**
     * 更新图片预览
     */
    private void updateImagePreview() {
        tvSelectedCount.setText("已选择 " + selectedImagePaths.size() + " 张图片");
        imageAdapter.notifyDataSetChanged();
        Toast.makeText(this, "已选择 " + selectedImagePaths.size() + " 张图片", Toast.LENGTH_SHORT).show();
    }

    /**
     * 删除图片
     */
    private void onImageRemoved(int position) {
        if (position >= 0 && position < selectedImagePaths.size()) {
            selectedImagePaths.remove(position);
            updateImagePreview();
        }
    }

    private void createProject() {
        String projectName = etProjectName.getText().toString().trim();
        if (projectName.isEmpty()) {
            Toast.makeText(this, "请输入项目名", Toast.LENGTH_SHORT).show();
            return;
        }
        if (selectedImagePaths.isEmpty()) {
            Toast.makeText(this, "请选择图片", Toast.LENGTH_SHORT).show();
            return;
        }
        
        android.util.Log.d("CreateProject", "开始创建项目: " + projectName);
        android.util.Log.d("CreateProject", "图片数量: " + selectedImagePaths.size());
        for (int i = 0; i < selectedImagePaths.size(); i++) {
            android.util.Log.d("CreateProject", "图片 " + (i+1) + ": " + selectedImagePaths.get(i));
        }
        
        // 重要：创建一个新的ArrayList副本，避免引用问题
        List<String> imagePathsCopy = new ArrayList<>(selectedImagePaths);
        
        // 新方案：直接保存图片列表，不再强制拼接
        // 拼接图片作为可选功能（用于缩略图预览）
        String mergedPath = null;
        try {
            // 尝试拼接，但失败不影响项目创建
            mergedPath = ImageMerger.mergeImages(CreateProjectActivity.this, imagePathsCopy, projectName);
            android.util.Log.d("CreateProject", "拼接图片成功: " + mergedPath);
        } catch (Exception e) {
            android.util.Log.w("CreateProject", "图片拼接失败，但项目仍可创建", e);
        }
        
        // 保存项目（即使mergedPath为null也可以）
        ImageProject newProject = new ImageProject(projectName, imagePathsCopy, mergedPath);
        android.util.Log.d("CreateProject", "保存项目对象 - 名称: " + newProject.getProjectName() + 
                           ", 图片数: " + (newProject.getImagePaths() != null ? newProject.getImagePaths().size() : "null"));
        
        ProjectManager.saveProject(this, newProject);
        Toast.makeText(this, R.string.project_created, Toast.LENGTH_SHORT).show();
        finish();
    }
}
