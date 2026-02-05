package com.example.myapplication;

import java.io.Serializable;
import java.util.List;

public class ImageProject implements Serializable {
    private String projectName;
    private List<String> imagePaths;
    private String mergedImagePath;
    private long createTime;

    // 添加选中状态
    private boolean isSelected = false;

    public ImageProject(String projectName, List<String> imagePaths, String mergedImagePath) {
        this.projectName = projectName;
        this.imagePaths = imagePaths;
        this.mergedImagePath = mergedImagePath;
        this.createTime = System.currentTimeMillis();
    }

    public String getProjectName() { return projectName; }
    public void setProjectName(String projectName) { this.projectName = projectName; }
    public List<String> getImagePaths() { return imagePaths; }
    public void setImagePaths(List<String> imagePaths) { this.imagePaths = imagePaths; }
    public String getMergedImagePath() { return mergedImagePath; }
    public long getCreateTime() { return createTime; }
    public boolean isSelected() { return isSelected; }
    public void setSelected(boolean selected) { isSelected = selected; }
}
