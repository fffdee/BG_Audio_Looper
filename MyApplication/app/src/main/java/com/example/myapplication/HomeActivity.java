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
import androidx.core.content.FileProvider;
import android.widget.EditText;

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
            } else if (item.getItemId() == R.id.menu_import_wechat) {
                openImportFromWechat();
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
        
        // 处理从微信等外部应用传入的ZIP文件
        handleIncomingIntent(getIntent());
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        handleIncomingIntent(intent);
    }

    /**
     * 处理从微信/其他应用分享过来的ZIP文件
     */
    private void handleIncomingIntent(Intent intent) {
        if (intent == null) return;
        
        String action = intent.getAction();
        String type = intent.getType();
        
        if (Intent.ACTION_SEND.equals(action) && type != null) {
            // 从微信"发送到其他应用"接收文件
            Uri fileUri = intent.getParcelableExtra(Intent.EXTRA_STREAM);
            if (fileUri != null) {
                Log.d("HomeActivity", "接收到分享文件: " + fileUri);
                confirmAndImport(fileUri);
            }
        } else if (Intent.ACTION_VIEW.equals(action)) {
            // 从文件管理器直接打开ZIP文件
            Uri fileUri = intent.getData();
            if (fileUri != null) {
                Log.d("HomeActivity", "接收到打开文件: " + fileUri);
                confirmAndImport(fileUri);
            }
        }
    }

    /**
     * 确认并导入项目
     */
    private void confirmAndImport(Uri fileUri) {
        new android.app.AlertDialog.Builder(this)
                .setTitle("导入项目")
                .setMessage("是否导入此项目文件？")
                .setPositiveButton("导入", (dialog, which) -> {
                    boolean success = importProjectFromUri(fileUri);
                    Toast.makeText(this, success ? "导入成功" : "导入失败（可能不是有效的项目文件）", Toast.LENGTH_SHORT).show();
                    if (success) {
                        // 刷新列表
                        initProjectList();
                    }
                })
                .setNegativeButton("取消", null)
                .show();
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
                                         System.currentTimeMillis() + ".gs";
                        
                        ContentValues values = new ContentValues();
                        values.put(MediaStore.Downloads.DISPLAY_NAME, fileName);
                        values.put(MediaStore.Downloads.MIME_TYPE, "application/octet-stream");
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
            } else if (item.getItemId() == R.id.menu_rename) {
                int position = projectList.indexOf(project);
                showRenameDialog(project, position);
                return true;
            } else if (item.getItemId() == R.id.menu_share_wechat) {
                shareProjectToWechat(project);
                return true;
            }
            return false;
        });
        popup.show();
    }

    private void openImportFileSelector() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[]{"application/zip", "application/octet-stream"});
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
            Log.d("HomeActivity", "开始从URI导入: " + uri);
            String tempZipPath = FileUtils.getTempImportDir(this) + "/import_" + System.currentTimeMillis() + ".gs";
            
            // 确保临时目录存在
            File tempZipFile = new File(tempZipPath);
            File parentDir = tempZipFile.getParentFile();
            if (parentDir != null && !parentDir.exists()) {
                parentDir.mkdirs();
            }
            
            try (InputStream is = getContentResolver().openInputStream(uri);
                 java.io.FileOutputStream fos = new java.io.FileOutputStream(tempZipPath)) {
                
                if (is == null) {
                    Log.e("HomeActivity", "无法打开输入流: " + uri);
                    return false;
                }
                
                byte[] buffer = new byte[8192];
                int len;
                long total = 0;
                while ((len = is.read(buffer)) != -1) {
                    fos.write(buffer, 0, len);
                    total += len;
                }
                Log.d("HomeActivity", "文件复制完成, 大小: " + total + " bytes -> " + tempZipPath);
            }
            
            return ProjectManager.importProject(this, tempZipPath);
        } catch (Exception e) {
            Log.e("HomeActivity", "从URI导入失败", e);
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
                    // 使用新逻辑：传递图片列表而不是拼接图
                    android.util.Log.d("HomeActivity", "点击项目: " + project.getProjectName());
                    
                    java.util.List<String> imagePaths = project.getImagePaths();
                    if (imagePaths == null || imagePaths.isEmpty()) {
                        android.util.Log.e("HomeActivity", "项目图片列表为空");
                        Toast.makeText(HomeActivity.this, "项目图片列表为空", Toast.LENGTH_SHORT).show();
                        return;
                    }
                    
                    android.util.Log.d("HomeActivity", "图片数量: " + imagePaths.size());
                    
                    // 验证图片文件是否存在
                    boolean allExists = true;
                    for (String path : imagePaths) {
                        if (path == null || !new java.io.File(path).exists()) {
                            android.util.Log.w("HomeActivity", "图片不存在: " + path);
                            allExists = false;
                            break;
                        }
                    }
                    if (!allExists) {
                        Toast.makeText(HomeActivity.this, "部分图片文件丢失", Toast.LENGTH_SHORT).show();
                        return;
                    }
                    
                    Intent intent = new Intent(HomeActivity.this, ProjectDetailActivity.class);
                    intent.putStringArrayListExtra("image_paths", new java.util.ArrayList<>(imagePaths));
                    startActivity(intent);
                }
            });
            
            holder.itemView.setOnLongClickListener(v -> {
                if (isSelectMode) return false;
                String[] options = {"重命名", "分享到微信", "删除"};
                new android.app.AlertDialog.Builder(HomeActivity.this)
                        .setTitle(project.getProjectName())
                        .setItems(options, (dialog, which) -> {
                            switch (which) {
                                case 0: // 重命名
                                    showRenameDialog(project, holder.getAdapterPosition());
                                    break;
                                case 1: // 分享到微信
                                    shareProjectToWechat(project);
                                    break;
                                case 2: // 删除
                                    new android.app.AlertDialog.Builder(HomeActivity.this)
                                        .setTitle("删除项目")
                                        .setMessage("确定要删除该项目吗？")
                                        .setPositiveButton("删除", (d2, w2) -> {
                                            ProjectManager.deleteProject(HomeActivity.this, project);
                                            projects.remove(holder.getAdapterPosition());
                                            notifyItemRemoved(holder.getAdapterPosition());
                                            notifyItemRangeChanged(holder.getAdapterPosition(), projects.size());
                                            Toast.makeText(HomeActivity.this, "项目已删除", Toast.LENGTH_SHORT).show();
                                        })
                                        .setNegativeButton("取消", null)
                                        .show();
                                    break;
                            }
                        })
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

    // 显示重命名对话框
    private void showRenameDialog(ImageProject project, int position) {
        EditText editText = new EditText(this);
        editText.setText(project.getProjectName());
        editText.setSelectAllOnFocus(true);
        editText.setPadding(60, 40, 60, 40);

        new android.app.AlertDialog.Builder(this)
                .setTitle("重命名项目")
                .setView(editText)
                .setPositiveButton("确定", (dialog, which) -> {
                    String newName = editText.getText().toString().trim();
                    if (newName.isEmpty()) {
                        Toast.makeText(this, "项目名不能为空", Toast.LENGTH_SHORT).show();
                        return;
                    }
                    if (ProjectManager.renameProject(this, project, newName)) {
                        project.setProjectName(newName);
                        if (projectAdapter != null) {
                            projectAdapter.notifyItemChanged(position);
                        }
                        Toast.makeText(this, "重命名成功", Toast.LENGTH_SHORT).show();
                    } else {
                        Toast.makeText(this, "重命名失败", Toast.LENGTH_SHORT).show();
                    }
                })
                .setNegativeButton("取消", null)
                .show();
    }

    /**
     * 分享项目到微信
     * 将项目导出为ZIP文件，然后通过系统分享发送到微信
     */
    private void shareProjectToWechat(ImageProject project) {
        ProgressDialog progressDialog = new ProgressDialog(this);
        progressDialog.setMessage("正在打包项目...");
        progressDialog.setCancelable(false);
        progressDialog.show();

        new Thread(() -> {
            try {
                String zipPath = ProjectManager.exportProject(HomeActivity.this, project);
                runOnUiThread(() -> {
                    progressDialog.dismiss();
                    if (zipPath != null) {
                        File zipFile = new File(zipPath);
                        Uri contentUri = FileProvider.getUriForFile(
                                HomeActivity.this,
                                getPackageName() + ".fileprovider",
                                zipFile);

                        Intent shareIntent = new Intent(Intent.ACTION_SEND);
                        shareIntent.setType("application/octet-stream");
                        shareIntent.putExtra(Intent.EXTRA_STREAM, contentUri);
                        shareIntent.putExtra(Intent.EXTRA_SUBJECT, project.getProjectName());
                        shareIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);

                        // 尝试直接定位微信，如果微信未安装则弹出通用分享器
                        shareIntent.setPackage("com.tencent.mm");
                        try {
                            startActivity(shareIntent);
                        } catch (android.content.ActivityNotFoundException e) {
                            // 微信未安装，使用系统分享选择器
                            shareIntent.setPackage(null);
                            startActivity(Intent.createChooser(shareIntent, "分享项目到..."));
                        }
                    } else {
                        Toast.makeText(HomeActivity.this, "项目打包失败", Toast.LENGTH_SHORT).show();
                    }
                });
            } catch (Exception e) {
                Log.e("HomeActivity", "分享到微信失败", e);
                runOnUiThread(() -> {
                    progressDialog.dismiss();
                    Toast.makeText(HomeActivity.this, "分享失败: " + e.getMessage(), Toast.LENGTH_SHORT).show();
                });
            }
        }).start();
    }

    /**
     * 从微信导入 — 扫描微信文件保存目录，列出可导入的ZIP项目文件。
     * 微信在不同版本/设备上的文件保存路径:
     *   - Download/            (Android 11+ 默认)
     *   - tencent/MicroMsg/Download/  (旧版微信)
     *   - Android/data/com.tencent.mm/MicroMsg/Download/ (部分设备)
     */
    private void openImportFromWechat() {
        // 检查微信是否已安装
        boolean wechatInstalled = isAppInstalled("com.tencent.mm");

        new android.app.AlertDialog.Builder(this)
                .setTitle("从微信导入项目")
                .setMessage(wechatInstalled
                        ? "操作步骤：\n\n"
                          + "1. 点击\"打开微信\"跳转到微信\n"
                          + "2. 找到聊天中收到的项目文件(.gs)\n"
                          + "3. 长按或点击该文件\n"
                          + "4. 选择\"用其他应用打开\"\n"
                          + "5. 选择本应用，即可自动导入\n\n"
                          + "也可以选择\"扫描已下载\"查找已保存到本地的微信文件。"
                        : "未检测到微信。\n\n"
                          + "你可以选择\"扫描已下载\"查找本地的项目文件，\n"
                          + "或\"手动选择\"从文件管理器中选择。")
                .setPositiveButton(wechatInstalled ? "打开微信" : "扫描已下载", (d, w) -> {
                    if (wechatInstalled) {
                        launchWechat();
                    } else {
                        scanAndShowWechatFiles();
                    }
                })
                .setNeutralButton(wechatInstalled ? "扫描已下载" : "手动选择", (d, w) -> {
                    if (wechatInstalled) {
                        scanAndShowWechatFiles();
                    } else {
                        openImportFileSelector();
                    }
                })
                .setNegativeButton("取消", null)
                .show();
    }

    /**
     * 检查应用是否已安装
     */
    private boolean isAppInstalled(String packageName) {
        try {
            getPackageManager().getPackageInfo(packageName, 0);
            return true;
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
    }

    /**
     * 启动微信APP
     */
    private void launchWechat() {
        try {
            Intent launchIntent = getPackageManager().getLaunchIntentForPackage("com.tencent.mm");
            if (launchIntent != null) {
                launchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                startActivity(launchIntent);
                Toast.makeText(this,
                        "请在微信中找到项目文件，点击后选择\"用其他应用打开\"",
                        Toast.LENGTH_LONG).show();
            }
        } catch (Exception e) {
            Log.e("HomeActivity", "启动微信失败", e);
            Toast.makeText(this, "启动微信失败，请手动打开微信", Toast.LENGTH_SHORT).show();
        }
    }

    /**
     * 扫描本地微信文件并显示选择对话框
     */
    private void scanAndShowWechatFiles() {
        if (!checkStoragePermissions()) {
            new android.app.AlertDialog.Builder(this)
                    .setTitle("需要存储权限")
                    .setMessage("扫描微信下载的文件需要存储权限。")
                    .setPositiveButton("获取权限", (d, w) -> requestStoragePermissions())
                    .setNeutralButton("手动选择", (d, w) -> openImportFileSelector())
                    .setNegativeButton("取消", null)
                    .show();
            return;
        }
        new Thread(() -> {
            List<File> zipFiles = scanWechatZipFiles();
            runOnUiThread(() -> showWechatFilePickerDialog(zipFiles));
        }).start();
    }

    /**
     * 扫描微信接收文件常见目录中的 .gs 项目文件
     */
    private List<File> scanWechatZipFiles() {
        List<File> result = new ArrayList<>();
        File sdcard = Environment.getExternalStorageDirectory();

        // 微信文件可能保存的目录列表
        String[] wechatDirs = {
            // Android 11+ 微信保存到公共 Download
            sdcard + "/Download",
            // 旧版微信
            sdcard + "/tencent/MicroMsg/Download",
            // 部分设备的微信路径
            sdcard + "/Android/data/com.tencent.mm/MicroMsg/Download",
            // 微信文件传输助手、企业微信等也可能存到这些位置
            sdcard + "/Download/WeiXin",
            sdcard + "/tencent/MicroMsg",
        };

        for (String dirPath : wechatDirs) {
            File dir = new File(dirPath);
            if (dir.exists() && dir.isDirectory()) {
                scanZipFilesRecursive(dir, result, 2); // 最多递归2层
            }
        }

        // 按修改时间降序排列（最新的在前）
        result.sort((a, b) -> Long.compare(b.lastModified(), a.lastModified()));
        return result;
    }

    /**
     * 递归扫描目录中的 .gs 项目文件
     */
    private void scanZipFilesRecursive(File dir, List<File> result, int maxDepth) {
        if (maxDepth < 0 || dir == null || !dir.isDirectory()) return;
        File[] files = dir.listFiles();
        if (files == null) return;
        for (File f : files) {
            String name = f.getName().toLowerCase(Locale.ROOT);
            if (f.isFile() && (name.endsWith(".gs") || name.endsWith(".zip"))) {
                result.add(f);
            } else if (f.isDirectory() && maxDepth > 0) {
                scanZipFilesRecursive(f, result, maxDepth - 1);
            }
        }
    }

    /**
     * 显示微信文件选择对话框
     */
    private void showWechatFilePickerDialog(List<File> zipFiles) {
        if (zipFiles.isEmpty()) {
            // 没有找到文件，提示用户并提供备选方案
            new android.app.AlertDialog.Builder(this)
                    .setTitle("从微信导入")
                    .setMessage("未在微信文件目录中找到项目文件。\n\n"
                            + "请先在微信中打开收到的项目文件（.gs格式），选择\"用其他应用打开\"即可自动导入。\n\n"
                            + "或者点击下方按钮手动浏览文件。")
                    .setPositiveButton("手动选择文件", (dialog, which) -> openImportFileSelector())
                    .setNegativeButton("取消", null)
                    .show();
            return;
        }

        // 构建文件列表显示名
        SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.getDefault());
        String[] displayItems = new String[zipFiles.size()];
        for (int i = 0; i < zipFiles.size(); i++) {
            File f = zipFiles.get(i);
            String size = formatFileSize(f.length());
            String date = sdf.format(f.lastModified());
            // 显示: 文件名 (大小, 日期)
            displayItems[i] = f.getName() + "\n" + size + "  " + date;
        }

        new android.app.AlertDialog.Builder(this)
                .setTitle("从微信导入 — 选择项目文件")
                .setItems(displayItems, (dialog, which) -> {
                    File selected = zipFiles.get(which);
                    Log.d("HomeActivity", "选择微信文件: " + selected.getAbsolutePath());
                    confirmAndImportFile(selected);
                })
                .setNeutralButton("手动选择", (dialog, which) -> openImportFileSelector())
                .setNegativeButton("取消", null)
                .show();
    }

    /**
     * 确认并导入选中的文件
     */
    private void confirmAndImportFile(File zipFile) {
        new android.app.AlertDialog.Builder(this)
                .setTitle("导入项目")
                .setMessage("是否导入项目文件？\n" + zipFile.getName())
                .setPositiveButton("导入", (dialog, which) -> {
                    boolean success = ProjectManager.importProject(HomeActivity.this, zipFile.getAbsolutePath());
                    Toast.makeText(HomeActivity.this, success ? "导入成功" : "导入失败", Toast.LENGTH_SHORT).show();
                    if (success) {
                        projectList.clear();
                        projectList.addAll(ProjectManager.getProjects(HomeActivity.this));
                        projectAdapter.notifyDataSetChanged();
                    }
                })
                .setNegativeButton("取消", null)
                .show();
    }

    /**
     * 格式化文件大小
     */
    private String formatFileSize(long bytes) {
        if (bytes < 1024) return bytes + " B";
        if (bytes < 1024 * 1024) return String.format(Locale.getDefault(), "%.1f KB", bytes / 1024.0);
        return String.format(Locale.getDefault(), "%.1f MB", bytes / (1024.0 * 1024));
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