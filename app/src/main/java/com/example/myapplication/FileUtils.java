package com.example.myapplication;

import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;
import android.provider.MediaStore;
import android.util.Log;

import java.io.*;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

public class FileUtils {
    private static final String TAG = "FileUtils";

    public static boolean zipFiles(File[] sourceFiles, String zipFilePath) {
        if (sourceFiles == null || sourceFiles.length == 0) {
            Log.e(TAG, "没有要压缩的文件");
            return false;
        }
        
        Log.d(TAG, "开始创建ZIP文件: " + zipFilePath);
        Log.d(TAG, "要压缩的文件数量: " + sourceFiles.length);
        
        // 确保目标目录存在
        File zipFile = new File(zipFilePath);
        File parentDir = zipFile.getParentFile();
        if (parentDir != null && !parentDir.exists()) {
            if (!parentDir.mkdirs()) {
                Log.e(TAG, "无法创建目标目录: " + parentDir.getAbsolutePath());
                return false;
            }
        }
        
        try (ZipOutputStream zos = new ZipOutputStream(new FileOutputStream(zipFilePath))) {
            for (int i = 0; i < sourceFiles.length; i++) {
                File file = sourceFiles[i];
                Log.d(TAG, String.format("处理文件[%d/%d]: %s", i+1, sourceFiles.length, file.getName()));
                
                if (!file.exists()) {
                    Log.w(TAG, "文件不存在，跳过: " + file.getAbsolutePath());
                    continue;
                }
                
                if (!file.canRead()) {
                    Log.w(TAG, "文件无法读取，跳过: " + file.getAbsolutePath());
                    continue;
                }
                
                Log.d(TAG, "添加文件到ZIP: " + file.getName() + " (大小: " + file.length() + " bytes)");
                
                ZipEntry entry = new ZipEntry(file.getName());
                zos.putNextEntry(entry);
                
                try (FileInputStream fis = new FileInputStream(file)) {
                    byte[] buffer = new byte[8192];
                    int len;
                    long totalBytes = 0;
                    
                    while ((len = fis.read(buffer)) > 0) {
                        zos.write(buffer, 0, len);
                        totalBytes += len;
                    }
                    
                    Log.d(TAG, "文件压缩完成: " + file.getName() + " (压缩了 " + totalBytes + " bytes)");
                }
                
                zos.closeEntry();
            }
            
            zos.finish();
            Log.i(TAG, "ZIP文件创建成功: " + zipFilePath + " (大小: " + zipFile.length() + " bytes)");
            return true;
            
        } catch (Exception e) {
            Log.e(TAG, "压缩失败", e);
            try {
                // 删除不完整的ZIP文件
                if (zipFile.exists()) {
                    boolean deleted = zipFile.delete();
                    Log.d(TAG, "清理不完整的ZIP文件: " + deleted);
                }
            } catch (Exception ex) {
                Log.e(TAG, "清理临时文件失败", ex);
            }
            return false;
        }
    }

    public static boolean unzipFile(String zipFilePath, String destDir) {
        File destDirectory = new File(destDir);
        if (!destDirectory.exists()) {
            destDirectory.mkdirs();
        }
        try (ZipInputStream zis = new ZipInputStream(new FileInputStream(zipFilePath))) {
            ZipEntry entry;
            while ((entry = zis.getNextEntry()) != null) {
                File file = new File(destDir, entry.getName());
                if (entry.isDirectory()) {
                    file.mkdirs();
                } else {
                    try (FileOutputStream fos = new FileOutputStream(file)) {
                        byte[] buffer = new byte[1024];
                        int len;
                        while ((len = zis.read(buffer)) > 0) {
                            fos.write(buffer, 0, len);
                        }
                    }
                }
                zis.closeEntry();
            }
            return true;
        } catch (Exception e) {
            Log.e(TAG, "unzip error", e);
            return false;
        }
    }

    public static String getExportDir(Context context) {
        File dir = new File(context.getExternalFilesDir(Environment.DIRECTORY_DOCUMENTS), "ImageProjectExports");
        
        Log.d(TAG, "导出目录路径: " + dir.getAbsolutePath());
        
        if (!dir.exists()) {
            boolean created = dir.mkdirs();
            Log.d(TAG, "创建导出目录结果: " + created);
            
            if (!created) {
                Log.e(TAG, "无法创建导出目录");
                // 尝试使用缓存目录作为备选方案
                dir = new File(context.getCacheDir(), "ImageProjectExports");
                if (!dir.exists()) {
                    created = dir.mkdirs();
                    Log.d(TAG, "使用缓存目录作为备选，创建结果: " + created);
                }
            }
        }
        
        // 验证目录权限
        if (dir.exists()) {
            Log.d(TAG, "导出目录存在: " + dir.exists());
            Log.d(TAG, "导出目录可写: " + dir.canWrite());
            Log.d(TAG, "导出目录可读: " + dir.canRead());
        }
        
        return dir.getAbsolutePath();
    }

    public static String getTempImportDir(Context context) {
        File dir = new File(context.getCacheDir(), "TempImports");
        // 先清理旧内容，再重建目录
        if (dir.exists()) {
            deleteDir(dir);
        }
        dir.mkdirs();
        return dir.getAbsolutePath();
    }

    public static boolean deleteDir(File dir) {
        if (dir.isDirectory()) {
            File[] children = dir.listFiles();
            if (children != null) {
                for (File child : children) {
                    deleteDir(child);
                }
            }
        }
        return dir.delete();
    }

    public static String getPathFromUri(Context context, Uri uri) {
        if (uri == null) return null;
        if ("content".equals(uri.getScheme())) {
            String[] projection = {MediaStore.Files.FileColumns.DATA};
            Cursor cursor = context.getContentResolver().query(uri, projection, null, null, null);
            if (cursor != null) {
                if (cursor.moveToFirst()) {
                    int columnIndex = cursor.getColumnIndexOrThrow(MediaStore.Files.FileColumns.DATA);
                    String path = cursor.getString(columnIndex);
                    cursor.close();
                    return path;
                }
                cursor.close();
            }
        }
        return uri.getPath();
    }
}
