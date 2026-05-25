package com.example.myapplication;

import android.app.Dialog;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.ImageButton;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import java.util.List;

public class SelectedImageAdapter extends RecyclerView.Adapter<SelectedImageAdapter.ViewHolder> {
    private List<String> imagePaths;
    private OnImageRemovedListener removeListener;

    public interface OnImageRemovedListener {
        void onImageRemoved(int position);
    }

    public SelectedImageAdapter(List<String> imagePaths, OnImageRemovedListener listener) {
        this.imagePaths = imagePaths;
        this.removeListener = listener;
    }

    @NonNull
    @Override
    public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext()).inflate(R.layout.item_selected_image, parent, false);
        return new ViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
        String imagePath = imagePaths.get(position);
        
        // 加载缩略图
        Bitmap thumbnail = decodeSampledBitmap(imagePath, 200, 200);
        if (thumbnail != null) {
            holder.ivThumbnail.setImageBitmap(thumbnail);
        }

        // 设置图片点击事件 - 放大预览
        holder.ivThumbnail.setOnClickListener(v -> {
            showImagePreviewDialog(v.getContext(), imagePath);
        });

        // 设置删除按钮
        holder.btnRemove.setOnClickListener(v -> {
            if (removeListener != null) {
                removeListener.onImageRemoved(holder.getAdapterPosition());
            }
        });
    }

    @Override
    public int getItemCount() {
        return imagePaths.size();
    }

    /**
     * 加载缩略图
     */
    private Bitmap decodeSampledBitmap(String imagePath, int reqWidth, int reqHeight) {
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inJustDecodeBounds = true;
        BitmapFactory.decodeFile(imagePath, options);

        options.inSampleSize = calculateInSampleSize(options, reqWidth, reqHeight);
        options.inJustDecodeBounds = false;
        options.inPreferredConfig = Bitmap.Config.RGB_565;
        
        return BitmapFactory.decodeFile(imagePath, options);
    }

    /**
     * 计算采样率
     */
    private int calculateInSampleSize(BitmapFactory.Options options, int reqWidth, int reqHeight) {
        final int height = options.outHeight;
        final int width = options.outWidth;
        int inSampleSize = 1;

        if (height > reqHeight || width > reqWidth) {
            final int halfHeight = height / 2;
            final int halfWidth = width / 2;

            while ((halfHeight / inSampleSize) >= reqHeight && (halfWidth / inSampleSize) >= reqWidth) {
                inSampleSize *= 2;
            }
        }
        return inSampleSize;
    }

    /**
     * 显示图片预览对话框
     */
    private void showImagePreviewDialog(Context context, String imagePath) {
        Dialog dialog = new Dialog(context, android.R.style.Theme_Black_NoTitleBar_Fullscreen);
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        dialog.setContentView(R.layout.dialog_image_preview);

        ImageView ivPreview = dialog.findViewById(R.id.iv_preview);
        ImageView ivClose = dialog.findViewById(R.id.iv_close);

        // 加载高质量图片
        Bitmap bitmap = BitmapFactory.decodeFile(imagePath);
        if (bitmap != null) {
            ivPreview.setImageBitmap(bitmap);
        }

        // 点击关闭
        ivClose.setOnClickListener(v -> dialog.dismiss());
        ivPreview.setOnClickListener(v -> dialog.dismiss());

        dialog.show();
    }

    static class ViewHolder extends RecyclerView.ViewHolder {
        ImageView ivThumbnail;
        ImageButton btnRemove;

        ViewHolder(@NonNull View itemView) {
            super(itemView);
            ivThumbnail = itemView.findViewById(R.id.iv_thumbnail);
            btnRemove = itemView.findViewById(R.id.btn_remove);
        }
    }
}
