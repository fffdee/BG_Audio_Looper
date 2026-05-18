package com.example.myapplication;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.View;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * DAW 时间轴视图
 * - 横轴：时间刻度（秒）
 * - 纵轴：多条轨道，每轨道显示彩色 clip 块
 * - 支持水平滚动、时间缩放、播放头指针
 */
public class TimelineView extends View {

    // ========== 样式常量 ==========
    private static final float RULER_HEIGHT = 40f;      // 标尺高度 dp
    private static final float TRACK_HEIGHT = 80f;      // 轨道行高 dp
    private static final float CLIP_MARGIN = 4f;        // clip 块上下边距 dp
    private static final float MIN_CLIP_WIDTH = 30f;    // clip 最小显示宽度 dp

    // ========== 时间缩放 ==========
    /** 像素/秒，默认 100px = 1s */
    private float pixelsPerSecond = 100f;
    private static final float MIN_ZOOM = 20f;   // 最小缩放 20px/s
    private static final float MAX_ZOOM = 500f;  // 最大缩放 500px/s

    // ========== 数据 ==========
    private List<SampleCreatorActivity.Track> tracks = new ArrayList<>();
    /** 播放头位置，毫秒 */
    private long playheadMs = 0;
    /** 是否显示播放头 */
    private boolean showPlayhead = false;

    // ========== 滚动 & 缩放 ==========
    private float scrollX = 0;              // 水平滚动偏移（像素）
    private GestureDetector gestureDetector;
    private ScaleGestureDetector scaleDetector;

    // ========== 拖拽状态 ==========
    private SampleCreatorActivity.Clip draggedClip = null;   // 正在被拖动的 clip
    private SampleCreatorActivity.Clip pendingClip = null;    // 手指按下时记录
    private int draggedTrackIdx = -1;
    private float dragStartX = 0;
    private long dragOriginalStartMs = 0;
    private boolean isDragActive = false;  // 已开始拖动
    /** 当前选中的 clip（点击选中，再拖拽） */
    private SampleCreatorActivity.Clip selectedClip = null;
    private int selectedTrackIdx = -1;
    /** 拖拽判定随识距离：超过此值才进入拖动，防止点击扮动 */
    private static final float DRAG_SLOP_DP = 6f;

    /** 拖拽起始时，触点相对于 clip 起始的偏移量（毫秒，世界坐标） */
    private long dragOffsetMs = 0;
    /** 触摸起始 Y 坐标（用于跨轨道拖拽） */
    private float dragStartScreenY = 0;

    // ========== 回调 ==========
    public interface OnClipEditListener {
        void onClipMoved();
        void onClipLongPress(SampleCreatorActivity.Clip clip, int trackIdx);
    }
    private OnClipEditListener editListener;

    /** Trim 手柄拖动结束回调 */
    public interface OnTrimListener {
        void onTrimChanged(SampleCreatorActivity.Clip clip);
    }
    private OnTrimListener trimListener;

    public void setOnTrimListener(OnTrimListener l) { trimListener = l; }

    // ========== Paint 对象 ==========
    private Paint rulerPaint;
    private Paint rulerTextPaint;
    private Paint trackBgPaint;
    private Paint clipPaint;
    private Paint clipTextPaint;
    private Paint playheadPaint;
    /** 选中框画笔 */
    private Paint selectionPaint;
    /** 小节线画笔 */
    private Paint measureLinePaint;
    /** trim 手柄画笔 */
    private Paint trimHandlePaint;

    private float density;

    // ========== 小节 / BPM ==========
    private int bpm = 120;
    private int beatsPerBar = 4;

    public void setTempo(int bpm, int beatsPerBar) {
        this.bpm = Math.max(1, bpm);
        this.beatsPerBar = Math.max(1, beatsPerBar);
        invalidate();
    }

