package com.example.myapplication;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

/**
 * 自定义音频轨裁剪视图。
 * 以可视化条形图呈现录音全长，两侧拖拽柄可剪辑头/尾。
 * 外部通过 setTrimFractions() 设置初始值，通过 OnTrimChangedListener 接收拖拽回调。
 */
public class AudioTrackTrimView extends View {

    public interface OnTrimChangedListener {
        /** 拖拽过程中实时回调（不发指令）
         *  @param startFraction 头部归一化位置 [0, 1]
         *  @param endFraction   尾部归一化位置 [0, 1] */
        void onTrimChanged(float startFraction, float endFraction);
    }

    public interface OnTrimCommittedListener {
        /** 手指打开屏幕时回调，适合发送 BLE 指令
         *  @param startFraction 头部归一化位置 [0, 1]
         *  @param endFraction   尾部归一化位置 [0, 1] */
        void onTrimCommitted(float startFraction, float endFraction);
    }

    private float startFrac = 0f;
    private float endFrac   = 1f;
    private OnTrimChangedListener   listener;
    private OnTrimCommittedListener committedListener;

    private final Paint bgPaint  = new Paint(Paint.ANTI_ALIAS_FLAG);  // 整体背景
    private final Paint barPaint = new Paint(Paint.ANTI_ALIAS_FLAG);  // 全长条
    private final Paint selPaint = new Paint(Paint.ANTI_ALIAS_FLAG);  // 选中区域高亮
    private final Paint hdlPaint = new Paint(Paint.ANTI_ALIAS_FLAG);  // 拖拽柄
    private final Paint dimPaint = new Paint(Paint.ANTI_ALIAS_FLAG);  // 裁剪外暗化
    private final RectF rectTmp  = new RectF();

    private static final int DRAG_NONE  = 0;
    private static final int DRAG_START = 1;
    private static final int DRAG_END   = 2;
    private int dragging = DRAG_NONE;

    public AudioTrackTrimView(Context ctx) {
        super(ctx); init();
    }
    public AudioTrackTrimView(Context ctx, AttributeSet attrs) {
        super(ctx, attrs); init();
    }
    public AudioTrackTrimView(Context ctx, AttributeSet attrs, int defStyleAttr) {
        super(ctx, attrs, defStyleAttr); init();
    }

    private void init() {
        bgPaint.setColor(0xFF141C27);
        barPaint.setColor(0xFF2D3748);
        selPaint.setColor(0x5500D9FF);
        hdlPaint.setColor(0xFF00D9FF);
        dimPaint.setColor(0xBB000000);
        setClickable(true);
    }

    /** 设置头/尾的归一化位置（0~1），立即刷新视图 */
    public void setTrimFractions(float start, float end) {
        startFrac = Math.max(0f, Math.min(1f, start));
        endFrac   = Math.max(startFrac + 0.01f, Math.min(1f, end));
        invalidate();
    }

    public float getStartFraction() { return startFrac; }
    public float getEndFraction()   { return endFrac;   }

    public void setOnTrimChangedListener(OnTrimChangedListener l) {
        this.listener = l;
    }

    public void setOnTrimCommittedListener(OnTrimCommittedListener l) {
        this.committedListener = l;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        float w = getWidth(), h = getHeight();
        if (w <= 0 || h <= 0) return;
        float d   = getResources().getDisplayMetrics().density;
        float hW  = 5 * d;       // 柄半宽（px）
        float r   = 3 * d;       // 圆角半径
        float barH = h * 0.36f;
        float barT = (h - barH) / 2f;
        float barB = barT + barH;

        // 整体背景
        canvas.drawRect(0, 0, w, h, bgPaint);

        // 全长波形条（底层）
        rectTmp.set(0, barT, w, barB);
        canvas.drawRoundRect(rectTmp, r, r, barPaint);

        // 伪波形刻度（确定性伪随机，不在 onDraw 内 new 对象）
        Paint tp = hdlPaint;
        int savedAlpha = tp.getAlpha();
        tp.setAlpha(35);
        int tickCount = 36;
        for (int i = 0; i < tickCount; i++) {
            // 用纯整数运算模拟不均匀高度
            float amp  = 0.25f + 0.75f * ((i * 73 + 17) % 31) / 31f;
            float th   = barH * amp;
            float tx   = w * i / tickCount;
            float ty   = (h - th) / 2f;
            canvas.drawRect(tx, ty, tx + w / tickCount * 0.6f, ty + th, tp);
        }
        tp.setAlpha(savedAlpha);

        float sx = startFrac * w;
        float ex = endFrac   * w;

        // 裁剪外区域暗化
        if (sx > 0)  canvas.drawRect(0,  0, sx, h, dimPaint);
        if (ex < w)  canvas.drawRect(ex, 0, w,  h, dimPaint);

        // 选中区域高亮
        canvas.drawRect(sx, barT, ex, barB, selPaint);

        // 头部拖拽柄
        rectTmp.set(sx - hW, 0, sx + hW, h);
        canvas.drawRoundRect(rectTmp, r, r, hdlPaint);

        // 尾部拖拽柄
        rectTmp.set(ex - hW, 0, ex + hW, h);
        canvas.drawRoundRect(rectTmp, r, r, hdlPaint);
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        float w = getWidth();
        if (w <= 0) return false;
        float d    = getResources().getDisplayMetrics().density;
        float slop = 22 * d;   // 触摸响应区半径（px）
        float x    = e.getX();

        switch (e.getAction()) {
            case MotionEvent.ACTION_DOWN: {
                float sx    = startFrac * w;
                float ex    = endFrac   * w;
                float distS = Math.abs(x - sx);
                float distE = Math.abs(x - ex);
                if (distS < slop || distE < slop) {
                    dragging = (distS <= distE) ? DRAG_START : DRAG_END;
                    getParent().requestDisallowInterceptTouchEvent(true);
                    return true;
                }
                return false;
            }
            case MotionEvent.ACTION_MOVE: {
                if (dragging == DRAG_NONE) return false;
                float f = Math.max(0f, Math.min(1f, x / w));
                if (dragging == DRAG_START) {
                    startFrac = Math.min(f, endFrac - 0.02f);
                } else {
                    endFrac = Math.max(f, startFrac + 0.02f);
                }
                invalidate();
                if (listener != null) listener.onTrimChanged(startFrac, endFrac);
                return true;
            }
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL: {
                if (dragging != DRAG_NONE) {
                    dragging = DRAG_NONE;
                    getParent().requestDisallowInterceptTouchEvent(false);
                    if (committedListener != null) {
                        committedListener.onTrimCommitted(startFrac, endFrac);
                    }
                }
                return true;
            }
        }
        return false;
    }
}
