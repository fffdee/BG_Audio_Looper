package com.example.myapplication;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Environment;
import com.google.gson.Gson;
import com.google.gson.reflect.TypeToken;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.Reader;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.List;
import java.lang.reflect.Type;
import java.io.FileInputStream;
import java.io.FileOutputStream;

public class ProjectManager {
    private static final String PREF_NAME = "image_projects";
    private static final String KEY_PROJECTS = "projects";
    
    // 添加缓存机制，避免频繁JSON解析
    private static List<ImageProject> cachedProjects = null;
    private static long lastCacheTime = 0;
    private static final long CACHE_VALIDITY_MS = 2000; // 缓存有效期2秒

    public static void saveProject(Context context, ImageProject project) {
        android.util.Log.d("ProjectManager", "saveProject - 项目名: " + project.getProjectName());
        android.util.Log.d("ProjectManager", "saveProject - 图片数: " + 
                          (project.getImagePaths() != null ? project.getImagePaths().size() : "null"));
        if (project.getImagePaths() != null) {
            for (int i = 0; i < project.getImagePaths().size(); i++) {
                android.util.Log.d("ProjectManager", "  图片 " + (i+1) + ": " + project.getImagePaths().get(i));
            }
        }
        
        List<ImageProject> projects = getProjects(context);
        projects.add(project);
        
        String json = new Gson().toJson(projects);
        android.util.Log.d("ProjectManager", "JSON长度: " + json.length());
        
        SharedPreferences prefs = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
        boolean success = prefs.edit().putString(KEY_PROJECTS, json).commit();
        android.util.Log.d("ProjectManager", "保存结果: " + success);
        
        invalidateCache(); // 清除缓存
        
        // 验证保存后立即读取
        List<ImageProject> verifyProjects = getProjects(context);
        android.util.Log.d("ProjectManager", "验证 - 总项目数: " + verifyProjects.size());
        if (!verifyProjects.isEmpty()) {
            ImageProject lastProject = verifyProjects.get(verifyProjects.size() - 1);
            android.util.Log.d("ProjectManager", "验证 - 最后一个项目: " + lastProject.getProjectName());
            android.util.Log.d("ProjectManager", "验证 - 图片数: " + 
                              (lastProject.getImagePaths() != null ? lastProject.getImagePaths().size() : "null"));
        }
    }

    public static List<ImageProject> getProjects(Context context) {
        // 检查缓存是否有效
        long now = System.currentTimeMillis();
        if (cachedProjects != null && (now - lastCacheTime) < CACHE_VALIDITY_MS) {
            return new ArrayList<>(cachedProjects); // 返回缓存的副本
        }
        
        // 从SharedPreferences读取并解析
        SharedPreferences prefs = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
        String json = prefs.getString(KEY_PROJECTS, "");
        if (json.isEmpty()) {
            cachedProjects = new ArrayList<>();
        } else {
            Type type = new TypeToken<List<ImageProject>>(){}.getType();
            cachedProjects = new Gson().fromJson(json, type);
        }
        
        lastCacheTime = now;
        return new ArrayList<>(cachedProjects); // 返回缓存的副本
    }
    
    // 清除缓存（在修改数据后调用）
    private static void invalidateCache() {
        cachedProjects = null;
        lastCacheTime = 0;
    }

    public static void deleteProject(Context context, ImageProject project) {
        List<ImageProject> projects = getProjects(context);
        // 按唯一标识删除（projectName + createTime）
        for (int i = 0; i < projects.size(); i++) {
            ImageProject p = projects.get(i);
            if (p.getProjectName().equals(project.getProjectName()) && p.getCreateTime() == project.getCreateTime()) {
                projects.remove(i);
                break;
            }
        }
        SharedPreferences prefs = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
        prefs.edit().putString(KEY_PROJECTS, new Gson().toJson(projects)).apply();
        invalidateCache(); // 清除缓存
    }

