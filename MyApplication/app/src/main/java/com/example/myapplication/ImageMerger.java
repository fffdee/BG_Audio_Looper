package com.example.myapplication;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.os.Build;
import android.os.Environment;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.List;

public class ImageMerger {
    private static final String TAG = "ImageMerger";
    private static final int MAX_IMAGE_WIDTH = 2048;  // 最大宽度
    private static final int MAX_TOTAL_HEIGHT = 16384; // 最大总高度（避免超过硬件限制）
    
    // 工具方法：创建Bitmap数组
    private static Bitmap[] newBitmapArray(int size) {
        return new Bitmap[size]; // 直接返回Bitmap数组
    }
    // 修复：使用应用私有目录存储（兼容Android 10+），并添加完整异常处理
    // 优化：添加图片采样和压缩功能，支持更多图片拼接，动态调整采样率
    public static String mergeImages(Context context, List<String> imagePaths, String projectName) {
        if (imagePaths.isEmpty()) {
            return null;
        }
        for (String path : imagePaths) {
            if (path == null || !new File(path).exists()) {
                Log.e(TAG, "图片文件不存在: " + path);
                return null;
            }
        }
        
        // 动态计算每张图片的最大高度
        int maxHeightPerImage = MAX_TOTAL_HEIGHT / imagePaths.size();
        Log.d(TAG, "图片总数: " + imagePaths.size() + ", 每张最大高度: " + maxHeightPerImage);
        
        int totalHeight = 0;
        int maxWidth = 0;
        Bitmap[] bitmaps = newBitmapArray(imagePaths.size());
        try {
            for (int i = 0; i < imagePaths.size(); i++) {
                // 优化：根据图片数量动态调整采样率
                Bitmap bitmap = decodeSampledBitmap(imagePaths.get(i), MAX_IMAGE_WIDTH, maxHeightPerImage);
                if (bitmap == null) {
                    Log.e(TAG, "图片解码失败: " + imagePaths.get(i));
                    recycleBitmaps(bitmaps);
                    return null;
                }
                bitmaps[i] = bitmap;
                totalHeight += bitmap.getHeight();
                if (bitmap.getWidth() > maxWidth) {
                    maxWidth = bitmap.getWidth();
                }
                Log.d(TAG, "已加载图片 " + (i+1) + "/" + imagePaths.size() + 
                      ", 尺寸: " + bitmap.getWidth() + "x" + bitmap.getHeight() +
                      ", 当前总高度: " + totalHeight);
            }
            
            Log.d(TAG, "开始合并图片，总尺寸: " + maxWidth + "x" + totalHeight);
            Bitmap mergedBitmap = Bitmap.createBitmap(maxWidth, totalHeight, Bitmap.Config.RGB_565);
            Canvas canvas = new Canvas(mergedBitmap);
            int currentHeight = 0;
            for (int i = 0; i < bitmaps.length; i++) {
                Bitmap bm = bitmaps[i];
                if (bm == null) continue;
                canvas.drawBitmap(bm, 0, currentHeight, null);
                Log.d(TAG, "绘制图片 " + (i+1) + " 在位置: " + currentHeight);
                currentHeight += bm.getHeight();
                bm.recycle();
            }
            Log.d(TAG, "图片合并完成，最终高度: " + currentHeight);
            File saveDir;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                saveDir = new File(context.getExternalFilesDir(Environment.DIRECTORY_PICTURES), "ImagePlayer/" + projectName);
            } else {
                saveDir = new File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_PICTURES), "ImagePlayer/" + projectName);
            }
            if (!saveDir.exists() && !saveDir.mkdirs()) {
                Log.e(TAG, "无法创建保存目录: " + saveDir.getAbsolutePath());
                mergedBitmap.recycle();
                return null;
            }
            File outputFile = new File(saveDir, "merged_image.jpg");
            Log.d(TAG, "开始保存图片到: " + outputFile.getAbsolutePath());
            try (FileOutputStream fos = new FileOutputStream(outputFile)) {
                // 使用JPEG格式减少文件大小，质量85%足够
                if (!mergedBitmap.compress(Bitmap.CompressFormat.JPEG, 85, fos)) {
                    Log.e(TAG, "图片压缩失败");
                    mergedBitmap.recycle();
                    return null;
                }
                fos.flush();
                Log.d(TAG, "图片保存成功: " + outputFile.getAbsolutePath());
                return outputFile.getAbsolutePath();
            } finally {
                mergedBitmap.recycle();
            }
        } catch (OutOfMemoryError e) {
            Log.e(TAG, "内存不足，无法合并图片", e);
            e.printStackTrace();
            recycleBitmaps(bitmaps);
            return null;
        } catch (IOException e) {
            Log.e(TAG, "IO错误: " + e.getMessage(), e);
            e.printStackTrace();
            recycleBitmaps(bitmaps);
            return null;
        } catch (Exception e) {
            Log.e(TAG, "未知错误: " + e.getMessage(), e);
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

    /**
     * 优化：采样解码图片以减少内存占用
     * @param imagePath 图片路径
     * @param reqWidth 目标宽度
     * @param reqHeight 目标高度
     * @return 采样后的Bitmap
     */
    private static Bitmap decodeSampledBitmap(String imagePath, int reqWidth, int reqHeight) {
        // 第一次解码：获取图片尺寸
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inJustDecodeBounds = true;
        BitmapFactory.decodeFile(imagePath, options);
        
        int origWidth = options.outWidth;
        int origHeight = options.outHeight;

        // 计算采样率
        options.inSampleSize = calculateInSampleSize(options, reqWidth, reqHeight);
        
        Log.d(TAG, "解码图片 - 原始尺寸: " + origWidth + "x" + origHeight + 
              ", 采样率: " + options.inSampleSize + 
              ", 目标尺寸: " + reqWidth + "x" + reqHeight);

        // 第二次解码：加载实际图片
        options.inJustDecodeBounds = false;
        options.inPreferredConfig = Bitmap.Config.RGB_565; // 使用RGB_565减少内存占用
        Bitmap result = BitmapFactory.decodeFile(imagePath, options);
        
        if (result != null) {
            Log.d(TAG, "图片加载成功，实际尺寸: " + result.getWidth() + "x" + result.getHeight());
        } else {
            Log.e(TAG, "图片加载失败: " + imagePath);
        }
        
        return result;
    }

    /**
     * 计算图片采样率
     * @param options 图片选项
     * @param reqWidth 目标宽度
     * @param reqHeight 目标高度
     * @return 采样率
     */
    private static int calculateInSampleSize(BitmapFactory.Options options, int reqWidth, int reqHeight) {
        final int height = options.outHeight;
        final int width = options.outWidth;
        int inSampleSize = 1;

        if (height > reqHeight || width > reqWidth) {
            final int halfHeight = height / 2;
            final int halfWidth = width / 2;

            // 计算最大的inSampleSize值，同时保持宽高大于目标尺寸
            while ((halfHeight / inSampleSize) >= reqHeight && (halfWidth / inSampleSize) >= reqWidth) {
                inSampleSize *= 2;
            }
        }
        return inSampleSize;
    }
}