<!-- ==========智慧鱼塘 · 远程环境控制的功能========== -->
<!--页面会实时显示水温、溶解氧、PH、空气质量、环境温湿度、大气压的值-->
<!--远程控制气泵、水泵、风扇的开关，可调节各参数阀值-->
<!--报警状态检测水温、PH、溶解氧、空气质量是否正常-->

<template>
	<view class="wrap">

		<!-- ========== 水质参数 ========== -->
		<view class="section-title">💧 水质参数</view>
		<view class="dev-area">

			<view class="dev-cart" :style="{backgroundColor: waterTempAlarm ? '#ffebee' : '#e3f2fd'}">
				<view>
					<view class="dev-name">水温</view>
					<image class="dev-logo" src="../../static/temp.png"></image>
				</view>
				<view class="dev-data" :style="{color: waterTempAlarm ? '#e53935' : '#1565c0'}">{{water_temp}} ℃</view>
			</view>

			<view class="dev-cart" :style="{backgroundColor: doAlarm ? '#ffebee' : '#e8f5e9'}">
				<view>
					<view class="dev-name">溶解氧(估算)</view>
					<image class="dev-logo" src="../../static/humi.png"></image>
				</view>
				<view class="dev-data" :style="{color: doAlarm ? '#e53935' : '#2e7d32'}">{{do_value}} mg/L</view>
			</view>

			<view class="dev-cart" :style="{backgroundColor: phAlarm ? '#ffebee' : '#fff3e0'}">
				<view>
					<view class="dev-name">PH值</view>
					<image class="dev-logo" src="../../static/ph.svg"></image>
				</view>
				<view class="dev-data" :style="{color: phAlarm ? '#e53935' : '#e65100'}">{{ph_value}}</view>
			</view>

			<view class="dev-cart" :style="{backgroundColor: airAlarm ? '#ffebee' : '#f3e5f5'}">
				<view>
					<view class="dev-name">空气质量</view>
					<image class="dev-logo" src="../../static/smog.png"></image>
				</view>
				<view class="dev-data" :style="{color: airAlarm ? '#e53935' : '#7b1fa2'}">{{mq2_vol}}</view>
			</view>

		</view>

		<!-- ========== 环境参数 ========== -->
		<view class="section-title">🌡 环境参数</view>
		<view class="dev-area">

			<view class="dev-cart">
				<view>
					<view class="dev-name">空气温度</view>
					<image class="dev-logo" src="../../static/temp.png"></image>
				</view>
				<view class="dev-data">{{air_temp}} ℃</view>
			</view>

			<view class="dev-cart">
				<view>
					<view class="dev-name">空气湿度</view>
					<image class="dev-logo" src="../../static/humi.png"></image>
				</view>
				<view class="dev-data">{{air_humi}} %</view>
			</view>

			<view class="dev-cart">
				<view>
					<view class="dev-name">大气压</view>
					<image class="dev-logo" src="../../static/pressure.svg"></image>
				</view>
				<view class="dev-data">{{bmp_pressure}} kPa</view>
			</view>

		</view>

		<!-- ========== 设备控制 ========== -->
		<view class="section-title">⚙ 设备控制</view>

		<view class="dev-cart-l" :style="{backgroundColor: aerator ? '#e8f5e9' : '#f5f5f5'}">
			<view style="display:flex;align-items:center;">
				<view class="dev-name" style="font-size:28rpx;margin-right:10rpx;">🫧 气泵</view>
			</view>
			<switch :checked="aerator" @change="onAeratorSwitch" color="#2b9939" />

			</view>
		<view class="dev-cart-l" :style="{backgroundColor: pump ? '#e3f2fd' : '#f5f5f5'}">
			<view style="display:flex;align-items:center;">
				<view class="dev-name" style="font-size:28rpx;margin-right:10rpx;">💦 水泵</view>
			</view>
			<switch :checked="pump" @change="onPumpSwitch" color="#1565c0" />
		</view>

		<view class="dev-cart-l" :style="{backgroundColor: fan ? '#fff3e0' : '#f5f5f5'}">
			<view style="display:flex;align-items:center;">
				<view class="dev-name" style="font-size:28rpx;margin-right:10rpx;">🌀 风扇</view>
			</view>
			<switch :checked="fan" @change="onFanSwitch" color="#e65100" />
		</view>
		<view class="device-cart-l" style="margin-top:10rpx;">
			<view><view class="dev-name">风扇调速</view></view>
			<view class="ctrl-slider">
				<slider :value="fan_adj" @change="sliderChangeFan($event)" min="0" max="100" step="1" block-size="20" show-value />
			</view>
		</view>

		<!-- ========== 阀值设置 (每参数一个) ========== -->

			<view class="dev-cart-l" :style="{backgroundColor: auto_mode ? '#e8f5e9' : '#fff3e0'}">
				<view style="display:flex;align-items:center;">
					<view class="dev-name" style="font-size:28rpx;margin-right:10rpx;">🔄 控制模式</view>
				</view>
				<view style="display:flex;align-items:center;">
					<text style="font-size:24rpx;margin-right:10rpx;">{{ auto_mode ? '自动' : '手动' }}</text>
					<switch :checked="auto_mode" @change="onModeSwitch" color="#2b9939" />
				</view>
			</view>
		<view class="section-title">🎛 阀值设置</view>

		<view class="device-cart-l">
			<view><view class="dev-name">烟雾浓度阈值 (0~10)</view></view>
			<view class="ctrl-slider">
				<slider :value="smog_th" @change="sliderChangeSmogTh($event)" min="0" max="10" step="0.1" block-size="20" show-value />
			</view>
		</view>

		<view class="device-cart-l">
			<view><view class="dev-name">溶解氧低阈值(mg/L)</view></view>
			<view class="ctrl-slider">
				<slider :value="do_low_th" @change="sliderChangeDoTh($event)" min="0" max="10" step="0.1" block-size="20" show-value />
			</view>
		</view>

		<view class="device-cart-l">
			<view><view class="dev-name">PH低阈值</view></view>
			<view class="ctrl-slider">
				<slider :value="ph_low_th" @change="sliderChangePhTh($event)" min="0" max="8.5" step="0.1" block-size="20" show-value />
			</view>
		</view>

		<view class="device-cart-l">
			<view><view class="dev-name">水温高阈值(℃)</view></view>
			<view class="ctrl-slider">
				<slider :value="water_temp_high_th" @change="sliderChangeWtTh($event)" min="2.0" max="60" step="0.1" block-size="20" show-value />
			</view>
		</view>

		<view class="device-cart-l">
			<view><view class="dev-name">气温高阈值(℃)</view></view>
			<view class="ctrl-slider">
				<slider :value="air_temp_high_th" @change="sliderChangeAtTh($event)" min="10" max="60" step="0.1" block-size="20" show-value />
			</view>
		</view>

		<!-- ========== 报警状态 ========== -->
		<view class="section-title">🔔 报警状态</view>

		<view v-if="alarm" class="dev-cart-l alarm-active">
			<view style="display:flex;align-items:center;">
				<view class="dev-name" style="color:#fff;font-size:28rpx;">🚨 检查到异常!</view>
			</view>
			<view style="color:#fff;font-size:36rpx;font-weight:bold;">报警中</view>
		</view>
		<view v-else class="dev-cart-l" style="background-color:#43a047;">
			<view style="display:flex;align-items:center;">
				<view class="dev-name" style="color:#fff;">✅ 系统正常</view>
			</view>
			<view style="color:#fff;font-size:36rpx;">安全</view>
		</view>

		<view class="alarm-detail">
			<view class="alarm-item" :style="{color: doAlarm ? '#e53935' : '#999'}">{{ doAlarm ? '⚠' : '✓' }} 溶氧 {{ doAlarm ? '过低' : '正常' }}</view>
			<view class="alarm-item" :style="{color: phAlarm ? '#e53935' : '#999'}">{{ phAlarm ? '⚠' : '✓' }} pH {{ phAlarm ? '异常' : '正常' }}</view>
			<view class="alarm-item" :style="{color: waterTempAlarm ? '#e53935' : '#999'}">{{ waterTempAlarm ? '⚠' : '✓' }} 水温 {{ waterTempAlarm ? '异常' : '正常' }}</view>
			<view class="alarm-item" :style="{color: airAlarm ? '#e53935' : '#999'}">{{ airAlarm ? '⚠' : '✓' }} 空气 {{ airAlarm ? '超标' : '正常' }}</view>
		</view>

	</view>