    /**
     * 重命名项目
     * @param context 上下文
     * @param project 要重命名的项目
     * @param newName 新名称
     * @return 是否重命名成功
     */
    public static boolean renameProject(Context context, ImageProject project, String newName) {
        if (newName == null || newName.trim().isEmpty()) {
            return false;
        }
        List<ImageProject> projects = getProjects(context);
        for (int i = 0; i < projects.size(); i++) {
            ImageProject p = projects.get(i);
            if (p.getProjectName().equals(project.getProjectName()) && p.getCreateTime() == project.getCreateTime()) {
                p.setProjectName(newName.trim());
                break;
            }
        }
        SharedPreferences prefs = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
        prefs.edit().putString(KEY_PROJECTS, new Gson().toJson(projects)).apply();
        invalidateCache();
        return true;
    }

    public static String exportProject(Context context, ImageProject project) {
        try {
            // 使用时间戳确保临时目录唯一性
            String tempDir = context.getCacheDir() + "/export_temp_" + System.currentTimeMillis();
            File tempDirFile = new File(tempDir);
            if (!tempDirFile.mkdirs() && !tempDirFile.exists()) {
                android.util.Log.e("ProjectManager", "无法创建临时目录: " + tempDir);
                return null;
            }
            
            // 创建项目信息JSON文件
            String projectJsonPath = tempDir + "/project_info.json";
            try (FileWriter writer = new FileWriter(projectJsonPath)) {
                new Gson().toJson(project, writer);
            }
            
            // 准备要压缩的文件列表
            List<File> filesToZip = new ArrayList<>();
            filesToZip.add(new File(projectJsonPath));
            
            // 检查并添加合并图片
            File mergedImageFile = new File(project.getMergedImagePath());
            if (mergedImageFile.exists()) {
                filesToZip.add(mergedImageFile);
            } else {
                android.util.Log.w("ProjectManager", "合并图片不存在: " + project.getMergedImagePath());
            }
            
            // 检查并添加原始图片
            for (String path : project.getImagePaths()) {
                File imageFile = new File(path);
                if (imageFile.exists()) {
                    filesToZip.add(imageFile);
                } else {
                    android.util.Log.w("ProjectManager", "原始图片不存在: " + path);
                }
            }
            
            // 创建项目文件（.gs格式）
            String exportDir = FileUtils.getExportDir(context);
            String zipFileName = project.getProjectName() + "_" + project.getCreateTime() + ".gs";
            String zipPath = exportDir + "/" + zipFileName;
            
            if (FileUtils.zipFiles(filesToZip.toArray(new File[0]), zipPath)) {
                android.util.Log.i("ProjectManager", "项目导出成功: " + zipPath);
                return zipPath;
            } else {
                android.util.Log.e("ProjectManager", "ZIP文件创建失败");
                return null;
            }
        } catch (Exception e) {
            android.util.Log.e("ProjectManager", "导出项目时发生异常", e);
        }
        return null;
    }

