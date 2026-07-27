# PICOX

PICOXは、Raspberry Pi Pico（RP2040 / Arm Cortex-M0+）向けの学習用組込みOSです。

## ピン配置

### UART0

| 信号 | GPIO | Picoのピン番号 |
|---|---:|---:|
| UART0 TX | GPIO0 | 1 |
| UART0 RX | GPIO1 | 2 |
| GND | - | 3 |

USB-UART変換アダプタとは、次のように接続します。

```text
Pico GPIO0（TX） -> USB-UART RX
Pico GPIO1（RX） -> USB-UART TX
Pico GND         -> USB-UART GND
```

3.3 VのUART信号に対応した変換アダプタを使用してください。

### SDカード（SPI0）

| 信号 | GPIO | Picoのピン番号 | SDカード側 |
|---|---:|---:|---|
| MISO | GPIO16 | 21 | DO |
| CS | GPIO17 | 22 | CS |
| SCK | GPIO18 | 24 | CLK |
| MOSI | GPIO19 | 25 | DI |
| 3.3 V | - | 36 | VCC |
| GND | - | 23または38 | GND |

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

## 必要なもの

- Raspberry Pi Pico
- Raspberry Pi Pico SDK
- GNU Arm Embedded Toolchain
- CMake
- Make
- USB-UART変換アダプタ
- SPI接続のmicroSDカードモジュール

## ビルド方法

環境変数 `PICO_SDK_PATH` にRaspberry Pi Pico SDKの場所を設定します。

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
```

その後、PICOXのディレクトリでビルドします。

```sh
mkdir -p build
cd build
cmake ..
make
```

ビルドに成功するとUF2ファイルが生成されます。

Raspberry Pi PicoのBOOTSELボタンを押しながらUSBへ接続し、生成されたUF2ファイルを書き込んでください。

## PICOXの起動

USB-UART変換アダプタを接続し、シリアルターミナルを開きます。

macOSでの例：

```sh
screen /dev/tty.usbserial-XXXX 115200
```

正常に起動すると、次のように表示されます。

```text
PICOX shell started
picox>
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
5. `run` コマンドでELFファイルを指定します。

例：

```text
picox> run LED.ELF
```

ファイル名には、FATの8.3形式に収まる名前を使用してください。

例：

```text
LED.ELF
APP.ELF
TEST.ELF
```

PICOXはSDカードからELFファイルを読み込み、ロード対象のセグメントをRAMへ配置して、エントリポイントを呼び出します。

## アプリケーション用RAM領域

現在のELFローダーでは、次のRAM領域をアプリケーション用として使用します。

```text
0x20020000 - 0x2003FFFF
```

アプリケーションのロード対象セグメントとエントリポイントが、この範囲に収まるようにリンクしてください。