</template>

<script>
	const { createCommonToken } = require('@/key.js')

	const product_id = 'YOUR_PRODUCT_ID';
	const device_name = 'YOUR_DEVICE_NAME';

	export default {
		data() {
			return {
				// 传感器数据
				water_temp: '',
				do_value: '',
				ph_value: '',
				air_temp: '',
				air_humi: '',
				mq2_vol: '',
				bmp_pressure: '',
				// 设备状态
				aerator: false,
				pump: false,
				fan: false,
				fan_adj: 0,
				alarm: false,
					auto_mode: true,
				// 阀值 (App可调, 与硬件默认值一致)
				smog_th: 0.4,
				do_low_th: 4.0,
				ph_low_th: 6.5,
				water_temp_high_th: 30.0,
				air_temp_high_th: 38.0,
				// 内部固定阀值 (硬件上报, 报警判断用)
				ph_high_th: 8.5,
				water_temp_low_th: 2.0,
				// 系统
				token: '',
				_timer: null,
				_wasAlarm: false,
			}
		},

		computed: {
			doAlarm() {
				const v = parseFloat(this.do_value);
				const th = parseFloat(this.do_low_th);
				return !isNaN(v) && !isNaN(th) && v > 0.1 && v < th;
			},
			phAlarm() {
				const v = parseFloat(this.ph_value);
				const lo = parseFloat(this.ph_low_th);
				const hi = parseFloat(this.ph_high_th);
				return !isNaN(v) && !isNaN(lo) && !isNaN(hi) && v > 0.5 && (v < lo || v > hi);
			},
			waterTempAlarm() {
				const v = parseFloat(this.water_temp);
				const lo = parseFloat(this.water_temp_low_th);
				const hi = parseFloat(this.water_temp_high_th);
				return !isNaN(v) && !isNaN(lo) && !isNaN(hi) && v > -50 && (v <= lo || v >= hi);
			},
			airAlarm() {
				const v = parseFloat(this.mq2_vol);
				const th = parseFloat(this.smog_th);
				return !isNaN(v) && !isNaN(th) && v > th * 2.0;
			},
		},

		onLoad() {
			const params = {
				access_key: 'YOUR_ACCESS_KEY',
				version: '2022-05-01',
				productid: 'YOUR_PRODUCT_ID',
			}
			try {
					this.token = createCommonToken(params);
					console.log('Token生成成功:', this.token ? this.token.substring(0, 50) + '...' : 'EMPTY');
				} catch(e) {
					console.error('Token生成失败:', e.message);
					uni.showToast({ title: 'Token生成失败: ' + e.message, icon: 'none', duration: 5000 });
				}
		},

		onShow() {
			this.fetchDevData();
			if (this._timer) clearInterval(this._timer);
			this._timer = setInterval(() => { this.fetchDevData(); }, 3000)
		},

		onHide() {
			if (this._timer) { clearInterval(this._timer); this._timer = null; }
		},

		onUnload() {
			if (this._timer) { clearInterval(this._timer); this._timer = null; }
		},

		methods: {
			fetchDevData() {
				uni.request({
					url: 'https://iot-api.heclouds.com/thingmodel/query-device-property',
					method: 'GET',
					data: { product_id: 'YOUR_PRODUCT_ID',
							device_name: 'YOUR_DEVICE_NAME' },
					header: { 'authorization': this.token },
					success: (res) => {
						console.log('Data received:', JSON.stringify(res.data));
						if (!res || !res.data || !Array.isArray(res.data.data)) {
							console.log('数据格式错误', res);
							return;
						}
				const arr = res.data.data;
							const now = Date.now();
							const skip = (key) => this._pendingKeys && this._pendingKeys[key] && this._pendingKeys[key] > now;
							console.log('=== 物模型返回顺序 ===');
							arr.forEach((d,i) => console.log('['+i+'] '+d.id+' = '+d.value));

							if (!skip("aerator")) this.aerator      = (arr[0].value === 'true' || arr[0].value === true);
							this.air_humi     = arr[1].value;
							this.air_temp     = arr[2].value;
							if (!skip("air_temp_high_th")) this.air_temp_high_th   = Number(arr[3].value);
							if (!skip("alarm")) this.alarm        = (arr[4].value === 'true' || arr[4].value === true);
							if (!skip("auto_mode")) this.auto_mode          = (arr[5].value === 'true' || arr[5].value === true);
							if (!skip("do_low_th")) this.do_low_th          = Number(arr[6].value);
							this.do_value     = arr[7].value;
							if (!skip("fan")) this.fan          = (arr[8].value === 'true' || arr[8].value === true);
							if (!skip("fan_adj")) this.fan_adj      = Number(arr[9].value);
							this.mq2_vol      = arr[10].value;
							if (!skip("ph_low_th")) this.ph_low_th          = Number(arr[11].value);
							this.ph_value     = arr[12].value;
							this.bmp_pressure = arr[13].value;
							if (!skip("pump")) this.pump         = (arr[14].value === 'true' || arr[14].value === true);
							if (!skip("smog_th")) this.smog_th      = Number(arr[15].value);
							this.water_temp   = arr[16].value;
							if (!skip("water_temp_high_th")) this.water_temp_high_th = Number(arr[17].value);
							
						if (this.alarm && !this._wasAlarm) {
							uni.showToast({ title: '鱼塘异常报警!', icon: 'none', duration: 5000 });
							uni.vibrateLong({ success: () => {}, fail: () => {} });
						}
						this._wasAlarm = this.alarm;
					},
					fail: (err) => {
							console.error('请求失败:', JSON.stringify(err));
							uni.showToast({ title: '获取数据失败，请检查网络', icon: 'none', duration: 3000 });
						}
				});
			},

			sendCommand(params, logMsg) {
					if (!this._pendingKeys) this._pendingKeys = {};
					Object.keys(params).forEach(k => { this._pendingKeys[k] = Date.now() + 5000; });
				uni.request({
					url: 'https://iot-api.heclouds.com/thingmodel/set-device-property',
					method: 'POST',
					data: { product_id: 'YOUR_PRODUCT_ID',
						device_name: 'YOUR_DEVICE_NAME',
						params: params },
					header: { 'authorization': this.token },
					success: (res) => {
							console.log(logMsg, '响应:', JSON.stringify(res.data));
							if (res.data && res.data.code === 0) {
								// 云端确认成功后，同步更新本地状态，防止轮询覆盖
								Object.keys(params).forEach(k => {
									if (k === 'smog_th') this.smog_th = params[k];
									else if (k === 'do_low_th') this.do_low_th = params[k];
									else if (k === 'ph_low_th') this.ph_low_th = params[k];
									else if (k === 'water_temp_high_th') this.water_temp_high_th = params[k];
									else if (k === 'air_temp_high_th') this.air_temp_high_th = params[k];
									else if (k === 'fan_adj') this.fan_adj = params[k];
									else if (k === 'aerator') this.aerator = params[k];
									else if (k === 'pump') this.pump = params[k];
									else if (k === 'fan') this.fan = params[k];
									else if (k === 'auto_mode') this.auto_mode = params[k];
								});
							} else if (res.data && res.data.code !== 0) {
								console.error('OneNET返回错误:', res.data);
								uni.showToast({ title: '发送失败: ' + (res.data.msg || res.data.code), icon: 'none', duration: 3000 });
							}
						},
					fail: (err) => {
							console.error('指令失败:', JSON.stringify(err));
							uni.showToast({ title: '网络请求失败，请检查网络', icon: 'none', duration: 3000 });
						}
				});
			},

			// === 设备开关 ===
			onAeratorSwitch(e) {

				const v = e.detail.value;
				this.aerator = v;
				this.sendCommand({ aerator: v }, '气泵 ' + (v ? '开' : '关'));
			},

			onModeSwitch(e) {
				const v = e.detail.value;
				this.auto_mode = v;
				this.sendCommand({ auto_mode: v }, "模式切换: " + (v ? "自动" : "手动"));
			},
			onPumpSwitch(e) {
				const v = e.detail.value;
				this.pump = v;
				this.sendCommand({ pump: v }, '水泵 ' + (v ? '开' : '关'));
			},
			onFanSwitch(e) {
				const v = e.detail.value;
				this.fan = v;
				this.fan_adj = v ? 99 : 0;
				this.sendCommand({ fan: v, fan_adj: this.fan_adj }, '风扇 ' + (v ? '开' : '关'));
			},
			sliderChangeFan(e) {
				this.fan_adj = e.detail.value;
				this.sendCommand({ fan_adj: this.fan_adj }, '风扇调速: ' + this.fan_adj);
			},

			// === 阀值调节 (每个参数一个) ===
			sliderChangeSmogTh(e) {
				this.smog_th = e.detail.value;
				this.sendCommand({ smog_th: this.smog_th }, '烟雾阈值 → ' + this.smog_th);
			},
			sliderChangeDoTh(e) {
				this.do_low_th = e.detail.value;
				this.sendCommand({ do_low_th: this.do_low_th }, '溶解氧阈值 → ' + this.do_low_th);
			},
			sliderChangePhTh(e) {
				this.ph_low_th = e.detail.value;
				this.sendCommand({ ph_low_th: this.ph_low_th }, 'PH阈值 → ' + this.ph_low_th);
			},
			sliderChangeWtTh(e) {
				this.water_temp_high_th = e.detail.value;
				this.sendCommand({ water_temp_high_th: this.water_temp_high_th }, '水温高阈值 → ' + this.water_temp_high_th);
			},
			sliderChangeAtTh(e) {
				this.air_temp_high_th = e.detail.value;
				this.sendCommand({ air_temp_high_th: this.air_temp_high_th }, '气温高阈值 → ' + this.air_temp_high_th);
			},
		}
	}
