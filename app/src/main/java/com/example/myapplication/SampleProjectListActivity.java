package com.example.myapplication;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.core.content.FileProvider;
import androidx.recyclerview.widget.DividerItemDecoration;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import java.io.File;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/**
 * 采样创作项目列表界面
 * - 展示 Music/BanBox/Projects/ 下所有已保存的项目
 * - 点击项目 → 返回 PROJECT_NAME 给 SampleCreatorActivity 加载
 * - 分享按钮 → 打包 ZIP 后通过系统分享面板发送
 * - 导入按钮 → 选择 ZIP 文件导入项目
 * - 删除按钮 → 确认后删除
 */
public class SampleProjectListActivity extends BaseActivity {

    public static final String EXTRA_PROJECT_NAME = "extra_project_name";

    private RecyclerView rvProjects;
    private TextView tvEmpty;
    private ProjectsAdapter adapter;
    private final List<ProjectInfo> projectInfoList = new ArrayList<>();

    /** 选择 ZIP 文件用于导入 */
    private final ActivityResultLauncher<Intent> importZipLauncher =
            registerForActivityResult(new ActivityResultContracts.StartActivityForResult(), result -> {
                if (result.getResultCode() == Activity.RESULT_OK && result.getData() != null) {
                    Uri zipUri = result.getData().getData();
                    if (zipUri != null) {
                        doImportProject(zipUri);
                    }
                }
            });

