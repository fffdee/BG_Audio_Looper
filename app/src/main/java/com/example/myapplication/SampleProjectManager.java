package com.example.myapplication;

import android.content.ContentUris;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;
import android.provider.MediaStore;
import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Date;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

/**
 * 采样创作项目文件管理器。
 *
 * 项目存储结构:
 *   {externalFilesDir}/BanBox/Projects/<projectName>/
 *       project.json          — 项目元数据 (SampleProject)
 *       sample_001.wav        — 采样文件副本
 *       sample_002.wav
 *       ...
 * 注：使用 getExternalFilesDir() 避免 Android 11+ Scoped Storage 权限问题
 */
public class SampleProjectManager {
    private static final String TAG = "SampleProjectManager";
    private static final String PROJECT_JSON = "project.json";
    
    private static Context sContext;

    public static void init(Context context) {
        sContext = context.getApplicationContext();
    }

    /** 获取项目根目录：{externalFilesDir}/BanBox/Projects/ */
    public static File getProjectsRoot() {
        if (sContext == null) {
            throw new RuntimeException("SampleProjectManager未初始化，请在Application或MainActivity中调用 init(context)");
        }
        File root = new File(sContext.getExternalFilesDir(null), "BanBox/Projects");
        if (!root.exists()) {
            root.mkdirs();
        }
        return root;
    }

    /** 获取某个项目的文件夹 */
    public static File getProjectDir(String projectName) {
        return new File(getProjectsRoot(), projectName);
    }

    // ==================== 保存 ====================

    /**
     * 保存项目：创建文件夹、复制 WAV 采样、写入 project.json。
     *
     * @param context       Context（用于 ContentResolver 读取 WAV URI）
     * @param projectName   项目名称
     * @param tracks        当前 SampleCreatorActivity 的轨道列表
     * @return 保存后的项目文件夹，失败返回 null
     */
    public static File saveProject(Context context, String projectName,
                                   List<SampleCreatorActivity.Track> tracks) {
        File projectDir = getProjectDir(projectName);
        if (!projectDir.exists() && !projectDir.mkdirs()) {
            Log.e(TAG, "无法创建项目文件夹: " + projectDir);
            return null;
        }

        // 收集所有需要用到的 WAV 文件（去重：同名文件只复制一次）
        Set<String> copiedFiles = new HashSet<>();
        for (SampleCreatorActivity.Track track : tracks) {
            for (SampleCreatorActivity.Clip clip : track.clips) {
                if (clip.fileUri != null && !copiedFiles.contains(clip.fileName)) {
                    File dest = new File(projectDir, clip.fileName);
                    if (copyUriToFile(context, clip.fileUri, dest)) {
                        copiedFiles.add(clip.fileName);
                        Log.d(TAG, "已复制采样: " + clip.fileName);
                    } else {
                        Log.w(TAG, "复制采样失败: " + clip.fileName);
                    }
                }
            }
        }

        // 生成 SampleProject 并写入 JSON
        SampleProject project = SampleProject.fromActivityTracks(projectName, tracks);
        File jsonFile = new File(projectDir, PROJECT_JSON);
        try (FileWriter writer = new FileWriter(jsonFile)) {
            writer.write(project.toJson());
            Log.i(TAG, "项目已保存: " + projectDir);
            return projectDir;
        } catch (IOException e) {
            Log.e(TAG, "写入 project.json 失败", e);
            return null;
        }
    }

    // ==================== 加载 ====================

    /**
     * 加载项目：读取 project.json，还原 Track/Clip 列表并设置 fileUri 指向本地文件。
     *
     * @param projectName 项目名称
     * @return Track 列表，失败返回 null
     */
    public static List<SampleCreatorActivity.Track> loadProject(String projectName) {
        File projectDir = getProjectDir(projectName);
        File jsonFile = new File(projectDir, PROJECT_JSON);
        if (!jsonFile.exists()) {
            Log.e(TAG, "project.json 不存在: " + jsonFile);
            return null;
        }

        try (FileReader reader = new FileReader(jsonFile)) {
            StringBuilder sb = new StringBuilder();
            char[] buf = new char[4096];
            int n;
            while ((n = reader.read(buf)) > 0) {
                sb.append(buf, 0, n);
            }
            SampleProject project = SampleProject.fromJson(sb.toString());
            List<SampleCreatorActivity.Track> activityTracks = project.toActivityTracks();

            // 为每个 Clip 设置 fileUri，指向项目文件夹中的 WAV 文件
            for (SampleCreatorActivity.Track track : activityTracks) {
                for (SampleCreatorActivity.Clip clip : track.clips) {
                    File wavFile = new File(projectDir, clip.fileName);
                    if (wavFile.exists()) {
                        clip.fileUri = Uri.fromFile(wavFile);
                    } else {
                        Log.w(TAG, "采样文件缺失: " + wavFile);
                    }
                }
            }

            Log.i(TAG, "项目已加载: " + projectName);
            return activityTracks;
        } catch (Exception e) {
            Log.e(TAG, "加载项目失败", e);
            return null;
        }
    }