    // ========== Trim 手柄 状态 ==========
    private static final float TRIM_HANDLE_WIDTH_DP = 10f;
    private static final int TRIM_NONE  = 0;
    private static final int TRIM_START = 1;
    private static final int TRIM_END   = 2;

    private int     trimMode      = TRIM_NONE;
    private SampleCreatorActivity.Clip trimClip = null;
    private int     trimTrackIdx  = -1;
    private float   trimStartTouchX = 0;

    public TimelineView(Context context) {
        super(context);
        init(context);
    }

    public TimelineView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(context);
    }

    private void init(Context context) {
        density = getResources().getDisplayMetrics().density;

        // 标尺背景
        rulerPaint = new Paint();
        rulerPaint.setColor(0xFF2A2A3A);
        rulerPaint.setStyle(Paint.Style.FILL);

        // 标尺文字和刻度
        rulerTextPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        rulerTextPaint.setColor(0xFFCCCCCC);
        rulerTextPaint.setTextSize(12 * density);

        // 轨道背景（交替颜色）
        trackBgPaint = new Paint();
        trackBgPaint.setStyle(Paint.Style.FILL);

        // Clip 块
        clipPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        clipPaint.setStyle(Paint.Style.FILL);

        // Clip 文字
        clipTextPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        clipTextPaint.setColor(0xFFFFFFFF);
        clipTextPaint.setTextSize(11 * density);

        // 播放头
        playheadPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        playheadPaint.setColor(0xFFFF5555);
        playheadPaint.setStrokeWidth(2 * density);

        // 选中框
        selectionPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        selectionPaint.setColor(0xFFFFFFFF);
        selectionPaint.setStyle(Paint.Style.STROKE);
        selectionPaint.setStrokeWidth(2.5f * density);

        // 小节线
        measureLinePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        measureLinePaint.setColor(0x40FFFFFF);
        measureLinePaint.setStrokeWidth(1f * density);

        // trim 手柄
        trimHandlePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        trimHandlePaint.setColor(0xFFFFD700);  // 金色
        trimHandlePaint.setStyle(Paint.Style.FILL);

        // 手势检测
        gestureDetector = new GestureDetector(context, new GestureDetector.SimpleOnGestureListener() {
            @Override
            public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
                if (isDragActive) return false; // 拖拽 clip 时不滚动视图
                scrollX += distanceX;
                scrollX = Math.max(0, scrollX);
                invalidate();
                return true;
            }

            @Override
            public boolean onSingleTapUp(MotionEvent e) {
                handleTap(e.getX(), e.getY());
                return true;
            }

            @Override
            public void onLongPress(MotionEvent e) {
                if (isDragActive) return;
                SampleCreatorActivity.Clip clip = findClipAt(e.getX(), e.getY());
                if (clip != null) {
                    // 长按激活拖拽模式
                    isDragActive    = true;
                    draggedClip     = clip;
                    selectedClip    = clip;
                    draggedTrackIdx = screenYToTrackIndex(e.getY());
                    if (draggedTrackIdx < 0) draggedTrackIdx = 0;
                    float worldX = e.getX() + scrollX;
                    dragOffsetMs = (long)(worldX / pixelsPerSecond * 1000) - clip.startTimeMs;
                    pendingClip = null;
                    performHapticFeedback(android.view.HapticFeedbackConstants.LONG_PRESS);
                    if (getParent() != null) {
                        getParent().requestDisallowInterceptTouchEvent(true);
                    }
                    invalidate();
                }
            }
        });

        // 缩放检测
        scaleDetector = new ScaleGestureDetector(context, new ScaleGestureDetector.SimpleOnScaleGestureListener() {
            @Override
            public boolean onScale(ScaleGestureDetector detector) {
                float scaleFactor = detector.getScaleFactor();
                pixelsPerSecond *= scaleFactor;
                pixelsPerSecond = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, pixelsPerSecond));
                invalidate();
                return true;
            }
        });
    }

    // ========== 数据设置 ==========
    public void setTracks(List<SampleCreatorActivity.Track> tracks) {
        this.tracks = tracks;
        invalidate();
    }

    public void setPlayheadMs(long ms) {
        this.playheadMs = ms;
        this.showPlayhead = true;
        invalidate();
    }

    public void hidePlayhead() {
        this.showPlayhead = false;
        invalidate();
    }

    public float getPixelsPerSecond() {
        return pixelsPerSecond;
    }

    public void setPixelsPerSecond(float pps) {
        this.pixelsPerSecond = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, pps));
        invalidate();
    }

    public void setOnClipEditListener(OnClipEditListener listener) {
        this.editListener = listener;
    }

    // ========== 尺寸计算 ==========
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int width = MeasureSpec.getSize(widthMeasureSpec);
        // 计算总时长（所有 clip 最远端）
        long maxMs = 10000; // 默认至少 10 秒
        for (SampleCreatorActivity.Track track : tracks) {
            for (SampleCreatorActivity.Clip clip : track.clips) {
                maxMs = Math.max(maxMs, clip.endTimeMs());
            }
        }
        int contentWidth = (int)(maxMs / 1000f * pixelsPerSecond) + width; // 额外留空间

        int trackCount = Math.max(tracks.size(), 4); // 至少显示 4 条
        int height = (int)((RULER_HEIGHT + trackCount * TRACK_HEIGHT) * density);

        setMeasuredDimension(contentWidth, height);
    }

    // ========== 绘制 ==========
    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        // 应用水平滚动
        canvas.save();
        canvas.translate(-scrollX, 0);

        drawRuler(canvas);
        drawTracks(canvas);
        if (showPlayhead) {
            drawPlayhead(canvas);
        }

        canvas.restore();
    }

    /** 绘制时间标尺（含小节线） */
    private void drawRuler(Canvas canvas) {
        float rulerH = RULER_HEIGHT * density;
        canvas.drawRect(scrollX, 0, scrollX + getWidth(), rulerH, rulerPaint);

        // 每小节秒数 = (60 / bpm) * beatsPerBar
        float secPerBar = 60f / bpm * beatsPerBar;
        float pxPerBar  = secPerBar * pixelsPerSecond;

        // 第一个可见小节
        int firstBar = Math.max(0, (int)(scrollX / pxPerBar) - 1);
        int lastBar  = (int)((scrollX + getWidth()) / pxPerBar) + 2;

        float totalH = rulerH + tracks.size() * TRACK_HEIGHT * density;

        for (int bar = firstBar; bar <= lastBar; bar++) {
            float x = bar * pxPerBar;

            // 延伸到轨道区域的淡色小节竖线
            canvas.drawLine(x, rulerH, x, totalH, measureLinePaint);

            // 标尺上的小节刻度
            canvas.drawLine(x, rulerH - 12 * density, x, rulerH, rulerTextPaint);
            // 小节编号（从1起）
            String label = String.valueOf(bar + 1);
            canvas.drawText(label, x + 3 * density, rulerH - 16 * density, rulerTextPaint);
        }

        // 秒级刻度（仅短线，不显示文字，避免与小节标签冲突）
        int visStart = (int)(scrollX / pixelsPerSecond);
        int visEnd   = (int)((scrollX + getWidth()) / pixelsPerSecond) + 1;
        for (int sec = visStart; sec <= visEnd; sec++) {
            float x = sec * pixelsPerSecond;
            rulerTextPaint.setAlpha(80);
            canvas.drawLine(x, rulerH - 5 * density, x, rulerH, rulerTextPaint);
            rulerTextPaint.setAlpha(255);
        }
    }

    /** 绘制轨道和 clip */
    private void drawTracks(Canvas canvas) {
        float rulerH = RULER_HEIGHT * density;
        float trackH = TRACK_HEIGHT * density;
        float clipMargin = CLIP_MARGIN * density;

        for (int i = 0; i < tracks.size(); i++) {
            SampleCreatorActivity.Track track = tracks.get(i);
            float trackTop = rulerH + i * trackH;

            // 轨道背景（交替颜色）
            trackBgPaint.setColor(i % 2 == 0 ? 0xFF1E1E2E : 0xFF252535);
            canvas.drawRect(scrollX, trackTop, scrollX + getWidth(), trackTop + trackH, trackBgPaint);

            // 绘制该轨道的所有 clip
            for (SampleCreatorActivity.Clip clip : track.clips) {
                drawClip(canvas, clip, track, trackTop, trackH, clipMargin);
            }
        }
    }

    /** 绘制单个 clip 块（含 trim 手柄） */
    private void drawClip(Canvas canvas, SampleCreatorActivity.Clip clip,
                          SampleCreatorActivity.Track track, float trackTop, float trackH, float margin) {
        float startX = clip.startTimeMs / 1000f * pixelsPerSecond;
        float clipWidth = clip.playDurationMs() / 1000f * pixelsPerSecond;
        clipWidth = Math.max(MIN_CLIP_WIDTH * density, clipWidth);

        RectF rect = new RectF(startX, trackTop + margin, startX + clipWidth, trackTop + trackH - margin);

        // clip 块主体
        clipPaint.setColor(track.color);
        canvas.drawRoundRect(rect, 6 * density, 6 * density, clipPaint);

        // 选中框
        if (clip == selectedClip) {
            canvas.drawRoundRect(rect, 6 * density, 6 * density, selectionPaint);
        }

        // 文件名
        String name = clip.fileName;
        if (name.length() > 15) name = name.substring(0, 12) + "...";
        canvas.drawText(name, startX + 6 * density, trackTop + 20 * density, clipTextPaint);

        // 时长标签（秒 + 小节数）
        float secPerBar = 60f / bpm * beatsPerBar;
        float bars = clip.playDurationMs() / 1000f / secPerBar;
        String duration = String.format(Locale.US, "%.1fs (%.1f\u5c0f\u8282)", clip.playDurationMs() / 1000f, bars);
        clipTextPaint.setTextSize(9 * density);
        canvas.drawText(duration, startX + 6 * density, trackTop + 36 * density, clipTextPaint);
        clipTextPaint.setTextSize(11 * density);

        // ⋮ 图标（选中时）
        if (clip == selectedClip) {
            float settingsIconSize = 16 * density;
            float settingsX = (startX + clipWidth) - (settingsIconSize + 4 * density);
            float settingsY = trackTop + margin + 2 * density;
            clipTextPaint.setTextSize(16 * density);
            clipTextPaint.setColor(0xFFCCCCCC);
            canvas.drawText("⋮", settingsX, settingsY + 12 * density, clipTextPaint);
            clipTextPaint.setColor(0xFFFFFFFF);
            clipTextPaint.setTextSize(11 * density);
        }

        // Trim 手柄（左端 & 右端，始终显示）
        float hw = TRIM_HANDLE_WIDTH_DP * density;
        float ht = trackH - margin * 2;
        // 左手柄
        RectF lh = new RectF(startX, trackTop + margin, startX + hw, trackTop + trackH - margin);
        int lAlpha = (trimClip == clip && trimMode == TRIM_START) ? 0xFF : 0xAA;
        trimHandlePaint.setAlpha(lAlpha);
        canvas.drawRoundRect(lh, 3 * density, 3 * density, trimHandlePaint);
        // 右手柄
        RectF rh = new RectF(startX + clipWidth - hw, trackTop + margin, startX + clipWidth, trackTop + trackH - margin);
        int rAlpha = (trimClip == clip && trimMode == TRIM_END) ? 0xFF : 0xAA;
        trimHandlePaint.setAlpha(rAlpha);
        canvas.drawRoundRect(rh, 3 * density, 3 * density, trimHandlePaint);
        trimHandlePaint.setAlpha(0xFF);
    }

    /** 绘制播放头 */
    private void drawPlayhead(Canvas canvas) {
        float x = playheadMs / 1000f * pixelsPerSecond;
        float rulerH = RULER_HEIGHT * density;
        float totalH = rulerH + tracks.size() * TRACK_HEIGHT * density;

        // 红色竖线
        canvas.drawLine(x, rulerH, x, totalH, playheadPaint);

        // 顶部三角形
        Paint.Style oldStyle = playheadPaint.getStyle();
        playheadPaint.setStyle(Paint.Style.FILL);
        android.graphics.Path path = new android.graphics.Path();
        path.moveTo(x, rulerH);
        path.lineTo(x - 6 * density, rulerH - 8 * density);
        path.lineTo(x + 6 * density, rulerH - 8 * density);
        path.close();
        canvas.drawPath(path, playheadPaint);
        playheadPaint.setStyle(oldStyle);
    }

    // ========== 触摸事件 ==========
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        boolean handled = scaleDetector.onTouchEvent(event);

        if (!scaleDetector.isInProgress()) {
            float slopPx = DRAG_SLOP_DP * density;
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN: {
                    float ax = event.getX(), ay = event.getY();
                    // 优先检测 trim 手柄
                    int[] res = findTrimHandle(ax, ay);
                    if (res != null) {
                        trimMode       = res[0];
                        trimTrackIdx   = res[1];
                        trimClip       = findClipAt(ax, ay);
                        trimStartTouchX = ax;
                        if (getParent() != null)
                            getParent().requestDisallowInterceptTouchEvent(true);
                        invalidate();
                        return true;
                    }
                    pendingClip      = findClipAt(ax, ay);
                    dragStartX       = ax;
                    dragStartScreenY = ay;
                    isDragActive     = false;
                    draggedClip      = null;
                    if (pendingClip != null && getParent() != null)
                        getParent().requestDisallowInterceptTouchEvent(true);
                    break;
                }
                case MotionEvent.ACTION_MOVE: {
                    // Trim 手柄拖动
                    if (trimMode != TRIM_NONE && trimClip != null) {
                        float worldX  = event.getX() + scrollX;
                        long  touchMs = (long)(worldX / pixelsPerSecond * 1000);
                        if (trimMode == TRIM_START) {
                            long newTrimStart = touchMs - trimClip.startTimeMs;
                            newTrimStart = Math.max(0, Math.min(newTrimStart, trimClip.trimEndMs - 50));
                            trimClip.trimStartMs = newTrimStart;
                        } else {
                            long newTrimEnd = touchMs - trimClip.startTimeMs;
                            newTrimEnd = Math.max(trimClip.trimStartMs + 50, Math.min(newTrimEnd, trimClip.durationMs));
                            trimClip.trimEndMs = newTrimEnd;
                        }
                        invalidate();
                        return true;
                    }
                    if (!isDragActive && pendingClip != null &&
                            Math.abs(event.getX() - dragStartX) > slopPx) {
                        isDragActive    = true;
                        draggedClip     = pendingClip;
                        selectedClip    = pendingClip;
                        draggedTrackIdx = screenYToTrackIndex(dragStartScreenY);
                        if (draggedTrackIdx < 0) draggedTrackIdx = 0;
                        float wx0 = dragStartX + scrollX;
                        dragOffsetMs = (long)(wx0 / pixelsPerSecond * 1000) - draggedClip.startTimeMs;
                        performHapticFeedback(android.view.HapticFeedbackConstants.LONG_PRESS);
                        if (getParent() != null)
                            getParent().requestDisallowInterceptTouchEvent(true);
                    }
                    if (isDragActive && draggedClip != null) {
                        float worldX = event.getX() + scrollX;
                        long newStartMs = (long)(worldX / pixelsPerSecond * 1000) - dragOffsetMs;
                        draggedClip.startTimeMs = Math.max(0, newStartMs);
                        int newTrackIdx = screenYToTrackIndex(event.getY());
                        if (newTrackIdx >= 0 && newTrackIdx != draggedTrackIdx
                                && newTrackIdx < tracks.size()) {
                            tracks.get(draggedTrackIdx).clips.remove(draggedClip);
                            tracks.get(newTrackIdx).clips.add(draggedClip);
                            draggedTrackIdx = newTrackIdx;
                        }
                        float edgeZone = 60 * density;
                        float viewW = getWidth();
                        if (viewW > 0) {
                            if (event.getX() > viewW - edgeZone) {
                                scrollX += 12f * (event.getX() - (viewW - edgeZone)) / edgeZone;
                            } else if (event.getX() < edgeZone) {
                                scrollX = Math.max(0, scrollX - 12f * (edgeZone - event.getX()) / edgeZone);
                            }
                        }
                        invalidate();
                        handled = true;
                    }
                    break;
                }
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    if (trimMode != TRIM_NONE && trimClip != null && trimListener != null) {
                        trimListener.onTrimChanged(trimClip);
                    }
                    trimMode      = TRIM_NONE;
                    trimClip      = null;
                    trimTrackIdx  = -1;
                    if (isDragActive && draggedClip != null && editListener != null) {
                        editListener.onClipMoved();
                    }
                    if (getParent() != null)
                        getParent().requestDisallowInterceptTouchEvent(false);
                    draggedClip     = null;
                    pendingClip     = null;
                    isDragActive    = false;
                    draggedTrackIdx = -1;
                    break;
            }
        }

        handled |= gestureDetector.onTouchEvent(event);
        return handled || super.onTouchEvent(event);
    }

    /**
     * 检测屏幕坐标是否在某个 clip 的左/右 trim 手柄上。
     * @return int[]{TRIM_START|TRIM_END, trackIdx}，或 null
     */
    private int[] findTrimHandle(float screenX, float screenY) {
        int trackIdx = screenYToTrackIndex(screenY);
        if (trackIdx < 0 || trackIdx >= tracks.size()) return null;
        SampleCreatorActivity.Track track = tracks.get(trackIdx);
        float worldX   = screenX + scrollX;
        float rulerH   = RULER_HEIGHT * density;
        float trackH   = TRACK_HEIGHT * density;
        float trackTop = rulerH + trackIdx * trackH;
        float margin   = CLIP_MARGIN * density;
        float hw       = TRIM_HANDLE_WIDTH_DP * density;
        float pad      = 6 * density;
        for (SampleCreatorActivity.Clip clip : track.clips) {
            float startX   = clip.startTimeMs / 1000f * pixelsPerSecond;
            float clipWidth = Math.max(MIN_CLIP_WIDTH * density,
                    clip.playDurationMs() / 1000f * pixelsPerSecond);
            boolean inY = screenY >= trackTop + margin - pad && screenY <= trackTop + trackH - margin + pad;
            if (!inY) continue;
            // 左手柄区域
            if (worldX >= startX - pad && worldX <= startX + hw + pad) {
                return new int[]{TRIM_START, trackIdx};
            }
            // 右手柄区域
            if (worldX >= startX + clipWidth - hw - pad && worldX <= startX + clipWidth + pad) {
                return new int[]{TRIM_END, trackIdx};
            }
        }
        return null;
    }

    /**
     * 单击处理：
     * - 点击 clip：选中该 clip（再次点击已选中 clip 的 ⋮ 图标则打开编辑）
     * - 点击空白区域：取消选中
     * x/y 为原始屏幕坐标（未加 scrollX）
     */
    private void handleTap(float x, float y) {
        SampleCreatorActivity.Clip clip = findClipAt(x, y);
        if (clip == null) {
            // 点击空白 → 取消选中
            selectedClip    = null;
            selectedTrackIdx = -1;
            invalidate();
            return;
        }
        int trackIdx = screenYToTrackIndex(y);
        if (clip == selectedClip) {
            // 已选中状态下点击 ⋮ 图标 → 打开剪辑对话框
            if (isOnSettingsIcon(x, y, clip, trackIdx) && editListener != null) {
                editListener.onClipLongPress(clip, trackIdx);
            }
        } else {
            // 选中新 clip
            selectedClip     = clip;
            selectedTrackIdx = trackIdx;
            invalidate();
        }
    }

    /**
     * 判断触摸点是否在 clip 右上角的 ⋮ 设置图标热区内
     * x/y 为原始屏幕坐标（未加 scrollX）
     */
    private boolean isOnSettingsIcon(float screenX, float screenY,
                                     SampleCreatorActivity.Clip clip, int trackIdx) {
        float worldX = screenX + scrollX;
        float rulerH = RULER_HEIGHT * density;
        float trackH = TRACK_HEIGHT * density;
        float margin = CLIP_MARGIN * density;
        float trackTop = rulerH + trackIdx * trackH;

        float startX = clip.startTimeMs / 1000f * pixelsPerSecond;
        float clipWidth = Math.max(MIN_CLIP_WIDTH * density,
                clip.playDurationMs() / 1000f * pixelsPerSecond);

        // ⋮ 图标绘制位置（同 drawClip 中保持一致）
        float iconSize = 16 * density;
        float iconLeft  = (startX + clipWidth) - (iconSize + 4 * density);
        float iconRight = (startX + clipWidth);
        float iconTop   = trackTop + margin;
        float iconBottom = trackTop + margin + iconSize + 4 * density;

        // 扩大 8dp 热区，使其更容易点击
        float pad = 8 * density;
        return worldX >= iconLeft - pad && worldX <= iconRight + pad
                && screenY >= iconTop - pad && screenY <= iconBottom + pad;
    }

    /**
     * 查找原始屏幕坐标 (screenX, screenY) 下的 clip。
     * screenX 不含 scrollX，方法内部转换为世界坐标。
     */
    private SampleCreatorActivity.Clip findClipAt(float screenX, float screenY) {
        int trackIdx = screenYToTrackIndex(screenY);
        if (trackIdx < 0 || trackIdx >= tracks.size()) return null;

        SampleCreatorActivity.Track track = tracks.get(trackIdx);
        float worldX = screenX + scrollX; // 只在这里加一次 scrollX
        float rulerH = RULER_HEIGHT * density;
        float trackH = TRACK_HEIGHT * density;
        float trackTop = rulerH + trackIdx * trackH;
        float clipMargin = CLIP_MARGIN * density;

        for (SampleCreatorActivity.Clip clip : track.clips) {
            float startX = clip.startTimeMs / 1000f * pixelsPerSecond;
            float clipWidth = Math.max(MIN_CLIP_WIDTH * density,
                    clip.playDurationMs() / 1000f * pixelsPerSecond);
            if (worldX >= startX && worldX <= startX + clipWidth &&
                    screenY >= trackTop + clipMargin && screenY <= trackTop + trackH - clipMargin) {
                return clip;
            }
        }
        return null;
    }

    // ========== 坐标转换 ==========
    /** 屏幕 X 坐标 → 时间轴毫秒 */
    public long screenXToMs(float screenX) {
        float worldX = screenX + scrollX;
        return (long)(worldX / pixelsPerSecond * 1000);
    }

    /** 时间轴毫秒 → 屏幕 X 坐标 */
    public float msToScreenX(long ms) {
        return ms / 1000f * pixelsPerSecond - scrollX;
    }

    /** 屏幕 Y 坐标 → 轨道索引 */
    public int screenYToTrackIndex(float screenY) {
        float rulerH = RULER_HEIGHT * density;
        if (screenY < rulerH) return -1;
        int idx = (int)((screenY - rulerH) / (TRACK_HEIGHT * density));
        return idx >= 0 && idx < tracks.size() ? idx : -1;
    }
}
