package com.example.myapplication;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.DashPathEffect;
import android.graphics.Paint;
import android.graphics.Path;
import android.util.AttributeSet;
import android.view.View;
import java.util.ArrayList;
import java.util.List;

/**
 * EQ 曲线绘制 View
 * 显示多个频段的均衡曲线
 */
public class EqCurveView extends View {
    
    // EQ 频段数据类
    public static class EqBand {
        public boolean enable = true;
        public int type = 0;      // 0=Peaking, 1=LowShelf, 2=HighShelf, 3=LowPass, 4=HighPass, 5=BandPass, 6=Notch
        public float f0 = 1000;   // 中心频率 Hz
        public float Q = 1.0f;    // Q 值
        public float gain = 0;    // 增益 dB
        
        public EqBand() {}
        
        public EqBand(boolean enable, int type, float f0, float Q, float gain) {
            this.enable = enable;
            this.type = type;
            this.f0 = f0;
            this.Q = Q;
            this.gain = gain;
        }
    }
    
    private List<EqBand> bands = new ArrayList<>();
    private int selectedBand = -1;
    
    private Paint gridPaint;
    private Paint curvePaint;
    private Paint bandPointPaint;
    private Paint selectedPointPaint;
    private Paint textPaint;
    private Paint zeroLinePaint;
    private Path curvePath;
    
    // 频率范围 (对数刻度)
    private static final float MIN_FREQ = 20f;
    private static final float MAX_FREQ = 20000f;
    
    // 增益范围
    private static final float MIN_GAIN = -18f;
    private static final float MAX_GAIN = 18f;
    
    // 采样率
    private static final float SAMPLE_RATE = 48000f;
    
    // Band 颜色数组
    private static final int[] BAND_COLORS = {
        0xFFE91E63, // Pink
        0xFFF44336, // Red
        0xFFFF9800, // Orange
        0xFFFFEB3B, // Yellow
        0xFF4CAF50, // Green
        0xFF00BCD4, // Cyan
        0xFF2196F3, // Blue
        0xFF9C27B0, // Purple
        0xFF795548, // Brown
        0xFF607D8B  // Blue Grey
    };
    
    public EqCurveView(Context context) {
        super(context);
        init();
    }
    
