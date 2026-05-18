package com.example.myapplication;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class ProductConfig {
    private final int productId;
    private final String productName;
    private final List<FeatureModule> featureModules;
    private final boolean hasOta;

    private ProductConfig(Builder builder) {
        this.productId = builder.productId;
        this.productName = builder.productName;
        this.featureModules = Collections.unmodifiableList(new ArrayList<>(builder.featureModules));
        this.hasOta = builder.hasOta;
    }

    public int getProductId() { return productId; }
    public String getProductName() { return productName; }
    public List<FeatureModule> getFeatureModules() { return featureModules; }
    public boolean hasOta() { return hasOta; }

    public boolean hasFeature(String featureId) {
        for (FeatureModule m : featureModules) {
            if (m.getId().equals(featureId)) return true;
        }
        return false;
    }

    public static class Builder {
        private final int productId;
        private final String productName;
        private final List<FeatureModule> featureModules = new ArrayList<>();
        private boolean hasOta = true;

        public Builder(int productId, String productName) {
            this.productId = productId;
            this.productName = productName;
        }

        public Builder addFeature(FeatureModule module) {
            featureModules.add(module);
            return this;
        }

        public Builder setHasOta(boolean hasOta) {
            this.hasOta = hasOta;
            return this;
        }

        public ProductConfig build() {
            return new ProductConfig(this);
        }
    }
}