    @Override
    protected String getToolbarTitle() {
        return "我的项目";
    }

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_sample_project_list);
        setupBaseToolbar(true);

        rvProjects = findViewById(R.id.rv_projects);
        tvEmpty    = findViewById(R.id.tv_empty);
        Button btnImport = findViewById(R.id.btn_import_project);

        rvProjects.setLayoutManager(new LinearLayoutManager(this));
        rvProjects.addItemDecoration(
                new DividerItemDecoration(this, DividerItemDecoration.VERTICAL));

        adapter = new ProjectsAdapter(projectInfoList,
                this::onProjectClicked,
                this::onShareClicked,
                this::onDeleteClicked);
        rvProjects.setAdapter(adapter);

        btnImport.setOnClickListener(v -> openZipPicker());

        loadProjects();
    }

    @Override
    protected void onResume() {
        super.onResume();
        loadProjects();
    }

    private void loadProjects() {
        new Thread(() -> {
            List<String> names = SampleProjectManager.listProjects();
            List<ProjectInfo> infos = new ArrayList<>();
            for (String name : names) {
                SampleProject meta = SampleProjectManager.loadProjectMeta(name);
                ProjectInfo info = new ProjectInfo();
                info.name = name;
                if (meta != null) {
                    info.createTime = meta.getCreateTime();
                    info.clipCount  = meta.getTotalClipCount();
                    info.trackCount = meta.getTracks().size();
                }
                infos.add(info);
            }
            runOnUiThread(() -> {
                projectInfoList.clear();
                projectInfoList.addAll(infos);
                adapter.notifyDataSetChanged();
                tvEmpty.setVisibility(projectInfoList.isEmpty() ? View.VISIBLE : View.GONE);
                rvProjects.setVisibility(projectInfoList.isEmpty() ? View.GONE : View.VISIBLE);
            });
        }).start();
    }

    // ==================== 打开项目 ====================

    private void onProjectClicked(ProjectInfo info) {
        Intent result = new Intent();
        result.putExtra(EXTRA_PROJECT_NAME, info.name);
        setResult(Activity.RESULT_OK, result);
        finish();
    }

    // ==================== 分享 ZIP ====================

    private void onShareClicked(ProjectInfo info) {
        Toast.makeText(this, "正在打包 " + info.name + "...", Toast.LENGTH_SHORT).show();
        new Thread(() -> {
            File zipFile = SampleProjectManager.exportProjectZip(info.name);
            runOnUiThread(() -> {
                if (zipFile == null) {
                    Toast.makeText(this, "打包失败，请重试", Toast.LENGTH_SHORT).show();
                    return;
                }
                shareZipFile(zipFile, info.name);
            });
        }).start();
    }

    private void shareZipFile(File zipFile, String projectName) {
        Uri zipUri = FileProvider.getUriForFile(this,
                getPackageName() + ".fileprovider", zipFile);

        Intent shareIntent = new Intent(Intent.ACTION_SEND);
        shareIntent.setType("application/zip");
        shareIntent.putExtra(Intent.EXTRA_STREAM, zipUri);
        shareIntent.putExtra(Intent.EXTRA_SUBJECT, "采样创作项目: " + projectName);
        shareIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);

        startActivity(Intent.createChooser(shareIntent, "分享项目 " + projectName));
    }

    // ==================== 删除项目 ====================

    private void onDeleteClicked(ProjectInfo info) {
        new AlertDialog.Builder(this)
                .setTitle("删除项目")
                .setMessage("确定删除「" + info.name + "」吗？\n此操作无法撤销，项目文件夹及所有采样将被删除。")
                .setPositiveButton("删除", (dialog, which) -> {
                    boolean ok = SampleProjectManager.deleteProject(info.name);
                    Toast.makeText(this,
                            ok ? "已删除: " + info.name : "删除失败",
                            Toast.LENGTH_SHORT).show();
                    loadProjects();
                })
                .setNegativeButton("取消", null)
                .show();
    }

    // ==================== 导入 ZIP ====================

    private void openZipPicker() {
        Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
        intent.setType("application/zip");
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        // 也接受 application/octet-stream（部分文件管理器对 zip 报此 MIME）
        Intent chooser = Intent.createChooser(intent, "选择项目 ZIP 文件");
        importZipLauncher.launch(chooser);
    }

    private void doImportProject(Uri zipUri) {
        Toast.makeText(this, "正在导入...", Toast.LENGTH_SHORT).show();
        new Thread(() -> {
            String importedName = SampleProjectManager.importProjectZip(this, zipUri);
            runOnUiThread(() -> {
                if (importedName == null) {
                    Toast.makeText(this, "导入失败：请确保所选文件是有效的项目 ZIP", Toast.LENGTH_LONG).show();
                } else {
                    Toast.makeText(this, "已导入项目: " + importedName, Toast.LENGTH_LONG).show();
                    loadProjects();
                }
            });
        }).start();
    }

    // ========== 数据类 ==========

    static class ProjectInfo {
        String name;
        long createTime;
        int clipCount;
        int trackCount;
    }

    // ========== Adapter ==========

    static class ProjectsAdapter extends RecyclerView.Adapter<ProjectsAdapter.VH> {
        interface OnClickListener { void onClick(ProjectInfo info); }

        private final List<ProjectInfo> list;
        private final OnClickListener onOpen;
        private final OnClickListener onShare;
        private final OnClickListener onDelete;

        ProjectsAdapter(List<ProjectInfo> list,
                        OnClickListener onOpen,
                        OnClickListener onShare,
                        OnClickListener onDelete) {
            this.list     = list;
            this.onOpen   = onOpen;
            this.onShare  = onShare;
            this.onDelete = onDelete;
        }

        @NonNull
        @Override
        public VH onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
            View v = LayoutInflater.from(parent.getContext())
                    .inflate(R.layout.item_sample_project, parent, false);
            return new VH(v);
        }

        @Override
        public void onBindViewHolder(@NonNull VH holder, int position) {
            ProjectInfo info = list.get(position);
            holder.tvName.setText(info.name);
            holder.tvClips.setText(info.trackCount + " 轨 · " + info.clipCount + " 片段");
            if (info.createTime > 0) {
                holder.tvDate.setText(new SimpleDateFormat("yyyy-MM-dd HH:mm",
                        Locale.getDefault()).format(new Date(info.createTime)));
            } else {
                holder.tvDate.setText("—");
            }
            holder.itemView.setOnClickListener(v -> onOpen.onClick(info));
            holder.btnShare.setOnClickListener(v -> onShare.onClick(info));
            holder.btnDelete.setOnClickListener(v -> onDelete.onClick(info));
        }

        @Override
        public int getItemCount() { return list.size(); }

        static class VH extends RecyclerView.ViewHolder {
            TextView tvName, tvClips, tvDate;
            ImageButton btnShare, btnDelete;
            VH(@NonNull View v) {
                super(v);
                tvName    = v.findViewById(R.id.tv_project_name);
                tvClips   = v.findViewById(R.id.tv_project_clips);
                tvDate    = v.findViewById(R.id.tv_project_date);
                btnShare  = v.findViewById(R.id.btn_share_project);
                btnDelete = v.findViewById(R.id.btn_delete_project);
            }
        }
    }
}
