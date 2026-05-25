package com.example.myapplication;

public class FeatureModule {
    private final String id;
    private final String name;
    private final Class<?> activityClass;
    private final int backgroundResId;

    public FeatureModule(String id, String name, Class<?> activityClass, int backgroundResId) {
        this.id = id;
        this.name = name;
        this.activityClass = activityClass;
        this.backgroundResId = backgroundResId;
    }

    public String getId() { return id; }
    public String getName() { return name; }
    public Class<?> getActivityClass() { return activityClass; }
    public int getBackgroundResId() { return backgroundResId; }
}
