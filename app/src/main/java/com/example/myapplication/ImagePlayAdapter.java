package com.example.myapplication;

import android.graphics.drawable.Drawable;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import com.bumptech.glide.Glide;
import com.bumptech.glide.load.DataSource;
import com.bumptech.glide.load.engine.DiskCacheStrategy;
import com.bumptech.glide.load.engine.GlideException;
import com.bumptech.glide.request.RequestListener;
import com.bumptech.glide.request.RequestOptions;
import com.bumptech.glide.request.target.Target;

import java.io.File;
import java.util.List;

/**
 * 图片播放适配器 - 用于RecyclerView垂直显示多张图片
 */
public class ImagePlayAdapter extends RecyclerView.Adapter<ImagePlayAdapter.ImageViewHolder> {
    private static final String TAG = "ImagePlayAdapter";
    private List<String> imagePaths;
    private int screenWidth;

    public ImagePlayAdapter(List<String> imagePaths, int screenWidth) {
        this.imagePaths = imagePaths;
        this.screenWidth = screenWidth;
    }

    @NonNull
    @Override
    public ImageViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext())
                .inflate(R.layout.item_play_image, parent, false);
        return new ImageViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull ImageViewHolder holder, int position) {
        String imagePath = imagePaths.get(position);
        Log.d(TAG, "加载图片 " + (position + 1) + "/" + imagePaths.size() + ": " + imagePath);

        // 设置ImageView尺寸为屏幕宽度
        ViewGroup.LayoutParams params = holder.imageView.getLayoutParams();
        params.width = screenWidth;
        holder.imageView.setLayoutParams(params);

        RequestOptions options = new RequestOptions()
                .override(screenWidth, Target.SIZE_ORIGINAL) // 宽度固定，高度保持原始比例
                .diskCacheStrategy(DiskCacheStrategy.RESOURCE)
                .fitCenter(); // 填充整个宽度

        Glide.with(holder.imageView.getContext())
                .load(new File(imagePath))
                .apply(options)
                .listener(new RequestListener<Drawable>() {
                    @Override
                    public boolean onLoadFailed(@Nullable GlideException e, Object model, 
                                               Target<Drawable> target, boolean isFirstResource) {
                        Log.e(TAG, "图片加载失败: " + imagePath, e);
                        return false;
                    }

                    @Override
                    public boolean onResourceReady(Drawable resource, Object model, 
                                                  Target<Drawable> target, DataSource dataSource, 
                                                  boolean isFirstResource) {
                        Log.d(TAG, "图片加载成功: " + (position + 1));
                        return false;
                    }
                })
                .into(holder.imageView);
    }

    @Override
    public int getItemCount() {
        return imagePaths.size();
    }

    static class ImageViewHolder extends RecyclerView.ViewHolder {
        ImageView imageView;

        ImageViewHolder(@NonNull View itemView) {
            super(itemView);
            imageView = itemView.findViewById(R.id.iv_play_image);
        }
    }
}
