package com.example.myapplication;

import android.util.Log;
import java.util.HashMap;
import java.util.Map;

public class ProductRegistry {
    private static final String TAG = "ProductRegistry";
    private static ProductRegistry instance;
    private final Map<Integer, ProductConfig> products = new HashMap<>();

    private ProductRegistry() {
        registerBanBox();
    }

    public static synchronized ProductRegistry getInstance() {
        if (instance == null) {
            instance = new ProductRegistry();
        }
        return instance;
    }

    private void registerBanBox() {
        ProductConfig banbox = new ProductConfig.Builder(BleProtocol.PRODUCT_ID_BANBOX, "BanBox")
                .addFeature(new FeatureModule("fx", "FX", FxControlActivity.class, R.drawable.reverb))
                .addFeature(new FeatureModule("eq", "EQ", EqControlActivity.class, R.drawable.eq))
                .addFeature(new FeatureModule("volume", "Volume", HardwareVolumeActivity.class, R.drawable.vol))
                .addFeature(new FeatureModule("metronome", "Metronome", MetronomeActivity.class, R.drawable.metro))
                .addFeature(new FeatureModule("audio_chain", "Audio Chain", AudioChainDiagramActivity.class, R.drawable.graph))
                .addFeature(new FeatureModule("looper", "Looper", LooperControlActivity.class, R.drawable.setting))
                .setHasOta(true)
                .build();
        products.put(BleProtocol.PRODUCT_ID_BANBOX, banbox);
    }

    public void registerProduct(ProductConfig config) {
        products.put(config.getProductId(), config);
        Log.d(TAG, "Registered product: " + config.getProductName() + " (0x"
                + String.format("%04X", config.getProductId()) + ")");
    }

    public ProductConfig getProduct(int productId) {
        ProductConfig config = products.get(productId);
        if (config == null) {
            Log.w(TAG, "Unknown product ID: 0x" + String.format("%04X", productId));
        }
        return config;
    }

    public ProductConfig getCurrentProduct() {
        int pid = BleParamCache.getInstance().getProductId();
        if (pid == 0) return null;
        return getProduct(pid);
    }
}
