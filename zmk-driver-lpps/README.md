# zmk-driver-lpps（Nickey44A デュアル TrackPoint 拡張仕様）

[sekigon-gonnoc/low-power-pointing-stick](https://github.com/sekigon-gonnoc/low-power-pointing-stick)（SK8707 センサー I2C 変換モジュール、アドレス `0x57`）向けの ZMK ドライバをリポジトリ内モジュールとして同梱し、左右分割構成向けにジッター抑制・打鍵ロックアウト・自動レイヤー起動ガード・ランタイム設定同期を追加したものです。

---

## 1. 信号処理パイプライン全体像

### 左手側（ペリフェラル：`nickey44a_l`）
1. `lpps.c`：I2C 読み出し（最短 `2ms` 間隔）→ 逆方向デッドゾーン → アイドルリセット（`50ms`）→ 累積閾値判定（既定 `N = 10`、セントラルからランタイム変更可）
2. `left_trackpoint_split`（`reg = <1>`）：BLE Split 経由でセントラルへ相対移動イベントを転送

### 右手側（セントラル：`nickey44a_r`）
- **右センサー入力（`pointing_listener`）**
  1. `lpps.c`：I2C 読み出し（最短 `2ms` 間隔）→ 逆方向デッドゾーン → アイドルリセット（`50ms`）→ 累積閾値判定（既定 `N = 8`、ランタイム変更可）
  2. `&zip_typing_lockout 150`：通常キー押下中および打鍵後 `150ms` の入力を破棄
  3. `&zip_tp_layer 4 400`：正味変位 `4` 以上（最初の 1 レポート目）で Layer 4 を `400ms` 起動
  4. `&zip_xy_transform INPUT_TRANSFORM_X_INVERT` → `&zip_xy_scaler 1 2` → `&zip_tpc_mode 1`（`MOUSE_MOVE`）
- **左 Split 受信入力（`left_pointing_listener`）**
  1. `&zip_left_tp_gate`：左 TrackPoint 無効（OFF）時にイベントを破棄
  2. `&zip_typing_lockout 150`：通常キー押下中および打鍵後 `150ms` の入力を破棄
  3. `&zip_tp_layer 4 400`：最初の 1 レポート目（正味変位 `4` 以上）で Layer 4 を `400ms` 起動
  4. `&zip_xy_transform INPUT_TRANSFORM_XY_SWAP` → `&zip_xy_scaler 1 2` → `&zip_tpc_mode 0`（`MOUSE_SCRL`：`1/16` 分周＝計 `1/32`、主軸ロック、逆方向入力時および 400ms アイドルで端数リセット）

---

## 2. Kconfig パラメータ一覧（`zmk-driver-lpps/Kconfig`）

設定ファイル：`config/nickey44a_r.conf`（右） / `config/nickey44a_l.conf`（左）

| シンボル名 | 右現在値 | 左現在値 | 説明 |
| :--- | :---: | :---: | :--- |
| `CONFIG_LPPS` | `y` | `y` | LPPS I2C ドライバの有効化 |
| `CONFIG_LPPS_JITTER_FILTER` | `y` | `y` | ソフトウェア累積ジッターフィルタの有効化（`n` で生値出力） |
| `CONFIG_LPPS_ACCUM_REPORT` | `8` | `10` | レポート送信に必要な累積移動量 `N` の初期値（LPPS は `±1` パルス頻度方式のため、サンプル単位の単純デッドゾーンではなく本累積値でノイズを切る） |
| `CONFIG_LPPS_INSTANT_REPORT` | `12` | `12` | 単一サンプルでの即時出力閾値（実効値は `MAX(INSTANT_REPORT, 現在のN)`） |
| `CONFIG_LPPS_ACCUM_IDLE_MS` | `50` | `50` | この時間（ms）次のパルスが来なければ累積バッファを `0` に戻す。またアイドル明け最初の 1 サンプルは `±2` にクランプして突発スパイクを防止 |

---

## 3. カスタム Input Processor / Behavior 一覧

定義ファイル：`zmk-driver-lpps/dts/behaviors/trackpoint-left-ctl.dtsi` / `trackpoint-control.dtsi` / `lpps-calibration.dtsi`

### Input Processors（セントラル側：`boards/shields/nickey44a/nickey44a_r.overlay`）

- **`&zip_typing_lockout <lockout_ms>`**（`src/input_processor_typing_lockout.c`）
  - 非モディファイアキーが押されている間、および押下・離鍵から `<lockout_ms>`（現在 `150ms`）の間、`INPUT_EV_REL` を破棄します。
  - Shift / Ctrl / Alt / GUI 単体押しや `&mkp` マウスクリックではロックアウトされません。
- **`&zip_tp_layer <layer> <timeout_ms>`**（`src/input_processor_tp_layer.c`）
  - `activation-threshold`（現在 `<4>`）：`activation-window-ms`（`<350>`）内の符号付き正味変位 `|Σdx| + |Σdy|` がこの値に達すると対象レイヤーを有効化します。
  - `min-activation-frames`（現在 `<1>`）：左右とも最初の 1 レポートが出た瞬間に Layer 4 へ切り替わります。
  - `require-prior-idle-ms`（現在 `<150>`）：最後の非モディファイアキー打鍵からこの時間が経過するまでレイヤーを起動しません。
  - `excluded-positions`（現在 `<38 39 40 41 42 43>`）：親指クリック 6 キー以外の位置が押された瞬間にレイヤーを即解除します。
- **`&zip_left_tp_gate`**（`src/trackpoint_left_central.c`）
  - 左 TrackPoint が OFF のとき、左からの `INPUT_EV_REL` を破棄します（キーマトリクス入力には影響しません）。
- **`&zip_tpc_mode <side>`**（`src/trackpoint_control.c`）
  - `side = 0`（左：`TPC_POS_LEFT = 44`）/ `side = 1`（右：`TPC_POS_RIGHT = 45`）のキーマップ設定（`&tpc MOUSE_MOVE` / `MOUSE_SCRL`）に従い、`MOUSE_SCRL` 時は `sync` フレーム単位で主軸ロック（`|dx| > |dy| * 1.5` のときのみ横、それ以外は縦優先）および逆向き時の端数即時クリアを行い、`1/16` スケールで縦横スクロールへ変換します。

### Behaviors

- **`&tp_left_ctl <cmd>`**（`src/behaviors/behavior_tp_left_ctl.c` / `src/trackpoint_left_central.c`）
  - `0`：左 TP ON/OFF トグル（NVS キー `trackpoint/left_en`）
  - `1` / `2`：左 TP の `N` 値 `+1` / `-1`（NVS キー `trackpoint/left_n_v2`、BLE 経由で左の `&lpps_set_accum` を呼び出し、再接続時にも自動再送）
  - `3` / `4`：右 TP の `N` 値 `+1` / `-1`（NVS キー `trackpoint/right_n`）
- **`&ps_calib`**（`src/behaviors/behavior_lpps_calibration.c`）
  - 押下された側の LPPS モジュール（レジスタ `0x04`）へゼロ点キャリブレーション要求を書き込みます。

---

## 4. ハードウェア・BLE Split 上の重要な設定メモ

1. **NFC ピンの GPIO 化（`P0.09` / `P0.10`）**
   Nickey44A では LPPS の I2C（SDA = `P0.09`, SCL = `P0.10`）に nRF52840 の NFC 兼用ピンを使用しています。Zephyr 4.1 以降では `CONFIG_NFCT_PINS_AS_GPIOS` が廃止されているため、左右 `.overlay` 内の `&uicr { nfct-pins-as-gpios; };` によって GPIO 化しています。
2. **セントラルのバッテリー取得無効化（ZMK GATT 購読競合の回避）**
   本リポジトリが参照している ZMK リビジョンでは、`CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING=y` が有効だと接続直後の GATT ディスカバリで `sub_discover_params` の競合（ZMK Issue #3156）が発生し、左キースイッチの通知購読（`POSITION_STATE`）が失敗する場合があります。そのため `config/nickey44a_r.conf` では `BATTERY_LEVEL_FETCHING=n` / `PROXY=n` とし、あわせて `BT_BUF_ACL_TX_COUNT=10`・`left_trackpoint_split@1`（`reg = <1>`）を設定しています。
