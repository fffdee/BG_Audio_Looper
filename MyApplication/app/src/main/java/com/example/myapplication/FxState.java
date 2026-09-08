package com.example.myapplication;

import java.util.HashMap;
import java.util.Map;

/**
 * 效果节点开关状态（进程内共享）。
 *
 * 音效界面（FxControlActivity）通过 graph query all 拿到各节点真实 bypass 状态后
 * 写入这里；Looper 的段设置里按声道显示效果开关时从这里读，避免两个界面状态不一致。
 *
 * 值语义：true = 效果启用（bypass=off），false = 旁路直通（bypass=on）。
 */
public class FxState {
    private static final Map<String, Boolean> enabled = new HashMap<>();
    private static boolean loaded = false;

    public static void put(String nodeName, boolean on) {
        enabled.put(nodeName, on);
    }

    /** 未查询过该节点时用 def（固件默认 bypass=true，故默认 false） */
    public static boolean get(String nodeName, boolean def) {
        Boolean v = enabled.get(nodeName);
        return (v != null) ? v : def;
    }

    public static boolean has(String nodeName) {
        return enabled.containsKey(nodeName);
    }

    /** 是否已完成过一次 graph query all 回填 */
    public static boolean isLoaded() {
        return loaded;
    }

    public static void setLoaded(boolean v) {
        loaded = v;
    }
}
