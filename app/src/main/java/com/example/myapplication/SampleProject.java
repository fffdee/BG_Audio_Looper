package com.example.myapplication;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;

import java.util.ArrayList;
import java.util.List;

/**
 * 采样创作项目数据模型，用于序列化/反序列化到 project.json
 */
public class SampleProject {

    /** 项目名称 */
    private String projectName;

    /** 创建时间戳（毫秒） */
    private long createTime;

    /** 轨道列表 */
    private List<TrackData> tracks = new ArrayList<>();

    // ========== 轨道数据 ==========

    public static class TrackData {
        public String name;
        public int volume = 80;
        public boolean muted = false;
        public int color;
        public List<ClipData> clips = new ArrayList<>();
    }

    // ========== 片段数据 ==========

    public static class ClipData {
        /** WAV 文件名（项目文件夹中的相对文件名） */
        public String fileName;
        /** 原始文件总时长，毫秒 */
        public long durationMs;
        /** 在时间轴上的起始位置，毫秒 */
        public long startTimeMs;
        /** 裁剪起点，毫秒 */
        public long trimStartMs;
        /** 裁剪终点，毫秒 */
        public long trimEndMs;
    }

    // ========== 构造 / 访问 ==========

    public SampleProject() {}

    public SampleProject(String projectName) {
        this.projectName = projectName;
        this.createTime = System.currentTimeMillis();
    }

    public String getProjectName() { return projectName; }
    public void setProjectName(String projectName) { this.projectName = projectName; }

    public long getCreateTime() { return createTime; }
    public void setCreateTime(long createTime) { this.createTime = createTime; }

    public List<TrackData> getTracks() { return tracks; }
    public void setTracks(List<TrackData> tracks) { this.tracks = tracks; }

    /** 获取项目内所有片段总数 */
    public int getTotalClipCount() {
        int count = 0;
        for (TrackData t : tracks) {
            count += t.clips.size();
        }
        return count;
    }

    // ========== JSON 序列化 ==========

    private static final Gson GSON = new GsonBuilder().setPrettyPrinting().create();

    public String toJson() {
        return GSON.toJson(this);
    }

    public static SampleProject fromJson(String json) {
        return GSON.fromJson(json, SampleProject.class);
    }

    // ========== 与 SampleCreatorActivity 的数据转换 ==========

    /**
     * 从 SampleCreatorActivity 的 Track/Clip 对象列表生成 SampleProject。
     * 注意：Clip.fileUri 不序列化，只保存 fileName（相对文件名）。
     */
    public static SampleProject fromActivityTracks(String name,
                                                   List<SampleCreatorActivity.Track> activityTracks) {
        SampleProject project = new SampleProject(name);
        for (SampleCreatorActivity.Track at : activityTracks) {
            TrackData td = new TrackData();
            td.name = at.name;
            td.volume = at.volume;
            td.muted = at.muted;
            td.color = at.color;
            for (SampleCreatorActivity.Clip ac : at.clips) {
                ClipData cd = new ClipData();
                cd.fileName = ac.fileName;
                cd.durationMs = ac.durationMs;
                cd.startTimeMs = ac.startTimeMs;
                cd.trimStartMs = ac.trimStartMs;
                cd.trimEndMs = ac.trimEndMs;
                td.clips.add(cd);
            }
            project.tracks.add(td);
        }
        return project;
    }

    /**
     * 将项目数据还原为 SampleCreatorActivity 使用的 Track/Clip 对象列表。
     * fileUri 由调用方根据项目文件夹路径设置。
     */
    public List<SampleCreatorActivity.Track> toActivityTracks() {
        List<SampleCreatorActivity.Track> result = new ArrayList<>();
        for (TrackData td : tracks) {
            SampleCreatorActivity.Track at = new SampleCreatorActivity.Track(td.name, td.color);
            at.volume = td.volume;
            at.muted = td.muted;
            for (ClipData cd : td.clips) {
                // fileUri 暂设 null，由 SampleProjectManager.loadProject() 填充
                SampleCreatorActivity.Clip ac = new SampleCreatorActivity.Clip(
                        cd.fileName, null, cd.durationMs);
                ac.startTimeMs = cd.startTimeMs;
                ac.trimStartMs = cd.trimStartMs;
                ac.trimEndMs = cd.trimEndMs;
                at.clips.add(ac);
            }
            result.add(at);
        }
        return result;
    }
}
