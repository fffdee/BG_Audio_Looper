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
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
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
import androidx.drawerlayout.widget.DrawerLayout;
import androidx.core.view.GravityCompat;

public class HomeActivity extends BaseActivity {
    private boolean isMultiSelectMode = false; // 是否多选模式
    private View exportPanel; // 底部导出按钮容器
    private ExportConfigManager exportConfigManager; // 导出配置管理器
    private BluetoothHelper bluetoothHelper; // 蓝牙助手

    // 侧边栏相关
    private DrawerLayout drawerLayout;
    private RecyclerView rvCategories;
    private CategoryAdapter categoryAdapter;
    private List<CategoryManager.Category> categoryList;
    private String currentCategoryId = CategoryManager.DEFAULT_CATEGORY_ID;
    private TextView tvCurrentCategory;

    private static final int REQUEST_NOTIFICATION_PERMISSION = 100;
    private static final int REQUEST_STORAGE_PERMISSION = 101;

    private RecyclerView projectRecyclerView;
    private List<ImageProject> projectList;
    private ProjectAdapter projectAdapter;

    // 蓝牙状态UI组件
    private TextView tvBluetoothStatus;
    private TextView tvDeviceInfo;
    private View statusIndicator;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_home_mecha);
        setupBaseToolbar(true);

        // 获取全局BluetoothHelper实例
        bluetoothHelper = BluetoothManager.getInstance().getBluetoothHelper();

        // 初始化导出配置管理器
        exportConfigManager = new ExportConfigManager(this);

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

        // 初始化当前分类标签
        tvCurrentCategory = findViewById(R.id.tv_current_category);

        // 初始化 DrawerLayout 和侧边栏
        drawerLayout = findViewById(R.id.drawer_layout);
        initSidebar();

        // "分类"按钮打开侧边栏
        View btnToggleDrawer = findViewById(R.id.btn_toggle_drawer);
        if (btnToggleDrawer != null) {
            btnToggleDrawer.setOnClickListener(v -> {
                if (drawerLayout != null) drawerLayout.openDrawer(GravityCompat.START);
            });
        }

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

        // 初始化设置侧边栏
        initSettingsDrawer();

        // Toolbar菜单点击事件
        if (baseToolbar != null) {
            baseToolbar.setOnMenuItemClickListener(item -> {
                if (item.getItemId() == R.id.menu_create) {
                    startActivity(new Intent(HomeActivity.this, CreateProjectActivity.class));
                    return true;
                } else if (item.getItemId() == R.id.menu_wechat_export) {
                    toggleMultiSelectMode();
                    return true;
                } else if (item.getItemId() == R.id.menu_bluetooth) {
                    openSettingsDrawer();
                    return true;
                }
                return false;
            });
        }

        // 初始化底部导出按钮点击事件
        View btnExportSelected = findViewById(R.id.btn_export_selected);
        if (btnExportSelected != null) {
            btnExportSelected.setOnClickListener(v -> {
                android.util.Log.d("ExportDebug", "点击导出按钮");
                showWechatExportDialog();
            });
        }

        // 初始化移至分类按钮点击事件
        View btnMoveSelected = findViewById(R.id.btn_move_selected);
        if (btnMoveSelected != null) {
            btnMoveSelected.setOnClickListener(v -> showMoveSelectedToCategory());
        }

        // 初始化删除按钮点击事件
        View btnDeleteSelected = findViewById(R.id.btn_delete_selected);
        if (btnDeleteSelected != null) {
            btnDeleteSelected.setOnClickListener(v -> showDeleteSelectedDialog());
        }
        
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
        // 先复制到临时文件以便检测
        String tempZipPath = FileUtils.getTempImportDir(this) + "/detect_" + System.currentTimeMillis() + ".tmp";
        try {
            File tempZipFile = new File(tempZipPath);
            File parentDir = tempZipFile.getParentFile();
            if (parentDir != null && !parentDir.exists()) {
                parentDir.mkdirs();
            }
            
            try (InputStream is = getContentResolver().openInputStream(fileUri);
                 java.io.FileOutputStream fos = new java.io.FileOutputStream(tempZipPath)) {
                
                if (is == null) {
                    Toast.makeText(this, "无法读取文件", Toast.LENGTH_SHORT).show();
                    return;
                }
                
                byte[] buffer = new byte[8192];
                int len;
                while ((len = is.read(buffer)) != -1) {
                    fos.write(buffer, 0, len);
                }
            }
            
            // 检测是否为总包
            final int projectCount = ProjectManager.detectBundlePackage(tempZipPath);
            final String finalTempPath = tempZipPath;
            
            if (projectCount > 0) {
                // 是总包，显示批量导入确认
                new android.app.AlertDialog.Builder(this)
                        .setTitle("导入总包")
                        .setMessage("检测到总包文件，包含 " + projectCount + " 个项目，是否导入？")
                        .setPositiveButton("导入", (dialog, which) -> {
                            importBundlePackage(finalTempPath, projectCount);
                        })
                        .setNegativeButton("取消", (dialog, which) -> {
                            new File(finalTempPath).delete();
                        })
                        .show();
            } else if (projectCount == 0) {
                // 是单个项目文件
                new android.app.AlertDialog.Builder(this)
                        .setTitle("导入项目")
                        .setMessage("是否导入此项目文件？")
                        .setPositiveButton("导入", (dialog, which) -> {
                            boolean success = ProjectManager.importProject(this, finalTempPath);
                            Toast.makeText(this, success ? "导入成功" : "导入失败（可能不是有效的项目文件）", Toast.LENGTH_SHORT).show();
                            if (success) {
                                initProjectList();
                            }
                            new File(finalTempPath).delete();
                        })
                        .setNegativeButton("取消", (dialog, which) -> {
                            new File(finalTempPath).delete();
                        })
                        .show();
            } else {
                // 检测失败
                Toast.makeText(this, "无法识别文件格式", Toast.LENGTH_SHORT).show();
                new File(finalTempPath).delete();
            }
            
        } catch (Exception e) {
            Log.e("HomeActivity", "检测文件失败", e);
            Toast.makeText(this, "文件读取失败", Toast.LENGTH_SHORT).show();
            new File(tempZipPath).delete();
        }
    }
    
    /**
     * 导入总包
     */
    private void importBundlePackage(String bundleZipPath, int totalCount) {
        // 显示进度对话框
        android.app.ProgressDialog progressDialog = new android.app.ProgressDialog(this);
        progressDialog.setTitle("导入总包");
        progressDialog.setMessage("正在导入项目...");
        progressDialog.setProgressStyle(android.app.ProgressDialog.STYLE_HORIZONTAL);
        progressDialog.setMax(totalCount);
        progressDialog.setCancelable(false);
        progressDialog.show();
        
        // 异步导入
        new Thread(() -> {
            int successCount = ProjectManager.importBundlePackage(this, bundleZipPath, 
                (current, total, projectName) -> {
                    // 更新进度对话框
                    runOnUiThread(() -> {
                        progressDialog.setProgress(current);
                        progressDialog.setMessage("正在导入: " + projectName + " (" + current + "/" + total + ")");
                    });
                });
            
            runOnUiThread(() -> {
                progressDialog.dismiss();
                new File(bundleZipPath).delete();
                
                String message = "成功导入 " + successCount + " 个项目";
                if (successCount < totalCount) {
                    message += "，失败 " + (totalCount - successCount) + " 个";
                }
                
                new android.app.AlertDialog.Builder(this)
                        .setTitle("导入完成")
                        .setMessage(message)
                        .setPositiveButton("确定", null)
                        .show();
                
                if (successCount > 0) {
                    initProjectList();
                }
            });
        }).start();
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

    // ===================== 侧边栏 & 分类管理 =====================

    /** 初始化侧边栏 */
    private void initSidebar() {
        rvCategories = findViewById(R.id.rv_categories);
        if (rvCategories == null) return;
        rvCategories.setLayoutManager(new LinearLayoutManager(this));

        categoryList = CategoryManager.getCategories(this);
        categoryAdapter = new CategoryAdapter(categoryList);
        rvCategories.setAdapter(categoryAdapter);

        // 添加分类按钮
        View btnAddCategory = findViewById(R.id.btn_add_category);
        if (btnAddCategory != null) {
            btnAddCategory.setOnClickListener(v -> showAddCategoryDialog());
        }

        refreshCategoryHeader();
    }

    /** 刷新顶部分类标题 */
    private void refreshCategoryHeader() {
        if (tvCurrentCategory == null) return;
        String name = CategoryManager.getCategoryName(this, currentCategoryId);
        tvCurrentCategory.setText("📁 " + name);
    }

    /** 弹出新增分类输入框 */
    private void showAddCategoryDialog() {
        EditText et = new EditText(this);
        et.setHint("分类名称");
        et.setPadding(60, 40, 60, 40);
        new android.app.AlertDialog.Builder(this)
                .setTitle("新建分类")
                .setView(et)
                .setPositiveButton("确定", (d, w) -> {
                    String name = et.getText().toString().trim();
                    if (name.isEmpty()) {
                        Toast.makeText(this, "名称不能为空", Toast.LENGTH_SHORT).show();
                        return;
                    }
                    CategoryManager.addCategory(this, name);
                    refreshSidebar();
                })
                .setNegativeButton("取消", null)
                .show();
    }

    /** 弹出重命名分类输入框 */
    private void showRenameCategoryDialog(CategoryManager.Category category, int position) {
        EditText et = new EditText(this);
        et.setText(category.getName());
        et.setSelectAllOnFocus(true);
        et.setPadding(60, 40, 60, 40);
        new android.app.AlertDialog.Builder(this)
                .setTitle("重命名分类")
                .setView(et)
                .setPositiveButton("确定", (d, w) -> {
                    String name = et.getText().toString().trim();
                    if (name.isEmpty()) {
                        Toast.makeText(this, "名称不能为空", Toast.LENGTH_SHORT).show();
                        return;
                    }
                    CategoryManager.renameCategory(this, category.getId(), name);
                    refreshSidebar();
                    refreshCategoryHeader();
                })
                .setNegativeButton("取消", null)
                .show();
    }

    /** 删除分类确认 */
    private void confirmDeleteCategory(CategoryManager.Category category, int position) {
        if (CategoryManager.DEFAULT_CATEGORY_ID.equals(category.getId())) {
            Toast.makeText(this, "默认分类不可删除", Toast.LENGTH_SHORT).show();
            return;
        }
        
        // 计算该分类下的项目数
        List<ImageProject> categoryProjects = new ArrayList<>();
        for (ImageProject p : ProjectManager.getProjects(this)) {
            if (category.getId().equals(p.getCategoryId())) {
                categoryProjects.add(p);
            }
        }
        
        String message = "确定要删除分类「" + category.getName() + "」及其下的所有项目吗？\n";
        if (categoryProjects.size() > 0) {
            message += "\n即将删除 " + categoryProjects.size() + " 个项目，此操作不可撤销。";
        } else {
            message += "\n该分类下没有项目。";
        }
        
        new android.app.AlertDialog.Builder(this)
                .setTitle("删除分类")
                .setMessage(message)
                .setPositiveButton("确认删除", (d, w) -> {
                    // 先将当前分类切换到默认分类（如果当前正在看被删除的分类）
                    if (currentCategoryId.equals(category.getId())) {
                        currentCategoryId = CategoryManager.DEFAULT_CATEGORY_ID;
                        refreshCategoryHeader();
                    }
                    // 执行删除（会删除分类和其下的所有项目及文件夹）
                    CategoryManager.deleteCategory(this, category.getId());
                    // 刷新UI
                    refreshSidebar();
                    initProjectList();
                    Toast.makeText(this, "分类及其项目已删除", Toast.LENGTH_SHORT).show();
                })
                .setNegativeButton("取消", null)
                .show();
    }

    /** 刷新侧边栏分类列表 */
    private void refreshSidebar() {
        categoryList = CategoryManager.getCategories(this);
        if (categoryAdapter != null) {
            categoryAdapter.updateData(categoryList);
        }
    }

    /** 弹出移至分类对话框 */
    private void showMoveToCategory(ImageProject project) {
        List<CategoryManager.Category> cats = CategoryManager.getCategories(this);
        String[] names = new String[cats.size()];
        for (int i = 0; i < cats.size(); i++) names[i] = cats.get(i).getName();

        new android.app.AlertDialog.Builder(this)
                .setTitle("移至分类")
                .setItems(names, (d, which) -> {
                    String targetId = cats.get(which).getId();
                    ProjectManager.moveProjectToCategory(this, project, targetId);
                    project.setCategoryId(targetId);
                    // 如果当前显示的不是目标分类，则从列表移除
                    if (!targetId.equals(currentCategoryId)) {
                        projectList.remove(project);
                        if (projectAdapter != null) projectAdapter.notifyDataSetChanged();
                    }
                    Toast.makeText(this, "已移至「" + cats.get(which).getName() + "」", Toast.LENGTH_SHORT).show();
                    refreshSidebar();
                })
                .setNegativeButton("取消", null)
                .show();
    }

    /** 批量移动选中项目到指定分类 */
    private void showMoveSelectedToCategory() {
        if (projectAdapter == null) return;
        List<ImageProject> selected = projectAdapter.getSelectedProjects();
        if (selected.isEmpty()) {
            Toast.makeText(this, "请先选择要移动的项目", Toast.LENGTH_SHORT).show();
            return;
        }

        List<CategoryManager.Category> cats = CategoryManager.getCategories(this);
        String[] names = new String[cats.size()];
        for (int i = 0; i < cats.size(); i++) names[i] = cats.get(i).getName();

        new android.app.AlertDialog.Builder(this)
                .setTitle("将 " + selected.size() + " 个项目移至分类")
                .setItems(names, (d, which) -> {
                    String targetId = cats.get(which).getId();
                    String targetName = cats.get(which).getName();
                    for (ImageProject project : selected) {
                        ProjectManager.moveProjectToCategory(this, project, targetId);
                    }
                    // 刷新项目列表
                    initProjectList();
                    // 退出多选模式
                    toggleMultiSelectMode();
                    refreshSidebar();
                    Toast.makeText(this, "已将 " + selected.size() + " 个项目移至「" + targetName + "」", Toast.LENGTH_SHORT).show();
                })
                .setNegativeButton("取消", null)
                .show();
    }

    /** 删除选中项目（带二次确认）*/
    private void showDeleteSelectedDialog() {
        if (projectAdapter == null) return;
        List<ImageProject> selected = projectAdapter.getSelectedProjects();
        if (selected.isEmpty()) {
            Toast.makeText(this, "请先选择要删除的项目", Toast.LENGTH_SHORT).show();
            return;
        }
        new android.app.AlertDialog.Builder(this)
                .setTitle("删除项目")
                .setMessage("确定要删除选中的 " + selected.size() + " 个项目吗？\n此操作不可撤销。")
                .setPositiveButton("删除", (d, w) -> {
                    ProjectManager.deleteProjects(this, selected);
                    initProjectList();
                    toggleMultiSelectMode(); // 退出多选模式
                    Toast.makeText(this, "已删除 " + selected.size() + " 个项目", Toast.LENGTH_SHORT).show();
                })
                .setNegativeButton("取消", null)
                .show();
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
            toolbar.setTitle("选择要导出到微信的项目");
        } else {
            if (exportPanel != null) {
                exportPanel.setVisibility(View.GONE);
            }
            if (projectAdapter != null) {
                projectAdapter.setSelectMode(false);
                projectAdapter.clearSelections();
            }
            Toolbar toolbar = findViewById(R.id.toolbar);
            toolbar.setTitle("项目列表");
        }
    }

    // 显示微信导出选择对话框
    private void showWechatExportDialog() {
        android.util.Log.d("ExportDebug", "显示导出选择对话框");
        
        if (projectAdapter == null) {
            Toast.makeText(this, "适配器未初始化", Toast.LENGTH_SHORT).show();
            return;
        }
        
        List<ImageProject> selectedProjects = projectAdapter.getSelectedProjects();
        android.util.Log.d("ExportDebug", "选中项目数量：" + selectedProjects.size());
        
        if (selectedProjects.isEmpty()) {
            Toast.makeText(this, "请先选择要导出的项目", Toast.LENGTH_SHORT).show();
            return;
        }
        
        // 显示导出方式选择对话框
        new android.app.AlertDialog.Builder(this)
            .setTitle("选择导出方式")
            .setMessage("已选择 " + selectedProjects.size() + " 个项目")
            .setPositiveButton("逐个导出", (dialog, which) -> {
                exportProjectsIndividually(selectedProjects);
            })
            .setNegativeButton("总包导出", (dialog, which) -> {
                exportProjectsAsBundle(selectedProjects);
            })
            .setNeutralButton("取消", null)
            .show();
    }
    
    // 逐个导出项目到微信
    private void exportProjectsIndividually(List<ImageProject> projects) {
        android.util.Log.d("ExportDebug", "开始逐个导出，共" + projects.size() + "个项目");
        
        ProgressDialog progressDialog = new ProgressDialog(this);
        progressDialog.setTitle("导出项目");
        progressDialog.setMessage("正在导出项目...");
        progressDialog.setProgressStyle(ProgressDialog.STYLE_HORIZONTAL);
        progressDialog.setMax(projects.size());
        progressDialog.setCancelable(false);
        progressDialog.show();
        
        new Thread(() -> {
            int successCount = 0;
            StringBuilder errorLog = new StringBuilder();
            java.util.List<String> exportedFiles = new java.util.ArrayList<>();
            
            for (int i = 0; i < projects.size(); i++) {
                ImageProject project = projects.get(i);
                final int currentIndex = i;
                
                runOnUiThread(() -> {
                    progressDialog.setProgress(currentIndex);
                    progressDialog.setMessage("正在导出: " + project.getProjectName());
                });
                
                try {
                    String zipPath = ProjectManager.exportProject(HomeActivity.this, project);
                    if (zipPath != null) {
                        exportedFiles.add(zipPath);
                        successCount++;
                        android.util.Log.d("ExportDebug", "成功导出：" + zipPath);
                    } else {
                        errorLog.append(project.getProjectName()).append(": 导出失败\n");
                    }
                } catch (Exception e) {
                    android.util.Log.e("ExportDebug", "导出异常: " + project.getProjectName(), e);
                    errorLog.append(project.getProjectName()).append(": ").append(e.getMessage()).append("\n");
                }
            }
            
            final int finalSuccessCount = successCount;
            final String finalErrorLog = errorLog.toString();
            final int finalTotal = projects.size();
            final java.util.List<String> finalExportedFiles = new java.util.ArrayList<>(exportedFiles);
            
            runOnUiThread(() -> {
                progressDialog.dismiss();
                
                if (finalSuccessCount == 0) {
                    new android.app.AlertDialog.Builder(this)
                        .setTitle("导出失败")
                        .setMessage("无法导出项目\n\n错误详情：\n" + finalErrorLog)
                        .setPositiveButton("确定", null)
                        .show();
                } else if (finalSuccessCount < finalTotal) {
                    String message = String.format("导出完成：成功%d个，失败%d个\n\n失败详情：\n%s", 
                        finalSuccessCount, finalTotal - finalSuccessCount, finalErrorLog);
                    new android.app.AlertDialog.Builder(this)
                        .setTitle("部分导出成功")
                        .setMessage(message)
                        .setPositiveButton("分享到微信", (d, w) -> shareFilesToWechat(finalExportedFiles))
                        .setNegativeButton("取消", null)
                        .show();
                } else {
                    // 全部成功，直接分享
                    shareFilesToWechat(finalExportedFiles);
                }
                
                // 导出完成后退出多选模式
                toggleMultiSelectMode();
            });
        }).start();
    }
    
    // 总包导出项目到微信
    private void exportProjectsAsBundle(List<ImageProject> projects) {
        android.util.Log.d("ExportDebug", "开始总包导出，共" + projects.size() + "个项目");
        
        ProgressDialog progressDialog = new ProgressDialog(this);
        progressDialog.setTitle("总包导出");
        progressDialog.setMessage("正在打包项目...");
        progressDialog.setCancelable(false);
        progressDialog.show();
        
        new Thread(() -> {
            try {
                // 1. 先导出每个项目为.gs文件
                java.util.List<File> gsFiles = new java.util.ArrayList<>();
                for (ImageProject project : projects) {
                    String gsPath = ProjectManager.exportProject(HomeActivity.this, project);
                    if (gsPath != null) {
                        gsFiles.add(new File(gsPath));
                    }
                }
                
                if (gsFiles.isEmpty()) {
                    runOnUiThread(() -> {
                        progressDialog.dismiss();
                        Toast.makeText(this, "导出失败：无法生成项目文件", Toast.LENGTH_SHORT).show();
                    });
                    return;
                }
                
                // 2. 将所有.gs文件打包成一个zip
                String bundleFileName = "项目合集_" + System.currentTimeMillis() + ".zip";
                String bundlePath = FileUtils.getExportDir(this) + "/" + bundleFileName;
                
                boolean success = FileUtils.zipFiles(gsFiles.toArray(new File[0]), bundlePath);
                
                // 3. 清理临时的.gs文件
                for (File gsFile : gsFiles) {
                    gsFile.delete();
                }
                
                if (success) {
                    File bundleFile = new File(bundlePath);
                    runOnUiThread(() -> {
                        progressDialog.dismiss();
                        // 分享总包文件
                        java.util.List<String> bundleList = new java.util.ArrayList<>();
                        bundleList.add(bundlePath);
                        shareFilesToWechat(bundleList);
                        toggleMultiSelectMode();
                    });
                } else {
                    runOnUiThread(() -> {
                        progressDialog.dismiss();
                        Toast.makeText(this, "总包导出失败", Toast.LENGTH_SHORT).show();
                    });
                }
                
            } catch (Exception e) {
                android.util.Log.e("ExportDebug", "总包导出异常", e);
                runOnUiThread(() -> {
                    progressDialog.dismiss();
                    Toast.makeText(this, "总包导出失败: " + e.getMessage(), Toast.LENGTH_SHORT).show();
                });
            }
        }).start();
    }
    
    // 分享文件列表到微信
    private void shareFilesToWechat(java.util.List<String> filePaths) {
        if (filePaths == null || filePaths.isEmpty()) {
            Toast.makeText(this, "没有可分享的文件", Toast.LENGTH_SHORT).show();
            return;
        }
        
        try {
            if (filePaths.size() == 1) {
                // 单个文件，直接分享
                File file = new File(filePaths.get(0));
                Uri contentUri = FileProvider.getUriForFile(
                    HomeActivity.this,
                    getPackageName() + ".fileprovider",
                    file);
                
                Intent shareIntent = new Intent(Intent.ACTION_SEND);
                shareIntent.setType("application/octet-stream");
                shareIntent.putExtra(Intent.EXTRA_STREAM, contentUri);
                shareIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                shareIntent.setPackage("com.tencent.mm");
                
                try {
                    startActivity(shareIntent);
                } catch (android.content.ActivityNotFoundException e) {
                    shareIntent.setPackage(null);
                    startActivity(Intent.createChooser(shareIntent, "分享到..."));
                }
            } else {
                // 多个文件，使用ACTION_SEND_MULTIPLE
                java.util.ArrayList<Uri> uris = new java.util.ArrayList<>();
                for (String path : filePaths) {
                    File file = new File(path);
                    Uri uri = FileProvider.getUriForFile(
                        HomeActivity.this,
                        getPackageName() + ".fileprovider",
                        file);
                    uris.add(uri);
                }
                
                Intent shareIntent = new Intent(Intent.ACTION_SEND_MULTIPLE);
                shareIntent.setType("application/octet-stream");
                shareIntent.putParcelableArrayListExtra(Intent.EXTRA_STREAM, uris);
                shareIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                shareIntent.setPackage("com.tencent.mm");
                
                try {
                    startActivity(shareIntent);
                } catch (android.content.ActivityNotFoundException e) {
                    shareIntent.setPackage(null);
                    startActivity(Intent.createChooser(shareIntent, "分享到..."));
                }
            }
        } catch (Exception e) {
            android.util.Log.e("ExportDebug", "分享失败", e);
            Toast.makeText(this, "分享失败: " + e.getMessage(), Toast.LENGTH_SHORT).show();
        }
    }

    // 批量导出选中项目（已废弃，保留用于兼容）
    @Deprecated
    private void exportAllSelectedProjects() {
        showWechatExportDialog();
    }

    // 批量导出到Downloads目录的方法（已废弃，保留用于兼容）
    @Deprecated
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

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.toolbar_menu, menu);
        return true;
    }

    private void initProjectList() {
        projectRecyclerView = findViewById(R.id.rv_project_list);
        
        // 加载全部项目后按当前分类过滤
        List<ImageProject> allProjects = ProjectManager.getProjects(this);
        List<ImageProject> filtered = new ArrayList<>();
        for (ImageProject p : allProjects) {
            if (currentCategoryId.equals(p.getCategoryId())) {
                filtered.add(p);
            }
        }
        
        if (projectList == null) projectList = new ArrayList<>();
        
        if (projectAdapter == null) {
            projectList = filtered;
            projectRecyclerView.setLayoutManager(new LinearLayoutManager(this));
            projectAdapter = new ProjectAdapter(projectList, new OnItemLongClickListener() {
                @Override
                public void onItemLongClick(ImageProject project, View view) {
                    showProjectPopupMenu(project, view);
                }
            });
            projectRecyclerView.setAdapter(projectAdapter);
        } else {
            projectAdapter.updateProjects(filtered);
            projectList = filtered;
        }
        
        // 刷新侧边栏数量
        refreshSidebar();
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

    /**
     * 确认并导入项目（从外部应用接收）
     */
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
    protected String getToolbarTitle() {
        return "项目列表";
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
                String[] options = {"重命名", "分享到微信", "移至分类"};
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
                                case 2: // 移至分类
                                    showMoveToCategory(project);
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

    // ===================== 分类侧边栏 Adapter =====================
    class CategoryAdapter extends RecyclerView.Adapter<CategoryAdapter.ViewHolder> {
        private List<CategoryManager.Category> categories;

        CategoryAdapter(List<CategoryManager.Category> categories) {
            this.categories = new ArrayList<>(categories);
        }

        void updateData(List<CategoryManager.Category> newList) {
            this.categories = new ArrayList<>(newList);
            notifyDataSetChanged();
        }

        @Override
        public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            View v = getLayoutInflater().inflate(R.layout.item_category, parent, false);
            return new ViewHolder(v);
        }

        @Override
        public void onBindViewHolder(ViewHolder holder, int position) {
            CategoryManager.Category cat = categories.get(position);
            holder.tvName.setText(cat.getName());

            // 计算该分类下的项目数
            int count = 0;
            List<ImageProject> all = ProjectManager.getProjects(HomeActivity.this);
            for (ImageProject p : all) {
                if (cat.getId().equals(p.getCategoryId())) count++;
            }
            holder.tvCount.setText(count > 0 ? String.valueOf(count) : "");

            // 选中态
            boolean isSelected = cat.getId().equals(currentCategoryId);
            holder.selectedBar.setVisibility(isSelected ? View.VISIBLE : View.INVISIBLE);
            holder.tvName.setTextColor(isSelected ? 0xFF00FFA3 : 0xFFCCCCCC);
            holder.itemView.setActivated(isSelected);

            // 点击切换分类
            holder.itemView.setOnClickListener(v -> {
                currentCategoryId = cat.getId();
                notifyDataSetChanged();
                refreshCategoryHeader();
                initProjectList();
                if (drawerLayout != null) drawerLayout.closeDrawers();
            });

            // 长按：重命名或删除
            holder.itemView.setOnLongClickListener(v -> {
                String[] options;
                if (CategoryManager.DEFAULT_CATEGORY_ID.equals(cat.getId())) {
                    options = new String[]{"重命名"};
                } else {
                    options = new String[]{"重命名", "删除分类"};
                }
                new android.app.AlertDialog.Builder(HomeActivity.this)
                        .setTitle(cat.getName())
                        .setItems(options, (d, which) -> {
                            if (which == 0) {
                                showRenameCategoryDialog(cat, holder.getAdapterPosition());
                            } else if (which == 1) {
                                confirmDeleteCategory(cat, holder.getAdapterPosition());
                            }
                        })
                        .show();
                return true;
            });
        }

        @Override
        public int getItemCount() { return categories.size(); }

        class ViewHolder extends RecyclerView.ViewHolder {
            TextView tvName, tvCount;
            View selectedBar;
            ViewHolder(View itemView) {
                super(itemView);
                tvName = itemView.findViewById(R.id.tv_category_name);
                tvCount = itemView.findViewById(R.id.tv_category_count);
                selectedBar = itemView.findViewById(R.id.category_selected_bar);
            }
        }
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
            if (drawerLayout != null && (drawerLayout.isDrawerOpen(GravityCompat.START) || drawerLayout.isDrawerOpen(GravityCompat.END))) {
                drawerLayout.closeDrawers();
                return true;
            }
            Intent intent = new Intent(HomeActivity.this, WelcomeActivity.class);
            startActivity(intent);
            finish();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onBackPressed() {
        if (drawerLayout != null && (drawerLayout.isDrawerOpen(GravityCompat.START) || drawerLayout.isDrawerOpen(GravityCompat.END))) {
            drawerLayout.closeDrawers();
        } else if (isMultiSelectMode) {
            toggleMultiSelectMode();
        } else {
            super.onBackPressed();
        }
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

    // 显示蓝牙连接状态对话框
    private void showBluetoothStatusDialog() {
        String connectedDevice = "模拟蓝牙设备";

        new android.app.AlertDialog.Builder(this)
            .setTitle("蓝牙连接状态")
            .setMessage("当前连接的设备：\n" + connectedDevice)
            .setPositiveButton("确定", null)
            .show();
    }

    private void initSettingsDrawer() {
        // 关闭按钮
        View btnClose = findViewById(R.id.btn_close_settings_drawer);
        if (btnClose != null) {
            btnClose.setOnClickListener(v -> closeSettingsDrawer());
        }

        // 调试终端
        View btnDebugTerminal = findViewById(R.id.btn_debug_terminal);
        if (btnDebugTerminal != null) {
            btnDebugTerminal.setOnClickListener(v -> {
                closeSettingsDrawer();
                startActivity(new Intent(HomeActivity.this, DebugTerminalActivity.class));
            });
        }

        // OTA固件升级
        View btnOtaUpgrade = findViewById(R.id.btn_ota_upgrade);
        if (btnOtaUpgrade != null) {
            btnOtaUpgrade.setOnClickListener(v -> {
                closeSettingsDrawer();
                startActivity(new Intent(HomeActivity.this, FirmwareUpgradeActivity.class));
            });
        }

        // 电池曲线矫正
        View btnBatteryCurve = findViewById(R.id.btn_battery_curve);
        if (btnBatteryCurve != null) {
            btnBatteryCurve.setOnClickListener(v -> {
                closeSettingsDrawer();
                startActivity(new Intent(HomeActivity.this, BatteryCurveActivity.class));
            });
        }

        // 版本号信息
        try {
            android.content.pm.PackageInfo pi = getPackageManager().getPackageInfo(getPackageName(), 0);
            TextView tvAppVersion = findViewById(R.id.tv_app_version);
            if (tvAppVersion != null) {
                tvAppVersion.setText(pi.versionName != null ? pi.versionName : "1.0.0");
            }
            TextView tvBuildNumber = findViewById(R.id.tv_build_number);
            if (tvBuildNumber != null) {
                tvBuildNumber.setText(String.valueOf(pi.versionCode));
            }
            TextView tvAppName = findViewById(R.id.tv_app_name);
            if (tvAppName != null) {
                tvAppName.setText(getString(pi.applicationInfo.labelRes));
            }
        } catch (Exception e) {
            Log.w("HomeActivity", "Failed to get package info", e);
        }
    }

    private void openSettingsDrawer() {
        if (drawerLayout != null) {
            drawerLayout.openDrawer(GravityCompat.END);
        }
    }

    private void closeSettingsDrawer() {
        if (drawerLayout != null) {
            drawerLayout.closeDrawer(GravityCompat.END);
        }
    }

    private void showTerminalDialog() {
        androidx.appcompat.app.AlertDialog.Builder builder = new androidx.appcompat.app.AlertDialog.Builder(this);
        builder.setTitle("BanBox BLE终端");
        android.widget.LinearLayout layout = new android.widget.LinearLayout(this);
        layout.setOrientation(android.widget.LinearLayout.VERTICAL);
        layout.setPadding(32, 32, 32, 32);

        android.widget.ScrollView scrollView = new android.widget.ScrollView(this);
        scrollView.setFillViewport(true);
        scrollView.setVerticalScrollBarEnabled(true);
        TextView rxBox = new TextView(this);
        rxBox.setHint("接收区");
        rxBox.setMinHeight(200);
        rxBox.setPadding(8, 8, 8, 8);
        rxBox.setBackgroundColor(0xFFEFEFEF);
        rxBox.setTextIsSelectable(true);
        scrollView.addView(rxBox, new android.widget.LinearLayout.LayoutParams(
                android.widget.LinearLayout.LayoutParams.MATCH_PARENT, android.widget.LinearLayout.LayoutParams.WRAP_CONTENT));
        layout.addView(scrollView, new android.widget.LinearLayout.LayoutParams(
                android.widget.LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f));

        android.widget.LinearLayout inputLayout = new android.widget.LinearLayout(this);
        inputLayout.setOrientation(android.widget.LinearLayout.HORIZONTAL);
        inputLayout.setPadding(0, 16, 0, 0);
        android.widget.EditText txBox = new android.widget.EditText(this);
        txBox.setHint("输入命令...");
        txBox.setMinHeight(80);
        txBox.setPadding(8, 8, 8, 8);
        android.widget.LinearLayout.LayoutParams txParams = new android.widget.LinearLayout.LayoutParams(0, android.widget.LinearLayout.LayoutParams.WRAP_CONTENT, 1f);
        txParams.rightMargin = 16;
        inputLayout.addView(txBox, txParams);
        Button sendBtn = new Button(this);
        sendBtn.setText("发送");
        inputLayout.addView(sendBtn, new android.widget.LinearLayout.LayoutParams(
                android.widget.LinearLayout.LayoutParams.WRAP_CONTENT, android.widget.LinearLayout.LayoutParams.WRAP_CONTENT));
        layout.addView(inputLayout, new android.widget.LinearLayout.LayoutParams(
                android.widget.LinearLayout.LayoutParams.MATCH_PARENT, android.widget.LinearLayout.LayoutParams.WRAP_CONTENT));

        builder.setView(layout);
        builder.setNegativeButton("关闭", null);
        androidx.appcompat.app.AlertDialog dialog = builder.create();
        dialog.show();

        sendBtn.setOnClickListener(v -> {
            String cmd = txBox.getText().toString();
            if (cmd.isEmpty()) return;
            String cmdWithCRLF = cmd + "\r\n";
            bluetoothHelper.writeCharacteristic("0000ab01-0000-1000-8000-00805f9b34fb", cmdWithCRLF.getBytes(), success -> {
                runOnUiThread(() -> {
                    if (success) {
                        rxBox.append("[TX] " + cmd + "\n");
                        txBox.setText("");
                    } else {
                        rxBox.append("[TX] 发送失败\n");
                    }
                    scrollView.post(() -> scrollView.fullScroll(View.FOCUS_DOWN));
                });
            });
        });

        bluetoothHelper.setBleNotifyListener((data) -> {
            runOnUiThread(() -> {
                rxBox.append("[RX] " + data + "\n");
                scrollView.post(() -> scrollView.fullScroll(View.FOCUS_DOWN));
            });
        });
    }
}