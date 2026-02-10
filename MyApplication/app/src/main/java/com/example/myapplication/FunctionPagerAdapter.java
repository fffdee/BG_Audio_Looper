package com.example.myapplication;

import android.content.Context;
import android.content.Intent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;
import java.util.List;

/**
 * 功能ViewPager适配器
 */
public class FunctionPagerAdapter extends RecyclerView.Adapter<FunctionPagerAdapter.FunctionViewHolder> {
    // 五个页面的背景图片资源
    private static final int[] BG_RES_IDS = {
        R.drawable.reverb,
        R.drawable.eq,        // 第1页
         R.drawable.vol ,   // 第5页
          R.drawable.metro,  
        R.drawable.graph     // 第2页
         
          // 第4页
       
    };

    private Context context;
    private List<BanBoxSettingsActivity.FunctionItem> functions;

    public FunctionPagerAdapter(Context context, List<BanBoxSettingsActivity.FunctionItem> functions) {
        this.context = context;
        this.functions = functions;
    }

    @NonNull
    @Override
    public FunctionViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(context).inflate(R.layout.item_function_page, parent, false);
        return new FunctionViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull FunctionViewHolder holder, int position) {
        BanBoxSettingsActivity.FunctionItem function = functions.get(position);
        holder.btnFunction.setText(function.title);

        // 设置不同背景
        if (position < BG_RES_IDS.length) {
            holder.btnFunction.setBackgroundResource(BG_RES_IDS[position]);
        } else {
            holder.btnFunction.setBackgroundResource(android.R.color.transparent);
        }

        holder.btnFunction.setOnClickListener(v -> {
            // 检查连接状态
            if (context instanceof BanBoxSettingsActivity) {
                BanBoxSettingsActivity activity = (BanBoxSettingsActivity) context;
                if (activity.checkConnection()) {
                    Intent intent = new Intent(context, function.activityClass);
                    context.startActivity(intent);
                }
            }
        });
    }

    @Override
    public int getItemCount() {
        return functions.size();
    }

    static class FunctionViewHolder extends RecyclerView.ViewHolder {
        Button btnFunction;

        FunctionViewHolder(@NonNull View itemView) {
            super(itemView);
            btnFunction = itemView.findViewById(R.id.btn_function_page);
        }
    }
}