    /**
     * 读取项目元数据（不还原轨道），用于列表展示。
     */
    public static SampleProject loadProjectMeta(String projectName) {
        File jsonFile = new File(getProjectDir(projectName), PROJECT_JSON);
        if (!jsonFile.exists()) return null;
        try (FileReader reader = new FileReader(jsonFile)) {
            StringBuilder sb = new StringBuilder();
            char[] buf = new char[4096];
            int n;
            while ((n = reader.read(buf)) > 0) {
                sb.append(buf, 0, n);
            }
            return SampleProject.fromJson(sb.toString());
        } catch (Exception e) {
            Log.e(TAG, "读取项目元数据失败: " + projectName, e);
            return null;
        }
    }

    // ==================== 列表 ====================

    /**
     * 列出所有已保存的项目名称（按修改时间降序）。
     */
    public static List<String> listProjects() {
        List<String> names = new ArrayList<>();
        File root = getProjectsRoot();
        File[] dirs = root.listFiles(File::isDirectory);
        if (dirs == null) return names;

        // 按修改时间降序排列
        Arrays.sort(dirs, (a, b) -> Long.compare(b.lastModified(), a.lastModified()));

        for (File dir : dirs) {
            File json = new File(dir, PROJECT_JSON);
            if (json.exists()) {
                names.add(dir.getName());
            }
        }
        return names;
    }

    // ==================== 删除 ====================

    /**
     * 删除项目（文件夹及所有内容）。
     */
    public static boolean deleteProject(String projectName) {
        File projectDir = getProjectDir(projectName);
        if (!projectDir.exists()) return false;
        return deleteRecursive(projectDir);
    }

