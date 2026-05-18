package com.example.myapplication;

import android.content.Context;
import android.content.SharedPreferences;
import androidx.appcompat.app.AppCompatDelegate;

/**
 * 全局主题管理器 - 管理白天/黑夜模式切换
 */
public class ThemeManager {
    private static final String PREFS_NAME = "theme_prefs";
    private static final String KEY_IS_NIGHT_MODE = "is_night_mode";

    /**
     * 应用已保存的主题设置（应在 Activity.onCreate 的 setContentView 之前调用）
     */
    public static void applyTheme(Context context) {
        boolean isNight = isNightMode(context);
        AppCompatDelegate.setDefaultNightMode(
                isNight ? AppCompatDelegate.MODE_NIGHT_YES : AppCompatDelegate.MODE_NIGHT_NO);
    }

    /**
     * 切换白天/黑夜模式
     * @return 切换后是否为夜间模式
     */
    public static boolean toggleTheme(Context context) {
        boolean currentNight = isNightMode(context);
        boolean newNight = !currentNight;
        SharedPreferences prefs = context.getApplicationContext()
                .getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        prefs.edit().putBoolean(KEY_IS_NIGHT_MODE, newNight).apply();
        AppCompatDelegate.setDefaultNightMode(
                newNight ? AppCompatDelegate.MODE_NIGHT_YES : AppCompatDelegate.MODE_NIGHT_NO);
        return newNight;
    }

    /**
     * 当前是否为夜间模式
     */
    public static boolean isNightMode(Context context) {
        SharedPreferences prefs = context.getApplicationContext()
                .getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        // 默认为夜间模式（与当前硬编码暗色方案保持一致）
        return prefs.getBoolean(KEY_IS_NIGHT_MODE, true);
    }
}
