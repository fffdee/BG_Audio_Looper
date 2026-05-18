 package com.example.myapplication;

import android.os.Bundle;
import android.widget.ImageButton;
import android.widget.TextView;
import android.widget.Toast;

/**
 * 电池曲线矫正页面
 *
 * 导航栏图标：
 *   刷新（ic_refresh）- 同步曲线
 *   播放/停止（ic_play / ic_stop）- 开始 / 停止矫正切换
 */
public class BatteryCurveActivity extends BaseActivity {

    private BatteryCurveView curveView;
    private TextView tvCalibStatus;
    private TextView tvPointCount;
    private TextView tvBleStatus;
    private ImageButton btnSync;
    private ImageButton btnCalibToggle;

    private BluetoothHelper bleHelper;
    private boolean isCalibRunning = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        bleHelper = com.example.myapplication.BluetoothManager.getInstance().getBluetoothHelper();

        setContentView(R.layout.activity_battery_curve);
        setupBaseToolbar(true);

        curveView      = findViewById(R.id.battery_curve_view);
        tvCalibStatus  = findViewById(R.id.tv_calib_status);
        tvPointCount   = findViewById(R.id.tv_point_count);
        tvBleStatus    = findViewById(R.id.tv_ble_connected);
        btnSync        = findViewById(R.id.btn_sync_curve);
        btnCalibToggle = findViewById(R.id.btn_calib_toggle);

        updateBleStatusText();

        // 观察曲线数据 LiveData
        bleHelper.getBatteryCurveLiveData().observe(this, points -> {
            if (points != null) {
                curveView.setCurvePoints(points.length > 0 ? points : null);
                tvPointCount.setText("曲线点数: " + points.length);
            } else {
                curveView.setCurvePoints(null);
                tvPointCount.setText("曲线点数: —");
            }
        });

        // 观察矫正状态 LiveData，同步导航栏图标
        bleHelper.getCalibStateLiveData().observe(this, state -> {
            tvCalibStatus.setText("状态: " + state);
            boolean running = state != null && state.startsWith("矫正中");
            if (running != isCalibRunning) {
                isCalibRunning = running;
                updateCalibToggleIcon();
            }
        });

        // 观察 BLE 连接状态
        bleHelper.getConnectionStateLiveData().observe(this, state -> {
            updateBleStatusText();
            if (!state.isConnected()) {
                tvCalibStatus.setText("状态: 未连接");
                isCalibRunning = false;
                updateCalibToggleIcon();
            }
        });

        // 导航栏刷新按钮
        if (btnSync != null) {
            btnSync.setOnClickListener(v -> {
                if (!bleHelper.isServiceReady()) {
                    Toast.makeText(this, bleHelper.isConnected() ? "蓝牙服务未就绪，请稍候" : "请先连接蓝牙设备", Toast.LENGTH_SHORT).show();
                    return;
                }
                bleHelper.requestBatteryCurve();
                Toast.makeText(this, "已请求同步电池曲线", Toast.LENGTH_SHORT).show();
            });
        }

        // 导航栏矫正切换按钮
        if (btnCalibToggle != null) {
            btnCalibToggle.setOnClickListener(v -> {
                if (!bleHelper.isServiceReady()) {
                    Toast.makeText(this, bleHelper.isConnected() ? "蓝牙服务未就绪，请稍候" : "请先连接蓝牙设备", Toast.LENGTH_SHORT).show();
                    return;
                }
                if (isCalibRunning) {
                    com.example.myapplication.BluetoothManager.getInstance()
                            .sendCalibCmd(BleProtocol.CALIB_CMD_STOP);
                    Toast.makeText(this, "已发送停止矫正指令", Toast.LENGTH_SHORT).show();
                } else {
                    com.example.myapplication.BluetoothManager.getInstance()
                            .sendCalibCmd(BleProtocol.CALIB_CMD_START);
                    Toast.makeText(this, "已发送开始矫正指令", Toast.LENGTH_SHORT).show();
                }
            });
        }
    }

    private void updateCalibToggleIcon() {
        if (btnCalibToggle == null) return;
        if (isCalibRunning) {
            btnCalibToggle.setImageResource(R.drawable.ic_stop);
            btnCalibToggle.setContentDescription("停止矫正");
        } else {
            btnCalibToggle.setImageResource(R.drawable.ic_play);
            btnCalibToggle.setContentDescription("开始矫正");
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        updateBleStatusText();
        // 每次回到页面自动请求一次最新曲线（需要服务完全就绪）
        if (bleHelper.isServiceReady()) {
            bleHelper.requestBatteryCurve();
        }
    }

    private void updateBleStatusText() {
        if (bleHelper.isConnected()) {
            tvBleStatus.setText("● 已连接");
            tvBleStatus.setTextColor(0xFF00C980);
        } else {
            tvBleStatus.setText("● 未连接");
            tvBleStatus.setTextColor(0xFFFF4444);
        }
    }

    @Override
    protected String getToolbarTitle() {
        return "电池曲线矫正";
    }
}