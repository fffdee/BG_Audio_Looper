package com.example.myapplication;

import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;

/**
 * 导出配置管理类 - 负责保存和管理导出相关的用户偏好设置
 */
public class ExportConfigManager {
    private static final String PREF_NAME = "export_config";
    private static final String KEY_LAST_EXPORT_DIR = "last_export_directory";
    private static final String KEY_EXPORT_QUALITY = "export_quality";
    private static final String KEY_AUTO_CLEANUP = "auto_cleanup_temp";
    private static final String KEY_EXPORT_FORMAT = "export_format";
    
    private final SharedPreferences preferences;
    
    public ExportConfigManager(Context context) {
        preferences = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
    }
    
    /**
     * 保存用户最后选择的导出目录
     */
    public void saveLastExportDirectory(Uri directoryUri) {
        if (directoryUri != null) {
            preferences.edit()
                    .putString(KEY_LAST_EXPORT_DIR, directoryUri.toString())
                    .apply();
        }
    }
    
    /**
     * 获取用户最后选择的导出目录
     */
    public String getLastExportDirectory() {
        return preferences.getString(KEY_LAST_EXPORT_DIR, null);
    }
    
    /**
     * 保存导出质量设置 (1-100)
     */
    public void saveExportQuality(int quality) {
        preferences.edit()
                .putInt(KEY_EXPORT_QUALITY, quality)
                .apply();
    }
    
    /**
     * 获取导出质量设置，默认为90
     */
    public int getExportQuality() {
        return preferences.getInt(KEY_EXPORT_QUALITY, 90);
    }
    
    /**
     * 保存是否自动清理临时文件的设置
     */
    public void setAutoCleanupTemp(boolean autoCleanup) {
        preferences.edit()
                .putBoolean(KEY_AUTO_CLEANUP, autoCleanup)
                .apply();
    }
    
    /**
     * 获取是否自动清理临时文件，默认为true
     */
    public boolean isAutoCleanupTemp() {
        return preferences.getBoolean(KEY_AUTO_CLEANUP, true);
    }
    
    /**
     * 保存导出格式 (zip, folder等)
     */
    public void saveExportFormat(String format) {
        preferences.edit()
                .putString(KEY_EXPORT_FORMAT, format)
                .apply();
    }
    
    /**
     * 获取导出格式，默认为"zip"
     */
    public String getExportFormat() {
        return preferences.getString(KEY_EXPORT_FORMAT, "zip");
    }
    
    /**
     * 清除所有导出配置
     */
    public void clearAllConfigs() {
        preferences.edit().clear().apply();
    }
}