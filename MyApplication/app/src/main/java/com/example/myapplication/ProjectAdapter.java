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
                android.content.Context context = v.getContext();
                String mergedImagePath = project.getMergedImagePath();
                if (mergedImagePath == null || !new java.io.File(mergedImagePath).exists()) {
                    android.widget.Toast.makeText(context, "项目图片损坏或丢失", android.widget.Toast.LENGTH_SHORT).show();
                    return;
                }
                android.content.Intent intent = new android.content.Intent(context, ProjectDetailActivity.class);
                intent.putExtra("merged_image_path", mergedImagePath);
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
