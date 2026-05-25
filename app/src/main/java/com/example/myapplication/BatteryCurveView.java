package com.example.myapplication;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.util.AttributeSet;
import android.view.View;

import java.util.Arrays;

/**
 * 电池曲线折线图 View
 * X 轴：待机时间（秒，从数据点索引推算）  Y 轴：电压（mV，3000-4350）
 * 数据格式：points[i][0] = 累积时间(s), points[i][1] = 电压(mV)
 */
public class BatteryCurveView extends View {

    private static final int MIN_MV   = 3000;
    private static final int MAX_MV   = 4350;
    private static final int MIN_TIME = 0;      // 最小时间 0 秒
    private static final int MAX_TIME = 36000;  // 默认最大 10 小时

    private Paint gridPaint;
    private Paint axisPaint;
    private Paint curvePaint;
    private Paint dotPaint;
    private Paint labelPaint;
    private Paint hintPaint;

    // 每个元素 int[2] = {soc_pct, mv}
    private int[][] curvePoints;

    // 绘图区内边距
    private float padLeft   = 0f;
    private float padRight  = 0f;
    private float padTop    = 0f;
    private float padBottom = 0f;

    public BatteryCurveView(Context context) {
        super(context);
        init();
    }

    public BatteryCurveView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public BatteryCurveView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    private void init() {
        gridPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        gridPaint.setColor(Color.argb(50, 0, 151, 178));
        gridPaint.setStrokeWidth(1.5f);
        gridPaint.setStyle(Paint.Style.STROKE);

        axisPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        axisPaint.setColor(Color.argb(160, 26, 26, 46));
        axisPaint.setStrokeWidth(2.5f);
        axisPaint.setStyle(Paint.Style.STROKE);

        curvePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        curvePaint.setColor(Color.parseColor("#00C980"));
        curvePaint.setStrokeWidth(3.5f);
        curvePaint.setStyle(Paint.Style.STROKE);
        curvePaint.setStrokeJoin(Paint.Join.ROUND);

        dotPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        dotPaint.setColor(Color.parseColor("#0097B2"));
        dotPaint.setStyle(Paint.Style.FILL);

        labelPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        labelPaint.setColor(Color.parseColor("#5A5A6E"));
        labelPaint.setTextSize(26f);

        hintPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        hintPaint.setColor(Color.parseColor("#9090A0"));
        hintPaint.setTextSize(36f);
        hintPaint.setTextAlign(Paint.Align.CENTER);
    }

    /** 更新曲线点并重绘，points 为 null 或空时显示占位文字 */
    public void setCurvePoints(int[][] points) {
        this.curvePoints = points;
        invalidate();
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldW, int oldH) {
        super.onSizeChanged(w, h, oldW, oldH);
        // 动态计算 padding（根据 View 尺寸，保证标签可放下）
        padLeft   = w * 0.14f;
        padRight  = w * 0.04f;
        padTop    = h * 0.05f;
        padBottom = h * 0.14f;
        labelPaint.setTextSize(Math.max(20f, h * 0.045f));
        hintPaint.setTextSize(Math.max(28f, h * 0.06f));
    }

    // ---- 坐标映射 ----

    private float timeToX(int timeS) {
        float usable = getWidth() - padLeft - padRight;
        // 动态计算最大时间（基于实际数据）
        int maxT = MAX_TIME;
        if (curvePoints != null && curvePoints.length > 0) {
            int maxData = 0;
            for (int[] pt : curvePoints) {
                if (pt[0] > maxData) maxData = pt[0];
            }
            if (maxData > 0) maxT = maxData;
        }
        return padLeft + usable * (timeS - MIN_TIME) / (float) (maxT - MIN_TIME);
    }

    private float mvToY(int mv) {
        float usable = getHeight() - padTop - padBottom;
        return padTop + usable * (1f - (float) (mv - MIN_MV) / (MAX_MV - MIN_MV));
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        float chartW = getWidth()  - padLeft - padRight;
        float chartH = getHeight() - padTop  - padBottom;

        // 动态计算最大时间
        int maxT = MAX_TIME;
        if (curvePoints != null && curvePoints.length > 0) {
            int maxData = 0;
            for (int[] pt : curvePoints) {
                if (pt[0] > maxData) maxData = pt[0];
            }
            if (maxData > 0) maxT = maxData;
        }

        // ---- 垂直网格线 / X 轴标签（时间，自适应间隔）----
        labelPaint.setTextAlign(Paint.Align.CENTER);
        int timeStep = (maxT > 7200) ? 3600 : (maxT > 1800) ? 600 : 300;  // 1h / 10min / 5min
        for (int t = 0; t <= maxT; t += timeStep) {
            float x = timeToX(t);
            canvas.drawLine(x, padTop, x, padTop + chartH, gridPaint);
            String label = (t >= 3600) ? (t / 3600) + "h" : (t / 60) + "m";
            canvas.drawText(label, x, padTop + chartH + labelPaint.getTextSize() + 4, labelPaint);
        }

        // ---- 水平网格线 / Y 轴标签（每 200mV）----
        labelPaint.setTextAlign(Paint.Align.RIGHT);
        for (int mv = MIN_MV; mv <= MAX_MV; mv += 200) {
            float y = mvToY(mv);
            canvas.drawLine(padLeft, y, padLeft + chartW, y, gridPaint);
            canvas.drawText(mv + "", padLeft - 6, y + labelPaint.getTextSize() * 0.38f, labelPaint);
        }

        // ---- 坐标轴 ----
        canvas.drawLine(padLeft, padTop, padLeft, padTop + chartH, axisPaint);
        canvas.drawLine(padLeft, padTop + chartH, padLeft + chartW, padTop + chartH, axisPaint);

        // ---- 曲线数据 ----
        if (curvePoints == null || curvePoints.length == 0) {
            canvas.drawText("暂无曲线数据", padLeft + chartW / 2f, padTop + chartH / 2f, hintPaint);
            return;
        }

        // 按 SOC 升序排列
        int[][] sorted = curvePoints.clone();
        Arrays.sort(sorted, (a, b) -> Integer.compare(a[0], b[0]));

        // 折线
        Path path = new Path();
        boolean first = true;
        for (int[] pt : sorted) {
            float x = timeToX(pt[0]);
            float y = mvToY(pt[1]);
            if (first) {
                path.moveTo(x, y);
                first = false;
            } else {
                path.lineTo(x, y);
            }
        }
        canvas.drawPath(path, curvePaint);

        // 数据点
        float dotR = Math.max(5f, getWidth() * 0.012f);
        for (int[] pt : sorted) {
            canvas.drawCircle(timeToX(pt[0]), mvToY(pt[1]), dotR, dotPaint);
        }
    }
}
