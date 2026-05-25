package com.example.myapplication;

import android.content.Context;
import android.content.SharedPreferences;
import com.google.gson.Gson;
import com.google.gson.reflect.TypeToken;
import java.lang.reflect.Type;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

/**
 * 分类管理器：负责项目分类的增删改查及持久化存储
 */
public class CategoryManager {

    public static final String DEFAULT_CATEGORY_ID = "default";
    public static final String DEFAULT_CATEGORY_NAME = "默认分类";

    private static final String PREF_NAME = "image_categories";
    private static final String KEY_CATEGORIES = "categories";

    // ---- 分类数据类 ----
    public static class Category {
        private String id;
        private String name;

        public Category(String id, String name) {
            this.id = id;
            this.name = name;
        }

        public String getId() { return id; }
        public String getName() { return name; }
        public void setName(String name) { this.name = name; }

        @Override
        public String toString() { return name; }
    }

    // ---- 读取分类列表 ----
    public static List<Category> getCategories(Context context) {
        SharedPreferences prefs = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
        String json = prefs.getString(KEY_CATEGORIES, null);
        if (json == null) {
            // 首次使用，创建默认分类
            List<Category> defaults = new ArrayList<>();
            defaults.add(new Category(DEFAULT_CATEGORY_ID, DEFAULT_CATEGORY_NAME));
            saveCategories(context, defaults);
            return defaults;
        }
        Type type = new TypeToken<List<Category>>() {}.getType();
        List<Category> list = new Gson().fromJson(json, type);
        if (list == null) list = new ArrayList<>();
        // 确保默认分类始终存在
        boolean hasDefault = false;
        for (Category c : list) {
            if (DEFAULT_CATEGORY_ID.equals(c.getId())) { hasDefault = true; break; }
        }
        if (!hasDefault) {
            list.add(0, new Category(DEFAULT_CATEGORY_ID, DEFAULT_CATEGORY_NAME));
            saveCategories(context, list);
        }
        return list;
    }

    // ---- 保存分类列表 ----
    public static void saveCategories(Context context, List<Category> categories) {
        SharedPreferences prefs = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
        prefs.edit().putString(KEY_CATEGORIES, new Gson().toJson(categories)).apply();
    }

    // ---- 添加分类 ----
    public static Category addCategory(Context context, String name) {
        List<Category> categories = getCategories(context);
        String newId = UUID.randomUUID().toString();
        Category newCat = new Category(newId, name.trim());
        categories.add(newCat);
        saveCategories(context, categories);
        return newCat;
    }

    // ---- 重命名分类 ----
    public static boolean renameCategory(Context context, String id, String newName) {
        if (DEFAULT_CATEGORY_ID.equals(id)) {
            // 允许重命名默认分类
        }
        List<Category> categories = getCategories(context);
        for (Category c : categories) {
            if (c.getId().equals(id)) {
                c.setName(newName.trim());
                saveCategories(context, categories);
                return true;
            }
        }
        return false;
    }

    // ---- 删除分类（同时删除其下的所有项目及文件夹）----
    public static void deleteCategory(Context context, String id) {
        if (DEFAULT_CATEGORY_ID.equals(id)) return;
        
        // 1. 找到该分类下的所有项目
        List<ImageProject> projects = ProjectManager.getProjects(context);
        List<ImageProject> toDelete = new ArrayList<>();
        for (ImageProject p : projects) {
            if (id.equals(p.getCategoryId())) {
                toDelete.add(p);
            }
        }
        
        // 2. 删除项目的文件夹
        for (ImageProject p : toDelete) {
            ProjectManager.deleteProjectFolder(context, p);
        }
        
        // 3. 从项目列表中删除这些项目
        projects.removeAll(toDelete);
        ProjectManager.saveAllProjects(context, projects);
        
        // 4. 删除分类
        List<Category> categories = getCategories(context);
        categories.removeIf(c -> c.getId().equals(id));
        saveCategories(context, categories);
        
        android.util.Log.d("CategoryManager", "分类 \"" + id + "\" 及其下的 " + toDelete.size() + " 个项目已删除");
    }

    // ---- 根据ID查找分类名称 ----
    public static String getCategoryName(Context context, String id) {
        for (Category c : getCategories(context)) {
            if (c.getId().equals(id)) return c.getName();
        }
        return DEFAULT_CATEGORY_NAME;
    }
}
