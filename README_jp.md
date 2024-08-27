# UPDI4AVR-USB : OSS/OSHW Programmer for UPDI/TPI

*Switching document languages* : __日本語__, [English](README.md)

- AVR-DUファミリーを、USB接続プログラム書込器に変身させるオープンソース ソフトウェア／ファームウェア。
- UPDIタイプと、TPIタイプの AVRシリーズの NVM（不揮発メモリ）を読み出し／消去／書き込みができる。
- ホストPC側のプログラム書き込みアプリケーションは AVRDUDEを想定。"PICKit4" や "Curiosity Nano" のように見える。
- VCP-UART トランスファー機能を装備。
- 全ての成果物は、MITライセンスで頒布。

従来の *USB4AVR* は USBシリアル変換回路を使用する設計だが、この *UPDI4AVR-USB* は MCU内蔵USB周辺回路を使用する 1チップ完結設計である。

## Quick Start

ビルド済バイナリを ["AVR64DU32 Curiosity Nano : EV59F82A"](https://www.microchip.com/en-us/development-tool/ev59f82a) 製品にアップロードして、手早くセットアップできる。

The pre-built binaries can be uploaded to the ["AVR64DU32 Curiosity Nano : EV59F82A"](https://www.microchip.com/en-us/development-tool/ev59f82a) product for easy setup.

[->Click Here](https://github.com/askn37/UPDI4AVR-USB/tree/main/hex/updi4avr-usb)

## Introduction

AVR-DUファミリーの存在は 2021年春に公表されたものの、すぐにペンディングした。それが停滞している間にAVR-Exシリーズの発売が先行し、さらに時間を経て 2024年5月にようやく AVR64DU32 第一次生産品（残念なErrata有）が発売された。14P/20P製品の発売にはまだ半年ほどかかるだろう。

最大の特徴は AVR-Dxシリーズのファミリーとして唯一、USB 2.0 "Full-Speed" デバイス周辺機能を内蔵していることだ。これは ATxmega AU ファミリー（高価でマイナー）から受け継がれた機能で、ATmega32U4ファミリーのそれよりずっと強力なものだ。AVR-DUファミリーでは SOIC-14P と、3ミリ角の VQFN-20P の外囲器も用意され、コンパクトにも作れることから、高い可能性を秘めている。

一方であまりにも新しい製品であるため、オープンソース対応は進んでいない。公式開発環境は MPLAB-X であるが、HALに基づく大仰なビルド環境のため、とても大きなフラッシュ容量を奪われてしまう。しかもスループットが十分稼げない。戦斧で鮒を調理させたいの？

私が AVR-DUファミリーにまず求めていたことは、USB-CDC/ACM ベースで普通に OSから認識される VCPトランスファーの実現、そして USB-HID ベースで AVRDUDEから扱える UPDI/TPI プログラミング機能だ。自ずから USB複合デバイスでなければならない。一方で MPLAB-Xからの利用はライセンスの問題もあるから考えていない。だから dWire や OCD の存在は忘れることにしよう。

手掛かりになる資料は [USB-IF 公開仕様書](https://www.usb.org/document-library/class-definitions-communication-devices-12)と、[AVR-DUファミリーデータシート](https://ww1.microchip.com/downloads/aemDocuments/documents/MCU08/ProductDocuments/DataSheets/AVR32-16DU-14-20-28-32-Prelim-DataSheet-DS40002576.pdf)と、[ATxmega AUファミリー概説](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8331-8-and-16-bit-AVR-Microcontroller-XMEGA-AU_Manual.pdf)だけ。これでは全くのクリーンルーム開発だ。AVR-GCC と AVR-LIBC だけを使用し、他のプロジェクトと何も関わりを持たないまま、USBプロトコルスタックをゼロから構築することから始めた。USB複合デバイスの主要な動作は 3KiB程度に凝縮して、自由に応用できるようになるまで 20日は必要だった。（もちろん DFUブートローダーも作れるが、それはまた別の計画）

次に AVRDUDE のソースコード "jtag3.c"の調査に取り組んだ。私は既に "jtagmkII.c" "serialupdi.c" の改良に貢献してきたので難易度は高くない。UPDI NVM制御の多くは *UPDI4AVR* シリーズをハンドメイドしたことで熟知していたし、AVR-Dx/Exシリーズ対応にも関わってきた。仮初に "Curiosity Nano"の応答を模倣するエミュレーター応用を作ったところ、USB-VID:PID を自由に変更できない問題が立ちはだかったので、"usb_hidapi.c" の強化が必要だった。加えて、わずかなコード追加だけで済むことから UPDIデバイスだけでなく、TPIデバイスも扱えるようにした。*TPI4AVR*をハンドメイドした経験がここで活かされた。

シナリオを作り、概ね動くようになるまで10日、手元にある 20種類以上の UPDIデバイスを片っ端から動作確認し、十分満足できる結果が得られるようになるまでさらに20日。だがそれ以上に、概説を整えなければならない退屈な時間！

そんな経緯を経てようやく["AVR64DU32 Curiosity Nano : EV59F82A"](https://www.microchip.com/en-us/development-tool/ev59f82a)で使用できる、最初のオープンソース ブランチを公開する。HV制御のような、予定機能のいくつかはまだ実装されていないが、一般用途で試してみるには困らないはずだ。

## What you can and can't do

おおむね、従来の *JTAG2UPDI* や *microUPDI* の代替となる。それらをすでに使っているのなら、すぐに使い始められるだろう。

このソフトウェアは、次のことができる：

- AVR-DUファミリーにのみインストールできる。
- 発売済の全ての UPDIタイプ AVRシリーズを操作できる：
  - tinyAVR-0/1/2、megaAVR-0、AVR-Dx、AVR-Ex の全ての製品
- 発売済の全ての TPIタイプ AVRシリーズを操作できる：
  - ATtiny4 ATtiny5 ATtiny9 ATtiny10 ATtiny20 ATtiny40 ATtiny102 ATtiny104 （計8種類）
- Windows/macos/Linuxの OS標準ドライバーを使用するため、ドライバーの追加導入は不要。
  - 追加ドライバー／Infファイルが既に入っている場合、VID:PIDに一致するデバイスベンダーが表示がされる。（ライセンス侵害に注意）
- RS232制御信号を含む、VCP（Virtual Communication Port）を混載。
  - RS232制御信号は余剰ピンに割り当てられるため、14P/20P外囲器品種では機能が制限される。
- VCP動作中は、ターゲットデバイスをリセットできる、押し下げスイッチが利用できる。
  - 押し下げている間、リセット状態が維持される。UPDI/TPIプログラム動作中は無効。
  - tinyAVRシリーズのようにハードウェアリセット端子が標準では存在しないターゲットデバイス（UPDIタイプ）でも、電源を切らずにリセットできる。
  - デバイスに書き込まれたブートローダーなどの応用コードを、強制再起動するのに便利。Arduino IDE互換。
- ターゲットデバイスの、デバイス施錠、解錠ができる。（LOCKビットFUSE操作と、強制チップ消去）

計画はされているが、まだ実現していない：（2024年8月時点）

- UPDIタイプの高電圧書込サポートが 2種類。*専用の追加ハードウェア回路が必要。*
- TPIタイプの高電圧書込サポートが 1種類。*専用の追加ハードウェア回路が必要。さらにAVRDUDEの追加修正も必要。*
- "DFU for Serial" および "arduino/urclock" などの、ブートローダープロトコル VCPブリッジ。*AVRDUDEに統合するかは未定。*

このソフトウェアは、次のことができない：

- AVR-DUファミリー以外には、必要な USB周辺機能が実装されていないため、動作しない。
  - ATxmega AU ファミリーへの移植は USB周辺機能が似ているため、おそらくは可能。（計画はない：Fork必須）
- USB 2.0 "Full-Speed" のみ対応。AVR-DUファミリーは "High-Speed"に対応していない。（不可能）
- ISP／PP／HVPPタイプのデバイスはサポート対象外。ハードウェア要件が異なり、GPIOに共通性がないので別のソフトウェアとなる。（Fork必須）
- PDIタイプの AVRシリーズはサポート対象外。*ただしハードウェア要件の違いは少ないため、将来追加対応の計画はある。*
- JTAG通信機能、SWD/SWO機能、dWire機能、OCD機能はサポートされない。（計画はない）
- 14P外囲器製品（AVR16-32DU14）には余剰ピンがないため、高電圧書込サポートはできない。全機能を同時に使用するには、28P/32P外囲器製品が必要。
- 14P/20Pではピン数が不足、16KiB品種では、空き容量がないため DEBUGビルド（PRINTF）は使用できない。

AVR-DUファミリーの外囲器種別毎のピン配列／信号割当の詳細については、[<configuration.h>](src/configuration.h)を参照。

> [!NOTE]
> ATmega328P ファミリーのような前世代のデバイスは、通常は ISP直列プログラミング（SPI技術）で扱うが、ひとたび FUSEを破壊すると HVPP高電圧並列プログラミング（パラレル技術）を使用しなければ、工場出荷時状態に戻せない品種がある。この場合、*UPDI4AVR* のような「デバイスの工場出荷時状態の復元」を至上命題とするハイレベル書込器は、ISP／PP／HVPP を全て処理できる必要がある。

## Practical Usage

以下は、"AVR64DU32 Curiosity Nano" のための簡単な使用例。
この製品シリーズは、付属のピンヘッダを使うとソルダーレスでブレッドボードに装着できる。

### UPDI Control

UPDI制御の場合、対象デバイスに必須の配線は "VCC" "GND" "UPDI(TDAT)" の3本だ。これに任意で "nRST(TRST)" "VCP-TxD" "VCP-RxD" の 3本を加えることができる。デバイス側が GPIOをプッシュプル出力に設定しない限り、どの接続もプルアップ抵抗内蔵オープンドレインだ。GPIO の競合を懸念するなら 330Ωの直列抵抗を挿入しても良い。

> 電気特性は 5V/225kbpsが基準で、VCCx0.2〜0.8範囲のスリューレートに注意したい。

<img src="https://askn37.github.io/product/UPDI4AVR/images/IMG_3832.jpg" width="400"> <img src="https://askn37.github.io/product/UPDI4AVR/images/U4AU_UPDI.drawio.svg" width="400">

AVR-ICSP MIL/6Pコネクタに変換するには、以下の信号配列を推奨。これは TPI制御や、2種類のHV制御方式と互換性がある。（ただし専用回路がなければ HV制御はできない）

<img src="https://askn37.github.io/svg/AVR-ICSP-M6P-UPDI4AVR.drawio.svg" width="320">

仮に、`AVR64DU28`を対象デバイスとした場合、最低限の接続テストは以下のコマンドラインで可能だ。

```console
avrdude -c pickit4_updi -p avr64du28 -v -U sib:r:-:r
```

```
avrdude: Version 7.3
         Copyright the AVRDUDE authors;
         see https://github.com/avrdudes/avrdude/blob/main/AUTHORS

         System wide configuration file is C:\usr\avrdude-v7.3-windows_mingw-x64\avrdude.conf

         Using port            : usb
         Using programmer      : pickit4_updi
         AVR Part              : AVR64DU28
         Programming modes     : UPDI, SPM
         Programmer Type       : JTAGICE3_UPDI
         Description           : MPLAB(R) PICkit 4 in UPDI mode
         ICE HW version        : 0
         ICE FW version        : 1.32 (rel. 40)
         Serial number         : **********
         Vtarget               : 5.02 V
         PDI/UPDI clk          : 225 kHz

avrdude: partial Family_ID returned: "AVR "
avrdude: silicon revision: 1.3
avrdude: AVR device initialized and ready to accept instructions
avrdude: device signature = 0x1e9622 (probably avr64du28)

avrdude: processing -U sib:r:-:r
avrdude: reading sib memory ...
avrdude: writing output file <stdout>
AVR     P:4D:1-3M2 (A3.KV00S.0)
avrdude done.  Thank you.
```

### TPI Control

TPI制御の場合、対象デバイスに必須の配線は "VCC" "GND" "TDAT" "TCLK" "TRST" の5本だ。
結果的に、6Pデバイスである ATtiny4/5/9/10 の場合、未使用のピンは1本しか残らない。
全ての接続は、プルアップ抵抗内蔵オープンドレインだ。

<img src="https://askn37.github.io/product/UPDI4AVR/images/IMG_3839.jpg" width="400"> <img src="https://askn37.github.io/product/UPDI4AVR/images/U4AU_TPI.drawio.svg" width="400">

仮に、`ATiny10`を対象デバイスとした場合、最低限の接続テストは以下のコマンドラインで可能だ。

```console
avrdude -c pickit4_tpi -p attiny10 -v
```

```
avrdude: Version 7.3
         Copyright the AVRDUDE authors;
         see https://github.com/avrdudes/avrdude/blob/main/AUTHORS

         System wide configuration file is C:\usr\avrdude-v7.3-windows_mingw-x64\avrdude.conf

         Using port            : usb
         Using programmer      : pickit4_tpi
         AVR Part              : ATtiny10
         Programming modes     : TPI
         Programmer Type       : JTAGICE3_TPI
         Description           : MPLAB(R) PICkit 4 in TPI mode
         ICE HW version        : 0
         ICE FW version        : 1.32 (rel. 40)
         Serial number         : **********
         Vtarget               : 5.01 V

avrdude: AVR device initialized and ready to accept instructions
avrdude: device signature = 0x1e9003 (probably t10)

avrdude: processing -U flash:r:-:I
avrdude: reading flash memory ...
Reading | ################################################## | 100% 0.91 s
avrdude: writing output file <stdout>
:200000000AC011C010C00FC00EC00DC00CC00BC00AC009C008C011271FBFCFE5D0E0DEBF02 // 00000> .@.@.@.@.@.@.@.@.@.@.@.'.?OeP`^?
:20002000CDBF02D016C0ECCF48ED50E04CBF56BF4FEF47BB7894619A0A9ABA98029A4FEF35 // 00020> M?.P.@lOHmP`L?V?OoG;x.a...:...Oo
:1600400059E668E1415050406040E1F700C00000F5CFF894FFCFAB                     // 00040> YfhaAPP@`@aw.@..uOx..O
:00000001FF

avrdude done.  Thank you.
```

> この例では PB2端子用の *Lチカ* スケッチバイナリが読み出されている。\
> ATtiny102とATtiny104の UARTを6Pコネクタに引き出して VCPと接続したい場合は工夫が必要。PA0/TCLKとPB3/RXDは短絡し、かつPA0は原則としてGPIO未使用としなければならない。

### LED blinking

オレンジLEDは、状況によって幾つかの表情を見せる。

- ハートビート - あるいは深呼吸。USB接続が ホストOSと確立されている。使用準備完了。
- 短い閃光 - USB接続待機中。ホストOSから認識されていない。
- 長い明滅 - SW0が押し下げられている。プログラミング中ではない。対象デバイスは（可能なら）リセット中。
- 短い明滅 - プログラミング実行中。VCP通信は無効。

> VCPの TxD/RxD通信表示についての LED制御端子は用意されていない。

### Other pinouts

ピン配列／信号割当の詳細については、[<configuration.h>](src/configuration.h)を参照されたい。

## High-Voltage control

対応計画中。専用の制御回路を外付けする必要がある。技術的にはすでに前作の[UPDI4AVR](https://askn37.github.io/product/UPDI4AVR/)で実現していることだ。

現在、2つの計画が進行中。

- FRISKケースサイズのオールインワンモデル。
- CNANOをドーターボードとして装着する拡張ボードモデル。

<img src="https://askn37.github.io/product/UPDI4AVR/images/U4AU_VIEW_MZU2410A.drawio.svg" width="400">

## Build and installation

Arduino IDEに、次のリンク先の SDK を導入すると、ベアメタルチップを含めた AVR-DUファミリー全てへのビルドとインストールが簡単にできる。

- https://github.com/askn37/multix-zinnia-sdk-modernAVR

ビルドオプションについては、[<UPDI4AVR-USB.ino>](UPDI4AVR-USB.ino)を参照されたい。

## Related link and documentation

- リポジトリフロントページ (このページ): We're looking for contributors to help us out.
  - [日本語(Native)](README_jp.md), [English](README.md)

- [UPDI4AVR-USB QUICK INSTALLATION GUIDE](hex/updi4avr/README.md)
- [UPDI4AVR](https://askn37.github.io/product/UPDI4AVR/) : USBシリアル変換版

- [AVRDUDE](https://github.com/avrdudes/avrdude)

- [USB-IF 公開仕様書](https://www.usb.org/document-library/class-definitions-communication-devices-12)
- [AVR-DUファミリーデータシート](https://ww1.microchip.com/downloads/aemDocuments/documents/MCU08/ProductDocuments/DataSheets/AVR32-16DU-14-20-28-32-Prelim-DataSheet-DS40002576.pdf)
- [ATxmega AUファミリー概説](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8331-8-and-16-bit-AVR-Microcontroller-XMEGA-AU_Manual.pdf)

## Copyright and Contact

Twitter(X): [@askn37](https://twitter.com/askn37) \
BlueSky Social: [@multix.jp](https://bsky.app/profile/multix.jp) \
GitHub: [https://github.com/askn37/](https://github.com/askn37/) \
Product: [https://askn37.github.io/](https://askn37.github.io/)

Copyright (c) askn (K.Sato) multix.jp \
Released under the MIT license \
[https://opensource.org/licenses/mit-license.php](https://opensource.org/licenses/mit-license.php) \
[https://www.oshwa.org/](https://www.oshwa.org/)
