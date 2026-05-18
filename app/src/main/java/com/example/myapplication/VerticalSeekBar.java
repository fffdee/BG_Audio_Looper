package com.example.myapplication;

import android.content.Context;
import android.graphics.Canvas;
import android.util.AttributeSet;
import android.view.MotionEvent;
import androidx.appcompat.widget.AppCompatSeekBar;

/**
 * 竖向 SeekBar - 简化版本
 */
public class VerticalSeekBar extends AppCompatSeekBar {

    private OnSeekBarChangeListener mListener;

    public VerticalSeekBar(Context context) {
        super(context);
    }

    public VerticalSeekBar(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public VerticalSeekBar(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(h, w, oldh, oldw);
    }

    @Override
    protected synchronized void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(heightMeasureSpec, widthMeasureSpec);
        setMeasuredDimension(getMeasuredHeight(), getMeasuredWidth());
    }

    @Override
    protected void onDraw(Canvas c) {
        c.rotate(-90);
        c.translate(-getHeight(), 0);
        super.onDraw(c);
    }

    @Override
    public void setOnSeekBarChangeListener(OnSeekBarChangeListener l) {
        mListener = l;
        super.setOnSeekBarChangeListener(l);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (!isEnabled()) {
            return false;
        }

        switch (event.getAction()) {
            case MotionEvent.ACTION_DOWN:
                if (mListener != null) {
                    mListener.onStartTrackingTouch(this);
                }
                updateProgress(event);
                break;
                
            case MotionEvent.ACTION_MOVE:
                updateProgress(event);
                break;
                
            case MotionEvent.ACTION_UP:
                updateProgress(event);
                if (mListener != null) {
                    mListener.onStopTrackingTouch(this);
                }
                break;
                
            case MotionEvent.ACTION_CANCEL:
                if (mListener != null) {
                    mListener.onStopTrackingTouch(this);
                }
                break;
        }
        return true;
    }

    private void updateProgress(MotionEvent event) {
        int progress = getMax() - (int) (getMax() * event.getY() / getHeight());
        progress = Math.max(0, Math.min(getMax(), progress));
        setProgress(progress);
        if (mListener != null) {
            mListener.onProgressChanged(this, progress, true);
        }
    }
}
