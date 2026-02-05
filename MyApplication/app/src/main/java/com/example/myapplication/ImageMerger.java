package com.example.myapplication;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.os.Build;
import android.os.Environment;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.List;

public class ImageMerger {
    // 工具方法：创建Bitmap数组
    private static Bitmap[] newBitmapArray(int size) {
        return new Bitmap[size]; // 直接返回Bitmap数组
    }
    // 修复：使用应用私有目录存储（兼容Android 10+），并添加完整异常处理
    public static String mergeImages(Context context, List<String> imagePaths, String projectName) {
        if (imagePaths.isEmpty()) {
            return null;
        }
        for (String path : imagePaths) {
            if (path == null || !new File(path).exists()) {
                return null;
            }
        }
        int totalHeight = 0;
        int maxWidth = 0;
        Bitmap[] bitmaps = newBitmapArray(imagePaths.size());
        try {
            for (int i = 0; i < imagePaths.size(); i++) {
                BitmapFactory.Options options = newBitmapFactoryOptions();
                options.inPreferredConfig = Bitmap.Config.ARGB_8888; // 保证高质量
                Bitmap bitmap = BitmapFactory.decodeFile(imagePaths.get(i), options);
                if (bitmap == null) {
                    recycleBitmaps(bitmaps);
                    return null;
                }
                bitmaps[i] = bitmap;
                totalHeight += bitmap.getHeight();
                if (bitmap.getWidth() > maxWidth) {
                    maxWidth = bitmap.getWidth();
                }
            }
            Bitmap mergedBitmap = Bitmap.createBitmap(maxWidth, totalHeight, Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(mergedBitmap);
            int currentHeight = 0;
            for (Bitmap bm : bitmaps) {
                if (bm == null) continue;
                canvas.drawBitmap(bm, 0, currentHeight, null);
                currentHeight += bm.getHeight();
                bm.recycle();
            }
            File saveDir;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                saveDir = new File(context.getExternalFilesDir(Environment.DIRECTORY_PICTURES), "ImagePlayer/" + projectName);
            } else {
                saveDir = new File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_PICTURES), "ImagePlayer/" + projectName);
            }
            if (!saveDir.exists() && !saveDir.mkdirs()) {
                mergedBitmap.recycle();
                return null;
            }
            File outputFile = new File(saveDir, "merged_image.png");
            try (FileOutputStream fos = new FileOutputStream(outputFile)) {
                // PNG无损保存
                if (!mergedBitmap.compress(Bitmap.CompressFormat.PNG, 100, fos)) {
                    mergedBitmap.recycle();
                    return null;
                }
                fos.flush();
                return outputFile.getAbsolutePath();
            } finally {
                mergedBitmap.recycle();
            }
        } catch (OutOfMemoryError | IOException e) {
            e.printStackTrace();
            recycleBitmaps(bitmaps);
            return null;
        }
    }


    // 工具方法：释放Bitmap数组资源
    private static void recycleBitmaps(Bitmap[] bitmaps) {
        if (bitmaps == null) return;
        for (Bitmap bm : bitmaps) {
            if (bm != null && !bm.isRecycled()) {
                bm.recycle();
            }
        }
    }

    // 工具方法：创建BitmapFactory.Options
    private static BitmapFactory.Options newBitmapFactoryOptions() {
        return new BitmapFactory.Options();
    }
}