# PICOX

PICOXは、Raspberry Pi Pico（RP2040 / Arm Cortex-M0+）向けの組込みOSです。

## ピン配置

### Picoprobeとの接続

Picoprobeには、PICOXのシリアル出力を確認するためのUART接続と、プログラムの書き込み・デバッグを行うためのSWD接続を行います。

#### UART0

| 信号       |  GPIO | Picoのピン番号 |
| -------- | ----: | --------: |
| UART0 TX | GPIO0 |         1 |
| UART0 RX | GPIO1 |         2 |

PICOXを実行するPicoとPicoprobeを、次のように接続します。

| 実行対象Pico       |  PicoProbe |
|-|-|
| GPIO0（UART0 TX） | GPIO5 （UART RX）|
| GPIO1（UART0 RX） | GPIO4 （UART TX）|
| GND | GND|


UART通信の設定は次のとおりです。

```
ボーレート  ：115200 bps
データ長    ：8 bit
パリティ    ：なし
ストップ    ：1 bit
フロー制御  ：なし
```

#### SWD

プログラムの書き込みやGDBによるデバッグには、SWDを使用します。

| 実行対象Pico       |  PicoProbe |
|-|-|
| SWCLK | SWCLK（GPIO2）|
| SWDIO | SWDIO（GPIO3）|
| GND | GND|

Raspberry Pi PicoのSWD端子は、基板下部にある次の3端子です。

```text
SWCLK
GND
SWDIO
```

### SDカード（SPI0）

| 信号    |   GPIO | Picoのピン番号 | SDカード側 |
| ----- | -----: | --------: | ------ |
| MISO  | GPIO16 |        21 | DO     |
| CS    | GPIO17 |        22 | CS     |
| SCK   | GPIO18 |        24 | CLK    |
| MOSI  | GPIO19 |        25 | DI     |
| 3.3 V |      - |        36 | VCC    |
| GND   | -      | 任意のGNDピン | GND |

接続は次のとおりです。

```text
Pico GPIO16（MISO） -> SD DO
Pico GPIO17（CS）   -> SD CS
Pico GPIO18（SCK）  -> SD CLK
Pico GPIO19（MOSI） -> SD DI
Pico 3.3V           -> SD VCC
Pico GND            -> SD GND
```

3.3 Vの信号に対応したSDカードモジュールを使用してください。

## 動作確認済みの開発環境

### PC

```text
MacBook Pro M1
macOS Sequoia 15.5
```

### マイコン

```text
Raspberry Pi Pico
RP2040
Arm Cortex-M0+
```

## ソフトウェアの環境構築

### 必要なソフトウェアのインストール

Homebrewを使用して、OpenOCD、GDB、ARM用コンパイラをインストールします。

```sh
brew install open-ocd
brew install arm-none-eabi-gdb
brew install --cask gcc-arm-embedded
```

各ソフトウェアの用途は次のとおりです。

* `open-ocd`

  * Picoprobeを経由してPicoへ接続します。
  * プログラムの書き込みやデバッグに使用します。

* `arm-none-eabi-gdb`

  * ARMマイコン向けのGDBです。
  * プログラムの停止、ステップ実行、変数やレジスタの確認に使用します。

* `gcc-arm-embedded`

  * ARMマイコン向けのコンパイラやリンカなどをインストールします。

GDBのコマンドへパスを通します。

```sh
brew link --overwrite arm-none-eabi-gdb
hash -r
```

* `brew link --overwrite arm-none-eabi-gdb`

  * HomebrewでインストールしたGDBをターミナルから実行できるようにします。

* `hash -r`

  * シェルが保持しているコマンドパスのキャッシュを更新します。

## PICOXのビルド

PICOXのリポジトリへ移動し、次のコマンドを実行します。

```sh
make
```

ビルドに成功すると、Picoへ書き込むための実行ファイルが生成されます。

## Picoprobeを使用した書き込み

Picoprobeと実行対象PicoのSWD端子を接続した状態で、OpenOCDを起動します。

使用するOpenOCDの設定ファイルや書き込みコマンドは、環境に合わせて指定してください。

## PICOXの起動

PicoprobeのUART機能を使用して、シリアルターミナルを開きます。

macOSでは、Picoprobeのシリアルデバイスを次のコマンドで確認できます。

```sh
ls /dev/cu.usbmodem*
```

表示されたデバイス名を指定して接続します。

```sh
screen /dev/cu.usbmodemXXXX 115200
```

正常に起動すると、次のように表示されます。

```text
PICOX shell started
picox>
```

`screen`を終了する場合は、次の順番でキーを入力します。

```text
Ctrl+A
K
Y
```

## シェルコマンド

使用できるコマンドには、次のものがあります。

```text
help
echo
version
clear
irqstat
reset
run FILE.ELF
```

コマンド一覧を表示する例：

```text
picox> help
```

SDカード上のELFファイルを実行する例：

```text
picox> run LED.ELF
```

## SDカードの準備

1. SDカードをFAT32でフォーマットします。
2. 実行するELFファイルをSDカードのルートディレクトリへコピーします。
3. SDカードをPicoへ接続したモジュールに挿入します。
4. PICOXを起動します。
5. `run`コマンドでELFファイルを指定します。

例：

```text
picox> run LED.ELF
```

ファイル名には、FATの8.3形式に収まる名前を使用してください。

例：

```text
LED.ELF
```