    private static boolean deleteRecursive(File file) {
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children != null) {
                for (File child : children) {
                    deleteRecursive(child);
                }
            }
        }
        return file.delete();
    }

    // ==================== 导出 ZIP ====================

    /**
     * 将项目文件夹打包为 ZIP 文件。
     *
     * @param projectName 项目名称
     * @return ZIP 文件路径，失败返回 null
     */
    public static File exportProjectZip(String projectName) {
        File projectDir = getProjectDir(projectName);
        if (!projectDir.exists()) return null;

        File[] files = projectDir.listFiles();
        if (files == null || files.length == 0) return null;

        File zipDir = new File(getProjectsRoot(), ".export");
        if (!zipDir.exists()) zipDir.mkdirs();

        String zipName = projectName + "_" + new SimpleDateFormat(
                "yyyyMMdd_HHmmss", Locale.US).format(new Date()) + ".zip";
        File zipFile = new File(zipDir, zipName);

        boolean ok = FileUtils.zipFiles(files, zipFile.getAbsolutePath());
        if (ok && zipFile.exists()) {
            Log.i(TAG, "项目已导出为 ZIP: " + zipFile);
            return zipFile;
        }
        return null;
    }

    // ==================== 导入 ZIP ====================

    /**
     * 从 ZIP 文件导入项目。
     *
     * @param context  Context
     * @param zipUri   ZIP 文件 URI
     * @return 导入的项目名称，失败返回 null
     */
    public static String importProjectZip(Context context, Uri zipUri) {
        // 将 ZIP 复制到临时文件
        File tempZip = new File(context.getCacheDir(), "import_temp.zip");
        try (InputStream is = context.getContentResolver().openInputStream(zipUri);
             OutputStream os = new FileOutputStream(tempZip)) {
            if (is == null) return null;
            byte[] buf = new byte[8192];
            int n;
            while ((n = is.read(buf)) > 0) {
                os.write(buf, 0, n);
            }
        } catch (IOException e) {
            Log.e(TAG, "复制 ZIP 临时文件失败", e);
            return null;
        }

        // 解压到临时目录，读取 project.json 获取项目名
        File tempDir = new File(context.getCacheDir(), "import_temp_dir");
        if (tempDir.exists()) deleteRecursive(tempDir);
        tempDir.mkdirs();

        if (!FileUtils.unzipFile(tempZip.getAbsolutePath(), tempDir.getAbsolutePath())) {
            Log.e(TAG, "解压 ZIP 失败");
            cleanup(tempZip, tempDir);
            return null;
        }

        // 读取 project.json
        File jsonFile = new File(tempDir, PROJECT_JSON);
        if (!jsonFile.exists()) {
            Log.e(TAG, "ZIP 中不包含 project.json");
            cleanup(tempZip, tempDir);
            return null;
        }

        SampleProject project;
        try (FileReader reader = new FileReader(jsonFile)) {
            StringBuilder sb = new StringBuilder();
            char[] buf = new char[4096];
            int len;
            while ((len = reader.read(buf)) > 0) {
                sb.append(buf, 0, len);
            }
            project = SampleProject.fromJson(sb.toString());
        } catch (Exception e) {
            Log.e(TAG, "解析 project.json 失败", e);
            cleanup(tempZip, tempDir);
            return null;
        }

        // 确定项目名（如重复则加后缀）
        String baseName = project.getProjectName();
        String finalName = baseName;
        int suffix = 1;
        while (getProjectDir(finalName).exists()) {
            finalName = baseName + "_" + suffix++;
        }

        // 移动解压内容到正式项目目录
        File projectDir = getProjectDir(finalName);
        if (!tempDir.renameTo(projectDir)) {
            // renameTo 跨分区可能失败，改为逐文件复制
            projectDir.mkdirs();
            File[] tempFiles = tempDir.listFiles();
            if (tempFiles != null) {
                for (File f : tempFiles) {
                    copyFile(f, new File(projectDir, f.getName()));
                }
            }
        }

        // 如果项目名被重命名，更新 project.json
        if (!finalName.equals(baseName)) {
            project.setProjectName(finalName);
            File newJson = new File(projectDir, PROJECT_JSON);
            try (FileWriter writer = new FileWriter(newJson)) {
                writer.write(project.toJson());
            } catch (IOException e) {
                Log.w(TAG, "更新导入项目名失败", e);
            }
        }

        cleanup(tempZip, tempDir);
        Log.i(TAG, "项目已导入: " + finalName);
        return finalName;
    }

    // ==================== 辅助方法 ====================

    /** 从 content URI 复制文件到目标路径 */
    private static boolean copyUriToFile(Context context, Uri uri, File dest) {
        try (InputStream is = context.getContentResolver().openInputStream(uri);
             OutputStream os = new FileOutputStream(dest)) {
            if (is == null) return false;
            byte[] buf = new byte[8192];
            int n;
            while ((n = is.read(buf)) > 0) {
                os.write(buf, 0, n);
            }
            return true;
        } catch (IOException e) {
            Log.e(TAG, "copyUriToFile 失败: " + uri, e);
            return false;
        }
    }

    /** 普通文件复制 */
    private static boolean copyFile(File src, File dest) {
        try (InputStream is = new FileInputStream(src);
             OutputStream os = new FileOutputStream(dest)) {
            byte[] buf = new byte[8192];
            int n;
            while ((n = is.read(buf)) > 0) {
                os.write(buf, 0, n);
            }
            return true;
        } catch (IOException e) {
            Log.e(TAG, "copyFile 失败: " + src, e);
            return false;
        }
    }

    /** 清理临时文件 */
    private static void cleanup(File... files) {
        for (File f : files) {
            if (f == null) continue;
            if (f.isDirectory()) {
                deleteRecursive(f);
            } else {
                f.delete();
            }
        }
    }

    /**
     * 检查项目名是否合法（不含文件系统非法字符）。
     */
    public static boolean isValidProjectName(String name) {
        if (name == null || name.trim().isEmpty()) return false;
        // 禁止文件系统非法字符
        return !name.matches(".*[/\\\\:*?\"<>|].*");
    }
}
