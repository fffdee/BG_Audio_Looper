package com.example.myapplication;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.TextView;
import androidx.recyclerview.widget.RecyclerView;
import java.util.ArrayList;
import java.util.List;

public class ProjectAdapter extends RecyclerView.Adapter<ProjectAdapter.ViewHolder> {
    private List<ImageProject> projectList;
    private OnItemLongClickListener longClickListener;
    private boolean isSelectMode = false;

    public interface OnItemLongClickListener {
        void onItemLongClick(ImageProject project, View view);
    }

    public ProjectAdapter(List<ImageProject> list, OnItemLongClickListener listener) {
        projectList = list;
        longClickListener = listener;
    }

    public void setSelectMode(boolean selectMode) {
        isSelectMode = selectMode;
        notifyDataSetChanged();
    }

    public List<ImageProject> getSelectedProjects() {
        List<ImageProject> selected = new ArrayList<>();
        if (projectList == null) return selected;
        for (ImageProject project : projectList) {
            if (project != null && project.isSelected()) {
                selected.add(project);
                android.util.Log.d("ExportCheck", "选中项目：" + project.getProjectName());
            }
        }
        return selected;
    }

    public void clearSelections() {
        for (ImageProject project : projectList) {
            project.setSelected(false);
        }
        notifyDataSetChanged();
    }

    @Override
    public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext())
                .inflate(R.layout.item_project, parent, false);
        return new ViewHolder(view);
    }

    @Override
    public void onBindViewHolder(ViewHolder holder, int position) {
        ImageProject project = projectList.get(position);
        holder.tvName.setText(project.getProjectName());
        holder.tvTime.setText(String.valueOf(project.getCreateTime()));
        holder.checkBox.setVisibility(isSelectMode ? View.VISIBLE : View.GONE);
        // 先移除监听，设置状态，再添加监听，确保同步
        holder.checkBox.setOnCheckedChangeListener(null);
        holder.checkBox.setChecked(project.isSelected());
        holder.checkBox.setOnCheckedChangeListener((buttonView, isChecked) -> {
            project.setSelected(isChecked);
            android.util.Log.d("ExportCheck", "勾选框更新：" + project.getProjectName() + "=" + isChecked);
        });
        holder.itemView.setOnClickListener(v -> {
            if (isSelectMode) {
                boolean newState = !project.isSelected();
                project.setSelected(newState);
                holder.checkBox.setChecked(newState);
                notifyItemChanged(holder.getAdapterPosition());
                android.util.Log.d("ExportCheck", "项目点击更新：" + project.getProjectName() + "=" + newState);
            } else {
                // 点击播放项目
                android.content.Context context = v.getContext();
                
                android.util.Log.d("ProjectAdapter", "===== 点击项目 =====");
                android.util.Log.d("ProjectAdapter", "项目名: " + project.getProjectName());
                
                java.util.List<String> imagePaths = project.getImagePaths();
                android.util.Log.d("ProjectAdapter", "imagePaths 是否为null: " + (imagePaths == null));
                
                if (imagePaths == null || imagePaths.isEmpty()) {
                    android.util.Log.e("ProjectAdapter", "错误：图片列表为空");
                    android.widget.Toast.makeText(context, "项目图片列表为空", android.widget.Toast.LENGTH_SHORT).show();
                    return;
                }
                
                android.util.Log.d("ProjectAdapter", "图片数量: " + imagePaths.size());
                for (int i = 0; i < imagePaths.size(); i++) {
                    android.util.Log.d("ProjectAdapter", "  图片 " + (i+1) + ": " + imagePaths.get(i));
                }
                
                // 验证图片文件是否存在
                boolean allExists = true;
                for (String path : imagePaths) {
                    if (path == null || !new java.io.File(path).exists()) {
                        android.util.Log.w("ProjectAdapter", "图片不存在: " + path);
                        allExists = false;
                        break;
                    }
                }
                if (!allExists) {
                    android.widget.Toast.makeText(context, "部分图片文件丢失", android.widget.Toast.LENGTH_SHORT).show();
                    return;
                }
                
                android.util.Log.d("ProjectAdapter", "准备启动ProjectDetailActivity");
                android.content.Intent intent = new android.content.Intent(context, ProjectDetailActivity.class);
                intent.putStringArrayListExtra("image_paths", new java.util.ArrayList<>(imagePaths));
                context.startActivity(intent);
            }
        });
        holder.itemView.setOnLongClickListener(v -> {
            if (longClickListener != null) {
                longClickListener.onItemLongClick(project, v);
            }
            return true;
        });
    }

    @Override
    public int getItemCount() {
        return projectList.size();
    }

    static class ViewHolder extends RecyclerView.ViewHolder {
        TextView tvName, tvTime;
        CheckBox checkBox;
        public ViewHolder(View itemView) {
            super(itemView);
            tvName = itemView.findViewById(R.id.tv_project_name);
            tvTime = itemView.findViewById(R.id.tv_project_time);
            checkBox = itemView.findViewById(R.id.cb_select);
        }
    }
}