</script>

<style>
	.wrap { padding: 20rpx 30rpx 40rpx; background-color: #f8f9fa; min-height: 100vh; }
	.section-title { font-size: 28rpx; font-weight: bold; color: #555; margin-top: 25rpx; margin-bottom: 5rpx; padding-left: 10rpx; }
	.dev-area { display: flex; justify-content: space-between; flex-wrap: wrap; }
	.dev-cart { height: 150rpx; width: 320rpx; border-radius: 24rpx; margin-top: 20rpx; display: flex; justify-content: space-around; align-items: center; box-shadow: 0 2rpx 15rpx rgba(0,0,0,0.06); background-color: #fff; }
	.device-cart-l { height: 130rpx; width: 700rpx; border-radius: 24rpx; margin-top: 20rpx; display: flex; justify-content: space-around; align-items: center; box-shadow: 0 2rpx 15rpx rgba(0,0,0,0.06); background-color: #fff; }
	.dev-cart-l { height: 110rpx; width: 700rpx; border-radius: 24rpx; margin-top: 20rpx; display: flex; justify-content: space-between; align-items: center; box-shadow: 0 2rpx 15rpx rgba(0,0,0,0.06); background-color: #fff; padding: 0 30rpx; }
	.ctrl-slider { width: 520rpx; }
	.dev-name { font-size: 20rpx; text-align: center; color: #6d6d6d; }
	.dev-logo { width: 60rpx; height: 60rpx; margin-top: 8rpx; }
	.dev-data { font-size: 44rpx; color: #333; font-weight: bold; }
	.alarm-active { background-color: #e53935 !important; animation: pulse 0.8s ease-in-out infinite; }
	@keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.75; } }
	.alarm-detail { display: flex; flex-wrap: wrap; justify-content: space-around; margin-top: 20rpx; padding: 15rpx 10rpx; background-color: #fff; border-radius: 20rpx; box-shadow: 0 2rpx 10rpx rgba(0,0,0,0.04); }
	.alarm-item { font-size: 24rpx; padding: 8rpx 12rpx; font-weight: bold; }
</style>