    public EqCurveView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }
    
    public EqCurveView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }
    
    private void init() {
        // 网格画笔
        gridPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        gridPaint.setColor(Color.parseColor("#333333"));
        gridPaint.setStrokeWidth(1);
        gridPaint.setStyle(Paint.Style.STROKE);
        
        // 曲线画笔
        curvePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        curvePaint.setColor(Color.parseColor("#4CAF50"));
        curvePaint.setStrokeWidth(3);
        curvePaint.setStyle(Paint.Style.STROKE);
        
        // 频段点画笔
        bandPointPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        bandPointPaint.setStyle(Paint.Style.FILL);
        
        // 选中点画笔
        selectedPointPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        selectedPointPaint.setColor(Color.WHITE);
        selectedPointPaint.setStyle(Paint.Style.STROKE);
        selectedPointPaint.setStrokeWidth(3);
        
        // 文字画笔
        textPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        textPaint.setColor(Color.parseColor("#888888"));
        textPaint.setTextSize(24);
        
        // 零线画笔
        zeroLinePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        zeroLinePaint.setColor(Color.parseColor("#666666"));
        zeroLinePaint.setStrokeWidth(2);
        zeroLinePaint.setStyle(Paint.Style.STROKE);
        zeroLinePaint.setPathEffect(new DashPathEffect(new float[]{10, 5}, 0));
        
        curvePath = new Path();
        
        setBackgroundColor(Color.parseColor("#1a1a1a"));
    }
    
    /**
     * 设置 EQ 频段列表
     */
    public void setBands(List<EqBand> bands) {
        this.bands = bands;
        invalidate();
    }
    
    /**
     * 更新单个频段
     */
    public void updateBand(int index, EqBand band) {
        if (index >= 0 && index < bands.size()) {
            bands.set(index, band);
            invalidate();
        }
    }
    
    /**
     * 添加频段
     */
    public void addBand(EqBand band) {
        bands.add(band);
        invalidate();
    }
    
    /**
     * 移除频段
     */
    public void removeBand(int index) {
        if (index >= 0 && index < bands.size()) {
            bands.remove(index);
            if (selectedBand >= bands.size()) {
                selectedBand = bands.size() - 1;
            }
            invalidate();
        }
    }
    
    /**
     * 设置选中的频段
     */
    public void setSelectedBand(int index) {
        this.selectedBand = index;
        invalidate();
    }
    
    /**
     * 获取频段数量
     */
    public int getBandCount() {
        return bands.size();
    }
    
    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        
        int width = getWidth();
        int height = getHeight();
        int padding = 60;
        int graphWidth = width - padding * 2;
        int graphHeight = height - padding * 2;
        
        // 绘制网格
        drawGrid(canvas, padding, graphWidth, graphHeight);
        
        // 绘制零线
        float zeroY = padding + graphHeight / 2f;
        canvas.drawLine(padding, zeroY, padding + graphWidth, zeroY, zeroLinePaint);
        
        // 计算并绘制总曲线
        drawEqCurve(canvas, padding, graphWidth, graphHeight);
        
        // 绘制各频段的控制点
        drawBandPoints(canvas, padding, graphWidth, graphHeight);
    }
    
    private void drawGrid(Canvas canvas, int padding, int graphWidth, int graphHeight) {
        // 频率网格线 (对数刻度)
        float[] freqs = {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};
        for (float freq : freqs) {
            float x = padding + freqToX(freq) * graphWidth;
            canvas.drawLine(x, padding, x, padding + graphHeight, gridPaint);
            
            // 频率标签
            String label;
            if (freq >= 1000) {
                label = (int)(freq / 1000) + "k";
            } else {
                label = String.valueOf((int)freq);
            }
            canvas.drawText(label, x - 15, padding + graphHeight + 30, textPaint);
        }
        
        // 增益网格线
        float[] gains = {-18, -12, -6, 0, 6, 12, 18};
        for (float gain : gains) {
            float y = padding + gainToY(gain) * graphHeight;
            canvas.drawLine(padding, y, padding + graphWidth, y, gridPaint);
            
            // 增益标签
            String label = (gain >= 0 ? "+" : "") + (int)gain + "dB";
            canvas.drawText(label, 5, y + 8, textPaint);
        }
    }
    
    private void drawEqCurve(Canvas canvas, int padding, int graphWidth, int graphHeight) {
        if (bands.isEmpty()) {
            return;
        }
        
        curvePath.reset();
        boolean firstPoint = true;
        
        // 在多个频率点采样计算总响应
        int numPoints = 200;
        for (int i = 0; i <= numPoints; i++) {
            float t = (float) i / numPoints;
            float freq = (float) (MIN_FREQ * Math.pow(MAX_FREQ / MIN_FREQ, t));
            
            // 计算所有频段的总增益
            float totalGain = 0;
            for (EqBand band : bands) {
                if (band.enable) {
                    totalGain += calculateBandGain(band, freq);
                }
            }
            
            // 限制增益范围
            totalGain = Math.max(MIN_GAIN, Math.min(MAX_GAIN, totalGain));
            
            float x = padding + t * graphWidth;
            float y = padding + gainToY(totalGain) * graphHeight;
            
            if (firstPoint) {
                curvePath.moveTo(x, y);
                firstPoint = false;
            } else {
                curvePath.lineTo(x, y);
            }
        }
        
        canvas.drawPath(curvePath, curvePaint);
    }
    
    private void drawBandPoints(Canvas canvas, int padding, int graphWidth, int graphHeight) {
        for (int i = 0; i < bands.size(); i++) {
            EqBand band = bands.get(i);
            if (!band.enable) continue;
            
            float x = padding + freqToX(band.f0) * graphWidth;
            float y = padding + gainToY(band.gain) * graphHeight;
            
            // 设置颜色
            bandPointPaint.setColor(BAND_COLORS[i % BAND_COLORS.length]);
            
            // 绘制点
            canvas.drawCircle(x, y, 15, bandPointPaint);
            
            // 如果是选中的，绘制外圈
            if (i == selectedBand) {
                canvas.drawCircle(x, y, 20, selectedPointPaint);
            }
            
            // 绘制频段编号
            textPaint.setColor(Color.WHITE);
            textPaint.setTextSize(20);
            canvas.drawText(String.valueOf(i), x - 5, y + 6, textPaint);
            textPaint.setColor(Color.parseColor("#888888"));
            textPaint.setTextSize(24);
        }
    }
    
    /**
     * 计算单个频段在指定频率处的增益响应
     */
    private float calculateBandGain(EqBand band, float freq) {
        // 简化的 EQ 响应计算（使用二阶滤波器近似）
        float f0 = band.f0;
        float Q = Math.max(0.1f, band.Q);
        float gainDb = band.gain;
        
        // 对于 Peaking EQ
        if (band.type == 0) {
            // 计算频率比
            float ratio = freq / f0;
            float logRatio = (float) Math.log10(ratio);
            
            // 使用高斯近似
            float bandwidth = 1.0f / Q;
            float response = (float) Math.exp(-Math.pow(logRatio / (bandwidth * 0.3f), 2));
            
            return gainDb * response;
        }
        // Low Shelf
        else if (band.type == 1) {
            float ratio = freq / f0;
            float response = 1.0f / (1.0f + (float)Math.pow(ratio, 2 * Q));
            return gainDb * response;
        }
        // High Shelf
        else if (band.type == 2) {
            float ratio = f0 / freq;
            float response = 1.0f / (1.0f + (float)Math.pow(ratio, 2 * Q));
            return gainDb * response;
        }
        // 其他类型暂时用 Peaking 近似
        else {
            float ratio = freq / f0;
            float logRatio = (float) Math.log10(ratio);
            float bandwidth = 1.0f / Q;
            float response = (float) Math.exp(-Math.pow(logRatio / (bandwidth * 0.3f), 2));
            return gainDb * response;
        }
    }
    
    /**
     * 频率转换为 X 坐标 (0~1)
     */
    private float freqToX(float freq) {
        freq = Math.max(MIN_FREQ, Math.min(MAX_FREQ, freq));
        return (float) (Math.log10(freq / MIN_FREQ) / Math.log10(MAX_FREQ / MIN_FREQ));
    }
    
    /**
     * 增益转换为 Y 坐标 (0~1)
     */
    private float gainToY(float gain) {
        gain = Math.max(MIN_GAIN, Math.min(MAX_GAIN, gain));
        return 1.0f - (gain - MIN_GAIN) / (MAX_GAIN - MIN_GAIN);
    }
}
