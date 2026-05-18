package com.example.myapplication;

import android.content.Context;
import android.content.Intent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;
import java.util.List;

public class FunctionPagerAdapter extends RecyclerView.Adapter<FunctionPagerAdapter.FunctionViewHolder> {
    private Context context;
    private List<FeatureModule> modules;

    public FunctionPagerAdapter(Context context, List<FeatureModule> modules) {
        this.context = context;
        this.modules = modules;
    }

    @NonNull
    @Override
    public FunctionViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(context).inflate(R.layout.item_function_page, parent, false);
        return new FunctionViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull FunctionViewHolder holder, int position) {
        int realPosition = position % modules.size();
        FeatureModule module = modules.get(realPosition);

        holder.btnFunction.setBackgroundResource(module.getBackgroundResId());

        holder.btnFunction.setOnClickListener(v -> {
            if (context instanceof BanBoxSettingsActivity) {
                BanBoxSettingsActivity activity = (BanBoxSettingsActivity) context;
                if (activity.checkConnection()) {
                    Intent intent = new Intent(context, module.getActivityClass());
                    context.startActivity(intent);
                }
            }
        });
    }

    @Override
    public int getItemCount() {
        return Integer.MAX_VALUE;
    }

    static class FunctionViewHolder extends RecyclerView.ViewHolder {
        ImageButton btnFunction;

        FunctionViewHolder(@NonNull View itemView) {
            super(itemView);
            btnFunction = itemView.findViewById(R.id.btn_function_page);
        }
    }
}
