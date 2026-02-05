package com.example.myapplication;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Toast;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import java.util.ArrayList;
import java.util.List;

public class CreateProjectActivity extends AppCompatActivity {
    private EditText etProjectName;
    private List<String> selectedImagePaths = new ArrayList<>();
    private static final int REQUEST_PERMISSION = 101;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_create_project);

        etProjectName = findViewById(R.id.et_project_name);
        Button btnSelectImages = findViewById(R.id.btn_select_images);
        Button btnConfirm = findViewById(R.id.btn_confirm);

        btnSelectImages.setOnClickListener(v -> checkPermissionAndSelectImages());
        btnConfirm.setOnClickListener(v -> createProject());
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
                    Toast.makeText(this, "已选择 " + selectedImagePaths.size() + " 张图片", Toast.LENGTH_SHORT).show();
                }
            }
    );

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
        String mergedPath = ImageMerger.mergeImages(CreateProjectActivity.this, selectedImagePaths, projectName);
        if (mergedPath != null) {
            ProjectManager.saveProject(this, new ImageProject(projectName, selectedImagePaths, mergedPath));
            Toast.makeText(this, R.string.project_created, Toast.LENGTH_SHORT).show();
            finish();
        } else {
            Toast.makeText(this, "图片拼接失败", Toast.LENGTH_SHORT).show();
        }
    }
}
