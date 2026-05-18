package com.example.myapplication;

import java.io.Serializable;
import java.util.List;

public class ImageProject implements Serializable {
    private String projectName;
    private List<String> imagePaths;
    private String mergedImagePath;
    private long createTime;

    // 添加选中状态（仅内存使用，不序列化到持久化）
    private transient boolean isSelected = false;

    // 所属分类ID
    private String categoryId = CategoryManager.DEFAULT_CATEGORY_ID;

    public ImageProject(String projectName, List<String> imagePaths, String mergedImagePath) {
        this.projectName = projectName;
        this.imagePaths = imagePaths;
        this.mergedImagePath = mergedImagePath;
        this.createTime = System.currentTimeMillis();
        this.categoryId = CategoryManager.DEFAULT_CATEGORY_ID;
    }

    public String getProjectName() { return projectName; }
    public void setProjectName(String projectName) { this.projectName = projectName; }
    public List<String> getImagePaths() { return imagePaths; }
    public void setImagePaths(List<String> imagePaths) { this.imagePaths = imagePaths; }
    public String getMergedImagePath() { return mergedImagePath; }
    public long getCreateTime() { return createTime; }
    public boolean isSelected() { return isSelected; }
    public void setSelected(boolean selected) { isSelected = selected; }
    public String getCategoryId() {
        return categoryId != null ? categoryId : CategoryManager.DEFAULT_CATEGORY_ID;
    }
    public void setCategoryId(String categoryId) {
        this.categoryId = categoryId != null ? categoryId : CategoryManager.DEFAULT_CATEGORY_ID;
    }
}
