package com.example.myapplication;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.content.pm.PackageManager;
import android.widget.Toast;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import java.util.List;
import java.util.ArrayList;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.PopupMenu;
import java.io.FileWriter;
import java.io.File;
import java.io.InputStream;
import java.io.OutputStream;
import android.content.ContentValues;
import android.provider.MediaStore;
import android.os.Environment;
import android.util.Log;
import android.app.ProgressDialog;
import com.google.gson.Gson;
import android.provider.DocumentsContract;
import androidx.recyclerview.widget.DividerItemDecoration;
import java.text.SimpleDateFormat;
import java.util.Locale;

public class HomeActivity extends AppCompatActivity {
    private boolean isMultiSelectMode = false; // 是否多选模式
    private View exportPanel; // 底部导出按钮容器
    private ExportConfigManager exportConfigManager; // 导出配置管理器
    private BluetoothHelper bluetoothHelper; // 蓝牙助手

    private static final int REQUEST_NOTIFICATION_PERMISSION = 100;
    private static final int REQUEST_STORAGE_PERMISSION = 101;

    private RecyclerView projectRecyclerView;
    private List<ImageProject> projectList;
    private ProjectAdapter projectAdapter;
    private ActivityResultLauncher<Intent> importLauncher;

    // 蓝牙状态UI组件
    private TextView tvBluetoothStatus;
    private TextView tvDeviceInfo;
    private View statusIndicator;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_home_mecha);

        // 获取全局BluetoothHelper实例
        bluetoothHelper = BluetoothManager.getInstance().getBluetoothHelper();

        // 初始化导出配置管理器
        exportConfigManager = new ExportConfigManager(this);

        // 设置Toolbar
        Toolbar toolbar = findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);

        // 添加返回按钮
        if (getSupportActionBar() != null) {
            getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        }

        // 初始化RecyclerView分割线
        DividerItemDecoration divider = new DividerItemDecoration(this, DividerItemDecoration.VERTICAL);
        RecyclerView rv = findViewById(R.id.rv_project_list);
        rv.addItemDecoration(divider);

        // 初始化蓝牙状态UI
        tvBluetoothStatus = findViewById(R.id.tv_bluetooth_status);
        tvDeviceInfo = findViewById(R.id.tv_device_info);
        statusIndicator = findViewById(R.id.status_indicator);

        // 初始化底部导出按钮容器
        exportPanel = findViewById(R.id.export_panel);
        if (exportPanel != null) exportPanel.setVisibility(View.GONE);

        // 监听蓝牙连接状态变化
        bluetoothHelper.setOnConnectionChangedListener(new BluetoothHelper.OnConnectionChangedListener() {
            @Override
            public void onConnected(String deviceName, android.bluetooth.BluetoothGatt gatt) {
                runOnUiThread(() -> {
                    updateBluetoothStatusUI();
                    Toast.makeText(HomeActivity.this, "蓝牙已连接: " + deviceName, Toast.LENGTH_SHORT).show();
                });
            }

            @Override
            public void onDisconnected() {
                runOnUiThread(() -> {
                    updateBluetoothStatusUI();
                    Toast.makeText(HomeActivity.this, "蓝牙已断开", Toast.LENGTH_SHORT).show();
                });
            }
        });

        // 更新初始状态
        updateBluetoothStatusUI();

        // Toolbar菜单点击事件
        toolbar.setOnMenuItemClickListener(item -> {
            if (item.getItemId() == R.id.menu_create) {
                startActivity(new Intent(HomeActivity.this, CreateProjectActivity.class));
                return true;
            } else if (item.getItemId() == R.id.menu_import) {
                openImportFileSelector();
                return true;
            } else if (item.getItemId() == R.id.menu_export) {
                toggleMultiSelectMode();
                return true;
            } else if (item.getItemId() == R.id.menu_export_downloads) {
                exportToDownloads();
                return true;
            } else if (item.getItemId() == R.id.menu_export_settings) {
                startActivity(new Intent(HomeActivity.this, ExportSettingsActivity.class));
                return true;
            } else if (item.getItemId() == R.id.menu_bluetooth) {
                showBluetoothStatusDialog();
                return true;
            }
            return false;
        });

        // 初始化底部导出按钮点击事件
        View btnExportSelected = findViewById(R.id.btn_export_selected);
        if (btnExportSelected != null) {
            btnExportSelected.setOnClickListener(v -> {
                android.util.Log.d("ExportDebug", "点击导出按钮");
                exportAllSelectedProjects();
            });
        }
        initImportLauncher();
    }

    /**
     * 更新蓝牙连接状态UI
     */
    private void updateBluetoothStatusUI() {
        if (bluetoothHelper.isConnected()) {
            if (tvBluetoothStatus != null) {
                tvBluetoothStatus.setText("● 蓝牙已连接");
                tvBluetoothStatus.setTextColor(0xFF00FF88);
            }
            if (statusIndicator != null) {
                statusIndicator.setBackgroundTintList(
                    android.content.res.ColorStateList.valueOf(0xFF00FF88));
            }
            if (tvDeviceInfo != null) {
                String deviceInfo = bluetoothHelper.getConnectedDevice();
                tvDeviceInfo.setText(deviceInfo != null ? deviceInfo : "BanBox 设备");
            }
        } else {
            if (tvBluetoothStatus != null) {
                tvBluetoothStatus.setText("● 蓝牙未连接");
                tvBluetoothStatus.setTextColor(0xFFFF4444);
            }
            if (statusIndicator != null) {
                statusIndicator.setBackgroundTintList(
                    android.content.res.ColorStateList.valueOf(0xFFFF4444));
            }
            if (tvDeviceInfo != null) {
                tvDeviceInfo.setText("BanBox 设备");
            }
        }
    }

    // 切换多选模式
    private void toggleMultiSelectMode() {
        isMultiSelectMode = !isMultiSelectMode;
        if (isMultiSelectMode) {
            if (exportPanel != null) {
                exportPanel.setVisibility(View.VISIBLE);
                exportPanel.requestLayout();
            }
            if (projectAdapter != null) projectAdapter.setSelectMode(true);
            Toolbar toolbar = findViewById(R.id.toolbar);
            toolbar.setTitle("选择项目（可多选）");
        } else {
            if (exportPanel != null) {
                exportPanel.setVisibility(View.GONE);
            }
            if (projectAdapter != null) {
                projectAdapter.setSelectMode(false);
                projectAdapter.clearSelections();
            }
            Toolbar toolbar = findViewById(R.id.toolbar);
            toolbar.setTitle("我的项目");
        }
    }

    // 批量导出选中项目到Downloads目录
    private void exportAllSelectedProjects() {
        android.util.Log.d("ExportDebug", "开始导出流程");
        
        if (projectAdapter == null) {
            Toast.makeText(this, "适配器未初始化", Toast.LENGTH_SHORT).show();
            android.util.Log.e("ExportDebug", "projectAdapter为null");
            return;
        }
        
        // 检查存储权限
        if (!checkStoragePermissions()) {
            android.util.Log.d("ExportDebug", "需要请求存储权限");
            requestStoragePermissions();
            return;
        }
        
        List<ImageProject> selectedProjects = projectAdapter.getSelectedProjects();
        android.util.Log.d("ExportDebug", "getSelectedProjects()返回数量：" + selectedProjects.size());
        
        if (selectedProjects.isEmpty()) {
            Toast.makeText(this, "请先选择要导出的项目", Toast.LENGTH_SHORT).show();
            android.util.Log.w("ExportDebug", "没有选中的项目");
            return;
        }
        
        // 直接导出到Downloads目录
        batchExportToDownloads(selectedProjects);
    }

    // 批量导出到Downloads目录的方法
    private void batchExportToDownloads(List<ImageProject> projects) {
        // 显示进度对话框
        ProgressDialog progressDialog = new ProgressDialog(this);
        progressDialog.setTitle("导出到Downloads");
        progressDialog.setMessage("正在导出项目...");
        progressDialog.setProgressStyle(ProgressDialog.STYLE_HORIZONTAL);
        progressDialog.setMax(projects.size());
        progressDialog.setCancelable(false);
        progressDialog.show();
        
        new Thread(() -> {
            int successCount = 0;
            StringBuilder errorLog = new StringBuilder();
            
            for (int i = 0; i < projects.size(); i++) {
                ImageProject project = projects.get(i);
                final int currentIndex = i;
                
                runOnUiThread(() -> {
                    progressDialog.setProgress(currentIndex);
                    progressDialog.setMessage("正在导出: " + project.getProjectName());
                });
                
                try {
                    // 先生成临时ZIP文件
                    String tempZipPath = ProjectManager.exportProject(HomeActivity.this, project);
                    if (tempZipPath != null) {
                        // 使用MediaStore API导出到Downloads
                        String fileName = project.getProjectName() + "_export_" + 
                                         System.currentTimeMillis() + ".zip";
                        
                        ContentValues values = new ContentValues();
                        values.put(MediaStore.Downloads.DISPLAY_NAME, fileName);
                        values.put(MediaStore.Downloads.MIME_TYPE, "application/zip");
                        values.put(MediaStore.Downloads.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS);
                        
                        Uri downloadUri = getContentResolver().insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values);
                        if (downloadUri != null) {
                            // 将临时ZIP文件复制到Downloads
                            try (InputStream inputStream = new java.io.FileInputStream(tempZipPath);
                                 OutputStream outputStream = getContentResolver().openOutputStream(downloadUri)) {
                                if (outputStream != null) {
                                    byte[] buffer = new byte[8192];
                                    int len;
                                    while ((len = inputStream.read(buffer)) != -1) {
                                        outputStream.write(buffer, 0, len);
                                    }
                                    successCount++;
                                    Log.d("Export", "Successfully exported to Downloads: " + fileName);
                                } else {
                                    errorLog.append(project.getProjectName()).append(": 无法打开输出流\n");
                                }
                            }
                            // 删除临时文件
                            new java.io.File(tempZipPath).delete();
                        } else {
                            errorLog.append(project.getProjectName()).append(": 无法创建Downloads文件\n");
                        }
                    } else {
                        errorLog.append(project.getProjectName()).append(": 生成ZIP文件失败\n");
                    }
                } catch (Exception e) {
                    Log.e("Export", "Error exporting to Downloads: " + project.getProjectName(), e);
                    errorLog.append(project.getProjectName()).append(": ").append(e.getMessage()).append("\n");
                }
            }
            
            // 在主线程显示结果
            final int finalSuccessCount = successCount;
            final String finalErrorLog = errorLog.toString();
            final int finalTotal = projects.size();
            
            runOnUiThread(() -> {
                progressDialog.dismiss();
                
                if (finalSuccessCount == 0) {
                    new android.app.AlertDialog.Builder(this)
                        .setTitle("导出到Downloads失败")
                        .setMessage("无法导出到Downloads目录\n\n错误详情：\n" + finalErrorLog)
                        .setPositiveButton("确定", null)
                        .show();
                } else if (finalSuccessCount < finalTotal) {
                    String message = String.format("导出到Downloads：成功%d个，失败%d个\n\n失败详情：\n%s", 
                        finalSuccessCount, finalTotal - finalSuccessCount, finalErrorLog);
                    new android.app.AlertDialog.Builder(this)
                        .setTitle("部分导出成功")
                        .setMessage(message)
                        .setPositiveButton("确定", null)
                        .show();
                } else {
                    Toast.makeText(this,
                        String.format("成功导出%d个项目到Downloads目录", finalSuccessCount),
                        Toast.LENGTH_LONG).show();
                }
                
                // 导出完成后退出多选模式
                toggleMultiSelectMode();
            });
        }).start();
    }

    // 直接导出到Downloads目录
    private void exportToDownloads() {
        if (projectList == null || projectList.isEmpty()) {
            Toast.makeText(this, "没有项目可以导出", Toast.LENGTH_SHORT).show();
            return;
        }
        
        new android.app.AlertDialog.Builder(this)
            .setTitle("导出到Downloads")
            .setMessage("将所有项目导出到Downloads目录")
            .setPositiveButton("确定", (dialog, which) -> {
                batchExportToDownloads(projectList);
            })
            .setNegativeButton("取消", null)
            .show();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.toolbar_menu, menu);
        return true;
    }

    private void initProjectList() {
        projectRecyclerView = findViewById(R.id.rv_project_list);
        projectList = ProjectManager.getProjects(this);
        
        if (projectAdapter == null) {
            // 首次初始化RecyclerView和Adapter
            projectRecyclerView.setLayoutManager(new LinearLayoutManager(this));
            projectAdapter = new ProjectAdapter(projectList, new OnItemLongClickListener() {
                @Override
                public void onItemLongClick(ImageProject project, View view) {
                    showProjectPopupMenu(project, view);
                }
            });
            projectRecyclerView.setAdapter(projectAdapter);
        } else {
            // 仅更新数据，避免重新创建adapter
            projectAdapter.updateProjects(projectList);
        }
    }

    private void showProjectPopupMenu(ImageProject project, View anchorView) {
        PopupMenu popup = new PopupMenu(this, anchorView);
        popup.getMenuInflater().inflate(R.menu.project_popup_menu, popup.getMenu());
        popup.setOnMenuItemClickListener(item -> {
            if (item.getItemId() == R.id.menu_delete) {
                ProjectManager.deleteProject(HomeActivity.this, project);
                projectList.clear();
                projectList.addAll(ProjectManager.getProjects(HomeActivity.this));
                projectAdapter.notifyDataSetChanged();
                Toast.makeText(HomeActivity.this, "项目已删除", Toast.LENGTH_SHORT).show();
                return true;
            }
            return false;
        });
        popup.show();
    }

    private void openImportFileSelector() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.setType("application/zip");
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        importLauncher.launch(intent);
    }

    private void initImportLauncher() {
        importLauncher = registerForActivityResult(
                new ActivityResultContracts.StartActivityForResult(),
                result -> {
                    if (result.getResultCode() == Activity.RESULT_OK && result.getData() != null) {
                        Uri importUri = result.getData().getData();
                        if (importUri != null) {
                            boolean success = importProjectFromUri(importUri);
                            Toast.makeText(this, success ? "导入成功" : "导入失败", Toast.LENGTH_SHORT).show();
                            if (success) {
                                projectList.clear();
                                projectList.addAll(ProjectManager.getProjects(this));
                                projectAdapter.notifyDataSetChanged();
                            }
                        }
                    }
                }
        );
    }

    private boolean importProjectFromUri(Uri uri) {
        try {
            String tempZipPath = FileUtils.getTempImportDir(this) + "/import_" + System.currentTimeMillis() + ".zip";
            
            try (InputStream is = getContentResolver().openInputStream(uri);
                 java.io.FileOutputStream fos = new java.io.FileOutputStream(tempZipPath)) {
                
                if (is == null) return false;
                
                byte[] buffer = new byte[8192];
                int len;
                while ((len = is.read(buffer)) != -1) {
                    fos.write(buffer, 0, len);
                }
            }
            
            return ProjectManager.importProject(this, tempZipPath);
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        // 异步加载项目列表，避免阻塞UI
        new Handler(Looper.getMainLooper()).postDelayed(() -> {
            initProjectList();
        }, 50);
        requestNotificationPermission();
    }

    // 项目适配器
    class ProjectAdapter extends RecyclerView.Adapter<ProjectAdapter.ViewHolder> {
        private final List<ImageProject> projects;
        private final OnItemLongClickListener onItemLongClickListener;
        private boolean isSelectMode = false;

        public ProjectAdapter(List<ImageProject> projects, OnItemLongClickListener onItemLongClickListener) {
            this.projects = projects;
            this.onItemLongClickListener = onItemLongClickListener;
        }
        
        // 添加更新数据方法，避免重新创建adapter
        public void updateProjects(List<ImageProject> newProjects) {
            this.projects.clear();
            this.projects.addAll(newProjects);
            notifyDataSetChanged();
        }

        public void setSelectMode(boolean selectMode) {
            this.isSelectMode = selectMode;
            notifyDataSetChanged();
        }
        
        public boolean isSelectMode() {
            return isSelectMode;
        }
        
        public void clearSelections() {
            for (ImageProject p : projects) p.setSelected(false);
            notifyDataSetChanged();
        }
        
        public List<ImageProject> getSelectedProjects() {
            List<ImageProject> selected = new ArrayList<>();
            for (ImageProject p : projects) if (p.isSelected()) selected.add(p);
            return selected;
        }

        @Override
        public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            View view = getLayoutInflater().inflate(R.layout.item_project, parent, false);
            return new ViewHolder(view);
        }

        @Override
        public void onBindViewHolder(ViewHolder holder, int position) {
            ImageProject project = projects.get(position);
            holder.textView.setText(project.getProjectName());
            
            TextView tvProjectTime = holder.itemView.findViewById(R.id.tv_project_time);
            if (tvProjectTime != null) {
                long createTime = project.getCreateTime();
                if (createTime > 0) {
                    SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.getDefault());
                    tvProjectTime.setText(sdf.format(new java.util.Date(createTime)));
                } else {
                    tvProjectTime.setText("");
                }
            }
            
            holder.checkBox.setOnCheckedChangeListener(null);
            holder.checkBox.setChecked(project.isSelected());
            holder.checkBox.setVisibility(isSelectMode ? View.VISIBLE : View.GONE);
            
            holder.checkBox.setOnCheckedChangeListener((buttonView, isChecked) -> {
                project.setSelected(isChecked);
            });
            
            holder.itemView.setOnClickListener(v -> {
                if (isSelectMode) {
                    boolean newState = !project.isSelected();
                    project.setSelected(newState);
                    holder.checkBox.setChecked(newState);
                } else {
                    String mergedImagePath = project.getMergedImagePath();
                    if (mergedImagePath == null || !new java.io.File(mergedImagePath).exists()) {
                        Toast.makeText(HomeActivity.this, "项目图片损坏或丢失", Toast.LENGTH_SHORT).show();
                        return;
                    }
                    Intent intent = new Intent(HomeActivity.this, ProjectDetailActivity.class);
                    intent.putExtra("merged_image_path", mergedImagePath);
                    startActivity(intent);
                }
            });
            
            holder.itemView.setOnLongClickListener(v -> {
                if (isSelectMode) return false;
                new android.app.AlertDialog.Builder(HomeActivity.this)
                        .setTitle("删除项目")
                        .setMessage("确定要删除该项目吗？")
                        .setPositiveButton("删除", (dialog, which) -> {
                            ProjectManager.deleteProject(HomeActivity.this, project);
                            projects.remove(position);
                            notifyItemRemoved(position);
                            notifyItemRangeChanged(position, projects.size());
                            Toast.makeText(HomeActivity.this, "项目已删除", Toast.LENGTH_SHORT).show();
                        })
                        .setNegativeButton("取消", null)
                        .show();
                return true;
            });
        }

        @Override
        public int getItemCount() {
            return projects.size();
        }

        class ViewHolder extends RecyclerView.ViewHolder {
            TextView textView;
            android.widget.CheckBox checkBox;
            public ViewHolder(View itemView) {
                super(itemView);
                textView = itemView.findViewById(R.id.tv_project_name);
                checkBox = itemView.findViewById(R.id.cb_select);
            }
        }
    }

    interface OnItemLongClickListener {
        void onItemLongClick(ImageProject project, View view);
    }

    private void requestNotificationPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (ContextCompat.checkSelfPermission(this,
                    android.Manifest.permission.POST_NOTIFICATIONS)
                    != android.content.pm.PackageManager.PERMISSION_GRANTED) {

                ActivityCompat.requestPermissions(this,
                        new String[]{android.Manifest.permission.POST_NOTIFICATIONS},
                        REQUEST_NOTIFICATION_PERMISSION);
            } else {
                new NotificationHelper(this).showCreateProjectNotification();
            }
        } else {
            new NotificationHelper(this).showCreateProjectNotification();
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode,
                                           String[] permissions,
                                           int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_NOTIFICATION_PERMISSION) {
            if (grantResults.length > 0 && grantResults[0] == android.content.pm.PackageManager.PERMISSION_GRANTED) {
                new NotificationHelper(this).showCreateProjectNotification();
            } else {
                Toast.makeText(this, "请允许通知权限以创建项目", Toast.LENGTH_SHORT).show();
            }
        } else if (requestCode == REQUEST_STORAGE_PERMISSION) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                Toast.makeText(this, "存储权限已获取，请重新尝试导出", Toast.LENGTH_SHORT).show();
            } else {
                Toast.makeText(this, "未获得存储权限，将使用备选方案导出", Toast.LENGTH_LONG).show();
            }
        }
    }
    
    private boolean checkStoragePermissions() {
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R) {
            return android.os.Environment.isExternalStorageManager();
        } else if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.M) {
            return ContextCompat.checkSelfPermission(this,
                android.Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED;
        }
        return true;
    }
    
    private void requestStoragePermissions() {
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R) {
            try {
                Intent intent = new Intent(android.provider.Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                intent.setData(Uri.parse("package:" + getPackageName()));
                
                new android.app.AlertDialog.Builder(this)
                    .setTitle("需要存储权限")
                    .setMessage("为了导出项目文件，请在设置中允许此应用管理所有文件")
                    .setPositiveButton("去设置", (dialog, which) -> {
                        try {
                            startActivityForResult(intent, REQUEST_STORAGE_PERMISSION);
                        } catch (Exception e) {
                            Toast.makeText(this, "无法打开设置页面", Toast.LENGTH_SHORT).show();
                        }
                    })
                    .setNegativeButton("取消", null)
                    .show();
            } catch (Exception e) {
                Toast.makeText(this, "请在系统设置中手动给予文件管理权限", Toast.LENGTH_LONG).show();
            }
        } else {
            ActivityCompat.requestPermissions(this,
                new String[]{android.Manifest.permission.WRITE_EXTERNAL_STORAGE},
                REQUEST_STORAGE_PERMISSION);
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_STORAGE_PERMISSION) {
            if (checkStoragePermissions()) {
                Toast.makeText(this, "存储权限已获取，请重新尝试导出", Toast.LENGTH_SHORT).show();
            } else {
                Toast.makeText(this, "未获得存储权限，将使用备选方案", Toast.LENGTH_LONG).show();
            }
        }
    }

    @Override
    public boolean onOptionsItemSelected(android.view.MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            // 返回到主页面
            Intent intent = new Intent(HomeActivity.this, WelcomeActivity.class);
            startActivity(intent);
            finish();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    // 显示蓝牙连接状态对话框
    private void showBluetoothStatusDialog() {
        // 模拟蓝牙设备列表（实际应用中应从蓝牙管理器获取）
        String connectedDevice = "模拟蓝牙设备"; // 替换为实际连接的设备名称

        new android.app.AlertDialog.Builder(this)
            .setTitle("蓝牙连接状态")
            .setMessage("当前连接的设备：\n" + connectedDevice)
            .setPositiveButton("确定", null)
            .show();
    }
}