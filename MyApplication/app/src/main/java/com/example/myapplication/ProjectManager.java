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

    public static void saveProject(Context context, ImageProject project) {
        List<ImageProject> projects = getProjects(context);
        projects.add(project);
        SharedPreferences prefs = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
        prefs.edit().putString(KEY_PROJECTS, new Gson().toJson(projects)).apply();
    }

    public static List<ImageProject> getProjects(Context context) {
        SharedPreferences prefs = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
        String json = prefs.getString(KEY_PROJECTS, "");
        if (json.isEmpty()) return new ArrayList<>();
        Type type = new TypeToken<List<ImageProject>>(){}.getType();
        return new Gson().fromJson(json, type);
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
            
            // 创建ZIP文件
            String exportDir = FileUtils.getExportDir(context);
            String zipFileName = project.getProjectName() + "_" + project.getCreateTime() + ".zip";
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
            String tempDir = FileUtils.getTempImportDir(context);
            if (!FileUtils.unzipFile(zipFilePath, tempDir)) {
                return false;
            }
            File jsonFile = new File(tempDir + "/project_info.json");
            if (!jsonFile.exists()) {
                return false;
            }
            ImageProject importedProject;
            try (Reader reader = new FileReader(jsonFile)) {
                Type type = new TypeToken<ImageProject>() {}.getType();
                importedProject = new Gson().fromJson(reader, type);
            }
            String targetDir = getProjectImageDir(context, importedProject);
            new File(targetDir).mkdirs();
            String mergedImageName = new File(importedProject.getMergedImagePath()).getName();
            File mergedImageSrc = new File(tempDir + "/" + mergedImageName);
            File mergedImageDest = new File(targetDir + "/" + mergedImageName);
            copyFile(mergedImageSrc, mergedImageDest);
            importedProject = new ImageProject(
                    importedProject.getProjectName(),
                    importedProject.getImagePaths(),
                    mergedImageDest.getAbsolutePath()
            );
            List<String> newImagePaths = new ArrayList<>();
            for (String oldPath : importedProject.getImagePaths()) {
                String fileName = new File(oldPath).getName();
                File src = new File(tempDir + "/" + fileName);
                File dest = new File(targetDir + "/" + fileName);
                copyFile(src, dest);
                newImagePaths.add(dest.getAbsolutePath());
            }
            importedProject.setImagePaths(newImagePaths);
            saveProject(context, importedProject);
            return true;
        } catch (Exception e) {
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
