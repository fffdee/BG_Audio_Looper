package com.example.myapplication;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.View;
import java.util.ArrayList;
import java.util.List;

public class AudioChainView extends View {
    
    private Paint nodePaint;
    private Paint textPaint;
    private Paint linePaint;
    private Paint detailTextPaint;
    private List<AudioNode> nodes = new ArrayList<>();
    private List<AudioEdge> edges = new ArrayList<>();
    
    // 缩放相关
    private ScaleGestureDetector scaleDetector;
    private float scaleFactor = 1.0f;
    private static final float MIN_SCALE = 0.3f;  // 最小缩放比例
    private static final float MAX_SCALE = 2.5f;  // 最大缩放比例
    private Matrix transformMatrix = new Matrix();
    private float translateX = 0f;
    private float translateY = 0f;
    private float lastTouchX = 0f;
    private float lastTouchY = 0f;
    private boolean isDragging = false;
    
    public AudioChainView(Context context) {
        super(context);
        init();
    }
    
    public AudioChainView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }
    
    private void init() {
        Context context = getContext();
        
        // 使用资源中的颜色
        setBackgroundColor(context.getColor(R.color.bg_card));
        
        nodePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        nodePaint.setStyle(Paint.Style.FILL);
        
        // 根据屏幕密度调整文字大小
        float density = context.getResources().getDisplayMetrics().density;
        // 进一步降低缩放因子以适配小屏设备
        float scaleFactor = Math.min(density * 0.7f, 1.4f);
        
        textPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        textPaint.setColor(context.getColor(R.color.text_primary));
        // 使用更小的基础字体
        textPaint.setTextSize(context.getResources().getDimension(R.dimen.text_size_small) * scaleFactor);
        textPaint.setTextAlign(Paint.Align.CENTER);
        textPaint.setFakeBoldText(true);
        textPaint.setShadowLayer(2f, 0f, 0f, context.getColor(R.color.black));
        
        detailTextPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        detailTextPaint.setColor(context.getColor(R.color.text_secondary));
        // 细节文字更小
        detailTextPaint.setTextSize(context.getResources().getDimension(R.dimen.text_size_tiny) * scaleFactor);
        detailTextPaint.setTextAlign(Paint.Align.CENTER);
        detailTextPaint.setShadowLayer(1.5f, 0f, 0f, context.getColor(R.color.black));
        
        linePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        linePaint.setColor(context.getColor(R.color.primary_accent));
        linePaint.setStrokeWidth(3f * scaleFactor); // 根据密度调整线宽
        linePaint.setStyle(Paint.Style.STROKE);
        // 降低发光效果以提高性能
        linePaint.setShadowLayer(2f, 0f, 0f, context.getColor(R.color.primary_accent));
        
        // 初始化缩放检测器
        scaleDetector = new ScaleGestureDetector(context, new ScaleListener());
        
        // 启用触摸事件
        setClickable(true);
        setFocusable(true);
        setFocusableInTouchMode(true);
    }
    
    public void setAudioChain(List<AudioNode> nodes, List<AudioEdge> edges) {
        this.nodes = nodes;
        this.edges = edges;
        invalidate();
    }
    
    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        
        // 保存当前画布状态
        canvas.save();
        
        // 应用缩放和平移变换
        canvas.translate(translateX, translateY);
        canvas.scale(scaleFactor, scaleFactor);
        
        // 绘制连接线
        for (AudioEdge edge : edges) {
            AudioNode from = findNode(edge.fromId);
            AudioNode to = findNode(edge.toId);
            if (from != null && to != null) {
                // 绘制箭头线
                float startX = from.x + from.width;
                float startY = from.y + from.height / 2;
                float endX = to.x;
                float endY = to.y + to.height / 2;
                
                // 如果是垂直连接，调整位置
                if (Math.abs(from.x - to.x) < 50) {
                    startX = from.x + from.width / 2;
                    startY = from.y + from.height;
                    endX = to.x + to.width / 2;
                    endY = to.y;
                }
                
                canvas.drawLine(startX, startY, endX, endY, linePaint);
                
                // 绘制箭头
                drawArrow(canvas, startX, startY, endX, endY);
            }
        }
        
        // 绘制节点
        for (AudioNode node : nodes) {
            // 设置节点颜色
            nodePaint.setColor(node.color);
            
            // 绘制圆角矩形（主体）
            RectF rect = new RectF(node.x, node.y, node.x + node.width, node.y + node.height);
            canvas.drawRoundRect(rect, 15, 15, nodePaint);
            
            // 绘制边框（优化性能）
            Paint borderPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
            borderPaint.setStyle(Paint.Style.STROKE);
            borderPaint.setStrokeWidth(2f);
            borderPaint.setColor(getContext().getColor(R.color.primary_accent));
            // 减少发光效果以提高兼容性
            borderPaint.setShadowLayer(3f, 0f, 0f, node.color);
            canvas.drawRoundRect(rect, 15, 15, borderPaint);
            
            // 绘制节点名称
            canvas.drawText(node.name, 
                          node.x + node.width / 2, 
                          node.y + node.height / 2 - 30, 
                          textPaint);
            
            // 绘制节点详情
            String details = "";
            if (node.channels > 0) {
                details += node.channels + "ch";
            }
            if (node.bitDepth > 0) {
                if (!details.isEmpty()) details += " | ";
                details += node.bitDepth + "bit";
            }
            if (node.sampleRate > 0) {
                if (!details.isEmpty()) details += " | ";
                details += (node.sampleRate / 1000) + "kHz";
            }
            
            if (!details.isEmpty()) {
                canvas.drawText(details, 
                              node.x + node.width / 2, 
                              node.y + node.height / 2 + 40, 
                              detailTextPaint);
            }
        }
        
        // 恢复画布状态
        canvas.restore();
    }
    
    private void drawArrow(Canvas canvas, float startX, float startY, float endX, float endY) {
        // 计算箭头方向
        double angle = Math.atan2(endY - startY, endX - startX);
        float arrowLength = 20;
        float arrowAngle = (float) Math.toRadians(30);
        
        // 箭头两个边
        float x1 = (float) (endX - arrowLength * Math.cos(angle - arrowAngle));
        float y1 = (float) (endY - arrowLength * Math.sin(angle - arrowAngle));
        float x2 = (float) (endX - arrowLength * Math.cos(angle + arrowAngle));
        float y2 = (float) (endY - arrowLength * Math.sin(angle + arrowAngle));
        
        canvas.drawLine(endX, endY, x1, y1, linePaint);
        canvas.drawLine(endX, endY, x2, y2, linePaint);
    }
    
    private AudioNode findNode(int id) {
        for (AudioNode node : nodes) {
            if (node.id == id) return node;
        }
        return null;
    }
    
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        // 先处理缩放手势
        boolean scaleHandled = scaleDetector.onTouchEvent(event);
        
        // 如果正在缩放，不处理拖动
        if (scaleDetector.isInProgress()) {
            isDragging = false;
            return true;
        }
        
        // 处理单指拖动
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                lastTouchX = event.getX();
                lastTouchY = event.getY();
                isDragging = false;
                return true;
                
            case MotionEvent.ACTION_MOVE:
                if (event.getPointerCount() == 1 && !scaleDetector.isInProgress()) {
                    float dx = event.getX() - lastTouchX;
                    float dy = event.getY() - lastTouchY;
                    
                    // 移动超过阈值才认为是拖动
                    if (Math.abs(dx) > 10 || Math.abs(dy) > 10) {
                        isDragging = true;
                    }
                    
                    if (isDragging) {
                        translateX += dx;
                        translateY += dy;
                        invalidate();
                    }
                    
                    lastTouchX = event.getX();
                    lastTouchY = event.getY();
                    return true;
                }
                break;
                
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                isDragging = false;
                return true;
                
            case MotionEvent.ACTION_POINTER_DOWN:
            case MotionEvent.ACTION_POINTER_UP:
                // 多指触摸时重置拖动状态
                isDragging = false;
                return true;
        }
        
        return true;
    }
    
    /**
     * 缩放监听器
     */
    private class ScaleListener extends ScaleGestureDetector.SimpleOnScaleGestureListener {
        @Override
        public boolean onScale(ScaleGestureDetector detector) {
            scaleFactor *= detector.getScaleFactor();
            // 限制缩放范围
            scaleFactor = Math.max(MIN_SCALE, Math.min(scaleFactor, MAX_SCALE));
            invalidate();
            return true;
        }
    }
    
    /**
     * 重置视图到默认大小和位置
     */
    public void resetView() {
        scaleFactor = 1.0f;
        translateX = 0f;
        translateY = 0f;
        invalidate();
    }
    
    /**
     * 获取当前缩放比例
     */
    public float getScaleFactor() {
        return scaleFactor;
    }
    
    public static class AudioNode {
        public int id;
        public String name;
        public int type; // 0=Source, 1=Effect, 2=Mixer, 3=Sink
        public int color;
        public float x, y;
        public float width, height;
        public int channels;
        public int bitDepth;
        public int sampleRate;
        
        public AudioNode(int id, String name, int type, float x, float y) {
            this.id = id;
            this.name = name;
            this.type = type;
            this.x = x;
            this.y = y;
            this.width = 600;
            this.height = 240;
            
            // 根据类型设置颜色（更鲜艳的颜色）
            switch (type) {
                case 0: // Source - 青绿色
                    this.color = 0xFF00FFA3;
                    break;
                case 1: // Effect - 紫蓝色
                    this.color = 0xFF5E72E4;
                    break;
                case 2: // Mixer - 橙红色
                    this.color = 0xFFFF6B35;
                    break;
                case 3: // Sink - 粉红色
                    this.color = 0xFFFF006E;
                    break;
                default:
                    this.color = 0xFF9E9E9E;
            }
        }
        
        public AudioNode setChannels(int channels) {
            this.channels = channels;
            return this;
        }
        
        public AudioNode setBitDepth(int bitDepth) {
            this.bitDepth = bitDepth;
            return this;
        }
        
        public AudioNode setSampleRate(int sampleRate) {
            this.sampleRate = sampleRate;
            return this;
        }
    }
    
    public static class AudioEdge {
        public int fromId;
        public int toId;
        
        public AudioEdge(int fromId, int toId) {
            this.fromId = fromId;
            this.toId = toId;
        }
    }
}