    public static boolean importProject(Context context, String zipFilePath) {
        try {
            android.util.Log.d("ProjectManager", "开始导入项目, ZIP路径: " + zipFilePath);
            
            // 检查ZIP文件是否存在
            File zipFile = new File(zipFilePath);
            if (!zipFile.exists() || zipFile.length() == 0) {
                android.util.Log.e("ProjectManager", "ZIP文件不存在或为空: " + zipFilePath);
                return false;
            }
            android.util.Log.d("ProjectManager", "ZIP文件大小: " + zipFile.length() + " bytes");
            
            // 使用独立的解压子目录，避免清空包含ZIP文件的父目录
            String tempDir = context.getCacheDir() + "/unzip_" + System.currentTimeMillis();
            File tempDirFile = new File(tempDir);
            if (tempDirFile.exists()) {
                FileUtils.deleteDir(tempDirFile);
            }
            tempDirFile.mkdirs();
            android.util.Log.d("ProjectManager", "临时解压目录: " + tempDir);
            
            if (!FileUtils.unzipFile(zipFilePath, tempDir)) {
                android.util.Log.e("ProjectManager", "解压ZIP失败");
                return false;
            }
            
            // 列出解压后的文件
            File[] extractedFiles = tempDirFile.listFiles();
            if (extractedFiles != null) {
                android.util.Log.d("ProjectManager", "解压出 " + extractedFiles.length + " 个文件:");
                for (File f : extractedFiles) {
                    android.util.Log.d("ProjectManager", "  - " + f.getName() + " (" + f.length() + " bytes)");
                }
            }
            
            File jsonFile = new File(tempDir + "/project_info.json");
            if (!jsonFile.exists()) {
                android.util.Log.e("ProjectManager", "project_info.json 不存在于解压目录中");
                return false;
            }
            
            ImageProject importedProject;
            try (Reader reader = new FileReader(jsonFile)) {
                Type type = new TypeToken<ImageProject>() {}.getType();
                importedProject = new Gson().fromJson(reader, type);
            }
            
            if (importedProject == null || importedProject.getProjectName() == null) {
                android.util.Log.e("ProjectManager", "项目JSON解析失败或项目名为空");
                return false;
            }
            android.util.Log.d("ProjectManager", "项目名: " + importedProject.getProjectName());
            
            String targetDir = getProjectImageDir(context, importedProject);
            new File(targetDir).mkdirs();
            
            // 处理合并图片（可能为null）
            String newMergedPath = null;
            if (importedProject.getMergedImagePath() != null && !importedProject.getMergedImagePath().isEmpty()) {
                String mergedImageName = new File(importedProject.getMergedImagePath()).getName();
                File mergedImageSrc = new File(tempDir + "/" + mergedImageName);
                File mergedImageDest = new File(targetDir + "/" + mergedImageName);
                if (mergedImageSrc.exists()) {
                    copyFile(mergedImageSrc, mergedImageDest);
                    newMergedPath = mergedImageDest.getAbsolutePath();
                    android.util.Log.d("ProjectManager", "合并图片已复制: " + newMergedPath);
                } else {
                    android.util.Log.w("ProjectManager", "合并图片源文件不存在: " + mergedImageSrc.getAbsolutePath());
                }
            }
            
            importedProject = new ImageProject(
                    importedProject.getProjectName(),
                    importedProject.getImagePaths(),
                    newMergedPath
            );
            
            // 复制原始图片
            List<String> newImagePaths = new ArrayList<>();
            if (importedProject.getImagePaths() != null) {
                for (String oldPath : importedProject.getImagePaths()) {
                    String fileName = new File(oldPath).getName();
                    File src = new File(tempDir + "/" + fileName);
                    File dest = new File(targetDir + "/" + fileName);
                    if (src.exists()) {
                        copyFile(src, dest);
                        newImagePaths.add(dest.getAbsolutePath());
                        android.util.Log.d("ProjectManager", "图片已复制: " + fileName);
                    } else {
                        android.util.Log.w("ProjectManager", "图片源文件不存在: " + src.getAbsolutePath());
                    }
                }
            }
            importedProject.setImagePaths(newImagePaths);
            saveProject(context, importedProject);
            android.util.Log.i("ProjectManager", "项目导入成功: " + importedProject.getProjectName());
            
            // 清理临时解压目录
            FileUtils.deleteDir(new File(tempDir));
            
            return true;
        } catch (Exception e) {
            android.util.Log.e("ProjectManager", "导入项目异常", e);
            e.printStackTrace();
            return false;
        }
    }

    private static String getProjectImageDir(Context context, ImageProject project) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            return context.getExternalFilesDir(Environment.DIRECTORY_PICTURES)
                    + "/ImagePlayer/" + project.getProjectName() + "_" + project.getCreateTime();
        } else {
            return Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_PICTURES)
                    + "/ImagePlayer/" + project.getProjectName() + "_" + project.getCreateTime();
        }
    }

    private static void copyFile(File src, File dest) throws IOException {
        try (FileInputStream fis = new FileInputStream(src);
             FileOutputStream fos = new FileOutputStream(dest)) {
            byte[] buffer = new byte[1024];
            int len;
            while ((len = fis.read(buffer)) > 0) {
                fos.write(buffer, 0, len);
            }
        }
    }
}
