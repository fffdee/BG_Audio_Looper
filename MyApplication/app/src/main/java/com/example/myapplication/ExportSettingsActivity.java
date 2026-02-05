package com.example.myapplication;

import android.os.Bundle;
import android.widget.CheckBox;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;

/**
 * 导出设置页面 - 允许用户配置导出相关的偏好设置
 */
public class ExportSettingsActivity extends AppCompatActivity {
    
    private ExportConfigManager configManager;
    private SeekBar seekBarQuality;
    private TextView textQualityValue;
    private CheckBox checkAutoCleanup;
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_export_settings);
        
        configManager = new ExportConfigManager(this);
        
        initToolbar();
        initViews();
        loadCurrentSettings();
    }
    
    private void initToolbar() {
        Toolbar toolbar = findViewById(R.id.toolbar_export_settings);
        setSupportActionBar(toolbar);
        if (getSupportActionBar() != null) {
            getSupportActionBar().setDisplayHomeAsUpEnabled(true);
            getSupportActionBar().setTitle("导出设置");
        }
        
        toolbar.setNavigationOnClickListener(v -> finish());
    }
    
    private void initViews() {
        seekBarQuality = findViewById(R.id.seekbar_export_quality);
        textQualityValue = findViewById(R.id.text_quality_value);
        checkAutoCleanup = findViewById(R.id.checkbox_auto_cleanup);
        
        // 设置质量滑动条监听
        seekBarQuality.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (fromUser) {
                    int quality = Math.max(10, progress); // 最低质量10%
                    textQualityValue.setText(quality + "%");
                    configManager.saveExportQuality(quality);
                }
            }
            
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}
            
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                Toast.makeText(ExportSettingsActivity.this, 
                    "导出质量已保存", Toast.LENGTH_SHORT).show();
            }
        });
        
        // 设置自动清理选择框监听
        checkAutoCleanup.setOnCheckedChangeListener((buttonView, isChecked) -> {
            configManager.setAutoCleanupTemp(isChecked);
            Toast.makeText(this, 
                isChecked ? "将自动清理临时文件" : "保留临时文件", 
                Toast.LENGTH_SHORT).show();
        });
    }
    
    private void loadCurrentSettings() {
        // 加载当前质量设置
        int currentQuality = configManager.getExportQuality();
        seekBarQuality.setProgress(currentQuality);
        textQualityValue.setText(currentQuality + "%");
        
        // 加载自动清理设置
        boolean autoCleanup = configManager.isAutoCleanupTemp();
        checkAutoCleanup.setChecked(autoCleanup);
    }
}